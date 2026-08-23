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
#include "tile.h"
#include "unit.h"
#include "unittype.h"
#include "world_object.h"

#include "deeptrunk.h"
#include "deeptuning.h"

#include "deepfunnel.h"

/* Live assignments per player. A worker holds at most one corridor
 * assignment, so the table is bounded by the worker count; the cap is a
 * BOOKKEEPING bound, not a game bound, exactly as deepfort's ring is.
 * An assignment that cannot be recorded because the table is full is
 * counted in `over` and then simply not followed -- it costs the funnel
 * one sample and costs the game nothing. */
#define DEEP_FUNNEL_SLOTS 512

struct deep_funnel_task {
  int unit;        /* Unit id; 0 marks a free slot. */
  int tile;        /* Tile index of the corridor tile held. */
  int turn;        /* Turn the assignment was last renewed. */
  int since;       /* Turn it was first made. */
  bool working;    /* The worker has begun the road activity there. */
  bool seen;       /* The task source ran for this worker this turn. */
};

/* Per-tile marks over the whole game, allocated on this player's first
 * pick. The compact index list beside it is what the completion sweep
 * walks, so the sweep costs one pass over the tiles this player ever
 * wanted rather than one over the map. */
#define DFT_PICKED 0x1
#define DFT_WORKED 0x2
#define DFT_DONE   0x4

struct deep_funnel_plr {
  struct deep_funnel_task task[DEEP_FUNNEL_SLOTS];
  unsigned char *mark;   /* MAP_INDEX_SIZE entries, or NULL. */
  int marksize;
  int *seen;             /* Tile indices ever picked, compacted. */
  int nseen;
  int seenalloc;

  /* The funnel, cumulative over the game. */
  int picks;
  int tiles;
  int start;
  int wtiles;
  int wturns;
  int done;
  int dwork;
  int dsalv;
  int over;
  int drop[DFC_CAUSE_COUNT];
};

static struct deep_funnel_plr *deep_funnel_plr[MAX_NUM_PLAYER_SLOTS];

static const char *const deep_funnel_cause_name[DFC_CAUSE_COUNT] = {
  "roaded", "replan", "unsafe", "taken", "nopath", "noact", "price",
  "switch", "outbid", "request", "norun", "gone", "flee", "stop"
};

/**********************************************************************//**
  This player's ledger, created on demand. NULL when the metric lines
  are not being written: the whole ledger is instrumentation, so when
  nothing will read it, nothing is kept and nothing is spent.
**************************************************************************/
static struct deep_funnel_plr *deep_funnel_get(const struct player *pplayer,
                                               bool create)
{
  int pn = player_number(pplayer);
  struct deep_funnel_plr *fp;

  if (pn < 0 || pn >= MAX_NUM_PLAYER_SLOTS) {
    return NULL;
  }
  fp = deep_funnel_plr[pn];
  if (fp != NULL || !create
      || !log_do_output_for_level(deep_metric_log_level())) {
    return fp;
  }

  fp = fc_calloc(1, sizeof(*fp));
  fp->marksize = MAP_INDEX_SIZE;
  fp->mark = fc_calloc(fp->marksize, sizeof(*fp->mark));
  deep_funnel_plr[pn] = fp;

  return fp;
}

/**********************************************************************//**
  The slot holding this worker's corridor assignment, or NULL.
**************************************************************************/
static struct deep_funnel_task *
deep_funnel_find(struct deep_funnel_plr *fp, int uid)
{
  int i;

  for (i = 0; i < DEEP_FUNNEL_SLOTS; i++) {
    if (fp->task[i].unit == uid) {
      return &fp->task[i];
    }
  }

  return NULL;
}

/**********************************************************************//**
  Charge one dropped assignment to 'cause' and free its slot.
**************************************************************************/
static void deep_funnel_drop(struct deep_funnel_plr *fp,
                             struct deep_funnel_task *task,
                             enum deep_funnel_cause cause)
{
  fp->drop[cause]++;
  task->unit = 0;
}

/**********************************************************************//**
  Note that this player has now wanted a road on 'idx' at some point.
  Returns TRUE the first time, which is what makes `tiles` a count of
  DISTINCT corridor tiles rather than of assignments.
**************************************************************************/
static bool deep_funnel_mark(struct deep_funnel_plr *fp, int idx,
                             unsigned char bit)
{
  if (fp->mark == NULL || idx < 0 || idx >= fp->marksize
      || (fp->mark[idx] & bit) != 0) {
    return FALSE;
  }
  fp->mark[idx] |= bit;

  if (bit == DFT_PICKED) {
    if (fp->nseen >= fp->seenalloc) {
      fp->seenalloc = MAX(2 * fp->seenalloc, 64);
      fp->seen = fc_realloc(fp->seen, fp->seenalloc * sizeof(*fp->seen));
    }
    fp->seen[fp->nseen++] = idx;
  }

  return TRUE;
}

