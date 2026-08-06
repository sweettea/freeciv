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

/* L0 unit test for ai/deep/deepodds.c (which is freestanding by
 * design so this file can compile it without the freeciv tree).
 * The oracle is an independent recursive dynamic program over
 * (attacker hp, defender hp) round states -- brute force checking the
 * closed-form recurrence, not the recurrence checking itself. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "deepodds.h"

#define EPS 1e-9

static int failures = 0;

#define CHECK(cond, ...) \
  do { \
    if (!(cond)) { \
      failures++; \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
      fprintf(stderr, __VA_ARGS__); \
      fprintf(stderr, "\n"); \
    } \
  } while (0)

/**********************************************************************//**
  Oracle: P(defender survives the duel), accumulating the survivor-HP
  distribution, by forward mass propagation over (attacker hp,
  defender hp) round states -- an independent brute force, not the
  closed-form recurrence checking itself.
**************************************************************************/
static double oracle_duel(const struct deep_duel *d, int att_hp,
                          int def_hp, double mass, double *dist)
{
  int afp = d->att_fp > 0 ? d->att_fp : 1;
  int dfp = d->def_fp > 0 ? d->def_fp : 1;
  int stride = def_hp + 1, ah, dh;
  double p, q, srv = 0.0;
  double *tab;

  if (att_hp <= 0) {
    if (dist != NULL) {
      dist[def_hp] += mass;
    }
    return mass;
  }
  if (def_hp <= 0) {
    return 0.0;
  }
  if (d->att_str + d->def_str == 0) {
    p = 0.5;
  } else {
    p = (double) d->def_str / (d->att_str + d->def_str);
  }
  q = 1.0 - p;

  tab = calloc((size_t) (att_hp + 1) * stride, sizeof(*tab));
  tab[att_hp * stride + def_hp] = mass;
  /* Every round strictly lowers one side's hp, so descending order
   * visits each state after all mass has arrived. */
  for (ah = att_hp; ah >= 1; ah--) {
    for (dh = def_hp; dh >= 1; dh--) {
      double m = tab[ah * stride + dh];

      if (m <= 0.0) {
        continue;
      }
      if (ah - dfp <= 0) {
        srv += m * p;
        if (dist != NULL) {
          dist[dh] += m * p;
        }
      } else {
        tab[(ah - dfp) * stride + dh] += m * p;
      }
      if (dh - afp > 0) {
        tab[ah * stride + (dh - afp)] += m * q;
      }
      /* Defender death mass just drains. */
    }
  }
  free(tab);
  return srv;
}

/**********************************************************************//**
  Oracle for the stack: chain oracle_duel() across attackers by hand,
  spreading the state distribution exactly as the model claims to.
**************************************************************************/
static double oracle_stack(int n_att, const int *att_hp,
                           int n_def, const int *def_hp,
                           const struct deep_duel *pair, int max_hp)
{
  int width = max_hp + 1;
  double *state = calloc((size_t) n_def * width, sizeof(*state));
  double *next = calloc((size_t) n_def * width, sizeof(*next));
  double *dd = calloc((size_t) width, sizeof(*dd));
  double dead = 0.0, alive = 0.0;
  int i, j, h, hh;

  state[0 * width + def_hp[0]] = 1.0;
  for (j = 0; j < n_att; j++) {
    for (i = 0; i < n_def * width; i++) {
      next[i] = 0.0;
    }
    for (i = 0; i < n_def; i++) {
      for (h = 1; h < width; h++) {
        double m = state[i * width + h], srv;

        if (m <= 0.0) {
          continue;
        }
        for (hh = 0; hh < width; hh++) {
          dd[hh] = 0.0;
        }
        srv = oracle_duel(&pair[j * n_def + i], att_hp[j], h, 1.0, dd);
        for (hh = 1; hh < width; hh++) {
          next[i * width + hh] += m * dd[hh];
        }
        if (i + 1 < n_def) {
          next[(i + 1) * width + def_hp[i + 1]] += m * (1.0 - srv);
        } else {
          dead += m * (1.0 - srv);
        }
      }
    }
    for (i = 0; i < n_def * width; i++) {
      state[i] = next[i];
    }
  }
  for (i = 0; i < n_def * width; i++) {
    alive += state[i];
  }
  free(state);
  free(next);
  free(dd);
  (void) alive;
  return 1.0 - dead;
}

