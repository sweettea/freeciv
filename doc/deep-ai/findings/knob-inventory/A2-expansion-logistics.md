# Knob inventory A2: expansion logistics (38 knobs)

Facts only. Compiled defaults come from `ai/deep/deeptuning.c` (`.field = value`); `benchmark/tuning/defaults.ini` is an empty `[tuning]`, so "defaults" in every benchmark means these compiled values. Line numbers are at HEAD `b42d64d7a8`. `tuning-params.ini` is the tuner's range file; where its default differs from the compiled one it is flagged.

**Three knobs have `tuning-params.ini` defaults that differ from the compiled defaults** (tune-audit checks only that a compiled default exists, not that it matches):

| knob | compiled (runs at defaults) | params.ini default |
|---|---:|---:|
| lift_op_slots | 0 | 4 |
| lift_manifest_max | 8 | 3 |
| lift_drop_radius | 12 | 2 |

Several tuning files (`benchmark/tuning/pv2-gov.ini`, `pv4.ini`, `derby-lean.ini`) set `lift_op_slots = 4`, `lift_manifest_max = 2`, `lift_drop_radius = 2`, so those arms ran different values from the compiled defaults.

## Summary table

"Default" is the compiled default. "# sites" counts distinct code decisions that read the knob (comment mentions and log-only reads excluded); the raw line count is in parentheses where it differs. "Prior A/B" is one word from the arms in the per-knob sections. Gate knobs from S-SITEPLAN / S-FRONTIER show `positive` only because they were in the five-gate S-STACKED stack (n=100 per venue); alone they were null or negative.

| knob | unit | default | live at defaults? | tier | screen verdict | prior A/B | # sites |
|---|---|---|---|---|---|---|---|
| site_choke_bonus | want/score units (site tile-sum) | 12.0 | yes (only at choke tiles) | unranked (X, ALL, ECON) | not screened | none | 1 |
| colony_lift_weight | multiplier | 0.5 | yes | tier B (BCIVIL) | partial, fires 457/game; flip not computable | null | 1 (2 refs) |
| hull_commission_weight | multiplier | 0.0 | no: knob is its own gate | unranked (X, ALL, ECON) | not screened | none | 1 (2 refs) |
| hut_risk_radius | tiles | 5 | only if barbarians armed and hut_risk_honest=0 | unranked (M, ALL, MIL) | not screened | none | 1 (2 refs) |
| hut_risk_discount | multiplier | 0.25 | only if barbarians armed | unranked (M, ALL, MIL) | not screened | none | 2 |
| hut_risk_honest | boolean gate | 0 | no: honest branch off | unranked (M, ALL, MIL) | not screened | none | 1 |
| lift_form_radius | tiles | 8 | yes | unranked (ALL, ECON) | not screened | null | 3 |
| lift_gather_turns | turns | 8 | yes | unranked (ALL, ECON) | not screened | null | 3 (4 refs) |
| lift_cross_turns | turns | 20 | yes | unranked (ALL, ECON) | not screened | none | 1 |
| lift_op_slots | count | 0 (params 4) | read, but 0 = lift-only slots unused | tier B (BCIVIL) | partial, fires 841/game; flip not computable | null | 2 |
| lift_manifest_max | count | 8 (params 3) | yes | unranked (ALL, ECON) | not screened | null | 1 |
| lift_hitch_wait_ratio | multiplier | 0.25 | yes | unranked (ALL, ECON) | not screened | null | 2 |
| boat_idle_damping | multiplier | 0.5 | yes | unranked (ALL, ECON) | not screened | null | 2 |
| boat_water_gate | boolean gate | 1 | yes | unranked (ALL) | not screened | null | 4 (+1 behind hull_commission) |
| boat_gate_at_bid | boolean gate | 1 | yes | unranked (ALL) | not screened | null | 4 |
| lift_drop_radius | tiles | 12 (params 2) | yes | unranked (ALL, ECON) | not screened | none | 1 |
| lift_hitch_wait_min | turns | 1 | yes | unranked (ALL, ECON) | not screened | none | 1 |
| lift_muster_release | boolean gate | 0 | no: off branch | unranked (ALL, ECON) | not screened | none | 1 |
| founder_berth_route | boolean gate | 0 | no: returns at first test | unranked (ALL, ECON) | not screened | null | 1 |
| lift_wait_tail_pct | percentile 0-100 | 80 | census only; no behavioural consumer at defaults | unranked (X, ALL, ECON) | not screened | none | 1 |
| founder_tier_gate | boolean gate | 0 | no | unranked (ALL) | flips 256.9/game (7.2% of merges) if turned on; mode, not a gradient | null | 1 |
| founder_run_hold | boolean gate | 0 | no | unranked (ALL) | not screened | null | 1 |
| founder_site_cap_gate | boolean gate | 0 | no (census line is emitted) | unranked (X, ALL, ECON) | not screened | positive (stack only) | 1 |
| founder_site_cap_slack | count (founders) | 1.0 | arithmetic runs and is logged; verdict discarded | unranked (X, ALL, ECON) | not screened | none | 1 (2 refs) |
| founder_join_guard_gate | boolean gate | 0 | no (census line is emitted) | unranked (X, ALL, ECON) | not screened | positive (stack only) | 1 (2 refs) |
| founder_join_guard_mult | multiplier | 1.0 | arithmetic runs and is logged; verdict discarded | unranked (X, ALL, ECON) | not screened | none | 1 |
| capital_stake_gate | boolean gate | 0 | no | unranked (ALL) | dead-path | none | 1 |
| capital_stake_gain | multiplier | 1.0 | no: behind capital_stake_gate | unranked (ALL) | dead-path | none | 2 |
| site_improvable_gate | boolean gate | 0 | no | unranked (ALL) | dead-path | null | 1 |
| site_starve_price_gate | boolean gate | 0 | no | unranked (ALL) | dead-path | null | 2 |
| poach_deploy_gate | boolean gate | 0 | no | unranked (ALL) | not screened | null | 2 |
| poach_marginal_gate | boolean gate | 0 | no | unranked (ALL) | dead-path | none | 2 |
| frontier_transit_gate | boolean gate | 0 | no | unranked (M, ALL, MIL) | not screened | positive (stack only) | 2 functions (4 refs) |
| frontier_transit_price | turns | 1.0 | no: behind frontier_transit_gate | unranked (M, ALL, MIL) | not screened | none | 1 |
| frontier_transit_stance_floor | enum | 0 | no: behind frontier_transit_gate | unranked (M, ALL, MIL) | not screened | none | 1 function (2 refs) |
| founder_floor_gate | boolean gate | 0 | no (census line is emitted) | unranked (M, ALL, MIL) | not screened | negative | 1 (2 refs) |
| founder_floor_want | multiplier | 1.0 | no: behind founder_floor_gate | unranked (M, ALL, MIL) | not screened | none | 1 |
| founder_floor_reach_ref | count (sites) | 1.0 | no: behind founder_floor_gate | unranked (M, ALL, MIL) | not screened | none | 1 (2 refs) |

Notes on the table:
- "Tier" means the tier in `tuning-blocks.ini`. None of these knobs is in AUCTION, DEFOPS, BFEAR, BASSAULT, BSITE or BANTICIP. Only colony_lift_weight and lift_op_slots are in a tier-B block (BCIVIL). The rest are tier C (pinned at compiled defaults) and are reachable only through ALL and the topic blocks X / ECON / M / MIL.
- "Screen verdict" is taken from `findings/knob-screen-2026-08-30.md` and `findings/screen-corpus-2026-08-30.md`. Those documents name the dead-path set only in prose (capital_stake_*, site_improvable_gate, site_starve_price_gate, poach_marginal_gate) and print full lists only from `knobscreen.py`. "Not screened" means the knob is not named in either document. It does not mean the screen proved it inert.

---

## Part 1: sites, boats, huts

### site_choke_bonus
- **Declared:** deeptuning.h:816 (`double site_choke_bonus;`); params.ini: default=12.0 min=0.0 max=60.0 kind=float; compiled default 12.0 (marked PLACEHOLDER, untuned)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Site bonus (tile-sum currency, scaled by crowding) for local isthmus tiles -- a city in a gate is worth a garrison army. 0 disables."
- **Unit:** want/score units (site tile-sum currency). It is added to `wspecials`, the site evaluator's weighted-output sum, after multiplication by `deep_map_crowding()`. That function returns `MIN(1.0, density_prior_tiles / land_per_player)`, a dimensionless 0-1 factor.
- **Formula at the primary call site:** `wspecials += deep_tuning_get()->site_choke_bonus * deep_map_crowding();` guarded by `site_choke_bonus > 0.0 && deep_site_is_choke(&(wld.map), result->tile)` (deepsite.c:1318-1322, `deep_cityresult_fill`).
- **All call sites:**
  - deepsite.c:1318 and 1321 - deep_cityresult_fill - site value bonus when the candidate tile is a chokepoint - live at defaults? yes, but only for candidates where `deep_site_is_choke` is true.
- **Log line:** none. knobcensus.py:175 records "the choke predicate is never logged (deepsite.c:769)" and the knob is NO-INSTRUMENT.
- **Interacts with:** density_prior_tiles (through deep_map_crowding); site_own_overlap and the other `wspecials` terms in the same sum.
- **Tier/screen:** unranked (blocks X, ALL, ECON); not screened.
- **Prior evidence:** none. The only mention in docs is a design note in `findings/civ2civ3-adjustments.md:359` and a tuned-value record in STATUS.md (line 15342, `site_choke_bonus=6` inside a tuner output).

