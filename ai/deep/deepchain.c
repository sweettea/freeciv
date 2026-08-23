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

/* common */
#include "city.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "terrain.h"
#include "tile.h"
#include "world_object.h"

/* server/advisors */
#include "advtools.h"
#include "infracache.h"

/* ai/deep */
#include "deepai.h"
#include "deepterra.h"
#include "deeptuning.h"

#include "deepchain.h"

/* Per-tile marks over the whole game, with the compact index list the
 * sweep walks beside them -- deepfunnel's shape, for deepfunnel's
 * reason: the sweep should cost one pass over the tiles this player
 * ever priced a chain on, not one over the map. */
#define DCT_LIFTED 0x1  /* A chain raised this tile's price. */
#define DCT_WORK   0x2  /* Its terrain has changed since. */
#define DCT_DONE   0x4  /* It reached the terrain the price was for. */
#define DCT_STRAY  0x8  /* It changed to something off the chain. */
#define DCT_LOST   0x10 /* It stopped being ours. */

struct deep_chain_plr {
  unsigned char *mark;   /* MAP_INDEX_SIZE entries, or NULL. */
  unsigned char *want;   /* Terrain number the price was paid for. */
  unsigned char *was;    /* Terrain number the tile had when priced. */
  int marksize;
  int *seen;             /* Tile indices ever lifted, compacted. */
  int nseen;
  int seenalloc;

  /* Cumulative over the game. */
  int cand;
  int multi;
  int lift;
  double gain;
  int tiles;
  int work;
  int done;
  int stray;
  int lost;
};