/**********************************************************************//**
  Sweep duel parameters against the oracle.
**************************************************************************/
static void test_duel_against_oracle(void)
{
  static const int strengths[] = {0, 1, 4, 12};
  static const int fps[] = {1, 2};
  static const int hps[] = {1, 3, 10, 20};
  size_t as, ds, af, df, ah, dh;

  for (as = 0; as < sizeof(strengths) / sizeof(*strengths); as++) {
    for (ds = 0; ds < sizeof(strengths) / sizeof(*strengths); ds++) {
      for (af = 0; af < sizeof(fps) / sizeof(*fps); af++) {
        for (df = 0; df < sizeof(fps) / sizeof(*fps); df++) {
          for (ah = 0; ah < sizeof(hps) / sizeof(*hps); ah++) {
            for (dh = 0; dh < sizeof(hps) / sizeof(*hps); dh++) {
              struct deep_duel d = {
                strengths[as], fps[af], strengths[ds], fps[df]
              };
              int att_hp = hps[ah], def_hp = hps[dh];
              double dist[21] = { 0 }, odist[21] = { 0 };
              double srv = deep_duel_defender_dist(&d, att_hp, def_hp,
                                                   dist);
              double osrv = oracle_duel(&d, att_hp, def_hp, 1.0, odist);
              double sum = 0.0;
              int h;

              CHECK(fabs(srv - osrv) < EPS,
                    "duel srv %f vs oracle %f (as=%d ds=%d af=%d df=%d"
                    " ah=%d dh=%d)", srv, osrv, d.att_str, d.def_str,
                    d.att_fp, d.def_fp, att_hp, def_hp);
              for (h = 0; h <= 20; h++) {
                CHECK(fabs(dist[h] - odist[h]) < EPS,
                      "duel dist[%d] %f vs oracle %f", h, dist[h],
                      odist[h]);
                sum += dist[h];
              }
              CHECK(fabs(sum - srv) < EPS, "dist sums %f != srv %f",
                    sum, srv);
              CHECK(srv >= -EPS && srv <= 1.0 + EPS,
                    "srv %f out of range", srv);
            }
          }
        }
      }
    }
  }
}

/**********************************************************************//**
  Closed forms and invariants for the chained stack.
**************************************************************************/
static void test_stack(void)
{
  /* Parity, one-round kills: every duel is a coin flip that kills.
   * Survival of one defender vs N such attackers must be 0.5^N --
   * the collapse a linear sum cannot see. */
  {
    struct deep_duel d = { 8, 10, 8, 10 };   /* fp >= hp: one round */
    struct deep_duel pair[4];
    int att_hp[4] = { 10, 10, 10, 10 };
    int def_hp[1] = { 10 };
    int n;

    for (n = 0; n < 4; n++) {
      pair[n] = d;
    }
    for (n = 1; n <= 4; n++) {
      double got = deep_stack_survival(n, att_hp, 1, def_hp, pair);

      CHECK(fabs(got - pow(0.5, n)) < EPS,
            "parity collapse n=%d: %f != %f", n, got, pow(0.5, n));
    }
  }

  /* Empty cases. */
  {
    struct deep_duel d = { 4, 1, 4, 1 };
    int hp10[1] = { 10 };

    CHECK(deep_stack_survival(0, NULL, 1, hp10, &d) == 1.0,
          "empty stream should survive");
    CHECK(deep_stack_survival(1, hp10, 0, NULL, &d) == 0.0,
          "empty stack should fall");
  }

  /* Oracle agreement + monotonicity on a mixed stack. */
  {
    int att_hp[3] = { 20, 10, 8 };
    int def_hp[2] = { 20, 12 };
    struct deep_duel pair[3 * 2] = {
      { 12, 2, 6, 1 }, { 12, 2, 8, 2 },
      { 6, 1, 6, 1 },  { 6, 1, 8, 2 },
      { 4, 1, 6, 1 },  { 4, 1, 8, 2 },
    };
    /* One-defender pairings are column 0 of the matrix, re-packed for
     * the tighter row stride n_def == 1. */
    struct deep_duel pair1[3] = { pair[0], pair[2], pair[4] };
    double full = deep_stack_survival(3, att_hp, 2, def_hp, pair);
    double fewer_att = deep_stack_survival(2, att_hp, 2, def_hp, pair);
    double fewer_def = deep_stack_survival(3, att_hp, 1, def_hp, pair1);
    double oracle = oracle_stack(3, att_hp, 2, def_hp, pair, 20);

    CHECK(fabs(full - oracle) < EPS, "stack %f vs oracle %f", full,
          oracle);
    CHECK(fewer_att >= full - EPS,
          "removing an attacker lowered survival: %f -> %f", full,
          fewer_att);
    CHECK(fewer_def <= full + EPS,
          "removing a defender raised survival: %f -> %f", full,
          fewer_def);
    CHECK(full > 0.0 && full < 1.0, "mixed stack should be uncertain");
  }
}

