# Knob inventory E: military operations layer (49 knobs)

Facts only. Generated 2026-10-26 from the tree at branch epic/w03-horizon-content (read-only pass; no build, no games). Sources: ai/deep/deeptuning.h, deeptuning.c, benchmark/tuning-params.ini, tuning-blocks.ini, ai/deep/deepops.c, deepdefence.c, deepcm.c, deepcombat.c, deepspy.c, doc/deep-ai/STATUS.md, findings/*.md. Line numbers are for this tree. "Live at defaults" means the code path executes with the compiled default vector; it does not mean the path fires in a given venue (population notes are in the entries). Benchmark venues use the server-default ruleset (civ2civ3: game.info.tired_attack = TRUE); the classic cells have it FALSE.

Conventions: SHIELD_WEIGHTING = 17 want per shield (server/advisors/advbuilding.h:20). adv_want = stock "want" currency. DAI_WANT_DOMESTIC_MAX = DAI_WANT_MILITARY_EMERGENCY/4*3 (ai/default/daitools.h:33). Op lifecycle in deepops.c: form (deep_op_form) -> MUSTER (deep_op_drive, members walk to the staging city) -> launch (go/patience, deep_op_launch) -> MARCH (re-gate each turn, contact at eta <= 1) -> STRIKE (deep_op_drive_strike) -> CONSOLIDATE -> dissolve/abort (deep_op_dissolve). All odds are probabilities 0-1 from deep_take_odds_cost()/deep_take_odds_at_arrival(); required, pool and quorum are unit counts; worth/value/cost are adv_want; loss is shields until multiplied by SHIELD_WEIGHTING.

## Summary table

| knob | unit | default | live at defaults? | tier | screen verdict | prior A/B | # call sites |
|---|---|---|---|---|---|---|---|
| sortie_defence_weight | multiplier | 0.5 | yes | unranked | not listed (instrument: see entry) | none | 2 |
| mil_opportunity_gate | boolean gate | 0 | no | unranked | not listed (dead-path inferred) | null (stacked: positive) | 1 |
| mil_opportunity_price | multiplier | 1.0 | no | B | not screened (tier-B block) | none (price never varied) | 2 |
| convoy_min_load | count | 2 | yes | unranked | not listed (instrument: see entry) | none | 2 |
| convoy_wait_radius | tiles | 6 | yes | unranked | not listed (instrument: see entry) | none | 1 |
| tired_attack_floor | probability 0-1 | 0.9 | yes (code reachable on civ2civ3; zero observed firings) | unranked | not listed (instrument: see entry) | none | 3 |
| attack_value_weight | multiplier | 1.0 | yes | B | tier-B firings 2384/game (flip not computable) | none (never varied alone) | 3 |
| attack_loss_gain | multiplier | 1.0 | yes | B | tier-B firings 2384/game (flip not computable) | null | 6 |
| conquest_worth_mult | multiplier | 2.0 | yes | B | tier-B firings 2389/game (flip not computable) | null | 1 |
| defence_worth_mult | multiplier | 2.0 | yes (single site; only on counter-claim events) | unranked | not listed (instrument: see entry) | null | 2 |
| assault_pool_reach | boolean gate | 1 | yes | unranked | not listed (instrument: see entry) | null | 1 |
| sortie_win_floor | probability 0-1 | 0.85 | yes | unranked | not listed (instrument: see entry) | none | 2 |
| sortie_prize_gain | multiplier | 0.0 | no | unranked | not listed (dead-path inferred) | null | 2 |
| deterrence_gain | UNSURE | 0.0 | no | unranked | not listed (dead-path inferred) | null | 2 |
| cover_removal | boolean gate | 0 | no | unranked | not listed (dead-path inferred) | none | 1 |
| op_take_target | probability 0-1 | 0.7 | yes | A | flips 4.9/game | none | 9 |
| op_muster_wait | turns | 10 | yes | unranked | not listed (instrument: see entry) | none | 5 |
| op_draft_marginal | boolean gate | 0 | no | unranked | not listed (dead-path inferred) | null | 1 |
| op_muster | boolean gate | 0 | no | unranked | not listed (dead-path inferred) | null | 2 |
| op_muster_stall | turns | 10 | no | unranked | not listed (dead-path inferred) | none | 1 |
| op_muster_home | boolean gate | 1 | no (default 1 inert until op_muster=1) | unranked | not listed (dead-path inferred) | null | 1 |
| op_launch_odds | boolean gate | 1 | yes | unranked | not listed (instrument: see entry) | null | 4 |
| op_net_value | boolean gate | 1 | yes | unranked | not listed (instrument: see entry) | null | 2 |
| op_hold_survivors | count | 1.0 | yes | unranked | not listed (instrument: see entry) | none | 1 |
| invasion_reinforce_rate | count per turn | 0.34 | yes | B | tier-B firings 2387/game (flip not computable) | none | 2 |
| invasion_reinforce_slope | multiplier | 0.0 | no | B | tier-B firings 2348/game (flip not computable) | none | 2 |
| invasion_embark_turns | turns | 2 | yes | B | tier-B firings 641/game (flip not computable) | none | 4 |
| op_wave_window | turns | 6 | yes | unranked | not listed (instrument: see entry) | none | 1 |
| raid_radius | tiles | 12 | yes | unranked | not listed (instrument: see entry) | none | 4 |
| op_abort_floor | probability 0-1 | 0.35 | yes | unranked | not listed (instrument: see entry) | none | 3 |
| spy_enabled | boolean gate | 0 | no | unranked | not listed (dead-path inferred) | none | 2 |
| spy_gold_value | want per gold | 0.5 | no | unranked | not listed (dead-path inferred) | none | 1 |
| spy_sabotage_gain | UNSURE | 0.5 | no | unranked | not listed (dead-path inferred) | none | 2 |
| leader_check_gain | fraction | 0.0 | no | unranked | not listed (dead-path inferred) | none | 1 |
| conquest_culture_gain | multiplier | 0.0 | no | unranked | not listed (dead-path inferred) | null | 2 |
| weak_prey_gate | boolean gate | 0 | no | unranked | unmapped (not screened) | negative | 1 |
| weak_prey_gain | multiplier | 1.0 | no | unranked | unmapped (not screened) | negative (at 1.0, via gate) | 2 |
| op_patience_odds_floor | probability 0-1 | 0.0 | no | unranked | not listed (dead-path inferred) | null | 1 |
| op_arrival_gate | boolean gate | 0 | no | unranked | not listed (dead-path inferred) | null (negative on takes) | 2 |
| op_arrival_decay | probability 0-1 per turn of eta | 1.0 | no | unranked | not listed (dead-path inferred) | null (bundled) | 1 |
| op_launch_eta_cap | turns | 0 | no | unranked | not listed (dead-path inferred) | null | 1 |
| op_march_loss_rate | count per turn of march | 0.0 | no | unranked | not listed (dead-path inferred) | null (bundled) | 1 |
| op_regate_gate | boolean gate | 0 | no | unranked | not listed (dead-path inferred) | null | 1 |
| op_regate_sunk_gain | fraction 0-1 | 0.0 | no | unranked | not listed (dead-path inferred) | null (bundled) | 1 |
| op_regate_eta_freeze | boolean gate | 0 | no | unranked | not listed (dead-path inferred) | null (bundled) | 1 |
| op_contact_quorum | count | 0 | no | unranked | not listed (dead-path inferred) | none | 1 |
| op_draft_attack_floor | rating units | 0.0 | no (identity value) | unranked | not listed (dead-path inferred) | none | 1 |
| mil_infra_gate | boolean gate | 0 | no | unranked | not listed (dead-path inferred) | none | 3 |
| mil_infra_threat_floor | stock danger-score units | 0 | no | unranked | not listed (dead-path inferred) | none | 3 |

Prior A/B column: positive / null / negative / none. "null" means every cited arm sat within roughly 2 SE of zero; magnitudes and n are in each entry.

## Entries

### sortie_defence_weight
- **Declared:** deeptuning.h:1347 (`double sortie_defence_weight;`); params.ini: default=0.5 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Weight on garrison attack ratings counted as defence in the grow-release coverage test: with killstack, attack-heavy units defend by sortie. 0 disables."
- **Unit:** multiplier (dimensionless): scales a sum of adv_unit_att_rating() values before it is ADDED to assess_defense(). Both addends are stock/deep "rating" quantities; note assess_defense is documented in-source as a quadratic of summed linear defence while the sortie term is a linear sum of attack ratings (the weight is the only bridge between the two scales).
- **Formula at the primary call site:** deepcm.c:233 deep_cm_sortie_value(): `return sortie * tune->sortie_defence_weight;` where `sortie += adv_unit_att_rating(punit)` over own non-civilian units on the city tile (deepcm.c:225-232). Consumer, deepcm.c:367-370 (CM goal choice): `assess_defense(ait, pcity) + deep_cm_sortie_value(pcity) >= tune->cm_grow_safety_margin * city_data->danger && deep_cm_survival_ok(pcity)`.
- **All call sites:**
  - deepcm.c:224 deep_cm_sortie_value — off-switch (<=0 returns 0) — yes
  - deepcm.c:233 deep_cm_sortie_value — weights the garrison attack-rating sum fed to the grow-hard-vs-garrison CM test (deepcm.c:369) — yes (only evaluated for young cities, size<cm_grow_below or deep_cm_grow_priced, with city_data->danger != 0)
- **Log line:** none (knobcensus.py:545 says "no line"; mechanism has no census tag)
- **Interacts with:** cm_grow_safety_margin, cm_grow_below, survival_target (via deep_cm_survival_ok)
- **Tier/screen:** unranked (tier C by the header's complement rule). Legacy blocks M, ALL, MIL only.. Screen: not listed in knob-screen-2026-08-30.md; knobcensus map: no instrument ("not-logged" class by construction)
- **Prior evidence:**
  - none

### mil_opportunity_gate
- **Declared:** deeptuning.h:672 (`int mil_opportunity_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "OPPORTUNITY, off by default (gate 0 charges nothing)." The defender want is priced only against the city's danger, never against the founder bid it displaces.
- **Unit:** boolean gate (0/1; range 0..1). Read as `<= 0` in one early-return.
- **Formula at the primary call site:** deepdefence.c:8416-8421 deep_mil_opportunity_charge(): `if (tune->mil_opportunity_gate <= 0 || tune->mil_opportunity_price <= 0.0 || mil->want <= 0 || dom == NULL || dom->want <= 0 || !deep_choice_is_founder(dom)) return;`
- **All call sites:**
  - deepdefence.c:8416 deep_mil_opportunity_charge — master gate for the military-vs-founder displacement charge (single caller deepdefence.c:8828, in the per-city build merge) — no: dead behind mil_opportunity_gate=0
- **Log line:** deep-milopp (turn, player, city, class, mil, founder, charge, after) — emitted only when a charge is made (deepdefence.c:8433)
- **Interacts with:** mil_opportunity_price (the only magnitude behind the gate); deep_merge_domestic_wins comparator (deep_want_cmp on tier,value); founder_tier_gate and founder_site_cap_gate act on the same seam
- **Tier/screen:** unranked (not in a tier A/B block; legacy X, ALL, ECON). Its sibling mil_opportunity_price is in tier B BANTICIP.. Screen: not individually listed in the screen docs. Instrument deep-milopp exists (S-FEARSEAT); at compiled defaults the tag never fires => dead-path by the screen's definition (inferred, not read off a table).
- **Prior evidence:**
  - S-FEARSEAT (STATUS 23422) — contested, 20 paired training seeds, score — +0.10 +/- 2.37 (10/20 wins) — wash; lategame-80 +0.25 +/- 1.86 (8/20). Mechanism: with price 1.0, contested munits@25 -0.61 +/- 0.06 (t -9.8), T1-25 military shields 15.8 -> 8.0; cities@50 +0.188 +/- 0.064 gone by T75.
  - S-FEARSINK (findings/fearsink-verdict-2026-08-30.md:275) — contested 2x2 with site cap/join guard — -1.05 +/- 2.12 (second base); lategame +0.25 +/- 1.86 — wash — n=20 seeds x 2 venues (203 games total)
  - S-MARGINAL (STATUS 24345) — predicted +2.15 +/- 1.28 from founding delta, observed -1.05 +/- 2.12 contested — residual -3.19 +/- 2.48 — n=20
  - S-STACKED (STATUS 24463) — moved as one of five gates together (mil_opportunity_gate, founder_site_cap_gate, founder_join_guard_gate, frontier_transit_gate, boat_shadow_gate) — war score +2.39 +/- 1.02 (t 2.3, 58/100), cities +0.15 +/- 0.04; econ score +4.12 +/- 0.61, cities +0.60 +/- 0.07 — positive as a STACK, not attributable to this gate — n=100 seeds x 2 venues

### mil_opportunity_price
- **Declared:** deeptuning.h:673 (`double mil_opportunity_price;`); params.ini: default=1.0 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "charges the military bid price x (the founder bid it is displacing) whenever the domestic auction's own winner IS a founder... At price 1.0 -- the unit case -- a defender has to beat the founder by the founder's own worth."
- **Unit:** multiplier (dimensionless) on the founder bid, both in adv_want ("want") units. LEFT side: mil->want, deep's military choice (class DWC_DEFENDER or DWC_ATTACKER only), as formed by the military advisor then deep class-scaled/belled/debited. RIGHT side: dom->want, the domestic advisor's winning choice WHEN it is a city founder (CT_CIVILIAN && utype_is_cityfounder), after the same class scale, bell and both absolute shield-equivalent debits; domestic wants are clamped at DAI_WANT_DOMESTIC_MAX = DAI_WANT_MILITARY_EMERGENCY/4*3 (~750.075 per source comment). STATUS 23517 measured a founder bid ~750 against a military bid ~175-235 in the opening, so at 1.0 the charge zeroes the military bid on 19.5% (S-FEARSEAT) / 22.5% (S-FEARSINK) of firings.
- **Formula at the primary call site:** deepdefence.c:8430-8431 deep_mil_opportunity_charge(): `charge = (adv_want) (tune->mil_opportunity_price * dom->want); mil->want = MAX(0, mil->want - charge);` then (8828-8834) `dom_won_after = deep_merge_domestic_wins(newchoice, &city_data->choice, contest)` which is `deep_want_cmp(deep_want_from_choice(dom), deep_want_from_choice(mil)) > 0` (tier first, then value; deepdefence.c:8507-8517). Within one tier the founder wins iff mil_orig < (1 + price) x dom->want.
- **All call sites:**
  - deepdefence.c:8417 deep_mil_opportunity_charge — second early-return guard (price<=0 => no charge) — no: dead behind mil_opportunity_gate=0
  - deepdefence.c:8430 deep_mil_opportunity_charge — the charge itself — no: dead behind mil_opportunity_gate=0
- **Log line:** deep-milopp (as above)
- **Interacts with:** mil_opportunity_gate; founder_tier_gate (deep_founder_contests_emergency => `contest`), founder_site_cap_gate (zeroes founder_want before the advisor so no charge), scale_defender/scale_attacker/bell/upkeep/switch debits applied before the charge
- **Tier/screen:** tier B — BANTICIP (7 constants; "screened as a block, never fit off a block result"). Screen: Not in the Part-1 flip table (log carries only half the comparison; BANTICIP header: "the comparison a flip needs -- the founder bid the defender displaced -- is only half in the log"). Class: tier B, not screened individually. Dead at defaults because the gate is 0.
- **Prior evidence:**
  - S-FEARSEAT (STATUS 23461) — the "price" rung is the same arm as mil_opportunity_gate=1 at price 1.0 (price itself never varied) — see mil_opportunity_gate; calibration note: "1.0 zeroes the military bid on 19.5% of firings; the two sides are not in one currency, RC4" and S-FEARSINK: 22.5% (68.9 of 305.9 firings a game), "a value well below 1.0 or an RC4 currency fix is what it wants" (STATUS 23788)
  - Price magnitude: never swept (only 1.0 tested) — none

### convoy_min_load
- **Declared:** deeptuning.h:845 (`int convoy_min_load;`); params.ini: default=2 min=0 max=8 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "a transport with cargo holds in port until it carries convoy_min_load units (<=1 disables) while boarders are inbound within convoy_wait_radius tiles, and any cargo-carrying transport recruits an idle warship bodyguard."
- **Unit:** count (units aboard; compared with get_transporter_occupancy and capped by transporter capacity).
- **Formula at the primary call site:** deepdefence.c:2810 deep_manage_convoys(): `want = MIN(get_transporter_capacity(boat), tune->convoy_min_load); if (cargo >= want) { continue; }` (cargo = get_transporter_occupancy(boat)); hold only if some own unit assigned to this ferryboat is within convoy_wait_radius (2823-2834).
- **All call sites:**
  - deepdefence.c:2767 deep_manage_convoys — master off-switch (<=1) — yes at 2
  - deepdefence.c:2810 deep_manage_convoys — load bar for a boat sitting in own city with cargo — yes, but the decision requires a loaded transport in port with an inbound boarder: STATUS 12836 measured "LIVE, population 1" (one firing in two naval games)
- **Log line:** deep-convoy (escort= line at 2792; hold line after deep_hold_unit(...,"convoy"))
- **Interacts with:** convoy_wait_radius; boat_* knobs elsewhere
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed in the screen docs. knobcensus (S2, "direct") = deep-convoy flag:hold; STATUS 12836: EXCLUDE stands on a measured population of 1.
- **Prior evidence:**
  - none (no arm moved it; STATUS 1422 "convoy_min_load=2 active" is a shipping note; naval slice 3 had no A/B — "two seeds is not a result")

### convoy_wait_radius
- **Declared:** deeptuning.h:846 (`int convoy_wait_radius;`); params.ini: default=6 min=1 max=20 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "...while boarders are inbound within convoy_wait_radius tiles."
- **Unit:** tiles (compared against real_map_distance between an assigned boarder and the boat).
- **Formula at the primary call site:** deepdefence.c:2826-2828 deep_manage_convoys(): `def_ai_unit_data(p, ait)->ferryboat == boat->id && !same_pos(unit_tile(p), unit_tile(boat)) && real_map_distance(unit_tile(p), unit_tile(boat)) <= tune->convoy_wait_radius`
- **All call sites:**
  - deepdefence.c:2828 deep_manage_convoys — "is a boarder close enough to wait for" — yes (same population caveat as convoy_min_load; unreachable when convoy_min_load<=1)
- **Log line:** deep-convoy (hold form); line does not report the distance (knobcensus "proxy")
- **Interacts with:** convoy_min_load
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 "proxy" instrument
- **Prior evidence:**
  - none

### tired_attack_floor
- **Declared:** deeptuning.h:1466 (`double tired_attack_floor;`); params.ini: default=0.9 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "a unit adjacent to its attack target with only a tired strike left (moves_left < SINGLE_MOVE scales attack power down) holds a turn to strike fresh, unless its tired win chance already reaches this floor. <= 0 disables."
- **Unit:** probability 0-1 (compared against unit_win_chance() and deep_fresh_win_chance()).
- **Formula at the primary call site:** deepops.c:5139-5147 deep_op_drive_strike(): `game.info.tired_attack && tune->tired_attack_floor > 0.0 && att->moves_left > 0 && att->moves_left < SINGLE_MOVE && best < tune->tired_attack_floor` then `if (fresh >= tune->tired_attack_floor) { deep_hold_unit(ait, att, "tired"); continue; }`. Second seat, deepdefence.c:3185 deep_hold_tired_attackers(): `if (chance >= tune->tired_attack_floor) continue;`
- **All call sites:**
  - deepdefence.c:3157 deep_hold_tired_attackers — off-switch (also !game.info.tired_attack) — only when ruleset tired_attack=TRUE (civ2civ3 TRUE, which benchmark configs inherit as server default; classic FALSE)
  - deepdefence.c:3185 deep_hold_tired_attackers — hold iff tired win chance < floor — as above; M1 entry: seat in first_activities can never fire (units have full moves)
  - deepops.c:5139,5142 deep_op_drive_strike — hold iff tired odds below floor AND fresh odds not (5147) — as above; M1 (STATUS 8600): "fires ZERO times in 120 games, both cells"
- **Log line:** deep-tired (seat=strike variant at deepops.c:5149; plain form at deepdefence.c:3187)
- **Interacts with:** survival/duel model via unit_win_chance only; game.info.tired_attack ruleset flag
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed in the screen docs. knobcensus S2 "direct": deep-tired; venue-locked on tired_attack. STATUS 8604: no population.
- **Prior evidence:**
  - M1 (STATUS 8600-8606) — contested + lategame-80, 120 games — mechanism fires 0 times; "Do not count it as a live mechanism" — none

### attack_value_weight
- **Declared:** deeptuning.h:1471 (`double attack_value_weight;`); params.ini: default=1.0 min=0.0 max=10.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Marginal-take-odds attacker want (combat E2): one more attacker is worth weight x target worth x the shift it makes in P(pool takes the best at-war city on this continent), amortized over the march. <= 0 disables."
- **Unit:** multiplier (dimensionless) on `tworth * marginal`; tworth = deep_conquest_worth() in want units (stock ai_city.worth x conquest_worth_mult x prey [+ culture]); marginal = p1 - p0, a probability difference. gross is therefore in want units, before military_amortize().
- **Formula at the primary call site:** deepdefence.c:6890 deep_assault_want(): `gross = tune->attack_value_weight * tworth * marginal;` then (6892-6900) `want = military_amortize(pplayer, pcity, MAX(gross - cost, 0), MAX(1, eta), build_cost_balanced(punittype))`; same product in the snap-up sweep at 6935.
- **All call sites:**
  - deepdefence.c:6620 deep_assault_want — master off-switch (<=0 returns, i.e. no attacker want at all) — yes
  - deepdefence.c:6890 deep_assault_want — marginal-blade want — yes
  - deepdefence.c:6935 deep_assault_want — same weight in the per-type snap-up sweep (only when p1 >= op_take_target) — yes
- **Log line:** deep-kill (deepdefence.c ~7036; carries gross=, loss=, worth=, eworth=, far=, sea=, robs=)
- **Interacts with:** conquest_worth_mult (inside tworth), attack_loss_gain (the cost subtracted), op_take_target (campaign pricing + snap-up), assault_pool_reach, invasion_reinforce_rate/slope/embark_turns (via eta and the odds), weak_prey_gate/gain, attacker_hold_damper, stance assault mult, scale_attacker
- **Tier/screen:** tier B — BASSAULT (6 constants; "group-screen, do not fit"). Screen: tier B firings: 2384 fires/game (knob-screen-2026-08-30.md:171). Part-1 flip rate not computable ("log carries one side of the comparison").
- **Prior evidence:**
  - M1 (STATUS 8464) — the "ALL" arm = whole E2 epic (cost term + reach test + odds launch + tired hold) vs OFF — contested score +1.75 (t +1.27), lategame-80 -1.49 (t -0.84), n=20 paired seeds each; attack_value_weight itself was not varied alone — wash (attribution to this knob: none)
  - objective-layer.md:141 / tuning-first-principles-2026-08-06.md:28 — a6 cells: all gradients 0.000 — none

### attack_loss_gain
- **Declared:** deeptuning.h:1478 (`double attack_loss_gain;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the expected attacker losses of the priced assault, in shields at SHIELD_WEIGHTING, are subtracted from the want before it is amortized. 1.0 charges the loss at exactly the rate the rest of the codebase values a shield; 0 restores the pre-E2 want."
- **Unit:** multiplier (dimensionless) on a shield quantity converted at SHIELD_WEIGHTING = 17 want per shield (server/advisors/advbuilding.h:20). `loss` is expected attacker losses in SHIELDS from deep_take_odds_cost(); the product is want.
- **Formula at the primary call site:** deepdefence.c:6892-6893 deep_assault_want(): `adv_want cost = (adv_want) (tune->attack_loss_gain * MAX(loss, 0.0) * SHIELD_WEIGHTING);` ; launch gate deepops.c:5570 deep_op_drive(): `double cost = tune->attack_loss_gain * loss_now * SHIELD_WEIGHTING;` compared in 5600 `if (value - cost <= 0.0) go = FALSE`.
- **All call sites:**
  - deepdefence.c:6892 deep_assault_want — cost of victory subtracted from gross want (clamped at 0) — yes
  - deepdefence.c:6937 deep_assault_want — same in snap-up sweep — yes
  - deepops.c:3846 deep_muster_worth — `bar*eff - gain*loss*SHIELD_WEIGHTING > 0` — no: only called with op_muster on (formation of a muster, muster-abandon)
  - deepops.c:5570 deep_op_drive MUSTER — launch net-value cost (duel part) — yes (needs op_net_value=1, default)
  - deepops.c:5596 deep_op_drive — march-loss cost term — no: dead behind op_arrival_gate=0 / op_march_loss_rate=0
  - deepops.c:5731 deep_op_drive — cost= field of the march log line — yes (log only)
- **Log line:** deep-kill (loss=, gross=), deep-netval refuse (loss=, value=, cost=), deep-op march (loss=, cost=)
- **Interacts with:** SHIELD_WEIGHTING, attack_value_weight, conquest_worth_mult, op_net_value, op_take_target (via bar in deep_muster_worth), op_march_loss_rate
- **Tier/screen:** tier B — BASSAULT. Screen: tier B firings 2384/game (knob-screen-2026-08-30.md:172).
- **Prior evidence:**
  - M1 (STATUS 8538-8560, combat-e2-attrition-2026-08-06.md) — LOSS arm (attack_loss_gain 1.0 alone) vs OFF — contested score +1.27 (t +1.18), unitslost -4.41 (t -4.40), unitsbuilt -4.21; lategame-80 +1.14 (t +0.78), unitslost -0.06 — null on score, positive on losses — n=20 paired seeds per venue. STATUS 8676: "wants a magnitude sweep before anything else in MIL is tuned" (no sweep since).
  - S-WEAKPREY §5 (STATUS 22745) — names attack_loss_gain as a suspect for the exchange rate; not varied.

### conquest_worth_mult
- **Declared:** deeptuning.h:1491 (`double conquest_worth_mult;`); params.ini: default=2.0 min=1.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "What a conquest is worth, as a multiple of what the city is worth to us (human call 2026-08-06): taking it gains us its amortized value AND denies that value to the opponent... Applied ONCE at each of the two conquest seats... 1.0 restores the pre-rider behaviour."
- **Unit:** multiplier (dimensionless); applied to ai_city.worth (stock "want" units, an amortized city value) producing want units.
- **Formula at the primary call site:** deepops.c:289-290 deep_conquest_worth(): `double worth = def_ai_city_data(target, ait)->worth * tune->conquest_worth_mult * prey;` (prey = deep_weak_prey_price(), 1.0 at weak_prey_gate=0; culture term added after, deepops.c:294-308).
- **All call sites:**
  - deepops.c:290 deep_conquest_worth — the single arithmetic site; every caller below reads through it — yes
    - caller deepdefence.c:6725 deep_assault_want (tworth for build want) — yes
    - caller deepops.c:3829 deep_muster_worth (muster worth gate) — no (op_muster=0)
    - caller deepops.c:5568 deep_op_drive launch net-value (`eff_worth`) — yes
    - caller deepops.c:4756 deep_op_poach_defender (`take = worth * odds`) — yes
    - caller deepspy.c:219 deep_spy_action_value (incite value) — computed but result discarded at spy_enabled=0
- **Log line:** deep-kill (worth= stock read, eworth= after multiplier), deep-netval (worth=/eworth=), deep-op poach (worth=)
- **Interacts with:** weak_prey_gate/gain (prey factor inside), conquest_culture_gain (added to worth), defence_worth_mult (ratio is the tilt), attack_value_weight, attack_loss_gain, op_take_target
- **Tier/screen:** tier B — BASSAULT. Screen: tier B firings 2389/game (knob-screen-2026-08-30.md:169; highest in BASSAULT).
- **Prior evidence:**
  - M1 rider 2 (STATUS 9204) — lategame-80, mult 1.0 vs 2.0 (alone), n=20 paired — score -0.81 +/- 1.67 (t -0.48), netval refusals 8.00 -> 3.45 (t -2.40), launches +0.10 (t +0.16) — wash
  - M1 rider 3 (STATUS 9314) — defence_worth_mult 1.0 vs 2.0 at conquest 2.0 — see defence_worth_mult
  - S-WEAKPREY — rides in the same product (unchanged)

### defence_worth_mult
- **Declared:** deeptuning.h:1544 (`double defence_worth_mult;`); params.ini: default=2.0 min=1.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "keeping our own city is worth what we keep AND what we deny the taker, so the defence-side stakes read carries its own multiple. Default 2.0 -- symmetric with conquest_worth_mult... READ THE RATIO, NOT THE LEVELS."
- **Unit:** multiplier (dimensionless) on ai_city.worth (want units), then times (1 - p_survival) a probability: result is expected-loss in want units, compared against `worth x P(take)` (also want units).
- **Formula at the primary call site:** deepops.c:4719-4721 deep_op_poach_defender(): `double dmult = tune->defence_worth_mult; double raw_own = def_ai_city_data(pcity, ait)->worth; double city_risk = raw_own * dmult * (1.0 - p_survival);` compared at 4759 `if (tune->poach_marginal_gate <= 0 && city_risk <= take) continue;` with `take = worth * odds` (worth = deep_conquest_worth, odds = deep_take_odds_at_arrival).
- **All call sites:**
  - deepops.c:4719 deep_op_poach_defender — defence-side stakes of a counter-claim on a mustering op's blade — yes (reached when a threatened city asks to poach from an op in MUSTER state; kind DEEP_OPK_ASSAULT)
  - deepops.c:~4830 same function, marginal branch `gain = raw_own * dmult * (surv_with - relieved)` (poach_marginal_gate>0) — no: poach_marginal_gate=0 at defaults
- **Log line:** deep-op ... poach (worth=, dworth=, risk=, take=; deepops.c:4845), deep-poachmarg (marginal branch only)
- **Interacts with:** conquest_worth_mult (the ratio is the tilt), poach_marginal_gate, survival_horizon, deterrence_gain (STATUS 10799: "finale must sweep ... together")
- **Tier/screen:** unranked (legacy M, ALL, MIL). Note BASSAULT lists only its sibling.. Screen: not listed in the screen docs; knobcensus S2 "direct" via deep-op flag:poach.
- **Prior evidence:**
  - M1 rider 3 (STATUS 9314-9411) — lategame-80, tilt 2.0 vs 1.0 (defence 1.0 vs 2.0 at conquest 2.0), n=20 paired — score -0.13 +/- 0.47 (t -0.29); 8/20 games identical; mean take odds of the op outbid 0.03 vs 0.16 — wash
  - S-POACHFIX (STATUS 22168) rewrote the poach seat; knob not moved

### assault_pool_reach
- **Declared:** deeptuning.h:1551 (`int assault_pool_reach;`); params.ini: default=1 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The pool a marginal attacker is priced against reaches only as far as the marginal attacker itself does: a free blade that cannot be at the target by the same arrival turn is not part of the wave the build is joining (military audit C4). 0 restores the whole-continent pool."
- **Unit:** boolean gate (0/1). Inside it the comparison is in turns (integer ETA of a candidate blade vs the marginal blade's eta).
- **Formula at the primary call site:** deepdefence.c:6786-6799 deep_assault_want(): `if (tune->assault_pool_reach && deep_op_claiming(pplayer, punit->id) < 0) { ... uturns = (real_map_distance(...) * SINGLE_MOVE + urate - 1) / urate + (overseas ? tune->invasion_embark_turns : 0); if (uturns > eta) { n_far++; continue; } }`
- **All call sites:**
  - deepdefence.c:6786 deep_assault_want — drops free attackers that cannot arrive by eta from the pool — yes
- **Log line:** deep-kill (far= counts dropped blades; 0 on every line is the off arm)
- **Interacts with:** invasion_embark_turns, attack_value_weight, op_take_target
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed in the screen docs; knobcensus S2 "direct": deep-kill far=.
- **Prior evidence:**
  - M1 (STATUS 8464-8590) — part of the "ops pair"/ALL arm: removes 3/4 of the pool (lategame-80 9459 -> 2144 member-slots/game); decline rate 55% -> 82% — ALL vs OFF contested +1.75 (t +1.27), lategame -1.49 (t -0.84), n=20 — wash (not separable from op_launch_odds)

### sortie_win_floor
- **Declared:** deeptuning.h:1555 (`double sortie_win_floor;`); params.ini: default=0.85 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Kill sortie (E3): a garrison blade attacks an adjacent besieger when the duel odds clear this floor AND the city still clears survival_target without it. <= 0 disables."
- **Unit:** probability 0-1 (compared against max unit_win_chance over adjacent enemy defenders).
- **Formula at the primary call site:** deepdefence.c:2990-2993 deep_kill_sorties(): `if (best < tune->sortie_win_floor || (priced && best_score <= 0.0)) { deep_employ_sortie(... DES_FLOOR / DES_PRICE ...); continue; }` followed by the survival gate `(covered ? cover.surv_after : deep_city_stack_survival_without(...)) < tune->survival_target` (3016-3024).
- **All call sites:**
  - deepdefence.c:2879 deep_kill_sorties — off-switch (<=0 returns for the whole pass) — yes
  - deepdefence.c:2990,2993 deep_kill_sorties — win-chance floor — yes
  - deepemploy.h:117 — DES_FLOOR enum comment only (not a call site)
- **Log line:** deep-sortie (accepted sortie; kill=, victim=, p=, prize=, score=), deep-sortie-no (declines, priced mode only), deep-employsortie (funnel incl. DES_FLOOR)
- **Interacts with:** survival_target, survival_horizon, sortie_prize_gain, cover_removal
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 "direct": deep-sortie has:kill.
- **Prior evidence:**
  - none (STATUS 1124/1169 shipping notes only; M4b bundle varied sortie_prize_gain, not the floor)

### sortie_prize_gain
- **Declared:** deeptuning.h:1572 (`double sortie_prize_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "THE KILLSTACK PRIZE (M4b item 1)... Under killstack a won duel outside a city annihilates the whole tile, so the prize is the SUMMED shield value of the stack... the pass selects on p x prize x sortie_prize_gain - (1 - p) x our own build cost... 0 = off."
- **Unit:** multiplier (dimensionless) on `prize` (shields: sum of unit_build_shield_cost_base over the tile's stack). The score is therefore in shields (p x prize x gain - (1-p) x own cost, both terms shields).
- **Formula at the primary call site:** deepdefence.c:2972-2973 deep_kill_sorties(): `score = chance * prize * tune->sortie_prize_gain - (1.0 - chance) * unit_build_shield_cost_base(u);` ; gate `bool priced = (tune->sortie_prize_gain > 0.0);` (2888).
- **All call sites:**
  - deepdefence.c:2888 deep_kill_sorties — priced-mode switch — no: dead at 0 (best-chance selection, no value term)
  - deepdefence.c:2972 deep_kill_sorties — score formula — no: dead at 0
- **Log line:** deep-sortie-no (decline census, only logged when priced), deep-sortie (prize=, score= fields present either way)
- **Interacts with:** sortie_win_floor, survival_target, cover_removal
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2: deep-sortie-no logs only when gain>0 => dead-path at defaults.
- **Prior evidence:**
  - M4b (STATUS 10643-10700) — contested, BUNDLE of all six new gains at 1.0 vs OFF, n=20 paired — score -1.71 +/- 2.49 (t -0.68), sortieno +8.95 (t +4.40), sortie +2.40 (t +0.60) — null; gain not varied alone

### deterrence_gain
- **Declared:** deeptuning.h:1591 (`double deterrence_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "THE DETERRENCE DIVIDEND (M4b item 2)... the MARGINAL attrition this candidate defender adds -- deep_city_defence_read() with and without it, in shields -- times the observed attack pressure on the city (deep_garrison_menace()) times this gain... added, not multiplied [with defence_worth_mult]. 0 = off."
- **Unit:** UNSURE: header says marginal attrition is "in shields"; the product marginal x menace(0..1) x gain is added DIRECTLY to a defender adv_want (deepdefence.c:5774-5779, 5916-5920) with no SHIELD_WEIGHTING factor in the expression, so the gain implicitly carries want-per-shield (SHIELD_WEIGHTING = 17 is not applied here). Treated as a multiplier whose dimension is want/shield.
- **Formula at the primary call site:** deepdefence.c:5242 deep_deterrence_dividend(): `div = (adv_want) (marginal * menace * tune->deterrence_gain);` with `marginal = deep_marginal_attrition(nmap, pcity, ut, &now)`; caller 5772/5916: `uchoice.want += deep_deterrence_dividend(nmap, pcity, ..., deep_garrison_menace(pplayer))`.
- **All call sites:**
  - deepdefence.c:5235 deep_deterrence_dividend — off-switch (<=0 returns 0 before any DP run) — no: dead at 0
  - deepdefence.c:5242 deep_deterrence_dividend — dividend formula, added to the defender want in two callers (5772 birth-guard branch, 5916 ordinary defender want) — no: dead at 0
- **Log line:** deep-deter (turn, player, city, type, wave, marg, menace, surv, div)
- **Interacts with:** defence_worth_mult, survival_fear_value (STATUS 10799: sweep the three together), garrison_menace_slope
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2: returns 0 before any DP run when gain<=0 => dead-path at defaults.
- **Prior evidence:**
  - M4b (STATUS 10643-10700) — contested, BUNDLE of six gains at 1.0 vs OFF, n=20 paired — score -1.71 +/- 2.49 (t -0.68); deter lines +273.5/game (t +6.41), unitslost -2.00 +/- 1.44 (t -1.39) — null; gain not varied alone

### cover_removal
- **Declared:** deeptuning.h:1634 (`int cover_removal;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "HOLES GET COVERED (NETTHREAT item 4)... At 1 the removal is priced AFTER the cheapest single covering move: the empire-wide loss minimised over one-hop neighbours that can genuinely spare a blade... 0 = the local hole exactly."
- **Unit:** boolean gate (0/1). Inside it, costs are survival probabilities (0-1) compared with survival_target.
- **Formula at the primary call site:** deepdefence.c:3551 deep_cover_removal_cost(): `if (tune->cover_removal <= 0 || blade == NULL || out == NULL) return FALSE;` ; consumers read `covered ? cover.surv_after : deep_city_stack_survival_without(...)` against `tune->survival_target`.
- **All call sites:**
  - deepdefence.c:3551 deep_cover_removal_cost — master gate (returns FALSE, touching nothing) — no: dead at 0
    - consumer deepdefence.c:3016 deep_kill_sorties (home-hole gate) — no
    - consumer deepops.c:4157 deep_op_form garrison draft (both release tests) — no
    - consumer deepops.c:5353 deep_muster_abandon (trigger 3 "HOME") — no
- **Log line:** deep-cover (per priced removal that found a cover), deep-cover-sum (per-turn census)
- **Interacts with:** survival_target, survival_horizon, op_draft_marginal, op_muster_home, sortie_win_floor
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S3 "mode": deep-cover. Zero lines at defaults.
- **Prior evidence:**
  - none (NETTHREAT STATUS 11961-12075: "Both knobs stay 0... unmeasured on a venue that can see them"; byte-identity proof only; ERA 3b STATUS 13461 and pv3 STATUS 18997 pin it at 0)

### op_take_target
- **Declared:** deeptuning.h:1710 (`double op_take_target;`); params.ini: default=0.7 min=0.0 max=0.95 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Assault operations (E6): form when the free-attacker pool's take odds against the best met target clear op_take_target (<= 0 disables); muster at the staging city up to op_muster_wait turns before launching..."
- **Unit:** probability 0-1: compared against deep_take_odds_at_arrival() / deep_take_odds_cost() (P that the arriving stream takes the city). At formation it is the bar on the smallest strongest-first prefix of the pool; capped at 1.0 after the stance multiplier.
- **Formula at the primary call site:** deepops.c:3658-3662 deep_op_assault_bar(): `double bar = deep_tuning_get()->op_take_target; ... if (omult != 1.0 && !deep_op_target_threatens(pplayer, target)) { bar = MIN(1.0, bar * omult); }`. Formation, deepops.c:3988/4047: `bar = deep_op_assault_bar(pplayer, target, TRUE); ... if (odds >= bar) { required = i; break; }`. Launch, deepops.c:5483: `go = odds_now >= deep_op_assault_bar(pplayer, target, FALSE) && surv_now >= tune->op_hold_survivors;`
- **All call sites:**
  - deepops.c:3658 deep_op_assault_bar — the bar used at FORMATION (deep_op_form), LAUNCH (deep_op_drive 5483), muster worth gate and muster-abandon — yes
  - deepops.c:3768 deep_opodds_log — logs take_target= beside the stance-scaled bar= — yes (log only)
  - deepops.c:4535 deep_op_retarget — off-switch (<=0 or non-assault) — yes
  - deepops.c:4581 deep_op_retarget — `if (codds < tune->op_take_target) continue;` retarget candidate must clear the UNscaled bar — yes
  - deepops.c:6038 deep_run_operations — master switch: <=0 returns, whole operations layer off — yes
  - deepdefence.c:6815 deep_assault_want — `p1 < op_take_target` triggers campaign pricing (smallest force clearing the bar valued at the average rate) — yes
  - deepdefence.c:6839 deep_assault_want — `pk >= op_take_target` ends the campaign loop — yes
  - deepdefence.c:6899 deep_assault_want — `p1 >= op_take_target` triggers the snap-up type sweep — yes
  - deepdefence.c:6929 deep_assault_want — snap-up eligibility `pa < op_take_target => continue` — yes
- **Log line:** deep-opodds (verdict, odds=, bar=, take_target=, required=, pool=, drafted=, eta=, prey=), deep-employform, deep-kill, deep-op regate/retarget
- **Interacts with:** op_abort_floor (hysteresis below it), op_muster_wait, op_launch_odds, op_hold_survivors, op_net_value, survival_target (draft), invasion_reinforce_rate/slope/embark_turns (odds are taken at arrival), stance op mult (deep_stance_op_mult), attack_value_weight
- **Tier/screen:** tier A — DEFOPS (fit directly; measured flip rate). Screen: flips 4.9 decisions/game (496 decisions/game touched, 1.0% flip rate, rule op_formation; self-check 100.0% on 19 852 decisions) — knob-screen-2026-08-30.md:97. 84.5% of consulted refusals sit at odds exactly 0.000 (STATUS 11641: "Do not schedule op_take_target. No gradient exists").
- **Prior evidence:**
  - S-OPXRATE (STATUS 22759) — census of 640 assault ops, knob not moved: formed 640 -> march 501 -> strike 39 (6.1%) -> take 13 (2.0%); launch forecast P(take) 0.88-0.93 vs realized 4.4%
  - ERA 3a (STATUS 11641) — "Do not schedule op_take_target": no gradient while 84.5% of refusals are 0.000
  - era-tech50-forensics-2026-08-11.md:372 proposed 0.65; tuning-first-principles-2026-08-06.md:49 proposed 0.60 — proposals only, no isolated A/B
  - tune-postfix campaign (STATUS 22487) — mutated jointly with ~80 others; holdout rejected (tune-postfix-verdict-2026-08-25.md) — not attributable

### op_muster_wait
- **Declared:** deeptuning.h:1711 (`int op_muster_wait;`); params.ini: default=10 min=2 max=30 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "...muster at the staging city up to op_muster_wait turns before launching with whoever stands there (at least half the quorum) or dissolving." Also the march-wander and strike-patience clock (STATUS 1251: "march timeout reuses op_muster_wait").
- **Unit:** turns (compared against `game.info.turn - op->formed_turn / phase_turn / march_turn` differences).
- **Formula at the primary call site:** deepops.c:5657 deep_op_drive (patience release): `game.info.turn - op->formed_turn >= tune->op_muster_wait && eta_ok && present >= (op->required + 1) / 2 && ...`; wait cap 5750: `else if (!op->muster && game.info.turn - op->formed_turn >= tune->op_muster_wait)` -> retarget or dissolve "abort-wait"; march wander 6002: `game.info.turn - op->march_turn >= op->march_eta + tune->op_muster_wait`.
- **All call sites:**
  - deepops.c:4951 deep_op_drive_consolidate — `cleared || turn - op->phase_turn >= op_muster_wait`: CONSOLIDATE patience, then bank members — yes
  - deepops.c:5224 deep_op_drive_strike — walls held past patience => retarget/bank — yes
  - deepops.c:5657 deep_op_drive — half-quorum patience release (released 54.5% of launches per S-OPXRATE) — yes
  - deepops.c:5750 deep_op_drive — wait cap without half quorum => retarget or abort-wait — yes (non-muster ops only)
  - deepops.c:6002 deep_op_drive — march wander clock => retarget or abort-march — yes
  - deepops.c:5460 comment only
- **Log line:** deep-op (march waited=, abort-wait, abort-march, strike, garrison), deep-opodds
- **Interacts with:** op_take_target, op_launch_odds, op_patience_odds_floor, op_muster_stall (shipped equal by fiat), op_abort_floor, op_arrival_gate (eta_ok)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed in the screen docs; knobcensus S2 direct (deep-op abort-wait/abort-march/strike/garrison).
- **Prior evidence:**
  - none isolated (S-OPARRIVAL varied op_patience_odds_floor, not the wait; tune-postfix joint mutation rejected)

### op_draft_marginal
- **Declared:** deeptuning.h:1740 (`int op_draft_marginal;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "THE MARGINAL GARRISON DRAFT (ERA 3a)... replaces the floor with the comparison the decision actually is: release the blade when the survival it COSTS its city is less than the take-probability it BUYS the operation... 0 = the absolute survival_target floor."
- **Unit:** boolean gate (0/1). Inside: both sides are probabilities 0-1 (survival lost vs take-odds gained).
- **Formula at the primary call site:** deepops.c:3871 deep_op_form(): `bool marginal_draft = tune->op_draft_marginal > 0;` ; deepops.c:4053 `if (required == 0 && (marginal_draft || tune->survival_target > 0.0))`; release test deepops.c:4158ff `if (with - held > cost || (muster_feed && cost <= 0.0)) { n++; drafted++; }` (cost = survival(city) - survival_without(blade)).
- **All call sites:**
  - deepops.c:3871 deep_op_form — selects marginal release test (4158) and an extra draft-entry condition (4053; the draft itself IS live at defaults because survival_target=0.5>0, with the absolute floor) — no: marginal branch dead at 0
- **Log line:** deep-opodds (drafted=), deep-employform
- **Interacts with:** survival_target, survival_horizon, cover_removal, op_muster (muster_feed), op_draft_attack_floor, survival_pf_reach (3c)
- **Tier/screen:** unranked (legacy ALL, MIL only). Screen: not listed; knobcensus S3 "mode" (deep-opodds drafted>=1).
- **Prior evidence:**
  - ERA 3a (STATUS 11482) — tech50war-200 mixed, 16 seeds/arm, A (0) -293.4 vs A' (1) -241.1 per seed — paired +52.3, se 74, t +0.71, 5/16 — null; mechanism falsified
  - 3a x 3c factorial (STATUS 12426) — draft main +67.1 se 59 t +1.13, 7/16; interaction +14.8 se 23 — null; draft stays 0

### op_muster
- **Declared:** deeptuning.h:1773 (`int op_muster;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "MUSTER (ERA 3b)... 1 lets a slot whose target clears the WORTH gate enter MUSTER with the odds REFUSED, accumulate members across turns, and launch iff the odds AT THE CURRENT POOL clear the same bar... Requires op_launch_odds. 0 = the odds test is the formation gate."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepops.c:4259 deep_op_form(): `if (tune->op_muster <= 0 || tune->op_launch_odds <= 0 || n < 1 || !deep_muster_worth(...)) { ... return FALSE; }` ; deepops.c:3876 `bool muster_feed = tune->op_muster > 0;` (free-blade draft clause).
- **All call sites:**
  - deepops.c:3876 deep_op_form — muster_feed: draft also takes blades that cost their city nothing — no: dead at 0
  - deepops.c:4259,4264 deep_op_form — whether an odds-refused slot enters MUSTER (else verdict DEV_MUSTEROFF) — no: dead at 0
- **Log line:** deep-op muster / grow, deep-opodds (verdict=muster, muster-hold, muster-launch, muster-abandon), deep-employform (DEV_MUSTER/DEV_MUSTEROFF)
- **Interacts with:** op_launch_odds (required), op_muster_stall, op_muster_home, op_net_value (worth gate), op_take_target, cover_removal
- **Tier/screen:** unranked (legacy ALL, MIL only). Screen: not listed; knobcensus S3 "mode" (deep-opodds muster>=1). Zero lines at defaults.
- **Prior evidence:**
  - MUSTER A/B 2026-08-13 (STATUS 14389) — tech50war-200 mixed, 16 mirrored seeds: B muster as shipped -34.8 vs control -22.7 (-12.1, t -1.98); B' (op_muster_home=0) -29.0 (-6.3, t -0.85); launched 24 of 2788 formed — negative as shipped / null without home trigger
  - pv3 A/B (STATUS 18984-19200) — 40 games seeds 17-36, op_muster=1 + op_muster_home=0 vs off — score T200 -1.6 +/- 15.2 (t -0.11); 0.28% of ops ever gain a member — null; "op_muster stays 0"

### op_muster_stall
- **Declared:** deeptuning.h:1788 (`int op_muster_stall;`); params.ini: default=10 min=1 max=40 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "How many turns a muster may go without GROWING before it is abandoned... PLACEHOLDER, untuned... Shipped equal to op_muster_wait... Only read when op_muster is on."
- **Unit:** turns (compared with `game.info.turn - op->muster_grew`).
- **Formula at the primary call site:** deepops.c:5333 deep_muster_abandon(): `if (game.info.turn - op->muster_grew >= tune->op_muster_stall) { return "muster-stall"; }`
- **All call sites:**
  - deepops.c:5333 deep_muster_abandon — stall clock; reached only for ops with op->muster set — no: dead behind op_muster=0
- **Log line:** deep-opodds / deep-op (why=muster-stall at dissolve)
- **Interacts with:** op_muster, op_muster_wait, op_muster_home
- **Tier/screen:** unranked (legacy ALL, MIL only). Screen: not listed; knobcensus S3 "mode" (why=muster-stall).
- **Prior evidence:**
  - ERA 3b / MUSTER A/B (STATUS 13465, 18996) — left at compiled placeholder in every arm (2 stalls vs 155 home abandons of 191, STATUS 13541) — not varied

### op_muster_home
- **Declared:** deeptuning.h:1812 (`int op_muster_home;`); params.ini: default=1 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The muster's HOME-DEFENCE abandon trigger, separably... 1 = the trigger as designed and as measured. 0 = the muster answers to the stall clock and the worth gate only. Only read when op_muster is on."
- **Unit:** boolean gate (0/1). Inside, compares survival probability after one-hop cover vs survival_target.
- **Formula at the primary call site:** deepops.c:5342 deep_muster_abandon(): `if (!tune->op_muster_home || tune->survival_target <= 0.0 || present < 1) { return NULL; }` then `surv_after < tune->survival_target => "muster-home"`.
- **All call sites:**
  - deepops.c:5342 deep_muster_abandon — enables the HOME abandon trigger; reached only for musters — no: dead behind op_muster=0 (default value 1 is inert until op_muster=1)
- **Log line:** deep-opodds (why=muster-home)
- **Interacts with:** op_muster, cover_removal, survival_target, survival_horizon
- **Tier/screen:** unranked (legacy ALL, MIL only). Screen: not listed; knobcensus S3 "mode".
- **Prior evidence:**
  - MUSTER A/B (STATUS 14389) — B (home on) -34.8 vs B' (home off) -29.0 vs control -22.7, 16 seeds — home trigger = churn engine (abandons 1208 -> 396), not launch blocker — null
  - pv3 A/B (STATUS 19012) — behavioural proof only: home=1 8 muster-home abandons, home=0 0

### op_launch_odds
- **Declared:** deeptuning.h:1820 (`int op_launch_odds;`); params.ini: default=1 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Launch on odds, not on a head count (combat E2): a mustered wave is released when the force STANDING at the staging city clears the same bar formation used, re-priced against the garrison as it is now... 0 restores the head count. The wait cap keeps its half-quorum count either way."
- **Unit:** boolean gate (0/1). It selects the launch currency: probability bar (odds_now >= bar) vs head count (present >= op->required).
- **Formula at the primary call site:** deepops.c:5470 deep_op_drive MUSTER: `if (tune->op_launch_odds && present > 0) { ... odds_now = deep_take_odds_cost(nmap, target, here, present, eta_here, &loss_now, &surv_now); go = odds_now >= deep_op_assault_bar(pplayer, target, FALSE) && surv_now >= tune->op_hold_survivors;` else (5619) `go = !op->muster && present >= op->required;`
- **All call sites:**
  - deepops.c:4259,4265 deep_op_form — required for a muster to form — no: only meaningful with op_muster=1
  - deepops.c:5470 deep_op_drive — launch on odds — yes
  - deepops.c:5664 deep_op_drive — patience-floor term `!tune->op_launch_odds` — inert at op_patience_odds_floor=0
  - deepops.c:5729 deep_op_drive — bar= field of the march line — yes (log only)
- **Log line:** deep-op march (bar=1 odds launch; bar=0 head count; odds=, surv=)
- **Interacts with:** op_take_target, op_hold_survivors, op_net_value, op_patience_odds_floor, op_muster
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 direct (deep-op march bar=).
- **Prior evidence:**
  - M1 (STATUS 8464-8590) — "ops pair" (op_launch_odds + assault_pool_reach) inside the ALL arm: launches contested 4.45 -> 2.60, lategame-80 4.85 -> 6.35; ops pair alone on lategame-80 cost 2.63 (t -1.54) — wash; "launch bar's sign is venue-dependent"

### op_net_value
- **Declared:** deeptuning.h:1827 (`int op_net_value;`); params.ini: default=1 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The op-level go/no-go asks the TOTAL question the marginal build want cannot (human call 2026-08-06): worth x P(take) minus the whole assault's expected losses, converted at SHIELD_WEIGHTING through attack_loss_gain... 0 restores the bar alone."
- **Unit:** boolean gate (0/1). Inside: value and cost are in want units (eff_worth x odds vs gain x shields x 17).
- **Formula at the primary call site:** deepops.c:5562-5602 deep_op_drive: `if (go && tune->op_net_value) { double value = arrive_now * odds_now * eff_worth; double cost = tune->attack_loss_gain * loss_now * SHIELD_WEIGHTING; ... if (value - cost <= 0.0) { go = FALSE; } }`
- **All call sites:**
  - deepops.c:5562 deep_op_drive — launch net-value veto of a bar-clearing wave — yes
  - deepops.c:3836 deep_muster_worth — if off, the muster worth gate degenerates to "worth > 0" — no: only reached with op_muster on
- **Log line:** deep-netval refuse (target, force, odds, worth, eworth, loss, value, cost), deep-netval etacap
- **Interacts with:** attack_loss_gain, conquest_worth_mult, op_arrival_gate terms (arrive_now, march loss), op_take_target
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 gate (deep-netval refuse; zero lines = off arm).
- **Prior evidence:**
  - M1 addendum (STATUS 8875) — lategame-80, 20 paired seeds, op_net_value off vs on — score +1.16 +/- 0.79 (t +1.47, 8/20), netval refusals 0 -> 7.65 (t +5.16), launches -1.25 (t -2.63) — null (ships at 1 on human call); contested not re-measured

### op_hold_survivors
- **Declared:** deeptuning.h:1832 (`double op_hold_survivors;`); params.ini: default=1.0 min=0.0 max=6.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "how much of the wave must be expected to SURVIVE taking the city, from the substrate's attacker-loss output... In units, expected. 0 disables the hold test."
- **Unit:** count (real-valued expected number of surviving attackers; compared against surv_now from deep_take_odds_cost).
- **Formula at the primary call site:** deepops.c:5483-5484 deep_op_drive: `go = odds_now >= deep_op_assault_bar(pplayer, target, FALSE) && surv_now >= tune->op_hold_survivors;`
- **All call sites:**
  - deepops.c:5484 deep_op_drive — survivor floor at the odds launch — yes (needs op_launch_odds=1)
- **Log line:** deep-op march (surv=)
- **Interacts with:** op_launch_odds, op_take_target
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 direct (deep-op march surv=).
- **Prior evidence:**
  - none (STATUS 8656 "PLACEHOLDER... never exercised against an alternative value")

### invasion_reinforce_rate
- **Declared:** deeptuning.h:1864 (`double invasion_reinforce_rate;`); params.ini: default=0.34 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Naval slice 3: virtual defenders per turn of sail time added to a target's garrison when pricing an amphibious take -- travel time is the defender's budget. 0 prices arrival like adjacency."
- **Unit:** count per turn (virtual defenders per turn of eta). Rounding: deepcombat.c:1600 does `reinforce = (int) (eta * deep_reinforce_rate(nmap, acity) + 0.5)`, so at 0.34 eta 1 adds 0 defenders, eta 2 adds 1.
- **Formula at the primary call site:** deepcombat.c:1521,1524 deep_reinforce_rate(): barbarian targets return barb_reinforce_rate; slope<=0 returns `tune->invasion_reinforce_rate`; else `rate + slope x deep_reinforce_observed()`. Consumer deepcombat.c:1600 (deep_take_odds_cost): virtual best-buildable defenders placed in the city.
- **All call sites:**
  - deepcombat.c:1521 deep_reinforce_rate — flat civilised-target rate — yes
  - deepcombat.c:1524 deep_reinforce_rate — sum with slope term — no: dead at slope 0
    - (via the function) deepcombat.c:1600 deep_take_odds_cost — odds at arrival, used by formation, launch, re-gate, deep_assault_want — yes
    - deepdefence.c:7048 deep-kill rrate= log
- **Log line:** deep-kill (rrate=, eta>=1 population), deep-invade (p=), deep-opodds
- **Interacts with:** invasion_reinforce_slope, barb_reinforce_rate, invasion_embark_turns, op_take_target, op_abort_floor, op_regate_eta_freeze
- **Tier/screen:** tier B — BASSAULT. Screen: tier B firings 2387/game (knob-screen-2026-08-30.md:170); STATUS 12835 "LIVE, population 8 307" on tech65sea-150.
- **Prior evidence:**
  - STATUS 12846 — "still a PLACEHOLDER 0.34 flat rate"; era-tech50-forensics-2026-08-11.md:374 / tuning-first-principles-2026-08-06.md:50 proposed 0.20 — proposals; no A/B isolating it

### invasion_reinforce_slope
- **Declared:** deeptuning.h:1886 (`double invasion_reinforce_slope;`); params.ini: default=0.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "INVASION-RATE: the SLOPE on the same law... the model charges rate + slope x [defenders/turn = surplus[O_SHIELD] / cost(best buildable defender)]. 0 = the flat model exactly... Civilised targets only."
- **Unit:** multiplier (dimensionless) on deep_reinforce_observed() (defenders per turn), so the sum with invasion_reinforce_rate stays in count/turn.
- **Formula at the primary call site:** deepcombat.c:1524-1526 deep_reinforce_rate(): `return tune->invasion_reinforce_rate + tune->invasion_reinforce_slope * deep_reinforce_observed(nmap, acity);`
- **All call sites:**
  - deepcombat.c:1518 deep_reinforce_rate — `if (invasion_reinforce_slope <= 0.0) return flat` — no: dead at 0
  - deepcombat.c:1525 deep_reinforce_rate — slope term — no: dead at 0
- **Log line:** deep-kill (robs= is the observed rate this multiplies; robs>0 population)
- **Interacts with:** invasion_reinforce_rate, barb_reinforce_rate
- **Tier/screen:** tier B — BASSAULT. Screen: tier B firings 2348/game (knob-screen-2026-08-30.md:173) — counts the instrument firing; the multiplier is 0 at defaults so no decision can change (inferred).
- **Prior evidence:**
  - none (STATUS has no A/B)

### invasion_embark_turns
- **Declared:** deeptuning.h:1900 (`int invasion_embark_turns;`); params.ini: default=2 min=0 max=6 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the fixed turns an overseas eta pays on top of the sail itself -- board the hull at this end, come ashore at the other. Charged by the four seams that price an OVERSEAS arrival... NOT charged by deep_op_eta()."
- **Unit:** turns (integer added to an eta computed in turns).
- **Formula at the primary call site:** deepops.c:4034-4036 deep_op_form(): `eta = (real_map_distance(...) * SINGLE_MOVE + rate - 1) / rate + (overseas ? tune->invasion_embark_turns : 0);` (same pattern deepdefence.c:6748, 6794, 6923).
- **All call sites:**
  - deepops.c:4036 deep_op_form — required-force pricing eta — yes (overseas targets only)
  - deepdefence.c:6748 deep_assault_want — main eta — yes (overseas only)
  - deepdefence.c:6794 deep_assault_want — pool-reach uturns — yes (overseas only; requires assault_pool_reach)
  - deepdefence.c:6923 deep_assault_want — snap-up sweep eta2 — yes (overseas only)
- **Log line:** deep-kill (sea=1 population), deep-opodds (eta=, overseas)
- **Interacts with:** invasion_reinforce_rate (eta x rate), assault_pool_reach, op_take_target
- **Tier/screen:** tier B — BASSAULT. Screen: tier B firings 641/game (knob-screen-2026-08-30.md:171).
- **Prior evidence:**
  - none (STATUS: "2 is what those seams shipped as a bare literal"; registered at identity)

### op_wave_window
- **Declared:** deeptuning.h:1905 (`int op_wave_window;`); params.ini: default=6 min=0 max=20 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Second waves (E7): for this many turns after a strike launches, the reserve gets first claim on the next operation against the same target."
- **Unit:** turns (compared with `game.info.turn - dp->last_strike_turn`).
- **Formula at the primary call site:** deepops.c:3929-3930 deep_op_form(): `if (dp->last_strike_city > 0 && game.info.turn - dp->last_strike_turn <= tune->op_wave_window) { prev = game_city_by_number(...); ... target = prev; }`
- **All call sites:**
  - deepops.c:3930 deep_op_form — second-wave claim window — yes (only after a STRIKE: 39 strikes in 640 ops per S-OPXRATE)
- **Log line:** deep-op form (wave2= field)
- **Interacts with:** op_take_target (re-priced at the full bar), op_muster_wait
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 direct (deep-op wave2>0).
- **Prior evidence:**
  - B8 session close (STATUS 1222) — E7 and E8 landed together: 211 second-wave formations / 534 raid missions per 10 games, losses 11 -> 23, score median 0.775 vs B6 0.811 — not isolated

### raid_radius
- **Declared:** deeptuning.h:1909 (`int raid_radius;`); params.ini: default=12 min=0 max=30 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Raids (E8): with no assault operation standing, idle fast attackers hunt exposed enemy civilians within this many tiles. 0 disables."
- **Unit:** tiles (real_map_distance, `d < radius + 1`); in the pillage census it is a square_iterate radius.
- **Formula at the primary call site:** deepops.c:3357-3380 deep_raid_victim(): `int bestd = radius + 1; ... d = real_map_distance(unit_tile(u), unit_tile(v)); if (d < bestd) {...}` called with `tune->raid_radius`.
- **All call sites:**
  - deepops.c:3416 deep_op_enroll_raiders — raider eligibility needs a victim within radius — yes
  - deepops.c:3445 deep_op_form_raid — off-switch (<=0) — yes
  - deepops.c:3494,3509 deep_op_drive_raid — keep hunting / pick victim — yes
  - deepdefence.c:4236-4237 deep_pillage_sorties — census of pillage-eligible enemy tiles within radius (diagnostic `nearby`) — yes (census only)
- **Log line:** deep-op raid (members=), deep-raid (raider, victim, dist), deep-pillage (nearby=)
- **Interacts with:** pillage_denial_gain (pillage side), op_take_target (assault formation runs first)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 direct (deep-raid, deep-op raid). tuning-first-principles-2026-08-06.md:28 — gradient 0.000 on a6 cells.
- **Prior evidence:**
  - B8/B10 (STATUS 1222, 1257) — landed with op_wave_window: 369 raid formations / 489 hunts per 10 games; "assault forms fell 114 -> 40"; kills-side metric absent — not isolated

### op_abort_floor
- **Declared:** deeptuning.h:1943 (`double op_abort_floor;`); params.ini: default=0.35 min=0.0 max=0.9 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "March re-gate (W33 commit 2): a marching wave re-prices the take each turn at the remaining eta and turns back -- banking every member -- when the odds fall below this floor. Set below op_take_target for hysteresis. <= 0 disables the re-gate."
- **Unit:** probability 0-1 (compared with deep_take_odds / deep_take_odds_at_arrival).
- **Formula at the primary call site:** deepops.c:5058 deep_op_drive_strike: `double odds = deep_take_odds(nmap, target, stream, ns); if (odds < tune->op_abort_floor) {retarget or bank}` (eta 0, before the first blow); deepops.c:5941-5958 deep_op_drive MARCH: `double floor_eff = tune->op_abort_floor; ... odds = deep_take_odds_at_arrival(nmap, target, stream, ns, price_eta); if (odds < floor_eff) { ... "abort-odds" }`
- **All call sites:**
  - deepops.c:5055,5058 deep_op_drive_strike — deliberate abort before first blow — yes
  - deepops.c:5895 deep_op_drive — enables the march re-gate (<=0 => none) — yes
  - deepops.c:5941 deep_op_drive — floor for the march re-gate; scaled down by op_regate_sunk_gain when op_regate_gate=1 — yes (nominal at defaults)
- **Log line:** deep-op regate (odds, eta, members, def_n, def_best, marched, peta, floor), deep-op abort-odds/abort-strike
- **Interacts with:** op_take_target, op_regate_gate, op_regate_sunk_gain, op_regate_eta_freeze, op_patience_odds_floor (structurally set equal to it in S-OPARRIVAL), invasion_reinforce_rate
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 direct (abort-strike/abort-odds). abort-odds = 302 of 640 terminal causes (S-OPXRATE).
- **Prior evidence:**
  - S-OPARRIVAL — floor itself not moved; only the re-gate memory terms were (see op_regate_*)
  - era-tech50-forensics-2026-08-11.md:373 — "72 of 83 operations died here"; principled-v1 proposed 0.3 — proposal, no isolated A/B

### spy_enabled
- **Declared:** deeptuning.h:2071 (`int spy_enabled;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Spy-action valuation (W35 E35.1, deepspy.c). spy_enabled is the off-switch: 0 (the compiled default) leaves stock's fixed ladder deciding what a diplomat does and runs deep's seam census only, so a default game is byte-identical."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepspy.c:390 deep_spy_manage(): `bool on = (tune->spy_enabled > 0);` then (458) `if (!on) { continue; /* Census only: the game is unchanged. */ }` after pricing.
- **All call sites:**
  - deepspy.c:390 deep_spy_manage — acts on deep's own action choice only if on — no: dead at 0 (census and pricing still run, result discarded)
  - deepspy.c:354 deep_spy_log — prints on= — log only
- **Log line:** deep-spy (census; on=, seen, at_seam, abstained, refused, steal, incite, sabotage), deep-spy-act (only when on)
- **Interacts with:** spy_gold_value, spy_sabotage_gain, embassy_science_value, deep_conquest_worth (incite)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed individually; knobcensus S2 "mode": deep-spy-act. knob-screen-2026-08-30.md names "spy (3 knobs)" among the compiled-off dead-path families.
- **Prior evidence:**
  - M4a (STATUS 10916-10990) — smoke, 10 games, spy_enabled=1: seen 4304, at_seam 76 (1.8%), steal 51, embassy 0, incite 0, sabotage 0 — census only, no score A/B

### spy_gold_value
- **Declared:** deeptuning.h:2072 (`double spy_gold_value;`); params.ini: default=0.5 min=0.0 max=5.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "spy_gold_value is one gold in the want currency, and it is the exchange rate three of the four families ride on -- a stolen tech's bulb cost at stock's own x3, and the incite cost the server charges."
- **Unit:** want per gold (exchange rate): `gold * spy_gold_value`; result in want units. (The steal-tech call passes 3.0 x bulbs as "gold".)
- **Formula at the primary call site:** deepspy.c:98 deep_spy_gold_worth(): `return gold * deep_tuning_get()->spy_gold_value;` ; steal-tech value `deep_spy_gold_worth(3.0 * (double)bulbs)` (deepspy.c:140); incite `value = deep_conquest_worth(...) - deep_spy_gold_worth((double)cost)` (deepspy.c:219-220).
- **All call sites:**
  - deepspy.c:98 deep_spy_gold_worth (callers deepspy.c:140 steal, 220 incite) — priced whenever a diplomat stands at an action seat, but the choice is acted on only when spy_enabled=1 — no: consequence-free at spy_enabled=0
- **Log line:** deep-spy-act (want=, kind=steal / incite)
- **Interacts with:** spy_enabled, embassy_science_value, conquest_worth_mult (incite)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 direct (deep-spy-act kind=steal); dead-path family at defaults.
- **Prior evidence:**
  - none

### spy_sabotage_gain
- **Declared:** deeptuning.h:2073 (`double spy_sabotage_gain;`); params.ini: default=0.5 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "spy_sabotage_gain prices shields DESTROYED against shields produced: denial is not acquisition, and the two are not worth the same. Both PLACEHOLDER."
- **Unit:** UNSURE: multiplier on a shield quantity (target->shield_stock or impr_build_shield_cost); the product competes in the same value ranking as want-denominated steal/incite values but no SHIELD_WEIGHTING factor appears in the expression, so the gain implicitly carries want per shield.
- **Formula at the primary call site:** deepspy.c:227 deep_spy_action_value(): `value = tune->spy_sabotage_gain * (double)target->shield_stock;` and (236) `value = tune->spy_sabotage_gain * (double)impr_build_shield_cost(target, pimprove);`
- **All call sites:**
  - deepspy.c:227 deep_spy_action_value — sabotage city production — no: result discarded at spy_enabled=0
  - deepspy.c:236 deep_spy_action_value — targeted sabotage of a building — no: same
- **Log line:** deep-spy-act (kind=sabotage; want=)
- **Interacts with:** spy_enabled, spy_gold_value
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 (deep-spy-act kind=sabotage); dead-path family at defaults.
- **Prior evidence:**
  - none (sabotage 0 on smoke: needs war + adjacent diplomat)

### leader_check_gain
- **Declared:** deeptuning.h:2685 (`double leader_check_gain;`); params.ini: default=0.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "deep's operations layer biases its target choice toward the runaway leader's cities by this fraction. 0.0 = off = today's target ordering byte-identically."
- **Unit:** fraction (dimensionless multiplier increment): `score *= 1.0 + gain` on the target score worth/(1+distance).
- **Formula at the primary call site:** deepops.c:3181-3184 deep_op_pick_target(): `if (deep_tuning_get()->leader_check_gain > 0.0 && deep_player_is_leader(ait, pplayer, aplayer)) { score *= 1.0 + deep_tuning_get()->leader_check_gain; deep_plr_data(pplayer)->leader_bias_applied++; }`
- **All call sites:**
  - deepops.c:3181,3183 deep_op_pick_target — leader bias in the assault target ranking only (NOT in deep_assault_want's own ranking loop) — no: dead at 0
- **Log line:** deep-leaders (applied= is leader_bias_applied)
- **Interacts with:** chatty_inbound_gate, weak_prey_gate (other score multipliers in the same loop)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 direct (deep-leaders applied>0). dead-path at defaults.
- **Prior evidence:**
  - none (landed in PEACE, peace-diplomacy-2026-08-06.md:158; no A/B)

### conquest_culture_gain
- **Declared:** deeptuning.h:1524 (`double conquest_culture_gain;`); params.ini: default=0.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the culture stock a conquest carries across... history/50 score points, each priced at what deep already pays for the citizen that scores exactly one point... 1.0 therefore means 'the transferred stock is worth what its score is worth'... 0.0 = off = byte-identical."
- **Unit:** multiplier (dimensionless) on `history / DEEP_CULTURE_PER_SCORE * deep_unworked_tile_value(target, adv, 3, NULL)`; the product is want units (score points x want per citizen-point).
- **Formula at the primary call site:** deepops.c:302-304 deep_conquest_worth(): `culture = (double) target->history / DEEP_CULTURE_PER_SCORE * deep_unworked_tile_value(target, adv, 3, NULL) * tune->conquest_culture_gain * prey;` returned as `worth + culture`.
- **All call sites:**
  - deepops.c:294 deep_conquest_worth — off-switch (gain<=0 or no history) — no: dead at 0
  - deepops.c:303 deep_conquest_worth — culture term — no: dead at 0 (would reach every deep_conquest_worth caller)
- **Log line:** deep-conqculture (priced=, paid=)
- **Interacts with:** conquest_worth_mult, weak_prey_gate/gain (prey), horizon_culture_weight (same citizen price chain)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; knobcensus S2 direct (deep-conqculture; logs only if gain>0). dead-path at defaults.
- **Prior evidence:**
  - BALANCE section 4 (STATUS 9872-9884) — conquest culture priced at face value: contested +0.76 (t +0.90, 9/20), lategame-80 +0.15 (t +0.31, 4/20), n=20 paired each; fired on 37 663 / 12 076 valuations — null; off-switch 10/10 byte-identical

### weak_prey_gate
- **Declared:** deeptuning.h:3387 (`int weak_prey_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "THE WEAK-PREY BIAS (deep_weak_prey_mult())... every conquest valuation is multiplied by 1 + weak_prey_gain x MAX(0, 1 - their score.units / ours) at three seats... weak_prey_gate: 0 (default) = the multiply is SKIPPED, not done."
- **Unit:** boolean gate (0/1; range 0..1).
- **Formula at the primary call site:** deepops.c:156-160 deep_weak_prey_mult(): `if (tune->weak_prey_gate == 0 || tune->weak_prey_gain <= 0.0 || victim == NULL || victim == pplayer || pplayers_allied(pplayer, victim)) { return 1.0; }`
- **All call sites:**
  - deepops.c:156 deep_weak_prey_mult — master gate (callers: deep_weak_prey_price via deep_conquest_worth deepops.c:288; deep_op_pick_target deepops.c:3158; deep_assault_want deepdefence.c:6663; deep-opodds log deepops.c:3793) — no: dead at 0
