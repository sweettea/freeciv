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

  /* Science: what one research STEP is worth, and how far away the
   * current one is. tech_want is deep's own read of stock's
   * tech_want[] array, which is the only place a tech is priced at
   * all. */
  double tech_want;
  double bulbs_needed;
  double sci_gain;

  /* Luxury: how many citizens are currently bought off the tiles by
   * unhappiness (plus those the luxury already flowing is paying
   * for), and what one of them earns back per turn. */
  double free_cap;
  double per_citizen;
  double happy_cost;            /* Luxury per happiness step (ruleset) */
  double content_weight;
  double lux_gain;

  double gold_weight;           /* upkeep_gold_weight */
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
