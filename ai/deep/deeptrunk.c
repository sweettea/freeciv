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

#include <math.h>

/* utility */
#include "log.h"
#include "mem.h"

/* common */
#include "city.h"
#include "extras.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "terrain.h"
#include "tile.h"
#include "world_object.h"

/* server/advisors */
#include "advtools.h"

#include "deeptuning.h"

#include "deeptrunk.h"

/* One planned link between two cities, measured over the tiles the
 * corridor walk actually visited. */
struct deep_trunk_link {
  int terr;                  /* Terrain movement cost over the corridor,
                              * in move fragments. */
  int len;                   /* Corridor tiles (city ends excluded). */
  int gap;                   /* Of those, still without a road that
                              * speeds movement. */
};

/* The plan, memoised on (turn, player): one link index per tile
 * (0 = none, otherwise link+1) and the links themselves. */
static int *deep_trunk_tile = NULL;
static int deep_trunk_tiles = 0;
static struct deep_trunk_link *deep_trunk_links = NULL;
static int deep_trunk_nlinks = 0;
static int deep_trunk_alloc = 0;
static int deep_trunk_turn = -1;
static int deep_trunk_plr = -1;

/* Per-player census, reset per turn by deep_trunk_census(). */
static int deep_trunk_c_links[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_gap[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_hits[MAX_NUM_PLAYER_SLOTS];
static double deep_trunk_c_max[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  Does this tile already carry a road that speeds movement? A corridor
  tile that does needs no further work for the link's sake -- the
  ruleset's later upgrades (rail) are a different question, priced by
  the advisor's own move-cost term on the tile.
**************************************************************************/
static bool deep_trunk_roaded(const struct tile *ptile)
{
  extra_type_by_cause_iterate(EC_ROAD, pextra) {
    if (tile_has_extra(ptile, pextra)
        && road_provides_move_bonus(extra_road_get(pextra))) {
      return TRUE;
    }
  } extra_type_by_cause_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  Walk a corridor from 'from' to 'to', greedily: each step goes to the
  land neighbour strictly nearer the destination, so the walk cannot
  loop and terminates in at most real_map_distance() steps. Returns the
  number of intermediate tiles written to 'path', or -1 when the walk
  runs out of land -- a link that has to cross water is not a road link
  and is dropped whole rather than half-marked.

  Greedy and not pathfinding on purpose. This runs once per city pair
  per turn and its output is a corridor to VALUE, not a route to
  follow; a worker's own pathfinding decides how it gets there.
**************************************************************************/
static int deep_trunk_walk(const struct civ_map *nmap,
                           struct tile *from, struct tile *to,
                           struct tile **path, int maxlen)
{
  struct tile *cur = from;
  int dist = real_map_distance(from, to);
  int n = 0;

  while (dist > 1) {
    struct tile *step = NULL;
    int stepdist = dist;

    adjc_iterate(nmap, cur, atile) {
      int d;

      if (is_ocean_tile(atile)) {
        continue;
      }
      d = real_map_distance(atile, to);
      if (d < stepdist) {
        stepdist = d;
        step = atile;
      }
    } adjc_iterate_end;

    if (step == NULL || n >= maxlen) {
      return -1;
    }
    path[n++] = step;
    cur = step;
    dist = stepdist;
  }

  return n;
}

/**********************************************************************//**
  Span this player's cities with a minimum spanning tree on map
  distance and turn its edges into corridors.

  An MST and not every pair: the empire needs each city reachable from
  the rest, and n-1 links do that, while pricing all n(n-1)/2 pairs
  would pile the same want onto whatever tiles sit between the two
  densest clusters. Cities on different continents simply never share
  an edge -- Prim restarts on a new component instead, so an island
  empire gets one tree per island rather than one link across water.
**************************************************************************/
static void deep_trunk_build(const struct player *pplayer)
{
  const struct civ_map *nmap = &(wld.map);
  int ncities = city_list_size(pplayer->cities);
  struct tile **ctile, **path;
  bool *seen;
  int *near_to, *near_d;
  int i, j, maxpath;

  if (deep_trunk_tiles != MAP_INDEX_SIZE) {
    /* First call, or a new game with a differently sized map. */
    free(deep_trunk_tile);
    deep_trunk_tiles = MAP_INDEX_SIZE;
    deep_trunk_tile = fc_calloc(deep_trunk_tiles, sizeof(*deep_trunk_tile));
  } else {
    memset(deep_trunk_tile, 0,
           deep_trunk_tiles * sizeof(*deep_trunk_tile));
  }
  deep_trunk_nlinks = 0;

  if (ncities < 2) {
    return;
  }

  if (deep_trunk_alloc < ncities) {
    free(deep_trunk_links);
    deep_trunk_alloc = ncities;
    deep_trunk_links = fc_malloc(deep_trunk_alloc
                                 * sizeof(*deep_trunk_links));
  }

  ctile = fc_malloc(ncities * sizeof(*ctile));
  seen = fc_calloc(ncities, sizeof(*seen));
  near_to = fc_malloc(ncities * sizeof(*near_to));
  near_d = fc_malloc(ncities * sizeof(*near_d));

  i = 0;
  city_list_iterate(pplayer->cities, pcity) {
    ctile[i++] = city_tile(pcity);
  } city_list_iterate_end;

  /* A greedy walk that must strictly close the gap every step cannot
   * take more steps than the map is wide plus tall. */
  maxpath = wld.map.xsize + wld.map.ysize;
  path = fc_malloc(maxpath * sizeof(*path));

  for (i = 0; i < ncities; i++) {
    near_to[i] = -1;
    near_d[i] = FC_INFINITY;
  }
  seen[0] = TRUE;
  for (j = 1; j < ncities; j++) {
    if (tile_continent(ctile[j]) == tile_continent(ctile[0])) {
      near_to[j] = 0;
      near_d[j] = real_map_distance(ctile[0], ctile[j]);
    }
  }

  for (i = 1; i < ncities; i++) {
    int pick = -1, best = FC_INFINITY;

    for (j = 0; j < ncities; j++) {
      if (!seen[j] && near_d[j] < best) {
        best = near_d[j];
        pick = j;
      }
    }
    if (pick < 0) {
      /* Every remaining city is on another landmass: start a fresh
       * component at the first one still unseen. */
      for (j = 0; j < ncities; j++) {
        if (!seen[j]) {
          pick = j;
          break;
        }
      }
      if (pick < 0) {
        break;
      }
      near_to[pick] = -1;
    }

    seen[pick] = TRUE;

    if (near_to[pick] >= 0) {
      int len = deep_trunk_walk(nmap, ctile[near_to[pick]], ctile[pick],
                                path, maxpath);

      if (len > 0) {
        struct deep_trunk_link *link = &deep_trunk_links[deep_trunk_nlinks];
        int t;

        link->terr = 0;
        link->len = 0;
        link->gap = 0;
        for (t = 0; t < len; t++) {
          link->terr += tile_terrain(path[t])->movement_cost * SINGLE_MOVE;
          link->len++;
          if (!deep_trunk_roaded(path[t])) {
            link->gap++;
          }
        }
        if (link->gap > 0) {
          /* A finished link is worth nothing more; only the tiles that
           * still need work carry the want, and only if some do. */
          for (t = 0; t < len; t++) {
            int idx = tile_index(path[t]);

            if (deep_trunk_tile[idx] == 0 && !deep_trunk_roaded(path[t])) {
              deep_trunk_tile[idx] = deep_trunk_nlinks + 1;
            }
          }
          deep_trunk_nlinks++;
        }
      }
    }

    for (j = 0; j < ncities; j++) {
      if (!seen[j]
          && tile_continent(ctile[j]) == tile_continent(ctile[pick])) {
        int d = real_map_distance(ctile[pick], ctile[j]);

        if (d < near_d[j]) {
          near_d[j] = d;
          near_to[j] = pick;
        }
      }
    }
  }

  free(ctile);
  free(seen);
  free(near_to);
  free(near_d);
  free(path);
}

/**********************************************************************//**
  The trunk want for building 'target' on 'ptile'.

  The link is worth the movement it saves on one traversal once it is
  whole: the terrain cost of the corridor minus what the same corridor
  would cost under this road, in whole movement points. That is the
  connectivity benefit stated in a quantity the ruleset already
  defines, not a new scale.

  It is then discounted by amortize() over the tiles still missing --
  a link one tile short pays almost at once, a link twelve tiles short
  pays much later, and the same worker-turn is therefore spent closing
  the nearly-finished link first. The gap in tiles stands in for the
  gap in turns: the two differ by the worker rate, which is a
  multiplier common to every link and so cannot change their order.

  trunk_road_gain converts the saved movement into the advisor's tile
  currency and gates the whole term at 0.
**************************************************************************/
adv_want deep_trunk_want(const struct player *pplayer,
                         const struct extra_type *target,
                         const struct tile *ptile)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct deep_trunk_link *link;
  const struct road_type *proad;
  int pn = player_number(pplayer);
  int saving, slot;
  adv_want want;

  if (tune->trunk_road_gain <= 0.0 || target == NULL) {
    return 0.0;
  }
  if (!is_extra_caused_by(target, EC_ROAD)) {
    return 0.0;
  }
  proad = extra_road_get(target);
  if (proad == NULL || !road_provides_move_bonus(proad)) {
    return 0.0;
  }

  if (deep_trunk_turn != game.info.turn || deep_trunk_plr != pn) {
    deep_trunk_turn = game.info.turn;
    deep_trunk_plr = pn;
    deep_trunk_build(pplayer);
    deep_trunk_c_links[pn] = deep_trunk_nlinks;
    deep_trunk_c_gap[pn] = 0;
    for (slot = 0; slot < deep_trunk_nlinks; slot++) {
      deep_trunk_c_gap[pn] += deep_trunk_links[slot].gap;
    }
  }

  slot = deep_trunk_tile[tile_index(ptile)];
  if (slot == 0) {
    return 0.0;
  }
  link = &deep_trunk_links[slot - 1];

  saving = link->terr - link->len * proad->move_cost;
  if (saving <= 0) {
    return 0.0;
  }

  want = tune->trunk_road_gain * ((double) saving / SINGLE_MOVE)
         * amortize(1.0, link->gap);

  deep_trunk_c_hits[pn]++;
  if (want > deep_trunk_c_max[pn]) {
    deep_trunk_c_max[pn] = want;
  }

  return want;
}

/**********************************************************************//**
  One line per player per turn: the links the plan spans, the corridor
  tiles still without a road, how many candidates the want reached and
  the largest it issued. links>0 with hits=0 is the ceiling in
  deeptrunk.h speaking -- a planned corridor the advisor never offered
  because no city radius covers it.
**************************************************************************/
void deep_trunk_census(struct player *pplayer)
{
  int pn = player_number(pplayer);

  if (log_do_output_for_level(deep_metric_log_level())
      && deep_trunk_c_links[pn] > 0) {
    log_base(deep_metric_log_level(),
             "deep-trunk turn=%d player=\"%s\" links=%d gap=%d hits=%d"
             " maxwant=%.2f",
             game.info.turn, player_name(pplayer), deep_trunk_c_links[pn],
             deep_trunk_c_gap[pn], deep_trunk_c_hits[pn],
             deep_trunk_c_max[pn]);
  }

  deep_trunk_c_links[pn] = 0;
  deep_trunk_c_gap[pn] = 0;
  deep_trunk_c_hits[pn] = 0;
  deep_trunk_c_max[pn] = 0.0;
}
