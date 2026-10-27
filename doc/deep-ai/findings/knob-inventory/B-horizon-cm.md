# B-horizon-cm knob inventory (62 knobs)

Facts only. Source of truth: ai/deep/deeptuning.h, ai/deep/deeptuning.c (compiled defaults, which match benchmark/tuning-params.ini for all 62), call sites in ai/deep/*.c, benchmark/tuning-blocks.ini, doc/deep-ai/findings/knob-screen-2026-08-30.md + screen-corpus-2026-08-30.md + knob-coverage.md, doc/deep-ai/STATUS.md. Line numbers are as of the working tree on branch epic/w03-horizon-content (HEAD b42d64d7a8).

Reading notes: (1) "live at defaults" refers to COMPILED defaults (deeptuning.c), not benchmark/tuning/pv3.ini, which overrides many of these (values quoted where relevant). (2) "Prior A/B" word is the sign of the best-matching evidence; most arms moved several knobs jointly and the entry says so -- a joint arm is attributed to every knob it moved. (3) building_want[] units: server/advisors/infracache.h:28-32, "output multiplied by a priority"; SHIELD_WEIGHTING=17, TRADE_WEIGHTING=18, FOOD_WEIGHTING=30 (server/advisors/advbuilding.h:19-21); amortize(b,d)=b*(1-1/MORT)^d, MORT=24 (advtools.c:32).

## Summary table

| knob | unit | default | live at defaults? | tier | screen verdict | prior A/B | # call sites |
|---|---|---|---|---|---|---|---|
| cm_happy_factor | CM weight (int) | 1 | yes | unranked | not screened | none | 1 |
| cm_war_food | CM weight (int) | 10 | yes | unranked | not screened | none | 2 |
| cm_war_shield | CM weight (int) | 12 | yes | unranked | not screened | none | 1 |
| cm_war_trade | CM weight (int) | 1 | yes | unranked | not screened | none | 1 |
| cm_cap_food | CM weight (int) | 2 | yes | unranked | not screened | negative | 1 |
| cm_cap_shield | CM weight (int) | 8 | yes | unranked | not screened | negative | 1 |
| cm_cap_trade | CM weight (int) | 8 | yes | unranked | not screened | negative | 1 |
| cm_wonder_shield | CM weight (int) | 10 | yes | unranked | not screened | negative | 1 |
| cm_wonder_trade | CM weight (int) | 4 | yes | unranked | not screened | negative | 1 |
| cm_grow_below | count (city size) | 6 | yes | unranked | not screened | negative | 1 |
| cm_grow_safety_margin | multiplier (x danger) | 1.0 | yes (conditional) | unranked | not screened | negative | 1 |
| cm_grow_food | CM weight (int) | 16 | yes | unranked | not screened | negative | 1 |
| cm_grow_shield | CM weight (int) | 4 | yes | unranked | not screened | negative | 1 |
| cm_grow_trade | CM weight (int) | 4 | yes | unranked | not screened | negative | 1 |
| cm_founder_food | multiplier (x founder food upkeep) | 0.0 | no | unranked | dead at defaults (not screened) | none | 2 |
| cm_trade_quality_slope | exponent | 0.5 | yes | unranked | not screened | negative | 1 |
| cm_grow_price_blend | multiplier (citizen vs build want) | 0.0 | no | unranked | dead at defaults (not screened) | negative | 2 |
| cm_specialist_gold_gain | multiplier (x shortfall frac) | 0.0 | no | unranked | dead at defaults (not screened) | none | 3 |
| cm_specialist_sci_gain | multiplier (x shortfall frac) | 0.0 | no | unranked | dead at defaults (not screened) | none | 3 |
| horizon_capacity_weight | multiplier on tile-sum credit (building_want units) | 1.0 | yes | A | tier A; flips 11.2/game (0.5%) | null | 3 |
| horizon_growth_weight | multiplier on tile-sum credit | 0.5 | yes | A | tier A; flips 78.9/game (3.4%) | null | 2 |
| horizon_bundle_weight | multiplier on effect-value x tiles (lump) | 1.0 | yes | A | tier A; flips 83.9/game (3.6%) | null | 1 |
| horizon_health_weight | multiplier on citizens x tile-sum | 1.0 | yes (conditional) | unranked | not screened | none | 1 |
| horizon_content_weight | multiplier on tile-sum credit | 1.0 | yes | A | tier A; flips 753/game (32.3%) | null | 2 |
| horizon_multiplier_weight | multiplier on tile-sum credit | 1.0 | yes | A | tier A; flips 144.6/game (6.2%) | null | 2 |
| cm_war_survival | probability 0-1 | 0.9 | yes | unranked | not screened | positive | 3 |
| cm_war_growth_charge | multiplier (x food-loss frac -> prob) | 0.0 | no | unranked | dead at defaults (not screened) | negative | 3 |
| horizon_tech_weight | multiplier -> tech_want units | 1.0 | yes (conditional) | unranked | not screened | none | 1 |
| worker_flee_radius | tiles | 3 | yes | B | tier B; fires 427/game, no flip rate | none | 2 |
| worker_guard_radius | tiles | 6 | yes | B | tier B; not individually screened | none | 4 |
| worker_danger_discount | multiplier (divisor 1+d) | 1.0 | yes | unranked | not screened | none | 2 |
| worker_cross_continent_discount | multiplier (turns x (1+d)) | 1.0 | yes (conditional) | unranked | not screened | null | 1 |
| worker_travel_discount | multiplier (amortize turns per walked turn) | 0.0 | no | unranked | dead at defaults (not screened) | negative | 3 |
| idle_policy_gate | boolean gate | 0 | no | unranked | dead at defaults (not screened) | positive | 3 |
| idle_reserve_per_city | count (workers/city) | 0.0 | no | unranked | dead at defaults (not screened) | null | 2 |
| trunk_road_gain | want units per move point saved | 0.0 | no | unranked | dead-path | negative | 6 |
| trunk_source_gain | multiplier | 0.0 | no | unranked | dead-path | null | 3 |
| trunk_ally_gate | boolean gate | 0 | no | unranked | dead-path | null | 1 |
| trunk_trade_gain | multiplier (dividend -> want) | 0.0 | no | unranked | dead-path | null | 3 |
| trunk_keepable_gate | boolean gate | 0 | no | unranked | dead-path | positive | 1 |
| trunk_keep_margin | multiplier (fraction premium) | 0.0 | no | unranked | dead-path | positive | 3 |
| trunk_soft_border_gate | boolean gate | 0 | no | unranked | dead-path | none | 1 |
| trunk_risk_weight | move fragments per fear point | 0.0 | no | unranked | dead-path | none | 3 |
| trunk_stack_gate | boolean gate | 0 | no | unranked | dead-path | null | 1 |
| trunk_upgrade_gate | boolean gate | 0 | no | unranked | dead-path | positive | 2 |
| rotation_power_ratio | multiplier (defense ratio) | 2.0 | yes | unranked | not screened | none | 3 |
| rotation_want | want units (danger points) | 35 | yes | unranked | not screened | none | 2 |
| rotation_finish_relax | multiplier | 0.5 | yes (conditional) | unranked | not screened | none | 1 |
| pop_insurance_gain | multiplier on citizens x tile-sum | 0.0 | no | unranked | dead at defaults (not screened) | null | 4 |
| heal_stream_gain | multiplier on shield-turns x 17 | 0.0 | no | unranked | dead at defaults (not screened) | negative | 3 |
| veteran_stream_gain | multiplier on shields x 17 | 0.0 | no | unranked | dead at defaults (not screened) | negative | 4 |
| horizon_decay_gate | boolean gate | 0 | no | unranked | dead at defaults (not screened) | positive | 3 |
| horizon_decay_early | probability 0-1 per turn | 0.0 | no | unranked | dead at defaults (not screened) | positive | 2 |
| horizon_decay_late | probability 0-1 per turn | 0.0 | no | unranked | dead at defaults (not screened) | positive | 2 |
| horizon_decay_break_start | turns (game turn) | 50 | no | unranked | dead at defaults (not screened) | positive | 1 |
| horizon_decay_break_end | turns (game turn) | 100 | no | unranked | dead at defaults (not screened) | positive | 1 |
| horizon_govcenter_weight | multiplier on per-turn saving x horizon | 0.0 | no | unranked | dead at defaults (not screened) | null | 2 |
| horizon_upkeepfree_weight | multiplier on per-turn saving x horizon | 0.0 | no | unranked | dead at defaults (not screened) | null | 2 |
| horizon_culture_weight | multiplier on score-pts x citizen value | 0.0 | no | unranked | dead at defaults (not screened) | negative | 3 |
| techgoal_deep | boolean gate | 0 | no | unranked | dead at defaults (not screened) | null | 1 |
| techgoal_distance_power | exponent | 1.0 | no | unranked | dead at defaults (not screened) | null | 1 |
| terrain_chain_gate | boolean gate | 0 | no | unranked | dead at defaults (not screened) | null | 1 |

## Per-knob sections

### cm_happy_factor
- **Declared:** deeptuning.h:1326 (`int cm_happy_factor;`); params.ini: default=1 min=0 max=6 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "City manager factors by city state (W09-lite); the stock hardcoded default is food 10-20, shield 5, trade 0, happy 0." (shared comment over cm_happy_factor..cm_wonder_trade)
- **Unit:** UNIT: weight (int) on `struct cm_parameter.happy_factor` (common/aicore/cm.h:48), the CM objective weight per happy citizen; stock AI leaves it 0. Enters the CM weighted-sum objective, so unit = CM objective weight (same family as factor[O_*]).
- **Formula at the primary call site:** deepcm.c:438 deep_cm_apply_posture(): `cmp->happy_factor = tune->cm_happy_factor;` (runs for every posture except NO_POSTURE).
- **All call sites:**
  - ai/deep/deepcm.c:438 -- deep_cm_apply_posture -- cmp->happy_factor for WAR/CAP/WONDER/GROW cities -- yes (default 1)
  - live at compiled defaults summary: yes
- **Log line:** none (no per-city CM posture line exists; knob-coverage.md 4c)
- **Interacts with:** cm_war_*/cm_cap_*/cm_wonder_*/cm_grow_* (all factors in the same cm_parameter)
- **Tier/screen:** unranked (tier C, pinned); in blocks G, GCM, ECON, ALL. knobcensus 2026-08: NO-INSTRUMENT. Not in knob-screen tables. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 GCM ablation does not move it (pv3.ini:39 = 1). none single-knob.

### cm_war_food
- **Declared:** deeptuning.h:1327 (`int cm_war_food;`); params.ini: default=10 min=0 max=25 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared comment. Food factor in the WAR posture (and, as coded, also in the WONDER posture).
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_FOOD]` (cm.h:47). Dimension: CM objective weight per unit of food surplus (stock default 10-20).
- **Formula at the primary call site:** deepcm.c:416 deep_cm_apply_posture() case DEEP_CM_WAR: `cmp->factor[O_FOOD] = tune->cm_war_food;`; deepcm.c:426 case DEEP_CM_WONDER: `cmp->factor[O_FOOD] = tune->cm_war_food;` (WONDER has no cm_wonder_food; it reuses this knob).
- **All call sites:**
  - ai/deep/deepcm.c:416 -- deep_cm_apply_posture -- food factor when threatened (wave>0 && survival < cm_war_survival) -- yes (default 10)
  - ai/deep/deepcm.c:426 -- deep_cm_apply_posture -- food factor when city is building a wonder (peace) -- yes
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_war_shield, cm_war_trade (same posture); cm_wonder_shield/cm_wonder_trade (WONDER posture); cm_war_survival (selects WAR); cm_trade_quality_slope (scales trade only)
- **Tier/screen:** unranked (tier C). In blocks M, MIL, G, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): M, ALL, MIL.
- **Prior evidence:** STATUS.md:792 -- campaign covariate table (G/H/H2 finals) agreed on "cut cm_war_food" -- observational direction, not an A/B. pv3 leaves it at 10 (pv3.ini:40).

### cm_war_shield
- **Declared:** deeptuning.h:1328 (`int cm_war_shield;`); params.ini: default=12 min=0 max=25 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared comment. Shield factor in the WAR posture ("Under threat: shields now").
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_SHIELD]`; CM objective weight per shield surplus (stock default 5).
- **Formula at the primary call site:** deepcm.c:417 deep_cm_apply_posture() case DEEP_CM_WAR: `cmp->factor[O_SHIELD] = tune->cm_war_shield;`
- **All call sites:**
  - ai/deep/deepcm.c:417 -- deep_cm_apply_posture -- shield factor when threatened -- yes (default 12; only fires for threatened cities)
  - live at compiled defaults summary: yes
- **Log line:** none (deep-cmpost carries warf/wars/wart surpluses only when cm_war_growth_charge>0)
- **Interacts with:** cm_war_food, cm_war_trade, cm_war_survival, cm_war_growth_charge
- **Tier/screen:** unranked (tier C). Blocks M, MIL, G, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): M, ALL, MIL.
- **Prior evidence:** STATUS.md:147 -- only a plumbing check that cm_war_shield=15 reached the log (tuning-file read-back); no effect measurement.

### cm_war_trade
- **Declared:** deeptuning.h:1329 (`int cm_war_trade;`); params.ini: default=1 min=0 max=15 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared comment. Trade factor in the WAR posture.
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_TRADE]`; then multiplied by pow(trade_richness, cm_trade_quality_slope) at deepcm.c:445 (richness = (1+workable trade)/(1+workable shields), dimensionless).
- **Formula at the primary call site:** deepcm.c:418 `cmp->factor[O_TRADE] = tune->cm_war_trade;` then deepcm.c:443-447 `scaled = cmp->factor[O_TRADE] * pow(deep_cm_trade_richness(pcity), tune->cm_trade_quality_slope); cmp->factor[O_TRADE] = MAX(0, (int)(scaled + 0.5));`
- **All call sites:**
  - ai/deep/deepcm.c:418 -- deep_cm_apply_posture -- trade factor when threatened -- yes (default 1)
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_trade_quality_slope (live at 0.5 -> rescales it), cm_war_food, cm_war_shield
- **Tier/screen:** unranked (tier C). Blocks M, MIL, G, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): M, ALL, MIL.
- **Prior evidence:** none

### cm_cap_food
- **Declared:** deeptuning.h:1330 (`int cm_cap_food;`); params.ini: default=2 min=0 max=25 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared comment; comment in code: "Growth-capped: food is nearly worthless, work the mines and trade specials instead."
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_FOOD]` in the CAP posture (city_can_grow_to(size+1) is false).
- **Formula at the primary call site:** deepcm.c:421 deep_cm_apply_posture() case DEEP_CM_CAP: `cmp->factor[O_FOOD] = tune->cm_cap_food;`
- **All call sites:**
  - ai/deep/deepcm.c:421 -- deep_cm_apply_posture -- food factor for growth-capped, non-threatened cities -- yes (default 2)
  - (comment) ai/deep/deephorizon.c:480,512 -- deep_counterfactual_ttg -- doc text only (the counterfactual food arrangement is NOT read from this knob)
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_cap_shield, cm_cap_trade (CAP posture); cm_war_survival (WAR pre-empts CAP)
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 3 vs default 2 (x1.50) per STATUS.md:16032-16036.

