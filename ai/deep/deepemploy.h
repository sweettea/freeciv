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

/* THE FORMATION FUNNEL (deep-employform) is the second, and it answers
 * the question the disposition census can only pose: an army standing
 * still is either an army with nothing to hit or an army refusing what
 * it can see, and those call for opposite fixes.
 *
 * One ATTEMPT is one call of deep_op_form(), and it ends in exactly one
 * VERDICT charged at the site that decided it. The odds facts beside
 * the verdict (zero / short of the bar / clear) are attributes of the
 * attempts that reached pricing, not verdicts of their own: with the
 * muster available, an attempt whose odds fall short is refused by the
 * WORTH gate or admitted as a muster, and it is that terminal branch
 * the ledger has to name.
 *
 * The verdict is also what the WAIT LEDGER slices the idle army by, so
 * the disposition census carries this turn's value: "no target worth
 * hitting" (NOTARGET/NOSTAGING), "target priced but vetoed" (WORTH,
 * MUSTEROFF, POOLEMPTY) and "waiting for a wave that never fills"
 * (MUSTER, with the census's own `musters` count beside it) are three
 * values of one field rather than three instruments. */
enum deep_employ_verdict {
  DEV_NOWAR = 0,   /* No attempt at all this turn: op_take_target off,
                    * or deep_run_operations() never reached formation. */
  DEV_NOSLOT,      /* Every army op slot was already occupied. */
  DEV_NOHOME,      /* No city to anchor an operation on. */
  DEV_NOTARGET,    /* No met, unclaimed, at-war city worth scoring. */
  DEV_NOSTAGING,   /* Target found; no own city can stage against it. */
  DEV_POOLEMPTY,   /* Target and staging found; not one free blade. */
  DEV_MUSTEROFF,   /* Odds refused and the muster is unavailable. */
  DEV_WORTH,       /* Odds refused AND the take is not worth mustering
                    * for -- the muster's own gate. */
  DEV_MUSTER,      /* A muster opened on the worth gate. */
  DEV_FORMED,      /* A wave formed against a priced quorum. */
  DEV_VERDICT_COUNT
};

/* One line per player-turn: the disposition census, sliced by this
 * turn's formation verdict, plus the cumulative formation funnel.
 * Called from deep_do_last_activities(). */
void deep_employ_census(struct ai_type *ait, struct player *pplayer);

/* Formation funnel. `attempt` opens one attempt at the top of
 * deep_op_form(); `priced` records what the odds loop saw, once, for an
 * attempt that reached it; `verdict` closes the attempt. Every value
 * passed is one the seat had already computed. */
void deep_employ_form_attempt(const struct player *pplayer);
void deep_employ_form_priced(const struct player *pplayer, double odds,
                             double bar, int pool, int drafted);
void deep_employ_form_verdict(const struct player *pplayer,
                              enum deep_employ_verdict verdict);

#endif /* FC__DEEPEMPLOY_H */
