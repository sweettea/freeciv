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
#ifndef FC__DEEPRATES_H
#define FC__DEEPRATES_H

/* common */
#include "fc_types.h"

struct ai_type;
struct player;

/* Everything the split decision needs, gathered from the live empire
 * so that the decision itself is a pure function of numbers. Split out
 * because "what is a bulb worth" and "which triple wins" are separate
 * claims and only the first one needs the game.
 *
 * The three yields are LINEAR EXTRAPOLATIONS of what the empire
 * actually produced last turn, not estimates from total trade: at the
 * current science rate s0 the player really did research bulbs_last
 * bulbs, so a rate point is worth bulbs_last/s0 bulbs. That reads
 * through corruption, waste, tithes, Output_Bonus buildings and every
 * other multiplier without modelling any of them. When a rate is 0
 * there is nothing to divide by and the trade-share estimate
 * (stock's distribute()) is used for that output instead. */
struct deep_rate_model {
  int maxrate;                  /* Government/handicap ceiling per rate */
  int step;                     /* Rate granularity; stock's is 10 */
  double bulb_slope;            /* Bulbs per point of science rate */
  double gold_slope;            /* Gross gold per point of tax rate */
  double lux_slope;             /* Luxury per point of luxury rate */
  double expenses;              /* Gold upkeep per turn */
  double treasury;              /* Gold on hand */
  double reserve;               /* dai_gold_reserve(): the buy float */

  /* Science: what a bulb is worth per turn, and how long the current
   * research still has to run. sci_priority is the advisor's own
   * price of a trade point spent on research -- a WAG by its own
   * source comment, and used here because it is the price the REST of
   * deep already pays for a bulb (deep_multiplier_credit values a
   * Library's science with exactly it). The amortization is the part
   * stock never does: bulbs bought a long way from the next advance
   * buy a discounted advance.
   *
   * tech_want is last turn's read of stock's tech_want[] array, which
   * is the only place in the codebase a TECH is priced. It gates the
   * override (no price, no opinion) and is logged, but it does not
   * scale the term: tech_want lives in stock's want currency and
   * every other term here lives in the advisor's tile-output
   * currency, and the conversion between them is W01's `want_t`,
   * which was declared and never built. Naming that gap is the
   * honest answer; guessing the ratio would be rule 7. */
  double tech_want;
  double bulbs_needed;
  double sci_priority;
  double sci_gain;
  /* Research solvency, the mirror of the gold constraint: bulbs the
   * ruleset charges every turn to KEEP what we know, and the bank
   * that can absorb a shortfall. Stock guards this with its own
   * rate_sci_min scan; deep guards it as a feasibility test so the
   * merit comparison cannot trade a tech away for luxury. */
  double tech_upkeep;
  double bulb_bank;

  /* Luxury, pre-summed PER CITY for each of the eleven luxury rates.
   * It has to be per city: a city that is already content gets
   * nothing from more luxury while its neighbour is paying three
   * entertainers, and pooling the empire's luxury against the
   * empire's entertainers credits the content city's share to the
   * unhappy one. Entry i is the freed-citizen value per turn at
   * luxury rate 10*i, summed over cities, each city's own
   * entertainer count saturating its own term. */
  double lux_by_rate[11];
  double free_cap;              /* Empire entertainer-equivalents (log) */
  double per_citizen;           /* Their mean tile value (log) */
  double happy_cost;            /* Luxury per happiness step (ruleset) */
  double lux_gain;

  /* Gold: the advisor's price of a trade point taken as gold. Same
   * family as sci_priority and same currency as per_citizen (which is
   * a sum of tile outputs times these very priorities), so the three
   * terms are commensurable BY CONSTRUCTION. The first cut priced
   * gold at upkeep_gold_weight instead -- deep's own gold price --
   * and measured a degenerate all-luxury split, because that knob
   * values a gold at 3 want against a freed citizen's 1300. One
   * empire cannot hold two prices for a trade point. */
  double gold_priority;
};

struct deep_rate_pick {
  int sci, tax, lux;
  double value;                 /* Total want of the winner */
  double v_sci, v_gold, v_lux;  /* Its three terms */
  int considered;               /* Legal triples scored */
  int feasible;                 /* Of those, solvent ones */
};

struct deep_rate_pick deep_rates_pick(const struct deep_rate_model *m);

void deep_rates_snapshot_tech(struct ai_type *ait, struct player *pplayer);
void deep_manage_taxes(struct ai_type *ait, struct player *pplayer);

#endif /* FC__DEEPRATES_H */
