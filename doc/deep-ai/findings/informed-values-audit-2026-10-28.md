# Audit of informed-v1: units, anchors, arithmetic (2026-10-28)

*Stage 3 of three. Adversarial and narrow: for each of the seven values
`benchmark/tuning/informed-v1.ini` changes, is the unit right, does the
anchor say what the memo (`informed-values-2026-10-28.md`) says it does,
and does the arithmetic hold? Whether the idea is good is not judged
here. Every claim below was checked against the C source at the current
tree. Nothing was built or run.*

## 0. Plumbing: parse and control

**The ini parses. Trailing `; comment` is safe.** `get_token_value()`
reads a number up to the first char that is not a digit or `.`, then
accepts `\0`, `,`, whitespace or a comment char (`utility/inputfile.c:844`,
`is_comment()` = `#` or `;` at :154-157). `get_token_eol()` (:736-758)
skips whitespace and accepts end of line or a comment char. The shipped
rulesets use the same form (`data/alien/cities.ruleset:92`
`add_to_size_limit = 8 ; cities >= this ...`). `deep_tuning_load_file()`
(`ai/deep/deeptuning.c:858-884`) goes through `secfile_load` →
`secfile_lookup_{float,int}_default`, so it uses the same tokenizer.
`tests/deeptuning_test.c` does not exercise trailing comments, so the
rulesets are the evidence here, not the test.

There is a separate trap, and this file avoids it. `entry_float_get()`
requires `ENTRY_FLOAT` (`utility/registry_ini.c:3513-3514`). A double
knob written as a bare integer (`= 2` rather than `= 2.0`) would fail
the lookup and silently fall back to the compiled default. Every double
in informed-v1.ini has a decimal point (`0.5`, `2.0`, `0.75`, `0.0`,
`0.0`, `2.0`). The one int knob, `garrison_floor_danger = 0`, is
`TUNE_INT` (`deeptuning.c:560`). All values are exact in float. The
loader has no range clamp. Whoever edits the file next must keep the
decimal points.

**The control is right.** The `[tuning]` section of `stacked-on.ini` is
exactly the six gate lines of informed-v1.ini (five gates = 1,
`founder_floor_gate = 0`). The contrast is therefore exactly the seven
knobs.

---

### garrison_floor_danger  40 -> 0   VERDICT: CONFIRMED (the magnitude transfer is weak)

- **Unit check:** `applied = (unsigned int) (tune->garrison_floor_danger * upkeep_discount * menace * w.best * fmult); city_data->danger = applied;` (`ai/deep/deepdefence.c:1995-1997`). The guard at :1979-1982 returns early unless `danger == 0`, the turn is at least `garrison_floor_turn`, and the tile holds fewer than `garrison_floor` units. The unit is danger points, as the memo says. **0 is not a sentinel.** At 0 the function writes `danger = 0` into a field that was already 0, so the floor is off. No other code reads `garrison_floor_danger` (grep: only :1995 plus the loader), and the inventory lists only that one call site. The count `garrison_floor`, which the melt and `deep_fear_satisfaction` read, is unchanged. The "switch" reading is right: an empty tile means `our_def == 0`, and with `urgency == 0` that gives `danger = 100` (:5730-5734) at the EMERGENCY tier (:5783-5784). One nuance: at 40 the product can truncate to 0 when `menace × w.best` is small, so 40 was never quite "always on".
- **Anchor check:** STATUS.md G1 §1 (≈7150-7175) gives, for `floor-off.ini` (`garrison_floor_danger = 0`) against identity, 20 seeds, pinned binary `536df20ee44c`: isolated-150 cities −1.09 ± 0.39 (t −2.77), lategame-80 −0.66 ± 0.16 (t −4.16), city losses +0.80 / +1.40. The memo quotes these correctly. The kind of anchor is right (a measurement of this knob alone). **But it is not the same mechanism or venue now:**
  1. The arm ran the **ungated** floor. The class gate (`92e289e0a2`), the marines escalation (`d8c436217a`) and gate 6 (`7c05d859e5`) all landed after it on 2026-08-05. G1 §2 measured the gate removing only 16% / 3% of firings, so the gap is small.
  2. The arm ran **without the opportunity charge**, which is ON in this arm's control. With `mil_opportunity_gate = 1`, a founder bid above 550 already beats the floor's 1100 first-defender want (see `mil_opportunity_price` below). That is the very displacement G1 measured ("57 settler decisions per game displaced in thirty turns", lategame-80). In the control that channel is already largely open, so the floor's marginal city effect here should be smaller than the anchor.
  3. The anchor venues are isolated-150 and lategame-80. The pre-registered primary venue is **contested**, where this knob has never been measured.
