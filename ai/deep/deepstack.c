/***********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "log.h"
#include "mem.h"
#include "support.h"

/* common */
#include "extras.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "terrain.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"
#include "world_object.h"

#include "deeptuning.h"

#include "deepstack.h"

/* Live worksites per player. A worksite needs a worker standing on it,
 * so the table is bounded by this player's worker count; the cap is a
 * BOOKKEEPING bound, exactly as the corridor funnel's is. A worksite
 * that cannot be recorded is counted in `over` and then not followed:
 * it costs the census one sample and costs the game nothing. */
#define DEEP_STACK_SLOTS 512

/* Stack sizes the histogram distinguishes; everything above lands in
 * the last bucket. Five is not a threshold anything decides on -- it is
 * how far the tail is printed. */
#define DEEP_STACK_HIST 5

struct deep_stack_site {
  int tile;             /* Tile index; -1 marks a free slot. */
  int act;              /* The activity being performed there. */
  int tgt;              /* Extra number of the target, or -1. */
  bool had;             /* Whether the target extra was present at open. */
  int terr;             /* Terrain number at open (for transforms). */
  int begun;            /* Turn work was first seen here. */
  int turns;            /* Turns work has been seen here. */
  int maxn;             /* Most simultaneous own workers seen. */
  int now;              /* Workers seen this turn (scratch). */
};

struct deep_stack_plr {
  struct deep_stack_site site[DEEP_STACK_SLOTS];

  /* Cumulative over the game, so the LAST line per player per game is
   * that game's census -- the shape deepfunnel and deepfort use. */
  int sites;
  int turns;
  int wturns;
  int stk;
  int hist[DEEP_STACK_HIST];
  int done;
  int dsolo;
  int dstk;
  int tsolo;
  int tstk;
  int lost;
  int over;
};

