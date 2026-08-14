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

/* L0 unit test for the ATTRITION half of ai/deep/deepodds.c: what an
 * assault is expected to COST the attacker, which is the output the
 * cost-of-victory want and the hold-the-city test are built on
 * (combat E2, military audit U1/G1).
 *
 * Every check here is a closed form or an accounting identity, so
 * the numbers can be followed by hand rather than trusted from a
 * second program. Its sibling tests/deepodds_test.c checks the
 * survival probability itself against an independent oracle. */

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

/**********************************************************************//**
  The DEFENDER's side of the same ledger (M4b): the expected number of
  defenders killed, which is the expected number of SUCCESSFUL enemy
  attacks and therefore the citizen bill under killcitizen.

  Same closed forms as above, read from the other end.
**************************************************************************/
static void test_defender_losses(void)
{
  /* One defender, coin-flip duels. It dies unless it wins all four
   * flips, so E[defenders killed] = P(it dies) = 1 - 0.5^4 = 0.9375,
   * and with one defender that is also 1 - survival exactly. */
  {
    struct deep_duel d = { 8, 10, 8, 10 };
    struct deep_duel pair[4];
    int att_hp[4] = { 10, 10, 10, 10 };
    int def_hp[1] = { 10 };
    double srv, killed = -1.0;
    int n;

    for (n = 0; n < 4; n++) {
      pair[n] = d;
    }
    srv = deep_stack_survival_full(4, att_hp, 1, def_hp, pair, NULL,
                                   &killed);
    CHECK(fabs(killed - 0.9375) < EPS,
          "expected defenders killed %f != 0.9375", killed);
    CHECK(fabs(killed - (1.0 - srv)) < EPS,
          "with one defender, killed %f must equal 1 - survival %f",
          killed, 1.0 - srv);
  }

  /* THE ACCOUNTING IDENTITY, which is what makes the new output
   * checkable rather than merely plausible: every duel kills exactly
   * one unit and every attacker fights at most once, so
   *
   *     E[attackers dead] + E[defenders dead] = E[duels fought]
   *
   * and a duel is fought by attacker j exactly when the stack is still
   * alive at its turn. With p(defender survives a duel) = 0.4 and one
   * defender: duels = 1 + 0.4 + 0.16 = 1.56, attacker deaths = 0.624,
   * so defender deaths must be 0.936. */
  {
    struct deep_duel d = { 6, 10, 4, 10 };
    struct deep_duel pair[3];
    int att_hp[3] = { 10, 10, 10 };
    int def_hp[1] = { 10 };
    double death[3] = { 0 }, sum = 0.0, killed = -1.0;
    int j;

    for (j = 0; j < 3; j++) {
      pair[j] = d;
    }
    (void) deep_stack_survival_full(3, att_hp, 1, def_hp, pair, death,
                                    &killed);
    for (j = 0; j < 3; j++) {
      sum += death[j];
    }
    CHECK(fabs(killed - 0.936) < EPS,
          "defenders killed %f != 0.936", killed);
    CHECK(fabs((sum + killed) - 1.56) < EPS,
          "attacker deaths %f + defender deaths %f != duels 1.56",
          sum, killed);
  }

  /* A stack of two against a stream that cannot lose: both defenders
   * die, and the count is the stack size rather than a probability. */
  {
    struct deep_duel d = { 12, 10, 0, 10 };   /* p(def survives) = 0 */
    struct deep_duel pair[4 * 2];
    int att_hp[4] = { 10, 10, 10, 10 };
    int def_hp[2] = { 10, 10 };
    double killed = -1.0;
    int n;

    for (n = 0; n < 4 * 2; n++) {
      pair[n] = d;
    }
    CHECK(deep_stack_survival_full(4, att_hp, 2, def_hp, pair, NULL,
                                   &killed) < EPS,
          "certain kill should leave no survivor");
    CHECK(fabs(killed - 2.0) < EPS,
          "both defenders should be counted dead, got %f", killed);
  }

  /* Degenerate cases write the output rather than leaving it stale,
   * and an empty stream kills nobody. */
  {
    struct deep_duel d = { 4, 1, 4, 1 };
    int hp10[2] = { 10, 10 };
    double killed = 9.0;

    CHECK(deep_stack_survival_full(0, NULL, 2, hp10, &d, NULL, &killed)
          == 1.0, "empty stream should survive");
    CHECK(killed == 0.0, "empty stream should kill nobody, got %f",
          killed);
    killed = 9.0;
    CHECK(deep_stack_survival_full(2, hp10, 0, NULL, &d, NULL, &killed)
          == 0.0, "empty stack should fall");
    CHECK(killed == 0.0,
          "an empty stack has nobody to lose, got %f", killed);
  }

  /* Bounds on the mixed stack: never more than the stack, never less
   * than P(the stack is annihilated) times the stack. */
  {
    int att_hp[3] = { 20, 10, 8 };
    int def_hp[2] = { 20, 12 };
    struct deep_duel pair[3 * 2] = {
      { 12, 2, 6, 1 }, { 12, 2, 8, 2 },
      { 6, 1, 6, 1 },  { 6, 1, 8, 2 },
      { 4, 1, 6, 1 },  { 4, 1, 8, 2 },
    };
    double killed = -1.0;
    double srv = deep_stack_survival_full(3, att_hp, 2, def_hp, pair,
                                          NULL, &killed);

    CHECK(killed >= 2.0 * (1.0 - srv) - EPS,
          "killed %f below the annihilation floor %f", killed,
          2.0 * (1.0 - srv));
    CHECK(killed <= 2.0 + EPS, "killed %f exceeds the stack", killed);
    CHECK(killed > 0.0, "a contested assault kills somebody");
  }
}