- **Arithmetic check:** none needed; it is a switch, and 0 is its off position. The value follows.
- **Scope check:** live (tier A, 113 flips/game per the inventory). `garrison_floor_turn` (default 0) and `founder_tier_gate`, `birth_guard_gain`, `fear_evidence_gate` and `deterrence_gain` (all 0) stay inert in both arms. No other call site.
- **Risk:** no unit risk. The risk is that the effect is much smaller than the +0.2…+0.6 cities the pre-registration rests on (point 2 above). The direction is not in doubt.

### scale_defender  1.0 -> 0.5   VERDICT: UNVERIFIABLE (the arithmetic gives 0.55; the load-bearing step has no anchor)

- **Unit check:** `case DWC_DEFENDER: return t->scale_defender;` (`ai/deep/deepwant.c:142-143`); `choice->want = choice->want * scale;` (:235), with an early return for EMERGENCY (:173-224) and for scale == 1.0. The only caller is `deepdefence.c:8792`, before the deflator, the upkeep charge, the switch charge and the opportunity charge. The unit is a dimensionless multiplier on NORMAL-tier defender want, as the memo says.
- **Anchor check:** panel-2026-10-27.md §2 (contested, training seeds 1-100, stack on), T150: units/city 3.42, settlers 1.86, cities 4.76. §1 (contested, **holdout seeds 101-120**): baseline units/city 2.57, settlers 2.40, cities 5.28. The memo's quotes are correct, but:
  - the two sides come from different seed sets and different panels;
  - what is subtracted is settlers only, not "founders+workers", so "military" still contains workers and explorers (`munits` counts every non-special unit, panel §0);
  - "military" includes **attackers**, which `scale_defender` does not touch.

  This is an observational, cross-arm ratio. It is not a measurement of what this knob does.
- **Arithmetic check:** deep military per city = 3.42 − 1.86/4.76 = 3.03. Baseline = 2.57 − 2.40/5.28 = 2.12. Excess 0.91 out of deep's non-first 2.03 is 45%, which would imply a scale of 0.55. The memo's 0.5 is within its own rounding. The step that carries the weight is "a ×s cut in want gives a ×s cut in count". That step has no anchor. The merge is a comparison against a domestic winner of about 192 (building) or 750 (founder), so the count response is a threshold effect, not a proportional one.
- **Scope check:** live only where a NORMAL-tier defender bid exists. With the floor off, the first defender of an empty city is EMERGENCY and out of reach (the memo says so). At T50, where the panel gap is largest (1.00 vs 0.33), most of the excess is first defenders, which this knob cannot move. Its effect is a T100+ effect, as the memo registers.
- **Risk:** if the count response is weak, nothing happens (a wasted factor). If it is strong (most NORMAL defender bids are 100-235 against a winner of 192-750), 0.5 removes most second defenders. The falsifier, cities lost in the war venue, is the right one.

### mil_opportunity_price  1.0 -> 2.0   VERDICT: CONFIRMED

