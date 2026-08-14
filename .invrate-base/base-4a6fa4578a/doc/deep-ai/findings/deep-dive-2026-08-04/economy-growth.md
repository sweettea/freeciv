# Economy/Growth mechanisms in `ai/deep` — audit (reader report, verbatim)

Scope: `deephorizon.c`, `deepcm.c`, `deepwant.c`, `deeptrade.c` (+ headers), cross-read against `deepstance.h`, the call sites in `deepdefence.c`/`deepops.c`, stock `daicity.c`/`daidomestic.c`/`advbuilding.c`/`cityturn.c`/`caravan.c`, `STATUS.md`, and `findings/{growth-emergency,happiness-ladder,trade-routes,multiplier-column}.md`.

Every claim tagged **[V]** was read in the current tree; **[I]** is inference from read code (reasoning given).

---

## 1. Mechanism inventory

### Wiring / ordering (the frame everything else hangs on) **[V]**

| Hook | Call site | Contains |
|---|---|---|
| `first_activities` | `deepdefence.c:1919` `deep_do_first_activities()` | … → `deep_set_city_manager_params()` (`deepdefence.c:1936`) → `deep_manage_units()` → `deep_choose_trade_targets()` (`deepops.c:1916`) |
| `last_activities` | `deepdefence.c:3909` `deep_do_last_activities()` | `dai_clear_tech_wants` → government → policies → `dai_manage_taxes` (`:3917`) → `deep_manage_cities` (`:3919`) → `deep_balance_treasury` → `dai_manage_tech` (`:3927`) |
| inside `deep_manage_cities` | `deepdefence.c:3563` | emergencies → `building_advisor(pplayer)` (`:3639`) → **`deep_adjust_building_wants()`** (`:3640`) → per-city choose loop → `deep_city_choose_build()` (`:3388`) → `deep_want_log` (`:3520`) |

So: the CM runs at the **start** of the turn, the build ladder at the **end**, and the two never exchange a value.

### `deephorizon.c` — the growth ladder (W03)

| # | Mechanism | file:line | Decides | Inputs | Output | Tunables |
|---|---|---|---|---|---|---|
| H0 | `deep_unworked_tile_value()` | `deephorizon.c:73-142` | what a marginal citizen is worth | catchment tiles not worked & `city_can_work_tile`; `adv->{food,science,shield}_priority` | mean weighted value of best ≤3 unworked tiles; optional mean raw F/S/T vector | — (reads stock WAGs) |
| H1 | `deep_capacity_credit()` | `:150-173` | credit for `Size_Adj`/`Size_Unlimit` buildings | `city_can_grow_to(size+1)`, `city_turns_to_grow()`, H0 | `amortize(per_citizen × MORT/ttg × w, ttg)` | `horizon_capacity_weight` |
| H2 | `deep_pacification_load()` | `:184-195` | citizens spent on order | `feel[CITIZEN_UNHAPPY][FEELING_FINAL]`, entertainer-equivalent specialists | int load | — |
| H3 | `deep_content_credit()` | `:216-259` | credit for `Make_Content`/`Force_Content` | `EFT_NO_UNHAPPY`, potential bonus, H2, ttg, H0 | `amortize(freed × per_citizen × w, load>0?0:ttg)` | `horizon_content_weight` |
| H4 | `deep_growth_credit()` | `:276-307` | credit for `Growth_Food` (Granary) | pct, `city_can_grow_to`, ttg, H0 | `amortize(steps×pct/100 × per_citizen × w, ttg)` | `horizon_growth_weight` |
| H5 | `deep_output_bonus_pct()` | `:322-372` | per-output % a building adds | effect list for the improvement; `VUT_OTYPE` req read directly, others via `is_req_active` | `int pct[O_LAST]` | — |
| H6 | `deep_multiplier_credit()` | `:395-454` | credit for Marketplace/Library/Bank/Factory | H5, `city_can_grow_to`, ttg, H0 raw vector, `pplayer->economic.{tax,luxury,science}`, `adv->*_priority` | amortized credit | `horizon_multiplier_weight` |
| H7 | `deep_bundle_credit()` | `:462-509` | co-investment (Supermarket+Farmland pattern) | every effect in the ruleset; reqs containing a CITY-range improvement **and** a TILE-range extra; catchment tiles without a conflicting extra | `Σ effect->value × viable × w` (**not** amortized, **not** in tile-sum currency) | `horizon_bundle_weight` |
| H8 | threat gate | `:544-573` | whether any credit is paid at all | `deep_city_stack_survival(map, city, survival_horizon, &wave)` vs `survival_target`; fallback `urgency`/`grave_danger` | skip city unless cap-credit>0 | `survival_target`, `survival_horizon` |
| H9 | tech-wall routing | `:591-614` | pay research instead of the build queue | cap_credit>0, building not buildable now but later, unknown `VUT_ADVANCE` reqs | `+= cap_credit × w` into `def_ai_player_data(...)->tech_want[]` | `horizon_tech_weight` |
| H10 | health credit | `:634-646` | plague insurance | `game.info.illness_on`, `illness_min_size`, `EFT_HEALTH_PCT` | `health × w` (raw percent points — a third currency) | `horizon_health_weight` |
| H11 | stance gate 3 | `:648-660` | scale the whole ladder | `deep_stance_growth_mult()` | `add×gmult`, floored at `add` for threatened cities | `stance_growth_gain` |
| H12 | upkeep charge | `:661-672` | net the ruleset's gold bill | `city_improvement_upkeep()` | `add -= upkeep × w`, clamped ≥0 | `upkeep_gold_weight` |
| H13 | write-out | `:673-679` | apply | | `pcity->server.adv->building_want[id] += add` | |

