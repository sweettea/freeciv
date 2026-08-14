# Diplomacy & game-state surface: what no AI exploits (reader report, verbatim)

Baseline `0896b34`; all line references read in the working tree today and matched. `ai/deep` forwards **every** diplomacy hook to `ai/default` unchanged (`deepai.c:1131-1138, 1153`); deep's only original diplomacy code is the purely social war-countdown retraction. So "stock" and "deep" are the same AI for this entire domain.

## 1. Treaty clause coverage

**Enablement:** 10 of 11 clause types enabled in both rulesets, no reqs; `CLAUSE_SHARED_TILES` has no section in either → disabled in every cell (confirmed: 0 of 8,900 diplstate rows).

| Clause | Priced? | Ever PROPOSED? | Observed |
|---|---|---|---|
| ADVANCE | `daidiplomacy.c:315-357` | only inside `dai_share()` — reached only from DS_TEAM / DS_ALLIANCE+love>500 (`:1832,:1848-1852`) | allies only |
| GOLD | `:460-466` (diplgoldcost default 0) | only as balance in `suggest_tech_exchange` (`:1214-1216`) | allies only |
| CEASEFIRE | `:437-450` | `:994`, `:1946` | yes |
| PEACE | `:395-417` | `:1929` | yes |
| ALLIANCE | `:418-436` | `:1912` | yes |
| VISION | give: 0 if allied else −BIG; **receive: 0 "we are omniscient"** (`:545-564`) | `dai_share` only | 18.4% of met pairs (contested) |
| EMBASSY | give: `-5×turn` at peace; receive: `tech_leak_pct/10` = **10 gold** (`:566-584`) | `dai_share` only | 5-81%, mostly via diplomats |
| MAP | `:488-509` "Useless to us - we're omniscient!" | **never** | never |
| SEAMAP | `:468-486` same | **never** | never |
| CITY | `:511-543` (buy at `city_gold_worth`, sell at **15×**, capital −BIG) | **never** | never |
| SHARED_TILES | `:585-598` | **never** | ruleset-disabled |

`dai_diplomacy_suggest()` call sites exhaustive: `:994,:1211,:1212,:1214,:1216,:1265,:1270,:1279,:1286,:1290,:1293,:1912,:1929,:1946`. **Four enabled clauses (MAP, SEAMAP, CITY, SHARED_TILES) are priced and never proposed by anyone.**

### 1a. The embassy swap is arithmetically impossible — in an Embassies-leakage ruleset

