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

/* Freestanding on purpose -- see deepodds.h. Keep it free of freeciv
 * includes so tests/deepodds_test.c can compile it alone. */

#include <stdlib.h>

#include "deepodds.h"

/**********************************************************************//**
  Probability that the defender survives one duel, and (when dist is
  non-NULL) the distribution of the HP it survives with:
  dist[h] += P(defender ends the duel alive on exactly h HP).

  Same recurrence as win_chance() (common/combat.c) with the roles
  swapped, keeping the per-outcome terms instead of only their sum.
  The defender wins the duel by winning att_rounds rounds before
  losing def_rounds; a win series that ends after losing lr rounds has
  probability C(att_rounds-1+lr, lr) * p^att_rounds * q^lr and leaves
  the defender on def_hp - lr * att_fp hit points.

  The caller owns zeroing dist; this function only accumulates, so a
  chained caller can merge several duels into one distribution.
**************************************************************************/
double deep_duel_defender_dist(const struct deep_duel *duel,
                               int att_hp, int def_hp,
                               double *dist)
{
  int att_fp = duel->att_fp > 0 ? duel->att_fp : 1;
  int def_fp = duel->def_fp > 0 ? duel->def_fp : 1;
  int att_rounds, def_rounds, lr;
  double p, q, term, total = 0.0;

  if (att_hp <= 0) {
    /* No live attacker: the defender keeps every hit point. */
    if (dist != NULL && def_hp > 0) {
      dist[def_hp] += 1.0;
    }
    return def_hp > 0 ? 1.0 : 0.0;
  }
  if (def_hp <= 0) {
    return 0.0;
  }

  /* Rounds each side can lose before dying. */
  att_rounds = (att_hp + def_fp - 1) / def_fp;
  def_rounds = (def_hp + att_fp - 1) / att_fp;

  if (duel->att_str + duel->def_str == 0) {
    p = 0.5;                    /* win_chance()'s degenerate case */
  } else {
    p = (double) duel->def_str / (duel->att_str + duel->def_str);
  }
  q = 1.0 - p;

  /* lr = 0 term: p^(att_rounds-1), times the final winning round's p
   * applied at the end of every term. */
  term = 1.0;
  for (lr = 1; lr < att_rounds; lr++) {
    term *= p;
  }
  for (lr = 0; lr < def_rounds; lr++) {
    double prob;

    if (lr > 0) {
      /* C(att_rounds-1+lr, lr) from C(att_rounds-2+lr, lr-1). */
      term *= (double) (att_rounds - 1 + lr);
      term /= (double) lr;
      term *= q;
    }
    prob = term * p;
    total += prob;
    if (dist != NULL) {
      dist[def_hp - lr * att_fp] += prob;
    }
  }

  return total;
}

/**********************************************************************//**
  Probability that at least one of n_def defenders is still alive
  after an ordered stream of n_att attackers, each duelling the
  stack's current front defender to the death. pair[j * n_def + i]
  holds the parameters of attacker j against defender i.

  State: the front defender's index and hit points (defenders behind
  the front are untouched until it falls, which is how one-duel-per
  -attack city assaults actually run). Every duel either kills the
  attacker (the front's HP distribution spreads) or kills the front
  (the next defender steps up at full HP, and the winning attacker's
  turn is spent either way). Single-wave model: no healing, no
  veteran promotion, no reinforcement between duels.

  Returns 1.0 for an empty stream, 0.0 for an empty stack.
**************************************************************************/
double deep_stack_survival(int n_att, const int *att_hp,
                           int n_def, const int *def_hp,
                           const struct deep_duel *pair)
{
  return deep_stack_survival_losses(n_att, att_hp, n_def, def_hp, pair,
                                    NULL);
}