- **Unit check:** `charge = (adv_want) (tune->mil_opportunity_price * dom->want); mil->want = MAX(0, mil->want - charge);` (`deepdefence.c:8430-8431`). The guards (:8416-8427) are gate, price, positive wants, founder domestic winner, and DEFENDER/ATTACKER class. **There is no tier guard**, as the memo says. The comparison is `deep_merge_domestic_wins()` → `deep_want_cmp()`, tier first (:8507-8516). The tier is re-derived from the magnitude (`deepwant.c:52-58`), so a charge can pull a ≥1000 want below 1000.
- **Anchor check:** the first-defender want is `DAI_WANT_BELOW_MIL_EMERGENCY (1000, daitools.h:31) + danger + div`. With `our_def == 0` and `urgency == 0` the danger is 100 (`deepdefence.c:5730-5734`), applied as EMERGENCY at :5783-5784. **`div` is identically 0 in both arms**: `deep_deterrence_dividend()` returns 0 when `deterrence_gain <= 0` (:5235), and its compiled default is 0.0 (`deeptuning.c:231`). That makes §E.4 of the memo moot: "1100 + div" is exactly 1100. What about the urgency > 0 case (danger = 200 + urgency)? It **never reaches the charge**. `skip_emerg = EMERGENCY && urgency != 0 && !contest` (`deepdefence.c:8661-8670`) suppresses the domestic advisor, so no founder bid exists and no charge applies. `founder_tier_gate` is 0. The founder ceiling is `DAI_WANT_DOMESTIC_MAX = 1000.1/4*3 = 750.075` (daitools.h:33, daidomestic.c:631). Switch charges (EMERGENCY-exempt on the military side, :7908) and the deflator can bring the founder side below 750. The upkeep charge cannot, because `unit_upkeep_charge` is 0.
- **Arithmetic check:** after the charge, M = 1100 − p·d. If M ≥ 1000 the military side wins on tier. Since d ≤ 750, the founder wins iff d > M, i.e. **d > 1100/(1+p)**. That is 550.0 at p = 1 and 366.7 at p = 2. This matches the memo with div = 0. The value 2.0 is the range maximum (`tuning-params.ini:107`), chosen on a directional argument, and the memo says so.
- **Scope check:** the knob is live behind the gate, which is on in both arms. The memo's band [367, 550) is right. Side effect at NORMAL tier: at p = 2, any founder d ≥ 333.4 beats **every** NORMAL military bid (M < 1000), attackers included. At p = 1 that takes d ≥ 500. Expansion-first is the intent, so this is a property of the arm and not an error.
- **Risk:** low as far as units go. The behavioural risk is a founder displacing a first defender in an empty, urgency-0 city that density fear has flagged, with the floor already off. The memo's falsifier covers this.

### upkeep_gold_weight  0.125 -> 0.75   VERDICT: CONFIRMED on unit and arithmetic; the anchor holds only for one building class

- **Unit check, the orchestrator's claim 1:**
  - **Stock.** `prod[O_GOLD] -= city_improvement_upkeep(acity, upkept)` (`ai/default/daicity.c:1490`), then `want += prod[O_GOLD] * adv->gold_priority` (:1508 or :1511), with `gold_priority = TRADE_WEIGHTING = 18` (`server/advisors/advdata.c:518`, `advbuilding.h:21`). `base_want()` is the *difference* of that per-turn output sum with and without the building (:1549-1554), and the result is added to `building_want[]` with no horizon multiplication (:1929). So stock charges **18 want per gold per turn**, on a per-turn footing.
  - **Deep.** The credits are **per-turn rates, not horizon sums**. `deep_unworked_tile_value()` (`ai/deep/deephorizon.c:389-457`) is ONE tile's per-turn output priced at stock's own priorities: `food*food_priority + trade*science_priority + shield*shield_priority`. The capacity credit is `amortize(per_citizen * steps * w, ttg)` (:578-579), where `steps = MORT/ttg` is a **count of arriving citizens**. Citizens × (want per turn per citizen) is want per turn, discounted once by `amortize()` = `benefit·q^delay`. The content credit (`freed * per_citizen`, :704-705) and the multiplier credit (`arriving * raw[]` × priorities, :915-925) have the same form. Nothing sums over turns; a sum would be about `per_citizen × steps × MORT/2`.
  - **Conclusion.** The two sides are on the same per-turn footing. `debit = upkeep × dk_bdur × w` with `dk_bdur = 24` (gate off, :2394-2398) prices one gold-turn at 24w, so stock parity is **w = 18/24 = 0.75**. The memo's §E.3 worry (that the consistent value might be 18) is resolved in the memo's favour. The code comment at `deephorizon.c:2370-2382` ("every credit above counts a per-turn value across MORT turns ... subtracting a single turn of it ... was a dimensional error") is itself the misreading. That is a documentation defect, recorded here and not fixed.