### `deepcm.c` — city manager (W09-lite)

| # | Mechanism | file:line | Decides | Inputs | Output | Tunables |
|---|---|---|---|---|---|---|
| C0 | `deep_cm_base()` | `deepcm.c:67-92` | safety rails | granary vs `food_stock` | `minimal_surplus`, gold/lux/sci factors (= stock) | — |
| C1 | `deep_cm_trade_richness()` | `:104-119` | map-relative trade appetite | catchment trade & shield totals | `(1+T)/(1+S)` | consumed at `:279-287` via `cm_trade_quality_slope` |
| C2 | `deep_cm_sortie_value()` | `:126-141` | counter-attack value of garrison | `adv_unit_att_rating` of non-civilian units on the city tile | double | `sortie_defence_weight` |
| C3 | `deep_cm_survival_ok()` | `:153-165` | attrition check on grow-release | `deep_city_stack_survival` | bool | `survival_target`, `survival_horizon` |
| C4 | state machine | `:219-277` | which factor set | **threatened** (`cm_war_survival`, `:209-217`) → **capped** (`!city_can_grow_to`) → **wonder** (`is_wonder(production)`) → **grow-hard** (`size < cm_grow_below` ∧ danger paid) → else unmatched | `cm_parameter` or NULL | `cm_war_*`, `cm_cap_*`, `cm_wonder_*`, `cm_grow_*`, `cm_happy_factor`, `cm_grow_safety_margin` |
| C5 | apply/clear | `:289-306` | commit | `cm_are_parameter_equal` | writes `pcity->cm_parameter`, calls `auto_arrange_workers()` on change | — |

### `deeptrade.c` — trade destinations

| # | Mechanism | file:line | Decides | Inputs | Output | Tunables |
|---|---|---|---|---|---|---|
| T1 | `deep_caravan_reach()` | `deeptrade.c:96-104` | candidate breadth | tunable only | `FTL_*` | `caravan_foreign_trade` |
| T2 | `deep_choose_trade_targets()` | `:167-268` | destination for each idle caravan | `task == AIUNIT_NONE`, action capability, homecity, `caravan_find_best_destination()` with `ignore_transit_time=FALSE` and one discount | `dai_unit_new_task(AIUNIT_TRADE\|AIUNIT_WONDER, dest)` | `caravan_discount`, `caravan_foreign_trade` |
| T3 | `deep_brief_allies_trade()` | `:122-158` | ally chat | destinations, ETA | notify only — **purely social by design** | — |

### `deepwant.c` — want currency (W01)

| # | Mechanism | file:line | Decides | Inputs | Output |
|---|---|---|---|---|---|
| W1 | `deep_want_from_choice()` | `deepwant.c:46-59` | tier split | `DAI_WANT_BELOW_MIL_EMERGENCY` | `{tier, value}` |
| W2 | `deep_want_apply()` | `:66-73` | inverse | | writes `choice->want` |
| W3 | `deep_want_cmp()` | `:78-87` | ordering | | tier-then-value |
| W4 | `deep_want_log()` | `:94-112` | calibration corpus | `ADV_CHOICE_TRACK` producer tag | log line |

---

## 2. Unconsumed signals, and stock signals read for a different purpose

**Computed and consumed by nothing but a log:**

