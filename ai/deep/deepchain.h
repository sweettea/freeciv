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
#ifndef FC__DEEPCHAIN_H
#define FC__DEEPCHAIN_H

/* What a terrain-changing worker action is worth when the terrain two
 * steps along is the one the city wants (W29).
 *
 * The advisors value cultivating and planting one step deep: the
 * terrain the activity produces, at that terrain's yield. deepterra.c
 * searches the graph those activities form; this file spends the search
 * on a price, and registers itself on the advisors' terrain-value hook
 * so that the price reaches the tile auction without any other code
 * knowing a chain exists.
 *
 * THE PRICE. A candidate terrain is worth what city_tile_value() says
 * it is worth -- the same function, the same city, the same weights the
 * one-step value came from -- amortized over the turns needed BEYOND
 * this first step. The first step's own time, and the walk to the tile,
 * are already amortized by the caller, so charging them again here
 * would be charging them twice. The action's price is the best such
 * candidate, and the immediate terrain is one of the candidates, so the
 * price is never below the one-step price and is exactly it whenever
 * the near terrain is the best one going.
 *
 * WHAT IT DOES NOT DO. It does not credit a destination with the
 * irrigation or mine it might later carry: every terrain in the chain
 * is valued bare, the immediate one included, so the only thing the
 * gate changes is that far terrain becomes VISIBLE. It does not plan --
 * nothing is remembered between turns, and the tile is re-priced from
 * whatever terrain it actually has, so a chain that gets one step done
 * and then stops has cost exactly that one step and left a legal
 * position. And it does not touch ACTIVITY_TRANSFORM. */

#include "fc_types.h"

/* Register the chain valuation on the advisors' terrain-value hook.
 * Called once from module setup. The registration is unconditional and
 * the GATE is per-seat (terrain_chain_gate): with it shut the callback
 * returns the one-step value it was handed, so the tile auction is
 * byte-identical, while the census below still publishes the
 * denominator the open arm's rate is taken against. */
void deep_chain_register(void);

/* End of this player's phase: follow the tiles whose price was raised
 * by a chain and print the funnel line. Called from
 * deep_do_last_activities() beside the other censuses.
 *
 * The funnel a VALUATION owes is not the one a task source owes. This
 * code assigns nobody: it raises a price, the advisor's own auction
 * decides, and what happens next is a worker's business. So the ledger
 * follows TILES rather than assignments -- the tiles whose price was
 * raised -- and asks of each only what a tile can answer: did its
 * terrain ever change, did it reach the terrain the price was paid for,
 * did it change to something off the chain instead, is it still ours.
 * `cand` and `multi` are counted with the gate SHUT as well as open, so
 * the closed arm publishes the denominator the open arm's rate is taken
 * against, and `lift` is zero by construction while it is shut. */
void deep_chain_census(struct player *pplayer);

#endif /* FC__DEEPCHAIN_H */