civ2civ3 `tech_leakage = "Embassies"` (`game.ruleset:355`), `tech_leak_pct` 100. Receiving an embassy: worth 10 gold flat forever; giving one at DS_PEACE: `-5 × turn`, growing without bound. Mutual swap nets `10 − 5T` each — non-positive from turn 2. Moot anyway: `dai_share()` (the only offer path) is alliance/team-gated. The AI compensates by sending diplomats; **deep pays units and turns for a thing a free treaty clause would deliver, because the treaty valuation forbids it** (deep's `embassy_science_value` want at `deepdefence.c:2666-2717`).

### 1b. Tech economics

`dai_goldequiv_tech` (`:158-179`) = `3 × goal bulbs + tech_want/turn`, halved if prereqs known, halved between allies, zero between teammates. Consequences: **tech-for-gold is structurally unavailable** (the gold-balance test only passes for near-equal techs — the AI can never simply BUY a tech); **two AIs at peace never trade a single tech** (alliance perk only). The tech-leak term (`:239-258`) is the AI's most sophisticated third-party reasoning anywhere: raises price by ¼ of what the buyer's allies gain; vetoes if the buyer is allied to someone dangerous.

### 1c. Unpriced/unproposed — prize and risk

- CITY sell: 15× markup means a net-negative frontier city is never sold. Needs deep's own city-value model; exploit-surface risk vs humans; low priority.
- CITY buy: both sides refuse at 15× — dead in AI-only cells unless both change.
- MAP/SEAMAP: **W04 tripwire** — three valuations hardcoded on omniscience (`:470,:490,:562`); an honest AI would refuse free maps. Checklist item.
- EMBASSY: the `-5×turn` giver cost is invented (rule-7 shaped); replace with a real "what do they learn" term → tuning entry.
- VISION receive-side 0: omniscience artifact; give-side `shared_vision_is_safe()` (`:185-205`) is careful — keep.

## 2. War/peace machinery

- Contact starts at war (`plrhand.c:2286-2300`).
- **A hard AI can never open with a cease-fire**: `H_CEASEFIRE` is NOVICE/EASY/NORMAL only; without it the branch needs `worth > 0`, and the ceasefire valuation returns exactly 0 while `turns_since_contact < TURNS_BEFORE_TARGET` — always true at first contact. (Upstream bug candidate.)
- Ceasefire runs 16 turns then lapses to WAR (`diplhand.c:57`, `srv_main.c:986-1009`).
- **The 4-vs-3-to-6 collision**: peace proposals refused while `turns_left > 4` (`:405-411`); the AI-AI anti-spam gate blocks proposals for `fc_rand(4)+3` turns (`:1814,:1822-1827`). The window and the gate are the same size.
- Peace price: needs **love ≥ 125 both sides**; love +15/turn, 4%/turn decay — but `best_target` LOSES up to 50/turn (`:1135-1141`). In 2-player cells the only other player is always best_target → peace never.
- Armistice: `case DS_ARMISTICE: break;` — the AI does nothing at all for the 16 turns after signing peace.
- Casus belli: read as an excuse, never earned deliberately; `EFT_CASUS_BELLI_*` in daieffects' "not supported" list.
- Treaty-breaking costs (senate + love) are cheap; at hard, anarchy is 0 turns (no `H_REVOLUTION`) so the senate is bypassable for free.
- War weariness: `dai_assess_military_unhappiness` returns a bool per city; `EFT_UNHAPPY_FACTOR` unsupported. The AI never models war exhaustion — own or enemy's — as a reason to end a war.

### 2a. Measured (800+ final saves, per-cell diplstate census)

| cell | met pairs | end War | end Peace | end Alliance | peaked at Cease-fire | embassy | vision |
|---|---|---|---|---|---|---|---|
| contested | 2228 | 72.6% | 3.8% | 22.6% | **27.8%** | 36.8% | 18.4% |
| cramped | 625 | 91.5% | 2.9% | 5.3% | 19.7% | 25.8% | 1.3% |
| isolated | 925 | 89.2% | 2.8% | 6.8% | 26.8% | 22.4% | 4.1% |
| isolated-150 | 768 | 90.2% | 0.9% | 0.4% | 24.7% | 5.2% | 0.0% |
| lategame | 1290 | 80.9% | 3.0% | 13.7% | **41.2%** | 34.7% | 10.9% |
| lategame-80 | 1242 | 79.6% | 3.5% | 13.2% | **42.0%** | 30.1% | 8.9% |
| long | 982 | 87.8% | 2.1% | 8.8% | 23.1% | 31.5% | 6.0% |
| smoke | 578 | 74.0% | 2.6% | 15.4% | 25.3% | 30.3% | 8.5% |
| garden2/2b | 60/99 | 100/87.9% | 0% | 0/2.0% | **66.7/89.9%** | 75-81% | 0% |

**20-42% of every pair that ever stopped shooting reached a cease-fire and never got past it.** Diplomacy is ON in every cell (DIPLO_FOR_ALL) at hard (no handicaps) — this is the full-strength algorithm.

## 3. Third-party awareness

Exists in exactly four channels (all stock; deep adds none): join-ally's-war (`:1733-1747`); discount war want by own treaties broken (`:886-908`, FIXME admits no chain reactions); enemy-of-my-enemy love bonus (`:1066-1071`); hard veto on pacts with my enemy's allies (`:367-392`).

Does NOT exist: dogpiling a distracted target; counting a target's existing enemies (war-desire inputs are exhaustively target's own assets + distance + love); peace-broking; noticing a gang-up on me. **`tech_leader` and `production_leader` are computed every turn (`advdata.c:486-500`) and read by nothing** — the same shape as WIN_CAPITAL, equally W04-independent. The phase loop produces three single pointers, each overwritten by the last match — it structurally cannot count.