1. **`want_t` and the whole W01 conversion layer.** `want_t` (`deepwant.h:33`) appears in exactly one place — inside `struct deep_want` — and nowhere else in `ai/deep` **[V]** (`grep 'want_t\b'` returns only the header). W01's central deliverable, the *per-producer conversion table*, does not exist: both maps are the identity (`deepwant.c:50-57`). The tier split is real and is used in `deepdefence.c:2357-3442`, but "one currency, discounted shield-equivalent" is a declared type with no implementation.
2. **`deep_want_log`'s producer field** is `"-"` unless `ADV_CHOICE_TRACK` is compiled in (`deepwant.c:98-104`) **[V]** — so the calibration corpus W01 was supposed to generate is missing its key column in default builds.
3. **`plr_data->stance_growth_gated`** (`deephorizon.c:659`) — incremented, only ever read by `deep_stance_log()` (`deepstance.c:220`) **[V]**. That is intentional (execution census), noted for completeness.
4. **`result.value` / `result.arrival_time`** from the caravan evaluator (`deeptrade.c:227,246-253`) are used only as a `>0` gate and for logging **[V]**. The NPV number — the one genuinely well-priced quantity in the AI — never reaches any comparison: not against the alternative use of the unit, not against what the city could otherwise build, not against a second caravan.
5. **`deep_cm_trade_richness()`** *is* consumed (`:279-287`), but only for the CM. The identical question — "is this city's catchment trade-rich?" — is re-derived independently and differently inside `deep_multiplier_credit` (`deephorizon.c:429-434`, best-3-unworked raw trade) **[V]**. Two catchment trade measures, no shared answer.
6. **`deeptrade.c:48` includes `deepdefence.h` and calls nothing from it** **[V]**. A dead include is weak evidence, but it is the exact shape of "risk pricing was intended and never wired" — see §3.

**Read from stock, where stock computes it for a different purpose:**

7. **`adv->*_priority`** (`deephorizon.c:103-105, 436-447`). These are set once in `advdata.c:509-523` and are near-constants: `food 30`, `shield 17`, `gold TRADE_WEIGHTING=18`, and — critically — `science = 21.6` / `luxury = 1` whenever `adv_wants_science()` is true, which is true unless the player is on future tech or is a tech parasite (`advdata.c:511-517, 526-531`) **[V]**. The source comment says outright *"They are all WAGs."* Consequences:
   - The `deephorizon.c:390-393` comment claims "if this empire has no use for gold then the gold half of a Marketplace is correctly worth little **without that having to be asserted here**". There is no such mechanism: `gold_priority` is the constant 18 regardless of treasury; the only state-dependence is the tax rate at `:441`. **Overclaim [V]**.
   - `luxury_priority == 1` in practice, so the luxury half of every Output_Bonus building is structurally near-zero in H6 — while H3 prices happiness in tile-sum currency and `cm_happy_factor` prices it as an integer 1. **Three incompatible prices for happiness inside deep.**
8. **`pplayer->economic.{tax,luxury,science}`** (`deephorizon.c:441-447`). Deep forwards `dai_manage_taxes()` unchanged (`deepdefence.c:3917`) **[V]**; that function optimises for *solvency and celebration feasibility* (`daihand.c:200-300`), not for what growth is worth. The multiplier credit therefore reads a solvency artefact as a statement of strategic value. STATUS itself flags this at line 4453: *"sci/tax/lux from amortized tech value, `deephorizon.c:439` reads them and nothing sets them"* — accurate.
9. **`city_data->urgency` / `grave_danger`** survive as fallbacks in both files (`deephorizon.c:552-553`, `deepcm.c:216`) when the survival knob is 0. Documented, fine.

---

## 3. Missing couplings, defects, and emergent interactions

Ordered by size of prize. The first three are **defects verifiable by reading**, not preferences.

### 3.1 The capacity credit dies in exactly the city it was built for **[V]**

`deep_capacity_credit()` returns 0 unless `city_turns_to_grow() > 0` (`deephorizon.c:162-166`).

For a city sitting at its ruleset size gate:
- `city_populate()` (`cityturn.c:1126-1138`) adds surplus, sees a full box, calls `city_increase_size()`.
- `city_increase_size()` takes the capped branch (`cityturn.c:1133-1152`) and sets
  `food_stock = MIN(food_stock, granary × (10000 − aqueductloss×(100−savings))/10000)`.