/**********************************************************************//**
  As deep_stack_survival(), also reporting what the stream pays for
  the answer: att_death[j] = P(attacker j dies).

  It is the same walk. At every state the duel has exactly two
  outcomes -- the defender survives (and the attacker is dead) or the
  defender falls (and the attacker lives, its turn spent) -- so the
  survival mass this function already computes per duel IS the
  attacker's death probability, and summing it over the live states
  costs one multiply-add per state. The mass that has already reached
  `dead` (the whole stack annihilated) is not live, so an attacker
  with nothing left to fight is charged nothing, which is what makes
  the total a force size rather than a casualty rate.
**************************************************************************/
double deep_stack_survival_losses(int n_att, const int *att_hp,
                                  int n_def, const int *def_hp,
                                  const struct deep_duel *pair,
                                  double *att_death)
{
  return deep_stack_survival_full(n_att, att_hp, n_def, def_hp, pair,
                                  att_death, NULL);
}

/**********************************************************************//**
  The same walk again, now also reporting the DEFENDER's side of the
  ledger: def_death is the expected number of defenders killed.

  It costs nothing to compute and it was always in the state vector.
  The DP's state index `i` is the front defender, which is to say that
  defenders 0..i-1 are already dead -- so at the end of the walk the
  expected number killed is the mass-weighted mean of `i`, plus n_def
  for the mass that reached `dead` (the whole stack annihilated).

  Why anyone wants it: in freeciv a defender dies exactly when an
  attack on it SUCCEEDS, so E[defenders killed] is E[successful enemy
  attacks on this stack] -- and a successful attack on a city stack is
  what costs the city a citizen under `killcitizen` +
  `UCF_KILLCITIZEN` (server/unithand.c:4601). That is the quantity
  `EFT_UNIT_NO_LOSE_POP` insures against, and stock prices it as
  units x 2 (daieffects.c:456).
**************************************************************************/
double deep_stack_survival_full(int n_att, const int *att_hp,
                                int n_def, const int *def_hp,
                                const struct deep_duel *pair,
                                double *att_death, double *def_death)
{
  int max_hp = 0, i, j, h, hh, width;
  size_t cells, c;
  double *base, *state, *next, *duel_dist, *swap;
  double dead = 0.0;

  if (att_death != NULL) {
    for (j = 0; j < n_att; j++) {
      att_death[j] = 0.0;
    }
  }
  if (def_death != NULL) {
    *def_death = 0.0;
  }
  if (n_def <= 0) {
    return 0.0;
  }
  if (n_att <= 0) {
    return 1.0;
  }

  for (i = 0; i < n_def; i++) {
    if (def_hp[i] > max_hp) {
      max_hp = def_hp[i];
    }
  }
  if (max_hp <= 0) {
    if (def_death != NULL) {
      *def_death = n_def;
    }
    return 0.0;
  }

  width = max_hp + 1;
  cells = (size_t) n_def * width;
  base = calloc(cells * 2 + width, sizeof(*base));
  if (base == NULL) {
    return 0.0;                 /* Pessimism beats a crash. */
  }
  state = base;
  next = base + cells;
  duel_dist = base + cells * 2;

  state[0 * width + def_hp[0]] = 1.0;

  for (j = 0; j < n_att; j++) {
    for (c = 0; c < cells; c++) {
      next[c] = 0.0;
    }
    for (i = 0; i < n_def; i++) {
      for (h = 1; h < width; h++) {
        double m = state[i * width + h];
        double survive, die;

        if (m <= 0.0) {
          continue;
        }
        for (hh = 0; hh < width; hh++) {
          duel_dist[hh] = 0.0;
        }
        survive = deep_duel_defender_dist(&pair[j * n_def + i],
                                          att_hp[j], h, duel_dist);
        if (att_death != NULL) {
          att_death[j] += m * survive;
        }
        for (hh = 1; hh < width; hh++) {
          if (duel_dist[hh] > 0.0) {
            next[i * width + hh] += m * duel_dist[hh];
          }
        }
        die = m * (1.0 - survive);
        if (i + 1 < n_def) {
          next[(i + 1) * width + def_hp[i + 1]] += die;
        } else {
          dead += die;
        }
      }
    }
    swap = state;
    state = next;
    next = swap;
  }

  if (def_death != NULL) {
    double killed = (double) n_def * dead;

    for (i = 0; i < n_def; i++) {
      for (h = 1; h < width; h++) {
        killed += (double) i * state[i * width + h];
      }
    }
    *def_death = killed;
  }

  free(base);
  return 1.0 - dead;
}
