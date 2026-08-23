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
#ifndef FC__DEEPTERRA_H
#define FC__DEEPTERRA_H

/* The terrain-CHANGE graph, walked (W29).
 *
 * A ruleset's terrains are the nodes and its cultivate/plant results are
 * the edges. Unlike the transform graph -- which is functional, one
 * successor per terrain, so "the chain" is a single walk
 * (adv_transform_chain) -- this one branches: a terrain may have both a
 * cultivate result and a plant result, and civ2civ3's Grassland, Plains
 * and Forest form a strongly connected triangle:
 *
 *     Grassland --plant 15--> Forest --cultivate 10--> Plains
 *     Plains    --plant 10--> Forest --plant     15--> Grassland
 *
 * Neither Grassland->Plains nor Plains->Grassland is a legal step, and
 * both are two steps through Forest. So the shortest path is a real
 * search, not a walk, and the one-step evaluators in infracache.c can
 * see neither destination at any price.
 *
 * WHY THIS GRAPH AND NOT THE TRANSFORM ONE. Transform Terrain in
 * civ2civ3 requires Fusion Power (actions.ruleset
 * enabler_transform_terrain), so the transform chain that W29 landed in
 * July is correct and dormant for the whole of every game this project
 * measures. Cultivate and Plant require nothing but a worker
 * (enabler_cultivate, enabler_plant), which is what makes later steps
 * safe to assume: the workstream's "assume feasibility optimistically"
 * is not an assumption here, it is the ruleset. Transform edges are
 * therefore deliberately NOT followed -- following them would import
 * the Fusion gate as an unpriced promise.
 *
 * COST UNITS. Edge cost is the ruleset's cultivate_time / plant_time,
 * which IS the turn count for the reference worker: tile_activity_time()
 * returns ruleset_time * ACTIVITY_FACTOR points, and get_activity_rate()
 * of a green worker whose move_rate is SINGLE_MOVE is exactly
 * ACTIVITY_FACTOR points a turn (common/unit.c). Faster or veteran
 * workers finish sooner; the reference is the same one stock's own
 * worker arithmetic uses when it has no unit in hand.
 *
 * The walk is pure: terrain structures in, numbers out, no map, no
 * player, no city. That is what lets tests/deepterra_test.c pin it
 * against a synthetic ruleset. */

#include "fc_types.h"

/* Terrains reachable from one start. Bigger than any shipped ruleset's
 * connected component (civ2civ3's is 3, classic's 4); a ruleset with a
 * larger one simply gets the first DEEP_TERRA_REACH_MAX found, which
 * costs the valuation candidates and cannot cost it correctness. */
#define DEEP_TERRA_REACH_MAX 8

struct deep_terra_step {
  const struct terrain *pterrain; /* The terrain reached. */
  int turns;                      /* Cumulative reference worker-turns. */
};

/* Terrains reachable from 'from' by doing 'first' (ACTIVITY_CULTIVATE or
 * ACTIVITY_PLANT) and then any sequence of cultivating and planting.
 * out[0] is always the immediate result of 'first' with its own time;
 * the rest are further terrains at their cheapest cumulative cost, in no
 * particular order. Returns the number of entries, 0 when 'first'
 * produces nothing at all. 'from' itself is never an entry: a path back
 * to where it started spends turns to arrive nowhere. */
int deep_terra_reach(const struct terrain *from, enum unit_activity first,
                     struct deep_terra_step out[DEEP_TERRA_REACH_MAX]);

/* The terrain 'act' produces on 'from', or NULL when it produces nothing
 * (no result in the ruleset, or the terrain itself). */
const struct terrain *deep_terra_result(const struct terrain *from,
                                        enum unit_activity act);

#endif /* FC__DEEPTERRA_H */