static struct deep_stack_plr *deep_stack_plr[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  This player's ledger, created on demand. NULL when the metric lines
  are not being written: the whole ledger is instrumentation, so when
  nothing will read it, nothing is kept and nothing is spent.
**************************************************************************/
static struct deep_stack_plr *deep_stack_get(const struct player *pplayer)
{
  int pn = player_number(pplayer);
  struct deep_stack_plr *sp;
  int i;

  if (pn < 0 || pn >= MAX_NUM_PLAYER_SLOTS) {
    return NULL;
  }
  sp = deep_stack_plr[pn];
  if (sp != NULL) {
    return sp;
  }
  if (!log_do_output_for_level(deep_metric_log_level())) {
    return NULL;
  }

  sp = fc_calloc(1, sizeof(*sp));
  for (i = 0; i < DEEP_STACK_SLOTS; i++) {
    sp->site[i].tile = -1;
  }
  deep_stack_plr[pn] = sp;

  return sp;
}

/**********************************************************************//**
  Did the improvement this worksite was for actually appear?

  Asked of the tile as it stands now, against what was recorded when the
  site opened, so it answers for a removal (the extra is gone) as
  readily as for a build (the extra is there) and for a transform (the
  terrain is not the one we started on). It cannot say WHO finished it
  -- extras record no builder -- so a site another player's worker
  completed under ours counts as done. That is the same attribution
  caveat deepfunnel states, and it is bounded by how rarely two players
  work one tile.
**************************************************************************/
static bool deep_stack_site_done(const struct deep_stack_site *s)
{
  struct tile *ptile = index_to_tile(&(wld.map), s->tile);

  if (ptile == NULL) {
    return FALSE;
  }
  if (s->tgt >= 0) {
    const struct extra_type *pextra = extra_by_number(s->tgt);

    return pextra != NULL && tile_has_extra(ptile, pextra) != s->had;
  }

  return terrain_number(tile_terrain(ptile)) != s->terr;
}

/**********************************************************************//**
  Charge a worksite that no worker is on any more to its outcome and
  free its slot. A site that ran a single turn and finished is as much
  a sample as one that ran twenty and was abandoned.
**************************************************************************/
static void deep_stack_close(struct deep_stack_plr *sp,
                             struct deep_stack_site *s)
{
  if (deep_stack_site_done(s)) {
    sp->done++;
    if (s->maxn > 1) {
      sp->dstk++;
      sp->tstk += s->turns;
    } else {
      sp->dsolo++;
      sp->tsolo += s->turns;
    }
  } else {
    sp->lost++;
  }
  s->tile = -1;
}

/**********************************************************************//**
  The live worksite for this tile and activity, opening one if there is
  none. NULL when the table is full, which is counted rather than
  grown: the ledger must not be able to cost more than the game.

  Keyed on (tile, activity, target) rather than on the tile alone,
  because a worker that starts a mine where another is building a road
  is not joining anything -- total_activity() sums only units doing the
  SAME activity on the SAME target, and a census that pooled them would
  report stacks the rules do not credit.
**************************************************************************/
static struct deep_stack_site *deep_stack_find(struct deep_stack_plr *sp,
                                               const struct tile *ptile,
                                               enum unit_activity act,
                                               const struct extra_type *tgt)
{
  int idx = tile_index(ptile);
  int tgtn = (tgt != NULL ? extra_number(tgt) : -1);
  int i, free_slot = -1;

  for (i = 0; i < DEEP_STACK_SLOTS; i++) {
    struct deep_stack_site *s = &sp->site[i];

    if (s->tile == idx && s->act == act && s->tgt == tgtn) {
      return s;
    }
    if (s->tile < 0 && free_slot < 0) {
      free_slot = i;
    }
  }

  if (free_slot < 0) {
    sp->over++;

    return NULL;
  }

  {
    struct deep_stack_site *s = &sp->site[free_slot];

    s->tile = idx;
    s->act = act;
    s->tgt = tgtn;
    s->had = (tgt != NULL && tile_has_extra(ptile, tgt));
    s->terr = terrain_number(tile_terrain(ptile));
    s->begun = game.info.turn;
    s->turns = 0;
    s->maxn = 0;
    s->now = 0;
    sp->sites++;

    return s;
  }
}

/**********************************************************************//**
  One line per player per turn: the workers-per-active-tile
  distribution, the worksite-turns behind it, and what the worksites
  that ended cost in calendar turns, split by whether they were ever
  stacked.

  Taken at the end of the phase, after the manage loop and
  auto_workers_player() have both run, so a worker's activity is the one
  it will actually spend this turn on.
**************************************************************************/
void deep_stack_census(struct player *pplayer)
{
  struct deep_stack_plr *sp = deep_stack_get(pplayer);
  int i, at = 0;
  char hist[(DEEP_STACK_HIST + 1) * 12];

  if (sp == NULL) {
    return;
  }

  for (i = 0; i < DEEP_STACK_SLOTS; i++) {
    sp->site[i].now = 0;
  }

  unit_list_iterate(pplayer->units, punit) {
    struct deep_stack_site *s;

    if (!unit_has_type_flag(punit, UTYF_WORKERS)
        || !is_tile_activity(punit->activity)) {
      continue;
    }
    s = deep_stack_find(sp, unit_tile(punit), punit->activity,
                        punit->activity_target);
    if (s != NULL) {
      s->now++;
    }
  } unit_list_iterate_end;

  for (i = 0; i < DEEP_STACK_SLOTS; i++) {
    struct deep_stack_site *s = &sp->site[i];
    int bucket;

    if (s->tile < 0) {
      continue;
    }
    if (s->now <= 0) {
      deep_stack_close(sp, s);
      continue;
    }
    sp->turns++;
    sp->wturns += s->now;
    if (s->now > 1) {
      sp->stk++;
    }
    bucket = MIN(s->now, DEEP_STACK_HIST) - 1;
    sp->hist[bucket]++;
    s->turns++;
    s->maxn = MAX(s->maxn, s->now);
  }

  if (sp->turns <= 0) {
    return;
  }

  for (i = 0; i < DEEP_STACK_HIST; i++) {
    at += fc_snprintf(hist + at, sizeof(hist) - at, "%s%d",
                      i == 0 ? "" : ":", sp->hist[i]);
  }

  /* n= is the distribution the whole census exists for: worksite-turns
   * by simultaneous worker count, 1:2:3:4:5+. stk= is the n>=2 part of
   * it summed, so stk/turns is the stacking RATE. done= worksites whose
   * improvement appeared, split dsolo/dstk by whether more than one
   * worker was ever on them, with tsolo=/tstk= the calendar turns those
   * two sets took -- a completion-time comparison that is observational
   * within one arm and paired between two. lost= worksites abandoned
   * with nothing to show. */
  log_base(deep_metric_log_level(),
           "deep-stack turn=%d player=\"%s\" sites=%d turns=%d wturns=%d"
           " stk=%d n=%s done=%d dsolo=%d dstk=%d tsolo=%d tstk=%d"
           " lost=%d over=%d",
           game.info.turn, player_name(pplayer), sp->sites, sp->turns,
           sp->wturns, sp->stk, hist, sp->done, sp->dsolo, sp->dstk,
           sp->tsolo, sp->tstk, sp->lost, sp->over);
}