- `GAME_DEFAULT_AQUEDUCTLOSS == 0` (`common/game.h:548`), and **no benchmark config sets `aqueductloss`** (grep over `benchmark/` returns nothing) **[V]**. So `food_stock == granary_size`, every turn, forever.
- `city_turns_to_grow()` (`common/city.c:2053-2058`) then returns `(granary − granary + surplus − 1)/surplus` = **0** by integer division, for any surplus ≥ 1.

So the credit is nonzero only during the food-box refill window immediately after the city first reaches the cap, and is **permanently zero thereafter**. And because `deep_adjust_building_wants` skips the whole city when `threatened && cap_credit <= 0` (`:556-573`), a long-capped city under any threat gets **no horizon credits of any kind**.

This is a **second, independent cause** of the headline evidence STATUS attributes entirely to the war gate (`STATUS.md:1640-1655`: Europe T106, 17 of 19 cities at the cap, zero Aqueducts). Fixing the war gate cannot fix this one.

The tech-wall route (H9) is gated on the same `cap_credit > 0` (`:591`), so the research pressure toward the cap-lifting advance dies with it — the exact failure mode `findings/2026-07-29` was written to close.

**Coupling that would fix it:** the trigger should be a *counterfactual* — "what would `ttg` be if the cap were lifted" — computed from the catchment and the grow-state CM factors, not from the observed `ttg` of a city whose box the ruleset is pinning full.

### 3.2 `deepcm` and `deephorizon` form a growth deadlock **[V]** + **[I]**

Independently of §3.1, the CM *manufactures* the condition under which the credit refuses to pay:

- CM cap state (`deepcm.c:226-232`) sets `cm_cap_food = 2` against `cm_cap_shield = 8`, `cm_cap_trade = 8`, and `deep_cm_base` sets `minimal_surplus[O_FOOD] = 0` once the box is full (`:77-81`) **[V]**.
- `cm.c`'s objective is `Σ surplus[o] × factor[o]` (`common/aicore/cm.c:623`) **[V]**, so the arrangement trades food away down to surplus 0 **[I — direct consequence of the objective and the floor]**.
- With surplus 0, `city_turns_to_grow()` returns `FC_INFINITY`, and `deep_capacity_credit()` bails at `:162-166` with the comment *"Not growing at all (no food surplus): the cap is not the binding constraint."*

The comment is exactly wrong in this case: the cap **is** the binding constraint, and the CM zeroed the food surplus *because* of the cap. Neither file knows the other exists — the CM runs in `first_activities`, the credit in `last_activities`, and no value passes between them.

**Prize:** this plus §3.1 is, on the reading, the whole reason the flagship W03 credit under-delivers. The mechanism is documented as landing (capacity buildings 8.9 vs baseline 3.4 per 100 cities) yet cities do not break their ceilings (Europe: max size 8→10 over 45 turns while stock ran to 22). Both are consistent with a credit that fires only in a short window after each cap.

### 3.3 Deep's credits accumulate across stock's recalculation schedule **[V]**

`deep_adjust_building_wants()` does `building_want[id] += add` (`deephorizon.c:674`) **every turn**.

But stock only *clears and recomputes* `building_want[]` when `city_data->building_turn <= game.info.turn` (`daicity.c:1985-1998` and `:2010`), and reschedules with `building_wait = fc_rand(5) + 5` (`daicity.c:2072-2079`, `AI_BA_RECALC_SPEED == 5` at `daicity.c:75`) **[V]**. On non-recalc turns the array is left as-is for any normal buildable improvement — only wonders-in-the-wrong-city and player-impossible improvements are re-zeroed each turn (`daicity.c:2007-2009, 2049-2053`).

So the credit a city actually sees is `(turns since last recalc + 1) × intended credit` — a sawtooth with a mean multiplier around **4.5×** and a peak of **9×**, phase-randomised per city.

Consequences that match the recorded evidence:
- `findings/happiness-ladder.md` §4's *"one-sided inflation of a shared auction"* — deep at 39.6 happiness buildings/100c against baseline's 4.0, with the conclusion *"the placeholder weight of 1.0 is very probably too hot"*. An unintended 1–9× multiplier is a simpler explanation than a hot weight.
- `STATUS.md:2020` — the ablation where *zeroing the seven horizon credits raised the Settlers share from ~18% to ~50%*. Buildings accumulate; the settler want does not (it is recomputed on its own `founder_turn` schedule and multiplied, not summed — `deepdefence.c:3700-3740`).
- Every credit-ON/OFF A/B in STATUS measured the credit convolved with a random phase, which inflates within-arm variance and is a candidate explanation for *"three credits in a row whose mechanism lands decisively and whose score does not"* (`STATUS.md:1957`).

