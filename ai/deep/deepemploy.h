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
#ifndef FC__DEEPEMPLOY_H
#define FC__DEEPEMPLOY_H

/* The EMPLOYMENT ledger: what deep's army spends its turns doing.
 *
 * The war-gap decomposition (STATUS 2026-08-22) split the tech50war
 * deficit into three multiplicative legs at T60 -- capacity 0.941, then
 * a further -13% of allocation (built/mfg 0.622 vs 0.709), then a
 * further -33% of EMPLOYMENT (kills/built 0.223 vs 0.328). The first
 * two legs have counters. The third has none, and the retained data
 * cannot say whether deep's blades have no target, refuse the targets
 * they have, or lose the fights they pick -- three findings calling for
 * three different fixes.
 *
 * Everything here observes and nothing steers. Every predicate read was
 * already evaluated by the code the counter sits in, no value computed
 * here reaches a decision, and the whole ledger is gated on the metric
 * log level, so a tuning run under FREECIV_DEEP_QUIET pays nothing.
 *
 * THE DISPOSITION CENSUS (deep-employ) is the first instrument and the
 * denominator the other three are read against: one line per
 * player-turn, in which every military unit lands in exactly one state.
 * "The army is 1.06x the size and kills 0.54x as much" is a claim about
 * a population whose composition nobody has ever counted; this counts
 * it.
 *
 * Taken at the honest end of the phase, beside deep_hold_census() and
 * for the same reason: a unit is idle only once everything that could
 * have tasked it has run. A unit that still holds its full move
 * allowance at that point spent the turn standing still, and that -- not
 * an activity flag, which a fortified defender and an abandoned
 * attacker share -- is what the census means by idle. */

#include "fc_types.h"

struct ai_type;

/* One line per player-turn: the disposition census. Called from
 * deep_do_last_activities(). */
void deep_employ_census(struct ai_type *ait, struct player *pplayer);

#endif /* FC__DEEPEMPLOY_H */
