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

/* THE SORTIE FUNNEL (deep-employsortie) is the other place deep decides
 * to attack, and the one an operation cannot explain: a garrison
 * duelling something standing next to its own walls. It has its own
 * three gates, and a garrison that declines every night for a hundred
 * turns is an employment leak that never touches the op machinery.
 *
 * One outcome per city per turn, charged at the gate that stopped it.
 * DES_NOBLADE and DES_NOVICTIM are the two ways the pass finds nothing
 * to decide about, and telling them apart is the whole point: a city
 * with no sparable blade is a garrison problem, a city with a blade and
 * nobody next door is a geography one. */
enum deep_employ_sortie {
  DES_NOBLADE = 0, /* No unclaimed, undone, attack-rated unit inside. */
  DES_NOVICTIM,    /* A blade, but no at-war defender on any adjacent
                    * tile -- deep_kill_sorties() leaves `blade` NULL in
                    * both cases, so the two are separated by whether
                    * any candidate blade existed at all. */
  DES_FLOOR,       /* Best win chance below sortie_win_floor. */
  DES_PRICE,       /* Priced, and the trade came out non-positive. */
  DES_HOME,        /* The lending city would fall below survival_target. */
  DES_GO,          /* The rampage ran. */
  DES_SORTIE_COUNT
};
void deep_employ_sortie(const struct player *pplayer,
                        enum deep_employ_sortie outcome, double chance);

/* And, one level down, why an attack-rated unit standing in the city
 * was not even a candidate blade. DES_NOBLADE is by far the commonest
 * sortie outcome, and it has three quite different meanings. */
enum deep_employ_blade {
  DEB_MOVES = 0,   /* Fewer than SINGLE_MOVE moves left. */
  DEB_DONE,        /* The done flag was already set when the pass ran. */
  DEB_CLAIM,       /* An operation holds it. */
  DEB_BLADE_COUNT
};
void deep_employ_sortie_blade(const struct player *pplayer,
                              enum deep_employ_blade why);

/* THE OP LIFECYCLE (deep-employop): one line per assault operation
 * resolved, from formation to dissolve.
 *
 * The funnel above says how often deep decides to march. This says what
 * happened afterwards -- how long the wave stood at its staging city,
 * whether it ever launched, what ended it, and the kills and losses the
 * operation itself booked at the walls. Without it the aborts are a
 * single deep-op line with a reason string and no cost beside it, so
 * "the machinery forms waves that dissolve" and "the machinery forms
 * waves that fight and lose" are indistinguishable.
 *
 * Kills are counted as the drop in enemy units on the target tile
 * across one ordered attack, which is exact for a duel and correctly
 * zero for a walk-in; losses are the attacker not surviving its own
 * strike. Both are read from state the strike loop already has. */
void deep_employ_op_formed(const struct player *pplayer, int oi,
                           bool muster, int members, int target);
void deep_employ_op_launch(const struct player *pplayer, int oi,
                           int present);
void deep_employ_op_strike(const struct player *pplayer, int oi,
                           int kills, int losses);
void deep_employ_op_end(const struct player *pplayer, int oi,
                        const char *why, int members);

/* WHY A WAVE LOSES ITS MEMBERS. The muster A/B (STATUS addendum
 * 2026-08-22) measured 4297 operations of which 80.2% dissolved with
 * ZERO members left, 83.6% of musters formed with one blade and 77.9%
 * of them died after one turn -- and could not say whether the blades
 * were killed or taken back, because the two leave through different
 * code and neither left a trace.
 *
 * They are different findings with opposite fixes. Killed means the
 * wave is walking through danger it should not be in; taken back means
 * deep's own defence outbids its own offence every turn, and the fix is
 * a claim-priority question rather than a routing one.
 *
 * There are exactly four ways an assault loses a member, and each is
 * charged where it happens:
 *
 *   DEAD      the validation pass found the unit gone or at zero hp
 *   LOST      the validation pass found it alive but no longer ours
 *             (captured, bribed, or handed over)
 *   DEFENCE   deep_op_release_member(), whose only caller is the
 *             defence counter-claim (deepdefence.c)
 *   POACH     deep_op_poach_defender(), a threatened city taking the
 *             wave's best blade back off the march
 *   GARRISON  the CONSOLIDATE pass converting a member into the
 *             garrison of the city the wave just took -- a success,
 *             counted so the other four cannot be inflated by it
 *
 * Recorded per operation both over the whole life and restricted to
 * the turn after formation, which is where the muster arm loses 78% of
 * its waves. */
enum deep_employ_depart {
  DED_DEAD = 0,
  DED_LOST,
  DED_DEFENCE,
  DED_POACH,
  DED_GARRISON,
  DED_DEPART_COUNT
};
void deep_employ_op_depart(const struct player *pplayer, int oi,
                           enum deep_employ_depart why);

/* WHAT THE POACH PAID, AND WHAT IT BOUGHT (deep-employpoach).
 *
 * The departure counter above settled the mechanism -- 89.4% of member
 * departures are deep_op_poach_defender(), deep's own threatened cities
 * taking blades back off forming waves -- and explicitly did not settle
 * whether the poach is RIGHT. Every poach is a priced decision, and
 * nothing on disk carried the value it paid or what the payment bought.
 *
 * One entry per poach, opened where the blade is taken and resolved
 * `survival_horizon` turns later, carrying three things:
 *
 *   the CLAIM   the city, its survival at claim time, the wave it saw,
 *               the reachable relief pool, the worth the defence bid,
 *               and whether the force it joined was actually deployed
 *               (a poach taken into a city that is then written off in
 *               the same pass buys nothing at all);
 *   the LOSS    the operation, its age, its member count before and
 *               after, and the expected take the poach outbid -- the
 *               op's own side of the comparison the poach already
 *               makes, not a new metric;
 *   the AFTER   sampled once a turn at the same point in the turn the
 *               claim was made: did the enemy ever reach the walls, did
 *               the blade arrive, did it take damage or die, did the
 *               city fall, was it re-claimed by another operation.
 *
 * The window is `survival_horizon` because that is the horizon the
 * claim itself is made over -- the survival model prices the city's
 * next `survival_horizon` turns, and the poach only takes a blade that
 * can arrive inside it -- and because the write-off ledger next door
 * adjudicates its own verdicts over exactly the same span. No new
 * constant.
 *
 * Everything here observes. Every value passed to the claim was already
 * computed by the decision it sits in; the aftermath reads only unit
 * and city state that any observer could read. */
void deep_employ_poach_claim(const struct player *pplayer, int city_id,
                             int unit_id, int utype_id, double p_survival,
                             double dworth, double risk, int oi, bool muster,
                             int age, int members_before, double take,
                             double odds, int eta, int target);

/* The claiming city's pass ends here: `kept` is whether the relief
 * force the blade joined was actually deployed, `took` whether the
 * blade actually left its operation, `relief` the survival that force
 * reaches, and `wave`/`pool` the threat and the reachable blades the
 * city saw. Called from both exits of the relief decision, so an
 * unsettled entry means neither ran.
 *
 * `kept` and `took` are the same bit until poach_deploy_gate is on,
 * which is exactly what the gate changes: with it on, a claim the
 * arithmetic refuses is ASKED (took = 0) rather than PAID. */
void deep_employ_poach_settle(const struct player *pplayer, int unit_id,
                              bool kept, bool took, double relief,
                              int wave, int pool);

/* One turn of aftermath for every open entry, and the verdict line for
 * every entry whose window has run out. */
void deep_employ_poach_tick(struct player *pplayer);

#endif /* FC__DEEPEMPLOY_H */