- **Anchor check, where it fails:** stock charges that 18 **only for buildings `adv->impr_calc` marks CALCULATE or CALCULATE_FULL**. For ADV_IMPR_ESTIMATE, `base_want()` returns 0 at its top (`daicity.c:1529-1530`), so no upkeep is charged at all. CALCULATE is set only by Capital_City, Pollu_*, Output_Bonus*, Output_Waste_Pct and Upkeep_Free, and FULL by Output_*_Tile (`advdata.c:89-133`). Make_Content and the other content effects sit in an `#if 0` (:91-99). In civ2civ3 that splits the buildings like this:
  - **ESTIMATE (stock charges 0):** Temple, Amphitheater (rule name Colosseum), Aqueduct.
  - **CALCULATE (stock already charges 18):** Library, Marketplace, Granary (via Output_Waste_Pct), Courthouse, Harbour.

  So the memo's "6× asymmetry between the gold a building earns (credited at 18 in base_want) and the gold it costs" does not describe the Amphitheater, the building the memo targets: stock's `base_want` for it is 0. As a **currency conversion** (one gold per turn = `gold_priority` want) the anchor is legitimate and 0.75 follows. As "matching stock's own charge", it matches only the ESTIMATE class.
- **Arithmetic check:** 18/24 = 0.75. Worked example: 3 × 24 × 0.75 = 54 off a 161 credit. Confirmed.
- **Scope check:**
  1. **Double charge on the CALCULATE class.** Deep's `add` is added *on top of* stock's `building_want[]` (`deephorizon.c:2484`), and for these buildings stock has already netted 18 per gold-turn. At 0.75 the effective charge on Library, Marketplace, Granary, Courthouse and Harbour is **36 per gold-turn**. It is bounded: `add = MAX(add, 0)` (:2399) lets deep's debit remove at most deep's own credit, never stock's want. No single w fixes both classes; 0.75 is correct for the class the memo aims at.
  2. Second call site `deepdefence.c:7854` (unit gold+food upkeep) is dead because `unit_upkeep_charge = 0.0` (`deeptuning.c:234`). No side effect there.
  3. `dk_bdur = 24` only while `deep_horizon_turns() = MORT`. Under the endgame clamp, both credit and debit shrink.
  4. The prior tuning campaign moved this knob to the equivalent of 0.196-0.25 on the new scale, with null results. 0.75 is about 3× beyond any value ever played.
- **Risk:** if a reader shows the credits really are horizon sums (I find they are not), 0.75 under-charges by 24×. The realistic risk is the other way round: Libraries and Marketplaces lose their deep credit to a doubled charge. That shows as science or gold loss, not as Amphitheater picks, and the falsifier as written would not catch it.

### founder_site_cap_slack  1.0 -> 0.0   VERDICT: CONFIRMED (0 is the range floor that the anchor's arithmetic hits)

