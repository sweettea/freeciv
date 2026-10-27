# A1 expansion / sites / settler risk / overseas fear / explore: tuning-knob inventory

Facts only. Slice: the 42 knobs in `scratchpad/knobs/A1-expansion-sites.txt`. Source of truth at inventory time: working tree of branch `epic/w03-horizon-content` (HEAD b42d64d7a8). Line numbers are for that tree. "Compiled defaults" = `deeptuning.c` initialisers, which equal `tuning-params.ini` for every knob here. "Live" = the code path executes on at least some turns of a default game; "identity" = executes but arithmetic is a no-op at the default value.

Unit conventions: site values are in the "tile-sum" want/score currency (weighted tile yields amortized by citizen-arrival turn; compared against site_enough_result=250); "danger units" are the stock assess_danger scale added to `city_data->danger`.

## Summary

| knob | unit | default | live at defaults? | tier | screen verdict | prior A/B | # call sites |
|---|---|---|---|---|---|---|---|
| settler_fearfulness_mult | multiplier on pf danger cost | 2.0 | yes | B (BCIVIL) | tier-B, ~892 fires/game, no flip count | positive (cities, n=5) | 1 |
| settler_risk_tolerance | multiplier on defence in veto bar | 1.0 | yes (=stock bar) | unranked | not screened (instrumented, no flip) | none | 1 |
| settler_guard_tolerance | multiplier on defence in veto bar | 1.0 | yes (trade site dead) | unranked | not screened (no instrument) | none | 2 |
| site_enough_result | want/score units | 250.0 | yes | unranked | not screened (no instrument) | none | 2 |
| site_defence_weight | percent of site value | 0.0 | no (value 0) | unranked | not screened (LOG_VERBOSE) | negative | 2 |
| site_naval_gate | boolean gate | 0 | no (value 0) | unranked | not screened | negative | 1 |
| site_wait_discount | multiplier on wait turns | 0.0 | no (value 0) | unranked | not screened (zero firings) | null | 1 |
| site_naval_weight | percent of site value | 20.0 | yes | B (BSITE) | tier-B, ~367 fires/game | none | 1 |
| site_naval_connect_weight | percent of site value | 0.0 | no (value 0) | unranked | not screened (zero at default) | none | 2 |
| founder_pool_per_site | count per site | 0.06 | yes | A (DEFOPS) | flips 9.9/game (9.6%) | none | 2 |
| committed_fear_mult | multiplier on fear | 1.0 | identity at 1.0 | unranked | not screened (no instrument) | none | 1 |
| incoming_defence_eta_weight | 1/turn coefficient | 0.5 | yes | unranked | not screened (no instrument) | none | 2 |
| incoming_defence_survival | probability per turn | 0.9 | yes | unranked | not screened (no instrument) | none | 2 |
| explore_hut_value | want units per hut | 15.0 | yes iff huts>0 | unranked | not screened (direct instrument, no flip) | none | 3 |
| explore_discovery_value | want units per unknown tile | 0.05 | no (multiplies forced-0 unknown) | unranked | structurally zero | none | 1 |
| explore_explorer_claim | count (huts per explorer) | 1.0 | yes iff huts>0 | B (BCIVIL) | tier-B, ~698 fires/game | null | 3 |
| explore_salvage_gap | count (explorers) | 1.0 | rarely (never fired) | unranked | not screened (proxy) | null | 1 |
| explore_retain_floor | count (explorers) | 1 | rarely (never fired) | unranked | not screened (proxy) | null | 2 |
| site_stream_weight | multiplier on stream | 1.0 | yes | B (BSITE) | tier-B (proxy) | none | 1 |
| land_pressure_tiles | tiles/player | 200 | yes (via rival terms) | unranked | not screened (no instrument) | none | 1 |
| site_potential_discount | multiplier on potentials | 0.5 | yes | B (BSITE) | tier-B (proxy) | negative (stale) | 1 |
| site_special_bonus | want units per resource tile | 10.0 | yes | unranked | not screened (no instrument) | none | 1 |
| site_special_cluster | want units per extra resource | 10.0 | yes (needs >=2 specials) | unranked | not screened (no instrument) | positive (stale, venue-split) | 1 |
| site_claim_bonus | want units per foreign tile | 8.0 | yes (needs foreign tiles) | unranked | not screened (no instrument) | none | 1 |
| site_pressure_factor | multiplier 0-1 on potentials | 0.3 | yes (pressured sites) | unranked | not screened (no instrument) | none | 1 |
| site_travel_discount | multiplier on travel turns | 0.9 | yes | B (BSITE) | tier-B (proxy) | none | 1 |
| site_travel_crowding | multiplier on map crowding | 0.0 | identity at 0 | unranked | not screened | none | 1 |
| site_potential_weight | multiplier on potentials | 1.5 | yes | B (BSITE) | tier-B (proxy) | none | 2 |
| site_settler_cost_weight | multiplier on shield cost | 1.0 | yes | B (BSITE) | tier-B (proxy) | none | 2 |
| site_boat_cost_weight | multiplier on shield cost | 0.333333 | yes (overseas leg only) | unranked | not screened (direct instrument) | none | 2 |
| site_rival_pressure | want units per contest term | 10.0 | yes (same-continent rivals) | unranked | not screened (no instrument) | none | 3 |
| site_rival_crowding | multiplier 0-1 on land pressure | 0.9 | yes | unranked | not screened (no instrument) | none | 2 |
| site_own_overlap | want units per shared tile | 0.0 | no (value 0) | unranked | not screened (zero at default) | none | 2 |
| expansion_war_survival | probability 0-1 | 0.9 | yes (war footing only) | A (DEFOPS) | flips 15.3/game (14.6%) | positive (n=8, directional) | 3 |
| expansion_gate_enabled | boolean mode | 1 | no (only =0 branch) | unranked | MODE | none | 1 |
| overseas_fear | multiplier on attack rating | 0.5 | yes | unranked | not screened | none | 2 |
| overseas_decay | 1/tile coefficient | 0.2 | yes | unranked | not screened | none | 1 |
| boat_shadow_fear | danger units (flat) | 25 | yes (city); trunk site dead | unranked | not screened | none | 4 |
| boat_shadow_gate | boolean gate | 0 | no (value 0) | unranked | not screened | null | 1 |
| boat_shadow_distance | multiplier on hull reach | 1.0 | no (behind boat_shadow_gate=0) | B (BANTICIP) | dead behind gate | none | 1 |
| boat_shadow_count | count (hulls) | 1 | no (behind boat_shadow_gate=0) | B (BANTICIP) | dead behind gate | none | 1 |
| settler_rush_weight | multiplier on map crowding | 1.0 | yes | unranked | not screened (no instrument) | none | 1 |

## Per-knob sections

### settler_fearfulness_mult
- **Declared:** deeptuning.h:92 (`double settler_fearfulness_mult;`); params.ini: default=2.0 min=0.5 max=8.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Settler pathfinding fearfulness as a multiple of the advisors' NORMAL_STACKING_FEARFULNESS. Kept above 1: losing a settler also wastes every turn already spent walking (W05)."
- **Unit:** multiplier (dimensionless) on `NORMAL_STACKING_FEARFULNESS` (= PF_TURN_FACTOR/36, advgoto.h:23). It becomes `risk_cost->fearfulness` in adv_avoid_risks(); the pf per-tile extra cost is `base_value (settler build shields) * p_killed * fearfulness` (advgoto.c stack_risk), i.e. pf extra-cost units (1/PF_TURN_FACTOR of a turn per EC point) — a path-cost price, not a want.
- **Formula at the primary call site:** deepsettler.c:403-405 deep_settler_apply_risk(): `adv_avoid_risks(parameter, &risk->base, punit, deep_tuning_get()->settler_fearfulness_mult * NORMAL_STACKING_FEARFULNESS);`
- **All call sites:**
  - deepsettler.c:404 — deep_settler_apply_risk — scales the danger pricing in every settler pf map (site search, goto, join-city search) — yes
- **Log line:** none for the multiplier itself (knobcensus proxy: `deep-settler`, `deep-siteladder best_raw/best_net` show the risk-priced travel only indirectly)
- **Interacts with:** settler_risk_tolerance (same pf parameter: veto vs price), site_travel_discount, frontier_transit_gate (chains get_EC after this)
- **Tier/screen:** tier B (BCIVIL); knob-screen tier-B list: ~892 instrument firings/game, flip not computable offline
- **Prior evidence:** STATUS.md ~1688-1695 — 7-arm site-weight probe, isolated-150, 5 seeds — "the only knob that moved [city count]": 133 -> 150 cities (want prevalence flat); no SE, no score — n=5. STATUS.md ~791 — campaign tunes (three boxes, bundled, not isolated) all raised it (2.7/4.4/5.8 vs 2.0) — not attributable. tuning-first-principles-2026-08-06.md:51 proposes 1.5 (reasoning only, no arm).