Note the saturation path: `domestic_advisor_choose_build` clips the winner at `DAI_WANT_DOMESTIC_MAX = 750.25` (`daidomestic.c:630-634`) **[V]**, so once accumulated building want saturates, buildings beat every non-emergency military want and the *magnitude* information is discarded.

### 3.4 Two thresholds for "will this city hold", used by two halves of the same decision **[V]**

`deephorizon.c:544-550` uses `survival_target`; `deepcm.c:209-214` uses `cm_war_survival`. The split is argued at length in `deepcm.c:186-208` and the argument is good *for the CM vs the defence layer*. But it means the **build queue** and the **citizen arrangement** of the same city, in the same turn, can disagree about whether it is at war: the CM runs grow-hard (food 16) while the horizon refuses every growth credit, or vice versa. Nothing reconciles them.

**Emergent behaviour if coupled:** one `threatened` verdict per city per turn, computed once and read by both, would make "this city is investing" a coherent state rather than two independent coin flips. Cheap; removes a variance source from every ECON tuning run.

### 3.5 The credit prices the marginal citizen with one set of weights; the CM places it with another **[V]**

`deep_unworked_tile_value()` scores tiles with `adv->{food,science,shield}_priority` = `30/21.6/17` (`deephorizon.c:103-105`). The citizen it is pricing will actually be placed by `cm.c` using `tune->cm_*` factors — e.g. grow-hard `16/4/4`, capped `2/8/8` (`deepcm.c:222-273`). The credit therefore imagines a citizen on a food tile while the CM puts it on a mine, or the reverse. This is RC4 reproduced *inside* `ai/deep`, between two mechanisms this project wrote.

**Coupling:** pass the city's live `cm_parameter` factors into `deep_unworked_tile_value()` and fall back to `adv->*_priority` only for unmatched cities. Then "what a new citizen earns" is what a new citizen will actually earn.

### 3.6 `deepcm` has no state for the empire's most productive cities **[V]**

The state machine's last branch requires `city_size_get(pcity) < cm_grow_below` (placeholder 6, `deeptuning.c:116`). A city that is size ≥ 6, not capped, not building a wonder and not threatened falls to `matched = FALSE` (`deepcm.c:276`) and gets its `cm_parameter` **cleared** — back to `set_default_city_manager()` with `factor[O_TRADE] = 0`, `happy_factor = 0`, `factor[O_SHIELD] = 5` (`cityturn.c:330-336`) **[V]**.

So the cities with the most trade to arrange are precisely the ones still running the hardcoded factors whose comment is quoted as RC5's evidence. The W09 acceptance criterion ("cities under urgency show ≥15% higher shield output") is also now measured on a much smaller population than when it was written, because the `cm_war_survival` change made `threatened` rare — a stale acceptance metric (see §5).

**Prize:** a "developed city" state is the single largest uncovered population in the CM, and `cm_trade_quality_slope` (already built, `:279-287`) only applies to matched cities, so the trade-richness mechanism is switched off for exactly the trade-rich cities.

### 3.7 No coupling between the treasury and the city manager **[V]**

`deep_balance_treasury()` (`deepdefence.c:~3860-3907`) sells buildings and disbands units when the projected balance goes negative — and `STATUS.md:708-718` records deep *bankrupting itself*, gold ratio 0.13–0.55 vs stock. Meanwhile `deep_cm_base()` hardcodes `factor[O_GOLD] = 2` and `minimal_surplus[O_GOLD] = -FC_INFINITY` (`deepcm.c:84,89`) in **every** state **[V]**. The CM is never told the empire is going broke.

**Emergent behaviour if coupled:** a solvency state (gold factor up, trade up, `minimal_surplus[O_GOLD]` raised toward 0) would let deep arrange its way out of a deficit instead of selling the buildings its own horizon credits just bought. Given that H12 exists solely because of this bankruptcy, this is the missing half of the same fix.

### 3.8 Trade: the destination policy improved, and nothing told the build queue **[V]**

Deep now picks long, foreign destinations with one honest transit convention (`deeptrade.c:204-225`). The *want* that decides whether to build a caravan at all is still stock's `dai_choose_trade_route()`, whose income estimate under `CBS_CLASSIC` is

```c
income = (10 + 10) * (1.75 * pcity->surplus[O_TRADE]) / 24;   /* daidomestic.c:360 */
```