- **Log line:** deep-weakprey (priced=, mean=, ours=), prey= / preyunits= on deep-opodds
- **Interacts with:** weak_prey_gain, conquest_worth_mult, conquest_culture_gain
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: "unmapped" in knob-screen-2026-08-30.md:134 and screen-corpus-2026-08-30.md:212 (landed after knobcensus.py's map). Dead at defaults.
- **Prior evidence:**
  - S-WEAKPREY (STATUS 22613) — contested, 24 TRAINING seeds, gate 0 vs 1 at gain 1.0, arm deep — score -2.01 +/- 1.20 (t -1.68, 8/24), pop -1.36 (t -3.07), unitslost +3.20 (t +2.96), unitskilled +1.58 (t +1.60), op-formed +3.33 (t +2.53) — negative (weak); gate stays 0. Identity: 20/20 smoke + 4/4 war byte-identical.

### weak_prey_gain
- **Declared:** deeptuning.h:3388 (`double weak_prey_gain;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "PLACEHOLDER 1.0, flagged in STATUS.md for a tuning pass. 1.0 is the unit case rather than a guess about size -- it makes a wholly disarmed rival's city worth exactly TWICE a peer's... 0.0 disables the term as completely as the gate does."
- **Unit:** multiplier (dimensionless): ratio of two score.units unit COUNTS is dimensionless; factor in [1, 1+gain].
- **Formula at the primary call site:** deepops.c:176-178 deep_weak_prey_mult(): `ratio = (double) victim->score.units / MAX(pplayer->score.units, 1); ... mult = 1.0 + tune->weak_prey_gain * (1.0 - ratio);` (ratio>=1 => 1.0; our units <=0 => 1.0).
- **All call sites:**
  - deepops.c:156 deep_weak_prey_mult — `weak_prey_gain <= 0.0` disables — no: dead behind weak_prey_gate=0
  - deepops.c:178 deep_weak_prey_mult — factor — no: dead behind weak_prey_gate=0
- **Log line:** deep-weakprey, prey= on deep-opodds
- **Interacts with:** weak_prey_gate, conquest_worth_mult
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: "unmapped" (see weak_prey_gate). Dead at defaults.
- **Prior evidence:**
  - S-WEAKPREY (above) — tested only at 1.0 (via the gate); "(c) only then a weak_prey_gain sweep" (STATUS 22757); a smaller gain is "the interesting direction" — never swept

### op_patience_odds_floor
- **Declared:** deeptuning.h:3423 (`double op_patience_odds_floor;`); params.ini: default=0.0 min=0.0 max=0.6 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The odds the HALF-QUORUM PATIENCE RELEASE must still clear... 273 of 501 launches (54.5%) came through it at mean predicted odds 0.08-0.15, and took a city 1.1% of the time against the bar-clearing population's 4.4%... At > 0 the release must ALSO clear this odds floor... 0.0 = today's behaviour exactly."
- **Unit:** probability 0-1 (compared with odds_now from deep_take_odds_cost at the staging tile).
- **Formula at the primary call site:** deepops.c:5657-5665 deep_op_drive: `patience = (!op->muster && turn - op->formed_turn >= tune->op_muster_wait && eta_ok && present >= (op->required + 1) / 2 && (tune->op_patience_odds_floor <= 0.0 || !tune->op_launch_odds || odds_now >= tune->op_patience_odds_floor));`
- **All call sites:**
  - deepops.c:5663,5665 deep_op_drive — floor on the half-quorum patience release — no at 0: `<= 0.0` short-circuits to the shipped expression
- **Log line:** deep-op march (patience=1 marks launches via this path; odds=)
- **Interacts with:** op_muster_wait, op_launch_odds, op_abort_floor (S-OPARRIVAL set the tested value 0.35 equal to it), op_arrival_gate (eta_ok)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed in the screen docs (landed after the instrument map); dead-path at defaults by construction (gate value 0.0).
- **Prior evidence:**
  - S-OPARRIVAL (findings/op-arrival-verdict-2026-08-29.md §4) — contested, 16 TRAINING seeds, cell 010 (P=0.35) vs control 83.90 — score +0.14 +/- 0.65 (t +0.22, 7/16); launches -1.88 +/- 0.55 (t -3.38), patience releases 2.94 -> 1.00 (t -4.48), takes +0.06 +/- 0.11 — null on score; "the only gate whose funnel improves without a bill"; cell 011 (P+R) +0.02 +/- 0.65

### op_arrival_gate
- **Declared:** deeptuning.h:3432 (`int op_arrival_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "PRICING THE JOURNEY. One gate over the three terms [op_arrival_decay, op_launch_eta_cap, op_march_loss_rate] that make the launch decision ask P(this operation ends holding that city) instead of P(this stream beats that garrison, arriving together and fresh). 0 = not one of them is computed and the binary is the shipped one."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepops.c:5520 deep_op_drive MUSTER launch: `if (tune->op_arrival_gate) { int e; launch_eta = MAX(deep_op_eta(pplayer, op, target), 1); if (tune->op_launch_eta_cap > 0 && launch_eta > tune->op_launch_eta_cap) { go = FALSE; eta_ok = FALSE; } for (e = 0; e < launch_eta && tune->op_arrival_decay < 1.0; e++) arrive_now *= tune->op_arrival_decay; }`
- **All call sites:**
  - deepops.c:5520 deep_op_drive — enables the eta cap + decay computation — no: dead at 0
  - deepops.c:5587 deep_op_drive — enables the march-loss cost term — no: dead at 0
- **Log line:** deep-op march (arrive=), deep-netval etacap (cap refusals)
- **Interacts with:** op_arrival_decay, op_launch_eta_cap, op_march_loss_rate, op_net_value, op_patience_odds_floor (eta_ok), op_contact_quorum (deep_op_eta k)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed in the screen docs; dead-path at defaults (gate 0).
- **Prior evidence:**
  - S-OPARRIVAL (op-arrival-verdict-2026-08-29.md §4) — contested, 16 TRAINING seeds, cell 100 (A: decay 0.447, eta cap 4, march loss 0.56) vs control — score -0.09 +/- 0.56 (t -0.15, 5/16); takes 0.44 -> 0.12 (-0.31 +/- 0.12, t -2.61); in cells carrying A the patience release was 100% of launches — null on score, negative on takes; "ships at 0"; A main effect +0.03

### op_arrival_decay
- **Declared:** deeptuning.h:3439 (`double op_arrival_decay;`); params.ini: default=1.0 min=0.3 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "P(the march survives one more turn), compounded over the quorum's eta and folded into the net-value product: the gate compares P(arrive) x P(win) x worth against cost instead of P(win) x worth... 1.0 = no decay."
- **Unit:** probability 0-1 per turn of eta (per-turn march survival), compounded multiplicatively: arrive_now = decay^launch_eta (computed as a product loop, not pow()).
- **Formula at the primary call site:** deepops.c:5535-5537 deep_op_drive: `for (e = 0; e < launch_eta && tune->op_arrival_decay < 1.0; e++) { arrive_now *= tune->op_arrival_decay; }` then 5594 `value = arrive_now * odds_now * eff_worth`.
- **All call sites:**
  - deepops.c:5535,5536 deep_op_drive — arrive_now factor in the launch net-value product — no: dead behind op_arrival_gate=0 (and decay 1.0 is identity)
- **Log line:** deep-op march (arrive=)
- **Interacts with:** op_arrival_gate, op_net_value, op_launch_eta_cap, attack_loss_gain
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; dead-path at defaults.
- **Prior evidence:**
  - S-OPARRIVAL §4/§7 — tested only inside the A-bundle at 0.447 (derived: 0.078 = d^3.17); "decay crushes the net-value product so completely that go is never true" — see op_arrival_gate; "a gentler decay is a different experiment and is not evidence" — not isolated

### op_launch_eta_cap
- **Declared:** deeptuning.h:3446 (`int op_launch_eta_cap;`); params.ini: default=0 min=0 max=8 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Refuse to launch at all when the quorum's eta exceeds this... 117 launches at eta >= 5 produced zero contacts and zero cities... It refuses the half-quorum patience release too. 0 = no cap."
- **Unit:** turns (integer; compared with launch_eta = max(deep_op_eta(), 1)).
- **Formula at the primary call site:** deepops.c:5524-5525 deep_op_drive: `if (tune->op_launch_eta_cap > 0 && launch_eta > tune->op_launch_eta_cap) { go = FALSE; eta_ok = FALSE; }`
- **All call sites:**
  - deepops.c:5524,5525,5531 deep_op_drive — hard cap on launch eta (also vetoes patience via eta_ok) — no: dead behind op_arrival_gate=0 / cap 0
- **Log line:** deep-netval etacap (target, force, eta, cap, odds)
- **Interacts with:** op_arrival_gate, op_muster_wait (patience vetoed), op_contact_quorum (eta k)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; dead-path at defaults.
- **Prior evidence:**
  - S-OPARRIVAL §5 — ETA cell (cap 4 alone, pre-registered falsification), contested, 16 TRAINING seeds — score -0.14 +/- 0.53 (t -0.27, 4/16); launches 4.88 -> 3.19 (t -2.17); eta>=5 launches 26 -> 0; unitslost -0.47 (t -1.25); unitskilled -0.30 (t -0.78); takes -0.12 (t -1.46) — null; "refusing them buys nothing measurable"

### op_march_loss_rate
- **Declared:** deeptuning.h:3455 (`double op_march_loss_rate;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Expected members lost per turn of march, charged into the net-value cost at the mean build cost of the force standing at the staging tile... 354 member deaths against 2 kills at the walls over 232 closed operations, 1.78 per operation booked dead in transit. 0.0 = the duel alone, as shipped."
- **Unit:** count per turn of march (members lost per turn), converted to want: `lost x mean build cost (shields) x attack_loss_gain x SHIELD_WEIGHTING`.
- **Formula at the primary call site:** deepops.c:5587-5597 deep_op_drive: `if (tune->op_arrival_gate && tune->op_march_loss_rate > 0.0 && present > 0) { double lost = MIN(tune->op_march_loss_rate * launch_eta, (double) present); ... cost += tune->attack_loss_gain * lost * (shields / present) * SHIELD_WEIGHTING; }`
- **All call sites:**
  - deepops.c:5587,5589 deep_op_drive — march-death cost term in the launch net-value test — no: dead behind op_arrival_gate=0
- **Log line:** deep-netval refuse / deep-op march (cost=)
- **Interacts with:** op_arrival_gate, attack_loss_gain, op_net_value
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; dead-path at defaults.
- **Prior evidence:**
  - S-OPARRIVAL §4/§8 — only inside the A-bundle at 0.56 ("confounded with the decay") — see op_arrival_gate; not isolated

### op_regate_gate
- **Declared:** deeptuning.h:3462 (`int op_regate_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "THE MARCH RE-GATE'S MEMORY. One gate over the two terms that stop a marching wave from talking itself out of its own operation. abort-odds is 302 of 640 terminal causes and in 57-71% of them the wave was intact or larger than at launch. 0 = neither term is computed."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepops.c:5944 deep_op_drive MARCH: `if (tune->op_regate_gate) { int marched = game.info.turn - op->march_turn; if (tune->op_regate_eta_freeze && op->march_eta >= 1) price_eta = MIN(eta, op->march_eta); if (tune->op_regate_sunk_gain > 0.0 && marched > 0) { ... floor_eff *= ...; } }`
- **All call sites:**
  - deepops.c:5944 deep_op_drive — enables eta-freeze and sunk-cost floor credit — no: dead at 0
- **Log line:** deep-op regate (peta=, floor=, marched=)
- **Interacts with:** op_regate_sunk_gain, op_regate_eta_freeze, op_abort_floor
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; dead-path at defaults.
- **Prior evidence:**
  - S-OPARRIVAL §4 — cell 001 (R: sunk gain 1.0, eta freeze 1), contested, 16 TRAINING seeds — score +0.33 +/- 0.29 (t +1.14, 7/16), launches -0.12 +/- 0.39, takes -0.12 +/- 0.09 (t -1.46); abort-odds 40 -> 36 but abort-march 10 -> 14 — null; R main effect +0.12

### op_regate_sunk_gain
- **Declared:** deeptuning.h:3468 (`double op_regate_sunk_gain;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Fraction of the journey already covered, credited against the abort floor: floor x (1 - gain x marched / (marched + eta)). 0.0 = the nominal floor."
- **Unit:** fraction 0-1 (dimensionless multiplier on the covered-journey fraction marched/(marched+eta)).
- **Formula at the primary call site:** deepops.c:5950-5953 deep_op_drive: `double sunk = (double) marched / (marched + MAX(eta, 1)); floor_eff *= MAX(0.0, 1.0 - tune->op_regate_sunk_gain * sunk);` compared as probability floor `odds < floor_eff`.
- **All call sites:**
  - deepops.c:5950,5953 deep_op_drive — lowers the abort floor — no: dead behind op_regate_gate=0 / gain 0.0
- **Log line:** deep-op regate (floor= shows floor_eff)
- **Interacts with:** op_regate_gate, op_abort_floor
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; dead-path at defaults.
- **Prior evidence:**
  - S-OPARRIVAL — tested at 1.0 only inside cell R (see op_regate_gate) — not isolated

### op_regate_eta_freeze
- **Declared:** deeptuning.h:3477 (`int op_regate_eta_freeze;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Price the phantom reinforcement at MIN(eta now, eta at launch) instead of eta now... so a wave that fails to converge inflates its own eta, which inflates the phantom garrison, which drives the odds under the floor, which dissolves the wave. 0 = the current eta, as shipped."
- **Unit:** boolean gate (0/1). It changes the eta (turns) used to price virtual reinforcements.
- **Formula at the primary call site:** deepops.c:5947-5948 deep_op_drive: `if (tune->op_regate_eta_freeze && op->march_eta >= 1) { price_eta = MIN(eta, op->march_eta); }` then `odds = deep_take_odds_at_arrival(nmap, target, stream, ns, price_eta);`
- **All call sites:**
  - deepops.c:5947 deep_op_drive — freezes the reinforcement eta at the launch eta — no: dead behind op_regate_gate=0 / switch 0
- **Log line:** deep-op regate (peta=)
- **Interacts with:** op_regate_gate, invasion_reinforce_rate (eta x rate), op_abort_floor
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; dead-path at defaults.
- **Prior evidence:**
  - S-OPARRIVAL — tested at 1 only inside cell R (see op_regate_gate) — not isolated

### op_contact_quorum
- **Declared:** deeptuning.h:3488 (`int op_contact_quorum;`); params.ini: default=0 min=0 max=8 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The k in deep_op_eta()'s k-th smallest member arrival, decoupled from `required`... a big wave can never reach the walls: 133 launches at required >= 9 produced two contacts and zero cities... 0 = k is `required`, as shipped."
- **Unit:** count (members; k-th smallest member arrival time in turns, k = MIN(quorum, n)).
- **Formula at the primary call site:** deepops.c:4479-4482 deep_op_eta(): `k = tune->op_contact_quorum > 0 ? MIN(tune->op_contact_quorum, n) : MIN(MAX(op->required, 1), n); return t[k - 1];` (t[] = sorted per-member arrival turns). deep_op_eta feeds the contact test (eta <= 1), march_eta, retarget, poach, and logs (12 call sites).
- **All call sites:**
  - deepops.c:4479 deep_op_eta — sets the quorum k for the whole op eta — no: dead at 0 (k = required)
- **Log line:** deep-march (quorum distance per marching turn), deep-op march (eta=)
- **Interacts with:** op_arrival_gate, op_launch_eta_cap, op_regate_eta_freeze (use deep_op_eta), op_take_target (determines required)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; dead-path at defaults.
- **Prior evidence:**
  - none — "NOT tested" (op-arrival-verdict §8): on the same causal chain as A and P, needs the next factorial

### op_draft_attack_floor
- **Declared:** deeptuning.h:3498 (`double op_draft_attack_floor;`); params.ini: default=0.0 min=0.0 max=8.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Minimum adv_unit_att_rating for a blade the garrison draft is allowed to borrow... Drafted formations are 63% of everything deep forms and take a city at 0.7% against 4.3% for undrafted ones. 0.0 = the shipped test: a rating of 0 or less is already refused, so a floor of 0 excludes nothing."
- **Unit:** rating units: adv_unit_att_rating() of a candidate blade (stock attack-rating currency, not shields or want). Compared as `adv_unit_att_rating(u) < floor`.
- **Formula at the primary call site:** deepops.c:4129-4130 deep_op_form garrison draft: `adv_unit_att_rating(u) <= 0 || adv_unit_att_rating(u) < tune->op_draft_attack_floor || def_ai_unit_data(u, ait)->charge > 0 || deep_op_claiming(pplayer, u->id) >= 0 => continue`.
- **All call sites:**
  - deepops.c:4130 deep_op_form — quality floor on drafted blades — no at 0.0: the term excludes nothing the `<= 0` clause did not already exclude (the draft itself is live at defaults, survival_target=0.5)
- **Log line:** deep-opodds (drafted=); no dedicated tag
- **Interacts with:** op_draft_marginal, survival_target, cover_removal, adv_unit_att_rating scale (stock)
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed; identity by construction (0.0 excludes nothing).
- **Prior evidence:**
  - none — "NOT tested" (op-arrival-verdict §8): no value derivable from the census (rule 7); §6 item 1 names drafted formations (3.8% vs 15.2% take, n=52/33 in control) as the cheapest remaining question

### mil_infra_gate
- **Declared:** deeptuning.h:3641 (`int mil_infra_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Fund mil-infra -- the EFT_DEFEND_BONUS building and the veterancy swap -- from stock's MEASURED assess_danger() figure rather than from deep's post-injection anticipatory fear, and refuse the run outright below mil_infra_threat_floor... 0 = today's funding basis and no refusal, byte for byte."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepdefence.c:5814-5816 deep_military_choose_build() (walls): `if (deep_tuning_get()->mil_infra_gate > 0 && fear_pre < (unsigned int) deep_tuning_get()->mil_infra_threat_floor) { infra_verdict = "refuse"; build_walls = FALSE; }` ; funding basis 5838-5841: `wall_fear_weight * (deep_tuning_get()->mil_infra_gate > 0 ? fear_pre : city_data->danger)`.
- **All call sites:**
  - deepdefence.c:5814 deep_military_choose_build — wall refusal below the floor — no: dead at 0
  - deepdefence.c:5840 deep_military_choose_build — switches wall funding basis fear_pre vs city_data->danger — no: dead at 0
  - deepdefence.c:6514 deep_military_choose_build — veterancy-swap (barracks) refusal below the floor — no: dead at 0
- **Log line:** deep-milinfra (turn, player, city, pre=, danger=, floor=, verdict=refuse|allow)
- **Interacts with:** mil_infra_threat_floor, wall_fear_weight, garrison_floor, diplo_price_gate
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed in the screen docs (landed after the instrument map); dead-path at defaults (gate 0).
- **Prior evidence:**
  - S-DIPLOPRICE (STATUS 23562, diploprice-verdict-2026-08-30.md §3) — 2x2 PARKED at 10 of 96 games (control only; the other three cells 0 games) — no result. Liveness at cell 11, one contested game: mil-infra gate refused 2813 of 3664 city-turns (76.8%).

### mil_infra_threat_floor
- **Declared:** deeptuning.h:3646 (`int mil_infra_threat_floor;`); params.ini: default=0 min=0 max=80 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Measured danger below which mil-infra is refused. PLACEHOLDER; the tested value is 1, the unit case -- at least one enemy unit that stock's own assessment scored above zero. Inert while the gate is 0."
- **Unit:** stock danger-score units: compared with fear_pre, an unsigned int copied from city_data->danger before deep injects its own fear (deepdefence.c:5521), i.e. the output of stock dai_assess_danger() (an integer danger/rating score; exact scale not defined in the deep sources). UNSURE of the absolute scale; the cell value 1 means "any positive stock danger". Default 0 means `fear_pre < 0u` is never true even with the gate on (funding-basis switch only, no refusal).
- **Formula at the primary call site:** deepdefence.c:5815 deep_military_choose_build(): `fear_pre < (unsigned int) deep_tuning_get()->mil_infra_threat_floor` ; also 6516 (veterancy swap) and the deep-milinfra log field floor= (6528).
- **All call sites:**
  - deepdefence.c:5815 deep_military_choose_build — walls refusal threshold — no: dead behind mil_infra_gate=0
  - deepdefence.c:6516 deep_military_choose_build — barracks/veterancy refusal threshold — no: dead behind mil_infra_gate=0
  - deepdefence.c:6528 deep-milinfra log field floor= — log only (printed every call)
- **Log line:** deep-milinfra (floor=)
- **Interacts with:** mil_infra_gate
- **Tier/screen:** unranked (legacy M, ALL, MIL). Screen: not listed in the screen docs; dead-path at defaults.
- **Prior evidence:**
  - S-DIPLOPRICE — cell value 1 only; 2x2 parked (see mil_infra_gate)

## Unresolved

- Per-knob screen verdicts for 39 of the 49 knobs: knob-screen-2026-08-30.md lists only op_take_target (flip rate), the six BASSAULT knobs (firing rates, tier B), and weak_prey_gate/gain ("unmapped"). I tried to regenerate the rest with benchmark/knobscreen.py on benchmark/results/w25-screen-corpus (zero games) but it did not finish in 500 s (it printed "bad predicate: 'slack'" to stderr), so the entries say "not listed" and mark "dead-path inferred" only where the compiled default gate is 0 (my inference from the code, not a table row).
- Tuning files: "live at defaults" is evaluated against the compiled defaults in deeptuning.c (they match tuning-params.ini for all 49). I did not check which benchmark/tuning/*.ini a given A/B loaded, so a cited arm may have pinned other knobs (e.g. survival_target 0.85 in pv3/postfix files vs compiled 0.5).
- Ruleset for tired_attack_floor: benchmark/configs/{contested,lategame-80}.conf name no ruleset; STATUS says civ2civ3 is the server default (tired_attack = TRUE in data/civ2civ3/game.ruleset:236; classic FALSE:231). Not re-verified against a live server. Observed population is zero firings (STATUS 8604).
- Units marked UNSURE: deterrence_gain (header says marginal attrition "in shields" but the expression adds the product to adv_want with no SHIELD_WEIGHTING), spy_sabotage_gain (same issue), mil_infra_threat_floor (absolute scale of stock city_data->danger not defined in ai/deep).
- sortie_defence_weight: the sum of adv_unit_att_rating (linear) is added to assess_defense(), which the in-source comment at deepcm.c describes as a quadratic of summed defence; I did not trace assess_defense to confirm the scale of the sum, so comparability of the two addends is unverified.
- Prior evidence for the tune-postfix campaign (2026-08-23): the verdict doc says ~80 constants mutated jointly; I did not confirm which of these 49 were in the mutated set, so it is cited as non-attributable context only.
- Early B-series statements (B8/B10, 2026-07-31) for op_wave_window/raid_radius are session-close prose in STATUS, not paired A/Bs; no SE exists.
- deep_op_eta is referenced 12 times in ai/deep/*.c (definition, comments and calls); I did not enumerate each call to list every consumer of op_contact_quorum.
- poach_marginal_gate (not in this slice) is 0 at defaults, so the second use of defence_worth_mult at deepops.c:~4830 is dead; the line number is approximate (found by reading, not by grep of the knob name).
