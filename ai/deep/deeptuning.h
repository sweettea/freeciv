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

  /* Survival-deficit fear multiplier for a city none of whose
   * LAUNCHED assault members (MARCH onward) could reach it within
   * survival_horizon (human doctrine 2026-08-01: a faraway
   * committed army thins the home front, so reactive defence should
   * outbid sooner). 1.0 = no posture effect; mustering armies
   * instead CREDIT nearby cities through the W11 reserve credit. */
  double committed_fear_mult;

  /* Multiplier on the overnight-danger bar for settler pathing
   * (human doctrine 2026-08-01: a hard any-threat veto is too
   * absolute -- some loss chance is affordable on the way to a very
   * good site). The veto fires when enemy a^2*10 >= defence *
   * THIS; 1.0 is exactly the stock bar, above 1.0 is braver. Mild
   * danger stays priced by fearfulness either way. Site-value
   * modulation (braver toward better sites) is the eventual form;
   * v4 owns the number. */
  double settler_risk_tolerance;

  /* The same danger bar, but for asking for an ESCORT rather than for
   * refusing to move. deep_settler_bodyguard_needed() sampled the
   * planned route with a bare adv_danger_at(), so the same trip was
   * judged by a parameterised bar for movement and an unparameterised
   * one for protection. Lower than settler_risk_tolerance means "ask
   * for help before you would refuse to go", which is the sensible
   * ordering. PLACEHOLDER 1.0 = the stock bar = exactly the previous
   * behaviour, so this starts inert and wants a tuning pass. */
  double settler_guard_tolerance;

  /* Site score above which the on-continent search stops and never
   * considers going overseas at all (deepsettler.c phase 1). Stock
   * hard-codes RESULT_IS_ENOUGH = 250 in daisettler.h -- a
   * compile-time constant that decides whether colonisation is even
   * evaluated, which makes it one of the highest-leverage numbers in
   * the expansion pipeline and the kind of inherited magic the project
   * exists to question. PLACEHOLDER 250.0 reproduces stock exactly.
   * Raising it makes the AI look overseas even when it has a decent
   * site at home; lowering it makes it settle for less, faster. */
  double site_enough_result;

  /* Founders to keep alive per SITE the unclaimed land could still
   * hold (deep_remaining_sites()) before Join City may dissolve one. A
   * founder pool must be nonzero -- a settler in hand is the only way
   * to take a site the moment it opens -- but never big, because every
   * one is two citizens and a shield bill standing on the map (human
   * call 2026-08-03).
   *
   * Per SITE, not per city: keying off city count inverts on a roomy
   * map, where many cities drove the target DOWN while settlers were
   * still usefully expanding (findings/idle-workers.md).
   *
   * PLACEHOLDER 0.06, and the derivation is worth keeping because the
   * two cells disagree by 3.5x. Sites remaining measures ~13 on
   * lategame-80 and ~175 on primary-120. Reproducing each cell's
   * observed pre-join pool would want 2.73/13.2 = 0.207 there and
   * 10.42/174.7 = 0.060 here. 0.060 calibrates on primary-120 BECAUSE
   * its pool is the one demonstrably doing work -- 10.4 live founders
   * against only 429 idle unit-turns in ten games, i.e. expanders, not
   * strays -- and it then puts lategame-80's target near 0.8, which
   * banks that cell's idle founders, which is the whole objective
   * there. The 3.5x spread is exactly what a per-cell tuning pass is
   * for; do not average it away silently. */
  double founder_pool_per_site;

  /* How steeply an en-route defender's contribution to its target
   * city's effective danger reduction decays per turn of distance
   * (W11): credit = power * survival^eta / (1 + eta_weight * eta). */
  double incoming_defence_eta_weight;
  double incoming_defence_survival;

  /* Scale of the catchment stream -- the discounted citizen-turns a
   * site will earn (W02b) -- against the city-centre tile it is added
   * to. 1.0 is the model as designed; the knob exists because the
   * stream replaced a weighted average and moved the founder_want
   * scale, and because founder want is arbitrated against building
   * want, whose own scale is set elsewhere (W01).
   *
   * This is the ONE free parameter of the catchment valuation. It
   * replaces site_tail_weight (the tail is now where the growth
   * simulation stops) and site_depth_weight (there is no separate
   * depth term to weigh: the stream IS the catchment's value). */
  double site_stream_weight;
  /* Reference land-per-player for deep_land_pressure(), the scale at
   * which land counts as half scarce: pressure = tiles / (tiles +
   * land-per-player). Deliberately its own knob rather than reusing
   * density_prior_tiles, so the tuner cannot move site spacing while
   * it is really adjusting a fear prior. */
  int land_pressure_tiles;
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
  /* Credit per citizen a Make_Content/Force_Content building frees
   * from pacification duty, in the same tile-sum currency as the
   * capacity credit. Unhappiness is the SOFT size cap: past it the
   * city still grows, but each new citizen becomes an entertainer
   * earning nothing instead of working a tile. Stock cannot see this
   * at all -- happiness effects are #if 0'd out of advdata.c's
   * impr_calc switch, so a Temple's base_want() is structurally ZERO
   * and its whole want is a 35-per-currently-unhappy-citizen WAG,
   * which is today's-state pricing of a future-state asset: exactly
   * the defect W03 was built to fix, in the column W03 skipped. */
  double horizon_content_weight;
  /* Credit for a percentage output building (Marketplace, Library,
   * Bank, Factory), scaling the output the citizens arriving within
   * the horizon will produce. This is the one ladder column stock DOES
   * price -- base_want() runs for it -- but on acity->citizen_base,
   * the tiles worked TODAY, while every other credit here is buying
   * the citizens that work more tiles TOMORROW. Nothing told the
   * multiplier about the growth being bought for it. */
  double horizon_multiplier_weight;
  /* Settler site evaluation. These replace three incompatible uses of
   * stock's PERFECTION constant (daisettler.h:56), which is DOCUMENTED
   * as a search-effort bound -- "how long to search for optimal
   * solutions" -- and is then also used as a travel-discount exponent
   * and, via integer PERFECTION/2, as a yield multiplier.
   *
   * site_travel_discount multiplies the travel time before amortize(),
   * so with MORT=24 the retained value is (23/24)^(d*turns).
   * site_travel_crowding then REDUCES that discount as good sites get
   * scarce, which is what PERFECTION's own comment asks for and a
   * compile-time constant cannot do: on a packed map the frontier is
   * the only land left and the walk is the price of expanding at all.
   *
   * W02b gives d a MEANING it did not have. Now that the catchment is
   * a stream of citizen-turns, founding T turns later delays every
   * citizen arrival by exactly T turns -- which is d = 1. Stock's
   * PERFECTION 3 claims the delay is three times worse than it is; the
   * b6 campaign's tuned 0.65 claims it is a third cheaper, and the
   * cost of that showed up in the tail: 9% of settler missions on an
   * 80-turn cell ran past 40 turns, the longest running the whole game
   * (findings/settler-missions.md).
   *
   * Human call 2026-08-02: hold it a little BELOW 1, not at 1. d = 1
   * prices the wrong counterfactual -- by the time the discount is
   * applied the settler EXISTS, its shields and its pop_cost are
   * already spent, so the choice is not "a city now versus a city in T
   * turns" but "a city in T turns versus no city at all", and the
   * alternatives (auto-worker, Join City) are worth far less than a
   * city. Charging the full delay over-favours refusing the walk.
   *
   * The RANGE carries as much of the decision as the default: [0.75,
   * 1.0] in tuning-params.ini keeps the tuner from rediscovering
   * "delay is nearly free" (0.65 is outside it) without keeping it
   * from the honest 1.0. */
  double site_travel_discount;
  double site_travel_crowding;
  /* Weight on a catchment tile's realistic improvement increment
   * (irrigation or mine). Placeholder 1.5 is stock's effective
   * PERFECTION/2 with integer truncation. */
  double site_potential_weight;
  /* What the settler itself costs, charged against the site's worth.
   * Dimensionally awkward -- shields subtracted from weighted tile
   * yield -- which is exactly why it should be tunable rather than 1. */
  double site_settler_cost_weight;
  double site_boat_cost_weight;
  /* Survival probability at or above which a city does NOT switch to
   * war weights in the city manager. Deliberately separate from
   * survival_target: that one arbitrates ~20 defence and operations
   * decisions where being wrong loses a city, while being wrong here
   * loses some food, so the two want different answers -- and sharing
   * one scalar would put the ECON and MIL tuning blocks in a tug of war
   * over it. 0 falls the gate back to stock's urgency/grave_danger
   * presence counts. */
  double cm_war_survival;
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
  /* Contested-continent penalty: non-allied cities on the SITE'S OWN
   * landmass, weighted 1/(1+distance), subtracted from the site's
   * worth. Counterweight to site_claim_bonus, which is the only
   * rival-proximity term today and is a BONUS -- nothing priced the
   * risk of founding against a border. Same-continent only (a rival
   * across water is the boat-shadow problem) and distance-weighted,
   * because a flat shared-continent test discriminates nothing on a
   * map like Europe where one landmass runs Morocco to Siberia. */
  double site_rival_pressure;
  /* How much of the contested-continent penalty is forgiven as the map
   * fills: the penalty becomes site_rival_pressure * MAX(0, 1 -
   * crowding_factor * deep_map_crowding()). 0 disables and leaves the
   * penalty flat.
   *
   * Sharing a continent is a CHOICE on a roomy map and a fact of life
   * on a tight one. Where every player is packed onto one landmass,
   * every candidate site has non-allied cities near it, so the term
   * stops ranking sites and becomes a uniform tax that only decides
   * whether to expand at all -- the failure the neighbourhood bound in
   * deepsite.c was already added to contain, and did not fully. The
   * bound answers "how far away do rivals stop counting"; this answers
   * "is being near a rival even avoidable here". Measured:
   * campaign-a6 raised site_rival_pressure to 35.9 at no cost in its
   * two island cells, where the term contributes exactly zero, and
   * paid for it only in the one crowded cell.
   *
   * Scaled by deep_land_pressure(), not deep_map_crowding(): the
   * crowding prior is pinned at 1.0 for every map at or below its
   * reference density, which includes the crowded cell this term
   * exists to answer. (site_overlap_crowding scaled the same way and
   * for the same reason; W02b deleted it along with the overlap
   * weight it modulated.) */
  double site_rival_crowding;
  /* Trade-route destination policy (findings/trade-routes.md).
   * caravan_discount is the per-turn patience fed to caravan.c's NPV
   * model; stock's 0.95 is kept as the placeholder, but stock also
   * SWITCHES IT OFF entirely (ignore_transit_time) for cross-continent
   * routes, so a 30-turn sea voyage is free while a 15-turn land walk
   * keeps 46%. Deep always counts transit and expresses patience as
   * this one number.
   * caravan_foreign_trade is how far afield to look, as a
   * foreign_trade_limit: 0 national, 1 allied, 2 peaceful,
   * 3+ non-war. Stock re-rolls fc_rand(2) between allied and non-war
   * per unit per call, so half the time the best destination on the
   * board is not a candidate at all. */
  /* Survival probability below which a city on war footing stops
   * planning settlers and workers. Stock zeroes both for EVERY city
   * whenever any war countdown exists anywhere, which sticks and
   * switches expansion off permanently. 0 restores stock exactly. */
  double expansion_war_survival;
  double caravan_discount;
  int caravan_foreign_trade;

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
