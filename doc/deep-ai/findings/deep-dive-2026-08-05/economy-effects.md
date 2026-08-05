# City/economy mechanics + effects coverage (reader report, verbatim)

## Part A — Effects coverage

**Counts (script-verified):** 149 effect types exist; 107 appear in our two rulesets. Valued nonzero by `dai_effect_value`: 49. Real `base_want` coverage: 11. **Claimed base_want-covered but is not: 8.** **Dark — returns 0 in all three venues (buildings `daicity.c:1755`, techs `daitech.c:309`, governments `daidata.c:583`): 39.** ai/deep adds zero new effect types (its H-credits re-price effects stock already values).

### A1. The structural finding

`base_want` (`daicity.c:1521-1568`) — the only simulation-based valuation — runs only if `impr_calc != ADV_IMPR_ESTIMATE`, and that flag comes from a hardcoded 11-effect switch at `advdata.c:89-138`.
(a) **Eight effects are claimed covered (`daieffects.c:176-201`) and are not**: GOV_CENTER, OUTPUT_WASTE, OUTPUT_WASTE_BY_DISTANCE, SPECIALIST_OUTPUT, TECH_UPKEEP_FREE, IRRIGATION_PCT, MINING_PCT, OUTPUT_TILE_PUNISH_PCT. Most survive by co-occurring with a trigger; GOV_CENTER does not.
(b) `impr_range` from the improvement's own req range: a City-range building with empire-wide benefit is measured at 1/N.
(c) `base_want` never re-runs the CM — "citizens stop entertaining and start working" value is invisible.

### A2. Dark effects carrying real money

1. **`EFT_GOV_CENTER` — civ2civ3 Ecclesiastical Palace** (140 shields, Theology). Waste scales by distance to nearest gov center (2%/tile trade pre-Corporation, 2%/tile shields pre-Trade, 1%/2 tiles food); a Republic city 12 tiles out loses 49% of trade; an EP 2 tiles away makes it 29%. GOV_CENTER returns 0; not a trigger; City-range reqs → base_want sees one city. Also: base_want returns 0 for owned small wonders (`daicity.c:1533-1537`) so **the Palace can never be relocated on merit**; post-capture the server picks a uniformly random survivor (`citytools.c:1037`, in-tree FIXME).
2. **`EFT_UNIT_UPKEEP_FREE_PER_CITY` — King Richard's Crusade (2 gold/city) and Statue of Zeus (1 shield/city), Player range.** At 20 cities: 40 gold + 20 shields per turn forever. Both `impr_calc == ESTIMATE` → base_want 0; **their entire want is HISTORY×5 (15 and 10) plus a MAKE_CONTENT_MIL WAG.** MinSize free-food-upkeep ladder also dark.
3. **`EFT_MAX_RATES`** — Despotism 60 → Democracy 90 ceilings; `dai_gov_value` does not re-optimize rates (in-source admissions `daidata.c:547-548`, `advdata.c:1031-1032`); effect returns 0; H_RATES set at EVERY difficulty. The AI is bound by a ceiling it cannot price.
4. Minor: HAS_SENATE (UN/Manhattan grant, Apollo/SoL suppress); **`UNIT_RECOVER` is classic-UN's ONLY effect → classic's UN has literally zero want** — the one fully-dark building.

## Part B — Mechanics, filtered by what is actually live

| Mechanic | Live? | Verdict |
|---|---|---|
| Trade routes | enabled, empirically zero (0 routes, 58-city T251 save) | **DEAD BRANCH — B1** |
| Disasters | **ACTIVE ALL CELLS, 10× on civ2civ3** (`disasters=100`) | **not modelled at all** |
| Illness | civ2civ3, firing hard (20/58 cities plagued T251) | on merit (stock + deep) |
| Celebration payoff | active all cells | **UNREACHABLE on civ2civ3 — B2** |
| Rapture growth | classic only | stock branch can fire there |
| Worklists | n/a | not used; see negative result below |
| Rush-buy | n/a | greedy by want, **never want-per-gold** (`daicity.c:560-568`, `deepdefence.c:3314-3482`); unit buy cost quadratic → topping up ~4× cheaper per shield, unexploited |
| Selling buildings | n/a | emergency-only; stock sells first non-defence building in random city order; deep ranks upkeep/cost but only when insolvent. **Nothing sells a bad investment while solvent** |
| Specialist type | n/a | CM searches per type (`cm.c:1029-1051`) but every AI pins gold factor == science factor == 2 → taxman-vs-scientist decided by lattice order. `allow_specialists` read by nothing (upstream candidate) |
| Coinage | n/a | hardcoded 1.8, skips base_want; deep never references (HEDGE owns) |
| Pollution/warming | empirically dead every sampled save | moot |
| Spaceship | never reached; **199/199 games end on the turn limit** | moot |
| Airlift | 1 Airport ever observed | moot ("totally braindead" want) |
| Civil war | no evidence it fires | moot |
| Migration | ON in 5 cells; neither AI models (W17); EFT_MIGRATION_PCT in no ruleset | recorded |
| Borders | active | stock: prohibition only; deep's site_claim_bonus placeholder is the only credit |