### settler_risk_tolerance
- **Declared:** deeptuning.h:110 (`double settler_risk_tolerance;`); params.ini: default=1.0 min=0.5 max=6.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Multiplier on the overnight-danger bar for settler pathing (...a hard any-threat veto is too absolute...). The veto fires when enemy a^2*10 >= defence * THIS; 1.0 is exactly the stock bar, above 1.0 is braver."
- **Unit:** multiplier (dimensionless) on the settler's squared defence rating `d` in the integer-derived comparison `a*a*10 >= d*tolerance` (a = summed adjacent at-war attack ratings; d = adv_unit_def_rating_basic_squared * terrain-bonus).
- **Formula at the primary call site:** deepsettler.c:352 deep_settler_overnight_veto(): `return (double) (a * a * 10) >= (double) d * tolerance;` with tolerance read at deepsettler.c:368 in deep_civilian_pos_dangerous().
- **All call sites:**
  - deepsettler.c:368 — deep_civilian_pos_dangerous (pf is_pos_dangerous callback, installed by deep_settler_apply_risk) — veto on ending a turn at a tile — yes (1.0 = stock bar; `tolerance > 0` else falls to adv_danger_at; min 0.5 so branch is always taken)
- **Log line:** `deep-settler` (dangerdest>0, blocker=danger) — the refusals the veto causes (knobcensus: direct)
- **Interacts with:** settler_fearfulness_mult, settler_guard_tolerance (same function, different tolerance)
- **Tier/screen:** unranked (not in BCIVIL/BSITE); X/ECON/ALL membership only; knobcensus S3 direct instrument; not individually listed in the knob-screen shortlist
- **Prior evidence:** none (design mentions only: STATUS.md:1493, 1605; era-tech50-forensics-2026-08-11.md:365 notes the bar is quadratic in enemy attack and saturates at tech-50)

### settler_guard_tolerance
- **Declared:** deeptuning.h:120 (`double settler_guard_tolerance;`); params.ini: default=1.0 min=0.3 max=6.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "The same danger bar, but for asking for an ESCORT rather than for refusing to move... Lower than settler_risk_tolerance means 'ask for help before you would refuse to go'. PLACEHOLDER 1.0 = the stock bar = exactly the previous behaviour, so this starts inert."
- **Unit:** multiplier (dimensionless) on the same `d` as settler_risk_tolerance (same function deep_settler_overnight_veto).
- **Formula at the primary call site:** deepsettler.c:565-567 deep_settler_bodyguard_needed(): `if (deep_settler_overnight_veto(punit, ppath->positions[i].tile, deep_tuning_get()->settler_guard_tolerance)) { needed = TRUE; ...`
- **All call sites:**
  - deepsettler.c:567 — deep_settler_bodyguard_needed (walks planned path, first dangerous tile requests an escort via aiguard_request_guard) — yes, only on the can_walk branch and only if adv_danger_at(dest) was false
  - deeptrade.c:996 — deep_route_danger_fraction (fraction of caravan-route tiles that are dangerous) — no: only called from deeptrade.c:1263 under `trade_route_risk_gain > 0.0`, default 0.0
- **Log line:** none (bodyguard request is UNIT_LOG at LOG_DEBUG only, deepsettler.c:578; knobcensus: not instrumented)
- **Interacts with:** settler_risk_tolerance; trade_route_risk_gain (gates the deeptrade site)
- **Tier/screen:** unranked; no instrument (knobcensus NO-INSTRUMENT)
- **Prior evidence:** none (STATUS.md:2794 "inert placeholder reproducing stock"; landed 190d47afc9)

### site_enough_result
- **Declared:** deeptuning.h:131 (`double site_enough_result;`); params.ini: default=250.0 min=80.0 max=600.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Site score above which the on-continent search stops and never considers going overseas at all... Stock hard-codes RESULT_IS_ENOUGH = 250... PLACEHOLDER 250.0 reproduces stock exactly."
- **Unit:** want/score units — compared against `cr->result` / `best->result`, the site value in the weighted-tile-yield "tile-sum" currency after amortize() and after subtracting settler/boat shield costs (deepsite.c:2470-2478).
- **Formula at the primary call site:** deepsettler.c:1158 deep_find_best_city_placement(): `if (cr1 && cr1->result > deep_tuning_get()->site_enough_result) { return cr1; }`
- **All call sites:**
  - deepsettler.c:1158 — deep_find_best_city_placement — phase-1 result above bar skips the overseas (phase 2) search entirely — yes
  - deepsite.c:2531 — deep_settler_map_iterate — early break out of the pf scan (`best->result > bar && turns > move_rate && best_turn < turns`) — yes
- **Log line:** none for the threshold (knobcensus: the skip is a bare `return cr1`; a search that ends empty shows on `deep-siteladder` best_raw/best_net)
- **Interacts with:** every term that moves cr->result (site_stream_weight, site_travel_discount, site_settler_cost_weight, site_boat_cost_weight, site_naval_weight, site_own_overlap, site_rival_pressure ...)
- **Tier/screen:** unranked (X/ECON/ALL); not screened (no instrument)
- **Prior evidence:** STATUS.md:7543 — wiring check (isolated-150, 10 seeds, BEFORE vs WIRED scorelog sha256) — byte-identical 10/10 at default. STATUS.md ~20290/20576 — Europe derby: tuned files carry 145.42 (pv3) and 1600 (derby-7); probe says best NET scores are 300-400 so "the satisficing shortcut never fires in the Europe game" — liveness, not an A/B. No isolated arm.

### site_defence_weight
- **Declared:** deeptuning.h:157 (`double site_defence_weight;`); params.ini: default=0.0 min=0.0 max=80.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "What defensible ground is worth at a candidate site, as a PERCENTAGE of the site's own value paid per 100% of terrain defence"; registered 0-80 "for the tuning pile". Default 0 is "MEASURED off: D3 contested A/B".
- **Unit:** multiplier (dimensionless), expressed as percent: weight/100 times the site's own `result->total` times the terrain defence fraction `(defence-10)/10` (0 flat, 1.0 for +100%). Result is want/score units added to cr->total.
- **Formula at the primary call site:** deepsite.c:1806 deep_site_defence_bonus(): `return result->total * (deep_tuning_get()->site_defence_weight / 100.0) * ((defence - 10) / 10.0);`
- **All call sites:**
  - deepsite.c:1786 — deep_site_defence_bonus — early-out `<= 0.0` returns 0 (skips virtual-city creation) — no: dead at the compiled default 0.0 (the term is skipped entirely)
  - deepsite.c:1806 — deep_site_defence_bonus — adds premium for defensible ground to cr->total (called from deep_city_desirability, deepsite.c:2009) — no: only when site_defence_weight > 0
- **Log line:** `deep-sitedef` (LOG_VERBOSE only, so absent from default-verbosity corpora; carries factor=, total=, deep=, stock=)
- **Interacts with:** the site's own total (so site_stream_weight, site_special_*, etc.); site_naval_weight/site_naval_connect_weight are computed after it on the post-defence total
- **Tier/screen:** unranked; not screened (instrument at LOG_VERBOSE; knobcensus NO-INSTRUMENT, S3)
- **Prior evidence:** STATUS.md ~7425-7490 — D3 contested, 20 paired seeds, one binary, term ON(20) vs OFF(0): +2.43 for OFF (se 1.87, t +1.30, 13/20) — negative for raising it; "defense bonuses for cities basically don't matter long-term" (human prior). FINAL vs AFTER+site-defence-off.ini byte-identical 20/20 (proof the off path is clean). Tuning pass over 0-80 owed, never run.