/**********************************************************************//**
  The attrition output: what the stream pays to ask the question.

  Every check here is a closed form or an invariant, so the numbers can
  be followed by hand rather than trusted from a second program.
**************************************************************************/
static void test_attacker_losses(void)
{
  /* THE HAND-CHECKABLE EXAMPLE. One defender on 10 HP; every duel is
   * a coin flip decided in a single round (fp >= hp both ways), so
   * before attacker j strikes the defender is still alive with
   * probability 0.5^j, and attacker j dies exactly when it loses its
   * flip:
   *
   *     P(attacker j dies) = 0.5^j * 0.5 = 0.5^(j+1)
   *     = 0.5, 0.25, 0.125, 0.0625 for j = 0..3
   *
   * Expected dead attackers after four = 0.9375, so the expected
   * force still standing is 4 - 0.9375 = 3.0625, and the defender
   * survives all four with probability 0.5^4 = 0.0625. */
  {
    struct deep_duel d = { 8, 10, 8, 10 };
    struct deep_duel pair[4];
    int att_hp[4] = { 10, 10, 10, 10 };
    int def_hp[1] = { 10 };
    double death[4] = { 0 };
    double srv, sum = 0.0;
    int n;

    for (n = 0; n < 4; n++) {
      pair[n] = d;
    }
    srv = deep_stack_survival_losses(4, att_hp, 1, def_hp, pair, death);
    for (n = 0; n < 4; n++) {
      CHECK(fabs(death[n] - pow(0.5, n + 1)) < EPS,
            "coin-flip attacker %d dies with p %f, expected %f",
            n, death[n], pow(0.5, n + 1));
      sum += death[n];
    }
    CHECK(fabs(sum - 0.9375) < EPS, "expected dead %f != 0.9375", sum);
    CHECK(fabs((4 - sum) - 3.0625) < EPS,
          "expected survivors %f != 3.0625", 4 - sum);
    CHECK(fabs(srv - 0.0625) < EPS, "survival %f != 0.0625", srv);
  }

  /* Nothing left to fight is charged nothing: a defender that cannot
   * win a round dies to attacker 0, and attackers 1-3 walk in. */
  {
    struct deep_duel d = { 12, 10, 0, 10 };   /* p(def survives) = 0 */
    struct deep_duel pair[4];
    int att_hp[4] = { 10, 10, 10, 10 };
    int def_hp[1] = { 10 };
    double death[4] = { 0 };
    int n;

    for (n = 0; n < 4; n++) {
      pair[n] = d;
    }
    CHECK(deep_stack_survival_losses(4, att_hp, 1, def_hp, pair, death)
          < EPS, "certain kill should leave no survivor");
    for (n = 0; n < 4; n++) {
      CHECK(death[n] < EPS, "attacker %d charged %f for a walk-in",
            n, death[n]);
    }
  }

  /* Degenerate cases clear the array rather than leaving it stale. */
  {
    struct deep_duel d = { 4, 1, 4, 1 };
    int hp10[2] = { 10, 10 };
    double death[2] = { 9.0, 9.0 };

    CHECK(deep_stack_survival_losses(2, hp10, 0, NULL, &d, death) == 0.0,
          "empty stack should fall");
    CHECK(death[0] == 0.0 && death[1] == 0.0,
          "empty stack should charge nobody: %f %f", death[0], death[1]);
    CHECK(deep_stack_survival_losses(0, NULL, 2, hp10, &d, death) == 1.0,
          "empty stream should survive");
  }

  /* Invariants on the mixed stack the survival test already uses, plus
   * the accounting identity that ties the two outputs together: over
   * the whole assault every duel kills exactly one unit, so
   *
   *     E[attackers dead] + E[defenders dead] = E[duels fought]
   *
   * and with one defender E[duels] = E[attackers dead] + P(it falls).
   * The zero-loss reading a linear rating would give is impossible
   * here: someone dies in every duel. */
  {
    int att_hp[3] = { 20, 10, 8 };
    int def_hp[2] = { 20, 12 };
    struct deep_duel pair[3 * 2] = {
      { 12, 2, 6, 1 }, { 12, 2, 8, 2 },
      { 6, 1, 6, 1 },  { 6, 1, 8, 2 },
      { 4, 1, 6, 1 },  { 4, 1, 8, 2 },
    };
    double death[3] = { 0 }, sum = 0.0;
    double plain = deep_stack_survival(3, att_hp, 2, def_hp, pair);
    double full = deep_stack_survival_losses(3, att_hp, 2, def_hp, pair,
                                             death);
    int j;

    CHECK(fabs(plain - full) < EPS,
          "the losses walk changed the survival answer: %f vs %f",
          plain, full);
    for (j = 0; j < 3; j++) {
      CHECK(death[j] >= -EPS && death[j] <= 1.0 + EPS,
            "attacker %d death p %f out of range", j, death[j]);
      sum += death[j];
    }
    CHECK(sum <= 3.0 + EPS, "more than the whole stream died: %f", sum);
    CHECK(sum > 0.0, "a contested assault costs something");
  }

  /* The accounting identity, on one-round duels so that a surviving
   * defender is an undamaged one and every quantity has a closed
   * form. p(defender survives a duel) = 4/(6+4) = 0.4, so
   *
   *     P(attacker j dies) = 0.4^(j+1)     -> 0.4, 0.16, 0.064
   *     E[duels fought]    = 1 + 0.4 + 0.16 = 1.56
   *     P(defender dies)   = 1 - 0.4^3      = 0.936
   *
   * and every duel kills exactly one unit, so the expected duels must
   * equal expected attacker deaths (0.624) plus 0.936. */
  {
    struct deep_duel d = { 6, 10, 4, 10 };
    struct deep_duel pair[3];
    int att_hp[3] = { 10, 10, 10 };
    int def_hp[1] = { 10 };
    double death[3] = { 0 }, sum = 0.0, srv;
    int j;

    for (j = 0; j < 3; j++) {
      pair[j] = d;
    }
    srv = deep_stack_survival_losses(3, att_hp, 1, def_hp, pair, death);
    for (j = 0; j < 3; j++) {
      CHECK(fabs(death[j] - pow(0.4, j + 1)) < EPS,
            "attacker %d dies with p %f, expected %f", j, death[j],
            pow(0.4, j + 1));
      sum += death[j];
    }
    CHECK(fabs(srv - pow(0.4, 3)) < EPS, "survival %f != 0.064", srv);
    CHECK(fabs(1.56 - sum - (1.0 - srv)) < EPS,
          "duels 1.56 != attacker deaths %f + defender death %f",
          sum, 1.0 - srv);
  }
}

int main(void)
{
  test_duel_against_oracle();
  test_stack();
  test_attacker_losses();

  if (failures > 0) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  printf("deepodds: all checks passed\n");
  return EXIT_SUCCESS;
}