— a hardcoded distance of 10 and **only the source city's** trade, with no destination at all **[V]**. Deep raised the realised value of a caravan and left the price unchanged, so caravans are under-built exactly in proportion to how much better the destinations got. `findings/trade-routes.md` §5.3 anticipated this and deferred it behind the arbitration work; that sequencing argument is still sound, but the asymmetry should be recorded as a live cost, not a neutral deferral.

**Coupling with a size argument:** run one `caravan_find_best_destination()` per city per recalc window and feed `result.value` into the want. The windfall is `distance × (both endpoints' trade surplus) / 24`, doubled for science+gold (`traderoutes.c:478`, `caravan.c:275`), so the priced-vs-realised gap grows with map size — largest on precisely the `long` and `primary-120` cells the project uses for headline numbers.

### 3.9 Trade valuation knows nothing about war risk or escort **[V]**

`caravan.c` prices the *route*, including `account_for_broken_routes`, but nothing prices the **caravan being killed en route**. Deep owns `deep_city_stack_survival` / `deepodds.c` and applies danger discounts to workers (`worker_danger_discount`, `worker_flee_radius`) and settlers (`settler_fearfulness_mult`) **[V]** — but a caravan is dispatched across an arbitrary distance with no danger term, no escort, and no guard assignment. `deeptrade.c` even includes `deepdefence.h` and uses nothing from it.

`deep_caravan_reach()` is also a **global** policy (`:96-104`): one `FTL_*` level for the whole player, so "war risk against route value, which is the same trade-off `deep_op_*` makes for targets" (`findings/trade-routes.md` §5.2) is still unimplemented — the coin flip was replaced by a constant, not by merit.

### 3.10 The growth ladder and the stance gates **[V]**

Gate 3 is wired (`deephorizon.c:525, 648-660`) and gate 4 scales `founder_want` (`deepdefence.c:3721-3740`). Two gaps:

- **Trade has no gate.** An econ-weighted player does not look further for routes, value them more, or build more caravans. `caravan_*` sits in the G/ECON blocks but no stance multiplier touches it.
- **The CM has no gate.** `cm_grow_food` vs `cm_war_shield` is the *sharpest* econ/mil trade-off in the codebase — it is literally food versus shields, tile by tile — and `deep_stance_growth_mult()` is not read in `deepcm.c` at all. The stance layer gates what the empire *buys* and not what it *works*. Given `deepcm.c:266-270`'s own argument ("civ2civ3 has no rapture, so the CM food weight is the only growth lever there is"), this is the highest-leverage ungated seam.

### 3.11 Currency incoherence *within* `deephorizon.c` **[V]**

Four of the credits are amortized tile-sum values; two are not:
- H7 bundle: `effect->value × viable_tiles × weight`, no amortize, no per-citizen conversion (`:503`).
- H10 health: raw `EFT_HEALTH_PCT` percent points × weight (`:644`).
- H12 upkeep: **one turn** of gold subtracted from a **MORT-horizon** amortized value (`:669-670`). Dimensionally wrong; `upkeep_gold_weight` absorbs the scale but not the fact that the correction is independent of horizon and of the building's own credit magnitude.

Also: `upkeep_gold_weight` is registered in the **S and MIL** blocks (`tuning-blocks.ini:16,50`) but not in G/ECON **[V]** — while it directly subtracts from every ECON credit. That is the block-assignment error `STATUS.md:1875-1880` identified for `survival_target`, still live for a different knob.

### 3.12 H7 `deep_bundle_credit` does not check that the partner extra is reachable **[V]**

`viable` counts a tile if it *has* the extra **or** merely has no conflicting extra (`:496-499`). It does not ask whether the player has the tech for the extra, whether a worker could ever build it, or whether the city works those tiles. On a typical catchment nearly every tile qualifies, so the credit is ≈ `effect_value × 20 × weight` for any bundle-shaped effect, regardless of whether the bundle can ever be completed. W03's own design defers the worker-side pairing to W30 ("recorded, not silently dropped") — but the *credit* was not deferred with it, so deep pays for half a bundle it has no mechanism to finish.

### 3.13 `caravan_discount = 1.0` is inside the registered tuner range and is illegal **[V]**