/**********************************************************************//**
  The corridor tile this worker was last assigned, or NULL. Also marks
  the assignment as having reached the task source this turn, so the
  end-of-phase sweep can tell "the source considered this worker and
  chose otherwise" from "the source never saw this worker at all".
**************************************************************************/
struct tile *deep_funnel_open(const struct player *pplayer,
                              const struct unit *punit)
{
  struct deep_funnel_plr *fp = deep_funnel_get(pplayer, FALSE);
  struct deep_funnel_task *task;

  if (fp == NULL) {
    return NULL;
  }
  task = deep_funnel_find(fp, punit->id);
  if (task == NULL) {
    return NULL;
  }
  task->seen = TRUE;

  return index_to_tile(&(wld.map), task->tile);
}

/**********************************************************************//**
  The task source has finished choosing for this worker. 'picked' is the
  corridor tile it assigned (NULL for none) and 'prev_stage' is how far
  the tile the worker already held got through the source's own loop.

  Three outcomes, and the middle one is the whole point of the ledger:
  the assignment is renewed (the worker keeps walking to the same tile),
  it is dropped with a cause read off the stage it reached, or a fresh
  assignment begins.
**************************************************************************/
void deep_funnel_close(const struct player *pplayer,
                       const struct unit *punit,
                       struct tile *picked, int prev_stage)
{
  struct deep_funnel_plr *fp = deep_funnel_get(pplayer, picked != NULL);
  struct deep_funnel_task *task;
  int i;

  if (fp == NULL) {
    return;
  }
  task = deep_funnel_find(fp, punit->id);

  if (task != NULL) {
    if (picked != NULL && tile_index(picked) == task->tile) {
      /* Renewed: the same tile, one more turn of walking (or of being
       * re-confirmed while standing on it). */
      task->turn = game.info.turn;
      fp->picks++;

      return;
    }

    switch (prev_stage) {
    case DEEP_FUNNEL_ST_ABSENT:
      deep_funnel_drop(fp, task,
                       deep_trunk_tile_complete(pplayer,
                                                index_to_tile(&(wld.map),
                                                              task->tile))
                       ? DFC_ROADED : DFC_REPLAN);
      break;
    case DEEP_FUNNEL_ST_UNSAFE: deep_funnel_drop(fp, task, DFC_UNSAFE); break;
    case DEEP_FUNNEL_ST_TAKEN:  deep_funnel_drop(fp, task, DFC_TAKEN);  break;
    case DEEP_FUNNEL_ST_NOPATH: deep_funnel_drop(fp, task, DFC_NOPATH); break;
    case DEEP_FUNNEL_ST_NOACT:  deep_funnel_drop(fp, task, DFC_NOACT);  break;
    case DEEP_FUNNEL_ST_PRICE:  deep_funnel_drop(fp, task, DFC_PRICE);  break;
    default:
      deep_funnel_drop(fp, task, picked == NULL ? DFC_OUTBID : DFC_SWITCH);
      break;
    }
  }

  if (picked == NULL) {
    return;
  }

  fp->picks++;
  if (deep_funnel_mark(fp, tile_index(picked), DFT_PICKED)) {
    fp->tiles++;
  }

  for (i = 0; i < DEEP_FUNNEL_SLOTS; i++) {
    if (fp->task[i].unit == 0) {
      fp->task[i].unit = punit->id;
      fp->task[i].tile = tile_index(picked);
      fp->task[i].turn = game.info.turn;
      fp->task[i].since = game.info.turn;
      fp->task[i].working = FALSE;
      fp->task[i].seen = TRUE;

      return;
    }
  }
  fp->over++;
}

/**********************************************************************//**
  A worker began its activity on 'ptile'. Counted as an arrival only
  when that is the corridor tile it holds -- a worker that started an
  irrigation somewhere else is not a corridor arrival, and the ledger
  will charge its corridor assignment on the next pass instead.
**************************************************************************/
void deep_funnel_note_work(const struct player *pplayer,
                           const struct unit *punit,
                           const struct tile *ptile)
{
  struct deep_funnel_plr *fp = deep_funnel_get(pplayer, FALSE);
  struct deep_funnel_task *task;

  if (fp == NULL || ptile == NULL) {
    return;
  }
  task = deep_funnel_find(fp, punit->id);
  if (task == NULL || task->tile != tile_index(ptile)) {
    return;
  }

  fp->start++;
  task->working = TRUE;
  if (deep_funnel_mark(fp, task->tile, DFT_WORKED)) {
    fp->wtiles++;
  }
}

/**********************************************************************//**
  A city work request took this worker before the task source ran, so a
  corridor assignment it held ends here rather than in the source.
**************************************************************************/
void deep_funnel_note_request(const struct player *pplayer,
                              const struct unit *punit)
{
  struct deep_funnel_plr *fp = deep_funnel_get(pplayer, FALSE);
  struct deep_funnel_task *task;

  if (fp == NULL) {
    return;
  }
  task = deep_funnel_find(fp, punit->id);
  if (task != NULL) {
    task->seen = TRUE;
    deep_funnel_drop(fp, task, DFC_REQUEST);
  }
}

