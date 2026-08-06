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
#ifndef FC__DEEPHORIZON_H
#define FC__DEEPHORIZON_H

/* common */
#include "fc_types.h"

struct ai_type;
struct adv_data;
struct city;

/* The two quantities the growth ladder already prices, exported so a
 * second consumer can reuse the price rather than invent a parallel
 * one (rule 3, and RC4's own lesson: deep already carried three
 * incompatible prices for happiness). deep_unworked_tile_value() is
 * what one more citizen earns per turn; deep_pacification_load() is
 * how many citizens a city currently spends on keeping order, and
 * deep_entertainer_load() is the half of that which is actually off
 * the tiles -- the population a luxury point can hand back to the
 * land. */
/* "The game has no bell" -- returned by deep_turns_left() when the
 * server carries no end turn at all. Larger than any horizon deep
 * plans over, so every MIN() against it is a no-op. */
#define DEEP_NO_END_TURN (1 << 24)

/* The endgame horizon (ENDGAME, 2026-08-06). deep_turns_left() is the
 * readable game state nothing in ai/ or server/advisors/ has ever
 * read: `end_turn - turn`, the turns of play that remain after this
 * one. deep_horizon_turns() is the planning horizon the growth ladder
 * may count over -- MORT, shortened to what remains -- and
 * deep_arrives_in_time() answers whether a benefit that first lands in
 * `delay` turns lands before the bell at all.
 *
 * All three are inert with `endgame_horizon` off, and inert with it on
 * until the game is inside MORT turns of its end. */
int deep_turns_left(void);
int deep_horizon_turns(void);
bool deep_arrives_in_time(int delay);

adv_want deep_unworked_tile_value(struct city *pcity,
                                  struct adv_data *adv, int count,
                                  int *raw);
int deep_pacification_load(const struct city *pcity);
int deep_entertainer_load(const struct city *pcity);

void deep_adjust_building_wants(struct ai_type *ait,
                                struct player *pplayer);
void deep_revert_building_wants(struct player *pplayer);

#endif /* FC__DEEPHORIZON_H */