- **Unit check:** `cap = sites + tune->founder_site_cap_slack; capped = ((double) committed >= cap);` (`deepdefence.c:8218-8219`). `committed` is founders alive plus founders under construction in every city *except this one* (:8165-8185). `sites = deep_remaining_sites()` = reachable free land ÷ own land per city (:674-690). The unit is founders, as the memo says. It is the only call site.
- **Anchor check:** city-ledger-2026-08-30.md §5 (`siteplan/war/c00`, contested, **gates-off defaults**, 60 games): the `sites` column reads 3.24 / 3.60 / 3.26 and "came home empty" reads 71.6% / 74.0% / 97.2% for T51-75 / T76-100 / T126-150. The quotes are exact. The kind is right: a measurement of the estimator's bias. It was taken with the gates off, but the transit gate does not change `deep_reachable_free_land()` (that counts unowned land).
- **Arithmetic check:** if the estimator carries about 3 phantom sites late and the slack is meant to admit about 1 walking founder, the consistent slack is about 1 − 3 < 0, which clamps to the range minimum of 0. The value follows. **There is an off-by-one in the memo's gloss:** at `sites = 3.3` and slack 0, `committed = 3` is not capped (3 < 3.3), so the city may start a **fourth** founder. The memo says three. The cap is looser than the memo says.
- **Scope check:** the cap is per player, not per mean. For a player whose `sites` estimate is below 1, slack 0 blocks even a first committed founder: at `sites = 0` the test 0 ≥ 0 caps. Slack 1 let one through. This is exactly the "walking founder" allowance being removed where the estimator says zero. Early in the game (`sites` 9.3, committed under 1) it never binds.
- **Risk:** small. Where the estimator says zero but a transit or overseas site exists, the last founder is refused. The memo's falsifier (cities at T100 down by more than its SE) is the right one.

### frontier_transit_price  1.0 -> 0.0   VERDICT: WRONG (the anchor is false and the mechanism is misdescribed)

- **Unit check:** `cost += (unsigned) (frontier_transit_price * PF_TURN_FACTOR)` is added to the **extra cost (EC)** on a non-allied owner's tile (`ai/deep/deepsettler.c:179-182`). It is chained onto `adv_avoid_risks()`'s EC (:408-413). 0.0 is not a sentinel: the gate is a separate knob, and at 0 the callback adds nothing. **The memo's unit line is wrong**: "Path cost feeds `turns` and hence the site's amortize" is false. `turns = move_cost / parameter->move_rate` (`deepsite.c:2414`), and `move_cost` comes from `pf_map_iter_move_cost()`, which returns `lattice[].cost`, the **move cost only** (`common/aicore/path_finding.c:795`, `:1803`). EC is kept separately (`node->extra_cost`) and enters only the priority-queue order through `pf_total_CC()` (:172-175, :708). So the price never reaches the site valuation. It changes (a) which route is chosen (a detour raises MC, and so `turns`, only when a detour exists), (b) the order tiles are visited, which interacts with the early `break` at `deepsite.c:2531-2535`, and (c) the executor's route. **There is no "double charge of the walk" for 0 to remove.**
- **Anchor check, the orchestrator's claim 3, item by item:**
  - *Transit is PEACE-only:* true. At floor 0, `deep_frontier_transit_open()` opens DS_PEACE and DS_ARMISTICE (`deeptrunk.c:573-574`, the class map at :103 and :475).
  - *A settler is NonMil:* true. The `!utype_has_flag(UTYF_CIVILIAN) && !player_can_invade_tile()` → `MR_PEACE` test (`common/movement.c:774-776`) does not stop it.
  - **"peace → war takes 16 turns (diplhand.c:57)": FALSE.** `cancel_pact_result(DS_PEACE) = DS_WAR` (`common/player.c:79-80`), applied at once by `handle_diplomacy_cancel_pact()` (`server/plrhand.c:953`, :972). `TURNS_LEFT = 16` (`diplhand.c:57`) is set only for CLAUSE_CEASEFIRE (:570-571), where it is the **ceasefire → war** countdown, and for CLAUSE_PEACE → DS_ARMISTICE (:594-595), where it is the **armistice → peace** countdown. Both are counted down at `server/srv_main.c:970-1012`. The `turns_left = 16` at `plrhand.c:973` is written into the new DS_WAR state, and nothing ever counts it down or reads it for war. The only obstacles to cancelling are a senate (`pplayer_can_cancel_treaty`, `player.c:103ff`), which civ2civ3 Despotism and Monarchy do not have, and AI temperament. The memo's own Interactions line ("cease-fire owners revert to war in 16 turns") is the correct fact, misapplied to peace.
  - **"no hostile unit may be there": FALSE.** Peace forbids *our* military units in *their* territory. The owner's army stands on its own land by right. On the turn of a cancellation, any of its units adjacent to the settler may attack. Third parties at war with us, and barbarians from T120, are not barred either.
  - The 13.6-turn walk is quoted correctly (frontier-verdict §3, T26-50 `wturns`), but with no 16-turn lag it guards nothing.