### cm_cap_shield
- **Declared:** deeptuning.h:1331 (`int cm_cap_shield;`); params.ini: default=8 min=0 max=25 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared comment. Shield factor in CAP posture.
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_SHIELD]` (CAP).
- **Formula at the primary call site:** deepcm.c:422 `cmp->factor[O_SHIELD] = tune->cm_cap_shield;`
- **All call sites:**
  - ai/deep/deepcm.c:422 -- deep_cm_apply_posture -- shield factor, growth-capped cities -- yes (default 8)
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_cap_food, cm_cap_trade
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 9 vs 8 (x1.12).

### cm_cap_trade
- **Declared:** deeptuning.h:1332 (`int cm_cap_trade;`); params.ini: default=8 min=0 max=25 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared comment; deepcm.c:192 documents "G cut cm_cap_trade for cramped and bankrupted isolated; H kept it high" (STATUS 2026-07-30).
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_TRADE]` (CAP), then x pow(trade_richness, cm_trade_quality_slope).
- **Formula at the primary call site:** deepcm.c:423 `cmp->factor[O_TRADE] = tune->cm_cap_trade;` then scaled at deepcm.c:443-447 as for cm_war_trade.
- **All call sites:**
  - ai/deep/deepcm.c:423 -- deep_cm_apply_posture -- trade factor, growth-capped cities -- yes (default 8)
  - (comment) ai/deep/deepcm.c:192 -- deep_cm_trade_richness doc
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_trade_quality_slope, cm_cap_food, cm_cap_shield
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 5 vs 8 (x0.62). Also STATUS 2026-07-30 isolated -405 lesson cited at deepcm.c:192 (pre-benchmark-discipline).

### cm_wonder_shield
- **Declared:** deeptuning.h:1333 (`int cm_wonder_shield;`); params.ini: default=10 min=0 max=25 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared comment; "Wonder under construction: shields, with trade kept up to fund completion."
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_SHIELD]` (WONDER posture: peace-time city whose production is a wonder).
- **Formula at the primary call site:** deepcm.c:427 `cmp->factor[O_SHIELD] = tune->cm_wonder_shield;`
- **All call sites:**
  - ai/deep/deepcm.c:427 -- deep_cm_apply_posture -- shield factor, wonder-building cities -- yes (default 10)
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_war_food (supplies WONDER food factor), cm_wonder_trade
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 6 vs 10 (x0.60).

### cm_wonder_trade
- **Declared:** deeptuning.h:1334 (`int cm_wonder_trade;`); params.ini: default=4 min=0 max=15 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared comment. Trade factor, WONDER posture.
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_TRADE]` (WONDER), then x pow(richness, cm_trade_quality_slope).
- **Formula at the primary call site:** deepcm.c:428 `cmp->factor[O_TRADE] = tune->cm_wonder_trade;`
- **All call sites:**
  - ai/deep/deepcm.c:428 -- deep_cm_apply_posture -- trade factor, wonder-building cities -- yes (default 4)
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_wonder_shield, cm_war_food, cm_trade_quality_slope
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 7 vs 4 (x1.75).

### cm_grow_below
- **Declared:** deeptuning.h:1338 (`int cm_grow_below;`); params.ini: default=6 min=0 max=12 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Grow-hard state: cities below cm_grow_below (and not at war, capped, or wonder-building) weight food heavily -- growth compounds and civ2civ3 has no rapture. 0 disables the state."
- **Unit:** UNIT: count (city size in citizens, int). Compared with `city_size_get(pcity)`; not a multiplier.
- **Formula at the primary call site:** deepcm.c:365-373 deep_cm_peace_posture(): `if ((city_size_get(pcity) < tune->cm_grow_below || deep_cm_grow_priced(ait, pcity)) && (city_data->danger == 0 || (assess_defense(ait, pcity) + deep_cm_sortie_value(pcity) >= tune->cm_grow_safety_margin * city_data->danger && deep_cm_survival_ok(pcity)))) return DEEP_CM_GROW;`
- **All call sites:**
  - ai/deep/deepcm.c:365 -- deep_cm_peace_posture -- size floor for entering GROW posture -- yes (default 6)
  - (comment) ai/deep/deepcm.c:263 -- deep_cm_grow_priced doc
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_grow_price_blend (OR-ed with it), cm_grow_safety_margin, cm_grow_food/shield/trade, sortie_defence_weight, survival_target
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 7 vs 6 (x1.17). findings/tuning-first-principles-2026-08-06.md:53 proposes 8 (ruleset size-8 cap) -- derivation, not an arm.

### cm_grow_safety_margin
- **Declared:** deeptuning.h:1343 (`double cm_grow_safety_margin;`); params.ini: default=1.0 min=0.25 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Grow-hard resumes when linear defence exceeds cm_grow_safety_margin x linear danger. Below 1 trusts the defence multipliers...; above 1 prices sequential attrition. Neutral 1.0 until tuned."
- **Unit:** multiplier (dimensionless) on `city_data->danger` (stock danger units, same currency as assess_defense()+sortie value); the comparison target is assess_defense()+deep_cm_sortie_value().
- **Formula at the primary call site:** deepcm.c:369-371 deep_cm_peace_posture(): `assess_defense(ait, pcity) + deep_cm_sortie_value(pcity) >= tune->cm_grow_safety_margin * city_data->danger`
- **All call sites:**
  - ai/deep/deepcm.c:370 -- deep_cm_peace_posture -- defence-vs-danger release test for GROW -- yes, but only evaluated when city_data->danger != 0 (the `danger == 0 ||` short-circuits it)
  - live at compiled defaults summary: yes (conditional: danger != 0)
- **Log line:** none (deep-cmgrow logs only when cm_grow_price_blend>0)
- **Interacts with:** sortie_defence_weight, cm_grow_below, cm_grow_price_blend, survival_target, survival_horizon
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 2.4558 vs 1.0 (x2.46, the largest GCM override).

### cm_grow_food
- **Declared:** deeptuning.h:1348 (`int cm_grow_food;`); params.ini: default=16 min=4 max=30 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** NO comment of its own: in deeptuning.h:1346-1350 the comment block above cm_grow_food/shield/trade ("Weight on garrison attack ratings counted as defence in the grow-release coverage test ... 0 disables.") documents sortie_defence_weight (declared at :1347). Behavioural intent from code (deepcm.c:429 "DEEP_CM_GROW") and the cm_grow_below comment: weight food heavily when growing.
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_FOOD]` (GROW posture).
- **Formula at the primary call site:** deepcm.c:431 deep_cm_apply_posture() case DEEP_CM_GROW: `cmp->factor[O_FOOD] = tune->cm_grow_food;`
- **All call sites:**
  - ai/deep/deepcm.c:431 -- deep_cm_apply_posture -- food factor for young cities (size<cm_grow_below or priced-in) with defence covering danger -- yes (default 16)
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_grow_shield, cm_grow_trade, cm_grow_below, cm_founder_food (reuses GROW posture factors), cm_grow_safety_margin
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 18 vs 16 (x1.12).

### cm_grow_shield
- **Declared:** deeptuning.h:1349 (`int cm_grow_shield;`); params.ini: default=4 min=0 max=15 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** NO comment of its own (see cm_grow_food: the preceding comment block belongs to sortie_defence_weight).
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_SHIELD]` (GROW).
- **Formula at the primary call site:** deepcm.c:432 `cmp->factor[O_SHIELD] = tune->cm_grow_shield;`
- **All call sites:**
  - ai/deep/deepcm.c:432 -- deep_cm_apply_posture -- shield factor, GROW posture -- yes (default 4)
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_grow_food, cm_grow_trade
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 6 vs 4 (x1.50).

### cm_grow_trade
- **Declared:** deeptuning.h:1350 (`int cm_grow_trade;`); params.ini: default=4 min=0 max=15 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** NO comment of its own (see cm_grow_food).
- **Unit:** UNIT: weight (int) -> `cm_parameter.factor[O_TRADE]` (GROW), then x pow(richness, cm_trade_quality_slope).
- **Formula at the primary call site:** deepcm.c:433 `cmp->factor[O_TRADE] = tune->cm_grow_trade;`
- **All call sites:**
  - ai/deep/deepcm.c:433 -- deep_cm_apply_posture -- trade factor, GROW posture -- yes (default 4)
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_grow_food, cm_grow_shield, cm_trade_quality_slope
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 9 vs 4 (x2.25).

### cm_founder_food
- **Declared:** deeptuning.h:1367 (`double cm_founder_food;`); params.ini: default=0.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Above 0 this adds a last arm to that machine -- fires only where DEEP_CM_NO_POSTURE would have handed the city back to the stock default -- which takes the grow posture and floors the food surplus at cm_founder_food x the founder's food upkeep. 0 is the off switch and the shipped default... PLACEHOLDER."
- **Unit:** multiplier (dimensionless) on the founder unit's food upkeep (food/turn, `utype_upkeep_cost(ftype,NULL,pplayer,O_FOOD)`); product is a food-surplus floor (food units per turn) written to `cmp.minimal_surplus[O_FOOD]`.
- **Formula at the primary call site:** deepcm.c:764 deep_set_city_manager_params(): `food_floor = (int) ceil(tune->cm_founder_food * upkeep);` then `cmp.minimal_surplus[O_FOOD] = food_floor` if larger (deepcm.c:776-777).
- **All call sites:**
  - ai/deep/deepcm.c:754 -- deep_set_city_manager_params -- gate: NO_POSTURE city with founder_want != 0 -- NO: dead behind cm_founder_food>0 (default 0.0)
  - ai/deep/deepcm.c:764 -- deep_set_city_manager_params -- food_floor = ceil(knob*upkeep) -- NO: same gate
  - live at compiled defaults summary: no (gate: cm_founder_food > 0)
- **Log line:** deep-cmfound (deepcm.c:766; fires only with knob>0)
- **Interacts with:** cm_grow_food/shield/trade (reuses the GROW factors), founder_pool_per_site (founder want), cm_war_survival
- **Tier/screen:** unranked (tier C); in blocks G, GCM, ECON, ALL. knobcensus: not in the 2026-08-07 map. Dead at defaults by construction. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** STATUS.md:12974-13038 (expansion slice 1): knob liveness only -- 2 lategame-80 games at 1.0, 69 and 76 deep-cmfound firings, no rate comparison; off-switch proof 10/10 byte-identical; "NO A/B, and nothing here claims the fix pays"; the owed arm (cm_founder_food=1.0, lategame-80, >=16 seeds) is listed as unrun.

### cm_trade_quality_slope
- **Declared:** deeptuning.h:1371 (`double cm_trade_quality_slope;`); params.ini: default=0.5 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Exponent on catchment trade-richness (workable trade / workable shields) scaling every CM state's trade factor: the map sets the trade appetite. 0 restores plain constants; 1 is proportional."
- **Unit:** exponent (dimensionless). Applied as pow(richness, slope) to the integer trade factor; richness = (1 + sum workable O_TRADE)/(1 + sum workable O_SHIELD) over the catchment.
- **Formula at the primary call site:** deepcm.c:443-447 deep_cm_apply_posture(): `double scaled = cmp->factor[O_TRADE] * pow(deep_cm_trade_richness(pcity), tune->cm_trade_quality_slope); cmp->factor[O_TRADE] = MAX(0, (int)(scaled + 0.5));`
- **All call sites:**
  - ai/deep/deepcm.c:440 -- deep_cm_apply_posture -- gate `slope > 0`; line 445 the exponent -- scales every posture's trade factor -- yes (default 0.5)
  - live at compiled defaults summary: yes
- **Log line:** none
- **Interacts with:** cm_war_trade, cm_cap_trade, cm_wonder_trade, cm_grow_trade
- **Tier/screen:** unranked (tier C). Blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned value 0.675 vs 0.5 (x1.35).