### B1. Trade routes: a provable dead branch in every civ2civ3 cell

`dai_choose_trade_route` (`daidomestic.c:172-327`): the national scan is gated on national pct > 0 → skipped; `prefer_different_cont = (NATIONAL_IC > NATIONAL)` = `0 > 0` = FALSE; **the only foreign scan is gated on `prefer_different_cont`** → also skipped; `return` with no want. **The function returns without producing any want in 100% of civ2civ3 games** — the foreign scan's guard compares two NATIONAL percentages and has nothing to do with whether foreign routes pay. Same family as the rapture finding. Upstream-bugs candidate. (Deep's R2 merit path does NOT share this guard — it evaluates via `caravan_find_best_destination` — so deep-with-trade_merit CAN want foreign routes once peace exists; stock cannot ever.)

### B2. Celebration: payoff without a trigger

Stock's celebration branch is gated on `EFT_RAPTURE_GROW > 0`; civ2civ3 defines none. But civ2civ3 pays for celebration anyway: `Output_Inc_Tile_Celebrate` +1 trade/tile under Monarchy/Federation/Republic/Democracy, and celebration lifts the despotic `Output_Penalty_Tile`. Both dark. `deeprates.c:75-98` defers celebration to a branch that never fires. **No sampled civ2civ3 save had any city with rapture > 0 — it never happens even by accident.**

### B3. Disasters (promoted by the settings audit)

civ2civ3 `disasters = 100` (10× default). Earthquake (Hills city: loses entire shield stock), Flood (river: food stock), Fire (size ≥9: building), Industrial/Nuclear Accident (**building-induced**: pop loss), Robbery. Nothing anywhere models any of it; disaster risk is a ruleset table, not an effect, so no valuation venue exists. Mfg./Nuclear Plant each buy an unpriced ~0.1%/turn pop-loss risk; Earthquake interacts with rush-buy timing and Hills production centres site selection prefers.

## Ranked opportunities

1. **Empire-scope the gov-center value** (EP + Palace relocation) — new deephorizon credit, distance-weighted waste-saved sum, one tunable. Large, 26 civ2civ3 cells.
2. **Price Player-range effects at `c` cities** (KRC/SoZ concrete) — `cities[REQ_RANGE_PLAYER]` already computed and passed as `c`. Cheapest structural win.
3. **Buy on gold-per-shield** — rank the greedy buy loop by want/buycost; quadratic unit curve makes top-ups ~4× cheaper. `deep_spend_gold`. Keep emergencies separate.
4. **Specialist type as a decision** — derive gold/science CM factors from treasury/research state; two tunables; smallest diff on the list. File `allow_specialists` upstream.
5. **MAX_RATES in government choice** — closed-form ceiling term in deep's gov_value wrapper (stock's own comment says re-optimizing is too expensive).
6. **Celebration as an economic decision on civ2civ3** — payoff exists, trigger missing; rates × CM × government coupling; sequence after 1-4.
7. **Building-induced disaster risk** — small, wholly unpriced, needs a new hook.
8. **Revive trade routes** — fix the guard in deep, file upstream; gated on peace (diplomacy sweep).
— History on turns-remaining horizon: flag as partial metric-gaming (culture/50; civ2civ3 38 buildings, classic none). Report, don't lead.
— **Worklists: negative result, do not build** — `city_change_production_penalty` EXEMPTS AI unless H_PRODCHGPEN, which hard does not set (`citytools.c:3221`, `difficulty.c:110-131`); all cells hard. Note: the want is still docked `shield_stock × SHIELD_WEIGHTING/15` (`daicity.c:1909`) — a PHANTOM cost worth deleting in deep.
— Pollution/warming/spaceship/airlift/civil-war/migration: dead or off everywhere. Facts, not opportunities.

**Cross-ref:** HEDGE's **penalty-avoidance leg hits its own stop condition** — the switch penalty is not charged to AI players at hard, so that leg is worth zero as benchmarked; its Coinage and disband-salvage legs survive. Rush-buy policy and selling are NOT covered by HEDGE — #3 is free ground.

**Stale-plan flags:** CLAUDE.md/00-context "cm_parameter never set by the AI" → true statement is "never set by ai/default". `daieffects.c:176-201`'s covered-list wrong for 8 of 25.