- **Arithmetic check:** zero exposure follows only from the false 16-turn lag. On the correct rules the exposure is P(a partner cancels during the walk) × P(kill | cancellation). Neither is measured, so no value follows. The "(23/24)^(extra tiles)" de-ranking does not happen, because EC is not in `turns`.
- **Scope check:** the knob is live behind the gate, which is on. The only call site is `deepsettler.c:180`, shared by the site search and the mover (`deep_settler_apply_risk`), so 0 also changes the routes walking settlers take. `settler_fearfulness_mult` prices only units that can attack *now*, i.e. at war, so it does not cover a surprise cancellation.
- **Risk:** at 0, settlers route through peace partners' land whenever it is no longer in move cost, and they are sitting next to that partner's army when a cancellation lands. The city effect the memo expects (+0.02…+0.06) has no mechanism behind it as stated. **Drop it from the arm (keep 1.0).** Re-derive only after measuring the peace-cancellation rate in the corpus.

### cm_founder_food  0.0 -> 2.0   VERDICT: CORRECTED (-> 1.0)

- **Unit check:** `food_floor = (int) ceil(tune->cm_founder_food * upkeep);` (`ai/deep/deepcm.c:764`). It is written into `cmp.minimal_surplus[O_FOOD]` (:777-779). The arm fires only when `posture == NO_POSTURE && cm_founder_food > 0 && founder_want != 0` **and `pcity->surplus[O_FOOD] < upkeep`** (:753-763). The unit is a multiplier on founder food upkeep, as the memo says. `upkeep = utype_upkeep_cost(..., O_FOOD)` is government-dependent: Settlers `uk_food = 1` (`data/civ2civ3/units.ruleset`), but 2 under Republic and Democracy per theory §B, so k = 2 asks for a 4-food floor there.
- **Anchor check:** theory-of-play §B.1 says: "Founding costs the parent 2 citizens now ... regrows them in 10–15 turns (30 food at +2…+3)". The ruleset facts check out (`pop_cost = 2`, `granary_food_ini = 20…`, `Growth_Food 50`, so 10 food per step). **But B.1 describes the parent's regrowth *after* founding, and this knob cannot hold that:** (i) it fires only while a founder want stands and the surplus is *below* upkeep; (ii) on the next turn with surplus ≥ upkeep the arm does not fire, `deep_cm_apply_posture(NO_POSTURE)` returns FALSE (:435-436), and the city's `cm_parameter` is freed back to stock's arrangement (:790-794). The floor is a one-turn kick at the trigger, not a sustained +2. The mechanism exists for a different purpose. The veto it was built to clear is `fed = surplus[O_FOOD] >= upkeep` (`deepdefence.c:8298`, mirroring `daidomestic.c:562`), and STATUS ≈12987 states that "1.0 ... is the identity scaling of the upkeep the veto itself tests, which is the only non-arbitrary value available". The only live data (69/76 fires per game) is at 1.0.
- **Arithmetic check:** the memo says "k = 1 would only cover the settler's own upkeep", and then sets k = 2 to "implement the +2 assumption". Those two statements are inconsistent. If k = 1 nets 0 after upkeep, then a +2 net surplus needs a floor of upkeep + 2 = 3, i.e. **k = 3**, which is the value the memo rejects as unaffordable. So the stated reasoning implies 3 for a quantity the knob does not control. The mechanism's own derivation implies 1.0.
- **Scope check:** there is one call site. The posture reuses `cm_grow_*` (16/4/4) and only acts over NO_POSTURE.
- **Risk:** at 2.0, every trigger turn demands one more food than the veto needs. In size 1-3 cities that comes from shield tiles, slowing the very Settlers build the arm is meant to fund. This is the memo's own pop/city@T50 worry, made more likely for no anchored benefit. Use 1.0.