### cm_grow_price_blend
- **Declared:** deeptuning.h:1385 (`double cm_grow_price_blend;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "This blend admits a city whose marginal CITIZEN (deep_unworked_tile_value) beats its marginal BUILD (the top of this city's building_want[]), scaled by the blend ... 0 = off = the size threshold alone."
- **Unit:** multiplier (dimensionless) on `citizen` = deep_unworked_tile_value(pcity, adv, 1, NULL) (mean best-1 unworked-tile weighted yield, building_want units "output x priority") compared against `build` = max building_want[] of the city. Genuine same-currency trade-off weight (findings/cm-completion-2026-08-07.md section 3).
- **Formula at the primary call site:** deepcm.c:323 deep_cm_grow_priced(): `fires = tune->cm_grow_price_blend * citizen >= build;`
- **All call sites:**
  - ai/deep/deepcm.c:308 -- deep_cm_grow_priced -- early return FALSE when blend<=0 -- NO: dead (default 0.0)
  - ai/deep/deepcm.c:323 -- deep_cm_grow_priced -- citizen-vs-build admission to GROW -- NO: same gate
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-cmgrow (deepcm.c:331; only when blend>0)
- **Interacts with:** cm_grow_below (OR-ed), cm_grow_safety_margin, horizon_* (they shape building_want[]), cm_grow_food/shield/trade
- **Tier/screen:** unranked (tier C); blocks G, GCM, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); the tuned package set it to 0.154 (off a compiled 0). findings/cm-completion-2026-08-07.md section 3: smoke probe at 1.0, 89 of 545 evaluations fire (after fixing a vacuous build=-1 side) -- liveness only.

### cm_specialist_gold_gain
- **Declared:** deeptuning.h:1397 (`double cm_specialist_gold_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Gains that let empire solvency choose between a taxman and a scientist (DARKFX item 4)... scale a shortfall fraction in [0, 1]: gold, the treasury against dai_gold_reserve()'s buy float... Both 0 = off = the pinned 2/2. NOTE these are the CM's output factors, so they weight trade-derived gold and science as well as the specialists."
- **Unit:** multiplier (dimensionless) on shortfall fraction in [0,1]; result scales the integer `cmp->factor[O_GOLD]` (base 2): factor = round(2 * (1 + gain*shortfall)). Units of factor[O_GOLD] = CM objective weight.
- **Formula at the primary call site:** deepcm.c:130-133 deep_cm_solvency_factors(): `cmp->factor[O_GOLD] = MAX(0, (int)(cmp->factor[O_GOLD] * (1.0 + tune->cm_specialist_gold_gain * shortfall) + 0.5));` with `shortfall = CLIP(0, (reserve - gold)/reserve, 1)`, reserve = dai_gold_reserve().
- **All call sites:**
  - ai/deep/deepcm.c:123 -- deep_cm_solvency_factors (called from deep_cm_base) -- gate -- NO: dead (default 0.0)
  - ai/deep/deepcm.c:132 -- deep_cm_solvency_factors -- factor[O_GOLD] scale -- NO
  - ai/deep/deepcm.c:800 -- deep_set_city_manager_params -- gate on the deep-cmspec census line -- NO
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-cmspec (deepcm.c:811; only when either gain>0)
- **Interacts with:** cm_specialist_sci_gain, dai_gold_reserve (stock), all postures (applies in deep_cm_base to every posture)
- **Tier/screen:** unranked (tier C); blocks G, GCM, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** none (pv3 sets it 0.0). findings/darkfx-2026-08-06.md:258 is the design note.

### cm_specialist_sci_gain
- **Declared:** deeptuning.h:1398 (`double cm_specialist_sci_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same comment as cm_specialist_gold_gain; science side: ruleset tech-upkeep bill against last turn's actual bulbs.
- **Unit:** multiplier (dimensionless) on shortfall fraction in [0,1] = (tech_upkeep - bulbs_last_turn)/tech_upkeep; scales integer `cmp->factor[O_SCIENCE]` (base 2).
- **Formula at the primary call site:** deepcm.c:145-148 deep_cm_solvency_factors(): `cmp->factor[O_SCIENCE] = MAX(0, (int)(cmp->factor[O_SCIENCE] * (1.0 + tune->cm_specialist_sci_gain * shortfall) + 0.5));`
- **All call sites:**
  - ai/deep/deepcm.c:137 -- deep_cm_solvency_factors -- gate (also needs tech_upkeep_style != NONE) -- NO: dead (default 0.0)
  - ai/deep/deepcm.c:147 -- deep_cm_solvency_factors -- factor[O_SCIENCE] scale -- NO
  - ai/deep/deepcm.c:801 -- deep_set_city_manager_params -- gate on deep-cmspec -- NO
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-cmspec (only when a gain>0)
- **Interacts with:** cm_specialist_gold_gain; game.info.tech_upkeep_style (ruleset)
- **Tier/screen:** unranked (tier C); blocks G, GCM, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** none (pv3 sets it 0.0).

### horizon_capacity_weight
- **Declared:** deeptuning.h:295 (`double horizon_capacity_weight;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Horizon credits for buildings (W03), in the tile-sum currency: scale on the unlocked-growth credit for capacity buildings..." (shared comment over horizon_capacity/growth/bundle_weight).
- **Unit:** multiplier (dimensionless) on a horizon-discounted SUM of per-turn weighted yields: per_citizen * steps (number of growth steps within MORT/ttg) amortized by ttg turns. `per_citizen` = deep_unworked_tile_value(pcity, adv, 3, NULL) (deephorizon.c:389) = mean over the best 3 currently-unworked workable tiles of `food*adv->food_priority + trade*adv->science_priority + shield*adv->shield_priority` -- a PER-TURN weighted-yield (tile-sum) of ONE extra citizen. building_want[] is stock's per-building desirability array in "output x priority" units (server/advisors/infracache.h:28-32: "output (shields/gold/luxuries) multiplied by a priority"); deep_adjust_building_wants() (deephorizon.c:2037) ADDS each credit to pcity->server.adv->building_want[id] (deephorizon.c:2475-2476) after stock has filled it (stock base_want()/dai_city_want() output); the building auction then compares building_want against unit/military wants. The comparison target therefore defines the unit: want/score units of the building auction.
- **Formula at the primary call site:** deephorizon.c:579 deep_capacity_credit(): `credit = amortize(per_citizen * steps * tune->horizon_capacity_weight, ttg);` with `steps = MAX(1, deep_horizon_turns() / ttg)` (MORT=24), ttg from deep_counterfactual_ttg(); then `credit = deep_decay_reprice(...)`. Added to building_want[] for improvements with EFT_SIZE_ADJ/EFT_SIZE_UNLIMIT only (deephorizon.c:2224-2230 (add += cap_credit at :2229)) -- also the only credit that survives the wartime gate.
- **All call sites:**
  - ai/deep/deephorizon.c:579 -- deep_capacity_credit -- unlocked-growth credit for cap-lifters in growth-capped cities (per_citizen*steps*w, amortized) -- yes (default 1.0; only for capped cities)
  - ai/deep/deephorizon.c:586 -- deep_capacity_credit -- same product, logged into the deep-decay shape line (b = per_citizen*w) -- yes
  - ai/deep/deephorizon.c:2213 (indirect) -- deep_adjust_building_wants -- cap_credit is also the base of horizon_tech_weight routing -- yes
  - live at compiled defaults summary: yes
- **Log line:** deep-capacity (deephorizon.c:592, per capped city per turn, credit=), deep-decay fam=capacity, deep-decay-sum capadd=, deep-credit add=
- **Interacts with:** horizon_tech_weight (shares cap_credit), horizon_decay_gate/early/late (re-prices steps), upkeep_gold_weight (upkeep bill subtracted from the sum), expiry_building_gain, endgame_horizon (clamps MORT), stance_growth_gain (gmult)
- **Tier/screen:** TIER A (block AUCTION, tuning-blocks.ini:95). knob-screen Part 1: flips 11.2/game (7.7 on the s5 replication), 0.5% flip rate, replay rule building_anchor, 2329 decisions/game, sigma 0.667. Blocks containing it (tuning-blocks.ini): AUCTION, G, ALL, ECON, GHORIZON.
- **Prior evidence:** none single-knob. Inside pv3 at 1.4686 (pv3.ini:66) and the GHORIZON ablation: GHORIZON ablation (13 horizon/streams keys incl. this one at tuned values) vs pv3: -6.3 +/- 21.9 (t -0.29, 6/16 wins), n=16, ECON venue; "collapses" to null once the +201 outlier seed is dropped -- block-level (STATUS.md:16013-16045). findings/multiplier-column.md:253 census: CAPACITY credits 1.8x baseline builds.

### horizon_growth_weight
- **Declared:** deeptuning.h:296 (`double horizon_growth_weight;`); params.ini: default=0.5 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared W03 comment: "the relative share for growth accelerators" (Granary-type Growth_Food buildings).
- **Unit:** multiplier (dimensionless) on a horizon-discounted sum: `extra * per_citizen` where extra = steps * pct/100 (pct = EFT_GROWTH_FOOD % the building adds, steps = MAX(1, MORT/ttg)). `per_citizen` = deep_unworked_tile_value(pcity, adv, 3, NULL) (deephorizon.c:389) = mean over the best 3 currently-unworked workable tiles of `food*adv->food_priority + trade*adv->science_priority + shield*adv->shield_priority` -- a PER-TURN weighted-yield (tile-sum) of ONE extra citizen. building_want[] is stock's per-building desirability array in "output x priority" units (server/advisors/infracache.h:28-32: "output (shields/gold/luxuries) multiplied by a priority"); deep_adjust_building_wants() (deephorizon.c:2037) ADDS each credit to pcity->server.adv->building_want[id] (deephorizon.c:2475-2476) after stock has filled it (stock base_want()/dai_city_want() output); the building auction then compares building_want against unit/military wants. The comparison target therefore defines the unit: want/score units of the building auction.
- **Formula at the primary call site:** deephorizon.c:772 deep_growth_credit(): `credit = amortize(extra * per_citizen * tune->horizon_growth_weight, ttg);` (extra = steps * pct / 100.0).
- **All call sites:**
  - ai/deep/deephorizon.c:772 -- deep_growth_credit -- Granary-type credit -- yes (default 0.5; peace-time, uncapped cities with ttg>0 only)
  - ai/deep/deephorizon.c:779 -- deep_growth_credit -- same weight in the deep-decay shape b -- yes
  - live at compiled defaults summary: yes
- **Log line:** deep-decay fam=growth; deep-decay-sum grow=; deep-credit add=
- **Interacts with:** horizon_capacity_weight, horizon_decay_*, upkeep_gold_weight, stance_growth_gain, endgame_horizon
- **Tier/screen:** TIER A (block AUCTION). knob-screen corpus retake (screen-corpus-2026-08-30.md:89): flips 78.85/game, 3.4% flip rate, rule building_anchor; was tier B ("fires" 2323/game) before the retake. Also in retired block RESCREEN. Blocks containing it (tuning-blocks.ini): AUCTION, RESCREEN, G, ALL, ECON, GHORIZON.
- **Prior evidence:** STATUS.md:1844 (Granary inversion fix): 5 paired mixed lategame-80 games, only horizon_growth_weight 0.0 -> 0.5: head-to-head score 1.0367 -> 1.0399 (+0.3pp), Granary builds 0 -> 12 per 100 cities -- "mechanism landed, score did not move" (wash, n=5). GHORIZON ablation: GHORIZON ablation (13 horizon/streams keys incl. this one at tuned values) vs pv3: -6.3 +/- 21.9 (t -0.29, 6/16 wins), n=16, ECON venue; "collapses" to null once the +201 outlier seed is dropped -- block-level (STATUS.md:16013-16045). pv3 tuned value 0.293 (pv3.ini:67).

### horizon_bundle_weight
- **Declared:** deeptuning.h:297 (`double horizon_bundle_weight;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same shared W03 comment: "the per-viable-tile weight on co-investment bundle effects" (e.g. Supermarket+Farmland).
- **Unit:** multiplier (dimensionless) on `peffect->value * viable` where peffect->value is the ruleset effect value (percent/amount of the bundle effect, ruleset units) and viable is a COUNT of catchment tiles where a worker could place the partner extra. Product is a LUMP (no amortize, no time dimension). building_want[] is stock's per-building desirability array in "output x priority" units (server/advisors/infracache.h:28-32: "output (shields/gold/luxuries) multiplied by a priority"); deep_adjust_building_wants() (deephorizon.c:2037) ADDS each credit to pcity->server.adv->building_want[id] (deephorizon.c:2475-2476) after stock has filled it (stock base_want()/dai_city_want() output); the building auction then compares building_want against unit/military wants. The comparison target therefore defines the unit: want/score units of the building auction. Note: not in per_citizen tile-sum currency -- ruleset effect value x tile count.
- **Formula at the primary call site:** deephorizon.c:1014 deep_bundle_credit(): `credit += peffect->value * viable * tune->horizon_bundle_weight;` (summed over every effect whose reqs contain this building at city range and an extra at tile range).
- **All call sites:**
  - ai/deep/deephorizon.c:1014 -- deep_bundle_credit -- co-investment bundle credit (building + worker-built extra) -- yes (default 1.0; peace-time `!threatened` only)
  - live at compiled defaults summary: yes
- **Log line:** deep-decay fam=bundle (deephorizon.c:1020); deep-decay-sum bund=; deep-credit add=
- **Interacts with:** horizon_* siblings (summed in the same add), upkeep_gold_weight, stance_growth_gain; NOT affected by horizon_decay_* (declared a lump)
- **Tier/screen:** TIER A (block AUCTION). corpus retake: flips 83.88/game, 3.6% (screen-corpus-2026-08-30.md:88); the pre-retake smoke read it "0.0 (called inert)" -- wrong sign of the answer (screen-corpus:123). findings say it flips ~84 decisions/game. Blocks containing it (tuning-blocks.ini): AUCTION, RESCREEN, G, ALL, ECON, GHORIZON.
- **Prior evidence:** none single-knob. pv3 tuned value 0.250 (pv3.ini:68); GHORIZON ablation: GHORIZON ablation (13 horizon/streams keys incl. this one at tuned values) vs pv3: -6.3 +/- 21.9 (t -0.29, 6/16 wins), n=16, ECON venue; "collapses" to null once the +201 outlier seed is dropped -- block-level (STATUS.md:16013-16045).

### horizon_health_weight
- **Declared:** deeptuning.h:308 (`double horizon_health_weight;`); params.ini: default=1.0 min=0.0 max=10.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Plague insurance, in the same tile-sum currency as the credits around it: scale on (expected citizens the building's Health_Pct saves over MORT turns) x (what a citizen earns from the best unworked tile). Active once the city is within one pop of the ruleset's illness threshold..."
- **Unit:** multiplier (dimensionless) on `saved * per_citizen`, saved = ill_base/1000 * min(health,100)/100 * deep_horizon_turns() (expected CITIZENS saved over the horizon; ill_base in tenths of a percent/turn) and per_citizen as defined for the capacity credit (weighted per-turn yield of one citizen). Not amortized (risk runs now). building_want[] is stock's per-building desirability array in "output x priority" units (server/advisors/infracache.h:28-32: "output (shields/gold/luxuries) multiplied by a priority"); deep_adjust_building_wants() (deephorizon.c:2037) ADDS each credit to pcity->server.adv->building_want[id] (deephorizon.c:2475-2476) after stock has filled it (stock base_want()/dai_city_want() output); the building auction then compares building_want against unit/military wants. The comparison target therefore defines the unit: want/score units of the building auction.
- **Formula at the primary call site:** deephorizon.c:2330-2332 deep_adjust_building_wants(): `hcred = saved * deep_unworked_tile_value(pcity, adv, 3, NULL) * deep_tuning_get()->horizon_health_weight;` then `hcred = deep_decay_reprice(hcred, deep_horizon_turns(), 0, 1, -1, &hdur); add += hcred;`
- **All call sites:**
  - ai/deep/deephorizon.c:2332 -- deep_adjust_building_wants -- plague-insurance credit for EFT_HEALTH_PCT buildings (Aqueduct etc.) -- yes at defaults (1.0) but only when !threatened && game.info.illness_on && size+1 >= illness_min_size && ill_base>0
  - (comment) ai/deep/deephorizon.c:2088,2300
  - live at compiled defaults summary: yes (conditional: illness_on, size within 1 of illness_min_size, not threatened)
- **Log line:** deep-credit health= field (tr_health, deephorizon.c:2523-2531); deep-decay fam=health
- **Interacts with:** horizon_decay_*, upkeep_gold_weight, other horizon_* credits in the same add
- **Tier/screen:** unranked (tier C); in G/GHORIZON/ECON/ALL. knobcensus 2026-08: NO-INSTRUMENT (since fixed: deep-credit gained health= at a9793209ac, STATUS.md:7079). Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GHORIZON.
- **Prior evidence:** STATUS.md:6812-6830, 7060-7090: dimension fix (raw percent points -> citizens) -- identity/liveness census only (health>0 on 1146/4491 credit lines isolated-150), no score arm; "flagged for the tuning pile". pv3 tuned value 3.911 (pv3.ini:69); GHORIZON: GHORIZON ablation (13 horizon/streams keys incl. this one at tuned values) vs pv3: -6.3 +/- 21.9 (t -0.29, 6/16 wins), n=16, ECON venue; "collapses" to null once the +201 outlier seed is dropped -- block-level (STATUS.md:16013-16045).

### horizon_content_weight
- **Declared:** deeptuning.h:319 (`double horizon_content_weight;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Credit per citizen a Make_Content/Force_Content building frees from pacification duty, in the same tile-sum currency as the capacity credit. Unhappiness is the SOFT size cap... a Temple's base_want() is structurally ZERO..."
- **Unit:** multiplier (dimensionless) on `freed * per_citizen` where freed = MIN(amount of Make_Content+Force_Content the building adds, current pacification load + citizens arriving within the horizon) -- a COUNT of citizens -- times per-turn weighted yield of one citizen; amortized by ttg (0 if load>0 already). building_want[] is stock's per-building desirability array in "output x priority" units (server/advisors/infracache.h:28-32: "output (shields/gold/luxuries) multiplied by a priority"); deep_adjust_building_wants() (deephorizon.c:2037) ADDS each credit to pcity->server.adv->building_want[id] (deephorizon.c:2475-2476) after stock has filled it (stock base_want()/dai_city_want() output); the building auction then compares building_want against unit/military wants. The comparison target therefore defines the unit: want/score units of the building auction.
- **Formula at the primary call site:** deephorizon.c:704 deep_content_credit(): `credit = amortize(freed * per_citizen * tune->horizon_content_weight, load > 0 ? 0 : ttg);` then deep_decay_reprice(credit, freed, ...).
- **All call sites:**
  - ai/deep/deephorizon.c:704 -- deep_content_credit -- Temple/Colosseum/Courthouse-type happiness credit -- yes (default 1.0; peace-time `!threatened`)
  - ai/deep/deephorizon.c:713 -- deep_content_credit -- same weight in deep-decay shape b -- yes
  - live at compiled defaults summary: yes
- **Log line:** deep-decay fam=content; deep-decay-sum cont=; deep-credit add=
- **Interacts with:** horizon_capacity_weight (same per_citizen), horizon_decay_*, upkeep_gold_weight, stance_growth_gain
- **Tier/screen:** TIER A (block AUCTION). corpus retake: flips 753.23/game, 32.3% flip rate -- "the largest single flip rate in the corpus" (screen-corpus-2026-08-30.md:85,145); pre-retake fires-rank 2323/game (tier B). Blocks containing it (tuning-blocks.ini): AUCTION, RESCREEN, G, ALL, ECON, GHORIZON.
- **Prior evidence:** STATUS.md:1809 / findings/happiness-ladder.md:113-132: only horizon_content_weight 0.0 -> 1.0, 6 paired games: happiness bldgs/100c 2.1 -> 39.6, specialists/city -30%, head-to-head score 1.047 -> 1.063 (+1.6pp, 3 paired seeds, inside noise -- "NOT a result"). pv3 tuned 1.661 (pv3.ini:70); GHORIZON: GHORIZON ablation (13 horizon/streams keys incl. this one at tuned values) vs pv3: -6.3 +/- 21.9 (t -0.29, 6/16 wins), n=16, ECON venue; "collapses" to null once the +201 outlier seed is dropped -- block-level (STATUS.md:16013-16045).

### horizon_multiplier_weight
- **Declared:** deeptuning.h:327 (`double horizon_multiplier_weight;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Credit for a percentage output building (Marketplace, Library, Bank, Factory), scaling the output the citizens arriving within the horizon will produce... Nothing told the multiplier about the growth being bought for it."
- **Unit:** multiplier (dimensionless) on `credit`, a horizon sum: sum over outputs of pct[o]/100 * arriving * raw[o] * priority[o] (arriving = MORT/ttg citizens, raw = mean raw yields of best-3 unworked tiles, priorities = adv food/shield/science/gold/luxury priorities, trade split by current rates). Amortized by ttg. building_want[] is stock's per-building desirability array in "output x priority" units (server/advisors/infracache.h:28-32: "output (shields/gold/luxuries) multiplied by a priority"); deep_adjust_building_wants() (deephorizon.c:2037) ADDS each credit to pcity->server.adv->building_want[id] (deephorizon.c:2475-2476) after stock has filled it (stock base_want()/dai_city_want() output); the building auction then compares building_want against unit/military wants. The comparison target therefore defines the unit: want/score units of the building auction.
- **Formula at the primary call site:** deephorizon.c:935 deep_multiplier_credit(): `adv_want total = amortize(credit * tune->horizon_multiplier_weight, ttg);` then deep_decay_reprice(total, arriving, ...).
- **All call sites:**
  - ai/deep/deephorizon.c:935 -- deep_multiplier_credit -- Marketplace/Library/Factory credit on arriving citizens -- yes (default 1.0; peace-time, uncapped)
  - ai/deep/deephorizon.c:944 -- deep_multiplier_credit -- same in deep-decay shape b -- yes
  - live at compiled defaults summary: yes
- **Log line:** deep-decay fam=multiplier; deep-decay-sum mult=; deep-credit add=
- **Interacts with:** other horizon_* credits, horizon_decay_*, upkeep_gold_weight, stance_growth_gain, tax/lux/sci rates
- **Tier/screen:** TIER A (block AUCTION). corpus retake: flips 144.62/game, 6.2% (screen-corpus-2026-08-30.md:86). Note the deep-tuning log line omits it (findings/mechanism-map.md:102). Blocks containing it (tuning-blocks.ini): AUCTION, RESCREEN, G, ALL, ECON, GHORIZON.
- **Prior evidence:** none single-knob (new in findings/multiplier-column.md; census only). pv3 tuned value 0.0 (pv3.ini:71) i.e. credit switched off in the pv3 package; GHORIZON: GHORIZON ablation (13 horizon/streams keys incl. this one at tuned values) vs pv3: -6.3 +/- 21.9 (t -0.29, 6/16 wins), n=16, ECON venue; "collapses" to null once the +201 outlier seed is dropped -- block-level (STATUS.md:16013-16045).

### cm_war_survival
- **Declared:** deeptuning.h:381 (`double cm_war_survival;`); params.ini: default=0.9 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Survival probability at or above which a city does NOT switch to war weights in the city manager. Deliberately separate from survival_target... 0 falls the gate back to stock's urgency/grave_danger presence counts."
- **Unit:** probability 0-1. Compared against `surv` from deep_city_survival_verdict() (survival probability of the garrison over survival_horizon turns).
- **Formula at the primary call site:** deepcm.c:682 deep_set_city_manager_params(): `threatened = wave > 0 && surv < tune->cm_war_survival;` (fallback if knob<=0: `threatened = city_data->urgency > 0 || city_data->grave_danger > 0;`, deepcm.c:719).
- **All call sites:**
  - ai/deep/deepcm.c:672 -- deep_set_city_manager_params -- branch select (>0 uses survival model) -- yes (default 0.9)
  - ai/deep/deepcm.c:682 -- deep_set_city_manager_params -- threatened = wave>0 && surv < knob; picks WAR posture -- yes
  - ai/deep/deepcm.c:700 -- deep_set_city_manager_params -- bar = knob - cm_war_growth_charge * cost -- NO: only when cm_war_growth_charge>0
  - (comment) ai/deep/deepcm.c:662; ai/deep/deepcombat.c:382
  - live at compiled defaults summary: yes
- **Log line:** deep-cmpost carries surv/bar (only when cm_war_growth_charge>0); otherwise none (knob-coverage 4c: deep-surv proves the estimator, not this branch)
- **Interacts with:** cm_war_growth_charge, survival_horizon, survival_target (deliberately separate), cm_war_food/shield/trade
- **Tier/screen:** unranked (tier C); blocks G, GCM, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** CM war gate, STATUS.md:1884-1925 (2026-08-01 night): old presence-count gate vs cm_war_survival gate, mixed lategame-80, n=10 paired: ratio 1.0026 -> 1.0343 (+0.0318), deep score 10554 -> 10670 (+116), 9/10 seeds improved (p~0.011) -- positive; earlier 5-seed run at 0.5 lost 85 points deep score, 2/5 seeds (negative at the wrong threshold). Compiled default 0.9 set by human. pv3 moves it to 0.9133 (not isolated).

### cm_war_growth_charge
- **Declared:** deeptuning.h:392 (`double cm_war_growth_charge;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "What the war footing is CHARGED for the growth it forgoes. The city manager is run under both candidate postures... the fraction of the food surplus the footing gives up is subtracted from cm_war_survival before the gate compares... 0 = off."
- **Unit:** multiplier (dimensionless) on `m->cost` in [0,1] (fraction of food surplus lost, = (peace_food - war_food)/peace_food); product is subtracted from the probability cm_war_survival, so result has unit probability 0-1.
- **Formula at the primary call site:** deepcm.c:700-701 deep_set_city_manager_params(): `double bar = tune->cm_war_survival - tune->cm_war_growth_charge * m->cost;` then `if (m->valid && surv >= bar) threatened = FALSE;`
- **All call sites:**
  - ai/deep/deepcm.c:684 -- deep_set_city_manager_params -- gate (threatened && knob>0) -- NO: dead (default 0.0)
  - ai/deep/deepcm.c:701 -- deep_set_city_manager_params -- bar computation -- NO
  - ai/deep/deepcm.c:821 -- deep_set_city_manager_params -- deep-cmsolve cost census gate -- NO
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-cmpost (deepcm.c:707), deep-cmsolve (deepcm.c:828)
- **Interacts with:** cm_war_survival, cm_war_food/shield/trade (via the counterfactual solve)
- **Tier/screen:** unranked (tier C); blocks G, GCM, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCM.
- **Prior evidence:** pv3 package ablation, GCM block (17 cm_* keys at tuned values, incl. this knob) vs pv3: -24.2 +/- 13.4 (t -1.80, 4/16 wins), ECON venue, n=16 -- block-level, not single-knob (STATUS.md:16013-16045, 'AUTOPSY closed' 2026-08-22); tuned to 0.285 (off a compiled 0). findings/cm-completion-2026-08-07.md section 4: counterfactual census at 1.0 on lategame-80/garden2b-150, 20 games each: flips revoked by the charge 5.9%/6.8% of valid -- mechanism census, no score arm.

### horizon_tech_weight
- **Declared:** deeptuning.h:396 (`double horizon_tech_weight;`); params.ini: default=1.0 min=0.0 max=5.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Share of a tech-gated cap-lifter's capacity credit routed into the gating advance's tech want (the credit cannot buy the building, but it can buy the research)."
- **Unit:** multiplier (dimensionless) on cap_credit (the capacity credit above); the product is added to `def_ai_player_data(pplayer, ait)->tech_want[tech]` -- STOCK tech_want units (accumulated mass later divided by distance in dai_select_tech()). Comparison target is stock research-goal selection, not the building auction.
- **Formula at the primary call site:** deephorizon.c:2211-2213 deep_adjust_building_wants(): `tech_want[advance_index(preq->source.value.advance)] += cap_credit * deep_tuning_get()->horizon_tech_weight;`
- **All call sites:**
  - ai/deep/deephorizon.c:2213 -- deep_adjust_building_wants -- route capacity credit into the tech want of the unmet advance for a tech-gated EFT_SIZE_ADJ/SIZE_UNLIMIT building -- yes (default 1.0) but only when cap_credit>0 and the building is can_build_later
  - live at compiled defaults summary: yes (conditional: tech-gated cap-lifter in a capped city)
- **Log line:** deep-techwall (LOG_DEBUG only, deephorizon.c:2215 -- absent from benchmark logs; knob-coverage.md 4e)
- **Interacts with:** horizon_capacity_weight (factor), techgoal_deep / techgoal_distance_power (consume tech_want), horizon_decay_*
- **Tier/screen:** unranked (tier C); blocks G, GHORIZON, ECON, ALL. knobcensus: NO-INSTRUMENT (instrument exists but at LOG_DEBUG). Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GHORIZON.
- **Prior evidence:** none single-knob. pv3 tuned 0.628 (pv3.ini:84); GHORIZON: GHORIZON ablation (13 horizon/streams keys incl. this one at tuned values) vs pv3: -6.3 +/- 21.9 (t -0.29, 6/16 wins), n=16, ECON venue; "collapses" to null once the +201 outlier seed is dropped -- block-level (STATUS.md:16013-16045). STATUS.md:822 lists it among overnight-pile knobs; findings/amortization-audit-2026-08-07.md:14 notes the routing.

### worker_flee_radius
- **Declared:** deeptuning.h:976 (`int worker_flee_radius;`); params.ini: default=3 min=0 max=8 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "A civilian outside a city flees to the nearest own city when an at-war military unit is within this many tiles. 0 disables."
- **Unit:** tiles (real_map_distance, int).
- **Formula at the primary call site:** deepdefence.c:3243-3244 deep_flee_workers(): `adv_unit_att_rating(e) > 0 && real_map_distance(unit_tile(e), unit_tile(w)) <= tune->worker_flee_radius` -> danger=TRUE -> worker drops activity and runs to nearest own city.
- **All call sites:**
  - ai/deep/deepdefence.c:3211 -- deep_flee_workers -- early return if <=0 -- yes (default 3)
  - ai/deep/deepdefence.c:3244 -- deep_flee_workers -- danger test radius for fleeing civilians -- yes
  - (comment) ai/deep/deepdefence.c:3203
  - live at compiled defaults summary: yes
- **Log line:** deep-flee (deepdefence.c:~3295)
- **Interacts with:** worker_guard_radius (wider ring, bodyguard request), survival_target (indirect via guard), aiguard (stock)
- **Tier/screen:** TIER B (block BCIVIL, tuning-blocks.ini:140). knob-screen Part 1 tier-B table: fires 427/game (instrument runs; log has one side of the comparison -- no flip rate). knobcensus LIVE direct (13/13 corpora). Blocks containing it (tuning-blocks.ini): BCIVIL, G, ALL, ECON, GCIVIL.
- **Prior evidence:** none single-knob (findings/omniscience-census-2026-08-11.md:164 notes the enemy-unit scan is ungated by visibility). pv3 keeps 3.

### worker_guard_radius
- **Declared:** deeptuning.h:980 (`int worker_guard_radius;`); params.ini: default=6 min=0 max=15 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "A working civilian on unowned ground, or with an at-war unit within this many tiles, requests a bodyguard (escorts arrive before the flee radius is breached). 0 disables."
- **Unit:** tiles (real_map_distance, int); also the search radius for cover-sortie lenders.
- **Formula at the primary call site:** deepdefence.c:3272 deep_flee_workers(): `real_map_distance(unit_tile(e), unit_tile(w)) <= tune->worker_guard_radius` -> exposed -> aiguard_request_guard(); deepsettler.c:3918 deep_worker_task_value(): same radius decides "exposed" tile for the danger discount.
- **All call sites:**
  - ai/deep/deepdefence.c:3259 -- deep_flee_workers -- gate (>0) for bodyguard requests -- yes (default 6)
  - ai/deep/deepdefence.c:3272 -- deep_flee_workers -- exposure radius for the request -- yes
  - ai/deep/deepdefence.c:3063,3069 -- deep_cover_sorties -- gate and initial bestd = radius+1 (how far a charge may be from the lending city) -- yes
  - ai/deep/deepsettler.c:3918 -- deep_worker_task_value -- "exposed" test for worker_danger_discount -- yes (needs worker_danger_discount>0)
  - (comment) ai/deep/deepsettler.c:3852
  - live at compiled defaults summary: yes
- **Log line:** deep-guard (bodyguard request, deepdefence.c:~3285)
- **Interacts with:** worker_flee_radius, worker_danger_discount, survival_target (cover sorties require >0)
- **Tier/screen:** TIER B (block BCIVIL). knobcensus LIVE direct (13/13). Not in the knob-screen fires top-24 table (flee_radius is; guard_radius is not listed). Blocks containing it (tuning-blocks.ini): BCIVIL, G, ALL, ECON, GCIVIL.
- **Prior evidence:** none single-knob. pv3 tuned value 3 vs default 6 (pv3.ini:130) -- not isolated.

### worker_danger_discount
- **Declared:** deeptuning.h:984 (`double worker_danger_discount;`); params.ini: default=1.0 min=0.0 max=5.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Discount on auto-worker candidate values for exposed tiles (unowned ground or at-war unit within worker_guard_radius): value /= (1 + discount). 0 disables."
- **Unit:** multiplier (dimensionless): divisor (1 + d) on the advisor's `value` (adv_want tile-improvement value, "output x priority" per tile). Base and trunk wants are added AFTER the discount and are exempt.
- **Formula at the primary call site:** deepsettler.c:3933-3935 deep_worker_task_value(): `return deep_worker_travel_price(pplayer, value / (1.0 + tune->worker_danger_discount) + addon, old_tile_value, arrival);`
- **All call sites:**
  - ai/deep/deepsettler.c:3904 -- deep_worker_task_value -- early return if <=0 -- yes (default 1.0)
  - ai/deep/deepsettler.c:3934 -- deep_worker_task_value -- discount on exposed tiles in the advisor's task-value hook (aw_set_task_value_hook, deepai.c:1674) -- yes
  - (comment) ai/deep/deepsettler.c:3853
  - live at compiled defaults summary: yes
- **Log line:** none (knob-coverage.md 4f: divides a task want silently)
- **Interacts with:** worker_guard_radius (exposure), base_fort_gain (exempt addon), trunk_road_gain (exempt addon), worker_travel_discount (applied last)
- **Tier/screen:** unranked (tier C); in G, GCIVIL, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCIVIL.
- **Prior evidence:** STATUS.md:15330-15415 (S-FORT live-play entry): derby-7 ini sets 3.15 so exposed non-fortress work is ~4x cheaper than fortress want; the cell A/B did not isolate it ("Cell B cannot separate those two"). findings/tuning-first-principles-2026-08-06.md:55 proposes 2.0. pv3 tuned 3.1535 (pv3.ini:131). No single-knob arm.

### worker_cross_continent_discount
- **Declared:** deeptuning.h:995 (`double worker_cross_continent_discount;`); params.ini: default=1.0 min=0.0 max=8.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "When the idle fallback finds nothing walkable it may instead take a candidate across the water, ranked by completion time like every other candidate -- inflated by this factor so the trip stays BACKUP behaviour: turns *= (1 + discount). 0 makes a boat trip cost exactly its real turns."
- **Unit:** multiplier (dimensionless): the candidate's turn count is multiplied by (1 + d) (lower turns win), so unit = fractional inflation of trip turns.
- **Formula at the primary call site:** deepsettler.c:1641-1643 deep_idle_cross_water(): `deep_idle_scan(nmap, pplayer, punit, pfm, parameter.omniscience, home, TRUE, ferry_wait, 1.0 + deep_tuning_get()->worker_cross_continent_discount, best);`
- **All call sites:**
  - ai/deep/deepsettler.c:1643 -- deep_idle_cross_water -- inflation factor passed to deep_idle_scan for overseas idle-worker candidates -- yes (default 1.0) but only reached when no walkable candidate AND a free ferry exists
  - (comment) ai/deep/deepsettler.c:696,1581
  - live at compiled defaults summary: yes (conditional: only the overseas fallback path)
- **Log line:** deep-cross (deepsettler.c:1604, with why= reasons) and deep-idlework (cross> field)
- **Interacts with:** worker_travel_discount (separate walk-time term), idle_policy_gate (the fallback runs in the empty-backlog path)
- **Tier/screen:** unranked (tier C); in G, GCIVIL, ECON, ALL. knobcensus LIVE direct (4/6 corpora, e.g. isolated-150 7421 lines). Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCIVIL.
- **Prior evidence:** STATUS.md:2866-2965 (2026-08-03): the FEATURE (workers may take a boat) A/B lategame-80, 10 paired seeds: score -0.10 (t -0.10) -- neutral; only 70 of 6834 attempts fired, 230 decided by the discount ("tuning it now would be measuring noise"). The knob value itself was never varied. pv3 tuned 2.578.

### worker_travel_discount
- **Declared:** deeptuning.h:1006 (`double worker_travel_discount;`); params.ini: default=0.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Travel discount on the PRIMARY worker task auction... the improvement (value minus the tile's value without it) is discounted by amortize(gain, discount * arrival)... 0 disables and restores stock ranking exactly."
- **Unit:** multiplier (dimensionless): turns of amortize() per turn of walking (`(int)(discount * arrival)` is the amortize delay; amortize base 1-1/MORT per turn). Acts on a per-tile improvement gain in advisor tile-value (adv_want) units.
- **Formula at the primary call site:** deepsettler.c:2030 deep_worker_travel_amortized(): `return amortize(gain, (int) (tune->worker_travel_discount * arrival));` (gain = candidate want - old_tile_value, deepsettler.c:3788-3794).
- **All call sites:**
  - ai/deep/deepsettler.c:2026 -- deep_worker_travel_amortized -- early return -- NO: dead (default 0.0)
  - ai/deep/deepsettler.c:2030 -- deep_worker_travel_amortized -- also used by the trunk beyond-ceiling source (:2347) -- NO
  - ai/deep/deepsettler.c:3781 -- deep_worker_travel_price -- primary-auction hook gate -- NO
  - (comment) ai/deep/deepsettler.c:2014,3748; ai/deep/deepidle.c:289
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-worklocal (cand=, disc=, meanturns=, shave= fields; deepsettler.c:3831)
- **Interacts with:** trunk_source_gain (reuses the same discount), worker_danger_discount (applied before), trunk_road_gain
- **Tier/screen:** unranked (tier C); in G, GCIVIL, ECON, ALL. knobcensus LIVE direct when on (843 lines, STATUS.md:14325). Off at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCIVIL.
- **Prior evidence:** STATUS.md:14300-14335, 14372-14395 (W12 slice 1): liveness lategame-80 2 games at 1.0: 28.7% of candidates discounted, walker unit-turns -10.4%, mean remaining walk -10.7%; no score arm alone ("the cheaper bet... its arm is worker_travel_discount alone" -- not run). W12 family venue pilot (STATUS.md:16150-16250), corridor-150, 8 mirrored seeds, family ON (worker_travel_discount=1.0, trunk_road_gain=5.0, trunk_source_gain=1.0, trunk_ally_gate=1, trunk_trade_gain=1.0) vs all five at compiled zeros: score -7.25 +/- 5.64 (t -1.29, 2/8), techs -1.29 (t -3.18) -- joint arm, underpowered (needs n>=29 for +6)

### idle_policy_gate
- **Declared:** deeptuning.h:1015 (`int idle_policy_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "The backlog-empty worker policy (deepidle.c). 0 = today's carpet, byte-identically: a worker whose only remaining candidate is a fortress builds it... 1 = the worker is held in reserve or walked home and recycled instead. Policy, not a magnitude, so 0/1."
- **Unit:** boolean gate (int 0/1; `> 0` = on).
- **Formula at the primary call site:** deepidle.c:403 deep_idle_policy(): `if (deep_tuning_get()->idle_policy_gate <= 0) { return DEEP_IDLE_ACT_NONE; }`; deepidle.c:380 deep_idle_policy_on(): `return deep_tuning_for(pplayer)->idle_policy_gate > 0;`
- **All call sites:**
  - ai/deep/deepidle.c:380 -- deep_idle_policy_on -- per-seat predicate, consulted at deepsettler.c:2816 -- NO: dead (default 0)
  - ai/deep/deepidle.c:403 -- deep_idle_policy -- verdict gate (counts `empty` in BOTH arms) -- NO
  - ai/deep/deepidle.c:471 -- deep_idle_census -- gate= field on the census line (log only)
  - (comment) ai/deep/deepsettler.c:113; deepidle.h:59
  - live at compiled defaults summary: no (default 0 = off)
- **Log line:** deep-idlepol (gate=, reserve=, target=, empty=, held=, walks=, disbands=, shields=)
- **Interacts with:** idle_reserve_per_city, base_fort_gain (the fortress carpet it replaces), worker_travel_discount
- **Tier/screen:** unranked/tier C; in G, GCIVIL, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:16784-16905 (S-FORT idle policy): contested T150, 16 seeds, mixed_dc (64 deep player-games/arm), gate 0 vs 1 with reserve=0: score +22.19 +/- 13.06 (t +1.70, 11/16), base share of worker-turns 9.97% -> 0.43%, 3390 shields recovered; isolation arm (reserve=1.0) +3.81 (t +0.31) -- "the movement travels with the shields".

### idle_reserve_per_city
- **Declared:** deeptuning.h:1026 (`double idle_reserve_per_city;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "How many idle workers per CITY the policy holds in reserve against demand that has not arrived yet; the empire target is this times the city count, rounded. Everything above it is recycled... DERIVED, not chosen: ... deep-pollute measures its mean concurrency... Inert while idle_policy_gate is 0."
- **Unit:** count (workers per city, float): reserve target = (int)(knob * number of cities + 0.5).
- **Formula at the primary call site:** deepidle.c:302 deep_idle_turn_start(): `deep_idle_target[pn] = (int) (deep_tuning_get()->idle_reserve_per_city * city_list_size(pplayer->cities) + 0.5);`
- **All call sites:**
  - ai/deep/deepidle.c:302 -- deep_idle_turn_start -- reserve target (computed each turn but consumed only after the gate check at :403) -- NO: dead behind idle_policy_gate=0 (and default 0.0 means no reserve even with the gate on)
  - ai/deep/deepidle.c:472 -- deep_idle_census -- reserve= on the census line (log only)
  - (comment) ai/deep/deepidle.c:285
  - live at compiled defaults summary: no (default 0.0; also gated by idle_policy_gate)
- **Log line:** deep-idlepol (reserve=, target=, held=)
- **Interacts with:** idle_policy_gate, deep-pollute measurement
- **Tier/screen:** unranked/tier C; in G, GCIVIL, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:16853-16905: isolation arm reserve=1.0 vs OFF, contested, 16 seeds: +3.81 (t +0.31, 6/16) -- wash; 7173 worker-turns held idle (1.0 was a dosing probe, "never shipped anywhere").

### trunk_road_gain
- **Declared:** deeptuning.h:1034 (`double trunk_road_gain;`); params.ini: default=0.0 min=0.0 max=20.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Trunk roads (W12). A road candidate on a tile the empire's minimum spanning tree runs through carries the LINK's worth on top of its own: the movement points the finished link saves on one traversal, discounted by amortize() over the corridor tiles still missing. This gain converts those movement points into the advisor's tile currency. 0 disables the whole term, plan included."
- **Unit:** want/score units per whole movement point saved (adv_want, advisor tile currency): saving is in move FRAGMENTS, divided by SINGLE_MOVE to whole points, then x gain. Probe value used was 5.0.
- **Formula at the primary call site:** deeptrunk.c:1609-1612 deep_trunk_price(): `return (tune->trunk_road_gain * ((double) saving / SINGLE_MOVE) + tune->trunk_trade_gain * link->dividend / MORT * TRADE_WEIGHTING) * amortize(1.0, link->gap);` where saving = (link->upgrade ? link->cur : link->terr) - link->len * proad->move_cost.
- **All call sites:**
  - ai/deep/deeptrunk.c:1609 -- deep_trunk_price -- corridor-link worth -- NO: dead (default 0.0)
  - ai/deep/deeptrunk.c:1622 -- deep_trunk_want -- advisor-hook gate (return 0 if <=0) -- NO
  - ai/deep/deeptrunk.c:1677 -- deep_trunk_source_want -- gate -- NO
  - ai/deep/deeptrunk.c:1718 -- deep_trunk_source_dividend -- gate (also needs trunk_trade_gain>0) -- NO
  - ai/deep/deepsettler.c:2225 -- deep_trunk_source_task -- gate (also needs trunk_source_gain>0) -- NO
  - ai/deep/deepsettler.c:2733 -- deep_auto_settler_run -- keepable-gate precondition -- NO
  - (comment) ai/deep/deeptrunk.c:650,1393,1578
  - live at compiled defaults summary: no (default 0.0 = off; plan not built)
- **Log line:** deep-trunk (links=, gap=, hits=, maxwant=), deep-trunkpick; OFF arm emits no deep-trunk line at all
- **Interacts with:** trunk_source_gain, trunk_trade_gain, trunk_upgrade_gate, worker_travel_discount, trunk_keepable_gate, trunk_stack_gate, trunk_ally_gate, trunk_risk_weight
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, GCIVIL, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCIVIL.
- **Prior evidence:** W12 family venue pilot (STATUS.md:16150-16250), corridor-150, 8 mirrored seeds, family ON (worker_travel_discount=1.0, trunk_road_gain=5.0, trunk_source_gain=1.0, trunk_ally_gate=1, trunk_trade_gain=1.0) vs all five at compiled zeros: score -7.25 +/- 5.64 (t -1.29, 2/8), techs -1.29 (t -3.18) -- joint arm, underpowered (needs n>=29 for +6). STATUS.md:14372: "trunk_road_gain = 5.0 was chosen to put the want on the same order as the tile wants... arbitrary" (probe, not result). Liveness: STATUS.md:14322, 724 knobcensus lines.

### trunk_source_gain
- **Declared:** deeptuning.h:1048 (`double trunk_source_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "W12 slice 2: the worker task SOURCE beyond the advisor's radius ceiling... deep_trunk_source_task() offers those tiles itself, priced by the same deep_trunk_price() the hook uses times this gain, walk-discounted by slice 1's worker_travel_discount... 1.0 is the unit case... 0 keeps today's ceiling byte-identically."
- **Unit:** multiplier (dimensionless) on the want from deep_trunk_price() (advisor tile-value currency) for beyond-radius corridor tiles.
- **Formula at the primary call site:** deepsettler.c:2345-2346 deep_trunk_source_task(): `want *= tune->trunk_source_gain; nodiv *= tune->trunk_source_gain;`
- **All call sites:**
  - ai/deep/deepsettler.c:2225 -- deep_trunk_source_task -- gate -- NO: dead (default 0.0)
  - ai/deep/deepsettler.c:2345,2346 -- deep_trunk_source_task -- scales the corridor bid -- NO
  - ai/deep/deepsettler.c:2732 -- deep_auto_settler_run -- keepable-gate precondition -- NO
  - (comment) ai/deep/deepsettler.c:2121
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-trunksrc (beyond=, offered=, picked=, hist=), deep-trunkpick
- **Interacts with:** trunk_road_gain (prerequisite), worker_travel_discount, trunk_keep_margin, trunk_keepable_gate, trunk_ally_gate, trunk_stack_gate
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, GCIVIL, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCIVIL.
- **Prior evidence:** STATUS.md:15155-15200 (W12 slice 2 A/B, lategame-80, 16 paired seeds, unit case 1.0): score -1.32 +/- 1.56 (t -0.85, 6/16) -- null; cities +0.43 (t 1.71); walker unit-turns +7.1%; offered/picked 0 -> 1587/776. W12 family venue pilot (STATUS.md:16150-16250), corridor-150, 8 mirrored seeds, family ON (worker_travel_discount=1.0, trunk_road_gain=5.0, trunk_source_gain=1.0, trunk_ally_gate=1, trunk_trade_gain=1.0) vs all five at compiled zeros: score -7.25 +/- 5.64 (t -1.29, 2/8), techs -1.29 (t -3.18) -- joint arm, underpowered (needs n>=29 for +6)

### trunk_ally_gate
- **Declared:** deeptuning.h:1069 (`int trunk_ally_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "W12 slice 3: allied cities as trunk endpoints... With this gate on the node set gains ONE city per allied player... those nodes are leaves... Geometry, not a magnitude, so 0/1. 0 = the slice-1 graph exactly."
- **Unit:** boolean gate (int 0/1).
- **Formula at the primary call site:** deeptrunk.c:1079 deep_trunk_build(): `if (tune->trunk_ally_gate > 0) { nnodes += deep_trunk_ally_nodes(pplayer, ctile, nown); }`
- **All call sites:**
  - ai/deep/deeptrunk.c:1079 -- deep_trunk_build -- adds allied-city nodes to the MST -- NO: dead (default 0; also needs trunk_road_gain>0)
  - (comment) ai/deep/deeptrunk.h:90; ai/deep/deeprequest.c:591
  - live at compiled defaults summary: no (default 0 = off)
- **Log line:** deep-trunkally (links=, gap=, offered=, picked=)
- **Interacts with:** trunk_road_gain, trunk_source_gain, trunk_soft_border_gate (ally corridors are clipped at non-allied ground)
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, ECON, ALL (not GCIVIL). Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:15659-15680 (W12 slice 3 A/B): 16 mirrored lategame-80 seeds, source-on vs +trunk_ally_gate=1: score +0.65 +/- 0.98 (t +0.66, 6/16) -- NULL; ally-picks 0 -> 10.7/game, 14/16 seeds live. W12 family venue pilot (STATUS.md:16150-16250), corridor-150, 8 mirrored seeds, family ON (worker_travel_discount=1.0, trunk_road_gain=5.0, trunk_source_gain=1.0, trunk_ally_gate=1, trunk_trade_gain=1.0) vs all five at compiled zeros: score -7.25 +/- 5.64 (t -1.29, 2/8), techs -1.29 (t -3.18) -- joint arm, underpowered (needs n>=29 for +6)

### trunk_trade_gain
- **Declared:** deeptuning.h:1096 (`double trunk_trade_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "W12 slice 4: the corridor's TRADE dividend... a road that halves a caravan's ETA multiplies the discounted value of every route it serves... converted the advisor's own way: / MORT turns the stock into a per-turn flow, * TRADE_WEIGHTING prices that flow... So 1.0 is the unit case by derivation... 0 disables the term and every pair evaluation with it."
- **Unit:** multiplier (dimensionless) on link->dividend (stock of discounted trade points from deep_trade_pair_value) / MORT (-> per-turn flow) * TRADE_WEIGHTING (18) -- converts to advisor tile-value (want) currency.
- **Formula at the primary call site:** deeptrunk.c:1609-1611 deep_trunk_price(): `+ tune->trunk_trade_gain * link->dividend / MORT * TRADE_WEIGHTING` inside `(...) * amortize(1.0, link->gap)`; and `*div_out = tune->trunk_trade_gain * link->dividend / MORT * TRADE_WEIGHTING * amortize(1.0, link->gap)` (:1595).
- **All call sites:**
  - ai/deep/deeptrunk.c:1247 -- deep_trunk_build -- gate: evaluates trade pairs for each link -- NO: dead (default 0.0)
  - ai/deep/deeptrunk.c:1595,1610 -- deep_trunk_price -- dividend term -- NO
  - ai/deep/deeptrunk.c:1718 -- deep_trunk_source_dividend -- gate -- NO
  - (comment) ai/deep/deeptrunk.c:1599
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-trunkdiv (pairs=, links=, maxdiv=, picked=, flipped=)
- **Interacts with:** trunk_road_gain (prerequisite), caravan_discount (inside the valuator), caravan_foreign_trade, trunk_source_gain
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:15840-15915 (W12 slice 4 A/B): 16 mirrored lategame-80 seeds, ally-on vs +trunk_trade_gain=1.0: score -1.23 +/- 1.77 (t -0.69, 7/16) -- NULL; div-picks +13.4/game, 9.06 picks flipped per game; techs +0.39 (t +2.48), mean_city_size +0.16 (t +2.99) not claimed. W12 family venue pilot (STATUS.md:16150-16250), corridor-150, 8 mirrored seeds, family ON (worker_travel_discount=1.0, trunk_road_gain=5.0, trunk_source_gain=1.0, trunk_ally_gate=1, trunk_trade_gain=1.0) vs all five at compiled zeros: score -7.25 +/- 5.64 (t -1.29, 2/8), techs -1.29 (t -3.18) -- joint arm, underpowered (needs n>=29 for +6)

### trunk_keepable_gate
- **Declared:** deeptuning.h:1113 (`int trunk_keepable_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "W12G: do not bid for a worker the founder logic is about to reclaim... With this gate open the site search for a cityfounder runs BEFORE the auction and its result becomes a floor the corridor bid must clear... 0 keeps today's ordering byte-identically."
- **Unit:** boolean gate (int 0/1).
- **Formula at the primary call site:** deepsettler.c:2732-2733 deep_auto_settler_run(): `if (tune->trunk_keepable_gate > 0 && tune->trunk_source_gain > 0.0 && tune->trunk_road_gain > 0.0 && unit_is_cityfounder(punit))` -> run the site search first.
- **All call sites:**
  - ai/deep/deepsettler.c:2732 -- deep_auto_settler_run -- reorders founder site search ahead of the corridor auction -- NO: dead (default 0; also needs source/road gains)
  - (comment) ai/deep/deepsettler.c:2143
  - live at compiled defaults summary: no (default 0 = off)
- **Log line:** deep-trunkkeep (veto=, held=, kept=)
- **Interacts with:** trunk_source_gain, trunk_road_gain, trunk_keep_margin
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:17117-17215 (W12G): 29 mirrored corridor-150 seeds, control=w12-div-on vs +trunk_keepable_gate=1 +trunk_keep_margin=0.25 (joint): score +3.23 +/- 2.82 (t +1.15, 18/29) -- positive-lean, unresolved; start-rate 8.72% -> 13.21% (t +5.22), norun drops 60.6 -> 6.8/game.

### trunk_keep_margin
- **Declared:** deeptuning.h:1131 (`double trunk_keep_margin;`); params.ini: default=0.0 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "W12G: how much more a rival bid must be worth before a worker already walking to a corridor tile is turned around... the tile the worker holds has its bid multiplied by 1 + this margin... 0 is the off-state and reproduces today's churn exactly... PLACEHOLDER: no measured value exists yet."
- **Unit:** multiplier (dimensionless): fractional premium m; bid x (1 + m) for the tile currently held (compared against rival bids on the same adv_want scale).
- **Formula at the primary call site:** deepsettler.c:2397-2398 deep_trunk_source_task(): `value *= 1.0 + tune->trunk_keep_margin; nodiv *= 1.0 + tune->trunk_keep_margin;`
- **All call sites:**
  - ai/deep/deepsettler.c:2219 -- deep_trunk_source_task -- held_tile ledger allocated only if >0 -- NO: dead (default 0.0)
  - ai/deep/deepsettler.c:2397,2398 -- deep_trunk_source_task -- premium on the held tile -- NO
  - ai/deep/deepsettler.c:2466 -- deep_trunk_source_task -- held-tile bookkeeping -- NO
  - (comment) ai/deep/deepsettler.c:2153,2214; deeptrunk.h:157
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-trunkkeep (held=, kept=)
- **Interacts with:** trunk_keepable_gate, trunk_source_gain, trunk_road_gain
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:17117-17215: same joint 29-seed arm as trunk_keepable_gate (margin=0.25 was a PROBE): +3.23 +/- 2.82 (t +1.15), keep-held +77, kept +15.7 per game (kept/held 18.8%).

### trunk_soft_border_gate
- **Declared:** deeptuning.h:1160 (`int trunk_soft_border_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "W12H: may a corridor cross a PEACE PARTNER's territory (DS_PEACE or DS_ARMISTICE)? 0 is today's hard clipping... the question is not 'is it legal' but 'is it safe'... Gate rather than magnitude."
- **Unit:** boolean gate (int 0/1).
- **Formula at the primary call site:** deeptrunk.c:505 deep_trunk_soft_open(): `return deep_tuning_get()->trunk_soft_border_gate > 0;` for DEEP_TRUNK_OWN_PEACE tiles (war/ceasefire/no-contact stay shut).
- **All call sites:**
  - ai/deep/deeptrunk.c:505 -- deep_trunk_soft_open -- may a corridor tile owned by a peace partner be used -- NO: dead (default 0; needs the trunk plan on)
  - (comment) ai/deep/deeptrunk.c:490,2150; deepsettler.c:271,2254; deeptrunk.h:112
  - live at compiled defaults summary: no (default 0 = off)
- **Log line:** deep-trunksoft (plan/off/pick/work/road by ownership class)
- **Interacts with:** trunk_ally_gate, trunk_road_gain, trunk_source_gain
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:18222-18260 (W12H): liveness only (lategame-80, 4 seeds, 3 arms): off peace 0 -> 76, pick peace 0 -> 49; no score arm for this key alone.

### trunk_risk_weight
- **Declared:** deeptuning.h:1189 (`double trunk_risk_weight;`); params.ini: default=0.0 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "How dearly a corridor step pays for the DANGER of the ground it crosses, in move fragments per point of deep's own standing fear... Above 0 the walk minimises an EDGE COST... 0 is the off state and restores today's walk exactly... PLACEHOLDER: nothing measures what a point of standing fear is worth in move fragments."
- **Unit:** move fragments (terrain movement_cost * SINGLE_MOVE units) per point of standing fear (deep_density_fear_at / boat_shadow_fear points). Registered range 0-2.0.
- **Formula at the primary call site:** deeptrunk.c:660-662 deep_trunk_step_cost(): `return (double) (tile_terrain(ptile)->movement_cost * SINGLE_MOVE) + tune->trunk_risk_weight * deep_trunk_tile_risk(pplayer, ptile, afloat, coastal_out);`
- **All call sites:**
  - ai/deep/deeptrunk.c:661 -- deep_trunk_step_cost -- edge cost of a corridor step -- NO: dead (default 0.0)
  - ai/deep/deeptrunk.c:695 -- deep_trunk_walk -- `risky` flag switches to cost-minimising neighbour choice -- NO
  - ai/deep/deeptrunk.c:1027 -- deep_trunk_build -- afloat (boat shadow) precondition -- NO
  - (comment) ai/deep/deeptrunk.c:647,650
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-trunkrisk (walked=, worst tile=, coastal steps=)
- **Interacts with:** density_fear_value, density_fear_radius, boat_shadow_fear (the fear it reuses), trunk_road_gain
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:18222-18300, 18438-18450: liveness only (lategame-80, 4 seeds, risk arm at 0.1): plan peace 19 -> 37, 4108 reroutes (2572 are terrain-cost, not risk); "0.1 is a PROBE, not a result"; no score arm.

### trunk_stack_gate
- **Declared:** deeptuning.h:1229 (`int trunk_stack_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "W28: may a corridor task JOIN a tile one of our own workers is already building the same road on?... Above the gate an occupied tile is not skipped but PRICED as a join... Gate rather than magnitude. 0 is today's skip exactly."
- **Unit:** boolean gate (int 0/1).
- **Formula at the primary call site:** deepsettler.c:2291 deep_trunk_source_task(): `if (tune->trunk_stack_gate <= 0) { ... continue; }` (occupied corridor tile skipped when 0).
- **All call sites:**
  - ai/deep/deepsettler.c:2291 -- deep_trunk_source_task -- skip vs price-as-join for occupied corridor tiles -- NO: dead (default 0; needs trunk source on)
  - (comment) ai/deep/deepsettler.c:2277
  - live at compiled defaults summary: no (default 0 = off)
- **Log line:** deep-trunkstack (seen=, offered=, declined idle:late:partial, picked=)
- **Interacts with:** trunk_source_gain, trunk_road_gain, trunk_keepable_gate
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:20863-20900 (W28 corridor join): 16 mirrored corridor-150 seeds, w12g-keep-on vs +trunk_stack_gate=1: PROVEN zero -- 0 joins ever picked, 16/16 scorelogs byte-identical, delta +0.00.

### trunk_upgrade_gate
- **Declared:** deeptuning.h:1270 (`int trunk_upgrade_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "W12 rail upgrade: is a corridor tile that already carries a road DONE, or merely done for now?... Above the gate the gap predicate becomes deep_trunk_tile_complete()... two-phase, per LINK... 0 is today's plan exactly."
- **Unit:** boolean gate (int 0/1).
- **Formula at the primary call site:** deeptrunk.c:391 deep_trunk_tile_complete(): `if (deep_tuning_get()->trunk_upgrade_gate <= 0) { return deep_trunk_tile_roaded(ptile); }` else `deep_trunk_tile_best(...) >= deep_trunk_tile_cost(ptile)`; deeptrunk.c:1183 link->upgrade = TRUE when the build phase is whole.
- **All call sites:**
  - ai/deep/deeptrunk.c:391 -- deep_trunk_tile_complete -- done-vs-upgradable predicate -- NO: dead (default 0; needs the trunk plan on)
  - ai/deep/deeptrunk.c:1183 -- deep_trunk_build -- enters the UPGRADE phase of a link -- NO
  - (comment) deeptrunk.c:76,243,377,2029; deeptrunk.h:46
  - live at compiled defaults summary: no (default 0 = off)
- **Log line:** deep-trunkup (up-live, up-pick counters)
- **Interacts with:** trunk_road_gain (prices the upgrade as the DIFFERENCE), trunk_source_gain, trunk_trade_gain
- **Tier/screen:** unranked/tier C; DEAD-PATH (knob-screen Part 1: "the trunk/road family (10 knobs)", tag never appeared in 40 games at compiled defaults). In G, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:21676-21800 (W12 rail upgrade): 16 mirrored lategame-80 seeds, w12g-keep-on vs +trunk_upgrade_gate=1: score +2.58 +/- 1.38 (t +1.87, 9/16) -- directional, not claimed; cities +0.40 (t +2.14), pop +1.60 (t +2.21), techs -0.21; up-live 0 -> 152.7/game. "owes a held-out confirmation".

### rotation_power_ratio
- **Declared:** deeptuning.h:1290 (`double rotation_power_ratio;`); params.ini: default=2.0 min=0.0 max=6.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Rotation of obsolete surplus defenders: melt the weakest upkeep-costing defender above the garrison floor when a defender rotation_power_ratio times stronger is buildable and the city is quiet (<=0 disables)."
- **Unit:** multiplier (dimensionless): ratio of unit-type `defense_strength` values (best buildable / present worst).
- **Formula at the primary call site:** deepdefence.c:5596-5597 deep_military_choose_build(): `best_present > 0 && best_buildable >= best_present * deep_tuning_get()->rotation_power_ratio` -> inject rotation_want; deepdefence.c:2657-2665 deep_melt_surplus_defenders(): `need = tune->rotation_power_ratio; ... best_buildable < worst_def * need` -> not obsolete.
- **All call sites:**
  - ai/deep/deepdefence.c:5571,5597 -- deep_military_choose_build -- replace-then-disband queueing gate/ratio -- yes (default 2.0; city danger==0)
  - ai/deep/deepdefence.c:2567 -- deep_melt_surplus_defenders -- early return if <=0 -- yes
  - ai/deep/deepdefence.c:2657 -- deep_melt_surplus_defenders -- obsolescence bar `need` -- yes
  - (comment) ai/deep/deepdefence.c:2554
  - live at compiled defaults summary: yes
- **Log line:** deep-rotate (melt/upgrade side, deepdefence.c:~2690; knobcensus LIVE direct 12/12)
- **Interacts with:** rotation_want, rotation_finish_relax, garrison_floor, upkeep_unit_weight
- **Tier/screen:** unranked/tier C; in G, GCIVIL, ECON, ALL. knobcensus LIVE direct. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCIVIL.
- **Prior evidence:** STATUS.md:2005 / findings/europe-T80.md:223: ratio <1.0 makes the rotation gate trivially true (campaign sampled 0.54-0.96); STATUS.md:822 overnight pile; no single-knob arm. pv3 tuned 3.639 (pv3.ini:135).

### rotation_want
- **Declared:** deeptuning.h:1291 (`int rotation_want;`); params.ini: default=35 min=0 max=150 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "rotation_want is the danger injected to queue the modern replacement first."
- **Unit:** want/score units: value injected into `city_data->danger` (unsigned, stock danger points that drive the military build want), scaled by updisc (a unit-upkeep discount factor): `danger = (unsigned)(rotation_want * updisc)`.
- **Formula at the primary call site:** deepdefence.c:5599-5600 deep_military_choose_build(): `city_data->danger = (unsigned int) (deep_tuning_get()->rotation_want * updisc);`
- **All call sites:**
  - ai/deep/deepdefence.c:5570 -- deep_military_choose_build -- gate (danger==0 && rotation_want>0) -- yes (default 35)
  - ai/deep/deepdefence.c:5599 -- deep_military_choose_build -- injected danger -- yes
  - live at compiled defaults summary: yes
- **Log line:** none (the deep-fear line at :5559 is emitted BEFORE the injection; knob-coverage 4d lists rotation_want as a want that never reaches a line)
- **Interacts with:** rotation_power_ratio, updisc (unit-upkeep discount)
- **Tier/screen:** unranked/tier C; in G, GCIVIL, ECON, ALL. knobcensus: NO-INSTRUMENT. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCIVIL.
- **Prior evidence:** none single-knob (STATUS.md:822 pile). pv3 tuned 37.

### rotation_finish_relax
- **Declared:** deeptuning.h:1294 (`double rotation_finish_relax;`); params.ini: default=0.5 min=0.25 max=1.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "Multiplier (<1 relaxes) on the obsolescence bar when the melt's recovered shields would finish the city's current build."
- **Unit:** multiplier (dimensionless) on rotation_power_ratio when shield_stock + unit_cost/2 >= current build cost.
- **Formula at the primary call site:** deepdefence.c:2661 deep_melt_surplus_defenders(): `need *= tune->rotation_finish_relax;` (need initial = rotation_power_ratio).
- **All call sites:**
  - ai/deep/deepdefence.c:2661 -- deep_melt_surplus_defenders -- relaxes the melt bar when recovered shields finish the build -- yes (default 0.5; only when that condition holds)
  - live at compiled defaults summary: yes (conditional: recovered shields would finish the build)
- **Log line:** deep-rotate has:disband (proxy; knobcensus LIVE proxy 12/12)
- **Interacts with:** rotation_power_ratio
- **Tier/screen:** unranked/tier C; in G, GCIVIL, ECON, ALL. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GCIVIL.
- **Prior evidence:** none. pv3 tuned 0.354.

### pop_insurance_gain
- **Declared:** deeptuning.h:1682 (`double pop_insurance_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "WALLS AS POP-LOSS INSURANCE... Honest price: E[successful attacks] x the citizen's stream value... this gain absorbs the horizon scale exactly as horizon_health_weight does. 0 = off."
- **Unit:** multiplier (dimensionless) on `insured * per_citizen`: insured = MIN(expected successful enemy attacks `hits` over survival_horizon, size-1) -- a COUNT of citizens at risk -- times weighted per-turn yield of one citizen (deep_unworked_tile_value). Lump, not amortized.
- **Formula at the primary call site:** deephorizon.c:1757-1759 deep_pop_insurance_credit(): `credit = (adv_want)(insured * deep_unworked_tile_value(pcity, adv, 3, NULL) * tune->pop_insurance_gain);`
- **All call sites:**
  - ai/deep/deephorizon.c:1736 -- deep_pop_insurance_credit -- early return when gain<=0 -- NO: dead (default 0.0)
  - ai/deep/deephorizon.c:1759 -- deep_pop_insurance_credit -- EFT_UNIT_NO_LOSE_POP credit -- NO
  - ai/deep/deephorizon.c:2123 -- deep_adjust_building_wants -- gates the per-city defence read (DP run) -- NO
  - ai/deep/deephorizon.c:2146 -- deep_adjust_building_wants -- wartime-skip exemption -- NO
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-popins (per player-turn)
- **Interacts with:** survival_target, survival_horizon (hits), horizon_health_weight (same currency), stance_growth_gain
- **Tier/screen:** unranked/tier C; blocks G, GHORIZON, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GHORIZON.
- **Prior evidence:** findings/m4b-deterrence-2026-08-07.md:649: live at 1876 deep-popins lines over 20 games, median credit 7.85/mean 44.11 (census, with the knob on); M4b arms "null on score at face value" (:660-670). pv3 tuned value 0.0816 (pv3.ini:151) -- not isolated.

### heal_stream_gain
- **Declared:** deeptuning.h:1703 (`double heal_stream_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "heal_stream_gain prices HP THROUGHPUT: the ruleset's own healing arithmetic says how many turns of the line each damaged unit in this city gets back, and each returned unit-turn is worth that unit's shields at SHIELD_WEIGHTING. ... Both 0 = off."
- **Unit:** multiplier (dimensionless) on `saved * SHIELD_WEIGHTING` where saved = sum over damaged units in the city of (turns-to-heal now - turns-to-heal with building) * unit shield cost (SHIELD_WEIGHTING = 17, advbuilding.h:20). Units: shield-turns x 17 = building_want currency. Lump.
- **Formula at the primary call site:** deephorizon.c:1558 deep_heal_stream_credit(): `credit = (adv_want)(saved * SHIELD_WEIGHTING * tune->heal_stream_gain);`
- **All call sites:**
  - ai/deep/deephorizon.c:1513 -- deep_heal_stream_credit -- early return when gain<=0 -- NO: dead (default 0.0)
  - ai/deep/deephorizon.c:1558 -- deep_heal_stream_credit -- Barracks EFT_HP_REGEN credit -- NO
  - ai/deep/deephorizon.c:2149 -- deep_adjust_building_wants -- wartime-skip exemption -- NO
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-barracks (heal=, healcr= fields)
- **Interacts with:** veteran_stream_gain (same Barracks), horizon_decay_* (lump: invariant)
- **Tier/screen:** unranked/tier C; blocks G, GHORIZON, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GHORIZON.
- **Prior evidence:** STATUS.md:11344-11375 (M4b item 4 population arm): heal_stream_gain=1.0 + veteran_stream_gain=1.0 (credit-barracks.ini), smoke 10 paired seeds: score -7.80 +/- 3.31 (t -2.36, 2/10) -- negative at face value, joint arm; STATUS.md:10738 "untuned AND untested against live data" (flag-gated effects invisible to the accessor, civ2civ3 Barracks: only 21 heal candidates). pv3 tuned 0.8126 (pv3.ini:152).

### veteran_stream_gain
- **Declared:** deeptuning.h:1704 (`double veteran_stream_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "veteran_stream_gain prices the VETERAN MARGIN... Value per unit = (power ratio - 1) x its shields; units over the horizon = horizon / turns to build one. Both factors are the ruleset's own numbers. Both 0 = off."
- **Unit:** multiplier (dimensionless) on `per_horizon * margin * unit_shield_cost * SHIELD_WEIGHTING`: per_horizon = deep_horizon_turns()/ttb (count of units built within the horizon), margin = power_fact ratio - 1 (dimensionless fraction), shield cost in shields, SHIELD_WEIGHTING = 17. building_want currency.
- **Formula at the primary call site:** deephorizon.c:1667-1669 deep_veteran_stream_credit(): `credit = (adv_want)(per_horizon * margin * utype_build_shield_cost_base(best) * SHIELD_WEIGHTING * tune->veteran_stream_gain);` then deep_decay_reprice(credit, per_horizon, 0, ttb, -1, &dur) (line 1673).
- **All call sites:**
  - ai/deep/deephorizon.c:1608 -- deep_veteran_stream_credit -- early return when gain<=0 -- NO: dead (default 0.0)
  - ai/deep/deephorizon.c:1669 -- deep_veteran_stream_credit -- Barracks EFT_VETERAN_BUILD credit -- NO
  - ai/deep/deephorizon.c:1682 -- deep_veteran_stream_credit -- same in deep-decay shape b -- NO
  - ai/deep/deephorizon.c:2150 -- deep_adjust_building_wants -- wartime-skip exemption -- NO
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-barracks (vet=, vetcr= fields), deep-decay fam=veteran
- **Interacts with:** heal_stream_gain, horizon_decay_* (re-prices per_horizon), SHIELD_WEIGHTING
- **Tier/screen:** unranked/tier C; blocks G, GHORIZON, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GHORIZON.
- **Prior evidence:** Same joint arm as heal_stream_gain: STATUS.md:11344-11375, smoke n=10: score -7.80 +/- 3.31 (t -2.36), 6300 credited veteran candidates; pv3 tuned 0.067 (pv3.ini:153).

### horizon_decay_gate
- **Declared:** deeptuning.h:2853 (`int horizon_decay_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "DECAY slice 2 -- the turn-scheduled exponential duration discount... horizon_decay_gate = 1 re-prices the DURATION of every such stream as a truncated geometric: an instalment arriving t turns after the first is worth (1-r)^t of it... 0 = off = every site keeps the legacy rectangle byte-identically." (shared comment over the five horizon_decay_* knobs)
- **Unit:** boolean gate (int 0/1). `<= 0` => legacy MORT rectangle; any >0 value enables the schedule.
- **Formula at the primary call site:** deephorizon.c:299 deep_decay_duration(): `if (deep_tuning_get()->horizon_decay_gate <= 0) { return steps; }`; deephorizon.c:245 deep_decay_rate_now(): `if (tune->horizon_decay_gate <= 0) { return 0.0; }`; deephorizon.c:133 deep_decay_reprice(): `if (deep_tuning_get()->horizon_decay_gate <= 0 || credit <= 0 || steps <= 0.0) return credit;`
- **All call sites:**
  - ai/deep/deephorizon.c:133 -- deep_decay_reprice -- bypass for all horizon credits -- NO: dead (default 0)
  - ai/deep/deephorizon.c:245 -- deep_decay_rate_now -- rate=0.0 with gate off (also the liveness field on census lines) -- NO
  - ai/deep/deephorizon.c:299 -- deep_decay_duration -- the shared duration helper (also reached from deepgov.c:545, deepdefence.c:7842 and the upkeep bill :2394) -- NO
  - (comment) ai/deep/deephorizon.h:53
  - live at compiled defaults summary: no (default 0 = rectangle)
- **Log line:** deep-decay and deep-decay-sum (rate= field is 0 at gate 0); census lines emit at defaults, the discount does not
- **Interacts with:** horizon_decay_early/late/break_start/break_end (all inert when gate=0 or both rates 0), endgame_horizon, endgame_window, every horizon_*_weight (the steps they feed), gov_valuation, unit_upkeep_charge
- **Tier/screen:** unranked (tier C, pinned) -- NOT in block DECAYTECH (that block holds the four schedule knobs + techgoal_distance_power and presumes the gate is set by hand); present in GPROD, G, ECON, ALL. Not individually listed in the knob-screen tables. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GPROD.
- **Prior evidence:** DECAY slice 2 A/B (STATUS.md:14801-14913), tech50war-200 mixed, 16 mirrored seeds, one pinned binary, pv3 vs pv3-decay.ini (gate 1, early 0.037, late 0.002, break 50/100 -- hand anchors): deep score +5.91 +/- 3.52 (t +1.68, 11/16 wins) -- a LEAN, not banked; byte-identity at defaults 20/20.  DECAYTECH three-way holdout (STATUS.md:15229-15290), tech50war-200, seeds 101-116: tuned (0.0571/0.0370/22/163, + techgoal) vs pv3 +54.6 mean but +4.4 over 15 seeds without seed 104 (+807), median +62, 10/16 wins -- "suggestive, not conclusive"; hand-picked anchors vs pv3 -13.6, 9/16 -- wash-to-loss. indirect users of the schedule: deep_decay_duration() is called from deephorizon.c deep_decay_reprice (:137, used by capacity :581, content :706, growth :774, multiplier :939, health :2335, govcenter :1264, upkeepfree :1414, veteran :1673, culture :1834), the upkeep bill (:2394), deepgov.c:545 (government payoff) and deepdefence.c:7842 (unit-lifetime upkeep charge)

### horizon_decay_early
- **Declared:** deeptuning.h:2854 (`double horizon_decay_early;`); params.ini: default=0.0 min=0.0 max=0.12 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same DECAY comment: "horizon_decay_early applies through horizon_decay_break_start... The rate defaults are PLACEHOLDER 0 (a zero rate keeps the legacy window even with the gate on)."
- **Unit:** probability 0-1 per turn (per-turn hazard-plus-growth discount rate r); an instalment t turns after the first is worth (1-r)^t. Measured anchor 0.037/turn before T50 (decay-anchors.py, 98-scorelog corpus).
- **Formula at the primary call site:** deephorizon.c:249-255 deep_decay_rate_now(): `if (turn <= bs) return tune->horizon_decay_early;` ... interpolation `tune->horizon_decay_early + (late - early) * ((turn - bs)/(be - bs))`; consumed at deephorizon.c:314-315 `q = 1.0 - rate; ratio = pow(q, spacing)`.
- **All call sites:**
  - ai/deep/deephorizon.c:249 -- deep_decay_rate_now -- early-phase rate (turn <= break_start) -- NO: dead behind horizon_decay_gate=0 (and default rate 0.0)
  - ai/deep/deephorizon.c:254-255 -- deep_decay_rate_now -- interpolation start -- NO
  - (comment) ai/deep/deephorizon.c:226
  - live at compiled defaults summary: no (gate 0; default rate 0.0)
- **Log line:** deep-decay / deep-decay-sum rate= (0 while gate off)
- **Interacts with:** horizon_decay_gate, horizon_decay_late, horizon_decay_break_start, horizon_decay_break_end
- **Tier/screen:** unranked/tier C (pinned) -- block DECAYTECH. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, DECAYTECH, GPROD.
- **Prior evidence:** DECAY slice 2 A/B (STATUS.md:14801-14913), tech50war-200 mixed, 16 mirrored seeds, one pinned binary, pv3 vs pv3-decay.ini (gate 1, early 0.037, late 0.002, break 50/100 -- hand anchors): deep score +5.91 +/- 3.52 (t +1.68, 11/16 wins) -- a LEAN, not banked; byte-identity at defaults 20/20.  DECAYTECH three-way holdout (STATUS.md:15229-15290), tech50war-200, seeds 101-116: tuned (0.0571/0.0370/22/163, + techgoal) vs pv3 +54.6 mean but +4.4 over 15 seeds without seed 104 (+807), median +62, 10/16 wins -- "suggestive, not conclusive"; hand-picked anchors vs pv3 -13.6, 9/16 -- wash-to-loss

### horizon_decay_late
- **Declared:** deeptuning.h:2855 (`double horizon_decay_late;`); params.ini: default=0.0 min=0.0 max=0.12 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same DECAY comment: "horizon_decay_late from horizon_decay_break_end".
- **Unit:** probability 0-1 per turn (late-phase discount rate); measured anchor ~0.002/turn after T100.
- **Formula at the primary call site:** deephorizon.c:252 deep_decay_rate_now(): `if (turn >= be || be <= bs) return tune->horizon_decay_late;`
- **All call sites:**
  - ai/deep/deephorizon.c:252 -- deep_decay_rate_now -- late-phase rate -- NO: dead behind gate=0 (default 0.0)
  - ai/deep/deephorizon.c:255 -- deep_decay_rate_now -- interpolation end -- NO
  - live at compiled defaults summary: no (gate 0; default rate 0.0)
- **Log line:** deep-decay / deep-decay-sum rate=
- **Interacts with:** horizon_decay_gate, horizon_decay_early, break_start/end
- **Tier/screen:** unranked/tier C (pinned) -- block DECAYTECH. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, DECAYTECH, GPROD.
- **Prior evidence:** DECAY slice 2 A/B (STATUS.md:14801-14913), tech50war-200 mixed, 16 mirrored seeds, one pinned binary, pv3 vs pv3-decay.ini (gate 1, early 0.037, late 0.002, break 50/100 -- hand anchors): deep score +5.91 +/- 3.52 (t +1.68, 11/16 wins) -- a LEAN, not banked; byte-identity at defaults 20/20.  DECAYTECH three-way holdout (STATUS.md:15229-15290), tech50war-200, seeds 101-116: tuned (0.0571/0.0370/22/163, + techgoal) vs pv3 +54.6 mean but +4.4 over 15 seeds without seed 104 (+807), median +62, 10/16 wins -- "suggestive, not conclusive"; hand-picked anchors vs pv3 -13.6, 9/16 -- wash-to-loss

### horizon_decay_break_start
- **Declared:** deeptuning.h:2856 (`int horizon_decay_break_start;`); params.ini: default=50 min=1 max=200 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same DECAY comment: last turn of the early-rate phase ("the 50/100 break defaults are the census's measured phase boundaries, not invented values").
- **Unit:** turns (absolute game turn number, int; registered 1..200).
- **Formula at the primary call site:** deephorizon.c:242-248 deep_decay_rate_now(): `int bs = tune->horizon_decay_break_start; ... if (turn <= bs) return tune->horizon_decay_early;`
- **All call sites:**
  - ai/deep/deephorizon.c:242 -- deep_decay_rate_now -- phase boundary -- NO: dead behind gate=0
  - (comment) ai/deep/deephorizon.c:227
  - live at compiled defaults summary: no (gate 0)
- **Log line:** none directly (rate= on deep-decay shows its effect)
- **Interacts with:** horizon_decay_break_end, early, late, gate
- **Tier/screen:** unranked/tier C (pinned) -- block DECAYTECH. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, DECAYTECH, GPROD.
- **Prior evidence:** DECAY slice 2 A/B (STATUS.md:14801-14913), tech50war-200 mixed, 16 mirrored seeds, one pinned binary, pv3 vs pv3-decay.ini (gate 1, early 0.037, late 0.002, break 50/100 -- hand anchors): deep score +5.91 +/- 3.52 (t +1.68, 11/16 wins) -- a LEAN, not banked; byte-identity at defaults 20/20.  DECAYTECH three-way holdout (STATUS.md:15229-15290), tech50war-200, seeds 101-116: tuned (0.0571/0.0370/22/163, + techgoal) vs pv3 +54.6 mean but +4.4 over 15 seeds without seed 104 (+807), median +62, 10/16 wins -- "suggestive, not conclusive"; hand-picked anchors vs pv3 -13.6, 9/16 -- wash-to-loss; the tuner moved it to 22 (decaytech-best.ini).

### horizon_decay_break_end
- **Declared:** deeptuning.h:2857 (`int horizon_decay_break_end;`); params.ini: default=100 min=1 max=300 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** Same DECAY comment: first turn of the late-rate phase; linear interpolation between break_start and break_end.
- **Unit:** turns (absolute game turn number, int; registered 1..300).
- **Formula at the primary call site:** deephorizon.c:243-253 deep_decay_rate_now(): `int be = ...break_end; if (turn >= be || be <= bs) return late;` interpolation denominator `(double)(be - bs)`.
- **All call sites:**
  - ai/deep/deephorizon.c:243 -- deep_decay_rate_now -- phase boundary -- NO: dead behind gate=0
  - (comment) ai/deep/deephorizon.c:228
  - live at compiled defaults summary: no (gate 0)
- **Log line:** none directly (rate=)
- **Interacts with:** horizon_decay_break_start, early, late, gate
- **Tier/screen:** unranked/tier C (pinned) -- block DECAYTECH. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, DECAYTECH, GPROD.
- **Prior evidence:** DECAY slice 2 A/B (STATUS.md:14801-14913), tech50war-200 mixed, 16 mirrored seeds, one pinned binary, pv3 vs pv3-decay.ini (gate 1, early 0.037, late 0.002, break 50/100 -- hand anchors): deep score +5.91 +/- 3.52 (t +1.68, 11/16 wins) -- a LEAN, not banked; byte-identity at defaults 20/20.  DECAYTECH three-way holdout (STATUS.md:15229-15290), tech50war-200, seeds 101-116: tuned (0.0571/0.0370/22/163, + techgoal) vs pv3 +54.6 mean but +4.4 over 15 seeds without seed 104 (+807), median +62, 10/16 wins -- "suggestive, not conclusive"; hand-picked anchors vs pv3 -13.6, 9/16 -- wash-to-loss; the tuner moved it to 163.

### horizon_govcenter_weight
- **Declared:** deeptuning.h:2761 (`double horizon_govcenter_weight;`); params.ini: default=0.0 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "horizon_govcenter_weight prices EFT_GOV_CENTER at what it saves the REST of the empire... for every OTHER city, the waste saved by moving its nearest centre from where it is to this candidate, summed over outputs and valued at the same output priorities... over the planning horizon. 0.0 = off = byte-identical. PLACEHOLDER."
- **Unit:** multiplier (dimensionless) on a per-turn waste-saving sum (output x priority, building_want currency) times deep_horizon_turns() (turns). Same dimension as the gold upkeep bill it is netted against.
- **Formula at the primary call site:** deephorizon.c:1260 deep_gov_center_credit(): `credit *= deep_horizon_turns() * tune->horizon_govcenter_weight;` then deep_decay_reprice(credit, deep_horizon_turns(), 0, 1, -1, &dur).
- **All call sites:**
  - ai/deep/deephorizon.c:1137 -- deep_gov_center_credit -- early return when weight<=0 -- NO: dead (default 0.0)
  - ai/deep/deephorizon.c:1260 -- deep_gov_center_credit -- Ecclesiastical-Palace credit -- NO
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-govcenter (per player-turn, only when cities found a saving)
- **Interacts with:** horizon_decay_*, upkeep_gold_weight, other horizon credits (summed in add)
- **Tier/screen:** unranked/tier C; blocks G, GHORIZON, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GHORIZON.
- **Prior evidence:** STATUS.md:9641-9700 (DARKFX): STRUCT arm (govcenter+upkeepfree at 1.0) isolated-150 20 seeds: -49.40 (t -10.67) before move fix, +2.15 (t +0.83) after; contested STRUCT -1.89 (t -0.88) -- joint arm with horizon_upkeepfree_weight, null after fix. pv3 sets 0.0.

### horizon_upkeepfree_weight
- **Declared:** deeptuning.h:2781 (`double horizon_upkeepfree_weight;`); params.ini: default=0.0 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "...EFT_UNIT_UPKEEP_FREE_PER_CITY... The credit is what the empire is actually paying today that the effect would stop paying: per city, the upkeep its supported units really pay in that output, capped at the units the new free allowance would cover. 0.0 = off = byte-identical. PLACEHOLDER."
- **Unit:** multiplier (dimensionless) on a per-turn saved-upkeep sum (output x priority) times deep_horizon_turns().
- **Formula at the primary call site:** deephorizon.c:1410 deep_upkeep_free_credit(): `credit *= deep_horizon_turns() * tune->horizon_upkeepfree_weight;` then deep_decay_reprice(...).
- **All call sites:**
  - ai/deep/deephorizon.c:1324 -- deep_upkeep_free_credit -- early return when weight<=0 -- NO: dead (default 0.0)
  - ai/deep/deephorizon.c:1410 -- deep_upkeep_free_credit -- King Richard's/Statue of Zeus credit -- NO
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-upkeepfree
- **Interacts with:** horizon_decay_*, upkeep_gold_weight, unit upkeep
- **Tier/screen:** unranked/tier C; blocks G, GHORIZON, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GHORIZON.
- **Prior evidence:** STATUS.md:9641-9700: joint STRUCT arm with horizon_govcenter_weight (see that entry): +2.15 (t +0.83, 10/20) isolated-150 after move fix; -1.89 (t -0.88) contested. pv3 sets 0.138719 (pv3.ini:217).

### horizon_culture_weight
- **Declared:** deeptuning.h:2813 (`double horizon_culture_weight;`); params.ini: default=0.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "The credit is the score arithmetic and nothing else: a building's EFT_HISTORY accrues every turn between completion and the bell, so it is worth h x turns-left / 50 score points, and deep already has a want price for one score point -- the citizen... 1.0 therefore means 'a culture point is worth what deep pays for the citizen that scores the same point', which is an UPPER bound... 0.0 = off. PLACEHOLDER."
- **Unit:** multiplier (dimensionless) on `(gained / DEEP_CULTURE_PER_SCORE) * deep_unworked_tile_value(...)`: gained = history points over `hor` turns (turns left to the bell, or MORT if no bell); /50 converts culture to score points; times per-citizen weighted yield. Result in building_want currency.
- **Formula at the primary call site:** deephorizon.c:1823-1825 deep_culture_credit(): `credit = gained / DEEP_CULTURE_PER_SCORE * deep_unworked_tile_value(pcity, adv, 3, NULL) * tune->horizon_culture_weight;`
- **All call sites:**
  - ai/deep/deephorizon.c:1783 -- deep_culture_credit -- early return when weight<=0 -- NO: dead (default 0.0)
  - ai/deep/deephorizon.c:1825 -- deep_culture_credit -- EFT_HISTORY credit -- NO
  - ai/deep/deephorizon.c:2141 -- deep_adjust_building_wants -- wartime-gate exemption (`horizon_culture_weight <= 0.0` keeps the old skip) -- NO
  - live at compiled defaults summary: no (default 0.0 = off)
- **Log line:** deep-culture
- **Interacts with:** horizon_decay_*, endgame_* (turns left), stance (culture axis), upkeep_gold_weight
- **Tier/screen:** unranked/tier C; blocks G, GHORIZON, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON, GHORIZON.
- **Prior evidence:** STATUS.md:9641-9700: CULT arm (culture=1.0) isolated-150, 20 paired seeds: -19.43 (t -5.55, 3/20 wins) -- negative at the 150-turn horizon (culture 14.5% of score at T150); findings/darkfx-2026-08-06.md. pv3 sets 0.0.

### techgoal_deep
- **Declared:** deeptuning.h:2937 (`int techgoal_deep;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "TECHMIL slice 2 -- deep's own research-goal selection (deeptech.c)... techgoal_deep = 0 (default): deep does not touch research selection... != 0: after stock has committed its choice, deep re-runs the same selection over the same tech_want[] mass with the distance divisions raised to techgoal_distance_power."
- **Unit:** boolean gate (int; `== 0` off, any non-zero on).
- **Formula at the primary call site:** deeptech.c:279 deep_manage_tech(): `if (tune->techgoal_deep == 0) { return; }` (after dai_manage_tech) else `deep_reselect_research(ait, pplayer, tune->techgoal_distance_power);`
- **All call sites:**
  - ai/deep/deeptech.c:279 -- deep_manage_tech -- gate for the reselection -- NO: dead (default 0)
  - (comment) ai/deep/deeptech.h:21; deepdefence.c:9524; deeptuning.c:333
  - live at compiled defaults summary: no (default 0 = stock selection)
- **Log line:** deep-techgoal (counter: goals differ, researching moved)
- **Interacts with:** techgoal_distance_power, horizon_tech_weight (writes tech_want[]), stock dai_select_tech
- **Tier/screen:** unranked/tier C (mode switch); in M, MIL, ALL (not in DECAYTECH). Dead at defaults. Blocks containing it (tuning-blocks.ini): M, ALL, MIL.
- **Prior evidence:** STATUS.md:14919-15020 (TECHMIL slice 2): gate on at power 1.0 vs off, tech50war-200 mixed, 16 holdout seeds: all 32 pairs BYTE-IDENTICAL (margin -259.50 both); 25,600 deep-techgoal lines; goals differ 0.6%, military research share 57.1% both -- proven inert at stock shape. DECAYTECH tune then ran it ON with power 0.9406 (STATUS.md:15229-15290; tuned vs pv3 +54.6, +4.4 without seed 104, n=16).

### techgoal_distance_power
- **Declared:** deeptuning.h:2945 (`double techgoal_distance_power;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "The exponent on both of dai_select_tech()'s distance divisions when techgoal_deep is on; read nowhere else. 1.0 is stock's shape; smaller is longer-sighted, 0.0 ignores distance entirely, larger is more near-sighted than stock. PLACEHOLDER."
- **Unit:** exponent (dimensionless) in `want / pow(steps, power)` with steps = research_goal_unknown_techs (count of techs to goal).
- **Formula at the primary call site:** deeptech.c:152 and 182 deep_reselect_research(): `... / pow((double) steps, power);` and `goal_values[i] /= pow((double) steps, power);`
- **All call sites:**
  - ai/deep/deeptech.c:284 -- deep_manage_tech -- passed as `power` into deep_reselect_research -- NO: dead behind techgoal_deep=0 (default 1.0 is stock shape)
  - live at compiled defaults summary: no (gated by techgoal_deep=0)
- **Log line:** deep-techgoal (power= field, deeptech.c:245)
- **Interacts with:** techgoal_deep, horizon_tech_weight
- **Tier/screen:** unranked/tier C; BLOCK DECAYTECH member (tuning-blocks.ini:240); also M, MIL, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): M, ALL, MIL, DECAYTECH.
- **Prior evidence:** STATUS.md:15229-15290: tuned to 0.9406 as part of the DECAYTECH incumbent (with decay knobs); tuned vs pv3 +54.6 mean, +4.4 over 15 seeds ex-seed-104, 10/16 wins; STATUS.md:15008: a power-alone sweep is "owed" and no record of it having run.

### terrain_chain_gate
- **Declared:** deeptuning.h:3069 (`int terrain_chain_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1–3 lines, quote key phrases):** "W29: does deep price a terrain-changing worker action by the best terrain the CHAIN reaches, or by the one terrain the action itself produces?... Gate rather than magnitude, and no companion weight... 0 is today's one-step valuation exactly."
- **Unit:** boolean gate (int 0/1).
- **Formula at the primary call site:** deepchain.c:215 deep_chain_value(): `if (tune->terrain_chain_gate <= 0 || n <= 1) { return one_step; }`
- **All call sites:**
  - ai/deep/deepchain.c:215 -- deep_chain_value -- terrain-change (cultivate/plant) price lift via chain reach -- NO: dead (default 0; census counters still run)
  - (comment) ai/deep/deepchain.h:49
  - live at compiled defaults summary: no (default 0 = off)
- **Log line:** deep-terrachain (cand=, multi=, lift=, gain=, tiles=, work=, done=)
- **Interacts with:** W28 stacking, city_tile_value / amortize (stock)
- **Tier/screen:** unranked/tier C; in G, ECON, ALL. Dead at defaults. Blocks containing it (tuning-blocks.ini): G, ALL, ECON.
- **Prior evidence:** STATUS.md:19466-19600 (W29): identity 10/10 byte-identical (3 arms); liveness garden.conf 6 seeds: lift 500.8/game but 0 of 8.2 lifted tiles ever changed terrain, 6/6 seeds byte-identical, score delta +0.00 -- "price too small to win" (null by proof).

## Unresolved

- **Per-knob screen verdicts.** knob-screen-2026-08-30.md prints full tables only for the shortlist (5 of these knobs: the AUCTION horizon weights), the tier-B fires list (worker_flee_radius) and the dead-path class summary ("trunk/road family (10 knobs)"). The classification of the remaining knobs (cm_*, worker_*, rotation_*, pop/heal/veteran, decay, govcenter/upkeepfree/culture, techgoal_*, terrain_chain_gate, idle_*) comes from `benchmark/knobscreen.py` output over the archived 40-game corpus, which I did not run (no game-corpus reads were requested). Entries marked "not screened" / "dead at defaults (not screened)" mean the docs do not print their class, not that they were screened and found inert. knob-coverage.md (2026-08-07 census) rates all cm_* as NO-INSTRUMENT, but that predates deep-cmpost/-cmgrow/-cmfound/-cmspec/-cmsolve, which exist only behind off-by-default knobs; the base posture machine (WAR/CAP/WONDER/GROW) still has no census line.
- **deeptuning.h:1346-1350 comment attribution.** The comment block immediately above `cm_grow_food/shield/trade` ("Weight on garrison attack ratings counted as defence in the grow-release coverage test...") is the doc for `sortie_defence_weight` (declared at :1347); the three cm_grow_* factors therefore have no header comment of their own. Intent for them is inferred from deepcm.c and the cm_grow_below comment.
- **cm_war_food is also the WONDER-posture food factor** (deepcm.c:426); there is no `cm_wonder_food`. Not documented in the header.
- **Single-knob effects are largely unmeasured.** Almost every arm in STATUS.md moved several keys at once (pv3 package vs defaults, GCM/GHORIZON block ablations, the five-key W12 family, keep_gate+keep_margin, heal+veteran, govcenter+upkeepfree, decay gate+early+late+breaks, DECAYTECH incumbent with techgoal). "Prior A/B" words for those are joint-arm signs, not attributions. Single-knob arms exist only for: horizon_content_weight (n=3, noise), horizon_growth_weight (n=5, noise), cm_war_survival (gate vs presence counts, n=10), trunk_ally_gate, trunk_trade_gain, trunk_stack_gate, trunk_upgrade_gate, techgoal_deep (inert), idle_policy_gate, terrain_chain_gate, idle_reserve_per_city (isolation arm).
- **No standalone A/B found** for: horizon_capacity_weight, horizon_bundle_weight, horizon_multiplier_weight, horizon_health_weight, horizon_tech_weight, all cm_* factors, worker_flee/guard/danger_discount/cross_continent (value never varied), rotation_*, pop_insurance_gain, trunk_soft_border_gate, trunk_risk_weight, cm_founder_food, cm_grow_price_blend, cm_war_growth_charge, cm_specialist_*.
- **Log-line line numbers** quoted with "~" are approximate (deep-flee, deep-guard, deep-rotate); all other line numbers were read directly.
- **Decay knobs' indirect consumers.** horizon_decay_* are read only inside deephorizon.c deep_decay_rate_now(), but the resulting duration discount reaches deepgov.c:545 and deepdefence.c:7842 through deep_decay_duration(); those two are additionally dormant unless gov_valuation / unit_upkeep_charge are on (STATUS.md:14845-14855), so liveness of the schedule outside deephorizon.c at defaults is nil for three reasons (gate, rate, parent mechanism).
- **horizon_*_weight units, caveat.** The credit currency is "tile-sum" (priority-weighted per-turn yield of the mean of the best 3 unworked tiles), added to stock building_want[]. How building_want compares to unit/military wants after the later merge (tier-first, DAI_WANT_DOMESTIC_MAX clamp 750.075, per screen doc) was not re-derived here; see knob-screen Part 1 "inert" table for the clamp.
- **Statement that the AUCTION block puts horizon_growth/bundle/content/multiplier in tier A** rests on tuning-blocks.ini header + block line 95; RESCREEN (retired) also lists them.
