# MILITARY domain audit — `ai/deep` (reader report, verbatim)

**Read at git HEAD `36bac91163`.** The E34.2 stance-gate commits (`1303e0f6c7`, `236e0fe813`, `7a06200351`, `88aacc3cbc`) are **already in** this tree, so gates 1–4 are landed as read. Nothing below was built, run, or benchmarked. **[V]** verified by reading code at this SHA, **[I]** inferred.

**Out of scope, one line each:** threading (W20); honest-information / RC6 omniscience inference (W04); ally conversation beyond noting the two existing chat emitters.

---

## 1. Mechanism inventory

### 1a. The fear pipeline, end to end

Danger is a single `unsigned int` (`ai_city::danger`) that stock computes and deep *edits in two places with identical arithmetic*. Both sites: `deepdefence.c:943` (once per turn, `deep_credit_incoming_defence`) and `deepdefence.c:2277-2345` (per city, inside `deep_military_choose_build`). The duplication is deliberate — stock's `military_advisor_choose_build` re-runs `assess_danger()` internally, so an edit at phase start cannot reach production (`deepdefence.c:2249-2252`). **[V]**

| # | Source | Site | Decides | Ramped? | Gate-1 scaled? |
|---|---|---|---|---|---|
| F1 | `deep_overseas_danger` | `deepdefence.c:325` | fear of **embarked** at-war cargo within ~1 transport move of a coastal city | no | yes |
| F2 | `deep_boat_shadow_fear` | `:422` | flat standing coastal fear while any at-war player *owns* a sea transport | no | yes |
| F3 | `deep_density_fear` | `:658` | per met non-allied city within `density_fear_radius`, `value/(1+d)`; cross-water weight `menace_overseas_weight` unless the neighbour floats hulls | **yes** (`:713`) | yes |
| F4 | `deep_density_prior` | `:610` | pre-contact prior from lobby settings (`startpos` × `deep_map_crowding`), zeroed on first contact | **yes** (`:647`) | yes |
| F5 | `deep_barbarian_fear` | `:724` | flat, gated on `barbarianrate`/`onsetbarbarian` — the server's own gate | no | yes |
| F6 | `deep_survival_deficit_fear` | `:852` | `survival_fear_value × (target − p)/target`, from the **observed** wave | no | **no — floor** |
| F7 | `deep_apply_garrison_floor` | `:827` | token garrison where danger==0, scaled by `deep_garrison_menace` | n/a | **no — floor** |
| F8 | `deep_garrison_menace` | `:758` | 0..1 militarisation ratio from **public** `score.units` of met players, reachability-weighted | n/a | scales F7 only |
| — | `deep_defender_upkeep_discount` | `:380` | multiplicative discount on *all* injected fear by cheapest buildable defender's gold+food upkeep | | applied to all |
| — | `deep_fear_satisfaction` | `:903` | kills injected fears (×`satisfied_fear_discount`, default 0) once floor met **and** survival ≥ target | | applied to F1–F5 |
| — | `deep_incoming_defence_credit` | `:173` | subtracts en-route defenders, decayed by eta and survival | | subtract |
| — | `deep_muster_reserve_credit` | `:225` | same formula for members of **MUSTER-state** assault ops | | subtract |
| — | `deep_stance_fear_mult` | `deepstance.c:241` | gate 1: `(mil/econ)^stance_fear_gain` | | the gate |

**Aggregation order** (`deepdefence.c:969-980`, mirrored at `:2299-2311`):
`danger = stock_assess_danger; += (F1+F2+F3[+F4]) × updisc × sat × fmult; += F6 × updisc (never scaled); -= min(danger, incoming + muster credits); then F7 floor if danger == 0`.
F4 applied **only at the build site**, not the once-per-turn site — asymmetry, probably deliberate but undocumented. **[V]**

**Consumers of aggregated danger:** the stock quadratic danger→percent ladder (`:2394-2551`); wall funding (`:2456-2464`); rotation want injection (`:2313-2344`, only when danger==0); `deep_melt_surplus_defenders` (`:1013`, gated danger==0 && urgency==0); `deep_balance_treasury` (`:3862`); CM uses survival probability directly, not danger (`deepcm.c`); horizon credits use survival (`deephorizon.c:546`).

### 1b. The survival substrate

**`deepodds.c`** — pure math, freestanding, unit-tested. `deep_duel_defender_dist` (`:36`) keeps the defender's post-duel **HP distribution**; `deep_stack_survival` (`:108`) DP over (front-defender × HP) chaining an ordered attacker stream. Returns `1 − P(stack wiped)`. Single wave; no healing/promotion/reinforcement.

