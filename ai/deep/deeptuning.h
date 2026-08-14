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

struct player;

/* WHERE THE VALUES COME FROM -- the loading scheme (PERPLAYER,
 * 2026-08-12). Two layers, and the first one is exactly what shipped
 * before this file grew the second.
 *
 * LAYER 1, the base values, unchanged: the compiled defaults below,
 * overlaid by the [tuning] section of $FREECIV_DEEP_TUNING if that
 * variable names a readable file. One file per server process, applied
 * to every deep seat. With FREECIV_DEEP_ARENA unset this is the whole
 * scheme, every seat's pointer is the base struct's address, and the
 * behaviour is byte-identical to the process-global loader it replaced
 * -- which is the gate this work shipped behind, not a hope.
 *
 * LAYER 2, the ARENA, opt-in: $FREECIV_DEEP_ARENA names a MANIFEST ini
 * that gives named seats their own tuning file, so several candidate
 * value sets can be judged inside ONE game (findings/
 * tuning-venues-2026-08-06.md section 6a -- eight candidates at one
 * table is within-game selection, and it triages, it never claims).
 *
 *   [arena]
 *   P1   = "benchmark/tuning/candidate-a.ini"
 *   P2   = "benchmark/tuning/candidate-b.ini"
 *   slot2 = "benchmark/tuning/candidate-c.ini"
 *
 * A seat is looked up FIRST by player name ("arena.<name>"), then by
 * slot number ("arena.slot<n>", n = player_number()). Names are the
 * useful key -- benchmark/run-game.sh creates P1..PN -- and the slot
 * form is the fallback for names an ini key cannot hold. A seat named
 * by neither keeps the base values, so a manifest mentioning two of
 * eight seats is a two-candidate arena with six controls.
 *
 * Each named file is loaded the SAME way the base file is (compiled
 * defaults, then the file's keys), so a candidate ini is an ordinary
 * tuning file and archived inis mean what they always meant. Files are
 * cached by path, so eight seats sharing one file share one struct.
 *
 * The two layers therefore compose at SEAT level and not at key level:
 * a named seat runs its own file from the defaults, NOT the base plus a
 * delta. One semantics for every tuning file this project has ever
 * written is worth more than the convenience of an overlay.
 *
 * The assignment resolves LAZILY, on the seat's first read, because the
 * player_alloc hook runs before server_player_set_name()
 * (server/stdinhand.c create_command_newcomer: server_create_player at
 * :908, the name at :939) -- at alloc time there is no name to key on.
 * By the time anything reads a tunable the seat is named and playing.
 *
 * READING: deep_tuning_get() returns the values of the seat whose
 * decision is being made, via a scope pointer that deepai.c's hook
 * wrappers set from the player the hook serves. AI phases are strictly
 * sequential per player, so that pointer has no concurrent reader by
 * construction. deep_tuning_for(pplayer) is the explicit form for code
 * that holds a player and does not want to trust the ambient scope.
 * deep_tuning_scopeless_reads() counts reads that found NO scope while
 * an arena was configured: it is zero when the guards cover the code
 * that runs, and any other number is a coverage defect to be reported,
 * not an approximation to be lived with. */

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

  /* What defensible ground is worth at a candidate site, as a
   * PERCENTAGE of the site's own value paid per 100% of terrain
   * defence: at 20, a hills site (+100% in civ2civ3) is worth 20%
   * more than the same yields on the flat, and grassland earns
   * nothing. Replaces stock's result_defense_bonus(), which is
   * upstream bug #1 and pays defensible ground LESS (deepsite.c) --
   * the SIGN correction ships unconditionally; this is its size.
   *
   * DEFAULT 0.0, and that number is MEASURED, not assumed. At stock's
   * own DEFENSE_EMPHASIS (20) the corrected term costs score on the
   * cell where defensible ground should matter most: contested, 20
   * paired seeds, term ON vs OFF inside the same binary, score 76.94
   * vs 79.37 (+2.43 for OFF, se 1.87, t +1.30, 13/20), and against
   * the pre-session tip the whole session went from -4.36 (t -2.57)
   * to -1.93 (t -1.10) with only this term removed. It agrees with
   * the human prior recorded 2026-08-05: "defense bonuses for cities
   * basically don't matter long-term" -- a city is defended by what
   * stands in it, and terrain the AI can pick between is worth less
   * than the yields it gives up to stand on it.
   *
   * So the magnitude, not the sign, is what stock got wrong twice
   * over: wrong direction AND too hot. Registered 0-80 for the
   * tuning pile; a pass that finds a small positive optimum would be
   * a real result, and the term is written and waiting for it. */
  double site_defence_weight;

  /* Refuse the maritime premium (stock naval_bonus(), NAVAL_EMPHASIS
   * percent of the site's value for ocean adjacency) to a site whose
   * only water closes on its own continent -- the same geometry the
   * build side's boat_water_gate uses to refuse the keel
   * (get_lake_surrounder, deep_city_water_bridges). GEOMETRY, not a
   * magnitude, so 0/1 like boat_water_gate rather than a weight.
   *
   * DEFAULT 0 (do not gate), and that is a MEASUREMENT, not caution.
   * isolated-150, 20 paired seeds, one binary, this key the only
   * difference: gating scores 187.11 against 198.79 ungated (-11.68,
   * se 8.02, t -1.46, 7/20) and lands 17.25 lifts against 21.55.
   * Direction, not a claim, at twenty seeds -- but it is the only
   * evidence there is, it points the same way as the contested
   * residual, and nothing anywhere measures the gate positive.
   *
   * The likely reason, which the gate's design missed: the premium is
   * not a shipping option, it is COASTAL YIELD. A Harbour feeds ocean
   * tiles whether or not the sea leads anywhere, lakes carry food
   * specials, and a closed sea is still tiles worked. Gating a yield
   * term on navigability prices out real food. The build-side water
   * gate is right for hulls and wrong here, and that asymmetry is the
   * finding. Set to 1 to re-enable; the census reports it either way. */
  int site_naval_gate;

  /* Weight on the EXPECTED WAIT when ranking an overseas candidate:
   * the delivery delay enters the same amortize() as the travel delay,
   * as site_wait_discount * deep_wait_expected(beachhead's water)
   * turns. 1.0 charges the estimated wait at face value; 0.0 is off
   * and is the A/B arm.
   *
   * The defect it addresses is not a magnitude but an absence: the
   * overseas leg ranked every candidate with NO wait term, so a
   * beachhead on a sea whose queue has held settlers for twenty turns
   * scored exactly as one with a hull idling beside it, and the probe
   * that publishes colony_want priced no wait at all
   * (expansion-logistics section 3.1).
   *
   * PLACEHOLDER weight -- flagged in STATUS.md for a tuning pass. The
   * TURNS it multiplies are measured (live wait spells); how heavily a
   * turn of queueing should count against a turn of walking is not. */
  double site_wait_discount;

  /* And what that premium is WORTH, as a percentage of the site's own
   * value: stock's naval_bonus() pays a compile-time NAVAL_EMPHASIS
   * (20) -- a fifth of the whole site score for standing next to
   * water, one of the largest single terms in site scoring, never
   * measured by anyone. PLACEHOLDER 20.0 reproduces stock operation
   * for operation; the knob exists so the magnitude can be tuned
   * where stock had a constant. 0 removes the premium.
   *
   * This half is coastal YIELD: paid on any workable water, dead lake
   * included, because a Harbour feeds ocean tiles whether or not the
   * sea leads anywhere. See site_naval_connect_weight for the other
   * half. */
  double site_naval_weight;

  /* The OTHER half of the maritime premium: additive option value for
   * water that leads off this continent, again as a percentage of the
   * site's own value, paid only where deep_city_water_bridges() says
   * the water goes somewhere.
   *
   * Stock had one number for both halves and no way to separate them.
   * site_naval_gate tested the VETO form of the same distinction --
   * all of the premium or none -- and measured -11.68 (se 8.02,
   * t -1.46, 7/20) on isolated-150, because a veto withholds coastal
   * yield from closed water that genuinely yields. The distinction
   * survives that result; the veto does not. A city on a useful lake
   * IS slightly more useful than one on a dead lake: it can lay a
   * keel that reaches somewhere, launch a colony lift, and reach the
   * intercontinental trade tier.
   *
   * PLACEHOLDER 0.0 = OFF, so the shipped premium stays exactly
   * today's 20% of yield. The design the human sized on 2026-08-05 is
   * a SPLIT of that 20 -- most of it yield, a small additive connect
   * term -- and choosing where to cut it is an A/B on isolated-150,
   * whose refusal census already counts the population the connect
   * term prices. Shipping the split un-measured would move site
   * scoring on every coastal map on the strength of an argument. */
  double site_naval_connect_weight;

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
  /* Plague insurance, in the same tile-sum currency as the credits
   * around it: scale on (expected citizens the building's Health_Pct
   * saves over MORT turns) x (what a citizen earns from the best
   * unworked tile). Active once the city is within one pop of the
   * ruleset's illness threshold, because the insurance is worth
   * having before the size gate the building's other effects wait
   * for. Was "credit per PERCENT POINT of Health_Pct" -- a third
   * currency, and a flat 30 for an Aqueduct in a city whose plague
   * risk was zero (economy audit 3.11). The knob's scale moved with
   * its dimension and it owes the tuning pile a value. */
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
  /* What the war footing is CHARGED for the growth it forgoes. The
   * city manager is run under both candidate postures (war, and
   * whatever the city would otherwise run -- including stock's own
   * default), and the fraction of the food surplus the footing gives
   * up is subtracted from cm_war_survival before the gate compares.
   * A city that arms for free flips on the unchanged threshold; a city
   * that stops growing to arm has to be in proportionally more danger.
   * 0 = off: no counterfactual is solved and the gate flips free, as
   * it did from W09-lite until now. Bounded by cm_war_survival itself,
   * since a bar below 0 can never be cleared. */
  double cm_war_growth_charge;
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
  /* Whether deep asks the expansion question at all. 1 (default, and
   * what ships) runs the two regimes above: deep's survival gate when
   * expansion_war_survival > 0, stock's dai_on_war_footing() when it
   * is <= 0. 0 means NO GATE -- deep_expansion_blocked() is FALSE on
   * every city-turn regardless of expansion_war_survival, so war never
   * stops a city planning settlers and workers.
   *
   * A separate switch rather than a third sentinel on
   * expansion_war_survival, because that double's two regimes are
   * already the meaning of every archived tuning file (era-a-pv1.ini
   * omits it; era-b-noexpgate.ini pins it to 0.0) and a new sentinel
   * would silently re-read them. This knob is the never-block ARM of
   * that comparison -- the corner the ERA probe could not ask
   * (findings/era-probe-evidence-2026-08-12.md S4) -- and like
   * stance_forced it is pinned by hand per experiment and given a
   * degenerate range so the tuner never spends a sample on it. */
  int expansion_gate_enabled;
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
  /* Weight the garrison floor carries while each threat class the
   * danger assessment cannot see is LIVE (deep_floor_class_facts()).
   * The floor takes the MAX over live classes, not the sum: it buys a
   * token garrison, and a second blind spot does not buy a second
   * token -- it decides WHETHER to insure and how firmly, not how
   * many. With no live class the floor is zero and the settler or the
   * building wins the auction (human direction, 2026-08-05).
   *
   * All PLACEHOLDERS, all untuned. At 1.0 each, a city with any class
   * live gets exactly the floor deep applied unconditionally before
   * this change, so the redesign is a pure gate at the shipping
   * values. Tuning invariant to preserve: floor_border_weight should
   * not fall below floor_sea_weight, or an overseas-only rival would
   * outweigh a land neighbour for a border city. */
  double floor_barb_weight;
  double floor_sea_weight;
  double floor_air_weight;
  double floor_border_weight;
  /* Geometry of the sea-landing class, per city. A city whose water
   * leads nowhere (a closed lake, by get_lake_surrounder) or that has
   * no water at all is INTERIOR and carries the class at
   * floor_interior_weight; a city on water that leads somewhere is
   * exterior and carries it in full. floor_marines_mult moves both
   * weights DOWNWARD from full and never upward past it: before
   * attack-from-hull tech exists anywhere the exterior weight is
   * 1/mult (a landing must disembark first, so a port gets a turn of
   * warning and the class is mild), and once it exists the exterior
   * weight reaches full while the interior weight is divided by mult.
   * The split sharpens either way, and the ceiling stays at
   * floor_sea_weight so no live class can buy MORE floor than the
   * ungated floor did. */
  double floor_interior_weight;
  double floor_marines_mult;
  /* Base danger for the barbarian era, multiplied by the SHARED
   * barbarian scaling (deepdefence.c, deep_barb_empire_scale()) that
   * mirrors the server's own uprising formula: the pre-onset ramp,
   * the (barbarianrate - 1) frequency scale, the willingness
   * probability MIN(1, cities * (rate - 1) / 30), the government's
   * civil-war chance and the horde ladder. Was a flat step at
   * onsetbarbarian, which is what the derby-2 uprising storm walked
   * through. */
  int barbarian_fear;
  /* How many turns BEFORE game.server.onsetbarbarian the terror
   * starts ramping in, linearly, from zero to full. The garrison has
   * to exist ON the onset turn and a defender takes ~10 turns to
   * build, so the lead is the knob; the shape is not. 0 restores the
   * step function that shipped. */
  int barb_anticipation_turns;
  /* Empire size (in cities) at which the shared barbarian scaling is
   * exactly 1.0, under BARBS_NORMAL. Pure normalisation: it exists so
   * barbarian_fear and floor_barb_weight keep the magnitude they were
   * set at while the SHAPE changes around them. Raising it makes the
   * whole class milder, lowering it makes it fiercer, and it must be
   * tuned together with the two bases it normalises. */
  int barb_size_ref_cities;
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
  /* PLACEHOLDER, untuned. The FIRST defender in an empty city, priced
   * instead of pinned: want = P(walk-in) x ai_city.worth x gain, at
   * NORMAL tier and through the stance fear gate, where P(walk-in) is
   * the union of the survival model's reachability verdict and the
   * barbarian uprising odds (deepdefence.c, deep_birth_guard_want()).
   * 0 restores the inherited rule exactly -- a flat danger of 100 at
   * DWT_EMERGENCY whenever urgency is 0, which is threat-blind,
   * stance-proof and unpriced against the settler it displaces. The
   * urgency > 0 branch keeps the emergency tier at every setting: a
   * warband two tiles out is a real emergency. */
  double birth_guard_gain;
  /* PLACEHOLDER, untuned. The DEFENSIVE tech wall, sibling of H9's
   * capacity routing: a city whose survival is below target and whose
   * best BUILDABLE land defender is beaten by one the ruleset offers
   * at reachable tech routes survival_fear_value x deficit x weight
   * into tech_want[] for the gating advance (deepdefence.c,
   * deep_defensive_techwall()). A pure ratio -- how many turns of the
   * fear a turn of research is worth -- not a magnitude in a new unit.
   * 0 = off, and no tech_want[] entry is written. Deliberately not
   * stance-gated: tech DIRECTION is W34 A.3's gate, and two gates on
   * one quantity is the failure A.3 names. */
  double defensive_techwall_weight;
  /* PLACEHOLDER, untuned. The never-launches dampers (CONSEQ item 5,
   * derby N3): no loop existed from "the operation never launches"
   * back to "stop building its passengers", nor from "the mission
   * never pays" back to "stop building agents". Both take the explorer
   * divisor's saturation shape, 1/(1 + gain x saturation), and both
   * are 1.0 exactly at 0.
   *
   * attacker_hold_damper's saturation is a RATE -- assault operations
   * formed that never reached MARCH, over all formed -- because a
   * cumulative count would grow without bound and turn the damper into
   * a clock. Exempt on a target that threatens us, gate 5's own test:
   * starving reconquest of what besieges us is the wrong loop closed.
   *
   * diplomat_pump_damper's saturation is a COUNT, of diplomat-class
   * units walking plus queued, because that is the prize-per-agent
   * question the explorer fix answers and it needs no success-rate
   * estimate -- there is no measured outcome signal for a diplomat
   * mission and one was not invented. */
  double attacker_hold_damper;
  double diplomat_pump_damper;

  /* WONDER RACE (WRACE, 2026-08-07). All PLACEHOLDERs.
   *
   * wrace_margin_turns is the SCALE of the P(win) ramp, not a
   * threshold: P = 0.5 + 0.5 x (rival ETA - our ETA) / margin,
   * clipped, so a dead heat is 0.5 and losing by the whole margin is
   * 0 -- which IS the go/no-go bar. It is an int in turns because
   * both inputs are turn counts.
   *
   * The three composition gains each move the want from "unchanged"
   * to "fully the factor": want x (1 - gain x (1 - factor)). At 0 the
   * factor is computed, logged and not applied, which is what makes
   * the census arm and the acting arm the same binary.
   * - wrace_pwin_gain      the race term: a contested wonder we would
   *                        lose ranks below an uncontested lesser one.
   * - wrace_eta_gain       the completion-ETA discount: a 40-turn
   *                        build must not be paid from today's budget
   *                        at face value.
   * - wrace_obsolete_gain  the obsolescence truncation: a wonder five
   *                        turns from the advance that voids it is
   *                        currently priced as eternal.
   * wrace_urgency_gain multiplies the help-wonder caravan want as a
   * race we LEAD gets close (R2b is the delivery arm). 1.0 + gain at
   * a dead heat, 1.0 once we lead by the whole margin or trail. */
  int wrace_margin_turns;
  double wrace_pwin_gain;
  double wrace_eta_gain;
  double wrace_obsolete_gain;
  double wrace_urgency_gain;

  /* EXPIRY-AWARE AMORTIZATION (CONSEQ item 4, human 2026-08-05: "some
   * units and buildings expire too"). All PLACEHOLDERs, all 0 = off.
   *
   * expiry_building_gain truncates a building credit's horizon at the
   * expected arrival of the advance that voids it, when OUR OWN queue
   * is the one bringing it (deephorizon.c). The credit sums a
   * per-turn value over MORT turns and assumes the building lives for
   * all of them; the server auto-sells an obsolete improvement and
   * its effects lapse before that. want x (1 - gain x (1 - surviving
   * share)).
   *
   * expiry_unit_gain defers a unit build whose SUCCESSOR arrives
   * inside its own build time (deepdefence.c). Never applied to an
   * emergency-tier want or a city carrying danger or urgency: tier
   * discipline, and a threatened city buys the body it can have now.
   * expiry_unit_gap is the hysteresis in turns -- the successor must
   * beat the build by this margin before the queue churns, the M3
   * pattern -- and it is a gap and not a ratio because both sides are
   * turn counts. */
  double expiry_building_gain;
  double expiry_unit_gain;
  int expiry_unit_gap;

  /* THE LAST-DITCH DEFENCE LADDER (human, 2026-08-07). Both
   * PLACEHOLDERs, both 0 = off = byte-identical.
   *
   * sell_defend_gain is rung 1, SELL TO DEFEND: when a city wants a
   * defender bought and the treasury cannot cover the rush, sell the
   * highest-value sellable building that is not itself defending the
   * city and put the gold toward the buy. A DEFENCE move, not a
   * concession, so the trigger is deliberately looser than the
   * abandonment slice's -- the misfire cost is one building against a
   * saved city. The gain is the BAR and not a scale: sell only while
   * the building's sale price is at most gain x the rush cost, so at
   * 1.0 the building may be worth up to what the defender costs.
   *
   * late_buy_gate is rung 3, THE FINAL DEFENCE BUY: at the end of
   * deep's phase, spend gold the turn ADDED since the main buy pass,
   * restricted to the triage population (urgency, grave danger, or
   * below-target survival against a live wave). Never the general want
   * ladder. */
  double sell_defend_gain;
  int late_buy_gate;

  /* Rung 2a, CIVILIAN EVACUATION (human, 2026-08-07). 0/1 off-switch,
   * PLACEHOLDER. Split from the defensive write-off because the two
   * have different bars: a civilian contributes exactly zero to the
   * defence (deep_collect_defenders() skips UTYF_CIVILIAN) and dies
   * with the city, so being wrong costs a few unit-turns of walking
   * and this half keeps the LOOSE compound gate. The DENIAL SALE
   * (rung 2b) does not, and the census is why -- see
   * findings/conseq-wrace-2026-08-07.md: even the exhausted slice
   * holds two times in three. */
  int evacuate_gate;
  /* Land-race coupling: fresh founder wants scale by
   * (1 + weight * map crowding); crowded maps race, empty ones
   * don't. 0 disables. */
  double settler_rush_weight;
  /* Site bonus (tile-sum currency, scaled by crowding) for local
   * isthmus tiles -- a city in a gate is worth a garrison army.
   * 0 disables. */
  double site_choke_bonus;
  /* What a defensive BASE is worth to a worker, in the same currency
   * the advisor values a tile's output in. The advisor's own answer
   * is structurally zero -- adv_calc_extra() sums food/shield/trade
   * and a base changes none of them -- so this gain is the entire
   * want. Multiplied by the survival a defender standing on the tile
   * gains from the base (0..1, priced through the sequential-
   * attrition substrate) and by how much the ground matters (0..1,
   * chokepoint or G1's border shape). PLACEHOLDER, untuned; the
   * product lands in the same band as site_choke_bonus at typical
   * survival deltas, which is where a good tile improvement sits.
   * 0.0 disables the want entirely and restores byte identity. */
  double base_fort_gain;
  /* Fortify a military unit already standing on ground the base want
   * values -- a defensive base, or a chokepoint -- instead of letting
   * the manage loop wander it off. Runs after every claiming pass, so
   * it can never pre-empt a kill or a defence. 0 disables and restores
   * byte identity. PLACEHOLDER, untuned. */
  int fortify_ground;
  /* Value of denying an enemy one point of tile output by pillaging
   * the extra that produces it. A unit already standing on an
   * at-war-owned tile pillages the best extra when gain x denied
   * output reaches 1. 0 disables the whole pass and restores byte
   * identity. PLACEHOLDER, untuned. */
  double pillage_denial_gain;
  /* Convoy discipline: a transport with cargo holds in port until it
   * carries convoy_min_load units (<=1 disables) while boarders are
   * inbound within convoy_wait_radius tiles, and any cargo-carrying
   * transport recruits an idle warship bodyguard from its tile. */
  int convoy_min_load;
  int convoy_wait_radius;
  /* The colony lift (findings/settler-missions.md section 9). A lift
   * operation forms when a waiting passenger has an unclaimed boat
   * within lift_form_radius tiles, and aborts if boat and manifest
   * have not met within lift_gather_turns, or the crossing has not
   * landed within lift_cross_turns. lift_form_radius <= 0 disables the
   * lift entirely and leaves stock's two-sided ferry protocol alone.
   * ALL PLACEHOLDERS -- flagged in STATUS.md for a tuning pass. */
  int lift_form_radius;
  int lift_gather_turns;
  int lift_cross_turns;
  /* How many of the DEEP_LIFT_OPS_MAX lift-only slots a lift may use.
   * 0 is exactly the old behaviour -- lifts take whatever the army
   * left free and queue when the array is full -- so this doubles as
   * the A/B switch for the change on one pinned binary. Measured
   * ceiling it exists to relieve: the array was full with an
   * unclaimed overseas passenger waiting 11.9 times per game
   * (findings/settler-missions.md section 10).
   * PLACEHOLDER -- flagged in STATUS.md for a tuning pass. */
  int lift_op_slots;
  /* Largest manifest a lift will gather, hitchers included. 1 is
   * exactly the old single-passenger lift, so it is the A/B switch.
   * The hull caps it independently and lower: civ2civ3's Trireme
   * carries 2 where its Transport carries 8. Not "fill the boat",
   * because concentration is a real cost -- one sunk hull loses the
   * whole manifest (human, 2026-08-03).
   * PLACEHOLDER -- flagged in STATUS.md for a tuning pass. */
  int lift_manifest_max;
  /* How long the boat will hold the quay for a straggler, as a
   * fraction of the crossing it is about to make (human doctrine
   * 2026-08-03: patience should scale with the trip -- two turns is
   * cheap against a fifteen-turn crossing and absurd against a
   * two-turn hop). 0.0 never waits, which is the old contract and the
   * A/B switch. Bounded so it never spends the gather window's last
   * turn: sailing beats timing out with the core passenger aboard.
   * PLACEHOLDER -- flagged in STATUS.md for a tuning pass. */
  double lift_hitch_wait_ratio;
  /* Damping on the colony boat want per ALREADY-IDLE hull:
   * want /= (1 + boat_idle_damping * idle_boats). colony_want is a
   * pure site-quality number and nothing between it and the shipyard
   * asked whether the fleet could already serve it -- measured, 73%
   * of the fleet sits idle and the carrying share FALLS as the fleet
   * grows (STATUS, "Too Many Boats"). A good site is not worth less
   * because a boat is idle, but the marginal HULL is, and a hull is
   * what this want buys. 0.0 restores the undamped signal exactly and
   * is the A/B switch.
   * PLACEHOLDER -- flagged in STATUS.md for a tuning pass. */
  double boat_idle_damping;
  /* Refuse a ferry outright when every sea the city touches closes on
   * the city's own continent, so nothing it carries could ever land
   * anywhere else. Pure geometry: 84% of idle hull-turns have no
   * waiting passenger because demand is BURSTY, so a demand test would
   * suppress the hulls that serve the next burst -- but a sea with one
   * surrounding continent does not become useful at any demand, ever.
   * 0 restores the ungated behaviour exactly and is the A/B switch.
   * Not a magnitude and NOT a candidate for the tuner: a value between
   * on and off has no meaning. */
  int boat_water_gate;
  /* Ask the useful-water test where a ferry bid is FORMED instead of
   * vetoing the bid that won. Both producers withdraw at formation:
   * the domestic founder's boat (a negative founder_want, zeroed for
   * that one advisor call) and deep's own colony lift want. There is
   * then no refused winner, so no runner-up to reconstruct and no
   * fallback ladder at want = 1 -- the city simply chooses its best
   * real candidate. 0 restores the post-hoc veto and is the A/B
   * switch. Not a magnitude and NOT a candidate for the tuner. */
  int boat_gate_at_bid;
  /* How far the boat will DETOUR to land a passenger at its own site
   * rather than making it walk, in tiles -- "two boat moves" at the
   * human's sizing (2026-08-04). Passengers adjacent to their target
   * disembark here regardless (that is the game mechanic, not a
   * tunable); anyone further gets another stop if it is within this
   * budget, and walks if it is not.
   *
   * Sized against measurement: a joiner released at the core's
   * beachhead lands a median 7 tiles from its own target, half of them
   * 6 or more. 0 collapses to a single drop for the whole manifest.
   * PLACEHOLDER -- flagged in STATUS.md for a tuning pass. */
  int lift_drop_radius;
  /* Floor, in whole turns, under the hitch wait budget when the ratio
   * admits any wait at all. The ratio scales patience with the
   * crossing; this says the smallest useful amount of patience is not
   * necessarily one turn (human, 2026-08-04). Still clipped to the
   * gather window's second-to-last turn, so it can never trade a
   * loaded boat for a timeout.
   * PLACEHOLDER -- flagged in STATUS.md for a tuning pass. */
  int lift_hitch_wait_min;
  /* Re-validate a lift passenger's mission during the MUSTER as well
   * as the crossing (0/1, not a magnitude). The op-hold sits above
   * the settler path's own city_can_be_built_here() check, so a held
   * passenger never re-examines its target -- deep_lift_revalidate()
   * has asked for it on every MARCH turn since 0d6440e194, and the
   * gather, which is just as held and lasts lift_gather_turns, never
   * did. 1 drops a member whose target died before the hull sails,
   * re-aims if that target was the beachhead, and aborts a lift with
   * nobody left to deliver. At 0 the muster only COUNTS them
   * (deep-lift-invalid phase=muster), which is what shipped first. */
  int lift_muster_release;
  /* Walk a founder that needs a hull to a coast a hull could actually
   * fetch it from (0/1, not a magnitude). The ferry path's own test is
   * is_terrain_class_near_tile(TC_OCEAN) -- "am I beside water" --
   * under which a lake, a dead-end sea and the world ocean are one
   * thing; a founder that passes it asks for a boat that can never
   * come and, because the drop-boatless clause spares a unit holding
   * FERRY_WANTED, stands there for the rest of the game. EXPANSION
   * slice 2 counted the population: 60 of 67 positive lift gaps were
   * founders on water that closes on its own continent, with 4-16
   * idle berths on the bridging water beside them.
   * At 1 such a founder is sent to the nearest tile that shares a
   * body of water with its target (deep_water_serves_crossing), by
   * its own risk pathfinder, and STILL asks for a boat on arrival, so
   * the mission is never dropped for lack of a request. At 0 nothing
   * moves and the geometry is only counted (deep-berth).
   * Not a magnitude and not a candidate for the tuner: a value between
   * on and off has no meaning. */
  int founder_berth_route;
  /* Which point of the live wait-spell distribution deep_wait_expected()
   * reports, as a percentile: 100 is the longest spell that water is
   * holding, 50 the median.
   *
   * Not a magnitude and not free: 80 is where the measured mass sits.
   * A fifth of spells hold three fifths of all waiting
   * (findings/commission-sizing.md), so the mean describes nobody --
   * most crossings are quick and the slow ones are the ones that cost
   * a colony. This selects the boundary of that fifth. It is a knob
   * rather than a literal because the SHAPE of the estimator is a
   * choice even when the number behind it is measured. */
  int lift_wait_tail_pct;
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
  /* Workers cannot cross water at all today: the land pf map discards
   * every tile of an overseas city's radius as unreachable, so a colony
   * is improved only by founders that happen to land there
   * (findings/idle-workers.md section 9). When the idle fallback finds
   * nothing walkable it may instead take a candidate across the water,
   * ranked by completion time like every other candidate -- inflated by
   * this factor so the trip stays BACKUP behaviour rather than
   * competing with work at home: turns *= (1 + discount). The same
   * 1 + d shape as worker_danger_discount, inverted because lower turns
   * win. 0 makes a boat trip cost exactly its real turns. */
  double worker_cross_continent_discount;
  /* Travel discount on the PRIMARY worker task auction. Among tiles
   * not yet worked, worker_evaluate_improvements() compares candidates
   * on the resulting tile value alone -- the walk is folded into
   * amortize() only once, after the loops, on the champion, so two
   * candidates worth the same to the city rank the same whether they
   * are one tile away or fifteen. Here the improvement (value minus
   * the tile's value without it) is discounted by
   * amortize(gain, discount * arrival), the same shape the site search
   * uses for its own travel term (site_travel_discount). 0 disables
   * and restores stock ranking exactly. */
  double worker_travel_discount;
  /* Trunk roads (W12). A road candidate on a tile the empire's
   * minimum spanning tree runs through carries the LINK's worth on top
   * of its own: the movement points the finished link saves on one
   * traversal, discounted by amortize() over the corridor tiles still
   * missing. This gain converts those movement points into the
   * advisor's tile currency. 0 disables the whole term, plan included.
   */
  double trunk_road_gain;

  /* Ruleset-upkeep awareness (2026-07-28 civ2civ3 analysis: deep
   * bankrupted itself head-to-head, gold ratio 0.13-0.55 vs stock).
   * upkeep_gold_weight charges a building's gold upkeep against its
   * horizon credit, over the MORT horizon the credit itself is summed
   * over (charging ONE turn of gold against a MORT-horizon value made
   * the ruleset's bill ~24x too cheap; the default and the registered
   * range were divided by MORT when that was corrected, so the landed
   * charge did not move and the knob owes a tuning pass); upkeep_unit_weight discounts the injected fear
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
   * unclaimed hut and per unknown tile, spread over the fleet.
   * Zero-gated when the map was generated without huts. */
  double explore_hut_value;
  double explore_discovery_value;
  /* How much of the visible prize one explorer already fielded or on
   * order is expected to collect, in HUTS (so it rescales with
   * explore_hut_value rather than fighting it). Subtracted from the
   * prize before the fleet divides it, which is what lets the want
   * reach exactly zero -- the old average-per-explorer form was a
   * hyperbola and equilibrated at fleets of 20-51 on island maps.
   * 0 restores that behaviour. */
  double explore_explorer_claim;
  /* Disposal, keyed to the same quantity. Slack in EXPLORERS between
   * the point where the prize stops paying for another one and the
   * point where standing ones are salvaged, so build and disband
   * cannot alternate on one turn's arithmetic; and the number of
   * explorers never salvaged, because island hut supply reopens as
   * new landmasses are reached and W04's recon value lands on this
   * same fleet. */
  double explore_salvage_gap;
  int explore_retain_floor;
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
  /* A city carrying a founder want it cannot FUND: domestic_advisor_
   * choose_build() (daidomestic.c:561) makes no founder bid at all
   * unless the city's food surplus covers the founder's food upkeep,
   * and 2 lategame-80 liveness games measured 13-31% of nonzero
   * founder wants vetoed there, 83% of them at surplus exactly 0
   * against an upkeep of exactly 1. 82% of the vetoed city-turns had
   * NO deep cm_parameter at all (deep-founderbid cm=0), i.e. deep's
   * posture machine declined to govern the food its own want needed.
   *
   * Above 0 this adds a last arm to that machine -- fires only where
   * DEEP_CM_NO_POSTURE would have handed the city back to the stock
   * default -- which takes the grow posture and floors the food
   * surplus at cm_founder_food x the founder's food upkeep. 0 is the
   * off switch and the shipped default: the coupling is measured, the
   * fix is not, and it needs an A/B before it can be worth anything.
   * 1.0 asks for exactly the upkeep the veto tests. PLACEHOLDER. */
  double cm_founder_food;
  /* Exponent on catchment trade-richness (workable trade / workable
   * shields) scaling every CM state's trade factor: the map sets the
   * trade appetite. 0 restores plain constants; 1 is proportional. */
  double cm_trade_quality_slope;
  /* Production-vs-growth arbitration on the grow-hard entry. Grow-hard
   * is a pure SIZE threshold (cm_grow_below), so a big city with a rich
   * unworked catchment and nothing worth building cannot enter it. This
   * blend admits a city whose marginal CITIZEN
   * (deep_unworked_tile_value) beats its marginal BUILD (the top of
   * this city's building_want[]), scaled by the blend.
   *
   * Both sides are output x priority (infracache.h), so the blend is a
   * genuine trade-off weight and not a unit conversion -- this test
   * does NOT depend on W01's unbuilt `want_t`. PLACEHOLDER all the
   * same; 0 = off = the size threshold alone. The threshold stays as a floor
   * either way, so this can only add cities to grow-hard, never remove
   * one, and the garrison-coverage conjunct still applies to both. */
  double cm_grow_price_blend;
  /* Gains that let empire solvency choose between a taxman and a
   * scientist (DARKFX item 4). cm.c searches per specialist type but
   * every AI pins factor[O_GOLD] == factor[O_SCIENCE] == 2, so the two
   * are indistinguishable to the solver and the pick falls out of
   * lattice order. The gains scale a shortfall fraction in [0, 1]:
   * gold, the treasury against dai_gold_reserve()'s buy float;
   * science, the ruleset tech-upkeep bill against last turn's actual
   * bulbs (deeprates.c's own feasibility test). Both 0 = off = the
   * pinned 2/2. NOTE these are the CM's output factors, so they weight
   * trade-derived gold and science as well as the specialists -- which
   * is what the design asked for, and is broader than the name. */
  double cm_specialist_gold_gain;
  double cm_specialist_sci_gain;

  /* Sequential-attrition survival model (combat workstream): a city's
   * garrison must survive the observed attacker wave with probability
   * survival_target, computed by chained duels (deepodds.c) over
   * at-war units reachable within survival_horizon turns. When the
   * odds fall short, survival_fear_value scales the deficit into the
   * danger field. target <= 0 disables every survival consumer. */
  double survival_target;
  double survival_fear_value;
  int survival_horizon;
  /* HONEST LAND REACH (ERA 3c,
   * findings/era-probe-evidence-2026-08-12.md section 3).
   *
   * The wave admits an attacker when straight-line distance over
   * move rate fits inside survival_horizon -- no terrain, no ZOC,
   * no pathfinder. The 3c instrument priced that estimate against
   * the pathfinder's on 69 336 wave evaluations (tech50war-200):
   * the furthest admitted attacker is unreachable outright in 19.1%
   * and slower than assumed in another 25.6%, and the pathfinder is
   * NEVER faster. So ~45% of survival verdicts are shaped by an
   * attacker that cannot arrive as assumed -- inflated n_att,
   * depressed survival, and (the 3a autopsy's suspect) blades pinned
   * at DEFEND_HOME by fear of units that were never coming.
   *
   * 1 requires the pathfinder to confirm every land-arm admission:
   * same attack parameter find_something_to_kill() uses, same
   * horizon bound the straight line was held to, memoised one map
   * per attacker per phase (deepcombat.c's reach memo) so the
   * hottest routine pays one shared Dijkstra per attacker instead
   * of one per evaluation. A mechanism switch, not a magnitude --
   * every quantity inside is already computed.
   *
   * 0 = the straight-line test alone, exactly as before. */
  int survival_pf_reach;
  /* THE GARRISON-PIN DISCOUNT (NETTHREAT item 2, the flat slice).
   *
   * The pin probe (deepcombat.c, `deep-pin`; merged ee61b90394)
   * watched every enemy unit that qualified for a survival wave while
   * garrisoning one of its OWNER's cities: over 18,538 closed windows
   * on tech50war-200, 68.7% never left their home tile inside the
   * horizon they were feared over, and only 3.22% ever reached a tile
   * from which they could hit one of our cities -- on an arrival
   * proxy that can only OVERSTATE reach.
   *
   * When > 0, an enemy unit the pin tracker currently classifies as
   * home-pinned (live tag, never yet seen off its home tile)
   * contributes (1 - threat_pin_discount) of its normal weight to the
   * wave, as a presence probability in the duel substrate
   * (deep_stack_survival_wfull) -- the exact expectation over it
   * showing up or not, not a rating scale. FLAT: one factor
   * regardless of posture, distance or war state; the
   * posture-conditional variant is a later slice, if this one moves
   * anything.
   *
   * The measured first cut is the never-left share, 0.687: a pinned
   * unit's weight becomes 0.313 = P(it ever sorties at all). NOT
   * 1 - 0.0322: the 28.1% that sorties but never arrives is
   * survival_pf_reach's population, and pricing it here too would
   * spend the same evidence twice.
   *
   * 0 = every attacker weighs its full self, byte for byte as
   * before. */
  double threat_pin_discount;
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
  /* Cost of victory (combat E2, military audit G1): the expected
   * attacker losses of the priced assault, in shields at
   * SHIELD_WEIGHTING, are subtracted from the want before it is
   * amortized. 1.0 charges the loss at exactly the rate the rest of
   * the codebase values a shield; 0 restores the pre-E2 want, which
   * priced an attack as if winning it were free. */
  double attack_loss_gain;
  /* What a conquest is worth, as a multiple of what the city is worth
   * to us (human call 2026-08-06): taking it gains us its amortized
   * value AND denies that value to the opponent, and every conquest
   * want in this project has only ever counted the first half. The
   * human's prior band is ~1.9-2.2. Applied ONCE at each of the two
   * conquest seats -- deep_assault_want's build side and the
   * operation's net-value gate -- never compounded between them.
   * 1.0 restores the pre-rider behaviour.
   *
   * NOT applied to the DEFENSIVE side: keeping our own city is worth
   * about 2x by the identical argument, and multiplying only conquest
   * tilts the offence/defence budget. Parked for the human (STATUS). */
  double conquest_worth_mult;
  /* BALANCE -- the OTHER half of what a conquest takes, and the one
   * nothing in this tree has ever priced: the culture stock.
   *
   * `pcity->history` is a per-city accumulator that transfer_city()
   * does not touch (server/citytools.c changes `pcity->owner` on the
   * same struct), player_culture() sums city_culture() over the
   * cities a player owns, and get_civ_score() pays culture at 1/50th
   * of a point. So the stock is a score asset that changes hands
   * WHOLE at the moment of capture -- the Europe derby's Carthago
   * inherited Gdansk's 2445 and grew it to 6408 (derby N2) -- while
   * conquest_worth_mult prices only the city's ongoing output.
   *
   * The accumulation is LINEAR, not compounding:
   * `history_interest_pml = 0` in both civ2civ3 and classic
   * (findings/darkfx-2026-08-06.md §4), so the stock earns nothing on
   * itself and the old annuity framing is dead. That does not change
   * what the transfer is worth -- a stock is a stock -- it changes
   * only what the loser's counterfactual would have been.
   *
   * The exchange rate is the score's own and is not invented:
   * history/50 score points, each priced at what deep already pays
   * for the citizen that scores exactly one point, which is the
   * identical chain horizon_culture_weight uses for the FLOW. 1.0
   * therefore means "the transferred stock is worth what its score is
   * worth", and it is an upper bound for the same reason the flow's
   * is: the citizen works a tile too.
   *
   * Applied inside deep_conquest_worth(), so all three conquest seats
   * see it at once and none can see it twice. 0.0 = off =
   * byte-identical. PLACEHOLDER, and note the venue: classic has no
   * History effect of any kind, so this is civ2civ3-only by
   * construction. */
  double conquest_culture_gain;
  /* The same argument pointed the other way (human call 2026-08-06):
   * keeping our own city is worth what we keep AND what we deny the
   * taker, so the defence-side stakes read carries its own multiple.
   * Default 2.0 -- symmetric with conquest_worth_mult, which is what
   * makes the offence/defence question a tunable TILT rather than a
   * decision taken in code.
   *
   * READ THE RATIO, NOT THE LEVELS. conquest/defence > 1 is a
   * conquest-hungry player, < 1 a hearth-guarding one, and 1:1 is
   * exactly the pre-rider behaviour at every level because the one
   * comparison that reads both sides (deep_op_poach_defender) is
   * invariant under a common factor.
   *
   * Applied at the defence counter-claim's stakes read only.
   * Deliberately NOT applied to the emergency triage comparator,
   * which sorts our OWN cities and is invariant under a common
   * factor by construction (and is lexicographic post-D1 besides),
   * nor to target SCORING, which is an argmax a positive scalar
   * cannot move. */
  double defence_worth_mult;
  /* The pool a marginal attacker is priced against reaches only as
   * far as the marginal attacker itself does: a free blade that
   * cannot be at the target by the same arrival turn is not part of
   * the wave the build is joining (military audit C4, second half).
   * 0 restores the whole-continent pool. Geometry, not a magnitude,
   * so 0/1 like boat_water_gate. */
  int assault_pool_reach;
  /* Kill sortie (E3): a garrison blade attacks an adjacent besieger
   * when the duel odds clear this floor AND the city still clears
   * survival_target without it. <= 0 disables. */
  double sortie_win_floor;
  /* THE KILLSTACK PRIZE (M4b item 1). The sortie gate above prices the
   * RISK of the sortie twice over and the TARGET not at all, which is
   * the one asymmetry op pricing does not have -- an operation values
   * the conquest it buys. Under killstack a won duel outside a city
   * annihilates the whole tile (is_stack_vulnerable(),
   * common/combat.c:996, which is FALSE inside a city and on
   * EF_NO_STACK_DEATH terrain), so the prize is the SUMMED shield
   * value of the stack and not one kill. With the gain positive the
   * pass selects on
   *
   *     p x prize x sortie_prize_gain - (1 - p) x our own build cost
   *
   * instead of on p alone, and requires that score to be positive on
   * top of the two probability gates -- which stay exactly as they
   * are. 0 = off, restoring best-chance selection with no value term
   * at all. */
  double sortie_prize_gain;
  /* THE DETERRENCE DIVIDEND (M4b item 2; human 2026-08-06, "we aren't
   * valuing defensive units highly enough... a musket behind walls is
   * pretty dang strong").
   *
   * A defender's priced value today is its contribution to WAVE
   * SURVIVAL and nothing else, so a blade that loses is worth zero --
   * which is false. It is worth the attacker shields it destroys on
   * the way down, and CONSEQ's fight-vs-deny design prices exactly
   * that quantity at the write-off seam ("what resisting COSTS the
   * taker", findings/conseq-wrace §2b). Same term, consumed here.
   *
   * Priced as the MARGINAL attrition this candidate defender adds --
   * deep_city_defence_read() with and without it, in shields -- times
   * the observed attack pressure on the city (deep_garrison_menace(),
   * the same 0..1 read the garrison floor is sized by), times this
   * gain. It does NOT touch defence_worth_mult, which prices the CITY;
   * this prices the ATTRITION, and the two are added, not multiplied.
   * 0 = off. */
  double deterrence_gain;
  /* THE ATTACKER'S GARRISON CREDIT (NETTHREAT item 3, promoted to lead
   * by the human 2026-08-12 alongside the Gunpowder doctrine).
   *
   * At the build seam an attacker and a defender compete for one
   * city's shields as if they were orthogonal goods: the defender want
   * carries survival plus the deterrence dividend, and the attacker
   * want (deep_assault_want) carries take-odds and nothing else. But a
   * home-based attacker PHYSICALLY JOINS the stack whenever it is
   * standing at home -- deep_city_survival_core() already counts it,
   * and the sortie pass already spends it -- so its defensive worth is
   * not zero, it is merely intermittent.
   *
   * Priced as exactly the dividend a defender of the same type would
   * earn in this city (the marginal attacker shields it destroys, from
   * deep_city_defence_read(), times deep_garrison_menace()), WEIGHTED
   * by the fraction of unit-turns this player's attack-rated units
   * have actually spent standing in own cities -- an OBSERVED cadence
   * (deep_attacker_home_share()), never an assumed one.
   *
   * That weight is also the anti-double-spend rule: the offensive half
   * of the want prices the unit as marching, this half prices it as
   * home, and home_share < 1 is what stops one body being paid for
   * being in two places. See the bookkeeping note at the credit's own
   * site in deepdefence.c. 0 = off. */
  double attacker_garrison_gain;
  /* HOLES GET COVERED (NETTHREAT item 4, human 2026-08-12). When a
   * blade sorties or is drafted, its removal is priced as the LOCAL
   * hole -- surv(city) minus surv(city without it) -- as if the rest
   * of the lattice were nailed down. It is not: a neighbour with a
   * genuinely sparable garrison can backfill, and the true cost is
   * the post-rearrangement marginal loss, which is smaller.
   *
   * At 1 the removal is priced AFTER the cheapest single covering
   * move: the empire-wide loss (the hole that remains here, plus what
   * the lender loses) minimised over one-hop neighbours that can
   * genuinely spare a blade. One step only, and it reuses
   * deep_city_sparable_blade() and deep_relay_execute() -- the
   * rotation/rehoming machinery -- rather than adding an optimizer.
   *
   * Geometry-with-a-side-effect rather than a magnitude, so 0/1 like
   * assault_pool_reach and op_draft_marginal. 0 = the local hole
   * exactly, which is the behaviour every consumer shipped with. */
  int cover_removal;
  /* UNIT LIFETIME UPKEEP AT THE BUILD SEAM (M4b item 5, amortization
   * audit 1b). A building's gold upkeep is charged against its horizon
   * credit over MORT (deephorizon.c, upkeep_gold_weight). A unit's
   * upkeep over its lifetime is charged NOWHERE at build time -- only
   * the defence FEAR terms carry a partial discount
   * (upkeep_unit_weight), which is a different seam and a different
   * quantity. A Warriors kept 200 turns costs about 20x its build
   * price, and derby-3's birth guards are the standing demonstration.
   *
   * Charged symmetrically with buildings: upkeep per turn x
   * min(E[lifetime], horizon), where E[lifetime] starts at
   * deep_horizon_turns() -- the same horizon the building side uses --
   * and is truncated by CONSEQ's obsolescence ETA where one exists,
   * because that is when deep's own rotation melts the unit.
   *
   * Scales this multiplies, none of them minted here: the shield half
   * converts at SHIELD_WEIGHTING (the codebase's own shield-to-want
   * constant) and the gold half at upkeep_gold_weight (the BUILDING
   * side's own knob, same dimension -- gold per turn against a
   * horizon). Food upkeep is deliberately NOT charged: this seam has
   * no food-to-want conversion, and under-charging is the safe
   * direction for a mechanism whose whole effect is to add a cost.
   *
   * 0 = off. Moves every unit want in the auction, so it is A/B'd
   * alone and not only in the bundle. */
  double unit_upkeep_charge;
  /* WALLS AS POP-LOSS INSURANCE (M4b item 3). Stock prices
   * EFT_UNIT_NO_LOSE_POP at units x 2 (daieffects.c:456) -- a WAG over
   * a headcount, with no reference to whether the city is under
   * attack. The mechanic is exact and CONSEQ verified it: capture
   * costs one size point flat (server/citytools.c:2141), but EVERY
   * successful attack costs another under `killcitizen` +
   * UCF_KILLCITIZEN (server/unithand.c:4601), and that is the one the
   * effect insures.
   *
   * Honest price: E[successful attacks] x the citizen's stream value.
   * The first factor is the chained-duel model's own `hits` output
   * (deep_city_defence_read()) against the observed wave, capped at
   * the citizens the city actually has to lose. The second is
   * deep_unworked_tile_value(), the same citizen valuation every
   * other credit in this ladder uses -- so this credit is directly
   * comparable with the health credit beside it, and this gain
   * absorbs the horizon scale exactly as horizon_health_weight does.
   *
   * ADDITIONAL to walls' Defend_Bonus, which is already inside the
   * odds the tile-aware defence read computes -- so no double count.
   * 0 = off. */
  double pop_insurance_gain;
  /* THE BARRACKS' TWO STREAMS (M4b item 4, amortization audit 1c).
   * Stock prices EFT_HP_REGEN at 5c + num (daieffects.c:459) and
   * EFT_VETERAN_BUILD at amount x (3c + num) (:479) -- two headcount
   * WAGs for two things that are streams.
   *
   * heal_stream_gain prices HP THROUGHPUT: the ruleset's own healing
   * arithmetic (hp_gain_coord(), common/unit.c:2216 -- base HP x
   * regen% per turn) says how many turns of the line each damaged
   * unit in this city gets back, and each returned unit-turn is worth
   * that unit's shields at SHIELD_WEIGHTING. Counted over the units
   * actually standing here damaged, so a city with no casualties is
   * owed nothing.
   *
   * veteran_stream_gain prices the VETERAN MARGIN, which is the
   * stream stock most obviously prices as a point: +power_fact for
   * the unit's WHOLE LIFE. Value per unit = (power ratio - 1) x its
   * shields; units over the horizon = horizon / turns to build one.
   * Both factors are the ruleset's own numbers.
   *
   * Both 0 = off. */
  double heal_stream_gain;
  double veteran_stream_gain;
  /* Assault operations (E6): form when the free-attacker pool's take
   * odds against the best met target clear op_take_target (<= 0
   * disables); muster at the staging city up to op_muster_wait turns
   * before launching with whoever stands there (at least half the
   * quorum) or dissolving. */
  double op_take_target;
  int op_muster_wait;
  /* THE MARGINAL GARRISON DRAFT (ERA 3a,
   * findings/era-probe-evidence-2026-08-12.md section 6).
   *
   * When the free pool cannot clear the formation bar, the draft
   * borrows a blade from the walls. It decides with an ABSOLUTE
   * floor: release the blade only if the city still clears
   * survival_target without it. The 2026-08-12 probe priced that
   * floor over 11 232 formation attempts -- the draft fires on 19.4%
   * of them and adds a mean of 0.28 units, because only 39.9% of
   * survival reads clear 0.85 at all. Meanwhile 44% of attempts reach
   * the odds model with an EMPTY pool and 81% of a deep seat's 13.9
   * late-game military units never appear in it. The floor is not
   * protecting cities that are in danger; it is refusing to spend
   * cities that are already lost, whose blades are worth nothing
   * where they stand.
   *
   * 1 replaces the floor with the comparison the decision actually
   * is: release the blade when the survival it COSTS its city is
   * less than the take-probability it BUYS the operation. Both sides
   * are probabilities the code already computes --
   * deep_city_stack_survival() minus _without() for the cost,
   * deep_take_odds_at_arrival() with and without the blade for the
   * gain -- so there is no new magnitude and no scale factor between
   * them. That the two probabilities are commensurable AT ALL (one
   * is a city surviving a night, the other an operation taking a
   * city) is precisely what the A/B tests; it is not assumed here.
   *
   * 0 = the absolute survival_target floor, exactly as before. */
  int op_draft_marginal;
  /* MUSTER (ERA 3b; findings/era-probe-evidence-2026-08-12.md section
   * 6 sub-mechanism 3b, sessions/S-ERA-3b-muster.md).
   *
   * 3a falsified the incremental route. The take-odds curve is FLAT AT
   * ZERO until pool 6-8 (P(odds > 0) is 5.4% at pool 1; the mean at
   * pool 8 is 0.065 against a 0.65 bar), so no decision that asks each
   * blade to justify itself -- absolute floor or marginal comparison,
   * it makes no difference -- can bootstrap across that flat region: a
   * SET of eight blades is worth having while every increment in it is
   * worth exactly nothing. Formation is memoryless besides (0.88
   * attempts per player-turn, each asking "can I win NOW", each
   * leaving no state behind), so the pool never accumulates across
   * turns either. Two independent reasons nothing masses.
   *
   * 1 lets a slot whose target clears the WORTH gate enter MUSTER with
   * the odds REFUSED, accumulate members across turns, and launch iff
   * the odds AT THE CURRENT POOL clear the same bar. op_take_target
   * becomes the launch gate it already pretends to be, and the force
   * is valued at the SET level when it launches rather than one blade
   * at a time when it forms. Requires op_launch_odds: a muster
   * released by a head count is the trickle the muster discipline
   * exists to prevent, so the mechanism declines to form without it.
   *
   * NOT here to attack more often (human doctrine 2026-08-12: "at
   * gunpowder era it's generally insane to do attacks" -- deep's
   * refusal at parity is CORRECT and the odds model is honest). Here
   * to make the RARE sane attack possible against a weak or
   * overextended target. Its success metrics are kill-exchange ratio,
   * cities held and score; formation counts are not evidence for it
   * and may legitimately fall.
   *
   * 0 = the odds test is the formation gate, exactly as shipped. */
  int op_muster;
  /* How many turns a muster may go without GROWING before it is
   * abandoned -- turns since the high-water member count last rose,
   * not turns since formation: a muster that keeps gaining blades is
   * doing its job however long it takes. One of three abandon
   * triggers, beside the target's worth collapsing and the home
   * defence demand spiking.
   *
   * PLACEHOLDER, untuned. The brief asks for an observed cadence and
   * there is none to be had: nothing in deep has ever accumulated a
   * force across turns, so there is no measurement of accumulation to
   * read a stall off. Shipped equal to op_muster_wait -- itself
   * PLACEHOLDER -- so that a muster is not granted more patience than
   * a formed wave by fiat, and flagged in STATUS.md for the tuning
   * pass. Only read when op_muster is on. */
  int op_muster_stall;
  /* The muster's HOME-DEFENCE abandon trigger, separably (ERA 3b
   * liveness, 2 tech50war-200 games, 2026-08-12).
   *
   * The trigger asks what the staging city keeps once the wave's best
   * defender leaves and the cheapest one-hop backfill arrives, and
   * abandons below survival_target. Measured on the venue it was
   * designed for, it is not one of three triggers -- it is THE
   * trigger: 155 of 191 abandons (81%), against 34 worth collapses
   * and 2 stalls, with 337 of 457 held muster-turns holding exactly
   * one member and only 5 of 433 musters ever launching.
   *
   * The reading is honest either way and this knob is what lets the
   * A/B decide which: at survival_target = 0.85 on a war venue, a
   * frontier staging city essentially never spares its best blade, so
   * either the muster is correctly refusing to disarm a home front
   * that is genuinely under threat (the Gunpowder doctrine's own
   * claim) or the trigger is priced against the wrong counterfactual
   * and starves the mechanism before it can gather. A dominant
   * behaviour that cannot be switched off cannot be attributed.
   *
   * 1 = the trigger as designed and as measured. 0 = the muster
   * answers to the stall clock and the worth gate only. Only read
   * when op_muster is on. */
  int op_muster_home;
  /* Launch on odds, not on a head count (combat E2): a mustered wave
   * is released when the force STANDING at the staging city clears
   * the same bar formation used, re-priced against the garrison as it
   * is now -- `required` was computed against the garrison of the
   * formation turn and the target has had op_muster_wait turns to
   * build since. 0 restores the head count. The wait cap keeps its
   * half-quorum count either way: that is a patience rule. */
  int op_launch_odds;
  /* The op-level go/no-go asks the TOTAL question the marginal build
   * want cannot (human call 2026-08-06): worth x P(take) minus the
   * whole assault's expected losses, converted at SHIELD_WEIGHTING
   * through attack_loss_gain so both seats price a shield the same.
   * A wave that clears the odds bar and still loses money does not
   * launch. 0 restores the bar alone. */
  int op_net_value;
  /* And how much of the wave must be expected to SURVIVE taking the
   * city, from the substrate's attacker-loss output: a wave that wins
   * the city and dies doing it hands it back next turn. In units,
   * expected. 0 disables the hold test. */
  double op_hold_survivors;
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
  /* Hull commission (EXPANSION slice 2): weight on the excess of
   * overseas FOUNDER demand over lift supply on one of this city's
   * waters, priced as the settlements that excess is holding times
   * the delay a hull takes off them --
   *
   *   want = amortize(w x n x (value - amortize(value, wait)), build)
   *
   * with n = MIN(gap, hull capacity), value = colony_want aged by
   * amortize(), wait = deep_wait_expected() for the same water. Every
   * term is measured or a ruleset fact; this weight is the whole
   * constant, and 0 = off is today's behaviour exactly.
   *
   * Distinct from colony_lift_weight, which prices a site SCORE
   * damped by fleet surplus and only on turns a probe published one.
   * This one prices the QUEUE, and it fires where that want is
   * silent. */
  double hull_commission_weight;
  /* Naval slice 3: virtual defenders per turn of sail time added to
   * a target's garrison when pricing an amphibious take -- travel
   * time is the defender's budget. 0 prices arrival like adjacency. */
  double invasion_reinforce_rate;
  /* INVASION-RATE: the SLOPE on the same law. invasion_reinforce_rate
   * above is flat -- every city on the map reinforces at one rate,
   * whether it is a four-shield frontier town or a war-economy
   * capital. This weights the rate that city can measurably sustain,
   *
   *   defenders/turn = surplus[O_SHIELD] / cost(best buildable defender)
   *
   * and the model charges rate + slope x that. 0 = the flat model
   * exactly, byte for byte, which is what shipped; 1.0 = take the
   * observed production rate at face value on top of the flat term.
   * Sweeping the pair spans "all cities reinforce alike" through
   * "reinforcement IS production" without assuming either.
   *
   * A floor on the production channel, not an estimate of all
   * reinforcement: gold-bought defenders and units walking in from a
   * neighbour are invisible to it and both push the true rate up.
   * That gap is what a tuned slope > 1 would be pricing.
   *
   * Civilised targets only -- the barbarian branch stays flat at
   * barb_reinforce_rate, because T160 measured barb cities not
   * garrisoning regardless of what they could afford. */
  double invasion_reinforce_slope;
  /* Naval slice 3: the fixed turns an overseas eta pays on top of the
   * sail itself -- board the hull at this end, come ashore at the
   * other. Charged by the four seams that price an OVERSEAS arrival
   * (deep_assault_want()'s main eta and its per-type sweep, the pool
   * reach test, and deep_op_form_assault()'s required-force pricing),
   * and by none of the land ones.
   *
   * 2 is what those seams shipped as a bare literal from naval slice
   * 3 (2026-07-31) until this field; it is registered at that value,
   * so the default is byte-identical and the tuner can finally reach
   * it. NOT charged by deep_op_eta(), the march re-gate's own clock
   * -- see STATUS's INVASION-RATE note; making the two agree is a
   * behaviour change and is not this knob. */
  int invasion_embark_turns;
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
  /* Which population hut_risk_discount is charged against.
   *
   * 0 = hut_risk_radius, own cities only: the shipped default, and
   * the SIGN IS BACKWARDS against the server. The Lua that decides
   * (data/default/default.lua:112, and hut_get_limited() at
   * server/unittools.c:3321) refuses to unleash when ANY player's
   * city lies within CITY_MAP_MAX_RADIUS of the hut, so huts near
   * cities are the risk-free ones and the radius form discounts
   * precisely those.
   *
   * 1 = the server's own predicate, discounting the huts that can
   * actually unleash (none near any city). hut_risk_radius is then
   * unread: the radius belongs to the server, not to us.
   *
   * Default 0 rather than 1 because the correction has never been
   * measured -- it moves an explore-value term on every hut map, and
   * the v3b tuner's habit of zeroing explore_hut_value on classic is
   * a documented response to the wrong-signed term, so flipping the
   * sign and re-pricing the hut are one factorial question. The
   * populations are on deep-explore as huts_disc / huts_unleash. */
  int hut_risk_honest;
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

  /* --- The stance layer (W34 phase A). ---
   *
   * These are the control surface, not a mechanism: at identity axes
   * every gate multiplies by exactly 1.0 and the binary is
   * byte-identical to one built without them. */

  /* Which corner every deep seat is clamped to, or -1 for FREE.
   * 0 = balanced (identity), 1 = economic, 2 = military
   * (enum deep_stance_preset). FREE is what ships and what self-play
   * should exercise; PINNED is what makes a tuning block separable,
   * because economy and military bid for one shield budget
   * (findings/objective-layer.md section 1). A run must LOG which
   * mode it was in: a tuning result taken free and reported as
   * pinned is a wasted campaign. NOT a magnitude -- the campaign
   * sets it by hand per pass, so its registered range is degenerate
   * and the optimiser cannot move it. */
  int stance_forced;

  /* Where a FREE player's axes come from: 0 identity, 1 traits,
   * 2 traits+position (W34 A.4's ladder). Only 0 is implemented;
   * E34.3 and E34.4 own the rest, and a file that asks for one of
   * them gets identity plus a loud log line rather than a silent
   * pretence. */
  int stance_source;

  /* How far off identity an axis may be moved -- by a pinned corner
   * today, by a trait in E34.3. Shared on purpose: a pinned corner
   * is then exactly the extreme a trait-driven player can reach, so
   * a constant tuned pinned means the same thing free. It is the one
   * number that decides how DIFFERENT players are allowed to be, and
   * exactly the kind rule 7 forbids guessing. PLACEHOLDER. */
  double stance_trait_span;

  /* Gate 1: sensitivity of the ANTICIPATORY fear terms to the
   * mil:econ ratio, as (mil/econ)^gain. Identity axes give identity
   * behaviour at any gain; 0.0 disables the gate outright. Does not
   * touch the survival deficit or the garrison floor -- gate 1
   * scales anticipation TOWARD the survival floor, never below it.
   * PLACEHOLDER. */
  double stance_fear_gain;

  /* Gate 2: sensitivity of ASSAULT FORMATION to the econ:mil ratio.
   * The smallest force that clears op_take_target is what forms an
   * operation; this scales that bar by (econ/mil)^gain, capped at
   * 1.0, so an economy-weighted player commits its blades only where
   * the take is near-certain and a force-weighted one forms freely.
   * Targets that THREATEN us keep the unscaled bar -- the offensive
   * mirror of gate 1's survival floor. PLACEHOLDER. */
  double stance_op_gain;

  /* Gate 3: sensitivity of the horizon growth credits (the growth
   * ladder that bids against barracks and walls) to the econ:mil
   * ratio, as (econ/mil)^gain. The credits are additive on top of
   * stock's base want and never go below zero, so this cannot price
   * a building under stock's own valuation. PLACEHOLDER. */
  double stance_growth_gain;

  /* Gate 4: sensitivity of EXPANSION -- the settler want and the
   * colony lift it needs to cross water -- to the econ:mil ratio, as
   * (econ/mil)^gain. Whether land remains is already asked by the
   * site machinery; this only says how much this player wants what
   * remains. PLACEHOLDER. */
  double stance_expand_gain;

  /* Gate 5: sensitivity of the OFFENSIVE BUILD -- deep_assault_want's
   * marginal-take-odds attacker demand -- to the mil:econ ratio, as
   * (mil/econ)^gain. It reads the ratio the way gate 1 does, and for
   * gate 1's reason: this prices a military want, so an economy
   * weight must lower it. Gate 2 governs whether blades that EXIST
   * are committed; nothing priced what they cost to buy, and E34.2
   * measured offence share RISING under an economic pin because of
   * it. Targets that THREATEN us keep the unscaled want, by the same
   * test gate 2 uses. PLACEHOLDER. */
  double stance_assault_gain;
  /* Gate 6: exponent on (mil/econ) scaling the class-gated garrison
   * floor. PLACEHOLDER. 1.0 is identity at equal axes, so the
   * benchmark baseline is a uniform floor; a pinned or trait-seeded
   * stance turns it into an early-war / early-peace personality. */
  double stance_floor_gain;
  /* SOURCE 2 (W34 E34.4, A.4 step 3): how far the POSITION and the
   * RESEARCH LEAN may move an axis off the trait prior, in index units
   * -- the same units stance_trait_span then scales, so the clamp that
   * keeps a free player inside the pinned corner still binds. Both
   * PLACEHOLDER. Zero on both is exactly source 1, which is the
   * per-term off-switch an A/B needs.
   *
   * stance_position_gain weights (free land, war, border pressure);
   * stance_techlean_gain weights the military-vs-economic mix of the
   * advances this player's bulbs have actually bought. */
  double stance_position_gain;
  double stance_techlean_gain;
  /* The border pressure PER CITY WE HOLD at which source 2 reads the
   * threat term as half its maximum: threat = p / (p + ref), the
   * saturating shape deep_land_pressure() uses. In density-lattice
   * units (density_fear_value / (1 + d) summed over city pairs), so it
   * moves with density_fear_value and cannot be reasoned about
   * independently of it. PLACEHOLDER, and the stance line carries the
   * raw pressure so a tuning pass can see the distribution. */
  double stance_pressure_ref;

  /* Spy-action valuation (W35 E35.1, deepspy.c). spy_enabled is the
   * off-switch: 0 (the compiled default) leaves stock's fixed ladder
   * deciding what a diplomat does and runs deep's seam census only,
   * so a default game is byte-identical.
   *
   * spy_gold_value is one gold in the want currency, and it is the
   * exchange rate three of the four families ride on -- a stolen
   * tech's bulb cost at stock's own x3, and the incite cost the server
   * charges. spy_sabotage_gain prices shields DESTROYED against
   * shields produced: denial is not acquisition, and the two are not
   * worth the same. Both PLACEHOLDER. */
  int spy_enabled;
  double spy_gold_value;
  double spy_sabotage_gain;

  /* Tax/science/luxury rates (the rates epic). Stock's
   * dai_manage_taxes() is a SOLVENCY ladder: it finds the minimum tax
   * that balances the books, the minimum science that avoids tech
   * loss, tests celebration, and dumps whatever is left into science
   * by a fixed preference order. No tech is ever priced, so science
   * is a residual rather than a decision -- and deep READS the
   * resulting rates when it values a multiplier building
   * (deephorizon.c), i.e. it reads a solvency artefact as a statement
   * of strategic value.
   *
   * rates_enabled != 0 lets deep re-choose the split by comparing the
   * amortized value of what each point BUYS, using prices that
   * already exist elsewhere in deep: a bulb is priced by the tech
   * want the research advances, a gold by upkeep_gold_weight (the
   * same gold-per-turn-over-the-horizon price the upkeep charge
   * pays), a luxury point by the freed citizen's tile value at
   * horizon_content_weight. 0 = off = stock's ladder untouched, which
   * is the compiled default. PLACEHOLDER.
   *
   * The two gains are the honest residue: deep's credit currency is
   * loose enough that a tech want, a gold stream and a freed citizen
   * are not guaranteed commensurable, and gold is the numeraire here
   * only because it is the one of the three deep already had a price
   * for. Both PLACEHOLDER 1.0 and both owed a tuning pass; a
   * degenerate all-science or all-tax split is what a wrong gain
   * looks like. */
  int rates_enabled;
  double rates_science_gain;
  double rates_luxury_gain;

  /* Government valuation (the gov epic, deepgov.c). Stock's
   * dai_gov_value() sums dai_city_want() over the empire under each
   * candidate government and adds an effect sum at a 9999-turn
   * horizon its own comment marks TODO -- a LEVEL, at today's size,
   * with no cost of getting there. So the decision that reshapes the
   * whole economy is priced in a currency nothing else in deep uses,
   * and a revolution is free.
   *
   * gov_valuation != 0 replaces that with the grammar every other
   * deep credit is written in (findings/amortization-audit item 1d):
   * the per-turn output DELTA against the incumbent, counted over
   * deep_horizon_turns(), against the one-off cost of the revolution.
   * 0 = off = the hook forwarded to stock unchanged, which is the
   * compiled default and byte-identical. Structural off-switch, not
   * an optimiser knob. */
  int gov_valuation;
  /* What the anarchy a revolution passes through is charged at. The
   * cost itself is MEASURED, not assumed: the turns the server would
   * actually make this player serve, times the empire output those
   * turns lose, in the same currency as the gain. 1.0 therefore means
   * "charge the measured loss at face value" and is an identity
   * rather than a taste -- but the range is registered because the
   * measurement values anarchy at TODAY's arrangement while the
   * empire that emerges is a few turns older, and only a tuning pass
   * can say whether that biases the charge up or down. Inert at
   * gov_valuation 0. NOT inert above EASY: H_REVOLUTION decides how
   * many EXTRA turns of anarchy are served, not whether the empire
   * passes through anarchy, and it always does for one turn of
   * production (see deep_gov_anarchy_turns()). */
  double gov_anarchy_weight;
  /* What a government's RATE CEILING is worth (darkfx item 5). A
   * government decides how far the tax/science/luxury split may be
   * pushed -- Despotism 60, Democracy 90 -- and nothing in the tree
   * has ever priced that: EFT_MAX_RATES returns 0 from
   * dai_effect_value(), and both daidata.c:547 and advdata.c:1031 say
   * in source that re-optimising the rates per candidate is too
   * expensive. Deep prices it by re-running deeprates.c's own
   * enumeration over the same empire with only the ceiling changed,
   * which reads the BINDING constraint for free: an empire whose
   * split is interior gains exactly zero from a higher cap.
   *
   * PLACEHOLDER, 0.0 = off, and off is where it ships -- the term is
   * the one part of this valuation whose magnitude has no honest
   * source, because it converts a rates-enumeration total into a
   * government want and nothing has measured that ratio. Flagged in
   * STATUS for the tuning pile. */
  double gov_rate_ceiling_gain;

  /* Caravan BUILD want on merit (the trade-merit epic). Deep already
   * chooses long, foreign destinations with one honest transit
   * convention (deeptrade.c), but the want that decides whether to
   * build a caravan at all is still stock's dai_choose_trade_route(),
   * whose CBS_CLASSIC income estimate is
   *
   *   income = (10 + 10) * (1.75 * pcity->surplus[O_TRADE]) / 24
   *
   * (daidomestic.c:360) -- a HARDCODED distance of 10, only the
   * SOURCE city's trade, and no destination at all. It is then
   * clamped to 0.1 whenever caravans pile up, and the fallback ladder
   * builds one at want 1 exactly when the city wants nothing. Deep
   * raised what a caravan is worth and left what it costs to decide
   * to build one untouched.
   *
   * trade_merit != 0 replaces that bid with the discounted stream the
   * caravan would actually collect: caravan.c's own NPV model run
   * against the REAL destinations from this city, over the MORT
   * horizon every other deep credit is summed over, annuitized back
   * to a per-turn output value (the currency build wants are in) and
   * amortized over the turns until the caravan is finished. 0 = off =
   * stock's bid untouched, which is the compiled default.
   * PLACEHOLDER.
   *
   * trade_merit_gain is the honest residue and the thing a tuning
   * pass must find: the NPV is in raw output points priced by the
   * advisor's own trade priority, and whether that lands on the same
   * scale as a Library's want is a CALIBRATION question the rates
   * epic (R1) already lost once, for the same reason -- W01's want_t
   * conversion was declared and never built. 1.0 is the untuned
   * corner, not a claim. PLACEHOLDER. */
  int trade_merit;
  double trade_merit_gain;

  /* The two knobs of the route PORTFOLIO (TRADE, 2026-08-06). Both sit
   * INSIDE trade_merit -- neither can execute while the master switch
   * is 0 -- and both are exact off-switches at their defaults.
   *
   * trade_ceasefire_peace_odds. A cease-fire partner's route has a
   * KNOWN death date: player_diplstate_get()->turns_left, after which
   * the pair is at war and both rulesets in use set the Enemy trade pct
   * to 0 with cancelling "Cancel". Deep therefore prices such a route
   * as the annuity TRUNCATED at the lapse rather than banning it (a
   * ban is an invented rule) or paying it in full (which is false).
   * This knob is the one thing truncation cannot know: the probability
   * that peace follows the cease-fire, which buys back the post-lapse
   * tail. At 0.0 -- the compiled default -- the price is pure known
   * mechanics and no fragile route is farmed on optimism. PLACEHOLDER;
   * a tuner that finds peace usually follows may pay for the tail.
   *
   * trade_route_risk_gain. A caravan that dies on the road opens no
   * route, and deep already owns the civilian danger test its settlers
   * refuse to stand on (deep_settler_overnight_veto, asked here at
   * settler_guard_tolerance). The route's NPV is multiplied by
   * 1 - gain * (dangerous tiles / path length), clamped to [0,1], so
   * 0.0 is exactly no discount and 1.0 prices a wholly dangerous road
   * as worthless. The path comes from the pf map the destination
   * enumeration already built, so the discount costs one path
   * extraction rather than a traversal. PLACEHOLDER. */
  double trade_ceasefire_peace_odds;
  double trade_route_risk_gain;

  /* Interdependence: love per ACTIVE foreign trade route per turn,
   * added from deep's own phase wrapper (human design call,
   * 2026-08-05). struct deep_plr embeds struct ai_plr, so stock's war
   * and peace code reads whatever love holds and ai/default is not
   * touched. Stateless: stock's 4%/turn decay unwinds it the moment
   * the routes stop being counted.
   *
   * It is the counterweight to a bleed that already exists --
   * best_target costs the chosen victim up to 50 love a turn against a
   * 125 peace bar, and it chooses the rich neighbour, which is the one
   * worth trading with. The clamp is STRUCTURAL rather than a second
   * knob: trade love is added only while love stays below the player's
   * own req_love_for_alliance, so routes can carry a pair to peace and
   * never, alone, into an alliance.
   *
   * Exploitable by design and symmetric: a rival can farm routes to
   * pacify deep. That is now a POSTURE and not an accident:
   *
   * HUMAN CALL 2026-08-06 -- "remove the love bar, trade want anchors
   * love". peace_seek_gain and trade_route_love ship ON TOGETHER, and
   * the pairing is the whole argument. Waiving the love precondition
   * on peace alone would make deep forgive indiscriminately; waiving
   * it while commerce pays love means a partner who trades with us
   * EARNS the affection that justifies the peace, and one who stops
   * decays back toward wariness at stock's own 4% a turn. The
   * forgiveness is safe because it is bought, per turn, by a route
   * that either exists or does not.
   *
   * So the compiled default is 5.0, not 0.0. It is still a
   * PLACEHOLDER -- the value the measurement arm used, chosen to give
   * the mechanism a population, never swept -- and 0.0 restores the
   * previous behaviour exactly.
   *
   * trade_partner_target_discount is the same posture on the military
   * side, and it is where the project's STANCE toward its partners is
   * tunable rather than asserted. deep_op_pick_target() scores an
   * enemy city by worth over distance; this multiplies that score by
   * (1 - discount) when we hold live routes with its owner, so among
   * comparable targets deep burns a stranger before a partner. It does
   * NOT own war and peace -- that stays stock's until W34-B -- it only
   * stops deep destroying its own trade when an equal target exists.
   * 0.0 is the PURE OPPORTUNIST posture and is the compiled default;
   * 1.0 refuses a partner outright. PLACEHOLDER. */
  double trade_route_love;
  double trade_partner_target_discount;

  /* Wonder-caravan BUILD want on merit (R2b), the SAME reform applied
   * to the larger claimant. R2's deep-want census found the caravan
   * build queue is decided by dai_choose_help_wonder()
   * (daidomestic.c:71), not by the trade producer: winning want p50
   * 197 against 71.
   *
   * That producer prices a caravan as
   *
   *   want = wonder_city->building_want[wonder] / MAX(walk_turns, 1)
   *
   * behind a count gate (build one while build_points_left exceeds
   * one caravan's shields times the caravans already on this
   * continent). Two things are wrong with it and neither is the
   * wonder's want, which D2 made honest: a caravan is credited with
   * the WHOLE wonder's want regardless of what fraction of the wonder
   * its shields actually buy, and the delay is divided by rather than
   * discounted -- so a wonder 40 turns of building away and one due
   * next turn are worth the same per turn of walking.
   *
   * wonder_merit != 0 re-denominates it per shield delivered: the
   * wonder city's own want times the share of the wonder these
   * shields pay for, with the shields already pledged by caravans in
   * the field deducted first (the graded form of stock's count gate),
   * amortized over the turns to build the caravan PLUS the turns to
   * walk it there. No new scale is minted: the number that comes out
   * is the wonder city's own building_want, scaled and discounted.
   * 0 = off = stock's bid untouched, the compiled default.
   * PLACEHOLDER.
   *
   * wonder_merit_gain is the residue, and it is DELIBERATELY not
   * trade_merit_gain: the two wants have different denominators (an
   * NPV in output points annuitized over MORT there, a building want
   * scaled by a shield share here), so one number cannot calibrate
   * both and sharing it would hide which producer a tuning pass had
   * moved. 1.0 is the untuned corner, not a claim. PLACEHOLDER. */
  int wonder_merit;
  double wonder_merit_gain;

  /* Whether a deep seat PAYS the change-production penalty every human
   * player pays.
   *
   * It does not today, and neither does any other AI:
   * change_build_target() (server/citytools.c:3221) applies
   * city_change_production_penalty() only when the player is human or
   * carries H_PRODCHGPEN, and no difficulty level above `easy` sets
   * that flag (ai/difficulty.c:71/91). So an AI seat can hold 200
   * shields against a wonder, switch to a defender, and keep all 200 --
   * while stock's own build code docks the want of a held stock as if
   * the shields were at risk (daicity.c:1909), pricing a cost the seat
   * never pays.
   *
   * pay_switch_penalty != 0 makes deep ISSUE ITSELF the handicap in its
   * own phase hook, the same way deep reads its own H_MAP rather than
   * asking for an exemption. Nothing in ai/default or the server
   * changes; the seat simply stops being exempt, so the dock becomes
   * true pricing and the shields a switch strands are really stranded.
   *
   * The cost lands before any mechanism learns to avoid it -- existing
   * churn is taxed on the first turn the flag is on -- so a score drop
   * is the expected short-run shape and not a reason to take the
   * honesty back out.
   *
   * 0 = exempt, which is every measurement taken before this knob
   * existed and is still the arm to run for comparison against them.
   * Compiled default 1 by human call, 2026-08-06: "honest but
   * expensive". NOTE THE ASYMMETRY THIS CHOOSES -- ai/default and
   * ai/baseline stay exempt, so in every benchmark from here on deep
   * is the only seat paying a cost its opponents do not. That is the
   * chosen posture, not an oversight. */
  int pay_switch_penalty;

  /* Whether the build auction PRICES the switch it is about to make.
   *
   * Paying the penalty and not pricing it is the worst of both: the
   * session measured 3470 shields a game really forfeited on contested
   * with the churn that caused it completely unchanged (-3%), because
   * nothing in the auction knew. switch_cost_price debits every
   * non-emergency candidate that would change production CLASS by the
   * shields `city_change_production_penalty()` says the change really
   * costs, converted at SHIELD_WEIGHTING -- the codebase's own
   * shield-to-want constant (advbuilding.h:20, and what
   * adv->shield_priority is set to) -- rather than at stock's WAG of
   * `shield_stock * SHIELD_WEIGHTING / 4`.
   *
   * The hysteresis is not a rule, it is the arithmetic: a candidate in
   * the incumbent's own class is charged nothing, because the ruleset
   * charges nothing for it, so a challenger from another class has to
   * beat the incumbent by the real forfeit. A challenger whose want
   * does not survive the debit does not win a cheaper prize -- the
   * city keeps building what it was building.
   *
   * With this on, deep also hides H_PRODCHGPEN from
   * dai_build_adv_adjust(), so stock's own dock (daicity.c:1907) keeps
   * the /15 form it has had in every measurement instead of switching
   * to its /4 branch. The cost is then charged once, by the honest
   * term, and the inherited phantom is left exactly as it was.
   *
   * switch_cost_gain is the residue on the conversion, 1.0 = the
   * shield's own weight, untuned. PLACEHOLDER. */
  int switch_cost_price;
  double switch_cost_gain;

  /* R3 -- one scale for what a city can do. The build auction compares
   * bids that are not in the same unit: a weighted-output sum for a
   * building, a percentage of danger for a defender, a trait-multiplied
   * settler count for a founder, a kill-expectation for an attacker.
   * Each key multiplies ONE producer class before the comparison. The
   * BUILDING is the anchor and has no key: a rescale is only meaningful
   * relative to something held fixed, and the building want is the one
   * bid in the auction that is already a discounted output sum.
   *
   * 1.0 everywhere is today's behaviour, byte-identically -- the scale
   * is skipped, not multiplied. Shipped values are corpus-DERIVED
   * (findings/want-corpus-2026-08-06.md) and delivered in a tuning
   * FILE, the way W01 delivered diplomat_defence_value. All
   * PLACEHOLDERs; the cloud pile owns refinement.
   *
   * scale_settler and scale_worker act where ai/default FORMS the bid
   * (city_data->founder_want / worker_want, before
   * domestic_advisor_choose_build() runs its own auction), because a
   * factor on that advisor's winner cannot reorder the classes inside
   * it. scale_defender / scale_attacker / scale_ferry act on deep's
   * own military choice at the merge seam. The EMERGENCY tier is
   * exempt everywhere: it is the ruleset's must-act signal, and a
   * deflator that leaked across it could lose a city. */
  double scale_settler;
  double scale_worker;
  double scale_defender;
  double scale_attacker;
  double scale_ferry;

  /* The same disease one tier up. Inside the EMERGENCY tier priority
   * is still a magnitude, so a kill_something_with attacker at 259976
   * outranks a grave-danger defender at 100 by three orders. This
   * caps the emergency VALUE -- the tier itself is never touched, so
   * every must-act want still outranks every normal one. Corpus-
   * derived: above the largest value any honest emergency producer
   * reaches in 20 games, below the spikes. 0 = no cap = today's
   * behaviour. PLACEHOLDER. */
  double emergency_value_cap;

  /* PEACE -- the cease-fire trap. 20-42% of every pair that ever
   * stopped shooting reached DS_CEASEFIRE and never got off it
   * (findings/deep-dive-2026-08-05/diplomacy-gamestate.md §2a), and
   * the mechanism is legible: a cease-fire lapses back to WAR after
   * 16 turns (diplhand.c:57), stock refuses to even PRICE peace while
   * `turns_left > 4` (daidiplomacy.c:405), and its own anti-spam gate
   * skips the whole AI-AI diplomacy branch for fc_rand(4)+3 turns
   * (:1814,:1824). The window and the gate are the same size, so the
   * pass that could ask often is not running on any of the four turns
   * it would be allowed to.
   *
   * peace_seek_gain runs a PRE-PASS in deep's own wrapper, before
   * stock's spam-gated pass, on exactly the turns the window is open.
   * The value is the fraction of stock's own req_love_for_peace bar
   * deep is willing to WAIVE for a partner it is not committed to
   * fighting: 0.0 asks for the full stock bar and therefore proposes
   * nothing stock would not already have proposed given the chance,
   * which is why 0.0 is the off switch AND the honest control. 1.0
   * would seek peace at any love at all.
   *
   * Note what this does NOT do: it never waives the structural guards
   * (a live war countdown, an alliance with our enemy, H_AWAY, the
   * non-attack precondition). It waives a LOVE THRESHOLD, and love is
   * the quantity the trap is built out of -- best_target bleeds up to
   * 50/turn against a 125-love bar (:1135-1141), so a pair can be
   * permanently below a bar it would clear if it were ever asked.
   *
   * THE TRADE IS REAL AND IS NOT HIDDEN: a peace deep signs is a
   * conquest deep forgoes, and on cells decided by conquest that can
   * cost score. PEACE measured that cost and did not find it: the
   * peace-vs-conquest trade never turned negative on either cell.
   *
   * HUMAN CALL 2026-08-06 -- "remove the love bar, trade want anchors
   * love". The compiled default is 1.0: deep seeks peace with no love
   * precondition at all. That is a values decision and it overrides
   * rule 7's hold on shipping an unswept number; what makes it safe
   * rather than merely forgiving is that it ships PAIRED with
   * trade_route_love (above), so the affection deep acts on is EARNED
   * by commerce and decays when the commerce stops. 1.0 is still the
   * setting PEACE measured its population at, not a tuned optimum, and
   * 0.0 restores stock's bar exactly. PLACEHOLDER. */
  double peace_seek_gain;

  /* FENCE -- who deep's cooperative dividend is for.
   *
   * Human ask 2026-08-13: "deep's alliance abilities should only
   * really benefit its fellow deeps". CAMPAIGN-v4 section 11.4's
   * refinement made ally-directed surplus a FEATURE -- gifts, shared
   * vision, coordinated stances are what deep's diplomacy buys -- and
   * the measured failure mode of the love gift was that the surplus
   * landed on a RIVAL (deep +1.93, counterparty +5.26). The gift
   * classes narrowed WHO gets paid; this knob narrows who deep will
   * be in a pact with at all.
   *
   * When 1, a treaty partner who is an AI and is NOT running ai/deep
   * gets nothing at peace or above: deep neither proposes nor accepts
   * a treaty carrying CLAUSE_PEACE or CLAUSE_ALLIANCE, and anything
   * bundled with such a clause (embassy, shared vision) dies with the
   * treaty it rode in on. Cease-fire is deliberately left open --
   * PEACE's cease-fire-trap pricing is the instrument the whole
   * diplomacy line was built on, and a fence that closed the
   * cease-fire would take that instrument away.
   *
   * Fellow-deep is `aplayer->ai == pplayer->ai`, which is how the
   * module already identifies itself (deepsettler.c:2313). No new
   * state, and mixed games have worked since Phase 0.
   *
   * HUMANS ARE EXEMPT, on purpose: `is_ai(aplayer)` guards the test.
   * The fence is a policy about which SOFTWARE deep shares a dividend
   * with; refusing peace to a human would just be deep playing worse
   * against the only opponent that can be surprised by it.
   *
   * EXISTING TREATIES ARE NOT TORN UP. The gate covers new agreements
   * only, because it lives on the proposal and evaluation paths and
   * nothing walks the standing diplstates. A knob flipped mid-game
   * therefore stops the next pact, not the last one.
   *
   * THE SIDE EFFECT IS THE COST, and it must be reported beside the
   * score: refusing peace means longer wars, and a fence that wins on
   * score by bleeding a frontier seat is a real result that has to be
   * visible. wars-active and cities-lost are read alongside every
   * arm of this knob's A/B.
   *
   * 0 = off = deep's diplomacy exactly as measured to date =
   * byte-identical. STRUCTURAL POLICY FLAG, not an optimiser knob:
   * the range is degenerate and the tuner must never sweep it.
   * PLACEHOLDER. */
  int diplo_deep_only_pacts;

  /* BALANCE -- the exchange rate between the density lattice's units
   * and gold, and the switch that turns the gift census on.
   *
   * deep_border_pressure_from() aggregates the SAME weights a city's
   * density fear is built from (density_fear_value / (1 + d), the
   * reachability discount for a neighbour who cannot float an army)
   * per player pair, so it answers "how much of a front would a war
   * with this player be". Its units are danger. A treaty is priced in
   * gold. There is no read anywhere in this tree for the rate between
   * them -- so it is a knob, flagged for the tuner, rather than a
   * number chosen in the code.
   *
   * 0.0 does two things at once and that is deliberate: the border
   * half of the peace valuation vanishes, and the `deep-gift` census
   * stops being emitted. There is no behaviour behind either, so this
   * is an off switch for a MEASUREMENT, and the two gift levers below
   * carry their own. PLACEHOLDER, untuned. */
  double gift_border_value;

  /* BALANCE lever 1 -- buying a partner's love, and what deep is
   * willing to pay for it.
   *
   * ai/default converts a wholly-beneficial treaty into affection at
   * a rate that is exactly computable from readable state
   * (dai_treaty_accepted, daidiplomacy.c:775-782): love rises by
   * balance / (their cities x 10 + 1), capped, times ten. So the
   * price of a relationship is not a guess.
   *
   * THE INSTRUMENT IS A TECH AND THAT WAS MEASURED, NOT CHOSEN. The
   * gift census over eight games says deep's treasury above
   * ai/default's own gold reserve has a median of 17 gold on
   * contested and 0 on lategame-80, while the gold that would clear
   * the love bar has a 90th percentile of 1085 and 1729. Gold is not
   * a currency this AI holds. A tech is: stock prices one at three
   * times the receiver's remaining bulbs, which is hundreds to
   * thousands of gold and costs no treasury at all.
   *
   * gift_love_gain is the willingness bound: deep hands over an
   * advance when what the receiver banks for it is no more than this
   * multiple of what the relationship is worth to us (routes at stake
   * plus border pressure, both in gold). It is therefore a ratio
   * between two computed quantities and not a magnitude -- but the
   * two quantities live on different scales (a peace is worth single
   * digits at gift_border_value = 1.0; a tech costs thousands), so
   * the pair of knobs has to be swept together and the ranges in
   * tuning-params.ini say so.
   *
   * 0.0 = off = no gift is ever offered = byte-identical.
   * PLACEHOLDER. */
  double gift_love_gain;

  /* GIFT-REFOLD -- WHERE THE SURPLUS LANDS (CAMPAIGN-v4 section 11.4).
   *
   * The love gift is the largest single-lever self-play move this
   * project has measured (+8.28 contested, +8.92 lategame-80) and the
   * mixed head-to-head says it is a TRANSFER: deep +1.93 (t 0.72),
   * the counterparty +5.26 (t 2.15), deep's margin over ai/default
   * -3.34 (findings/balance-statecraft-2026-08-06.md section 0). The
   * human's fold criterion is not "no surplus" -- it is that the
   * surplus lands INSIDE the alliance: "i don't mind multiple folks
   * getting better, but it _is_ more fun to get a wash as an alliance
   * benefit."
   *
   * So these three multiply gift_love_gain's willingness bound by the
   * relationship the gift would serve (deep_gift_rel_of):
   *
   * - gift_ally_gain    the alliance and its candidates: an existing
   *                     ally, a shared enemy, a common ally. This is
   *                     where section 11.4 wants the surplus.
   * - gift_instrument_gain  no friendship, but we are already at war
   *                     elsewhere and this player presses our border:
   *                     the second front deep cannot afford, priced
   *                     rather than assumed. Its own term so the fold
   *                     can keep it, kill it, or read it separately
   *                     from friendship.
   * - gift_rival_gain   everyone else -- the population that banked
   *                     the +5.26. 0.0 refuses them outright.
   *
   * A MULTIPLIER RATHER THAN A GATE because the classes differ in
   * degree as well as in kind: a would-be ally is worth paying MORE
   * for than today's flat bound, not merely more than a rival, and a
   * bool could not say that.
   *
   * 1.0 on all three is exactly today's bound for every class, so the
   * shipped configuration -- including the adopted pv2 baseline, which
   * carries gift_love_gain = 46.59 -- is unchanged until an arm moves
   * them. PLACEHOLDERS: the fold arm is a mixed head-to-head and the
   * numbers are its business, not this file's. */
  double gift_ally_gain;
  double gift_instrument_gain;
  double gift_rival_gain;

  /* BALANCE lever 2 -- proxy arming (human design call, 2026-08-05).
   *
   * Hand a military advance to a DISTANT power that is already in the
   * way of whoever is menacing us, so that the menacer has a second
   * front to think about. Every part of the selection is state deep
   * already reads: the menacer is the top of deep's own border-
   * pressure ranking among players at war with us; the proxy is at
   * war with, or bordering, that menacer; the proxy is distant by the
   * fear lattice's own reachability law, so arming them cannot raise
   * OUR density fear; and the gift is leak-safe by ai/default's tech
   * leak veto (daidiplomacy.c:239-258) INVERTED from a price
   * adjustment into a targeting criterion, with the embassy channel
   * civ2civ3's leakage rule adds and stock has no term for.
   *
   * The tech is the one that most shifts THEIR balance -- the
   * strongest attacker the advance unlocks for them -- and the whole
   * lever SWITCHES OFF for a proxy whose best attacker already beats
   * the menacer's best defender. That satisfaction rule, not a
   * budget, is what stops this emptying the tech tree into a third
   * party.
   *
   * proxy_arm_gain is the willingness bound, and it is denominated
   * exactly like the love gift's: deep will hand over an advance
   * whose price to the receiver is at most this multiple of what the
   * menacer's pressure is costing us, at gift_border_value's rate. So
   * the more a player is pressing on our borders, the more deep will
   * spend arming somebody else against them, and at zero pressure it
   * spends nothing.
   *
   * ETHICS OF PLAY, and it belongs in the changelog rather than in a
   * footnote: switched on, this AI arms your rivals to keep you busy.
   * It will not arm anyone who could pass the tech back to you
   * through an alliance or an embassy, and it will not arm anyone who
   * could then reach deep -- but the intent is what it looks like.
   *
   * 0.0 = off = no proxy is ever offered anything = byte-identical.
   * PLACEHOLDER. */
  double proxy_arm_gain;

  /* PEACE -- the embassy clause, which is arithmetically impossible
   * in the one ruleset where embassies matter most.
   *
   * civ2civ3 sets tech_leakage = "Embassies": an embassy IS the
   * research discount. Stock prices RECEIVING one at
   * `tech_leak_pct / 10` = 10 gold, flat, forever, and GIVING one at
   * DS_PEACE at `-5 * game.info.turn`, which grows without bound. A
   * mutual swap nets 10 - 5T each and is non-positive from turn 2
   * (upstream bug #46). It is moot anyway, because the only code path
   * that ever OFFERS an embassy clause is dai_share(), which is
   * alliance/team-gated -- so at peace the clause is never proposed
   * and would be refused if it were. Deep meanwhile pays diplomat
   * units and turns for the same thing (embassy_science_value).
   *
   * embassy_price_gain is a GATE and a residue in one number. At 0.0
   * deep does not touch embassy clauses and does not offer swaps:
   * today's behaviour exactly. Above 0.0 deep prices an embassy by
   * what the leakage rule says it is worth -- for each tech the other
   * player knows and we do not, the embassy takes
   * tech_leak_pct/players off that tech's price when we come to
   * research it (common/research.c:954-981), valued at the current
   * step's bulb cost and converted to gold at stock's own x3
   * (dai_goldequiv_tech, :158-179). Nothing here is invented: the
   * rate, the denominator and the conversion are all read from the
   * ruleset or from ai/default.
   *
   * embassy_give_gain is the weight deep puts on the RIVAL's side of
   * the same computation when giving. 1.0 says their discount hurts
   * exactly as much as ours helps, so a swap clears only for whoever
   * is behind on tech; below 1.0 deep is more willing to trade.
   * PLACEHOLDER.
   *
   * WHICH CHANNEL OWNS WHICH CASE: the TREATY channel (this pair of
   * knobs) owns every player deep is at peace-or-better with, because
   * there a clause can deliver the embassy for free. The DIPLOMAT
   * channel (embassy_science_value, deepdefence.c) owns everyone
   * else. With the gate on, the diplomat want stops counting players
   * the treaty channel can reach, so deep no longer builds a unit for
   * something a free clause delivers -- that is the composition, and
   * it is a change in the diplomat want that only happens when this
   * gate is open. */
  double embassy_price_gain;
  double embassy_give_gain;

  /* PEACE -- the three signals that are computed every turn and read
   * by nothing.
   *
   * `tech_leader` and `production_leader` are recomputed for every
   * player in advdata.c:486-500 and no line of ai/default or
   * ai/deep has ever read either; WIN_CAPITAL is written to
   * ai->diplomacy.strategy at daidiplomacy.c:880 and only WIN_SPACE
   * is ever read back. Deep computes its own leaders rather than
   * reading adv->dipl, because the stock loop uses players_iterate
   * (not _alive) with a strict `<` and can therefore name a DEAD
   * player (upstream bug candidate, same finding).
   *
   * leader_check_gain is the first and deliberately minimal consumer:
   * deep's operations layer biases its target choice toward the
   * runaway leader's cities by this fraction. 0.0 = off = today's
   * target ordering byte-identically. The real statecraft on top of
   * these signals -- dogpiling, peace-broking, counting a target's
   * existing enemies -- is BALANCE's, not this session's.
   * PLACEHOLDER. */
  double leader_check_gain;

  /* ENDGAME -- the game ends, and until now nothing in ai/ or
   * server/advisors/ knew it. `game.server.end_turn` is plain readable
   * state and has zero readers in the whole AI; MORT is a fixed 24;
   * 199 of 199 benchmark games end on the turn limit. So a Granary
   * started on turn 195 of 200 is priced with 24 turns of payback and
   * 24 turns of upkeep, neither of which will happen.
   *
   * endgame_horizon = 1 makes deep's growth ladder plan over
   * MIN(MORT, end_turn - turn) instead of MORT, on BOTH the benefit
   * and the upkeep side, and refuse a credit whose first instalment
   * lands after the last turn. It is a correctness clamp, not a
   * strategy: it is arithmetically inert on every turn more than MORT
   * from the end, which the `deep-endgame clamps=` counter proves per
   * player per turn. 0 = off = byte-identical.
   *
   * FAIRNESS, stated once: stock does not know the game ends either,
   * so this is deep learning something its opponent does not. It is a
   * benchmark-realism feature -- a human absolutely plays the bell in
   * a scored game -- and a real-game feature wherever `endturn` is
   * set at all. PLACEHOLDER. */
  int endgame_horizon;

  /* ENDGAME -- the score mode, and its window.
   *
   * `endgame_window` is how many of the final turns deep plays for the
   * scoreboard rather than for the empire: inside it, a build that
   * cannot be FINISHED before the bell earns nothing it can spend,
   * and a great wonder that can be finished is worth its five score.
   * 0 = off = byte-identical; the window is a PLACEHOLDER and its size
   * is measured, not argued.
   *
   * The two gains are a multiplier family over the existing want
   * scales -- RC4 discipline, no new want is minted:
   *
   * endgame_unfinished_gain multiplies a candidate whose
   * city_turns_to_build() exceeds the turns that remain. 1.0 is off
   * (today's behaviour, where an unfinishable build competes at full
   * price); 0.0 refuses it outright. It is a DEFLATOR and the
   * emergency tier is exempt, like every other deep gate: a city that
   * needs a defender this turn is not playing for score.
   *
   * endgame_wonder_gain multiplies a great wonder that CAN still be
   * finished. get_civ_score() pays 5 per great wonder against 1 per
   * citizen and 2 per tech, and a wonder is the one build whose whole
   * value lands in a single lump at completion -- so the last turns of
   * a scored game are exactly when it is worth the most and exactly
   * when deep, pricing it as a perpetual improvement, values it
   * least. 1.0 is off. Both PLACEHOLDERs. */
  int endgame_window;
  double endgame_unfinished_gain;
  double endgame_wonder_gain;

  /* DARKFX -- the effects audit's "dark" list: effects that carry real
   * money and return 0 in all three of stock's valuation venues
   * (findings/deep-dive-2026-08-05/economy-effects.md A2).
   *
   * horizon_govcenter_weight prices EFT_GOV_CENTER at what it saves
   * the REST of the empire. civ2civ3's Ecclesiastical Palace is the
   * concrete case: distance to the nearest government centre costs a
   * Republic city 2% of its trade and 2% of its shields per tile, so a
   * city twelve tiles from the Palace loses about half its trade, and
   * a second centre two tiles away cuts that to a fifth. Stock cannot
   * see any of it -- daieffects.c:176 files GOV_CENTER under "already
   * evaluated in base_want()", base_want() does not run for it (it is
   * not one of advdata.c's eleven trigger effects), and even if it did
   * the effect's requirement is City range, so a simulation would
   * measure an empire-wide benefit in one city.
   *
   * The credit is the counterfactual the ruleset's own waste formula
   * gives: for every OTHER city, the waste saved by moving its nearest
   * centre from where it is to this candidate, summed over outputs and
   * valued at the same output priorities the rest of this file uses,
   * over the planning horizon. 0.0 = off = byte-identical.
   * PLACEHOLDER. */
  double horizon_govcenter_weight;

  /* DARKFX -- the second ranked dark effect, and the cheapest
   * structural win in the audit: PLAYER-range effects, priced at the
   * empire they act on rather than at the city that houses them.
   *
   * EFT_UNIT_UPKEEP_FREE_PER_CITY is listed under "currently not
   * supported for building AI -- wait for modpack users"
   * (daieffects.c:588), which means King Richard's Crusade (2 free
   * gold of unit upkeep in EVERY city) and the Statue of Zeus (1 free
   * shield, likewise) have no want at all beyond History x 5 and a
   * Make_Content_Mil WAG. At twenty cities that is forty gold and
   * twenty shields a turn, forever, priced at fifteen and ten.
   *
   * The credit is what the empire is actually paying today that the
   * effect would stop paying: per city, the upkeep its supported units
   * really pay in that output, capped at the units the new free
   * allowance would cover. A wonder in an empire whose units cost it
   * nothing is correctly worth nothing. 0.0 = off = byte-identical.
   * PLACEHOLDER. */
  double horizon_upkeepfree_weight;

  /* DARKFX -- culture, the term the Europe derby re-sized from
   * "metric-gaming, report don't lead" to the plurality of the metric
   * (findings/europe-derby-hindsight.md N1).
   *
   * get_civ_score() is citizens + 2 x techs + 5 x wonders + built/10 +
   * kills/3 + culture/50, and culture's share of world score rises
   * monotonically with horizon: 7.9% at T100, 29.5% at T300, 42.5% at
   * T600. Nothing in deep prices any of its sources. In the derby deep
   * held 7 Temples to stock's 28 and generated 1.41 history a city per
   * turn to stock's 2.92, and the decomposed cohort gap was culture
   * -361 against noise everywhere else.
   *
   * The credit is the score arithmetic and nothing else: a building's
   * EFT_HISTORY accrues every turn between completion and the bell, so
   * it is worth h x turns-left / 50 score points, and deep already has
   * a want price for one score point -- the citizen, which scores
   * exactly one. The metric-chasing guard is structural rather than
   * argued: the horizon is the turns that actually remain, so the same
   * weight buys three score points at a 150-turn cell and twelve at a
   * 600-turn one, and shipping the weight small ships it small
   * everywhere.
   *
   * 1.0 therefore means "a culture point is worth what deep pays for
   * the citizen that scores the same point", which is an UPPER bound:
   * the citizen also works a tile and the culture point does not. The
   * real number is the tuning pile's. 0.0 = off = byte-identical.
   * PLACEHOLDER.
   *
   * Note for the stance work: this un-blocks the culture axis
   * deepstance.h declares inert. It is deliberately NOT wired here. */
  double horizon_culture_weight;

  /* DARKFX -- the rush-buy policy, the audit's third ranked
   * opportunity and the one that costs no new valuation at all.
   *
   * The greedy buy loop spends the treasury in want order and never
   * looks at the price (daicity.c:560, and deep inherited it). Buy
   * cost is QUADRATIC in the shields remaining for units, so the same
   * gold buys roughly four times the shields when it tops up a nearly
   * finished build instead of starting a bare one -- and the loop
   * cannot see the difference, because want is a property of the thing
   * being built and the discount is a property of how far along it is.
   *
   * buy_per_gold = 1 ranks the loop by want per gold instead. The
   * triage is deliberately untouched: a city whose urgency or
   * grave_danger says it is about to fall is ranked on want exactly as
   * before and outranks every bargain in the empire, because a
   * defender that arrives cheap next turn is worth nothing. 0 = off =
   * byte-identical. PLACEHOLDER. */
  int buy_per_gold;

  /* Whether a deep seat declines omniscience (W04 / RC6, epic S5).
   *
   * H_MAP and H_FOG are set at exactly one difficulty level -- AWAY,
   * the "not playing" mode (ai/difficulty.c:52-133) -- and H_TARGETS
   * stops at `normal`, so at `hard` and above the seat this project
   * measures has no information handicap at all. Everything in
   * findings/honest-information.md follows from that, including the
   * argument that a strategy layer evaluated under omniscience cannot
   * be evaluated at all.
   *
   * honest_info != 0 ORs H_MAP, H_FOG and H_TARGETS onto deep's own
   * player in deep's own phase hook -- the same shape as
   * pay_switch_penalty, and for the same reason: nothing in
   * ai/default, ai/baseline or the server changes, the seat simply
   * stops being exempt. It never CLEARS a bit, so 0 means "deep adds
   * nothing" rather than "deep guarantees omniscience", and an AWAY
   * seat keeps the handicaps its level gave it either way.
   *
   * What this closes: the ~18 handicap-gated sites in deep
   * (pathfinding omniscience, the site-eval unknown-tile gate, the
   * explorer discovery term, the settler overnight veto) plus the ~40
   * forwarded stock gates. What it does NOT close, and cannot: every
   * ground-truth read that carries no gate at all -- the survival and
   * take-odds census over rival unit lists, enemy-city `worth`, the
   * garrison-menace score ratio, and the whole of stock's Tier-3
   * strategic layer (dai_war_desire, daitech.c, daidata.c). Those are
   * inventoried with file:line in
   * findings/omniscience-census-2026-08-11.md; the flag is the
   * measuring instrument for the ones that ARE gated, not a fix.
   *
   * 1 is therefore expected to make deep WORSE, and per that census
   * worse in a specific direction: deep's gates mostly fail
   * dangerously optimistic (an unseen wave reads as no wave), where
   * stock's fail passive. The knob exists so the breakage can be
   * measured rather than argued. 0 = off = every measurement ever
   * taken. STRUCTURAL EXPERIMENT FLAG, not a tuning target. */
  int honest_info;
};

void deep_tuning_init(void);
enum log_level deep_metric_log_level(void);
const struct deep_tuning *deep_tuning_get(void);

/* The per-seat layer; see the loading scheme at the top of this file. */
const struct deep_tuning *deep_tuning_for(const struct player *pplayer);
void deep_tuning_player_init(const struct player *pplayer);
void deep_tuning_player_free(const struct player *pplayer);
const struct deep_tuning *deep_tuning_scope_set(const struct player *pplayer);
void deep_tuning_scope_restore(const struct deep_tuning *saved);
bool deep_tuning_arena_active(void);
int deep_tuning_scopeless_reads(void);

#endif /* FC__DEEPTUNING_H */