### site_naval_gate
- **Declared:** deeptuning.h:181 (`int site_naval_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Refuse the maritime premium... to a site whose only water closes on its own continent... DEFAULT 0 (do not gate), and that is a MEASUREMENT, not caution": isolated-150, 20 paired seeds, gating scores 187.11 vs 198.79 ungated.
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepsite.c:2036-2038 deep_city_desirability(): `if (deep_tuning_get()->site_naval_gate > 0 && !deep_city_water_bridges(cr->tile)) { deep_site_naval_refused++; ... naval = 0; }`
- **All call sites:**
  - deepsite.c:2036 — deep_city_desirability — zero the coastal-yield premium when the adjacent water does not bridge off the continent — no: dead at the compiled default 0 (branch needs `> 0`; the premium `naval` itself is live via site_naval_weight)
- **Log line:** `deep-sitenaval` (refused=, paid=)
- **Interacts with:** site_naval_weight (the premium it can zero), site_naval_connect_weight
- **Tier/screen:** unranked (X/ECON/ALL); knobcensus S3 direct instrument but structurally zero at default
- **Prior evidence:** STATUS.md ~7584-7597 — isolated-150, 20 paired seeds, AFTER binary, gate ON vs OFF — -11.68 score (se 8.02, t -1.46, 7/20 wins); lift-arrived 17.25 vs 21.55 — n=20. Stated as "direction, not a claim".

### site_wait_discount
- **Declared:** deeptuning.h:199 (`double site_wait_discount;`); params.ini: default=0.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Weight on the EXPECTED WAIT when ranking an overseas candidate: the delivery delay enters the same amortize() as the travel delay, as site_wait_discount * deep_wait_expected(beachhead's water) turns. 1.0 charges the estimated wait at face value; 0.0 is off and is the A/B arm."
- **Unit:** multiplier (dimensionless) on wait TURNS (from deep_site_beach_wait), then cast to int and used as the exponent delay in amortize() (retention (23/24)^delay with MORT=24). Output is want/score units.
- **Formula at the primary call site:** deepsite.c:2462-2464 deep_settler_map_iterate(): `cr->result = amortize(cr->result, (int) (deep_tuning_get()->site_wait_discount * wait));`
- **All call sites:**
  - deepsite.c:2458/2464 — deep_settler_map_iterate — discounts overseas candidates (boat_cost > 0 only) by queueing delay; also reaches the colony_want probe path — no: dead at default 0.0 (needs `> 0.0`) and only for the overseas leg
- **Log line:** `deep-sitewait` (priced=, wait-turns; also `deep-waitspell`)
- **Interacts with:** site_travel_discount (same discounter), site_boat_cost_weight, lift_wait_tail_pct (shape of deep_wait_expected), colony_lift_weight
- **Tier/screen:** unranked (X/ECON/ALL); knobcensus S3 direct, zero firings at default
- **Prior evidence:** STATUS.md ~12864 — G2: isolated-150 holdout, pv2+wait=1.0 vs pv2, -54.6/seed (t -4.00, 16/20 worse, n=20) — WITHDRAWN (STATUS.md ~13558-13740 "W36-DIM": arm and control ran different binaries; seeds 105/108/111 never applied the discount). Same-binary 5-seed probe: -1.65, t -0.86 (null). The proper same-binary 20-seed gate is queued, not run. Liveness (w36-live, 2 seeds): 568 candidates discounted, mean 1.92 wait-turns.

### site_naval_weight
- **Declared:** deeptuning.h:213 (`double site_naval_weight;`); params.ini: default=20.0 min=0.0 max=60.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "what that premium is WORTH, as a percentage of the site's own value: stock's naval_bonus() pays a compile-time NAVAL_EMPHASIS (20)... PLACEHOLDER 20.0 reproduces stock operation for operation... 0 removes the premium." This half is coastal YIELD.
- **Unit:** multiplier (dimensionless), expressed as percent of the site's `result->total` (weight/100); result added to cr->total in want/score units.
- **Formula at the primary call site:** deepsite.c:1836-1837 deep_site_naval_bonus(): `return (result->total * (adv_want) deep_tuning_get()->site_naval_weight) / 100.0f;`
- **All call sites:**
  - deepsite.c:1836 — deep_site_naval_bonus — premium for any site adjacent to TC_OCEAN, added at deepsite.c:2045 — yes
- **Log line:** `deep-sitenaval` (paid=)
- **Interacts with:** site_naval_gate, site_naval_connect_weight (a split of one premium; both read the same base cr->total), site_defence_weight (applied before)
- **Tier/screen:** tier B (BSITE); knob-screen tier-B list: ~367 firings/game (flip not computable)
- **Prior evidence:** none isolated (STATUS.md:7559 "bigger unmeasured magnitude"; 7639 "tuning pass owed"). The connect/split design (STATUS.md ~12623-12735) keeps 20.0 unchanged.

### site_naval_connect_weight
- **Declared:** deeptuning.h:237 (`double site_naval_connect_weight;`); params.ini: default=0.0 min=0.0 max=20.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "The OTHER half of the maritime premium: additive option value for water that leads off this continent, again as a percentage of the site's own value, paid only where deep_city_water_bridges() says the water goes somewhere." PLACEHOLDER 0.0 = OFF.
- **Unit:** multiplier (dimensionless), percent of `result->total` (weight/100); result is want/score units added to cr->total.
- **Formula at the primary call site:** deepsite.c:1875-1877 deep_site_naval_connect_bonus(): `return (result->total * (adv_want) deep_tuning_get()->site_naval_connect_weight) / 100.0f;`
- **All call sites:**
  - deepsite.c:1866 — deep_site_naval_connect_bonus — early-out `<= 0.0` returns 0 (skips geometry) — no: dead at default 0.0
  - deepsite.c:1876 — deep_site_naval_connect_bonus — additive navigability premium for coastal sites whose water bridges continents — no: only when weight > 0
- **Log line:** `deep-sitenaval` (connect=, connectwant=)
- **Interacts with:** site_naval_weight, site_naval_gate
- **Tier/screen:** unranked (X/ECON/ALL); not screened (structurally zero at default)
- **Prior evidence:** none (liveness only: STATUS.md ~12735 "connect fired on 587/587 searches" in a tech65sea-150 probe with a non-default weight; A/B "owed")

### founder_pool_per_site
- **Declared:** deeptuning.h:261 (`double founder_pool_per_site;`); params.ini: default=0.06 min=0.0 max=0.5 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Founders to keep alive per SITE the unclaimed land could still hold (deep_remaining_sites()) before Join City may dissolve one... never big, because every one is two citizens and a shield bill." PLACEHOLDER 0.06 calibrated on primary-120; lategame-80 would want ~0.207.
- **Unit:** count (founders per remaining site). Product with deep_remaining_sites() is a founder count compared to an integer unit count.
- **Formula at the primary call site:** deepsettler.c:1929-1930 deep_settler_join_city(): `if (founders <= (int) (deep_tuning_get()->founder_pool_per_site * deep_remaining_sites(pplayer))) { return FALSE; /* Pool is at or under target. */ }`
- **All call sites:**
  - deepsettler.c:1926 — deep_settler_join_city — value computed only for the deep-pool log line — yes
  - deepsettler.c:1929 — deep_settler_join_city — gate: Join City may dissolve a founder only if founders exceed the pool target — yes
- **Log line:** `deep-pool` (sites=, target=)
- **Interacts with:** (deep_remaining_sites() — not a knob); founder_join_guard_gate / founder_join_guard_mult (the next refusal in the same function)
- **Tier/screen:** tier A (DEFOPS); knob-screen shortlist: flips 9.9/game, 9.6% flip rate, 104 decisions/game, replay rule founder_pool
- **Prior evidence:** STATUS.md ~7034/7103 — recalibration debt: after the item-5 reachability filter the target fell from 0.48-2.83 to 0.17-0.21 ("stale by ~13x in meaning") — the knob itself has not been A/B'd. The BEFORE vs AFTER1 arm (isolated-150 +1.46 se 4.38, n=20; cramped +7.93 se 17.37, n=10) moved the INPUT (sites), not this knob. screen-corpus-2026-08-30.md:192 re-confirms 9.90 flips/game.

### committed_fear_mult
- **Declared:** deeptuning.h:100 (`double committed_fear_mult;`); params.ini: default=1.0 min=1.0 max=3.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Survival-deficit fear multiplier for a city none of whose LAUNCHED assault members (MARCH onward) could reach it within survival_horizon... 1.0 = no posture effect."
- **Unit:** multiplier (dimensionless) on the survival-deficit fear, which is in danger units (unsigned int added to city_data->danger).
- **Formula at the primary call site:** deepdefence.c:2043-2050 deep_survival_deficit_fear(): `double away_mult = deep_committed_away(pcity) ? tune->committed_fear_mult : 1.0; fear = (unsigned int)(tune->survival_fear_value * (tune->survival_target - surv) / tune->survival_target * updisc * away_mult);`
- **All call sites:**
  - deepdefence.c:2044 — deep_survival_deficit_fear (called from deepdefence.c:2393 ledger seat and 5532 build seat) — scales survival-deficit fear when the assault army is away — live code, but identity at the compiled 1.0 (also needs survival_target>0 and survival_fear_value>0, both nonzero by default, and a launched assault op)
- **Log line:** none for the predicate (`deep-surv` logs the resulting fear= but not the away predicate)
- **Interacts with:** survival_fear_value, survival_target, survival_horizon, upkeep_unit_weight (via updisc)
- **Tier/screen:** unranked (M/MIL/ALL); no instrument (knobcensus S2, needs away=)
- **Prior evidence:** none (design mentions: STATUS.md:1500, 1604)

### incoming_defence_eta_weight
- **Declared:** deeptuning.h:266 (`double incoming_defence_eta_weight;`); params.ini: default=0.5 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "How steeply an en-route defender's contribution to its target city's effective danger reduction decays per turn of distance (W11): credit = power * survival^eta / (1 + eta_weight * eta)."
- **Unit:** coefficient per turn of ETA (1/turn; eta is `real_map_distance * SINGLE_MOVE / move_rate`, i.e. approx turns). The credit it scales is in danger units (assess_defense_unit power, subtracted from city danger).
- **Formula at the primary call site:** deepdefence.c:239-241 deep_incoming_defence_credit(): `credit += power * pow(tune->incoming_defence_survival, eta) / (1.0 + tune->incoming_defence_eta_weight * eta);`
- **All call sites:**
  - deepdefence.c:241 — deep_incoming_defence_credit — credit for defenders en route (AIUNIT_DEFEND_HOME / ESCORT) — yes (when such units exist)
  - deepdefence.c:291 — deep_muster_reserve_credit — same formula for mustering assault members on the same continent — yes (when an assault op is in MUSTER)
  - (both credits are applied at deepdefence.c:2395 and 5534: `city_data->danger -= MIN(city_data->danger, credit)`)
- **Log line:** `deep-fear` credit= (site=ledger / site=build)
- **Interacts with:** incoming_defence_survival (same expression)
- **Tier/screen:** unranked (S/MIL/ALL); no formal instrument (knobcensus S2), though deep-fear now carries credit=
- **Prior evidence:** none

### incoming_defence_survival
- **Declared:** deeptuning.h:267 (`double incoming_defence_survival;`); params.ini: default=0.9 min=0.5 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** same block as incoming_defence_eta_weight — the per-turn survival probability of an en-route defender: "credit = power * survival^eta / (1 + eta_weight * eta)".
- **Unit:** probability 0–1 per turn of ETA, raised to the power eta (turns); multiplies danger-unit power.
- **Formula at the primary call site:** deepdefence.c:240 deep_incoming_defence_credit(): `* pow(tune->incoming_defence_survival, eta)`
- **All call sites:**
  - deepdefence.c:240 — deep_incoming_defence_credit — en-route defender credit — yes
  - deepdefence.c:290 — deep_muster_reserve_credit — mustering assault credit — yes
- **Log line:** `deep-fear` credit= (post-hoc)
- **Interacts with:** incoming_defence_eta_weight
- **Tier/screen:** unranked; no instrument (knobcensus S2)
- **Prior evidence:** none

### explore_hut_value
- **Declared:** deeptuning.h:1305 (`double explore_hut_value;`); params.ini: default=15.0 min=0.0 max=200.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Explorer production want (W06): build-want units per visible unclaimed hut and per unknown tile, spread over the fleet. Zero-gated when the map was generated without huts."
- **Unit:** want/score units per hut (build-want units: the result is compared to a city's best build want, `explore_want > city_data->choice.want`, deepdefence.c:9220).
- **Formula at the primary call site:** deepexplore.c:254-255 deep_explore_prize(): `return hut_weight * tune->explore_hut_value + unknown * tune->explore_discovery_value;`
- **All call sites:**
  - deepexplore.c:254 — deep_explore_prize — prize of visible huts (hut_weight = sum of per-hut weights, 1.0 or hut_risk_discount) — yes iff `wld.map.server.huts > 0` (server default 15; benchmark config hutfree.conf sets HUTS=0)
  - deepexplore.c:328 — deep_explore_want — scales the claim `claimed` so it rescales with the prize — same condition
  - deepexplore.c:387, 406 — deep_explore_surplus — `<= 0.0` disables salvage; divisor of `employable` — same condition (salvage also needs explorers > retain floor)
- **Log line:** `deep-explore` (hutw=, prize=, claimed=, want=)
- **Interacts with:** explore_discovery_value, explore_explorer_claim, explore_salvage_gap, explore_retain_floor, hut_risk_radius, hut_risk_discount, hut_risk_honest
- **Tier/screen:** unranked (G/GCIVIL/ECON/ALL); knobcensus S1 direct (`huts_vis>0`); not in the knob-screen shortlist
- **Prior evidence:** none isolated. tuning-first-principles-2026-08-06.md:22,56 — an earlier tune evolved it to 58 (proposal 50, reasoning only). STATUS.md ~12619/12733 — hut_risk_honest x explore_hut_value factorial "owed", not run.

### explore_discovery_value
- **Declared:** deeptuning.h:1306 (`double explore_discovery_value;`); params.ini: default=0.05 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** the second half of the explorer prize: "build-want units per ... unknown tile" (same comment block as explore_hut_value).
- **Unit:** want/score units per unknown reachable-continent tile.
- **Formula at the primary call site:** deepexplore.c:255 deep_explore_prize(): `+ unknown * tune->explore_discovery_value;    /* discovery */`
- **All call sites:**
  - deepexplore.c:255 — deep_explore_prize — discovery term — no: multiplies zero. deepexplore.c:243 forces `unknown = 0` unless the player has the H_MAP handicap, which is unset at the levels benchmarked (SKILL=hard) — so the term is structurally zero at defaults
- **Log line:** `deep-explore` (unknown=); knobcensus notes it is structurally zero
- **Interacts with:** explore_hut_value, explore_explorer_claim (the claim subtraction is applied to the whole prize)
- **Tier/screen:** unranked; knob-screen: no firing possible (STRUCTURALLY ZERO per knobcensus)
- **Prior evidence:** none (omniscience-flag-breakage.md:353,393 identifies it as the cheapest item to wake under W04; tuning-first-principles-2026-08-06.md:57 proposes 0.30, reasoning only)

### explore_explorer_claim
- **Declared:** deeptuning.h:1314 (`double explore_explorer_claim;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "How much of the visible prize one explorer already fielded or on order is expected to collect, in HUTS (so it rescales with explore_hut_value rather than fighting it)... what lets the want reach exactly zero... 0 restores [the hyperbola] behaviour."
- **Unit:** count — huts' worth of prize per explorer (dimensionless hut-equivalents); multiplied by explore_hut_value to become want units.
- **Formula at the primary call site:** deepexplore.c:325-329 deep_explore_want(): `double claimed = (explorers + queued) * tune->explore_explorer_claim * tune->explore_hut_value; double left = prize - claimed; ... want = left / (1 + explorers + queued);`
- **All call sites:**
  - deepexplore.c:327 — deep_explore_want — subtracts fielded/queued explorers' share of the prize before dividing by fleet — yes iff huts > 0
  - deepexplore.c:386 — deep_explore_surplus — `<= 0.0` disables salvage (returns 0) — same
  - deepexplore.c:406 — deep_explore_surplus — divisor in `employable` — same
- **Log line:** `deep-explore` (claimed=, saturated=)
- **Interacts with:** explore_hut_value, explore_salvage_gap, explore_retain_floor, explore_discovery_value
- **Tier/screen:** tier B (BCIVIL); knob-screen tier-B list: ~698 firings/game, flip not computable
- **Prior evidence:** STATUS.md ~6960-7000 (item 4) — isolated-150, AFTER1 vs AFTER3 (claim shape + salvage leg + floor, bundled), 20 paired seeds: +0.01 +/- 0.34 (t +0.04), 18/20 seeds bit-identical; peak explorers 12 -> 1; huts cell 20 seeds identical games. Null on score (mechanism fixed a tail event).

### explore_salvage_gap
- **Declared:** deeptuning.h:1322 (`double explore_salvage_gap;`); params.ini: default=1.0 min=0.0 max=8.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Slack in EXPLORERS between the point where the prize stops paying for another one and the point where standing ones are salvaged, so build and disband cannot alternate on one turn's arithmetic."
- **Unit:** count (explorers of hysteresis slack).
- **Formula at the primary call site:** deepexplore.c:408 deep_explore_surplus(): `surplus = explorers - (int) (employable + tune->explore_salvage_gap);`
- **All call sites:**
  - deepexplore.c:408 — deep_explore_surplus (consumer: deepdefence.c:7390 deep_spend_gold disbands that many idle explorers inside cities) — live code, but never reaches this line unless huts>0, claim>0, hut_value>0 and explorers > retain floor
- **Log line:** `deep-salvage` (surplus=, recovered=) — a proxy; the gap and the floor are not separable
- **Interacts with:** explore_retain_floor, explore_explorer_claim, explore_hut_value
- **Tier/screen:** unranked (G/GCIVIL/ECON/ALL); knobcensus S1 proxy
- **Prior evidence:** STATUS.md ~6980-7000: "The salvage leg is built and has never fired. Zero deep-salvage lines in every cell" — part of the bundled item-4 arm (+0.01 +/- 0.34, n=20 isolated-150).

### explore_retain_floor
- **Declared:** deeptuning.h:1323 (`int explore_retain_floor;`); params.ini: default=1 min=0 max=8 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "the number of explorers never salvaged, because island hut supply reopens as new landmasses are reached and W04's recon value lands on this same fleet."
- **Unit:** count (explorers).
- **Formula at the primary call site:** deepexplore.c:396, 409-410 deep_explore_surplus(): `if (explorers <= tune->explore_retain_floor) { return 0; } ... if (surplus > explorers - tune->explore_retain_floor) { surplus = explorers - tune->explore_retain_floor; }`
- **All call sites:**
  - deepexplore.c:396 — deep_explore_surplus — never salvage at or below the floor — yes (same preconditions as explore_salvage_gap)
  - deepexplore.c:409,410 — deep_explore_surplus — clamps the surplus — same
- **Log line:** `deep-salvage` (proxy)
- **Interacts with:** explore_salvage_gap, explore_explorer_claim, explore_hut_value
- **Tier/screen:** unranked; knobcensus S1 proxy
- **Prior evidence:** same bundled item-4 arm as explore_salvage_gap (+0.01 +/- 0.34, n=20); salvage never fired.

### site_stream_weight
- **Declared:** deeptuning.h:280 (`double site_stream_weight;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Scale of the catchment stream -- the discounted citizen-turns a site will earn (W02b) -- against the city-centre tile it is added to. 1.0 is the model as designed... This is the ONE free parameter of the catchment valuation."
- **Unit:** multiplier (dimensionless) on `stream` (weighted-tile-yield sums amortized by citizen arrival turn = "tile-sum currency"); result in want/score units. Also sets the currency in which site_own_overlap's derived 33.2 want/tile is denominated.
- **Formula at the primary call site:** deepsite.c:1332 deep_cityresult_fill(): `stream *= deep_tuning_get()->site_stream_weight;` followed by `result->total = result->city_center.tdc->sum + stream + wspecials;` (virtual city) or `stream + wspecials`.
- **All call sites:**
  - deepsite.c:1332 — deep_cityresult_fill — scales the catchment stream versus city centre and the wspecials terms — yes (every site evaluation)
- **Log line:** none direct (knobcensus proxy `deep-settler flag:pick`; `deep-sitecand total=` at LOG_VERBOSE)
- **Interacts with:** site_potential_discount, site_potential_weight, site_special_bonus, site_special_cluster, site_claim_bonus, site_rival_pressure, site_own_overlap, site_enough_result (thresholds in the same currency)
- **Tier/screen:** tier B (BSITE); no individual flip measure (knobcensus S3 proxy)
- **Prior evidence:** none (STATUS.md:2327 introduced as placeholder; 2431/2557/2642 "untuned at 1.0"). The W02b restructure itself (STATUS.md ~2330) read "Result: nothing" deep pre vs post, 10 paired seeds — not a knob move.

### land_pressure_tiles
- **Declared:** deeptuning.h:286 (`int land_pressure_tiles;`); params.ini: default=200 min=20 max=800 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Reference land-per-player for deep_land_pressure(), the scale at which land counts as half scarce: pressure = tiles / (tiles + land-per-player). Deliberately its own knob rather than reusing density_prior_tiles."
- **Unit:** tiles (land tiles per player); output of deep_land_pressure() is a probability-like ratio in (0,1).
- **Formula at the primary call site:** deepdefence.c:771-772 deep_land_pressure(): `return (double) tune->land_pressure_tiles / (tune->land_pressure_tiles + MAX(land_per_player, 1));`
- **All call sites:**
  - deepdefence.c:771-772 — deep_land_pressure — scarcity ratio; sole consumers are the two site_rival_crowding expressions at deepsite.c:1271 (observation) and 1283 (the real penalty) — yes (live while site_rival_pressure > 0 and site_rival_crowding > 0, both true by default)
- **Log line:** none (knobcensus S2: feeds an unlogged prior)
- **Interacts with:** site_rival_crowding, site_rival_pressure (density_prior_tiles is a separate knob by design)
- **Tier/screen:** unranked (X/ECON/ALL); no instrument
- **Prior evidence:** none

### site_potential_discount
- **Declared:** deeptuning.h:287 (`double site_potential_discount;`); params.ini: default=0.5 min=0.0 max=1.5 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** no dedicated comment (grouped under "Reference land-per-player..." block). In code (deepsite.c:896): "A tile is worth what it yields now plus a discounted share of what one standard improvement would add (W02 potential term)."
- **Unit:** multiplier (dimensionless) on `potentials[i]` (weighted-yield units: terrain irrigation/mining increment * food/shield priority * site_potential_weight), added to the tile's current `sums[i]` before amortize().
- **Formula at the primary call site:** deepsite.c:899-901 deep_catchment_stream(): `total += amortize(sums[i] + tune->site_potential_discount * potentials[i], arrive[i]);`
- **All call sites:**
  - deepsite.c:900 — deep_catchment_stream — improvement potential share per catchment tile — yes
- **Log line:** none (knobcensus proxy `deep-settler flag:pick`)
- **Interacts with:** site_potential_weight (the same product), site_pressure_factor (scales potentials when contested), site_stream_weight
- **Tier/screen:** tier B (BSITE); proxy only
- **Prior evidence:** STATUS.md:651 — early ablation (12 contested / 6 primary seeds, PRE-W02b formula): removing site_potential_discount and site_overlap_weight "at placeholder values are HARMFUL in both regimes (removing them: +4pp/+8pp and +3pp/+10pp)" — bundled with the since-deleted overlap weight and a formula that no longer exists; treat as stale. europe-T76.md:158-181 lists it among terms that "do not move the outcome".

### site_special_bonus
- **Declared:** deeptuning.h:288 (`double site_special_bonus;`); params.ini: default=10.0 min=0.0 max=60.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** no dedicated comment; code comment (deepsite.c:1192): "Explicit specials term: resources already speak through their yields, but a catchment holding several is worth more than the sum -- one set of buildings serves them all (W02)."
- **Unit:** want/score units per resource tile (`nspecials`) in the catchment, added to wspecials (same tile-sum currency as the stream).
- **Formula at the primary call site:** deepsite.c:1195-1198 deep_cityresult_fill(): `wspecials = nspecials * deep_tuning_get()->site_special_bonus + MAX(0, nspecials - 1) * deep_tuning_get()->site_special_cluster + nclaim * deep_tuning_get()->site_claim_bonus;`
- **All call sites:**
  - deepsite.c:1195 — deep_cityresult_fill — per-resource site bonus — yes
- **Log line:** none (nspecials not logged; knobcensus NO-INSTRUMENT)
- **Interacts with:** site_special_cluster, site_claim_bonus, site_rival_pressure, site_own_overlap (all added to the same wspecials)
- **Tier/screen:** unranked; no instrument
- **Prior evidence:** none isolated

### site_special_cluster
- **Declared:** deeptuning.h:289 (`double site_special_cluster;`); params.ini: default=10.0 min=0.0 max=60.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** no dedicated comment; same "catchment holding several [specials] is worth more than the sum" block.
- **Unit:** want/score units per resource tile beyond the first (`MAX(0, nspecials - 1)`).
- **Formula at the primary call site:** deepsite.c:1196-1197: `+ MAX(0, nspecials - 1) * deep_tuning_get()->site_special_cluster`
- **All call sites:**
  - deepsite.c:1197 — deep_cityresult_fill — cluster bonus — yes, but nonzero only when nspecials >= 2
- **Log line:** none
- **Interacts with:** site_special_bonus, site_claim_bonus
- **Tier/screen:** unranked; no instrument
- **Prior evidence:** STATUS.md:653 — early ablation (12 contested / 6 primary seeds, pre-W02b): "site_special_cluster helps contested (+3.6pp), hurts primary"; STATUS.md:793 — island tunes doubled it to ~21, land tune killed it at 1.4. Bundled/stale, no SE.

### site_claim_bonus
- **Declared:** deeptuning.h:403 (`double site_claim_bonus;`); params.ini: default=8.0 min=0.0 max=60.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Border contest terms (W07-lite, landed with W02): per catchment tile owned by a non-allied player (founding here steals or denies that space)."
- **Unit:** want/score units per catchment tile owned by a non-allied player (`nclaim`).
- **Formula at the primary call site:** deepsite.c:1198 deep_cityresult_fill(): `+ nclaim * deep_tuning_get()->site_claim_bonus;`
- **All call sites:**
  - deepsite.c:1198 — deep_cityresult_fill — bonus for founding into a rival's claimed catchment tiles — yes, but nonzero only if the catchment contains foreign-owned tiles (city_can_be_built_here() refuses foreign-owned center tiles; catchment tiles may be foreign)
- **Log line:** none (nclaim not logged)
- **Interacts with:** site_pressure_factor, site_rival_pressure (the counterweight), site_special_bonus
- **Tier/screen:** unranked; no instrument
- **Prior evidence:** none (darkfx-2026-08-06.md:240 "remains deep's only border credit")

### site_pressure_factor
- **Declared:** deeptuning.h:404 (`double site_pressure_factor;`); params.ini: default=0.3 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "the horizon compression under pressure -- how much of the potential/tail value survives when a non-allied city sits within contest range of the site."
- **Unit:** multiplier (dimensionless, 0–1) on a tile's `potentials[ncatch]` (weighted-yield units).
- **Formula at the primary call site:** deepsite.c:1105-1106 deep_cityresult_fill(): `if (pressured) { potentials[ncatch] *= deep_tuning_get()->site_pressure_factor; }`
- **All call sites:**
  - deepsite.c:1106 — deep_cityresult_fill — compress improvement-potential value when a non-allied city is within 4*city_radius_sq of the site — yes, but only for pressured sites
- **Log line:** none (the `pressured` predicate is not logged)
- **Interacts with:** site_potential_weight, site_potential_discount
- **Tier/screen:** unranked; no instrument
- **Prior evidence:** none (europe-T76.md:114 design discussion only)

### site_travel_discount
- **Declared:** deeptuning.h:362 (`double site_travel_discount;`); params.ini: default=0.9 min=0.75 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "site_travel_discount multiplies the travel time before amortize(), so with MORT=24 the retained value is (23/24)^(d*turns)." Human call 2026-08-02: "hold it a little BELOW 1, not at 1" (the settler already exists; the choice is a city in T turns versus no city). Range [0.75, 1.0] keeps the tuner from "delay is nearly free".
- **Unit:** multiplier (dimensionless) on travel TURNS (`turns = move_cost / move_rate`, real terrain-aware pf time); cast to int and used as amortize() delay. Output want/score units.
- **Formula at the primary call site:** deepsite.c:2424-2426 deep_settler_map_iterate(): `double d = deep_tuning_get()->site_travel_discount * MAX(relief, 0.0); cr->result = amortize(cr->total, (int) (d * turns));`
- **All call sites:**
  - deepsite.c:2424 — deep_settler_map_iterate — discount for the walk to a candidate site (both phase-1 and overseas legs) — yes
- **Log line:** none direct (knobcensus proxy `deep-settler flag:pick`; `deep-siteladder best_raw/best_net` bracket the travel+cost effect)
- **Interacts with:** site_travel_crowding (relief factor), site_wait_discount, site_settler_cost_weight, site_boat_cost_weight, site_stream_weight (scale of the thing discounted)
- **Tier/screen:** tier B (BSITE); proxy only. Also in GSITE.
- **Prior evidence:** none at 0.9 (settler-missions.md:176 "Not yet measured... should be measured on its own"). Historical: tuner-driven 0.654809 (b6) vs stock-equivalent d=3; the tail cost (9% of missions >40 turns, 80-turn cell) is in settler-missions.md:138-140. STATUS.md:5789-5817: float-roundtrip makes 0.9 load as 0.8999999761581421.

### site_travel_crowding
- **Declared:** deeptuning.h:363 (`double site_travel_crowding;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "REDUCES that [travel] discount as good sites get scarce... on a packed map the frontier is the only land left and the walk is the price of expanding at all."
- **Unit:** multiplier (dimensionless) on deep_map_crowding() (0..1, `MIN(1, density_prior_tiles / land_per_player)`); `relief = 1 - crowding_knob * crowding`, clamped at 0, multiplies site_travel_discount.
- **Formula at the primary call site:** deepsite.c:2422-2423 deep_settler_map_iterate(): `double relief = 1.0 - deep_tuning_get()->site_travel_crowding * deep_map_crowding();`
- **All call sites:**
  - deepsite.c:2422 — deep_settler_map_iterate — relaxes the travel discount on crowded maps — live code, identity at the compiled default 0.0 (relief = 1.0)
- **Log line:** none (relief folded into the discount, knobcensus NO-INSTRUMENT)
- **Interacts with:** site_travel_discount, density_prior_tiles
- **Tier/screen:** unranked (G/GSITE/ECON/ALL); no instrument
- **Prior evidence:** none

### site_potential_weight
- **Declared:** deeptuning.h:367 (`double site_potential_weight;`); params.ini: default=1.5 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Weight on a catchment tile's realistic improvement increment (irrigation or mine). Placeholder 1.5 is stock's effective PERFECTION/2 with integer truncation."
- **Unit:** multiplier (dimensionless) on `pot = MAX(irrigation_food_incr*food_priority, mining_shield_incr*shield_priority)` (weighted-yield units); output feeds potentials[] (then * site_potential_discount * pressure factor).
- **Formula at the primary call site:** deepsite.c:1104 deep_cityresult_fill(): `potentials[ncatch] = pot * deep_tuning_get()->site_potential_weight;`
- **All call sites:**
  - deepsite.c:1104 — deep_cityresult_fill — improvement potential per catchment tile — yes
  - deepsite.c:1100-1101 — same function — census-only `deep_site_imp_gain += (imp->gain - pot) * weight` — no: only under `site_improvable_gate > 0` (default 0), observation only
- **Log line:** none (knobcensus proxy `deep-settler flag:pick`)
- **Interacts with:** site_potential_discount (same product in deep_catchment_stream), site_pressure_factor, site_improvable_gate
- **Tier/screen:** tier B (BSITE); also GSITE
- **Prior evidence:** none isolated. STATUS.md ~22117 (siteplan): "re-level, then re-measure" owed after half A of the improvable gate lowered mean potential by -5.6 per tile at 1.5.

### site_settler_cost_weight
- **Declared:** deeptuning.h:371 (`double site_settler_cost_weight;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "What the settler itself costs, charged against the site's worth. Dimensionally awkward -- shields subtracted from weighted tile yield -- which is exactly why it should be tunable rather than 1."
- **Unit:** multiplier (dimensionless) on `cost` (= unit_build_shield_cost_base(punit), SHIELDS); the product is subtracted from cr->result, which is in want/score (tile-sum) units. Unit mismatch is declared in the header.
- **Formula at the primary call site:** deepsite.c:2475-2476 deep_settler_map_iterate(): `cr->result -= cost * deep_tuning_get()->site_settler_cost_weight + boat_cost * deep_tuning_get()->site_boat_cost_weight;`
- **All call sites:**
  - deepsite.c:2475 — deep_settler_map_iterate — net-of-cost site value that drives which site is best and whether it is > 0 (candidate dropped when result <= 0; counted as `neg` on deep-siteladder) — yes
  - deepsite.c:2477 — same — the `undiscounted` shadow value used only by the wait census — observation only
- **Log line:** `deep-siteladder` (neg=, best_net=) — the rung where settler cost eats the score
- **Interacts with:** site_boat_cost_weight, site_travel_discount, site_enough_result (compares post-cost value)
- **Tier/screen:** tier B (BSITE); also GSITE
- **Prior evidence:** none

### site_boat_cost_weight
- **Declared:** deeptuning.h:372 (`double site_boat_cost_weight;`); params.ini: default=0.333333 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** same block as site_settler_cost_weight; deepsite.c comment: "also penalise for using a boat. Both weights were stock's implicit 1 and 1/3; they subtract SHIELDS from a weighted tile-yield sum".
- **Unit:** multiplier (dimensionless) on `boat_cost` (SHIELDS of the ferry type; 0 on the on-continent leg); subtracted from want/score units. Same mismatch as site_settler_cost_weight.
- **Formula at the primary call site:** deepsite.c:2475-2476 (see above): `... + boat_cost * deep_tuning_get()->site_boat_cost_weight;`
- **All call sites:**
  - deepsite.c:2476 — deep_settler_map_iterate — boat penalty on overseas candidates — live in the code path, but a nonzero term only for overseas/virtual-boat legs (boat_cost > 0: deepsettler.c:1200 and 1282); zero on the phase-1 leg
  - deepsite.c:2478 — same — observation-only shadow value
- **Log line:** `deep-settler` (overseas>0, virt>0 — direct per knobcensus)
- **Interacts with:** site_settler_cost_weight, site_wait_discount, site_travel_discount
- **Tier/screen:** unranked (G/GSITE/ECON/ALL); knobcensus S3 direct
- **Prior evidence:** none

### site_rival_pressure
- **Declared:** deeptuning.h:413 (`double site_rival_pressure;`); params.ini: default=10.0 min=0.0 max=60.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Contested-continent penalty: non-allied cities on the SITE'S OWN landmass, weighted 1/(1+distance), subtracted from the site's worth. Counterweight to site_claim_bonus." Notes campaign-a6 raised it to 35.9 "at no cost in its two island cells, where the term contributes exactly zero".
- **Unit:** want/score units per unit of the contest sum `Σ 1/(1+real_map_distance)` over non-allied same-continent cities within 4*city_radius_sq of the site; subtracted from wspecials.
- **Formula at the primary call site:** deepsite.c:1281-1283 deep_cityresult_fill(): `wspecials -= contest * deep_tuning_get()->site_rival_pressure * MAX(0.0, 1.0 - deep_tuning_get()->site_rival_crowding * deep_land_pressure());`
- **All call sites:**
  - deepsite.c:1200 — deep_cityresult_fill — guards the whole contest block (`> 0.0`) — yes
  - deepsite.c:1281 — same — the actual penalty — yes, nonzero only if a non-allied city shares the site's continent within range
  - deepsite.c:1269 — same — `deep_site_contest_want` census (contest_ours, needs log level metric) — observation only
- **Log line:** none for the per-site contest (knobcensus: contest count not logged); observation counters `contest_ours` exist
- **Interacts with:** site_rival_crowding, land_pressure_tiles, site_claim_bonus, site_stream_weight (currency)
- **Tier/screen:** unranked (G/GSITE/ECON/ALL); no instrument
- **Prior evidence:** none isolated (tuning-venues-2026-08-06.md:14 "The 35.9 artifact": inflated freely in a campaign because it was zero in island cells). STATUS.md:2041 notes it contributes 0 on an island save.

### site_rival_crowding
- **Declared:** deeptuning.h:437 (`double site_rival_crowding;`); params.ini: default=0.9 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "How much of the contested-continent penalty is forgiven as the map fills: penalty becomes site_rival_pressure * MAX(0, 1 - crowding_factor * deep_map_crowding()). 0 disables and leaves the penalty flat." (Implementation actually uses deep_land_pressure(), per the comment's own later paragraph.)
- **Unit:** multiplier (dimensionless, 0–1) on deep_land_pressure() ∈ (0,1); the (1 − …) factor scales site_rival_pressure.
- **Formula at the primary call site:** deepsite.c:1282-1283: `MAX(0.0, 1.0 - deep_tuning_get()->site_rival_crowding * deep_land_pressure())`
- **All call sites:**
  - deepsite.c:1282 — deep_cityresult_fill — forgiveness of rival-proximity penalty on crowded maps — yes (only matters while site_rival_pressure > 0)
  - deepsite.c:1270 — same — observation-only census mirror
- **Log line:** none
- **Interacts with:** site_rival_pressure, land_pressure_tiles
- **Tier/screen:** unranked; no instrument
- **Prior evidence:** none

### site_own_overlap
- **Declared:** deeptuning.h:462 (`double site_own_overlap;`); params.ini: default=0.0 min=0.0 max=60.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "OWN-empire crowding: want charged per catchment tile of a candidate site that already lies inside the work radius of a city we hold... 0.0 = off, and that is the shipping default: the byte-identity gate is taken against it." Derived on-value 33.2 want per shared tile (265.8/8) from the T110 Europe probe.
- **Unit:** want/score units per shared catchment tile (`nshare`), same site-value currency as the stream; also multiplied by deep_stance_pox_mult(pplayer) (1.0 at identity axes).
- **Formula at the primary call site:** deepsite.c:1305-1309 deep_cityresult_fill(): `double mult = deep_stance_pox_mult(pplayer); double charge = nshare * deep_tuning_get()->site_own_overlap * mult; wspecials -= charge;`
- **All call sites:**
  - deepsite.c:1285 — deep_cityresult_fill — guard `site_own_overlap > 0.0 && nshare > 0`  — no: dead at default 0.0
  - deepsite.c:1309 — same — own-crowding charge — no: only when site_own_overlap > 0
- **Log line:** `deep-sitespace` (charged=, charge= — identically zero at default)
- **Interacts with:** stance_pox_gain (via deep_stance_pox_mult), site_stream_weight (currency), site_rival_pressure, site_claim_bonus
- **Tier/screen:** unranked (G/GSITE/ECON/ALL); knobcensus S3 direct, zero at default
- **Prior evidence:** none (derivation only: STATUS.md:20368-20450; no A/B)

### expansion_war_survival
- **Declared:** deeptuning.h:479 (`double expansion_war_survival;`); params.ini: default=0.9 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Survival probability below which a city on war footing stops planning settlers and workers. Stock zeroes both for EVERY city whenever any war countdown exists anywhere... 0 restores stock exactly."
- **Unit:** probability 0–1 (threshold compared against `surv`, deep_city_stack_survival()'s survival probability over survival_horizon).
- **Formula at the primary call site:** deepdefence.c:4941-4942 deep_expansion_blocked(): `surv = deep_city_stack_survival(&(wld.map), pcity, tune->survival_horizon, &wave); blocked = (wave > 0 && surv < tune->expansion_war_survival);`
- **All call sites:**
  - deepdefence.c:4927 — deep_expansion_blocked — `<= 0.0` routes to stock's dai_on_war_footing() (returns `footing`) — no: only at 0 (non-default arm)
  - deepdefence.c:4942 — same — the deep gate: blocked only when on war footing, a hostile wave exists and surv < threshold — yes, but live only on war-footing city-turns (`!footing` returns FALSE earlier, deepdefence.c:4933)
  - deepdefence.c:4924/4929/4934/4944 — same — arguments to deep_expgate_log (logging only)
  - consumer: deepdefence.c:9206-9215 deep_manage_cities — when blocked and the city's military `choice.want > 0`: `worker_want = 0; founder_want = 0`
- **Log line:** `deep-expgate` (surv=, thresh=, blocked=, wave=)
- **Interacts with:** expansion_gate_enabled (master switch), survival_horizon, survival_target (separate threshold, different arbitration)
- **Tier/screen:** tier A (DEFOPS); knob-screen shortlist: flips 15.3/game, 14.6% flip rate, 105 decisions/game, replay rule expansion_gate
- **Prior evidence:** STATUS.md ~11068-11085 and findings/era-probe-evidence-2026-08-12.md:265-290 (ERA stage 2) — tech50war-200, mixed seat-mirrored, 8 seeds, arm A (0.9) vs arm B (0.0 = stock's war-footing gate): B is -148.9/seed (SD 229.8, SEM 81.2, t -1.83); B blocks 3.6x more and founds 19 fewer cities — directional support for 0.9 over stock's gate, n=8. screen-corpus-2026-08-30.md:190 re-confirms 15.25 flips/game.

### expansion_gate_enabled
- **Declared:** deeptuning.h:496 (`int expansion_gate_enabled;`); params.ini: default=1 min=1 max=1 kind=int (degenerate range: tuner never samples it)
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Whether deep asks the expansion question at all. 1 (default, and what ships) runs the two regimes... 0 means NO GATE -- deep_expansion_blocked() is FALSE on every city-turn." Pinned by hand per experiment, like stance_forced.
- **Unit:** boolean gate (mode switch).
- **Formula at the primary call site:** deepdefence.c:4922-4926 deep_expansion_blocked(): `if (tune->expansion_gate_enabled == 0) { deep_expgate_log(..., -2, -1.0, ...); return FALSE; }`
- **All call sites:**
  - deepdefence.c:4922 — deep_expansion_blocked — never-block arm — no: only when set to 0 (off-arm, never part of a benchmark default)
- **Log line:** `deep-expgate` with wave=-2 (appears only when the knob is 0)
- **Interacts with:** expansion_war_survival
- **Tier/screen:** unranked (G/GSITE/ECON/ALL); knobcensus MODE (empty live list at defaults is expected)
- **Prior evidence:** none as an A/B (STATUS.md ~11416-11480: identity 10/10 byte-identical at default; branch live 1693 wave=-2 lines/game on 2 seeds; smoke games identical; "can only be priced on a war venue", the third arm was never run).

### overseas_fear
- **Declared:** deeptuning.h:514 (`double overseas_fear;`); params.ini: default=0.5 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Fear of enemies the stock danger assessment cannot see: at-war military power on other continents (or at sea) adds to a coastal city's danger, scaled by fear and decayed per tile of straight-line distance."
- **Unit:** multiplier (dimensionless) on `adv_unit_att_rating(aunit)` of at-war units embarked on a boat within reach; result is danger units (unsigned int added to city_data->danger).
- **Formula at the primary call site:** deepdefence.c:393-394 deep_overseas_danger(): `danger += rating * tune->overseas_fear / (1.0 + tune->overseas_decay * dist);`
- **All call sites:**
  - deepdefence.c:365 — deep_overseas_danger — early return 0 if `overseas_fear <= 0.0` or the city is not ocean-adjacent — yes
  - deepdefence.c:393 — deep_overseas_danger — danger from embarked cargo within ferry reach (`move_rate/SINGLE_MOVE + 1` tiles) — yes, only while hostile loaded boats are in range
  - consumed at deepdefence.c:2348/2384 (ledger seat) and 5493/5526 (build seat): `danger += over * updisc * sat * fmult`
- **Log line:** `deep-fear` over= (site=ledger / site=build); knobcensus predates the field and lists NO-INSTRUMENT
- **Interacts with:** overseas_decay, upkeep_unit_weight (updisc), satisfied_fear_discount (sat), stance_fear_gain (fmult), boat_shadow_fear (sibling term)
- **Tier/screen:** unranked (S/MIL/ALL); not screened (post-injection danger not logged at screen time)
- **Prior evidence:** none isolated. STATUS.md:797 — island-native tunes (H, H2) both zeroed it ("pure tax" on island regimes; bundled campaign output, no SE). tune-postfix-verdict-2026-08-25.md:49 — `overseas_fear=0` listed among 76 params pinned at a bound in the rejected final vector (holdout -52/-33): consistent with noise.

### overseas_decay
- **Declared:** deeptuning.h:515 (`double overseas_decay;`); params.ini: default=0.2 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** the "decayed per tile of straight-line distance" half of the overseas_fear comment.
- **Unit:** coefficient per tile of real_map_distance (1/tile).
- **Formula at the primary call site:** deepdefence.c:394: `/ (1.0 + tune->overseas_decay * dist)`
- **All call sites:**
  - deepdefence.c:394 — deep_overseas_danger — distance decay of embarked-cargo fear — yes (inside the same branch as overseas_fear)
- **Log line:** `deep-fear` over= (post-hoc)
- **Interacts with:** overseas_fear
- **Tier/screen:** unranked (X/ECON/ALL); not screened
- **Prior evidence:** none

### boat_shadow_fear
- **Declared:** deeptuning.h:597 (`int boat_shadow_fear;`); params.ini: default=25 min=0 max=200 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Standing coastal fear while any at-war player owns transport-capable ships: the embarked-cargo term warns ~1 turn ahead, the boat shadow funds the garrison before the boats load. Also vetoes grow-hard on exposed coasts via the CM danger gate."
- **Unit:** danger units (flat integer added to a coastal city's danger; same scale as the other injected fears, multiplied by updisc*sat*fmult).
- **Formula at the primary call site:** deepdefence.c:486-487 deep_boat_shadow_fear(): `return (deep_boat_shadow_afloat(city_owner(pcity)) ? tune->boat_shadow_fear : 0);` (gate=0 form); with gate>0 `fear = (unsigned int)(boat_shadow_fear * scale)` at :474.
- **All call sites:**
  - deepdefence.c:461 — deep_boat_shadow_fear — early return 0 when `<= 0` or city not ocean-adjacent — yes
  - deepdefence.c:487 — same — flat term when any at-war player owns a transport — yes
  - deepdefence.c:474 — same — scaled term — no: only under boat_shadow_gate > 0
  - deeptrunk.c:630,635 — deep_trunk_tile_risk (corridor coastal risk) and deeptrunk.c:1028 (`afloat` precondition) — no: contributes only when `trunk_risk_weight > 0.0` (default 0.0)
  - consumed at deepdefence.c:2349/2385 (ledger) and 5494/5527 (build): `danger += shadow * updisc * sat * fmult`
- **Log line:** `deep-fear` shadow= (post-hoc, both seats); `deep-shadow` only under the gate
- **Interacts with:** boat_shadow_gate, boat_shadow_distance, boat_shadow_count, overseas_fear, trunk_risk_weight
- **Tier/screen:** unranked (S/MIL/ALL); not screened at screen time
- **Prior evidence:** none moving the magnitude. STATUS.md:23493 (S-FEARSEAT) shows the flat term fires on 42.1% of T51-75 build-seat evaluations vs 3.3% with the gate on (13 051 coastal city-turns, 81.2% of firings had no hull within reach); derby-7 zeroes it (STATUS.md ~18430).

### boat_shadow_gate
- **Declared:** deeptuning.h:616 (`int boat_shadow_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "The boat shadow's GEOMETRY, off by default (gate 0 keeps the flat term exactly as it shipped). With the gate open ... a hostile transport counts against this city only while it is within boat_shadow_distance of its own one-move reach... boat_shadow_count hulls inside that ring are what a landing takes."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepdefence.c:466-476 deep_boat_shadow_fear(): `if (tune->boat_shadow_gate > 0) { unsigned int hulls = deep_boat_shadow_hulls(...); int need = MAX(tune->boat_shadow_count, 1); double scale = MIN(1.0, (double) hulls / need); unsigned int fear = (unsigned int) (tune->boat_shadow_fear * scale); ...`
- **All call sites:**
  - deepdefence.c:466 — deep_boat_shadow_fear — switches the flat world-wide step function to the reachability-scaled term — no: dead at default 0
- **Log line:** `deep-shadow` (hulls=, need=, scale=, fear=, afloat=)
- **Interacts with:** boat_shadow_fear, boat_shadow_distance, boat_shadow_count
- **Tier/screen:** unranked (S/MIL/ALL; one of the five "gates" in the S-STACKED stack); no flip measure (off at screen time)
- **Prior evidence:** S-FEARSEAT (STATUS.md ~23493, contested / lategame-80, 20 paired training seeds): +0.00 +/- 0.74 (8/20) / -0.71 +/- 0.90 (6/20), unitslost down in both venues. S-MARGINAL re-read (STATUS.md ~24349): contested observed -0.46 +/- 0.85 vs predicted +0.04 +/- 0.35. S-STACKED (STATUS.md ~24398+, n=100 x 2 venues, five gates incl. this one on vs off): war +2.39 +/- 1.02 score, econ +4.12 +/- 0.61 — not attributable to this gate. Verdict: score-null, correctness fix.

### boat_shadow_distance
- **Declared:** deeptuning.h:617 (`double boat_shadow_distance;`); params.ini: default=1.0 min=0.25 max=6.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "boat_shadow_distance is measured in HULL REACHES, not tiles, so it keeps its meaning across rulesets and across the trireme-to-transport ladder."
- **Unit:** multiplier on a hull's own reach in tiles: `reach = (int)(distance * (move_rate/SINGLE_MOVE + 1))` (tiles after cast); dimensionless in knob terms.
- **Formula at the primary call site:** deepdefence.c:521-522 deep_boat_shadow_hulls(): `reach = (int) (tune->boat_shadow_distance * (ut->move_rate / SINGLE_MOVE + 1)); if (real_map_distance(unit_tile(aunit), ptile) <= reach) { hulls++; }`
- **All call sites:**
  - deepdefence.c:521 — deep_boat_shadow_hulls — range ring for counting hostile hulls — no: only reached from deep_boat_shadow_fear under boat_shadow_gate > 0 (default 0); the function's only caller is deepdefence.c:470
- **Log line:** `deep-shadow` (hulls=) only under the gate
- **Interacts with:** boat_shadow_gate, boat_shadow_count, boat_shadow_fear
- **Tier/screen:** tier B (BANTICIP); knob-screen: dead behind gate
- **Prior evidence:** none separately (STATUS.md ~23535 lists it PLACEHOLDER 1.0, owed to tuning; only moved as part of boat_shadow_gate=1 at its default)

### boat_shadow_count
- **Declared:** deeptuning.h:618 (`int boat_shadow_count;`); params.ini: default=1 min=1 max=8 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "boat_shadow_count hulls inside that ring are what a landing takes -- fewer scale the fear down linearly, more do not scale it up."
- **Unit:** count (hulls).
- **Formula at the primary call site:** deepdefence.c:472-473 deep_boat_shadow_fear(): `int need = MAX(tune->boat_shadow_count, 1); double scale = MIN(1.0, (double) hulls / need);`
- **All call sites:**
  - deepdefence.c:472 — deep_boat_shadow_fear — hull count that saturates the fear — no: only under boat_shadow_gate > 0
- **Log line:** `deep-shadow` (need=) only under the gate
- **Interacts with:** boat_shadow_gate, boat_shadow_distance, boat_shadow_fear
- **Tier/screen:** tier B (BANTICIP); dead behind gate; STATUS.md ~23535: "boat_shadow_count (1, axis untested)"
- **Prior evidence:** none (axis untested; at the default 1 it is a no-op scale of 1.0 whenever any hull is in range)

### settler_rush_weight
- **Declared:** deeptuning.h:812 (`double settler_rush_weight;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Land-race coupling: fresh founder wants scale by (1 + weight * map crowding); crowded maps race, empty ones don't. 0 disables."
- **Unit:** multiplier (dimensionless) on deep_map_crowding() (0..1); applied to `founder_want` (adv_want/int build-want units).
- **Formula at the primary call site:** deepdefence.c:9265-9268 deep_manage_cities(): `city_data->founder_want = (int) (city_data->founder_want * (1.0 + deep_tuning_get()->settler_rush_weight * deep_map_crowding()));`
- **All call sites:**
  - deepdefence.c:9260,9267 — deep_manage_cities — scales a freshly computed founder_want (not cached ones) — yes (nonzero effect scales with crowding: crowding is 1.0 at or below density_prior_tiles land per player, so at most x2 at weight 1.0)
- **Log line:** none (knobcensus: post-rush founder want not logged; `deep-def founder=` is the pre-rush value)
- **Interacts with:** density_prior_tiles (via deep_map_crowding), stance_expand_gain (applied right after), scale_settler
- **Tier/screen:** unranked (X/ECON/ALL); no instrument
- **Prior evidence:** none (europe-T80.md:117 "at most about 2x"; difficulty-audit notes the multiplier is upstream of the advisor divisor)


## Unresolved

- Screen verdicts: the per-knob inert/dead-path/flips classification was not recomputed. `benchmark/knobscreen.py --md` on the s6-nogates corpus did not complete in the session (stderr: `bad predicate: 'slack'`, empty output). The "screen" column therefore uses only (a) the shortlist and tier-B tables in knob-screen-2026-08-30.md / screen-corpus-2026-08-30.md and (b) `benchmark/knobcensus.py` MAP entries (instrument/no-instrument). A knob marked "not screened" may still be classed inert/dead-path in the full 277-knob list.
- Several knobcensus MAP entries (e.g. overseas_fear, boat_shadow_fear, incoming_defence_*) say "NO-INSTRUMENT / needs fields"; the current source emits `deep-fear ... over= shadow= credit=` at both seats (deepdefence.c ~2420 ledger; build seat analogous), so the instrument now exists. Whether the corpus the screen used (binary 24867bef2364d609) carried those fields was not checked.
- Existence of benchmark map huts: `explore_*` are live only if `wld.map.server.huts > 0`. Benchmark `.conf` files mention HUTS only in hutfree.conf (HUTS=0) and huts.conf; whether every other cell leaves the server default (MAP_DEFAULT_HUTS=15, map.h:642) was inferred from the absence of an override, not read from run.sh output.
- explore_discovery_value "structurally zero" assumes SKILL=hard (no H_MAP handicap); lower difficulty levels set H_MAP (ai/difficulty.c:63 for AI_LEVEL_AWAY at least; other levels not enumerated here).
- Prior-evidence venue/n details were taken from STATUS.md prose; the 2026-08-12 W36 and S-STACKED numbers were not re-derived from result dirs. Line references into STATUS.md are approximate (file is 24k+ lines; marked `~`).
- site_special_cluster / site_potential_discount "prior" are pre-W02b ablations (STATUS.md:648-653) against a formula that no longer exists; listed for completeness, not as evidence about the current expression.
- settler_fearfulness_mult "positive": the 133->150 cities result is from the 7-arm site-weight probe (n=5, isolated-150, no SE, no score); which arm value was used is not stated in the entry.
