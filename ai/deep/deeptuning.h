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
 * benchmark numbers.
 *
 * Adding a field takes FOUR edits or the field silently cannot be
 * tuned (this happened; see STATUS.md "Tuning loader gap"):
 *   1. this struct;  2. deep_tuning_defaults in deeptuning.c;
 *   3. a TUNE_ line in deep_tuning_init();  4. the deep-tuning
 *   log line there;  plus benchmark/tuning-params.ini to make it
 *   visible to tune.py. */
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

  /* Weight of catchment tiles beyond the food-supportable target
   * size in site evaluation (W02); tiles inside it weigh 1.0. */
  double site_tail_weight;

  /* Share of a tile's post-improvement yield increment counted at
   * settle time, and the explicit resource-special bonuses, all in
   * the tile-sum currency (W02). */
  double site_depth_weight;
  /* Weight for catchment tiles a neighbouring own city can work --
   * deliberately far below the ordinary tail weight. */
  double site_overlap_weight;
  double site_potential_discount;
  double site_special_bonus;
  double site_special_cluster;

  /* Horizon credits for buildings (W03), in the tile-sum currency:
   * scale on the unlocked-growth credit for capacity buildings, the
   * relative share for growth accelerators, and the per-viable-tile
   * weight on co-investment bundle effects. */
  double horizon_capacity_weight;
  double horizon_growth_weight;
  double horizon_bundle_weight;
  /* Credit per point of Health_Pct once the city is within one pop
   * of the ruleset's illness threshold: plague insurance is worth
   * having before the size gate the building's other effects wait
   * for. */
  double horizon_health_weight;
  /* Share of a tech-gated cap-lifter's capacity credit routed into
   * the gating advance's tech want (the credit cannot buy the
   * building, but it can buy the research). */
  double horizon_tech_weight;

  /* Border contest terms (W07-lite, landed with W02): per catchment
   * tile owned by a non-allied player (founding here steals or denies
   * that space), and the horizon compression under pressure -- how
   * much of the potential/tail value survives when a non-allied city
   * sits within contest range of the site. */
  double site_claim_bonus;
  double site_pressure_factor;

  /* Defensive-diplomat priority (W01): stock prices it at a
   * self-described 'totally arbitrary' 16000 that outranks even
   * military emergencies. Deep re-expresses it as an emergency-tier
   * value; the default sits just above the defender emergency range
   * observed in the calibration corpus (Phalanx p90 native 1224 =
   * tier value 224), honouring the stock comment 'more important
   * than soldiers'. */
  double diplomat_defence_value;

  /* Fear of enemies the stock danger assessment cannot see: at-war
   * military power on other continents (or at sea) adds to a coastal
   * city's danger, scaled by fear and decayed per tile of
   * straight-line distance. Found via head-to-head island games
   * (2026-07-28): assess_danger() is blind to boat invasions. */
  double overseas_fear;
  double overseas_decay;
  /* Token garrison kept even at zero assessed danger: requisition a
   * defender while the city tile holds fewer than garrison_floor
   * units, by injecting garrison_floor_danger into the danger field
   * (same fix family as overseas_fear). */
  int garrison_floor;
  int garrison_floor_danger;
  /* Slope on the met-neighbour militarisation ratio (their summed
   * military rating / ours) that scales the garrison floor: 0 before
   * contact, capped at 1. Zero disables the scaling entirely. Each
   * neighbour is weighted by reachability: same-continent armies in
   * full, wholly-overseas armies at menace_overseas_weight (they
   * must build and sail boats before they are a garrison problem). */
  double garrison_menace_slope;
  double menace_overseas_weight;
  /* Flat danger added to every city from the turn the server can
   * first spawn barbarians (barbarianrate/onsetbarbarian gate);
   * zero before that turn. */
  int barbarian_fear;
  /* Standing coastal fear while any at-war player owns transport-
   * capable ships: the embarked-cargo term warns ~1 turn ahead, the
   * boat shadow funds the garrison before the boats load. Also
   * vetoes grow-hard on exposed coasts via the CM danger gate. */
  int boat_shadow_fear;
  /* Convoy discipline: a transport with cargo holds in port until it
   * carries convoy_min_load units (<=1 disables) while boarders are
   * inbound within convoy_wait_radius tiles, and any cargo-carrying
   * transport recruits an idle warship bodyguard from its tile. */
  int convoy_min_load;
  int convoy_wait_radius;
  /* A civilian outside a city flees to the nearest own city when an
   * at-war military unit is within this many tiles. 0 disables. */
  int worker_flee_radius;
  /* A working civilian on unowned ground, or with an at-war unit
   * within this many tiles, requests a bodyguard (escorts arrive
   * before the flee radius is breached). 0 disables. */
  int worker_guard_radius;

  /* Ruleset-upkeep awareness (2026-07-28 civ2civ3 analysis: deep
   * bankrupted itself head-to-head, gold ratio 0.13-0.55 vs stock).
   * upkeep_gold_weight charges a building's gold upkeep against its
   * horizon credit; upkeep_unit_weight discounts the injected fear
   * terms by the cheapest buildable defender's gold+food upkeep:
   * discount = 1 / (1 + weight * upkeep). */
  double upkeep_gold_weight;
  double upkeep_unit_weight;

  /* Rotation of obsolete surplus defenders (findings doc): melt the
   * weakest upkeep-costing defender above the garrison floor when a
   * defender rotation_power_ratio times stronger is buildable and
   * the city is quiet (<=0 disables); rotation_want is the danger
   * injected to queue the modern replacement first. */
  double rotation_power_ratio;
  int rotation_want;

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
  /* Grow-hard state: cities below cm_grow_below (and not at war,
   * capped, or wonder-building) weight food heavily -- growth
   * compounds and civ2civ3 has no rapture. 0 disables the state. */
  int cm_grow_below;
  /* Grow-hard resumes when linear defence exceeds
   * cm_grow_safety_margin x linear danger. Below 1 trusts the
   * defence multipliers and the overcounting danger sum; above 1
   * prices sequential attrition. Neutral 1.0 until tuned. */
  double cm_grow_safety_margin;
  /* Weight on garrison attack ratings counted as defence in the
   * grow-release coverage test: with killstack, attack-heavy units
   * defend by sortie. 0 disables. */
  double sortie_defence_weight;
  int cm_grow_food;
  int cm_grow_shield;
  int cm_grow_trade;
  /* Exponent on catchment trade-richness (workable trade / workable
   * shields) scaling every CM state's trade factor: the map sets the
   * trade appetite. 0 restores plain constants; 1 is proportional. */
  double cm_trade_quality_slope;
};

void deep_tuning_init(void);
enum log_level deep_metric_log_level(void);
const struct deep_tuning *deep_tuning_get(void);

#endif /* FC__DEEPTUNING_H */