/**********************************************************************//**
  The presence-weighted walk (NETTHREAT item 2's substrate): att_w[j]
  is P(attacker j fights at all), and because the per-attacker DP
  transition is linear, the weighted walk must equal the exact mixture
  of the with-him and without-him walks. Every check is that identity
  or a closed form.
**************************************************************************/
static void test_presence_weights(void)
{
  /* Certain presence is the old walk, bit for bit: NULL weights and
   * all-1.0 weights agree with deep_stack_survival_full exactly (the
   * == is deliberate -- multiply-by-1.0 is IEEE-exact, and the
   * byte-identity gate of the consuming knob rests on it). */
  {
    int att_hp[3] = { 20, 10, 8 };
    int def_hp[2] = { 20, 12 };
    struct deep_duel pair[3 * 2] = {
      { 12, 2, 6, 1 }, { 12, 2, 8, 2 },
      { 6, 1, 6, 1 },  { 6, 1, 8, 2 },
      { 4, 1, 6, 1 },  { 4, 1, 8, 2 },
    };
    double w1[3] = { 1.0, 1.0, 1.0 };
    double da[3], db[3], dc[3];
    double ka = -1.0, kb = -1.0, kc = -1.0;
    double plain = deep_stack_survival_full(3, att_hp, 2, def_hp, pair,
                                            da, &ka);
    double wnull = deep_stack_survival_wfull(3, att_hp, NULL, 2, def_hp,
                                             pair, db, &kb);
    double wones = deep_stack_survival_wfull(3, att_hp, w1, 2, def_hp,
                                             pair, dc, &kc);
    int j;

    CHECK(plain == wnull && plain == wones,
          "certain presence must be bit-identical: %.17g %.17g %.17g",
          plain, wnull, wones);
    CHECK(ka == kb && ka == kc,
          "def_death must be bit-identical: %.17g %.17g %.17g",
          ka, kb, kc);
    for (j = 0; j < 3; j++) {
      CHECK(da[j] == db[j] && da[j] == dc[j],
            "att_death[%d] must be bit-identical: %.17g %.17g %.17g",
            j, da[j], db[j], dc[j]);
    }
  }

  /* Weight 0 is absence: zeroing the middle attacker equals the
   * two-attacker stream that never contained it. */
  {
    int att_hp3[3] = { 20, 10, 8 };
    int att_hp2[2] = { 20, 8 };
    int def_hp[1] = { 12 };
    struct deep_duel pair3[3] = {
      { 12, 2, 8, 2 }, { 6, 1, 8, 2 }, { 4, 1, 8, 2 },
    };
    struct deep_duel pair2[2] = {
      { 12, 2, 8, 2 }, { 4, 1, 8, 2 },
    };
    double w[3] = { 1.0, 0.0, 1.0 };
    double zeroed = deep_stack_survival_wfull(3, att_hp3, w, 1, def_hp,
                                              pair3, NULL, NULL);
    double absent = deep_stack_survival_full(2, att_hp2, 1, def_hp,
                                             pair2, NULL, NULL);

    CHECK(zeroed == absent,
          "weight 0 must equal absence: %.17g vs %.17g", zeroed, absent);
  }

  /* The mixture identity, which is the semantics: with one attacker
   * weighted w among certain companions, every output equals
   * (1-w) x the without-him walk + w x the with-him walk. */
  {
    int att_hp3[3] = { 20, 10, 8 };
    int att_hp2[2] = { 20, 8 };
    int def_hp[2] = { 20, 12 };
    struct deep_duel pair3[3 * 2] = {
      { 12, 2, 6, 1 }, { 12, 2, 8, 2 },
      { 6, 1, 6, 1 },  { 6, 1, 8, 2 },
      { 4, 1, 6, 1 },  { 4, 1, 8, 2 },
    };
    struct deep_duel pair2[2 * 2] = {
      { 12, 2, 6, 1 }, { 12, 2, 8, 2 },
      { 4, 1, 6, 1 },  { 4, 1, 8, 2 },
    };
    double w[3] = { 1.0, 0.3, 1.0 };
    double d3[3], dw[3];
    double kwith = -1.0, kwithout = -1.0, kmix = -1.0;
    double with = deep_stack_survival_full(3, att_hp3, 2, def_hp, pair3,
                                           d3, &kwith);
    double without = deep_stack_survival_full(2, att_hp2, 2, def_hp,
                                              pair2, NULL, &kwithout);
    double mixed = deep_stack_survival_wfull(3, att_hp3, w, 2, def_hp,
                                             pair3, dw, &kmix);

    CHECK(fabs(mixed - (0.7 * without + 0.3 * with)) < EPS,
          "survival mixture: %f != 0.7*%f + 0.3*%f",
          mixed, without, with);
    CHECK(fabs(kmix - (0.7 * kwithout + 0.3 * kwith)) < EPS,
          "def_death mixture: %f != 0.7*%f + 0.3*%f",
          kmix, kwithout, kwith);
    CHECK(dw[1] <= 0.3 + EPS,
          "a 0.3-present attacker dies at most 0.3 of the time: %f",
          dw[1]);
  }

  /* Closed form: one coin-flip attacker present half the time against
   * one defender. It fights with p 0.5 and wins its flip with p 0.5,
   * so the defender falls with p 0.25 and the attacker dies with
   * p 0.5 x 0.5 = 0.25. */
  {
    struct deep_duel d = { 8, 10, 8, 10 };
    int att_hp[1] = { 10 };
    int def_hp[1] = { 10 };
    double w[1] = { 0.5 };
    double death[1] = { -1.0 };
    double srv = deep_stack_survival_wfull(1, att_hp, w, 1, def_hp, &d,
                                           death, NULL);

    CHECK(fabs(srv - 0.75) < EPS, "half-present coin flip: %f != 0.75",
          srv);
    CHECK(fabs(death[0] - 0.25) < EPS,
          "half-present attacker death %f != 0.25", death[0]);
  }
}

int main(void)
{
  test_attacker_losses();
  test_defender_losses();
  test_presence_weights();

  if (failures > 0) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  printf("deepattrition: all checks passed\n");
  return EXIT_SUCCESS;
}