### colony_lift_weight
- **Declared:** deeptuning.h:1843 (`double colony_lift_weight;`); params.ini: default=0.5 min=0.0 max=5.0 kind=float; compiled default 0.5 (PLACEHOLDER)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Colony lift (task #20): weight on a fresh lift-limited overseas site score converted into a transport build want in coastal cities, amortized over the boat's build time. <= 0 disables."
- **Unit:** multiplier (dimensionless). It multiplies `lift_demand`, which is `dp->colony_want` (a site-score want) after idle-hull damping, and the product is amortized over the hull's build turns, so the result is in want units.
- **Formula at the primary call site:** `adv_want w = amortize(deep_tuning_get()->colony_lift_weight * lift_demand, MAX(1, bt));` where `bt = utype_build_shield_cost(...) / MAX(pcity->surplus[O_SHIELD], 1)` (deepdefence.c:6463-6464, `deep_military_choose_build`). The whole block is entered only if `colony_lift_weight > 0.0 && lift_demand > 0 && is_terrain_class_near_tile(TC_OCEAN) && (boat_gate_at_bid <= 0 || boat_water_gate <= 0 || deep_city_water_bridges(ptile))` (deepdefence.c:6448-6453).
- **All call sites:**
  - deepdefence.c:6448 - deep_military_choose_build - enable test for the colony-lift ferry bid - yes.
  - deepdefence.c:6463 - deep_military_choose_build - weight on the ferry build want - yes.
- **Log line:** `deep-colony ... lift="<type>" want=` (deepdefence.c:6467). The `demand=` form of `deep-colony` is the settler side's probe and does not prove this weight applied (knobcensus.py:830).
- **Interacts with:** boat_idle_damping (in `lift_demand`), boat_water_gate, boat_gate_at_bid, hull_commission_weight (the same function writes a competing ferry choice), stance expansion multiplier (`deep_stance_expand_mult`, applied to `lift_demand`).
- **Tier/screen:** tier B (BCIVIL; also X, ALL, ECON). Screen: partial, fires 457 per game; the log carries one side of the comparison so a flip is not computable (knob-screen Part 1, tier-B table).
- **Prior evidence:**
  - STATUS "combined configuration, 20 seeds (2026-08-04)" - isolated-150 - quartering colony_lift_weight (0.779 to 0.195) inside a human-specified bundle: -14.65 (se 19.05, t -0.77, 12/20), deliveries -8% - n=20 paired seeds. Wash, direction negative.
  - STATUS "full stack, three arms (2026-08-04)" - isolated-150 - combo with boat cut -17.15 (t -0.93); transport-only arm without the cut -9.45 (t -0.58) - n=20.
  - STATUS "Boat numbers: the fleet has THREE sources" (line 4006) - quartering the weight moved fleet 5.32 to 4.57, idle 75% in both; it is one of three ferry producers.

### hull_commission_weight
- **Declared:** deeptuning.h:1860 (`double hull_commission_weight;`); params.ini: default=0.0 min=0.0 max=5.0 kind=float; compiled default 0.0 ("off; A/B switch")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "weight on the excess of overseas FOUNDER demand over lift supply on one of this city's waters, priced as the settlements that excess is holding times the delay a hull takes off them". "this weight is the whole constant, and 0 = off is today's behaviour exactly."
- **Unit:** multiplier (dimensionless). It scales `n * gain`, where `n` is a count of founders (capped at hull capacity) and `gain` is `value - amortize(value, wait)` in colony_want (want) units; the product is amortized over the hull build time, giving want units.
- **Formula at the primary call site:** `cw = amortize(deep_tuning_get()->hull_commission_weight * n * gain, MAX(1, bt));` (deepdefence.c:6409-6410, `deep_military_choose_build`). `choice` is replaced when `cw > choice->want`.
- **All call sites:**
  - deepdefence.c:6346 - deep_military_choose_build - enable test for the whole commission block (also needs `dp->colony_want > 0` and a coastal city) - no: dead at default 0.0.
  - deepdefence.c:6409 - deep_military_choose_build - the commission want formula - no: same block.
- **Log line:** `deep-commission` (deepdefence.c:6423), written only on the branch the knob gates. `deep-liftgap` is emitted regardless and measures the same gap.
- **Interacts with:** boat_water_gate and boat_gate_at_bid (the block skips waters that do not bridge when both are on, deepdefence.c:6351-6352, 6377), lift_wait_tail_pct (through `deep_wait_expected`), the stance expansion multiplier.
- **Tier/screen:** unranked (X, ALL, ECON); not screened. Behind its own zero it is dead by construction.
- **Prior evidence:** STATUS "EXPANSION slice 2" (line ~14021-14090) - 2 x isolated-150 liveness games with `hull_commission_weight=1.0` and `lift_muster_release=1`: 23 `deep-commission` firings, 4 beat the running military choice, mean want 32.8 against colony-lift want mean 197; scorelogs byte-identical to the default arm, so no score effect and not a result. Notes that parity needs a weight near 5. Derby-7 (STATUS line 20197, 20309) ran with it at 0.0.

### hut_risk_radius
- **Declared:** deeptuning.h:1915 (`int hut_risk_radius;`); params.ini: default=5 min=0 max=15 kind=int; compiled default 5 (PLACEHOLDER)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "while the server can unleash hut barbarians, huts within hut_risk_radius of an own city count at hut_risk_discount in the explore want ... radius 0 disables."
- **Unit:** tiles. It is compared with `real_map_distance(city_tile(hc), ptile)`.
- **Formula at the primary call site:** `if (real_map_distance(city_tile(hc), ptile) <= tune->hut_risk_radius) { w = tune->hut_risk_discount; huts_discounted++; break; }` (deepexplore.c:216-221, `deep_explore_prize`).
- **All call sites:**
  - deepexplore.c:214 - deep_explore_prize - radius form gate (`hut_risk && hut_risk_radius > 0`) - conditional: `hut_risk` is `barbarianrate != BARBS_DISABLED && turn >= onsetbarbarian` (deepexplore.c:136); the branch is skipped when `hut_risk_honest > 0`.
  - deepexplore.c:217 - deep_explore_prize - distance test - same condition.
- **Log line:** `deep-explore ... huts_disc=` counts the huts the radius form discounted (knobcensus.py:565); `barbs_armed=` shows the gate.
- **Interacts with:** hut_risk_discount (same expression), hut_risk_honest (when 1, this knob is unread), explore_hut_value (the discounted weight is multiplied by it, deepexplore.c:254).
- **Tier/screen:** unranked (blocks M, ALL, MIL); not screened.
- **Prior evidence:** none. STATUS (line 6859, 12604) records that the radius form discounts the risk-free huts (sign backwards against the server) and that the A/B is owed.

### hut_risk_discount
- **Declared:** deeptuning.h:1916 (`double hut_risk_discount;`); params.ini: default=0.25 min=0.0 max=1.0 kind=float; compiled default 0.25 (PLACEHOLDER)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** huts within the radius "count at hut_risk_discount in the explore want -- far huts stay full value (a lost explorer is a fine trade for tech and gold; a barbarian spawn at home is not)."
- **Unit:** multiplier (dimensionless, 0-1). It replaces the per-hut weight `w` (default 1.0), and `hut_weight` is summed and then multiplied by `explore_hut_value` (deepexplore.c:254).
- **Formula at the primary call site:** `w = tune->hut_risk_discount;` in both branches, then `hut_weight += w;` and `return hut_weight * tune->explore_hut_value + unknown * tune->explore_discovery_value;` (deepexplore.c:211, 218, 225, 254, `deep_explore_prize`).
- **All call sites:**
  - deepexplore.c:211 - deep_explore_prize - honest-form weight for unleashable huts - no (needs `hut_risk_honest > 0`).
  - deepexplore.c:218 - deep_explore_prize - radius-form weight for huts near an own city - conditional on barbarians armed (see hut_risk_radius).
- **Log line:** `deep-explore ... huts_disc=`; `huts_unleash=` is the server-predicate population.
- **Interacts with:** hut_risk_radius, hut_risk_honest, explore_hut_value, explore_discovery_value.
- **Tier/screen:** unranked (M, ALL, MIL); not screened.
- **Prior evidence:** none (see hut_risk_honest).

### hut_risk_honest
- **Declared:** deeptuning.h:1937 (`int hut_risk_honest;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("Off: today's sign, pending A/B")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "0 = hut_risk_radius, own cities only: the shipped default, and the SIGN IS BACKWARDS against the server." "1 = the server's own predicate, discounting the huts that can actually unleash (none near any city). hut_risk_radius is then unread."
- **Unit:** boolean gate (0/1; read as `> 0`).
- **Formula at the primary call site:** `if (hut_risk && tune->hut_risk_honest > 0) { if (unleashable) { w = tune->hut_risk_discount; huts_discounted++; } } else if (hut_risk && tune->hut_risk_radius > 0) {...}` (deepexplore.c:205-221, `deep_explore_prize`).
- **All call sites:**
  - deepexplore.c:205 - deep_explore_prize - chooses honest or radius form - read, but the honest branch is dead at default 0.
- **Log line:** `deep-explore ... huts_unleash=` is computed in both arms; `huts_disc=` shows which set was charged (knobcensus.py:575).
- **Interacts with:** hut_risk_radius (unread when 1), hut_risk_discount, explore_hut_value.
- **Tier/screen:** unranked (M, ALL, MIL); not screened.
- **Prior evidence:** none. STATUS "hut-risk sign, corrected behind hut_risk_honest" (line 12604) shipped it at 0; the hutrisk-80 A/B and the explore_hut_value factorial are listed as owed (line 12732) with no result recorded in STATUS.md or findings.

---

## Part 2: the colony lift (sea lift) and boats

`lift_form_radius <= 0` disables the whole lift (deepops.c:6223: the formation block is skipped), so when it is 0 the following are never reached: lift_gather_turns, lift_cross_turns, lift_op_slots (except for the `deep_hitch_census` log arithmetic), lift_manifest_max, lift_hitch_wait_ratio, lift_hitch_wait_min, lift_drop_radius, lift_muster_release. At compiled defaults `lift_form_radius` is 8, so the lift is on.

### lift_form_radius
- **Declared:** deeptuning.h:854 (`int lift_form_radius;`); params.ini: default=8 min=0 max=20 kind=int; compiled default 8 (PLACEHOLDER)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "A lift operation forms when a waiting passenger has an unclaimed boat within lift_form_radius tiles ... lift_form_radius <= 0 disables the lift entirely and leaves stock's two-sided ferry protocol alone. ALL PLACEHOLDERS."
- **Unit:** tiles. Compared with `real_map_distance(unit_tile(psngr), unit_tile(boat))` and passed as the search radius to `deep_lift_best_meeting`.
- **Formula at the primary call site:** `d = real_map_distance(unit_tile(psngr), unit_tile(boat)); if (d > tune->lift_form_radius) { continue; }` (deepops.c:2117-2119, `deep_op_form_lift`).
- **All call sites:**
  - deepops.c:2118 - deep_op_form_lift - maximum passenger-to-boat distance for forming a lift - yes.
  - deepops.c:2146 - deep_op_form_lift - radius bound on the rendezvous search - yes.
  - deepops.c:6223 - deep_run_operations - `> 0` test that enables lift formation at all - yes.
- **Log line:** `deep-lift-op ... form` (knobcensus.py:866, grade direct).
- **Interacts with:** lift_op_slots (same block), lift_gather_turns, lift_cross_turns.
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:**
  - STATUS "colony lift ON vs OFF" (line ~3069-3100) - lategame-80 - lift on against negative control `lift_form_radius = 0`: score diff -1.35 (t -0.36, 8/20), cities -0.07; 110 crossings per 20 games versus none - n=10 paired seeds, 20 games. Wash.
  - Later mechanism repairs (rendezvous, multi-drop) measured large on proxies and flat on score (STATUS lines 3640-3900).

### lift_gather_turns
- **Declared:** deeptuning.h:855 (`int lift_gather_turns;`); params.ini: default=8 min=2 max=30 kind=int; compiled default 8 (PLACEHOLDER)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** the lift "aborts if boat and manifest have not met within lift_gather_turns". Later comment: with a manifest above 1 it "becomes a real GATHERING window".
- **Unit:** turns. It is compared with `game.info.turn - op->phase_turn`.
- **Formula at the primary call site:** `} else if (game.info.turn - op->phase_turn > tune->lift_gather_turns) { deep_op_dissolve(op, oi, "lift-gather-timeout", pplayer);` (deepops.c:2577-2578, `deep_op_drive_lift`).
- **All call sites:**
  - deepops.c:2538 - deep_op_drive_lift - turns left in the muster, passed to `deep_lift_worth_waiting` - yes.
  - deepops.c:2577 - deep_op_drive_lift - gather timeout dissolution - yes.
  - deepops.c:2629 - deep_op_drive_lift - turns left passed to `deep_lift_enroll` - yes.
- **Log line:** `deep-op ... lift-gather-timeout` and `deep-lift-op ... hitch-wait` (`left=`).
- **Interacts with:** lift_hitch_wait_ratio and lift_hitch_wait_min (budget is clipped to `turns_left - 1`), lift_manifest_max, lift_muster_release.
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:** STATUS "MEASURED (AND NOW SUPERSEDED): lift_gather_turns in 4..16" (line 3194) - lategame-80, 20 games each - gather 4: -0.50 (t -0.03), gather 16: -4.90 (t -0.20), timeouts 55 to 12, deliveries within one colony - 10 paired seeds. Wash; the STATUS entry says it was measured against the broken rendezvous and does not speak to the current mechanism.

### lift_cross_turns
- **Declared:** deeptuning.h:856 (`int lift_cross_turns;`); params.ini: default=20 min=4 max=60 kind=int; compiled default 20 (PLACEHOLDER)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** the lift aborts if "the crossing has not landed within lift_cross_turns."
- **Unit:** turns; compared with `game.info.turn - op->phase_turn`.
- **Formula at the primary call site:** `if (game.info.turn - op->phase_turn > tune->lift_cross_turns) { deep_op_dissolve(op, oi, "lift-cross-timeout", pplayer);` (deepops.c:2862-2863, `deep_op_drive_lift`).
- **All call sites:**
  - deepops.c:2862 - deep_op_drive_lift - crossing timeout dissolution - yes.
- **Log line:** `deep-op ... lift-cross-timeout`.
- **Interacts with:** lift_drop_radius (a further stop resets `phase_turn`, deepops.c:2292, giving a fresh cross budget).
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:** none as an arm. STATUS line 3693 reports that after the muster fix "the failures moved downstream" to the cross timeout; no sweep.

### lift_op_slots
- **Declared:** deeptuning.h:865 (`int lift_op_slots;`); params.ini: default=4 min=0 max=4 kind=int; compiled default 0 ("MEASURED negative; off"). **params.ini and compiled defaults differ.**
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "How many of the DEEP_LIFT_OPS_MAX lift-only slots a lift may use. 0 is exactly the old behaviour -- lifts take whatever the army left free and queue when the array is full -- so this doubles as the A/B switch."
- **Unit:** count (operation slots, clipped to `DEEP_LIFT_OPS_MAX` = 4). It is an upper limit on a loop index range: `for (oi = DEEP_ARMY_OPS_MAX; oi < DEEP_ARMY_OPS_MAX + slots; oi++)`.
- **Formula at the primary call site:** `int slots = CLIP(0, tune->lift_op_slots, DEEP_LIFT_OPS_MAX);` followed by the slot loop (deepops.c:6224-6232, `deep_run_operations`).
- **All call sites:**
  - deepops.c:6224 - deep_run_operations - number of lift-only slots tried first - read at defaults, but 0 means no lift-only slot is tried and lifts fall to the army's leftover slots.
  - deepops.c:1222 - deep_hitch_census - `usable` slot count for the `opfull=`/`liftslots=` log arithmetic only - log only.
- **Log line:** `deep-hitch` (`liftslots=`, `opfull=`); knobcensus grade proxy.
- **Interacts with:** lift_form_radius (the block is inside `lift_form_radius > 0`).
- **Tier/screen:** tier B (BCIVIL; also ALL, ECON). Screen: partial, fires 841/game; flip not computable.
- **Prior evidence:**
  - STATUS "four switches, isolated on one binary (2026-08-04)" - isolated-150 - slots=4: +22.30 (t +1.05, 5/10) - n=10 paired seeds.
  - STATUS "lift_op_slots: the ceiling was real, relieving it costs score" - isolated-150 - slots=4: -13.50 (t -0.88, 9/20), opfull 8% to 0%, forms +26%, deliveries +22% - n=20. The sign flipped under more seeds; the knob was defaulted to 0.

### lift_manifest_max
- **Declared:** deeptuning.h:873 (`int lift_manifest_max;`); params.ini: default=3 min=1 max=8 kind=int; compiled default 8 ("human call 2026-08-04"). **params.ini and compiled defaults differ.**
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Largest manifest a lift will gather, hitchers included. 1 is exactly the old single-passenger lift, so it is the A/B switch. The hull caps it independently and lower."
- **Unit:** count (units aboard). It enters `MIN(lift_manifest_max, get_transporter_capacity(ferry))`, a unit count.
- **Formula at the primary call site:** `room = MIN(tune->lift_manifest_max, get_transporter_capacity(ferry)) - op->n_members;` (deepops.c:1946-1947, `deep_lift_enroll`).
- **All call sites:**
  - deepops.c:1946 - deep_lift_enroll - cap on how many hitchers join a lift - yes. With compiled 8 the hull capacity is the binding cap (civ2civ3 Trireme carries 2).
- **Log line:** `deep-lift-op ... hitch` (enrolments), knobcensus grade proxy: the line does not say whether the cap bound.
- **Interacts with:** lift_hitch_wait_ratio, lift_hitch_wait_min, lift_gather_turns.
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:**
  - STATUS "Hitch-a-ride measured at last" (line 3658) - isolated-150 - `lift_manifest_max` 1 to 3 with `lift_hitch_wait_ratio` 0 to 0.25: -16.90 (se 19.35, t -0.87, 4/10) - n=10.
  - STATUS "Hitch-a-ride, final" (line ~3780) - isolated-150, multi-drop live: -8.25 (se 17.67, t -0.47, 10/20) - n=20. Recommendation recorded: default `lift_manifest_max = 1`. The compiled default is nevertheless 8 (header comment cites a human call of 2026-08-04).

### lift_hitch_wait_ratio
- **Declared:** deeptuning.h:882 (`double lift_hitch_wait_ratio;`); params.ini: default=0.25 min=0.0 max=1.0 kind=float; compiled default 0.25 (PLACEHOLDER)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "How long the boat will hold the quay for a straggler, as a fraction of the crossing it is about to make ... 0.0 never waits, which is the old contract and the A/B switch."
- **Unit:** multiplier (dimensionless fraction). It multiplies `eta_cross`, an integer count of turns, to give a wait budget in turns.
- **Formula at the primary call site:** `budget = MAX(MAX(1, tune->lift_hitch_wait_min), (int) (tune->lift_hitch_wait_ratio * eta_cross));` then `budget = MIN(budget, turns_left - 1);` (deepops.c:2034-2038, `deep_lift_worth_waiting`).
- **All call sites:**
  - deepops.c:2023 - deep_lift_worth_waiting - `<= 0` returns FALSE (never wait) - yes.
  - deepops.c:2035 - deep_lift_worth_waiting - scales the wait budget with the crossing length - yes.
- **Log line:** `deep-lift-op ... hitch-wait` (knobcensus.py:888, direct).
- **Interacts with:** lift_hitch_wait_min, lift_gather_turns (via `turns_left`), lift_manifest_max (a wait only matters if a straggler can be enrolled).
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:** only as part of the hitch bundle; see lift_manifest_max (same arms: -16.90 t -0.87 n=10; -8.25 t -0.47 n=20) and STATUS "full stack, three arms": patience cost, deliveries 430 to 414 with transport-only change, -9.45 (t -0.58) - n=20.

### boat_idle_damping
- **Declared:** deeptuning.h:893 (`double boat_idle_damping;`); params.ini: default=0.5 min=0.0 max=4.0 kind=float; compiled default 0.5 (PLACEHOLDER)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Damping on the colony boat want per ALREADY-IDLE hull: want /= (1 + boat_idle_damping * idle_boats). ... 0.0 restores the undamped signal exactly and is the A/B switch."
- **Unit:** multiplier (dimensionless coefficient per idle hull; the hull count it multiplies is a count). It sits in the denominator `1.0 + damping * idle`.
- **Formula at the primary call site:** `lift_demand = (int) (dp->colony_want / (1.0 + deep_tuning_get()->boat_idle_damping * idle));` (deepdefence.c:6249-6250, `deep_military_choose_build`; `idle` sums, per adjacent water, `MAX(0, idle ferries - ferry demand)`).
- **All call sites:**
  - deepdefence.c:6250 - deep_military_choose_build - damps the colony-lift demand by surplus hulls on the city's own waters - yes.
  - deepdefence.c:9315 - deep_manage_cities - `> 0.0` test for the second damping pass - yes.
  - deepdefence.c:9323 - deep_manage_cities - damps any ferry build choice (from any of the three ferry producers) by `deep_water_surplus` - yes.
- **Log line:** `deep-hull ... surplus= want=<before> -> <after>` (second site, deepdefence.c:9326). The first site has no dedicated line; its effect is in the `deep-colony` want.
- **Interacts with:** colony_lift_weight (same expression chain), boat_water_gate.
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:**
  - STATUS "four switches" (line 3745) - isolated-150 - damp (0.5): -20.00 (t -1.17, 3/10) - n=10.
  - STATUS "Two arms now agree" (line 3812) - same direction as colony_lift_weight quartering; deliveries fell.
  - STATUS "Boat numbers: the fleet has THREE sources" (line 4015) - per-ocean idle damping moved fleet 6.14 to 5.78, idle 80% to 79%, cost deliveries. A bug (damping never fired because docked hulls matched no ocean) was fixed first.

### boat_water_gate
- **Declared:** deeptuning.h:903 (`int boat_water_gate;`); params.ini: default=1 min=0 max=1 kind=int; compiled default 1 ("geometry, not a magnitude")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Refuse a ferry outright when every sea the city touches closes on the city's own continent, so nothing it carries could ever land anywhere else. ... 0 restores the ungated behaviour exactly and is the A/B switch. Not a magnitude and NOT a candidate for the tuner."
- **Unit:** boolean gate (0/1; read as `> 0`).
- **Formula at the primary call site:** `return deep_tuning_get()->boat_water_gate > 0 && choice->want > 0 && is_unit_choice_type(choice->type) && choice->value.utype != NULL && dai_is_ferry_type(choice->value.utype, ait) && !deep_city_water_bridges(city_tile(pcity));` (deepdefence.c:7638-7644, `deep_boat_choice_refused`).
- **All call sites:**
  - deepdefence.c:7638 - deep_boat_choice_refused - post-hoc veto of a winning ferry choice (backstop; also reached from the attacker-ferry path at 6171 and the final backstop in `deep_city_choose_build`) - yes.
  - deepdefence.c:8722 - deep_city_choose_build - `water_capped`: withdraws a negative founder_want (founder's boat) at bid formation - yes.
  - deepdefence.c:6452 - deep_military_choose_build - colony-lift ferry bid skipped when the city's water does not bridge - yes.
  - deepdefence.c:6352 - deep_military_choose_build - `gated` flag inside the hull-commission block - no: dead behind `hull_commission_weight > 0`.
- **Log line:** `deep-watergate` (backstop veto), `deep-watergate-bid` (bid-time withdrawal), `deep-watergate-attack`.
- **Interacts with:** boat_gate_at_bid. When `boat_water_gate` is 0, boat_gate_at_bid is dead: every use of it is `boat_gate_at_bid > 0 && boat_water_gate > 0`, or reaches `deep_boat_choice_refused`, which returns FALSE immediately (deepdefence.c:6171, 6351-6352, 6451-6452, 8721-8722). Also site_naval_gate / site_naval_weight read the same `deep_city_water_bridges` geometry on the site side (separate knobs).
- **Tier/screen:** unranked (ALL only); not screened. knobcensus grade: mode, direct.
- **Prior evidence:**
  - STATUS "useful-water gate: the first fleet change that RAISES arrivals" (line ~4152) - isolated-150 - gate on: score +4.65 (t +1.12), arrivals +3.90 (t +2.67) - n=20 paired seeds. Retired later.
  - STATUS "gate confirmed at power" - isolated-150 - +1.82 (se 2.71, t +0.67, 18/40) - n=40; lategame-80 - -0.07 (t -0.16) - n=20. Score null; mechanism exact (closed-sea hulls to 0).

### boat_gate_at_bid
- **Declared:** deeptuning.h:912 (`int boat_gate_at_bid;`); params.ini: default=1 min=0 max=1 kind=int; compiled default 1 ("filter before the choice")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Ask the useful-water test where a ferry bid is FORMED instead of vetoing the bid that won ... 0 restores the post-hoc veto and is the A/B switch. Not a magnitude and NOT a candidate for the tuner."
- **Unit:** boolean gate (0/1; read as `> 0` or `<= 0`).
- **Formula at the primary call site:** `bool water_capped = (deep_tuning_get()->boat_gate_at_bid > 0 && deep_tuning_get()->boat_water_gate > 0 && city_data->founder_want < 0 && !deep_city_water_bridges(city_tile(pcity)));` (deepdefence.c:8721-8724, `deep_city_choose_build`).
- **All call sites:**
  - deepdefence.c:8721 - deep_city_choose_build - withdraw the founder's boat bid at formation - yes.
  - deepdefence.c:6451 - deep_military_choose_build - colony-lift ferry bid withdrawn when the water does not bridge - yes.
  - deepdefence.c:6171 - deep_military_choose_build - revert a stock attacker-ferry pick to the pre-attacker choice - yes.
  - deepdefence.c:6351 - deep_military_choose_build - `gated` in the hull-commission block - no: dead behind `hull_commission_weight > 0`.
- **Log line:** `deep-watergate-bid`, `deep-watergate-attack`, and `deep-watergate` (backstop; should read near zero with this on).
- **Interacts with:** boat_water_gate (required: dead if it is 0), hull_commission_weight, colony_lift_weight.
- **Tier/screen:** unranked (ALL only); not screened. knobcensus grade: mode, direct.
- **Prior evidence:** STATUS "Cell 4: bid-time withdrawal" (line ~4284) - isolated-150, vs post-hoc veto with gate on in both - score +3.56 (se 4.22, t +0.85, 12/20), landarea +12125 (t +2.89), gate-refusals 17.25 to 1.40 - n=20 paired seeds. Score null; the OFF arm reproduced the earlier cell to the digit.

### lift_drop_radius
- **Declared:** deeptuning.h:924 (`int lift_drop_radius;`); params.ini: default=2 min=0 max=20 kind=int; compiled default 12 ("human: two boat moves"). **params.ini and compiled defaults differ.**
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "How far the boat will DETOUR to land a passenger at its own site rather than making it walk, in tiles ... 0 collapses to a single drop for the whole manifest."
- **Unit:** tiles; compared with `bestd`, a `real_map_distance` from the ferry to the nearest remaining passenger target.
- **Formula at the primary call site:** `if (next == NULL || bestd > tune->lift_drop_radius || deep_lift_meeting_tile(nmap, ferry, next->goto_tile) == NULL) { return FALSE; }` (deepops.c:2285-2289, `deep_lift_serve_stop`).
- **All call sites:**
  - deepops.c:2287 - deep_lift_serve_stop - detour budget for a further drop - yes (only matters for manifests above 1).
- **Log line:** `deep-lift-op ... nextstop` (`gap=` is `bestd`).
- **Interacts with:** lift_manifest_max (a second stop needs a second passenger), lift_cross_turns (a further stop resets the cross budget).
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:** none as its own arm. It was part of the multi-drop routing commits (STATUS "Dead code that measured as no effect (2026-08-04)": routing never ran until `fc4262ff15`; with routing live the hitch bundle moved from -16.90/-19.00 to -8.25, t -0.47, n=20).

### lift_hitch_wait_min
- **Declared:** deeptuning.h:932 (`int lift_hitch_wait_min;`); params.ini: default=1 min=1 max=8 kind=int; compiled default 1 (PLACEHOLDER)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Floor, in whole turns, under the hitch wait budget when the ratio admits any wait at all ... Still clipped to the gather window's second-to-last turn."
- **Unit:** turns.
- **Formula at the primary call site:** `budget = MAX(MAX(1, tune->lift_hitch_wait_min), (int) (tune->lift_hitch_wait_ratio * eta_cross));` (deepops.c:2034-2035, `deep_lift_worth_waiting`).
- **All call sites:**
  - deepops.c:2034 - deep_lift_worth_waiting - floor under the wait budget - yes, but only past the `lift_hitch_wait_ratio <= 0` early return; binds only when `ratio * eta_cross < min`.
- **Log line:** `deep-lift-op ... hitch-wait` (knobcensus grade proxy: the binding case is not logged).
- **Interacts with:** lift_hitch_wait_ratio, lift_gather_turns. Dead when `lift_hitch_wait_ratio <= 0`.
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:** none as its own arm. STATUS "combined configuration" (line 3793) bundled a ">=2 turn wait floor"; see colony_lift_weight (-14.65, t -0.77, n=20).

### lift_muster_release
- **Declared:** deeptuning.h:943 (`int lift_muster_release;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("off; A/B switch")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Re-validate a lift passenger's mission during the MUSTER as well as the crossing (0/1, not a magnitude). ... At 0 the muster only COUNTS them (deep-lift-invalid phase=muster)."
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (deep_tuning_get()->lift_muster_release > 0) { if (!deep_lift_revalidate(ait, pplayer, op, oi, ferry, beach, "muster")) { return; } ... } else { (void) deep_lift_muster_invalid(...); }` (deepops.c:2512-2525, `deep_op_drive_lift`).
- **All call sites:**
  - deepops.c:2512 - deep_op_drive_lift - muster-phase revalidation or count-only - read; the release branch is dead at default 0.
- **Log line:** `deep-lift-invalid phase=muster` is written in both arms; the `lift-invalid` dissolution and `reaim` separate them (knobcensus.py:835).
- **Interacts with:** lift_gather_turns (the muster length), lift_form_radius (needs lifts).
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:** STATUS "EXPANSION slice 2" (line ~14037-14141) - 2 x isolated-150 games with `lift_muster_release=1` - population about 2 muster invalidations per two games; knob arm scorelogs byte-identical to default. Liveness only, no score arm.

### founder_berth_route
- **Declared:** deeptuning.h:961 (`int founder_berth_route;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("off; A/B switch")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Walk a founder that needs a hull to a coast a hull could actually fetch it from (0/1, not a magnitude). ... At 0 nothing moves and the geometry is only counted (deep-berth)."
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (deep_tuning_get()->founder_berth_route <= 0 || !unit_can_do_action(punit, ACTION_FOUND_CITY) || ... || deep_water_serves_crossing(unit_tile(punit), dest)) { return TRUE; }` (deepsettler.c:781-786, `deep_settler_berth_route`; called from `deep_settler_gothere` at deepsettler.c:840).
- **All call sites:**
  - deepsettler.c:781 - deep_settler_berth_route - redirect a boat-needing founder to a serviceable berth - no: returns immediately at default 0.
- **Log line:** `deep-berthroute` (written only when the knob acts); `deep-berth` is the census, written in both arms.
- **Interacts with:** none in the expression. Related by mechanism: boat_water_gate, hull_commission_weight.
- **Tier/screen:** unranked (ALL, ECON); not screened.
- **Prior evidence:** STATUS "BERTH A/B on the naval venue" (line 14485) - tech65sea-150, pv3 vs pv3 + `founder_berth_route=1` - +0.14 per seat, sem 0.67, t +0.21; 13 of 16 seed pairs identical - n=16 mirrored seeds. Null; population absent on that venue. STATUS line 20191: inert by construction in the Europe game (0 `deep-berth` lines).

### lift_wait_tail_pct
- **Declared:** deeptuning.h:973 (`int lift_wait_tail_pct;`); params.ini: default=80 min=50 max=100 kind=int; compiled default 80 ("measured: 20% of spells, 60%...")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Which point of the live wait-spell distribution deep_wait_expected() reports, as a percentile: 100 is the longest spell that water is holding, 50 the median. Not a magnitude and not free: 80 is where the measured mass sits."
- **Unit:** percentile 0-100 (integer index into the sorted spell list; a probability times 100). It enters `idx = (n * pct) / 100`.
- **Formula at the primary call site:** `pct = CLIP(0, deep_tuning_get()->lift_wait_tail_pct, 100); idx = (n * pct) / 100; if (idx >= n) { idx = n - 1; } return elapsed[idx];` (deepops.c:1614-1621, `deep_wait_expected`).
- **All call sites:**
  - deepops.c:1614 - deep_wait_expected - picks the percentile of live wait-spell durations - read in every call, but the returned value changes a decision only through two consumers, both off at defaults: deepdefence.c:6399 (behind `hull_commission_weight > 0`, 0.0) and deepsite.c:2459 via `deep_site_beach_wait` (behind `site_wait_discount > 0.0`, default 0.0). Remaining consumers are log lines (`deep-liftgap wait=` at deepops.c:1725).
- **Log line:** `deep-waitspell` (live/longest) and `deep-liftgap ... wait=`.
- **Interacts with:** site_wait_discount and hull_commission_weight (the two consumers; the STATUS note says to move site_wait_discount alone and keep this at 80).
- **Tier/screen:** unranked (X, ALL, ECON); not screened. knobcensus grade: mode, direct.
- **Prior evidence:** none. STATUS line 12394 says it "is a shape parameter and should stay at 80"; the site_wait_discount A/B is listed as scheduled separately, with no result recorded here.

---

## Part 3: founder gates, capital stake, site pricing, poach

### founder_tier_gate
- **Declared:** deeptuning.h:3004 (`int founder_tier_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = today's arbitration exactly")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "let a founder want CONTEST the military emergency tier instead of being discarded by it." With the gate on, a city whose `founder_want` strictly exceeds the military want "still reaches the domestic advisor when the emergency skip would have suppressed the call" and "wins the merge if the advisor's own winner is a founder". "no discount, no scale, no constant."
- **Unit:** boolean gate (0/1; read as `<= 0`).
- **Formula at the primary call site:** `if (deep_tuning_get()->founder_tier_gate <= 0 || city_data->founder_want <= 0 || deep_want_from_choice(&city_data->choice).tier != DWT_EMERGENCY || (adv_want) city_data->founder_want <= city_data->choice.want) { return FALSE; }` (deepdefence.c:8478-8482, `deep_founder_contests_emergency`; caller deepdefence.c:8654 in `deep_city_choose_build`).
- **All call sites:**
  - deepdefence.c:8478 - deep_founder_contests_emergency - decides whether a founder want contests the emergency tier - no: dead at default 0.
- **Log line:** `deep-tiercontest` (cannot be emitted with the gate off, knobcensus.py:1529).
- **Interacts with:** founder_run_hold (the ON arm in the one A/B ran both gates). The comparator is `>` between two existing wants; no other tuning knob appears in the expression.
- **Tier/screen:** unranked (ALL only). Screen: shortlisted as a mode key. Off-to-on would change 7.2% of city-turn merges (256.9 flips/game over 3571 decisions, rule `auction_merge`); the screen text says it "belongs in a gate A/B, not in a parameter search". Not in any fit block.
- **Prior evidence:**
  - STATUS "S-EXPAND-FIX" (line 19212) and the "Corrections" paragraph at line 23102 - tech50war-200 mixed cells - pv3 vs pv3 + both gates (`founder_tier_gate` and `founder_run_hold`): cities gap at T201 -0.35 (sem 1.76, t -0.20, 9/20) mixed_dc and -1.15 (sem 2.27, t -0.51, 9/20) mixed_cd; score -36.80 (sem 34.24, t -1.07) and -6.30 (sem 33.78, t -0.19); founder auction wins +46% and +68%; prod-per-win 1.005 to 1.262 - n=20 seeds per ordering, 80 games. Null. The arm moved both gates together, so it does not isolate this one.

### founder_run_hold
- **Declared:** deeptuning.h:3031 (`int founder_run_hold;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = today's preemption exactly")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "a founder build already under way is not preempted by a military switch." When the city is already building a founder, has shields in stock, can still build it and the replacing choice is a military unit, "the decision is declined and production stands", except on the advisor's grave-danger signal.
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (deep_tuning_get()->founder_run_hold <= 0 || pcity->shield_stock <= 0 || pcity->production.kind != VUT_UTYPE || !utype_is_cityfounder(...)) { return FALSE; }` then further tests on choice type, `can_city_build_now`, city size versus pop cost, and `grave_danger > 0` (deepdefence.c:8553-8590, `deep_founder_run_held`; caller deepdefence.c:8888 in `deep_city_choose_build`).
- **All call sites:**
  - deepdefence.c:8553 - deep_founder_run_held - decline a military switch away from a founder run - no: dead at default 0.
- **Log line:** `deep-runhold` (cannot be emitted with the gate off, knobcensus.py:1536).
- **Interacts with:** founder_tier_gate (joint arm), founder_site_cap_gate and founder_join_guard_gate (factorial partners in S-SITEPLAN).
- **Tier/screen:** unranked (ALL only); not screened as its own entry.
- **Prior evidence:**
  - S-EXPAND-FIX 80-game 2-ordering A/B, together with founder_tier_gate (see above): null on cities gap and score - n=20 per ordering.
  - STATUS "S-PRODKEEP" (line 22573) - contested.conf - hold arm vs off: -7.0 (se 16.1, 10/20); delivery rate 24.8% to 30.7% (T125+ 37.3% to 68.8%) - n=20 training seeds. Wash.
  - `findings/siteplan-verdict-2026-08-29.md` c01 (run_hold alone), war: score +2.12 (se 10.12, t +0.21), joins +4.12 (t +2.60); econ: -2.02 (se 8.40, t -0.24), joins +17.38 (t +10.57) - n=40 per cell. c11 (all three gates) war -0.72 (se 10.90) n=40, econ +13.78 (se 8.02) n=40. Null alone; interaction with the cap and guard changes sign between venues.
  - Excluded from the S-STACKED stack because its score effect was -0.63 / -0.35 per seat (`findings/marginal-city-2026-08-30.md`).

### founder_site_cap_gate
- **Declared:** deeptuning.h:3530 (`int founder_site_cap_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = today's founder bid exactly")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "never carry more founders than there are sites left to put them on." Withdraws the founder bid "when the founders this player ALREADY has -- alive, plus under construction in any OTHER city -- reach the sites still counted for it." Exists so the join guard does not buy baseline's 19-40 idle settlers; "the reason the two ship as one arm."
- **Unit:** boolean gate (read as `> 0` in the return).
- **Formula at the primary call site:** `cap = sites + tune->founder_site_cap_slack; capped = ((double) committed >= cap); ... return capped && tune->founder_site_cap_gate > 0;` with `sites = deep_remaining_sites(pplayer)` and `committed = deep_founders_committed(pplayer, pcity)` (deepdefence.c:8217-8230, `deep_founder_site_capped`).
- **All call sites:**
  - deepdefence.c:8230 - deep_founder_site_capped - gates the verdict that withdraws the founder bid (the caller zeroes `founder_want` before the domestic advisor runs, deepdefence.c:8718-8737) - no: the function runs and logs every city-turn with a founder bid, but returns FALSE at default 0.
  - deepdefence.c:8228 - deep_founder_site_capped - logged only.
- **Log line:** `deep-sitecap ... capped= gate=` (emitted in both arms).
- **Interacts with:** founder_site_cap_slack (read in the same expression whether or not the gate is on; the slack changes nothing observable while the gate is 0), founder_join_guard_gate (shipped as one arm), boat_gate_at_bid / boat_water_gate (the `site_capped || water_capped` branch in the same caller), mil_opportunity_gate (S-FEARSINK factorial partner; a withdrawn bid is not charged as displacement).
- **Tier/screen:** unranked (blocks X, ALL, ECON); not screened.
- **Prior evidence:**
  - `findings/siteplan-verdict-2026-08-29.md` c10 (cap + guard) - contested.conf, 8 deep seats, T150: score +16.32 (se 9.15, t +1.78), joins -24.23, cities end -0.09 (t -1.93) - n=60; lategame-80, T80: score +7.97 (se 6.26, t +1.27), cities T60 +0.37 (t +4.68) - n=40. Gate stayed at 0.
  - `findings/fearsink-verdict-2026-08-30.md` c01 (cap + guard, no opportunity price) - contested: +2.08 (se 1.82), econ +2.07 (se 1.18) per seat - n=20 per venue. Positive in both, 1.1 and 1.7 sigma.
  - `findings/stacked-lever-2026-08-31.md` (S-STACKED) - five gates on vs off (mil_opportunity_gate, founder_site_cap_gate, founder_join_guard_gate, frontier_transit_gate, boat_shadow_gate, every gain at placeholder) - war cities/seat +0.15 (se 0.04, t 3.7), score/seat +2.39 (se 1.02, t 2.3), 58/100; econ cities/seat +0.60 (se 0.07, t 8.3), score/seat +4.12 (se 0.61, t 6.8), 79/100 - n=100 paired training seeds per venue, 403 games. Positive, but for the stack; per-gate attribution was not run (a leave-one-out is estimated at 600 games). Both venues end with about 1.7 more live settlers per seat.

### founder_site_cap_slack
- **Declared:** deeptuning.h:3541 (`double founder_site_cap_slack;`); params.ini: default=1.0 min=0.0 max=5.0 kind=float; compiled default 1.0 (PLACEHOLDER, unmeasured)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "How many founders above the counted sites the cap tolerates." "PLACEHOLDER 1.0 and NOT MEASURED ... One is the smallest slack that admits that founder; it is not evidence that one is the right number."
- **Unit:** count (founders, held as a double). It is added to `deep_remaining_sites()` (a site count) and compared with an integer count of committed founders.
- **Formula at the primary call site:** `cap = sites + tune->founder_site_cap_slack; capped = ((double) committed >= cap);` (deepdefence.c:8218-8219, `deep_founder_site_capped`).
- **All call sites:**
  - deepdefence.c:8218 - deep_founder_site_capped - threshold for the cap - the arithmetic runs at default but the verdict is discarded while `founder_site_cap_gate` is 0 (behaviourally dead).
  - deepdefence.c:8227 - deep_founder_site_capped - logged as `slack=`.
- **Log line:** `deep-sitecap ... slack= cap=`.
- **Interacts with:** founder_site_cap_gate (it matters only if that is 1), founder_join_guard_gate.
- **Tier/screen:** unranked (X, ALL, ECON); not screened.
- **Prior evidence:** none. The verdict doc lists it as a placeholder owed to a tuning pass; the cap fired 47 times per game in each venue in the S-SITEPLAN cell, and 69.3 per game in the S-FEARSINK corner (the slack's reach depends on the guard).

### founder_join_guard_gate
- **Declared:** deeptuning.h:3585 (`int founder_join_guard_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = today's join exactly")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "a founder is not dissolved while a site may open inside the time it would take to build another one." The join is declined when any of three signals (we founded, a mission was cancelled under a walking founder, a search was refused land on a border or reservation) falls within the window; "it holds and repositions rather than dissolving."
- **Unit:** boolean gate.
- **Formula at the primary call site:** `return held && tune->founder_join_guard_gate > 0;` where `held = (s_found || s_cancel || s_blocked)` (deepsettler.c:1865, `deep_founder_join_guarded`; caller deepsettler.c:1951 in `deep_settler_join_city`, placed as the last refusal before the unit is spent).
- **All call sites:**
  - deepsettler.c:1865 - deep_founder_join_guarded - makes the join refusal effective - no: dead at default 0.
  - deepsettler.c:1863 - deep_founder_join_guarded - logged as `gate=` (the function and its census line run in both arms).
- **Log line:** `deep-joinguard ... held= gate=` (emitted in both arms).
- **Interacts with:** founder_join_guard_mult (window), founder_site_cap_gate (paired), founder_floor_gate (reads the same `deep_site_blocked_seen_turn` signal on the production side).
- **Tier/screen:** unranked (X, ALL, ECON); not screened.
- **Prior evidence:** same arms as founder_site_cap_gate (it is the second half of the pair): siteplan c10 war +16.32 (se 9.15) n=60, econ +7.97 (se 6.26) n=40; fearsink c01 +2.08 / +2.07 per seat n=20; S-STACKED stack n=100 per venue. `siteplan-verdict` §3 records that the blocked-land signal fires on 99.6% of join attempts at mult 1.0, so the guard as built is equivalent to switching the join off.

### founder_join_guard_mult
- **Declared:** deeptuning.h:3596 (`double founder_join_guard_mult;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float; compiled default 1.0 (PLACEHOLDER, unmeasured)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The window, in settler-build-times. PLACEHOLDER 1.0 and NOT MEASURED. One build-time is the break-even the paragraph above describes and nothing more."
- **Unit:** multiplier (dimensionless; "settler-build-times"). It multiplies `rebuild`, the integer turns to replace a settler (`ceil(cost / best shield surplus)`), producing a window in turns.
- **Formula at the primary call site:** `double window = tune->founder_join_guard_mult * rebuild; ... s_found = (founded >= 0 && (double) (turn - founded) < window);` (deepsettler.c:1847-1851, `deep_founder_join_guarded`).
- **All call sites:**
  - deepsettler.c:1847 - deep_founder_join_guarded - window length for the three signals - arithmetic runs at default, but the verdict is discarded while `founder_join_guard_gate` is 0 (behaviourally dead).
- **Log line:** `deep-joinguard ... window=`.
- **Interacts with:** founder_join_guard_gate, deep_founder_rebuild_turns (not a knob).
- **Tier/screen:** unranked (X, ALL, ECON); not screened.
- **Prior evidence:** none. `siteplan-verdict` §5 and `fearsink-verdict` §8 say sweeping it is "close to pointless while the blocked signal fires 99.6% of the time".

### capital_stake_gate
- **Declared:** deeptuning.h:3138 (`int capital_stake_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = the inherited ledger exactly")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Losing the capital is not losing a city: it rolls the ruleset's civil-war dice". 0 is "the ledger the epic inherited, byte for byte"; non-zero multiplies the capital's danger by `1 + capital_stake_gain x P(civil war) x (expected defector worth / capital worth)`.
- **Unit:** boolean gate (tested `== 0`).
- **Formula at the primary call site:** `if (tune->capital_stake_gate == 0 || tune->capital_stake_gain <= 0.0 || !is_capital(pcity)) { return 1.0; }` (deepdefence.c:2201-2204, `deep_capital_stake_mult`; callers `deep_apply_capital_stake` at deepdefence.c:2412 "ledger" and 5555 "build").
- **All call sites:**
  - deepdefence.c:2201 - deep_capital_stake_mult - first-comparison exit - no: returns 1.0 at default 0.
- **Log line:** `deep-capstake` (cannot exist in a gate-off game, knobcensus.py:1631).
- **Interacts with:** capital_stake_gain. When this is 0 the gain is not read (the `||` short-circuits on the first operand).
- **Tier/screen:** unranked (ALL only). Screen: dead-path (named in the knob-screen dead-path list).
- **Prior evidence:** STATUS "capital stake" entry (line ~21290-21460): identity 20/20 byte-identical twice (10 smoke seeds x deep and mixed_dc); liveness by paired replay of a T120 Europe save to T135: capital garrisons +15.0% (1.157 to 1.330), other cities -3.1%, 5 of 28 applications hit the quadratic clip, n=1 save. The 16-seed war-venue A/B was deferred (box busy). No score result.

### capital_stake_gain
- **Declared:** deeptuning.h:3151 (`double capital_stake_gain;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float; compiled default 1.0 ("DERIVED: worth/worth is a ratio")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The conversion in that formula, and it is a RATIO of two ai_city.worth figures, so 1.0 is the identity". Below 1.0 discounts the empire's exposure; above 1.0 "says the disruption costs more than the cities themselves".
- **Unit:** multiplier (dimensionless). It multiplies `exposure / capital_worth`, a ratio of two worth figures.
- **Formula at the primary call site:** `mult = 1.0 + tune->capital_stake_gain * exposure / capital_worth;` (deepdefence.c:2252, `deep_capital_stake_mult`), where `exposure = odds * pool * flips / cands` and `odds` is the civil-war chance from `EFT_CIVIL_WAR_CHANCE` less city bonuses.
- **All call sites:**
  - deepdefence.c:2201 - deep_capital_stake_mult - `<= 0.0` exit - no: behind `capital_stake_gate == 0`.
  - deepdefence.c:2252 - deep_capital_stake_mult - the multiplier formula - no: same.
- **Log line:** `deep-capstake` (`mult=` is monotone in the gain).
- **Interacts with:** capital_stake_gate; reads (not tuning knobs) civil-war effects and `ai_city.worth`.
- **Tier/screen:** unranked (ALL only). Screen: dead-path.
- **Prior evidence:** none beyond the capital_stake_gate liveness replay.

### site_improvable_gate
- **Declared:** deeptuning.h:3188 (`int site_improvable_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = as-is site pricing exactly")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "price a candidate site by what workers can MAKE of its tiles, not only by what those tiles yield on the day the settler arrives." A catchment tile's potential becomes "the best of {irrigate, mine, every terrain reachable by cultivating and planting}" amortized over the ruleset's activity time. "A GATE with no companion magnitude".
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (deep_tuning_get()->site_improvable_gate > 0) { const struct deep_improve *imp = deep_tile_improve(pplayer, ptile, adv); ... pot = imp->gain; }` followed by `potentials[ncatch] = pot * site_potential_weight;` (deepsite.c:1081-1103, `deep_cityresult_fill`).
- **All call sites:**
  - deepsite.c:1081 - deep_cityresult_fill - replaces the as-is potential `MAX(irrigation, mining)` with the achievable-yield price - no: dead at default 0.
- **Log line:** `deep-siteimprove` (tiles/lifted/cut/gain/chain); absent entirely when the gate is off.
- **Interacts with:** site_potential_weight and site_potential_discount (keep their meanings; the weight still scales the increment), site_pressure_factor; site_starve_price_gate (separate gate, both live in the same `deep-siteimprove` line).
- **Tier/screen:** unranked (ALL only). Screen: dead-path.
- **Prior evidence:** STATUS "site improvable" entry (line 21843) - isolated-150, 4-arm 2x2 with site_starve_price_gate, 16 training seeds, one binary: pricing arm (this gate alone) -1.70 (se 3.36, t -0.51, 8/16); both +2.27 (se 4.61, t +0.49, 10/16). In civ2civ3 the pricing half cut 2.5M of 5.1M tile-prices and lifted none (arithmetic, not measurement). Null.

### site_starve_price_gate
- **Declared:** deeptuning.h:3217 (`int site_starve_price_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = the starve DROP, as before")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** the second half of site-improvable, gated separately: instead of dropping a site that "starves at founding", offer each tile as the tile a worker has finished with and charge "amortize(total, wait)"; "Only a site no improvement ever feeds is still dropped."
- **Unit:** boolean gate.
- **Formula at the primary call site:** `int wait = (deep_tuning_get()->site_starve_price_gate > 0 ? deep_site_last_starve_wait : -1); if (wait < 0) { ... return NULL; } cr->total = amortize(cr->total, wait);` (deepsite.c:1974-1986, `deep_city_desirability`).
- **All call sites:**
  - deepsite.c:1147 - deep_cityresult_fill - computes `deep_site_last_starve_wait` from the improvement costs - no: dead at default 0.
  - deepsite.c:1974 - deep_city_desirability - turns the starve drop into a wait charge - no: dead at default 0.
- **Log line:** `deep-siteimprove ... starvepriced= starveturns= starvehard=`.
- **Interacts with:** site_improvable_gate (independent), food_cost (ruleset), amortize() (MORT 24).
- **Tier/screen:** unranked (ALL only). Screen: dead-path.
- **Prior evidence:** same 2x2 as site_improvable_gate - starve arm alone -3.38 (se 6.04, t -0.56, 7/16); starve drops -84.7%, scored candidates +16.2%, unitsbuilt +7.78 (t +3.43) - n=16. Null on score.

### poach_deploy_gate
- **Declared:** deeptuning.h:3243 (`int poach_deploy_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "THE POACH SEQUENCING GATE." Moves the removal of the poached blade from the ask to "the one exit that actually deploys the force, so a claim the arithmetic then refuses leaves the wave intact." "Control flow, not a magnitude".
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (tune->poach_deploy_gate <= 0) { op->members[blade_i] = op->members[--op->n_members]; deep_employ_op_depart(pplayer, oi, DED_POACH); }` (deepops.c:4865-4868, `deep_op_poach_defender`).
- **All call sites:**
  - deepops.c:4865 - deep_op_poach_defender - removes the blade inside the ask when off - the OFF branch is live at default (that is the historical behaviour); the ON branch is dead.
  - deepdefence.c:4085 - deep_defend_counter_claims - `poach_spent = tune->poach_deploy_gate <= 0;` tells the later exits whether the blade is already gone - same.
- **Log line:** `deep-employpoach ... took=` (`took=0` cannot exist with the gate off, knobcensus.py:1672); also `deep-op ... poach`.
- **Interacts with:** poach_marginal_gate (the same decision; the header says deploy should land first), survival_target, survival_horizon, defence_worth_mult, conquest_worth_mult (in the poach expressions).
- **Tier/screen:** unranked (ALL only); not screened.
- **Prior evidence:** STATUS "S-POACHFIX" (line ~22150-22460) - tech50war-200, `pv3.ini`, mixed_dc: paid-for-nothing poaches 64.3% to 0.0%, blades off a wave 55.2 to 27.8 per game; score -0.71 +/- 3.34 per seat (t -0.21, 5/16) - n=16 paired seeds. Mechanism win, outcome null. Derby-7 Europe rider sets it to 1 (line 22439).

### poach_marginal_gate
- **Declared:** deeptuning.h:3273 (`int poach_marginal_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = the total-vs-total ...")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "THE POACH ARBITRATION GATE." Replaces the city's TOTAL risk against the operation's TOTAL take with `gain = own worth x defence_worth_mult x (survival with the blade - survival of the force already assembled)` against `loss = conquest worth x (odds with - odds without)`; "the blade is claimed only when gain > loss". "ORDER MATTERS. poach_deploy_gate should land first".
- **Unit:** boolean gate.
- **Formula at the primary call site:** `gain = raw_own * dmult * (surv_with - relieved); loss = worth * (odds - odds_wo); ... if (gain <= loss) { continue; }` (deepops.c:4830-4841, `deep_op_poach_defender`; the OFF comparison is `if (tune->poach_marginal_gate <= 0 && city_risk <= take) continue;` at deepops.c:4758).
- **All call sites:**
  - deepops.c:4758 - deep_op_poach_defender - the inherited total-vs-total test, applied only when the gate is 0 - yes (the OFF behaviour).
  - deepops.c:4787 - deep_op_poach_defender - the marginal block - no: dead at default 0.
- **Log line:** `deep-poachmarg` (only inside the gated branch; includes refusals).
- **Interacts with:** poach_deploy_gate (ordering, not a code dependency), defence_worth_mult, conquest_worth_mult, survival_horizon.
- **Tier/screen:** unranked (ALL only). Screen: dead-path.
- **Prior evidence:** STATUS "S-POACHFIX" - tech50war-200, nested ladder off/deploy/marg on seeds 17-24: deploy to marg +2.34 (se 4.99, t +0.47, 6/8); off to marg +3.44 (se 2.83, t +1.21, 5/8) - n=8, stated in STATUS as "not a result". Its own A/B at 16 seeds is listed as open (line 22449).

---

## Part 4: frontier transit and the founder floor

### frontier_transit_gate
- **Declared:** deeptuning.h:3677 (`int frontier_transit_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = today's border wall ...")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "let a settler's site search PATH THROUGH a peace partner's border to the free land beyond it, at a price per foreign tile entered." The wall is deep's own policy: `deep_settler_tile_behavior()` returns TB_IGNORE for every non-allied owner's tile, and pf drops such a node.
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (deep_tuning_get()->frontier_transit_gate > 0) { risk->base_EC = parameter->get_EC; parameter->get_EC = deep_settler_transit_EC; }` (deepsettler.c:408-413, `deep_settler_apply_risk`). The tile test is `case DEEP_TRUNK_OWN_PEACE: return tune->frontier_transit_gate > 0;` (deeptrunk.c:574, `deep_frontier_transit_open`, called from `deep_settler_tile_behavior` at deepsettler.c:200).
- **All call sites:**
  - deepsettler.c:408 - deep_settler_apply_risk - installs the per-foreign-tile cost callback - no: dead at default 0.
  - deeptrunk.c:574, 576, 580 - deep_frontier_transit_open - opens the tile-behaviour wall for peace (and, with the floor, cease-fire / war) owners - no: dead at default 0 (the function returns exactly `pplayers_allied()`'s old answer).
- **Log line:** `deep-transit` (one line per search that crossed a foreign tile; silent with the gate shut). `deep-sitewall` is an unconditional counterfactual census and does not read the gate.
- **Interacts with:** frontier_transit_price and frontier_transit_stance_floor. **When this gate is 0, both are dead:** the price is read only in `deep_settler_transit_EC`, which is installed only when the gate is non-zero, and the stance floor is read only after `frontier_transit_gate > 0 &&` in each case branch. Also trunk_soft_border_gate (a different gate, reads the same ownership classes for road corridors).
- **Tier/screen:** unranked (blocks M, ALL, MIL); not screened.
- **Prior evidence:**
  - `findings/frontier-verdict-2026-08-30.md` c10 (this gate alone) - contested (8 seats, T151): score -0.05 +/- 2.22 per seat (t -0.02, 10/20), cities @T75 +0.147 (se 0.059, t +2.48), @end -0.030, empty% T26-50 -10.79 (t -6.16), founders alive @T50 +0.562 (t +5.21), unitslost @T75 -0.690 (t -2.98); lategame-80 (6 seats, T81): +1.53 +/- 1.23 (t +1.24, 11/20) - n=20 per venue, 160 games. Mechanism win, score wash; gate stayed at 0.
  - `findings/fearsink-verdict-2026-08-30.md` / `marginal-city-2026-08-30.md` price-list reading: observed -0.65 +/- 2.42 (war) against predicted +0.77; +1.90 +/- 1.38 residual in econ.
  - `findings/stacked-lever-2026-08-31.md` (S-STACKED) - one of the five gates in the stack, n=100 per venue; transit fired 89 (war) and 361 (econ) times per arm. Stack result positive; see founder_site_cap_gate.

### frontier_transit_price
- **Declared:** deeptuning.h:3690 (`double frontier_transit_price;`); params.ini: default=1.0 min=0.0 max=8.0 kind=float; compiled default 1.0 (PLACEHOLDER, unmeasured)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "What one foreign tile of transit costs the path, in turns. PLACEHOLDER 1.0 and NOT MEASURED." 1.0 "means 'a tile of someone else's ground costs a turn'".
- **Unit:** turns (per foreign tile entered). It is multiplied by `PF_TURN_FACTOR` to give path extra-cost points; `common/aicore/path_finding.h:85-88` states that each EC point is 1/PF_TURN_FACTOR-th of one turn.
- **Formula at the primary call site:** `cost += (unsigned) (deep_tuning_get()->frontier_transit_price * (double) PF_TURN_FACTOR);` for a tile whose owner is non-NULL, not the unit's owner and not allied (deepsettler.c:180-181, `deep_settler_transit_EC`, chained after `adv_avoid_risks()`'s own EC).
- **All call sites:**
  - deepsettler.c:180 - deep_settler_transit_EC - per-foreign-tile path cost - no: the callback is installed only while `frontier_transit_gate` is non-zero.
- **Log line:** `deep-transit` (indirectly: `n=` counts crossings; the price is not printed).
- **Interacts with:** frontier_transit_gate (required), frontier_transit_stance_floor, settler_fearfulness_mult (through the chained `adv_avoid_risks` cost), site_travel_discount (de-ranks sites behind a costly corridor).
- **Tier/screen:** unranked (M, ALL, MIL); not screened.
- **Prior evidence:** none. Adjudicated only at the placeholder 1.0; `frontier-verdict` §7 lists it as owed to a tuning pass.

### frontier_transit_stance_floor
- **Declared:** deeptuning.h:3701 (`int frontier_transit_stance_floor;`); params.ini: default=0 min=0 max=2 kind=int; compiled default 0 ("PLACEHOLDER; 0 = a peace ...")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "How bad the relationship may get before transit is refused outright: 0 admits a PEACE PARTNER (DS_PEACE, DS_ARMISTICE) only, 1 also a cease-fire, 2 also war and no-contact."
- **Unit:** enum/sentinel (0 = peace and armistice owners only; 1 = adds cease-fire; 2 = adds war and no-contact). It is compared with `>= 1` and `>= 2`.
- **Formula at the primary call site:** `case DEEP_TRUNK_OWN_CEASE: return tune->frontier_transit_gate > 0 && tune->frontier_transit_stance_floor >= 1; default: return tune->frontier_transit_gate > 0 && tune->frontier_transit_stance_floor >= 2;` (deeptrunk.c:575-582, `deep_frontier_transit_open`).
- **All call sites:**
  - deeptrunk.c:577 and 581 - deep_frontier_transit_open - extends transit to cease-fire / war owners - no: short-circuited by `frontier_transit_gate > 0 &&`.
- **Log line:** `deep-transit` (indirect; the stance floor is not printed).
- **Interacts with:** frontier_transit_gate (required).
- **Tier/screen:** unranked (M, ALL, MIL); not screened.
- **Prior evidence:** none. The header argues 0 is "the only value the census argues for"; 1 and 2 have never been run.

### founder_floor_gate
- **Declared:** deeptuning.h:3730 (`int founder_floor_gate;`); params.ini: default=0 min=0 max=1 kind=int; compiled default 0 ("0 = an empty search zeroes ...")
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "an empty site search may not ZERO the founder want when the failure is one a turn could undo." When the failed search was refused something a turn could undo and the player still has counted sites, "the previous want DECAYS rather than being deleted."
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (tune->founder_floor_gate > 0 && undoable && prev != 0) { sites = deep_remaining_sites(pplayer); reach = ...; kept = (adv_want) (tune->founder_floor_want * (double) prev * reach); }` (deepsite.c:2754-2760, `deep_founder_floor`; caller deepsite.c:2837 in `deep_contemplate_new_city`, on the "want no city" branch).
- **All call sites:**
  - deepsite.c:2754 - deep_founder_floor - decides whether an empty search keeps a fraction of the previous want - no: dead at default 0.
  - deepsite.c:2767 - deep_founder_floor - logged as `gate=`.
- **Log line:** `deep-founderfloor` (one line per empty fresh search, gate on or off).
- **Interacts with:** founder_floor_want and founder_floor_reach_ref. **When this gate is 0, both are dead:** they are read only inside the `if (founder_floor_gate > 0 && undoable && prev != 0)` block. Also founder_join_guard_gate (reads the same `deep_site_blocked_seen_turn` signal).
- **Tier/screen:** unranked (blocks M, ALL, MIL); not screened.
- **Prior evidence:**
  - `findings/frontier-verdict-2026-08-30.md` c01 (this gate alone) - contested: score -10.17 +/- 1.38 per seat (t -7.36, 0/20), cities @end -0.276 (t -2.09), pop -4.523 (t -6.45); lategame-80: -0.77 +/- 1.48 (t -0.52, 12/20) - n=20 per venue. Failure mode was a hoard of founders; gate stayed at 0.
  - c11 (transit + floor): contested -7.99 +/- 1.93 (t -4.14, 4/20), lategame-80 -1.06 +/- 1.70; interaction +2.23 +/- 1.84 and -1.82 +/- 1.50 (inside SE).
  - `findings/stacked-lever-2026-08-31.md`: excluded from the stack "by design (-10.17 +/- 1.38 per seat, a hoard not a channel)".

### founder_floor_want
- **Declared:** deeptuning.h:3742 (`double founder_floor_want;`); params.ini: default=1.0 min=0.0 max=1.0 kind=float; compiled default 1.0 (PLACEHOLDER, unmeasured)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The fraction of the previous want that survives such a search. PLACEHOLDER 1.0 ... the limit case -- the undoable empty search does not touch the want at all." Warns of the failure mode: "a player permanently walled in keeps bidding for founders it cannot place."
- **Unit:** multiplier (dimensionless fraction 0-1 of the previous `founder_want`, which is in want units).
- **Formula at the primary call site:** `kept = (adv_want) (tune->founder_floor_want * (double) prev * reach);` (deepsite.c:2759, `deep_founder_floor`).
- **All call sites:**
  - deepsite.c:2759 - deep_founder_floor - retained fraction of the previous founder want - no: behind `founder_floor_gate > 0`.
- **Log line:** `deep-founderfloor ... kept=`.
- **Interacts with:** founder_floor_reach_ref (in the same product, via `reach`), founder_floor_gate.
- **Tier/screen:** unranked (M, ALL, MIL); not screened.
- **Prior evidence:** none as a swept value. The c01 arm (-10.17, n=20) ran it at the placeholder 1.0; the verdict says a decayed floor "is not thereby rescued" and a sweep was deliberately not run on an unadjudicated mechanism.

### founder_floor_reach_ref
- **Declared:** deeptuning.h:3758 (`double founder_floor_reach_ref;`); params.ini: default=1.0 min=0.5 max=8.0 kind=float; compiled default 1.0 (PLACEHOLDER, unmeasured)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The counted sites at which the floor is fully paid; below it the floor is scaled down in proportion, and at zero sites it is not paid at all." Counted sites is `deep_remaining_sites()`.
- **Unit:** count (sites; denominator of `sites / reach_ref`, a ratio clipped at 1.0).
- **Formula at the primary call site:** `reach = (tune->founder_floor_reach_ref > 0.0 ? MIN(1.0, sites / tune->founder_floor_reach_ref) : 1.0);` (deepsite.c:2756-2758, `deep_founder_floor`).
- **All call sites:**
  - deepsite.c:2756 and 2757 - deep_founder_floor - scales the floor with the remaining-site count - no: behind `founder_floor_gate > 0`.
- **Log line:** `deep-founderfloor ... sites= reach=`.
- **Interacts with:** founder_floor_want, founder_floor_gate.
- **Tier/screen:** unranked (M, ALL, MIL); not screened.
- **Prior evidence:** none as a swept value; the c01 arm ran it at 1.0. The frontier verdict notes `deep_remaining_sites()` "was supposed to bound [the hoard] and did not".

---

## Unresolved

- **Screen verdicts for most of this slice.** `knob-screen-2026-08-30.md` and `screen-corpus-2026-08-30.md` print full per-knob classes only through `knobscreen.py`'s own output, which I did not run. They name explicitly: capital_stake_gate/gain, site_improvable_gate, site_starve_price_gate, poach_marginal_gate (dead-path); colony_lift_weight (457/game) and lift_op_slots (841/game) (tier B partial); founder_tier_gate (256.9 flips/game, mode). Every other knob is marked "not screened" meaning "not named in those two documents". A zero-game run of `knobscreen.py` on the corpus would resolve it. The default-0 gates not named (hull_commission_weight, lift_muster_release, founder_berth_route, founder_run_hold, founder_site_cap_gate, founder_join_guard_gate, poach_deploy_gate, frontier_transit_gate, founder_floor_gate) would be expected to be dead-path or mode by construction, but I did not verify it.
- **Compiled versus params.ini defaults for lift_op_slots (0 vs 4), lift_manifest_max (8 vs 3), lift_drop_radius (12 vs 2).** I have not found a document stating which is intended as the tuner's starting point; `tune-audit.py` does not compare them. `tune.py` has no reference to those names. The STATUS entry at line 3846 recommended `lift_manifest_max = 1` as the default; the compiled default is 8 and the header cites "human call 2026-08-04". Not reconciled.
- **hut_risk_* barbarian condition.** `contested.conf` and `lategame-80.conf` set no `barbarians` line (only hutrisk-80, raging-120, nobarb-120, storm-150 and the garden configs do), so those two venues use the server's default barbarian rate and onset. I did not check what those defaults are, so I cannot say if hut_risk_radius/discount are live there. `knob-coverage.md` (2026-08-05) lists them as NO-INSTRUMENT, which predates the `huts_disc=` field.
- **hut_risk_honest A/B.** STATUS lists it as owed (hutrisk-80 and the explore_hut_value factorial); I found no later result.
- **S-STACKED holdout.** The stacked-lever findings doc says the holdout (seeds 101-120, 83 games) was running and would be appended; the copy of the doc I read has no holdout table, so the stacked numbers above are training-seed numbers only.
- **Line numbers for STATUS.md.** References such as "line ~3780" marked with `~` are approximate section locations; the numbers given for the quoted results were read from the cited sections.
- **founder_run_hold row in the siteplan table versus the price-list table.** `siteplan-verdict` reports c01 score +2.12 (se 10.12, war) and the S-MARGINAL price-list table reports -0.63 +/- 1.42 per seat for the same arm; the units differ (the former appears to be per game and the latter per seat), which I did not confirm from the code.
