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
#include "shared.h"
#include "support.h"

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

/* Distance histogram for beyond-ceiling picks: one bucket per map
 * distance 0..DEEP_TRUNK_SRC_HIST-1, and a last bucket for everything
 * farther. Instrumentation shape, not a tunable: it changes what the
 * log line says, never what any unit does. */
#define DEEP_TRUNK_SRC_HIST 16

/* One planned link between two cities, measured over the tiles the
 * corridor walk actually visited. */
struct deep_trunk_link {
  int terr;                  /* Terrain movement cost over the corridor,
                              * in move fragments. */
  int len;                   /* Corridor tiles (city ends excluded). */
  int gap;                   /* Of those, still without a road that
                              * speeds movement. */
  bool ally;                 /* Far endpoint is an allied city, not one
                              * of ours (W12 slice 3). */
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

/* The corridor gap tiles the advisor can NEVER offer: those outside
 * every own city radius (autoworkers.c:586). Tile indices, rebuilt
 * with the plan. */
static int *deep_trunk_beyond = NULL;
static int deep_trunk_nbeyond = 0;
static int deep_trunk_beyond_alloc = 0;

/* Per-player census, reset per turn by deep_trunk_census(). */
static int deep_trunk_c_links[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_gap[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_hits[MAX_NUM_PLAYER_SLOTS];
static double deep_trunk_c_max[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_beyond[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_src_off[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_src_pick[MAX_NUM_PLAYER_SLOTS];
static int
deep_trunk_c_src_hist[MAX_NUM_PLAYER_SLOTS][DEEP_TRUNK_SRC_HIST + 1];
/* W12 slice 3, the ally corridors' own instruments: links planned to
 * an allied city, their corridor tiles still missing a road, and how
 * many of those the task source offered and picked. */
static int deep_trunk_c_ally_links[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_ally_gap[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_ally_off[MAX_NUM_PLAYER_SLOTS];
static int deep_trunk_c_ally_pick[MAX_NUM_PLAYER_SLOTS];

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

  With 'clip' the walk also stops at the first tile owned by a player
  'pplayer' is not allied with, returning the corridor up to that
  border. Building there would be legal -- the classic ruleset's road
  enabler asks only for the Workers flag and CanRoad terrain
  (actions.ruleset:1066), and ACTRES_ROAD carries no hard ownership
  requirement (common/actions.c) -- but a worker cannot GET there:
  both worker pathfinders TB_IGNORE every tile owned by a non-ally
  (autoworkers.c:521-535, deepsettler.c:99-114). So the reachable
  border is where the corridor ends, and the tiles past it would be
  want nobody can spend.

  Greedy and not pathfinding on purpose. This runs once per city pair
  per turn and its output is a corridor to VALUE, not a route to
  follow; a worker's own pathfinding decides how it gets there.
**************************************************************************/
static int deep_trunk_walk(const struct civ_map *nmap,
                           const struct player *pplayer,
                           struct tile *from, struct tile *to,
                           struct tile **path, int maxlen, bool clip)
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
    if (clip) {
      const struct player *owner = tile_owner(step);

      if (owner != NULL && owner != pplayer
          && !pplayers_allied(owner, pplayer)) {
        return n;
      }
    }
    path[n++] = step;
    cur = step;
    dist = stepdist;
  }

  return n;
}

/**********************************************************************//**
  W12 slice 3: append one trunk node per allied player -- that ally's
  city nearest (map distance) to any of our own cities on the same
  continent. Returns the number of nodes appended at 'ctile + nown'.

  One per ally and not the whole allied city list, because the MST is
  the point: n allied cities dropped into the node set would let Prim
  spend its edges inside the ally's empire, which the ally's own
  workers are already covering, and would dilute the one link that
  matters -- the shortest hop from our empire to theirs. Same-continent
  because a corridor is a road: deep_trunk_walk() drops any link it
  cannot walk over land anyway, so an overseas ally would cost a walk
  and yield nothing.
**************************************************************************/
static int deep_trunk_ally_nodes(const struct player *pplayer,
                                 struct tile **ctile, int nown)
{
  int added = 0;

  players_iterate_alive(aplayer) {
    struct tile *best = NULL;
    int bestd = FC_INFINITY;

    if (aplayer == pplayer || !pplayers_allied(pplayer, aplayer)) {
      continue;
    }

    city_list_iterate(aplayer->cities, acity) {
      struct tile *atile = city_tile(acity);
      int i;

      for (i = 0; i < nown; i++) {
        int d;

        if (tile_continent(ctile[i]) != tile_continent(atile)) {
          continue;
        }
        d = real_map_distance(ctile[i], atile);
        if (d < bestd) {
          bestd = d;
          best = atile;
        }
      }
    } city_list_iterate_end;

    if (best != NULL) {
      ctile[nown + added++] = best;
    }
  } players_iterate_alive_end;

  return added;
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
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  int nown = city_list_size(pplayer->cities);
  int nnodes = nown;
  struct tile **ctile, **path;
  bool *seen, *is_ally;
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
  deep_trunk_nbeyond = 0;

  if (nown < 1) {
    return;
  }

  /* One node per own city, plus at most one per other player. */
  ctile = fc_malloc((nown + player_count()) * sizeof(*ctile));

  i = 0;
  city_list_iterate(pplayer->cities, pcity) {
    ctile[i++] = city_tile(pcity);
  } city_list_iterate_end;

  if (tune->trunk_ally_gate > 0) {
    nnodes += deep_trunk_ally_nodes(pplayer, ctile, nown);
  }

  if (nnodes < 2) {
    free(ctile);
    return;
  }

  if (deep_trunk_alloc < nnodes) {
    free(deep_trunk_links);
    deep_trunk_alloc = nnodes;
    deep_trunk_links = fc_malloc(deep_trunk_alloc
                                 * sizeof(*deep_trunk_links));
  }

  seen = fc_calloc(nnodes, sizeof(*seen));
  is_ally = fc_calloc(nnodes, sizeof(*is_ally));
  near_to = fc_malloc(nnodes * sizeof(*near_to));
  near_d = fc_malloc(nnodes * sizeof(*near_d));

  for (i = nown; i < nnodes; i++) {
    is_ally[i] = TRUE;
  }

  /* A greedy walk that must strictly close the gap every step cannot
   * take more steps than the map is wide plus tall. */
  maxpath = wld.map.xsize + wld.map.ysize;
  path = fc_malloc(maxpath * sizeof(*path));

  for (i = 0; i < nnodes; i++) {
    near_to[i] = -1;
    near_d[i] = FC_INFINITY;
  }
  seen[0] = TRUE;
  for (j = 1; j < nnodes; j++) {
    if (tile_continent(ctile[j]) == tile_continent(ctile[0])) {
      near_to[j] = 0;
      near_d[j] = real_map_distance(ctile[0], ctile[j]);
    }
  }

  for (i = 1; i < nnodes; i++) {
    int pick = -1, best = FC_INFINITY;

    for (j = 0; j < nnodes; j++) {
      if (!seen[j] && near_d[j] < best) {
        best = near_d[j];
        pick = j;
      }
    }
    if (pick < 0) {
      /* Every remaining city is on another landmass: start a fresh
       * component at the first one still unseen. */
      for (j = 0; j < nnodes; j++) {
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
      int len = deep_trunk_walk(nmap, pplayer, ctile[near_to[pick]],
                                ctile[pick], path, maxpath, is_ally[pick]);

      if (len > 0) {
        struct deep_trunk_link *link = &deep_trunk_links[deep_trunk_nlinks];
        int t;

        link->terr = 0;
        link->len = 0;
        link->gap = 0;
        link->ally = is_ally[pick];
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
              if (deep_trunk_nbeyond >= deep_trunk_beyond_alloc) {
                deep_trunk_beyond_alloc = MAX(2 * deep_trunk_beyond_alloc,
                                              32);
                deep_trunk_beyond
                  = fc_realloc(deep_trunk_beyond,
                               deep_trunk_beyond_alloc
                               * sizeof(*deep_trunk_beyond));
              }
              deep_trunk_beyond[deep_trunk_nbeyond++] = idx;
            }
          }
          deep_trunk_nlinks++;
        }
      }
    }

    if (is_ally[pick]) {
      /* An ally is an ENDPOINT, never a hub: relaxing from it could
       * make it the nearest neighbour of one of our own cities and put
       * a foreign capital in the middle of our own trunk. Skipping the
       * relaxation also fixes every ally link's direction as own ->
       * ally, which is what the border clip in deep_trunk_walk()
       * needs to mean "as far towards the ally as a worker can go". */
      continue;
    }

    for (j = 0; j < nnodes; j++) {
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

  /* Of the gap tiles just collected, keep as "beyond" only those the
   * advisor can never enumerate: worker_evaluate_improvements() offers
   * exactly the tiles inside some own city's radius
   * (autoworkers.c:586), so subtract those. Marking is done by
   * negating the slot in deep_trunk_tile -- restored in the same pass
   * that compacts the list, so the array leaves this function
   * all-positive exactly as before. */
  if (deep_trunk_nbeyond > 0) {
    int m = 0;

    city_list_iterate(pplayer->cities, pcity) {
      city_tile_iterate(nmap, city_map_radius_sq_get(pcity),
                        city_tile(pcity), ptile) {
        int idx = tile_index(ptile);

        if (deep_trunk_tile[idx] > 0) {
          deep_trunk_tile[idx] = -deep_trunk_tile[idx];
        }
      } city_tile_iterate_end;
    } city_list_iterate_end;

    for (i = 0; i < deep_trunk_nbeyond; i++) {
      int idx = deep_trunk_beyond[i];

      if (deep_trunk_tile[idx] < 0) {
        deep_trunk_tile[idx] = -deep_trunk_tile[idx];
      } else {
        deep_trunk_beyond[m++] = idx;
      }
    }
    deep_trunk_nbeyond = m;
  }

  free(ctile);
  free(seen);
  free(is_ally);
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
/* The road behind a candidate extra, if it is one that speeds
 * movement; NULL otherwise. The gate both pricing paths share. */
static const struct road_type *
deep_trunk_road_target(const struct extra_type *target)
{
  const struct road_type *proad;

  if (target == NULL || !is_extra_caused_by(target, EC_ROAD)) {
    return NULL;
  }
  proad = extra_road_get(target);
  if (proad == NULL || !road_provides_move_bonus(proad)) {
    return NULL;
  }

  return proad;
}

/**********************************************************************//**
  Build (or reuse) this player's plan for this turn. Memoised on
  (turn, player); also banks the per-turn census counts the build
  determines, so the census line can be emitted after the plan's
  scratch state has moved on to another player.
**************************************************************************/
static void deep_trunk_refresh(const struct player *pplayer)
{
  int pn = player_number(pplayer);
  int slot;

  if (deep_trunk_turn == game.info.turn && deep_trunk_plr == pn) {
    return;
  }
  deep_trunk_turn = game.info.turn;
  deep_trunk_plr = pn;
  deep_trunk_build(pplayer);
  deep_trunk_c_links[pn] = deep_trunk_nlinks;
  deep_trunk_c_gap[pn] = 0;
  deep_trunk_c_ally_links[pn] = 0;
  deep_trunk_c_ally_gap[pn] = 0;
  for (slot = 0; slot < deep_trunk_nlinks; slot++) {
    deep_trunk_c_gap[pn] += deep_trunk_links[slot].gap;
    if (deep_trunk_links[slot].ally) {
      deep_trunk_c_ally_links[pn]++;
      deep_trunk_c_ally_gap[pn] += deep_trunk_links[slot].gap;
    }
  }
  deep_trunk_c_beyond[pn] = deep_trunk_nbeyond;
}

/* Whether this tile's corridor runs to an allied city, on an
 * already-refreshed plan. The ally counters' only question. */
static bool deep_trunk_tile_ally(const struct tile *ptile)
{
  int slot;

  if (deep_trunk_tile == NULL) {
    return FALSE;
  }
  slot = deep_trunk_tile[tile_index(ptile)];

  return slot > 0 && deep_trunk_links[slot - 1].ally;
}

/* The want itself, on an already-refreshed plan: the movement the
 * corridor's link saves once whole, amortized over the tiles still
 * missing. Counter-free so both the hook path and the task source can
 * share it without polluting each other's instruments. */
static adv_want deep_trunk_price(const struct deep_tuning *tune,
                                 const struct extra_type *target,
                                 const struct tile *ptile)
{
  const struct deep_trunk_link *link;
  const struct road_type *proad = deep_trunk_road_target(target);
  int saving, slot;

  if (proad == NULL) {
    return 0.0;
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

  return tune->trunk_road_gain * ((double) saving / SINGLE_MOVE)
         * amortize(1.0, link->gap);
}

adv_want deep_trunk_want(const struct player *pplayer,
                         const struct extra_type *target,
                         const struct tile *ptile)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int pn = player_number(pplayer);
  adv_want want;

  if (tune->trunk_road_gain <= 0.0
      || deep_trunk_road_target(target) == NULL) {
    return 0.0;
  }

  deep_trunk_refresh(pplayer);

  want = deep_trunk_price(tune, target, ptile);
  if (want <= 0.0) {
    return 0.0;
  }

  deep_trunk_c_hits[pn]++;
  if (want > deep_trunk_c_max[pn]) {
    deep_trunk_c_max[pn] = want;
  }

  return want;
}

/**********************************************************************//**
  The corridor gap tiles outside every own city radius -- the demand
  the advisor's enumeration ceiling hides. Returns the count and
  points 'list' at the tile indices, valid until the next refresh.
  Builds (and memoises) this player's plan for this turn on first
  call, exactly as deep_trunk_want() does.
**************************************************************************/
int deep_trunk_beyond_tiles(const struct player *pplayer,
                            const int **list)
{
  deep_trunk_refresh(pplayer);
  *list = deep_trunk_beyond;

  return deep_trunk_nbeyond;
}

/**********************************************************************//**
  The trunk want for offering 'target' on 'ptile' as a task of deep's
  own sourcing, beyond the advisor's ceiling. The same pricing as
  deep_trunk_want() -- same plan, same saving, same amortize over the
  gap -- but counted on the source's own instrument rather than the
  hook's hits, so links>0 hits=0 keeps meaning what slice 1 said it
  means (a corridor the AUCTION never offered).
**************************************************************************/
adv_want deep_trunk_source_want(const struct player *pplayer,
                                const struct extra_type *target,
                                const struct tile *ptile)
{
  const struct deep_tuning *tune = deep_tuning_get();
  adv_want want;

  if (tune->trunk_road_gain <= 0.0) {
    return 0.0;
  }

  deep_trunk_refresh(pplayer);

  want = deep_trunk_price(tune, target, ptile);
  if (want > 0.0) {
    deep_trunk_c_src_off[player_number(pplayer)]++;
    if (deep_trunk_tile_ally(ptile)) {
      deep_trunk_c_ally_off[player_number(pplayer)]++;
    }
  }

  return want;
}

/**********************************************************************//**
  Whether this tile sits on a corridor to an allied city, for a caller
  that wants to say so in its own log line. Builds (and memoises) this
  player's plan for this turn on first call, like every other public
  entry point here.
**************************************************************************/
bool deep_trunk_tile_is_ally(const struct player *pplayer,
                             const struct tile *ptile)
{
  deep_trunk_refresh(pplayer);

  return deep_trunk_tile_ally(ptile);
}

/**********************************************************************//**
  Record that a beyond-ceiling task was actually assigned on 'ptile',
  'dist' map tiles from where the worker stood. Count and histogram
  feed the deep-trunksrc census line; a pick on an ally corridor also
  feeds the deep-trunkally line, which is the one instrument that says
  the gate did work rather than merely being open.
**************************************************************************/
void deep_trunk_source_pick(const struct player *pplayer,
                            const struct tile *ptile, int dist)
{
  int pn = player_number(pplayer);
  int bucket = CLIP(0, dist, DEEP_TRUNK_SRC_HIST);

  deep_trunk_c_src_pick[pn]++;
  deep_trunk_c_src_hist[pn][bucket]++;
  if (deep_trunk_tile_ally(ptile)) {
    deep_trunk_c_ally_pick[pn]++;
  }
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

  if (log_do_output_for_level(deep_metric_log_level())
      && (deep_trunk_c_beyond[pn] > 0 || deep_trunk_c_src_off[pn] > 0
          || deep_trunk_c_src_pick[pn] > 0)) {
    /* One bucket per map distance plus the overflow, colon-separated;
     * beyond= is the demand the ceiling hides this turn, offered= the
     * candidates the source actually priced, picked= the tasks
     * assigned. beyond>0 with offered=0 is the source knob off (or no
     * worker free), the denominator the A/B reads in both arms. */
    char hist[(DEEP_TRUNK_SRC_HIST + 1) * 12];
    int b, at = 0;

    for (b = 0; b <= DEEP_TRUNK_SRC_HIST; b++) {
      at += fc_snprintf(hist + at, sizeof(hist) - at, "%s%d",
                        b == 0 ? "" : ":",
                        deep_trunk_c_src_hist[pn][b]);
    }
    log_base(deep_metric_log_level(),
             "deep-trunksrc turn=%d player=\"%s\" beyond=%d offered=%d"
             " picked=%d hist=%s",
             game.info.turn, player_name(pplayer),
             deep_trunk_c_beyond[pn], deep_trunk_c_src_off[pn],
             deep_trunk_c_src_pick[pn], hist);
  }

  if (log_do_output_for_level(deep_metric_log_level())
      && (deep_trunk_c_ally_links[pn] > 0 || deep_trunk_c_ally_off[pn] > 0
          || deep_trunk_c_ally_pick[pn] > 0)) {
    /* W12 slice 3. links= corridors planned to an allied city this
     * turn, gap= their tiles still without a road (the demand the
     * gate exposed), offered= the ally corridor tiles the task source
     * priced, picked= the ally corridor tasks actually assigned.
     * links=0 in a game where deep HAS an ally is a miss, not a null:
     * the mechanism never ran. */
    log_base(deep_metric_log_level(),
             "deep-trunkally turn=%d player=\"%s\" links=%d gap=%d"
             " offered=%d picked=%d",
             game.info.turn, player_name(pplayer),
             deep_trunk_c_ally_links[pn], deep_trunk_c_ally_gap[pn],
             deep_trunk_c_ally_off[pn], deep_trunk_c_ally_pick[pn]);
  }

  deep_trunk_c_links[pn] = 0;
  deep_trunk_c_gap[pn] = 0;
  deep_trunk_c_ally_links[pn] = 0;
  deep_trunk_c_ally_gap[pn] = 0;
  deep_trunk_c_ally_off[pn] = 0;
  deep_trunk_c_ally_pick[pn] = 0;
  deep_trunk_c_hits[pn] = 0;
  deep_trunk_c_max[pn] = 0.0;
  deep_trunk_c_beyond[pn] = 0;
  deep_trunk_c_src_off[pn] = 0;
  deep_trunk_c_src_pick[pn] = 0;
  memset(deep_trunk_c_src_hist[pn], 0,
         sizeof(deep_trunk_c_src_hist[pn]));
}
