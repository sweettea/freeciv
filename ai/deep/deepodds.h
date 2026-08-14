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
#ifndef FC__DEEPODDS_H
#define FC__DEEPODDS_H

/* Pure sequential-combat probability math. Deliberately freestanding
 * (no freeciv headers) so it can be unit-tested in isolation.
 *
 * A freeciv attack is a duel to the death resolved in HP-chipping
 * rounds; win_chance() in common/combat.c computes the attacker's
 * total chance for ONE duel and throws the rest of the distribution
 * away (its own comment: "Maybe that info should be preserved for use
 * in the AI"). These helpers preserve it: the defender's post-duel HP
 * distribution, chained across an ordered stream of attackers hitting
 * a stack of defenders one duel at a time. That is the arithmetic the
 * linear rating sums in the stock AI cannot express: a defender rated
 * equal to N attackers must win N straight fights, and its chance of
 * that compounds DOWN with every duel while a sum stays at parity
 * (findings "Sequential-attrition odds"). */

/* Combat parameters of one attacker/defender pairing, in the same
 * units win_chance() takes: effective strengths after all situational
 * bonuses, and modified firepowers. */
struct deep_duel {
  int att_str, att_fp;
  int def_str, def_fp;
};

double deep_duel_defender_dist(const struct deep_duel *duel,
                               int att_hp, int def_hp,
                               double *dist /* [def_hp + 1] or NULL */);

double deep_stack_survival(int n_att, const int *att_hp,
                           int n_def, const int *def_hp,
                           const struct deep_duel *pair
                           /* [n_att * n_def], attacker-major */);

/* The same walk, keeping what it costs the attacker. att_death[j] is
 * filled with P(attacker j dies in its duel) -- which is exactly the
 * probability that the defender it met survived, because a freeciv
 * duel runs to a death. An attacker that never gets to strike (the
 * stack fell before its turn) contributes 0, so
 * n_att - sum(att_death) is the expected size of the force still
 * standing after the assault: the quantity "can I hold what I take"
 * needs and nothing in this project could previously ask for. */
double deep_stack_survival_losses(int n_att, const int *att_hp,
                                  int n_def, const int *def_hp,
                                  const struct deep_duel *pair,
                                  double *att_death /* [n_att] or NULL */);

/* Both sides of the ledger from one walk. def_death is the expected
 * number of DEFENDERS killed -- which, because a freeciv defender dies
 * exactly when an attack on it succeeds, is the expected number of
 * SUCCESSFUL enemy attacks. That is the quantity a city pays a citizen
 * for under killcitizen, and the one EFT_UNIT_NO_LOSE_POP insures. */
double deep_stack_survival_full(int n_att, const int *att_hp,
                                int n_def, const int *def_hp,
                                const struct deep_duel *pair,
                                double *att_death /* [n_att] or NULL */,
                                double *def_death /* or NULL */);

/* The same walk with a PRESENCE WEIGHT per attacker: att_w[j] is the
 * probability that attacker j takes part in the wave at all (NULL, or
 * an entry outside [0,1] clamped, means certain presence -- the
 * unweighted walk, bit for bit). Because the DP transition for one
 * attacker is linear in the state distribution, mixing it as
 * (1-w) * skip + w * fight computes the EXACT expectation of every
 * output over independent Bernoulli(att_w[j]) presence draws, with the
 * stream order preserved -- not an approximation. att_death[j] is the
 * unconditional P(attacker j fights and dies), so it already carries
 * its own weight.
 *
 * What it is for: a threat model that has measured evidence that some
 * attackers (e.g. enemy garrisons pinned at home, NETTHREAT item 2)
 * only show up a fraction of the time can price them at that fraction
 * instead of at certainty, in the substrate's own probability space
 * rather than by scaling a rating that only orders the wave. */
double deep_stack_survival_wfull(int n_att, const int *att_hp,
                                 const double *att_w /* [n_att] or NULL */,
                                 int n_def, const int *def_hp,
                                 const struct deep_duel *pair,
                                 double *att_death /* [n_att] or NULL */,
                                 double *def_death /* or NULL */);

#endif /* FC__DEEPODDS_H */
