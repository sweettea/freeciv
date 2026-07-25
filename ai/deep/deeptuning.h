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
#ifndef FC__DEEPTUNING_H
#define FC__DEEPTUNING_H

/* Tunable constants of the deep AI.
 *
 * Every value here is a PLACEHOLDER until it has been through a
 * benchmark/tune.py pass on training seeds (W25); new entries must be
 * flagged in doc/deep-ai/STATUS.md. Do not hand-adjust values to chase
 * benchmark numbers. */
struct deep_tuning {
  /* Settler pathfinding fearfulness as a multiple of the advisors'
   * NORMAL_STACKING_FEARFULNESS. Kept above 1: losing a settler also
   * wastes every turn already spent walking (W05). */
  double settler_fearfulness_mult;

  /* How steeply an en-route defender's contribution to its target
   * city's effective danger reduction decays per turn of distance
   * (W11): credit = power * survival^eta / (1 + eta_weight * eta). */
  double incoming_defence_eta_weight;
  double incoming_defence_survival;

  /* Defensive-diplomat priority (W01): stock prices it at a
   * self-described 'totally arbitrary' 16000 that outranks even
   * military emergencies. Deep re-expresses it as an emergency-tier
   * value; the default sits just above the defender emergency range
   * observed in the calibration corpus (Phalanx p90 native 1224 =
   * tier value 224), honouring the stock comment 'more important
   * than soldiers'. */
  double diplomat_defence_value;

  /* Explorer production want (W06): build-want units per visible
   * unclaimed hut and per unknown tile, divided by (1 + explorers
   * owned). Zero-gated when the map was generated without huts. */
  double explore_hut_value;
  double explore_discovery_value;
  /* City manager factors by city state (W09-lite); the stock
   * hardcoded default is food 10-20, shield 5, trade 0, happy 0. */
  int cm_happy_factor;
  int cm_war_food;
  int cm_war_shield;
  int cm_war_trade;
  int cm_cap_food;
  int cm_cap_shield;
  int cm_cap_trade;
  int cm_wonder_shield;
  int cm_wonder_trade;
};

void deep_tuning_init(void);
const struct deep_tuning *deep_tuning_get(void);

#endif /* FC__DEEPTUNING_H */