static struct deep_chain_plr *deep_chain_plr[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  This player's ledger, created on demand. NULL when the metric lines
  are not being written: the whole ledger is instrumentation, so when
  nothing will read it, nothing is kept and nothing is spent.
**************************************************************************/
static struct deep_chain_plr *deep_chain_get(const struct player *pplayer,
                                             bool create)
{
  int pn = player_number(pplayer);
  struct deep_chain_plr *cp;

  if (pn < 0 || pn >= MAX_NUM_PLAYER_SLOTS) {
    return NULL;
  }
  cp = deep_chain_plr[pn];
  if (cp != NULL || !create
      || !log_do_output_for_level(deep_metric_log_level())) {
    return cp;
  }

  cp = fc_calloc(1, sizeof(*cp));
  cp->marksize = MAP_INDEX_SIZE;
  cp->mark = fc_calloc(cp->marksize, sizeof(*cp->mark));
  cp->want = fc_calloc(cp->marksize, sizeof(*cp->want));
  cp->was = fc_calloc(cp->marksize, sizeof(*cp->was));
  deep_chain_plr[pn] = cp;

  return cp;
}

/**********************************************************************//**
  Note that a chain raised the price of 'ptile' towards 'target'.
**************************************************************************/
static void deep_chain_note_lift(const struct player *pplayer,
                                 const struct tile *ptile,
                                 const struct terrain *target,
                                 adv_want gain)
{
  struct deep_chain_plr *cp = deep_chain_get(pplayer, TRUE);
  int idx;

  if (cp == NULL) {
    return;
  }
  cp->lift++;
  cp->gain += gain;

  idx = tile_index(ptile);
  if (idx < 0 || idx >= cp->marksize) {
    return;
  }
  if ((cp->mark[idx] & DCT_LIFTED) == 0) {
    cp->mark[idx] |= DCT_LIFTED;
    cp->tiles++;
    if (cp->nseen >= cp->seenalloc) {
      cp->seenalloc = (cp->seenalloc == 0 ? 64 : cp->seenalloc * 2);
      cp->seen = fc_realloc(cp->seen, cp->seenalloc * sizeof(*cp->seen));
    }
    cp->seen[cp->nseen++] = idx;
    cp->was[idx] = terrain_number(tile_terrain(ptile));
  }
  /* The target is the LAST one priced, not the first: the tile is
   * re-priced from scratch every turn, and what it is being worked
   * towards now is the current answer. */
  cp->want[idx] = terrain_number(target);
}

/**********************************************************************//**
  Is 'to' still reachable from 'from' by cultivating and planting? Asked
  of a tile whose terrain changed, to tell a step along the chain from a
  step off it.
**************************************************************************/
static bool deep_chain_still_reaches(const struct terrain *from,
                                     const struct terrain *to)
{
  const enum unit_activity acts[] = { ACTIVITY_CULTIVATE, ACTIVITY_PLANT };
  int a;

  if (from == to) {
    return TRUE;
  }
  for (a = 0; a < 2; a++) {
    struct deep_terra_step reach[DEEP_TERRA_REACH_MAX];
    int n = deep_terra_reach(from, acts[a], reach);
    int i;

    for (i = 0; i < n; i++) {
      if (reach[i].pterrain == to) {
        return TRUE;
      }
    }
  }

  return FALSE;
}

/**********************************************************************//**
  What 'act' is worth on 'ptile' to 'pcity', given the one-step value
  the advisors computed. Registered on the advisors' terrain-value hook;
  see deepchain.h.
**************************************************************************/
static adv_want deep_chain_value(const struct city *pcity,
                                 const struct tile *ptile,
                                 enum unit_activity act,
                                 const struct terrain *result,
                                 adv_want one_step)
{
  const struct player *pplayer;
  const struct deep_tuning *tune;
  struct deep_chain_plr *cp;
  struct deep_terra_step reach[DEEP_TERRA_REACH_MAX];
  const struct terrain *target = NULL;
  adv_want best = one_step;
  int n, i;

  if (pcity == NULL || ptile == NULL) {
    return one_step;
  }
  pplayer = city_owner(pcity);
  if (pplayer == NULL || pplayer->ai != deep_ai_get_self()) {
    /* A stock seat in a mixed game keeps stock's price, which is what
     * makes the mixed cell a control rather than a second treatment. */
    return one_step;
  }

  n = deep_terra_reach(tile_terrain(ptile), act, reach);

  /* Counted BEFORE the gate, so that the shut arm publishes the
   * denominator the open arm's rate is taken against: "the advisor
   * offered a terrain change at all" and "a chain was worth paying for"
   * are different questions. */
  cp = deep_chain_get(pplayer, TRUE);
  if (cp != NULL) {
    cp->cand++;
    if (n > 1) {
      cp->multi++;
    }
  }

  tune = deep_tuning_for(pplayer);
  if (tune->terrain_chain_gate <= 0 || n <= 1) {
    return one_step;
  }

  /* reach[0] is the immediate result, whose price is 'one_step' -- the
   * caller has already computed exactly that, on exactly this tile, and
   * recomputing it here would only be a way to disagree with it. */
  fc_assert(reach[0].pterrain == result);

  for (i = 1; i < n; i++) {
    struct tile *vtile;
    adv_want value;

    if (tile_city(ptile)
        && terrain_has_flag(reach[i].pterrain, TER_NO_CITIES)) {
      /* The same refusal the one-step evaluators make. */
      continue;
    }

    vtile = tile_virtual_new(ptile);
    tile_change_terrain(vtile, (struct terrain *) reach[i].pterrain);
    value = city_tile_value(pcity, vtile, 0, 0);
    tile_virtual_destroy(vtile);

    /* Only the turns BEYOND this first step: the caller amortizes the
     * first step and the walk to the tile itself. */
    value = amortize(value, reach[i].turns - reach[0].turns);
    if (value > best) {
      best = value;
      target = reach[i].pterrain;
    }
  }

  if (target != NULL) {
    deep_chain_note_lift(pplayer, ptile, target, best - one_step);
  }

  return best;
}

/**********************************************************************//**
  Register the chain valuation with the advisors. See deepchain.h: the
  registration is unconditional and the gate is per-seat.
**************************************************************************/
void deep_chain_register(void)
{
  adv_terrain_value_hook_set(deep_chain_value);
}

/**********************************************************************//**
  Follow the lifted tiles and print this player's chain funnel. See
  deepchain.h for what the stages mean and why they are tiles.
**************************************************************************/
void deep_chain_census(struct player *pplayer)
{
  const struct civ_map *nmap = &(wld.map);
  struct deep_chain_plr *cp = deep_chain_get(pplayer, FALSE);
  int i, live = 0;

  if (cp == NULL || cp->cand <= 0) {
    return;
  }

  for (i = 0; i < cp->nseen; i++) {
    int idx = cp->seen[i];
    struct tile *ptile = index_to_tile(nmap, idx);
    const struct terrain *now;

    if (ptile == NULL
        || (cp->mark[idx] & (DCT_DONE | DCT_STRAY | DCT_LOST)) != 0) {
      continue;
    }
    if (tile_owner(ptile) != pplayer) {
      /* Somebody else's ground now: the price we paid was for a tile we
       * no longer decide anything about. Not a failure of the chain. */
      cp->mark[idx] |= DCT_LOST;
      cp->lost++;
      continue;
    }

    now = tile_terrain(ptile);
    if (terrain_number(now) != cp->was[idx]
        && (cp->mark[idx] & DCT_WORK) == 0) {
      cp->mark[idx] |= DCT_WORK;
      cp->work++;
    }
    if (terrain_number(now) == cp->want[idx]) {
      cp->mark[idx] |= DCT_DONE;
      cp->done++;
    } else if ((cp->mark[idx] & DCT_WORK) != 0
               && !deep_chain_still_reaches(now,
                                            terrain_by_number(cp->want[idx]))) {
      /* Worked, and now standing on a terrain from which the terrain we
       * paid for cannot be reached at all -- somebody transformed it,
       * or the ruleset's graph has no way back. */
      cp->mark[idx] |= DCT_STRAY;
      cp->stray++;
    } else {
      live++;
    }
  }

  log_base(deep_metric_log_level(),
           "deep-terrachain turn=%d player=\"%s\" cand=%d multi=%d"
           " lift=%d gain=%.0f tiles=%d work=%d done=%d stray=%d"
           " lost=%d live=%d",
           game.info.turn, player_name(pplayer), cp->cand, cp->multi,
           cp->lift, cp->gain, cp->tiles, cp->work, cp->done, cp->stray,
           cp->lost, live);
}
