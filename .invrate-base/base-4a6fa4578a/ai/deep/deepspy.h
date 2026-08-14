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
#ifndef FC__DEEPSPY_H
#define FC__DEEPSPY_H

/* common */
#include "fc_types.h"

/* Spy-action valuation (W35 E35.1).
 *
 * Stock decides what a diplomat does at a city by a fixed LADDER --
 * embassy 10000, steal tech 9000, incite 7000, sabotage 6000, steal
 * gold 4000, plague 2500 -- whose own FIXME says the numbers are
 * arbitrary and unscaled (daiactions.c:96). A tech is worth 9000
 * whether it is Bronze Working or Computers; a city is worth inciting
 * at 7000 whether it is a size-1 backwater or the enemy capital. That
 * is RC4 in one function: a private scale that cannot be compared with
 * anything else the AI wants.
 *
 * Deep prices the four families it can price in the ONE currency
 * (deepwant.h: discounted shield-equivalent), from quantities the
 * server already computes -- bulb cost at stock's own x3 gold
 * conversion, the incite cost the server charges, the shield stock a
 * sabotage destroys, the embassy want deep already pays for in its
 * build queue -- and abstains where it has no honest price, leaving
 * stock's ladder to answer. */

struct ai_type;
struct player;

/* Run before deep's unit loop, once per player per phase. Off (the
 * compiled default) it is a pure CENSUS: how many diplomat-turns stand
 * where the action seam is reached at all. On, it takes the action
 * choice for those units and holds them, and leaves every other
 * diplomat-turn to stock. */
void deep_spy_manage(struct ai_type *ait, struct player *pplayer);

#endif /* FC__DEEPSPY_H */
