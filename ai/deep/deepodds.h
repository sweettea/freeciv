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

#endif /* FC__DEEPODDS_H */