**`deepcombat.c`** — grounding. `deep_fresh_attack_power` (`:95`, purity restored); `deep_collect_defenders` (`:114`); `deep_city_survival_core` (`:200`, wave = at-war units with att>0 reachable within `survival_horizon`; continent match / transported / UMT_SEA vs coastal; barbarians exempt from contact req `:222-226`); `deep_take_odds_at_arrival` (`:331`, adds `eta × invasion_reinforce_rate` virtual best-buildable defenders; virtuals destroyed `:396`). Faithful to the engine's combat triple (walls/terrain counted). **[V]**

**All nineteen consumers of the substrate (verified by grep):** survival-deficit fear (:864); satisfaction gate (:928); kill sortie (:1306); cover sortie (:1383); sparable-blade (:1634); counter-claim trigger (:1774); lender safety (:1823); relief sizing (:1863,:1873); expansion-block override (:2240); build-side assault want (:3043,:3046,:3073,:3118); CM threat + grow-release (deepcm.c:162,211); horizon credits (deephorizon.c:546); op formation bar/garrison draft (deepops.c:2538,2578,2588); retarget (:2830); poach (:2955); consolidate bar (:3033); strike abort (:3150); march re-gate/invade gate (:3414,:3437). `survival_target` is one scalar arbitrating ~20 decisions.

### 1c. Everything else, by file