`deeptrade.c:179-182` rejects `discount <= 0.0 || discount > 1.0` — so **exactly 1.0 is accepted**. `caravan_parameter_is_legal()` rejects it (`caravan.c:85-88`: *"infinite horizon with no discount gives infinite reward"*), and `perpetuity(payment, 1.0)` is `payment / 0.0` (`caravan.c:437-439`). `tuning-params.ini:55` registers the range as `0.95, 0.80, 1.0` — the tuner can draw the endpoint, at which point every destination scores `inf` (or `NaN`) and destination choice becomes arbitrary. Deep's guard should call `caravan_parameter_is_legal()` rather than re-derive it.

### 3.14 Observability gap in the tuning log **[V]**

The `deep-tuning` line (`deeptuning.c:302-380`) omits `horizon_multiplier_weight`, `caravan_discount`, `caravan_foreign_trade` and the `site_travel_*` family, though all are registered and tunable. Given `deepstance.c:105-129`'s own warning — *"a switch nobody verified has already cost this project a 40-game batch"* — a knob that cannot be confirmed loaded from the log the project greps is a live hazard for exactly the three economy mechanisms most recently landed.

### 3.15 Redundant catchment work (performance, not threading)

`deep_unworked_tile_value()` is **independent of `pimprove`** yet is called from H1, H3, H4 and H6 — i.e. up to 4 full catchment scans **per city per improvement** (`deephorizon.c:169, 253, 304, 429`) **[V]**. H7 additionally walks every effect in the ruleset per (city, improvement) pair (`:470-506`). For an 80-city empire and ~70 improvements this is order 10⁵ catchment scans and order 10⁶ requirement-vector walks per player-turn **[I — arithmetic on the loop bounds]**. Hoisting the per-city value out of `improvement_iterate` is behaviour-identical and free.

---

## 4. Intelligence gaps, ranked

**1. Anticipate the ceiling instead of reacting to it. (RC1)**
A human starts the Aqueduct so it *completes* as the city hits size 8; deep only begins to want it after the cap binds, and then (§3.1) stops wanting it. Nothing anywhere projects "this city will be capped in N turns". Highest ranked because it is RC1's own headline example, the machinery to fix it already exists (`city_turns_to_grow` + the catchment), and the two defects blocking it are provable by reading rather than by benchmark. It also subsumes `findings/growth-emergency.md`'s proposal: with a live cap credit, the growth-emergency buy override has something to act on. **Expected: the largest single move in this domain, and the cheapest.**

**2. Cities have roles; the empire allocates between them. (RC2, RC4)**
A human designates a settler pump, a wonder city, a science city, and *deliberately keeps one small and one large*. Deep has exactly one empire-level notion — `adv->wonder_city`, chosen by stock on raw shield surplus (`advbuilding.c:189-260`) — and every other decision is a city bidding alone from local state. The CM state machine is the closest thing to a role system and derives entirely from local size/threat/production. Ranked second because it is the structural ceiling on everything else: five per-column credits landing mechanism and not score (`STATUS.md:1957`) is what an unarbitrated auction looks like, and §3.3's accumulation is only possible because no one owns the total.

**3. Tax and luxury rates as strategic levers, including celebration. (RC5, RC4)**
Deep forwards `dai_manage_taxes()` untouched and never touches rates itself **[V]**. A human raises luxury to celebrate (rapture growth in the classic cells the suite runs), drops science to bank gold for a rush-buy, or runs 100% science behind a Library wall. `ai/deep` contains **zero** references to celebration or rapture **[V]**. The CM never sets `require_happy` even though `dai_manage_taxes` builds a `require_happy` parameter to test feasibility (`daihand.c:255-260`). Ranked third because it is a real lever the AI has and never pulls, and because H6 already *reads* the rates as if they meant something (§2.9).

**4. Trade as a program, not as a disposal route for idle units. (RC1, RC2)**
A human grows two cities *in order to* run a route between them, times caravans to arrive at a wonder, and treats the windfall as a planned tech. Deep improved destination choice and nothing else: no want, no risk, no escort, no sequencing (§3.8, §3.9). Ranked fourth rather than higher only because `findings/trade-routes.md` §6's sequencing argument is right — adding a sixth claimant to an unarbitrated build queue before gap 2 is fixed will read as a regression.

**5. Corruption, waste and distance from the capital. (RC3)**
Nothing in these four files prices distance-to-capital, Courthouse, or the fact that a far city keeps a fraction of what it makes — and civ2civ3 charges heavily for it. H0 values a marginal citizen's raw tile output identically in the capital and twelve tiles away. This is RC3 in the economy rather than the map: the empire's world model has no *shape*, only a set of independent catchments.