## 4. Barbarians, huts, animals

- **Leader ransom**: executes but never valued — the want is the leader's 40 build shields; the ~100-gold ransom (`ransom_gold=100` all rulesets) is invisible. Barbarian AI defends the asset; nobody hunts it. Small stakes in our cells (≤1 leader/game). Correctness item.
- **Hut EV**: ruleset Lua table (default.lua:145-173: EV ≈ 19.6g + 21.4% tech + 14.3% unit + 7.1% city + 7.1% barbarians); stock uses two ruleset-blind constants (HUT_SCORE 60902 with FIXME; RAMPAGE 99998); deep's `explore_hut_value` 15.0 PLACEHOLDER is the only code modelling hut→barb risk. Nobody derives the constant from the table, or moves it for H_LIMITEDHUTS or density.
- **Uprising geometry**: barbarian spawns ring 5-8 tiles from a city, probability ∝ OWN CITY COUNT × (rate−1) (`barbarian.c:580-583`). ZERO reads of barbarianrate/onset in ai/default or advisors; deep's flat `barbarian_fear=30` is the only code past onset, same for 1×/2×/3× rates. "My Nth city buys barbarian pressure" priced by nobody.
- **Animals**: civ2civ3's Storm (att 0, def 9, ocean) runs the stock barbarian short-circuits — terrain-locked city-incapable Storms ordered to march on cities every turn; stack_cost prices def-9 Storm at 10 shields. Storms decay to ~0 by end; small.

## 5. Game-state levers

- **Revolution timing: non-issue at hard** (0 anarchy turns without H_REVOLUTION; nothing reads revolentype). Deprioritize.
- **Government choice measured at incumbent rate split** — `advdata.c:1030-1031` in-source: "Ideally we should change tax rates here… we'd rather not." A raised MAX_RATES ceiling can never be a reason to switch government. Deep's `deeprates.c` is uniquely positioned; pairs with the forwarded `gov_value` hook.
- **Spaceship**: parts only at 100% max; score 0 unless ARRIVED; probably dead in our cells (needs one scorelog grep).
- **No AI code reads `end_turn`. Anywhere.** MORT=24 fixed horizon; on turn 195 of 200 both AIs amortize as if 24 turns remained. Score: citizens ×1, techs ×2, future techs ×5, great wonders ×5, units/10, kills/3, culture/50.

## 6. Ranked opportunities

1. **Endturn awareness / endgame score mode** — no AI knows the game ends; techs/citizens/wonder-finishes farmable in the last 10-20 turns; measurable on every cell; the metric is the primary metric. Must not bundle with credit tuning (MORT interaction).
2. **The cease-fire trap** — 20-42% of pacified pairs stuck; mechanism legible (4-turn window vs 3-6-turn gate + best_target love bleed). Deep can fix in its own wrapper (pre-pass proposing peace the turn the window opens) without touching ai/default. Census script exists. Highest confidence, lowest risk.
3. **Embassy clause pricing** — make peace-level embassy swaps clear (override treaty_evaluate in deep); pairs with `embassy_science_value`. Both numbers → struct tuning.
4. **Free third-party signals** — tech_leader/production_leader reads; fold into the WIN_CAPITAL slice.
5. **Government × rates coupling** — the gov_value row made concrete.
6. **Ransom/hut-EV/animals** — correctness items, upstream-bugs appendix, no session.
7. **MAP/SEAMAP/VISION receive-side** — W04 checklist (honest AI must not refuse free maps).

## New upstream bug candidates

- `daidiplomacy.c:1839` clobbers the function-scope war target inside the DS_ALLIANCE branch → the no-mercy guard compares the wrong player for the rest of the loop.
- A hard AI can never open with a cease-fire (H_CEASEFIRE + zero-valuation collision).
- Dead `ally_patience` ladder ⇒ alliance-dissolution path unreachable — an alliance once formed is never broken for non-participation (matches 22.6% end-alliance rate).
- `advdata.c:488-498` `players_iterate` (not _alive), strict `<`: tech/production leader can name a dead player.