---

## Summary table

| knob | proposed | verdict | corrected value | severity |
|---|---:|---|---:|---|
| garrison_floor_danger | 0 | CONFIRMED (0 = off, not a sentinel) | — | low on units; **medium** on the size of the effect (anchor is ungated floor, other venues, no opportunity charge) |
| scale_defender | 0.5 | UNVERIFIABLE | (0.55 by its own arithmetic) | medium: the want→count step has no anchor; the ratio includes attackers and workers |
| mil_opportunity_price | 2.0 | CONFIRMED | — | low (div ≡ 0 at defaults; urgency > 0 never reaches the charge) |
| upkeep_gold_weight | 0.75 | CONFIRMED (unit and arithmetic) | — | medium: 36 per gold-turn double charge on the CALCULATE class; stock charges 0 on Temple/Amphitheater |
| founder_site_cap_slack | 0.0 | CONFIRMED | — | low (off-by-one in the gloss; blocks the last founder when `sites` < 1) |
| frontier_transit_price | 0.0 | **WRONG** | drop (keep 1.0) | **high**: peace → war is immediate, not 16 turns; EC is not in `turns` |
| cm_founder_food | 2.0 | CORRECTED | **1.0** | medium: the derivation targets post-founding regrowth the knob cannot hold, and is internally inconsistent |

## Conclusion

1. The arm will load. The inline `;` comments parse (inputfile.c:844, :747), and every double carries a decimal point, so none falls back to its default.
2. The control is exact: stacked-on.ini equals the ini's six gate lines, so the contrast is the seven knobs.
3. These go in as proposed: `garrison_floor_danger = 0`, `mil_opportunity_price = 2.0`, `founder_site_cap_slack = 0.0`, `upkeep_gold_weight = 0.75`. For the last, the §E.3 fear is unfounded: the credits are per-turn rates, so 18/24 is the right conversion. Write the CALCULATE-class double charge into its falsifier: watch Library and Marketplace picks, not only Amphitheaters.
4. `scale_defender = 0.5` may go in as a labelled guess. The arithmetic gives 0.55; the step from want to unit count is unanchored.
5. `cm_founder_food` goes in at **1.0**, not 2.0. The veto it clears is `surplus >= upkeep`, and the floor is released the moment that holds.
6. **Drop `frontier_transit_price = 0.0`.** The 16-turn lag is the ceasefire and armistice timer. Cancelling peace is immediate war (player.c:79-80), the partner's army is on that ground, and the price never entered `turns`, so there was no double charge to remove.
7. Pre-registration: summing the memo's own per-knob ranges over the surviving knobs gives about +0.23…+0.92 cities at T150. Arithmetically, +0.25…+0.70 still "follows".
8. But roughly 80% of that range is the floor term. Its anchor (−1.09 / −0.66) was measured on isolated-150 and lategame-80, before class gating, and without the opportunity charge that the control now carries. That charge already lets a 750 founder beat the floor's 1100.
9. Expect the floor's contested effect to come in below its anchor. A result of +0.10…+0.25 at T150 should be read as consistent with the mechanism, not as a falsification of it. The pre-registered lower bound is not derived from anything and should be loosened or relabelled before the 400 games, not after.
10. Bisection: H-ECON loses `frontier_transit_price` and changes `cm_founder_food` to 1.0. H-DEF is unchanged.