**6. Wonder economics. (RC4)**
Wonder city by shield surplus; no valuation of *which* wonder, no caravan-feeding plan, no denial reasoning beyond stock's flat `×1.5`. The CM has a wonder state (`deepcm.c:233-240`) that arranges tiles for a wonder nobody chose on merit.

**7. Moving population between cities.** Join City / food caravans / migration-aware growth. `city_can_grow_to` gates migration (W03-before-W17 in the phasing), and nothing here participates.

Out of scope, one line each: **threading** — §3.15's cost is a hoisting problem, not a parallelism one; **honest information** — H0/H6 and `deeptrade.c:225`'s `!has_handicap(pplayer, H_MAP)` all read the true map, RC6's problem; **ally conversation** — `deep_brief_allies_trade()` is correctly self-declared as purely social.

---

## 5. Stale-plan notes (code contradicts STATUS/findings)

1. **`findings/growth-emergency.md` §2: "The want half already exists … Nothing more is needed on the want side."** Not true as written. The want half is zero in the steady-state capped city (§3.1) — an emergency buy override triggered on a want that is structurally zero would fire never. The buy-side spec is sound; its stated precondition is not.
2. **`STATUS.md:1640-1660` attributes the Europe "zero Aqueducts across 85 cities" entirely to the wartime credit gate.** The gate is one cause; `ttg == 0` in a capped city (§3.1) and the CM's own food-shedding (§3.2) are two more, and neither is addressed by the gate fix that shipped.
3. **`findings/happiness-ladder.md` §5 and `STATUS.md:1789-1799`: "39.6 per 100 cities … says the placeholder weight of 1.0 is very probably too hot."** The `+=`-into-a-stale-array accumulation (§3.3) is an unpriced 1–9× multiplier on every horizon credit and is a simpler explanation. Any tuning pass on `horizon_*` before §3.3 is fixed will be tuning against the sawtooth.
4. **`STATUS.md:1948-1953`: "The column closed entirely on the SCIENCE side — the `adv->*_priority` routing declining to buy gold multipliers for an empire with no use for gold."** No such test exists in the code. `gold_priority` is the constant `TRADE_WEIGHTING = 18` (`advdata.c:518`) and the gold term at `deephorizon.c:440-441` is nonzero whenever `economic.tax > 0`. Marketplace being bit-identical between arms means it lost the auction, not that its credit was zero — a different diagnosis with a different fix.
5. **`deephorizon.c:390-393`'s own comment** makes the same claim as (4) about `adv->*_priority` doing the work. Overclaim; the state-dependence comes from the tax rate, which is set for solvency (§2.9).
6. **`workstreams/W01-want-unification.md` Design items 2 and 4** (per-producer boundary conversion table; traits multiply `want_t` only) are **not built**. `want_t` is used nowhere (§2.1); traits still multiply the native scale (`daidomestic.c:620` multiplies deep's credits by `TRAIT_BUILDER`; `daicity.c:2038-2042` divides stock's by `TRAIT_EXPANSIONIST`, and deep's credit escapes that division entirely). The W01 acceptance line "no `DAI_WANT_*` magnitude comparisons left in `ai/deep`" holds; the currency itself does not exist.
7. **`STATUS.md:367-395`, the W09-lite acceptance ("cities under urgency show ≥15% higher shield output", measured +3.4%)** is now measured on a population redefined by the later `cm_war_survival` commit (`STATUS.md:1866-1890`), which the same document says shrank `threatened` from 69% of cities to a small fraction. The number and the criterion are no longer comparable and the criterion should be restated against the new gate.
8. **`STATUS.md:1866-1880`'s block-assignment lesson** applies unfixed to two more knobs: `survival_target` still gates the **horizon** threat test (`deephorizon.c:544`) from the MIL block, and `upkeep_gold_weight` subtracts from every ECON credit from the MIL/S blocks (§3.11).
9. **`STATUS.md:1836-1840` tunables audit** covers struct/default/loader/registry/block — it does **not** cover the `deep-tuning` log line, where four economy knobs are missing (§3.14).

---

### Two smallest-diff, highest-confidence items

If only two things are done here, §3.3 (hoist deep's credit out of `+=` on stock's stale array — e.g. keep a deep-owned per-turn credit array, or only add on stock's recalc turns) and §3.1 (trigger the capacity credit on a counterfactual `ttg` rather than the observed one) are both provable by reading, both small, and both invalidate the interpretation of several existing A/B results rather than merely adding a new mechanism.