/**********************************************************************//**
  The safety pass in settler_cont just idled this worker mid-activity.
  Charged here and not in the sweep because this is the site that made
  the decision; the worker is about to be re-tasked by settler_run in
  the same loop iteration, and if it picks the same corridor tile again
  that shows up as a fresh assignment, which is what churn looks like.
**************************************************************************/
void deep_funnel_note_flee(const struct player *pplayer,
                           const struct unit *punit)
{
  struct deep_funnel_plr *fp = deep_funnel_get(pplayer, FALSE);
  struct deep_funnel_task *task;

  if (fp == NULL) {
    return;
  }
  task = deep_funnel_find(fp, punit->id);
  if (task != NULL && task->working) {
    deep_funnel_drop(fp, task, DFC_FLEE);
  }
}

/**********************************************************************//**
  End of this player's phase. Three passes, in the order that keeps them
  independent:

  1. Every assignment the task source did NOT consider this turn is
     resolved. A worker still building its road on the tile is credited
     one work turn and left alone; anything else has ended, and the
     cause is death, a mid-build stop, or the source never running.
  2. Every corridor tile this player ever wanted is checked for a road,
     once, and the first time one appears it is counted -- split by
     whether a deep worker ever began work there, because extras do not
     record who built them.
  3. The line.
**************************************************************************/
void deep_funnel_census(struct player *pplayer)
{
  struct deep_funnel_plr *fp = deep_funnel_get(pplayer, FALSE);
  const struct civ_map *nmap = &(wld.map);
  char causes[DFC_CAUSE_COUNT * 24];
  int i, at = 0, live = 0, drops = 0;

  if (fp == NULL) {
    return;
  }

  for (i = 0; i < DEEP_FUNNEL_SLOTS; i++) {
    struct deep_funnel_task *task = &fp->task[i];
    struct unit *punit;
    struct tile *ptile;

    if (task->unit == 0) {
      continue;
    }
    if (task->seen) {
      task->seen = FALSE;
      continue;
    }

    punit = game_unit_by_number(task->unit);
    ptile = index_to_tile(nmap, task->tile);
    if (punit == NULL || unit_owner(punit) != pplayer
        || !unit_has_type_flag(punit, UTYF_WORKERS)) {
      deep_funnel_drop(fp, task, DFC_GONE);
      continue;
    }
    if (task->working) {
      if (same_pos(unit_tile(punit), ptile)
          && activity_to_extra_cause(punit->activity) == EC_ROAD) {
        fp->wturns++;
        continue;
      }
      deep_funnel_drop(fp, task,
                       deep_trunk_tile_complete(pplayer, ptile)
                       ? DFC_ROADED : DFC_STOP);
      continue;
    }
    /* Still walking, and the source never looked at it: the worker was
     * held, mid-activity elsewhere, out of moves or under orders. */
    deep_funnel_drop(fp, task, DFC_NORUN);
  }

  for (i = 0; i < fp->nseen; i++) {
    int idx = fp->seen[i];

    if ((fp->mark[idx] & DFT_DONE) != 0
        || !deep_trunk_tile_complete(pplayer, index_to_tile(nmap, idx))) {
      continue;
    }
    fp->mark[idx] |= DFT_DONE;
    fp->done++;
    if ((fp->mark[idx] & DFT_WORKED) != 0) {
      fp->dwork++;
    } else {
      fp->dsalv++;
    }
  }

  for (i = 0; i < DEEP_FUNNEL_SLOTS; i++) {
    if (fp->task[i].unit != 0) {
      live++;
    }
  }

  for (i = 0; i < DFC_CAUSE_COUNT; i++) {
    drops += fp->drop[i];
    at += fc_snprintf(causes + at, sizeof(causes) - at, "%s%s=%d",
                      i == 0 ? "" : ",", deep_funnel_cause_name[i],
                      fp->drop[i]);
  }

  if (fp->picks > 0) {
    /* Cumulative over the game, so the LAST such line of a game is that
     * game's funnel -- the same shape deepfort's want histogram uses,
     * and for the same reason: the question "what became of a corridor
     * task" cannot be answered by pooling per-turn medians. */
    log_base(deep_metric_log_level(),
             "deep-trunkfun turn=%d player=\"%s\" picks=%d tiles=%d"
             " start=%d wtiles=%d wturns=%d done=%d dwork=%d dsalv=%d"
             " live=%d over=%d drops=%d %s",
             game.info.turn, player_name(pplayer), fp->picks, fp->tiles,
             fp->start, fp->wtiles, fp->wturns, fp->done, fp->dwork,
             fp->dsalv, live, fp->over, drops, causes);
  }
}