**`deepdefence.c`** — melt (rotation move 3, `:997`); convoys (`:1161`); kill sorties (`:1257`, duel ≥ `sortie_win_floor` AND city survives without the blade); cover sorties (`:1334`); tired-attack hold (`:1416`); worker flee + border bodyguards (`:1468`); sparable blade (`:1609`); brigade relay (`:1662`/`:1734`, hole migrates to the safe end); counter-claims (`:1757`, sized relief or **nobody**, then poach, then logged write-off); expansion block (`:2223`, per-city survival replaces stock's sticky war footing); military choose build (`:2254`); assault want (`:2937`); emergency triage (`:3166`); spend gold (`:3192`); balance treasury (`:3816`, civilians die last); ferry/water geometry (`:2009-2221`).

**`deepops.c`** — claim registry (`:72`,`:107`,`:1979`); ASSAULT lifecycle MUSTER→MARCH→STRIKE→CONSOLIDATE (form `:2413`, launch `:2634`, quorum-eta `:2690`, drive `:3289`, strike best-duel-first `:3118`, consolidate `:3006`, retarget `:2771`); RAID (`:2269`,`:2308`); LIFT (`:1252`,`:1453`,`:1382`); poach (`:2918`); target-threatens gate-2 exemption (`:2390`); ally briefing (`:2136`, purely social); manage units (`:1884`); five censuses.

**`deepstance.c`** — three axes, four gates, `x^gain` on a ratio, identity at econ==mil. Shipping defaults (`stance_forced=-1`, `stance_source=0`) mean **every gate returns exactly 1.0 in a shipping game** — the acceptance gate, not a defect, but the whole W34 layer is currently inert by construction. **[V]**

---

## 2. Unconsumed signals

**U1 — the HP distribution the substrate exists to preserve is discarded at every consumer. [V]**
`deepodds.h:16-29` argues `win_chance()` "throws the rest of the distribution away"; `deep_stack_survival` recovers it — then returns one scalar. No consumer reads expected surviving HP, surviving defenders, or **how many of our own attackers die**; `n_att` is input-only. Consequence: `deep_assault_want` prices `worth × Δp` (`deepdefence.c:3086`) with **no cost-of-losses term**. The AI can answer "will I take it" and not "is it worth what it costs" — precisely the W14 war-ledger input, and why W14 has nothing to build on. Prize: large; the one change that makes "the army is the cost, not the product" actionable from the military side.

**U2 — the LIFT's military use case has no producer. [V]**
`deepops.h:92-96` says a lift carries "ANY unit… or a defensive unit being redeployed overseas". But nothing sets a cross-continent goto for a defender: counter-claims skip other continents (`deepdefence.c:1794-1796`), relay BFS is continent-local (`:1691`), garrison draft continent-local (`deepops.c:2558`); op-launched units are claim-skipped (`:1262`). **The lift carries civilians only.** An overseas city below the bar can never be reinforced except locally.

**U3 — `deep_committed_away` is computed and multiplied by 1.0. [V]** (`deepdefence.c:275-307`; `committed_fear_mult` default 1.0 "PLACEHOLDER = no effect".)

**U4 — the write-off is a log line. [V]** (`:1878-1885`) No evacuation, no production switch, no fear reduction for the doomed city. Defers to W14. `grep evacuat ai/deep/` → nothing.

**U5 — no kills-side accounting anywhere. [V]** Losses are logged (`deepai.c:689-699`); kills/raids/captures are not. Raid value, sortie value, op value unmeasurable at runtime; nothing can feed back.

**U6 — `stance.cult` and `stance_source` 1/2**: declared inert on purpose, correctly documented.

**U7 — barbarian-city assault demand exists on the ops side and not the build side** (see C4).

---

## 3. Missing couplings / emergent interactions

**C1 — `deep_manage_units` wipes every non-op `done` flag, silently disabling three mechanisms. [V] — highest confidence, highest surprise**

`deepops.c:1903`: `unit_data->done = deep_op_holds(pplayer, punit->id);` — an **unconditional assignment**, running (`deepdefence.c:1941`) *after* every pass that sets `done` in first_activities (`:1927-1936`). STATUS 2026-08-01 records finding this hole for op holds; the fix preserved **only** op holds. Still erased today:
- `deep_hold_tired_attackers` (`:1455`) — the entire tired-attack discipline; unit logged held, then attacks anyway (`dai_manage_unit` only self-marks at moves_left ≤ 0). Only surviving effect: skipped as a sortie blade same turn (`:1276`).
- `deep_manage_convoys` HOLD (`:1233`) — the boat sails regardless. STATUS's "verified inert" has a second, independent cause.
- `deep_flee_workers` (`:1568,1577`) — the flee move happens but the unit is re-tasked the same turn.
- consolidate's just-converted garrison (`deepops.c:3062-3064`) — removed from members first, so `done` resets and `:1904` clears its fresh `AIUNIT_DEFEND_HOME`; `dai_set_defenders` re-decides.

*Emergent behaviour:* deep attacks at one-third strength exactly where it decided not to; convoys sail half-loaded; fleeing workers walk back toward the threat. *Prize:* three landed mechanisms go from zero to live for ~5 lines + a phase-scoped marker (the flag is also stale from last turn).

**C2 — sortie passes are the only unit-claiming pool that skips `deep_op_claiming`. [V]**
Sparable-blade, counter-claims, op pool, garrison draft, raid enrolment, lift scan all check it; `deep_kill_sorties` (`:1271-1301`) and `deep_cover_sorties` (`:1369-1391`) do not. MARCH members can be assigned as a worker's bodyguard while enrolled in a marching wave.

**C3 — the melt does not run the survival test every other leave-the-city decision runs. [V]**
Gates on urgency==0 && danger==0; with `satisfied_fear_discount=0`, a *satisfied* city can have danger 0 with an observed wave present; melt disbands without `_without`. Also does not check op claims (a drafted obsolete Phalanx mustering in a quiet city is meltable).

**C4 — build side and ops side pick assault targets by different rules. [V]**
`deep_op_pick_target` (`deepops.c:2037-2043`) exempts barbarians from contact; `deep_assault_want` (`deepdefence.c:2972-2976`) does not, though `:2022-2024` claims shared gate logic. `deep_assault_want` also doesn't exclude op-targeted cities, and its pool (`:3007-3019`) counts every free attacker on the continent as one stream regardless of how many ops own them → with 4 concurrent assaults the marginal attacker prices against a fictitious combined force, Δp≈0, build stops funding attackers exactly when campaigns run; post-T160 (47 empty barbarian cities) ops will form reconquests the build side never funds a unit for.

**C5 — target selection is anchored at an arbitrary city. [V]**
`deep_op_form` uses the **first** city in `pplayer->cities` as the scoring anchor (`deepops.c:2426-2429`, `worth/(1+dist)` from there `:2457`) while staging is chosen near the target afterwards (`:2496-2512`). Systematic bias toward the list head's neighbourhood. Cheap fix: score from best staging candidate or empire centroid.

**C6 — nothing closes the loop from outcomes back into calibration. [V]**
No war ledger (W14). `invasion_reinforce_rate` predictions never checked; abort/strike ratios offline-only; raid trades unmeasurable (U5); fear knobs cannot self-correct. Prerequisite: U1.

**C7 — survival probability replaced urgency/grave_danger in four places on 2026-08-01; not in four others.**
Still presence-counts: the melt (C3), emergency triage sort (`:3166`), veteran-building guard (`:2905-2906`), severe-danger attacker cutoff (`:2588-2589`). Instances six through nine of the project's own named recurring defect. The triage sort matters most — it decides which of two dying cities gets the gold.

**C8 — counter-claim and ops never trade across water. [V]** (U2; poach also continent-filtered `deepops.c:2965-2966`.)

---

## 4. Intelligence-gap flags, ranked (gaps the E1–E8 map does NOT cover)

**G1 — Cost of victory. (RC4, RC1)** Nothing prices what an attack costs us. `worth × Δp`, no expected-losses subtraction, no same-currency comparison against a granary. The substrate computes everything needed and returns one scalar (U1). A strong human declines to attack far more often than this AI.

**G2 — Force concentration across operations. (RC2)** 4 slots, one op per target, no merge of two under-strength ops, no sequencing. E7 covers "hit again", not "these two half-waves should have been one wave". With C4's double-counting: four marginal campaigns where a human runs one decisive one.

**G3 — Timing windows. (RC6, RC2)** Target scoring is `worth/(1+distance)`, full stop. Nothing reads third-party wars (no `pplayers_at_war(a,b)` anywhere in the military files), rival army departures, war-weariness, anarchy. `WIN_CAPITAL` — computed by stock, read by nothing — is the abandoned infrastructure this would connect to.

**G4 — Tech-edge windows. (RC1)** Defence prices obsolescence well; offence has no "strike before they get X / after I get Y". *When* is unmodelled.

**G5 — Defence in depth / terrain. (RC3)** The brigade relay is real logistics depth. But: no fortress/airbase construction (one comment only), no chokepoint defence (bonus exists only in site selection), no coastal interdiction, no fortify doctrine. Survival wave is **blind to air units** on another continent (neither UMT_SEA nor transported → dropped).

**G6 — Deterrence signalling. (RC6)** Both chat emitters are social-only; nothing reads incoming signals. `deep_garrison_menace` reading public score is the one honest-inference channel and the right template. Ranked low: war/peace is still stock, nowhere to put the output.

**G7 — Naval combat proper.** Sea attackers = stock `kill_something_with`; deep contributes escort pairing + hull geometry only. Naval-doctrine §3.2–3.4 unbuilt. Era-reachability caveat.

---

## 5. Stale-plan notes

1. `deepops.c:2022-2024` claims shared gate logic with `deep_assault_want`; they differ on the barbarian exemption (C4).
2. STATUS 1318-1322 describes the manage-loop fix as preserving op holds; it's an unconditional overwrite erasing three non-op holds (C1). Accurate about intent, silent about collateral.
3. `deepdefence.c:1464-1466` "marked done… so nothing re-tasks it": untrue after `deepops.c:1903` (C1).
4. `deepdefence.c:1410-1414` tired-attack rationale describes a hold that does not hold (C1).
5. `deepops.h:92-96` lift-carries-defenders: no producer exists (U2).
6. `deepdefence.c:1977-1997` orphaned doc-comment ("Hulls that are genuinely SPARE…") above the wrong function; the `deep_expansion_blocked` rationale (`:1950-1976`) separated from its function by it.
7. `deepdefence.c:2064-2067` stale forward declarations.
8. 00-context §3: deep now reads ocean decomposition via its own scan (not `adv->oceans[]`) and reads `get_lake_surrounder`; `is_pos_dangerous` and own-city `ai_invasion` rows remain accurate.
9. sessions/README.md posture-notes warning: two banked notes ARE recoverable from code and should be re-anchored: "sorties priced as survival-deltas" (implemented as a survival *floor*, not a delta — `deepdefence.c:1306`) and "escort demand scales from zero with observed sea threat" (implemented as boat-shadow fear, but escort recruitment `:1188-1207` is unconditional on threat).
10. `barb_reinforce_rate = 0.0` and `committed_fear_mult = 1.0`: two knobs whose shipping defaults no-op their mechanisms; flagged in-file, absent from STATUS's owed-tuning list.

---

### One-paragraph summary

The military layer is unusually complete on the defensive and probability side — a faithful sequential-combat model with nineteen honest consumers, a fear lattice with explicit floors and an anticipation/evidence split, a relief system that sizes forces or sends nobody, and a real multi-turn operation object with claim discipline. It is missing three things that compound: it computes a full combat distribution and reads only one number from it, so nothing prices what a war *costs* (U1 → C6 → G1); three landed mechanisms are silently disabled by an unconditional `done`-flag reset in `deepops.c:1903` (C1); and its build side and ops side pick targets by different rules from different anchors (C4, C5). Fixing C1 costs about five lines and turns three no-ops into live mechanisms; extending `deep_stack_survival` to report attacker attrition is the single change that unblocks the war ledger, the raid trade metric, and the cost-of-victory gap at once.
