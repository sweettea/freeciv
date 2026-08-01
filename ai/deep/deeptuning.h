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

  /* Multiplier on the overnight-danger bar for settler pathing
   * (human doctrine 2026-08-01: a hard any-threat veto is too
   * absolute -- some loss chance is affordable on the way to a very
   * good site). The veto fires when enemy a^2*10 >= defence *
   * THIS; 1.0 is exactly the stock bar, above 1.0 is braver. Mild
   * danger stays priced by fearfulness either way. Site-value
   * modulation (braver toward better sites) is the eventual form;
   * v4 owns the number. */
  double settler_risk_tolerance;

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
  /* Border-pressure fear: each met non-allied city within the radius
   * adds value/(1+dist) standing danger. 0 disables. */
  int density_fear_value;
  int density_fear_radius;
  int density_fear_ramp_turns;
  /* Pre-contact prior from lobby settings: shared-continent startpos
   * on thin land predicts early contact before anyone is met. Value
   * scales by min(1, prior_tiles / land-per-player); off at first
   * contact. 0 disables. */
  int density_prior_value;
  int density_prior_tiles;
  /* Injected fears also fund WALLS (the upkeep-free defence):
   * building_want[walls] += weight * danger. 0 disables. */
  double wall_fear_weight;
  /* Land-race coupling: fresh founder wants scale by
   * (1 + weight * map crowding); crowded maps race, empty ones
   * don't. 0 disables. */
  double settler_rush_weight;
  /* Site bonus (tile-sum currency, scaled by crowding) for local
   * isthmus tiles -- a city in a gate is worth a garrison army.
   * 0 disables. */
  double site_choke_bonus;
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
  /* Discount on auto-worker candidate values for exposed tiles
   * (unowned ground or at-war unit within worker_guard_radius):
   * value /= (1 + discount). 0 disables. */
  double worker_danger_discount;

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
  /* Multiplier (<1 relaxes) on the obsolescence bar when the melt's
   * recovered shields would finish the city's current build. */
  double rotation_finish_relax;

  /* Embassy science (W06-adjacent): extra diplomat build-want per
   * met non-allied player we lack an embassy with, active only under
   * Embassies tech leakage (each missing embassy is forgone research
   * discount). 0 disables. */
  double embassy_science_value;

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

  /* Sequential-attrition survival model (combat workstream): a city's
   * garrison must survive the observed attacker wave with probability
   * survival_target, computed by chained duels (deepodds.c) over
   * at-war units reachable within survival_horizon turns. When the
   * odds fall short, survival_fear_value scales the deficit into the
   * danger field. target <= 0 disables every survival consumer. */
  double survival_target;
  double survival_fear_value;
  int survival_horizon;
  /* Tired-attack discipline: a unit adjacent to its attack target
   * with only a tired strike left (moves_left < SINGLE_MOVE scales
   * attack power down) holds a turn to strike fresh, unless its
   * tired win chance already reaches this floor. <= 0 disables. */
  double tired_attack_floor;
  /* Marginal-take-odds attacker want (combat E2): one more attacker
   * is worth weight x target worth x the shift it makes in P(pool
   * takes the best at-war city on this continent), amortized over
   * the march. <= 0 disables. */
  double attack_value_weight;
  /* Kill sortie (E3): a garrison blade attacks an adjacent besieger
   * when the duel odds clear this floor AND the city still clears
   * survival_target without it. <= 0 disables. */
  double sortie_win_floor;
  /* Assault operations (E6): form when the free-attacker pool's take
   * odds against the best met target clear op_take_target (<= 0
   * disables); muster at the staging city up to op_muster_wait turns
   * before launching with whoever stands there (at least half the
   * quorum) or dissolving. */
  double op_take_target;
  int op_muster_wait;
  /* Satisfaction gate (T153 forensics): once the garrison floor is
   * met, urgency and grave danger are zero, and survival against
   * the observed wave clears the target, the injected fear terms
   * scale by this factor so the domestic ladder can win the build
   * auction. 0 = satisfied cities are quiet (default; also lets the
   * danger==0 rotation hygiene run); >= 1 disables the gate. */
  double satisfied_fear_discount;
  /* Colony lift (task #20): weight on a fresh lift-limited overseas
   * site score converted into a transport build want in coastal
   * cities, amortized over the boat's build time. <= 0 disables. */
  double colony_lift_weight;
  /* Naval slice 3: virtual defenders per turn of sail time added to
   * a target's garrison when pricing an amphibious take -- travel
   * time is the defender's budget. 0 prices arrival like adjacency. */
  double invasion_reinforce_rate;
  /* Second waves (E7): for this many turns after a strike launches,
   * the reserve gets first claim on the next operation against the
   * same target -- reinforce while the garrison is softened, or let
   * the window close as the formal withdrawal. */
  int op_wave_window;
  /* Raids (E8): with no assault operation standing, idle fast
   * attackers hunt exposed enemy civilians within this many tiles.
   * 0 disables. */
  int raid_radius;
  /* Hut risk (task #18): while the server can unleash hut
   * barbarians, huts within hut_risk_radius of an own city count at
   * hut_risk_discount in the explore want -- far huts stay full
   * value (a lost explorer is a fine trade for tech and gold; a
   * barbarian spawn at home is not). radius 0 disables. */
  int hut_risk_radius;
  double hut_risk_discount;
  /* March re-gate (W33 commit 2): a marching wave re-prices the take
   * each turn at the remaining eta and turns back -- banking every
   * member -- when the odds fall below this floor. Set below
   * op_take_target for hysteresis: a wave formed at the bar should
   * not turn back on the first wobble. <= 0 disables the re-gate. */
  double op_abort_floor;
  /* Barbarian targets reinforce at this rate instead of
   * invasion_reinforce_rate (T160: barbarians field armies and lean
   * against garrisoning their takes -- 47/52 cities empty). 0 prices
   * a barb city exactly as it stands today. */
  double barb_reinforce_rate;
};

void deep_tuning_init(void);
enum log_level deep_metric_log_level(void);
const struct deep_tuning *deep_tuning_get(void);

#endif /* FC__DEEPTUNING_H */
