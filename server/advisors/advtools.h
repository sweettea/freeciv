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
#ifndef FC__ADVTOOLS_H
#define FC__ADVTOOLS_H

/* common */
#include "fc_types.h"

#define MORT 24

/* relative comparison for equality of floats */
#define FC_EPSILON (0.001)
#define ADV_WANTS_EQ(_w1, _w2) (fabs((_w1) - (_w2)) < FC_EPSILON * (fabs((float) (_w1)) + fabs((float) (_w2))))

struct terrain;

adv_want amortize(adv_want benefit, int delay);

/* Terrain transform chains (W29). The transform graph is functional
 * (one successor per terrain); the walk is visited-set bounded and
 * never returns to the start terrain. */
#define ADV_TRANSFORM_CHAIN_MAX 8
int adv_transform_chain(const struct terrain *from,
                        struct terrain *chain[ADV_TRANSFORM_CHAIN_MAX],
                        int total_time[ADV_TRANSFORM_CHAIN_MAX]);
int adv_transform_cost(const struct terrain *from,
                       const struct terrain *to);

/* An optional second opinion on what a terrain-CHANGING worker action is
 * worth (W29). initialize_infrastructure_cache() values cultivating and
 * planting one step deep -- the terrain the activity produces, by that
 * terrain's yield -- which is the whole of the greedy evaluation RC1's
 * corollary describes: a first step that looks unappealing alone is never
 * taken, however good the terrain two steps along is.
 *
 * This is an EXTENSION POINT and nothing else (CLAUDE.md rule 4): with no
 * consumer registered the value returned is the one-step value the caller
 * computed, byte for byte, so a server without ai/deep -- and a server
 * with it whose consumer declines -- behaves exactly as before. The
 * consumer receives the values it needs to re-price the action and cannot
 * reach anything else: the city asking, the tile, the activity, the
 * terrain that activity produces, and the one-step value.
 *
 * It is deliberately NOT called for ACTIVITY_TRANSFORM, which already
 * carries its own chain valuation in adv_calc_transform(). */
struct city;
struct tile;

typedef adv_want (*adv_terrain_value_fn)(const struct city *pcity,
                                         const struct tile *ptile,
                                         enum unit_activity act,
                                         const struct terrain *result,
                                         adv_want one_step);
void adv_terrain_value_hook_set(adv_terrain_value_fn fn);
adv_want adv_terrain_value(const struct city *pcity,
                           const struct tile *ptile,
                           enum unit_activity act,
                           const struct terrain *result,
                           adv_want one_step);

/*
 * To prevent integer overflows the product "power * hp * firepower"
 * is divided by POWER_DIVIDER.
 *
 */
#define POWER_DIVIDER (POWER_FACTOR * 3)

#endif /* FC__ADVTOOLS_H */
