# A theory of play for the benchmark venues (2026-10-26)

**What this is.** A positive account of what a strong strategy *is* in the
game as `benchmark/run-game.sh` actually configures it, derived from
`server/score.c`, the two rulesets (`data/civ2civ3/`, the build default per
`common/game.h:681`, and `data/classic/`, which the user plays and which the
`-classic` configs run), and the project's own measurements. The PRD and
three months of findings are a diagnosis of why `ai/deep` trails
`ai/baseline`; nothing on the shelf says what winning looks like. This does.

**Scope and method.** Read-only. Every rules claim cites the file and
section or the C function; where a number is inferred I say so. The one
new computation is a per-seat read of the 20+20 archived holdout
scorelogs per venue (`benchmark/results/tune-postfix/s6-nogates-{war,econ}/
{baseline,holdout-report}/cfg0/seed-*/scorelog.txt`), used for the baseline
milestone columns in §F; the script is reproduced in the appendix. No
games were run. Network was not attempted; the ruleset was sufficient.

**Two corrections to the brief, up front.**

1. *"Every benchmark config leaves RULESET empty"* is false. Twelve
   `-classic` twins exist (`contested-classic`, `lategame-classic`,
   `primary-120-classic`, …, `grep -l RULESET=classic benchmark/configs/`)
   and at least `primary-120-classic`, `garden-classic` and
   `cramped-classic` have result directories. They are not in the canonical
   portfolio (`benchmark/configs/README.md` lists only `isolated-classic`
   and two TO-CREATE classic cells), so the brief's substantive point stands:
   **no deep knob default was ever adjudicated on classic**, and the
   canonical score claims are civ2civ3-only.
2. The ruleset's `[settings]` block is live in the civ2civ3 venues.
   `load_rulesets()` runs in `srv_prepare()` (`srv_main.c:3214`) *before*
   `--read commands.serv` is processed (`srv_main.c:3643`), and
   `settings_ruleset()` (`settings.c:4423`) applies the ruleset's values.
   So contested plays with `citymindist 3`, `startunits "cwsx"` (settler,
   worker, **diplomat**, explorer), `techlevel 1`, `onsetbarbs 120`,
   `occupychance 100`, `razechance 10`, `restrictinfra`, `disasters 100`
   (10× the server default) and `animals 14` (`data/civ2civ3/game.ruleset`
   `[settings]`). A `-classic` twin issues `rulesetdir classic` first, and
   classic's `[settings]` is empty, so it reverts to server defaults:
   `citymindist 2`, `startunits "ccwwx"` (two settlers), `techlevel 0`,
   `onsetbarbs 60`, `occupychance 0`, `razechance 20`
   (`common/game.h:387–668`). **A classic twin is a different game in its
   settings as well as its rules.**

---

## A. What the score rewards, exactly

`get_civ_score()` (`server/score.c`, the `get_civ_score` definition) is:

| component | weight | how computed | source |
|---|---:|---|---|
| citizens | **1** each | `total_player_citizens()` — workers *and* specialists, every mood | `score.c` |
| techs | **2** each | count of `TECH_KNOWN` advances (+ `future_tech × 5/2`) | `calc_civ_score`, `advance_index_iterate` |
| great wonders | **5** each | owned, not destroyed | `is_great_wonder` loop |
| units built | **1 per 10** | lifetime count, settlers included | `get_units_score` |
| units killed | **1 per 3** | lifetime count | `get_units_score` |
| culture | **1 per 50** | `player_culture()` = nation history + Σ city history (+ Performance, zero in both rulesets) | `common/culture.c` |
| spaceship | pop×success/100 | **only if `SSHIP_ARRIVED`** | `get_spaceship_score` |
| **not counted** | 0 | gold, land, buildings, army size, happiness (explicitly removed: "too easily manipulated") | comment in `get_civ_score` |

Culture does **not** compound: `history_interest_pml = 0` in both
`game.ruleset` files (civ2civ3 `:383`, classic `:378`). The derby hindsight
memo's "compounds" is wrong; `city_history_gain()` is a flat per-turn sum
of `History` effects. What grows over time is the *count of buildings*
emitting history, and the lump sums below.

### A.1 Where the T151 contested score actually comes from

Baseline, 20 holdout games, per seat at T150 (appendix script):

| term | raw | points | share |
|---|---:|---:|---:|
| citizens | 30.4 | 30.4 | 33% |
| techs | 20.6 | 41.2 | **45%** |
| wonders | 0.16 | 0.8 | 1% |
| units built | 49.6 | 5.0 | 5% |
| units killed | 9.2 | 3.1 | 3% |
| culture | 620 | 12.4 | 13% |
| sum / logged | | 92.9 / 91.5 | |

Deep (same seeds): 26.5 + 39.1 + 1.5 + 5.0 + 3.3 + 10.5 = 85.9 / 84.7.

In lategame-80 the picture inverts: of baseline's 174.6 at T80, **100.3 is
the starting endowment** (50 techs × 2); the *earned* 74 points are
citizens 44 (59%), culture 13 (18%), units 10 (13%), techs 7 (9%),
wonders 2. That venue measures growth and almost nothing else.

### A.2 The exchange rates the formula defines

- **1 citizen = 1 point at the horizon, every turn it exists.** A tech is
  2 citizens; a wonder is 5; 10 built units or 3 kills are 1; 50 culture
  is 1.
- **Techs are a function of citizens.** `research_total_bulbs_required()`
  with civ2civ3's `tech_cost_style = "Linear"`, `base_tech_cost 10`
  (`game.ruleset:332–342`) and the unconditional `Tech_Cost_Factor 3`
  (`effects.ruleset:3803`) gives **30 bulbs × num_reqs**, where
  `advance_req_iterate` counts the tech *and* all ancestors
  (`tech.c:advance_req_iter_init` seeds the array with the goal). The 20
  cheapest techs sum to ≈1290 bulbs; baseline's logged `techout` integrates
  to ≈1480 over 150 turns, consistent. Baseline's science was 6 bulbs/turn
  per seat at T50 and 22 at T150. A citizen on a 2-trade tile at 50%
  science yields 1 bulb/turn → **~100–200 citizen-turns per tech** at this
  horizon, i.e. a citizen earns ~1 point of tech per 100 turns of work
  while being worth 1 point just by standing there. **The citizen count is
  the primary, and the tech term is its integral.** "More citizens sooner"
  is the whole of A, twice over.
- **Culture lumps.** civ2civ3's achievements credit culture straight to
  `pplayer->history` (`common/achievements.c:149,170`), i.e. **20 score
  points each** at the 1000-culture size: `Map_Known` (100% of tiles
  known), `Literate` (`get_literacy() ≥ 100`, which needs Library +
  University in every city — a Library alone scores 50%, `score.c`
  literacy clip), `Multicultural` (a city with 2 nationalities),
  `Metropolis` (a size-20 city), `Cultured_City` (1000 culture in one
  city); `Land_Ahoy` is 2 points; a launched spaceship 100
  (`data/civ2civ3/game.ruleset` `[achievement_*]`, all `unique = FALSE`).
  **Classic has no achievements and no `History` effects at all** — its
  culture term is identically zero. This resolves an oddity in the
  marginal-city regression: the `taken` coefficient carries +761 ± 38
  culture (≈15 points) because conquering a size ≥ 2 city whose citizens
  keep their nationality (`conquest_convert_pct 0`, `cities.ruleset:115`)
  fires `Multicultural` once. **The first conquest in a civ2civ3 game is
  worth ~20 points flat; the second is worth its citizens.**
- **Per-turn culture.** `History` per building (`effects.ruleset`): Palace
  1, Temple 1, Library 1, Amphitheater 2, University 2, Cathedral 3,
  Research Lab 3; wonders 2–6. A Temple built at T50 has earned 2 points
  (one tech) by T150; by T300, 5. Culture is 13% at T150, 29.5% at T300
  and 42.5% at T600 in the Europe derby (hindsight memo §1) — the term
  that scales with horizon.
- **Wonders.** 200 shields (Pyramids, Colossus, Temple of Artemis,
  Hanging Gardens, Mausoleum, Statue of Zeus, Lighthouse, Marco Polo;
  `buildings.ruleset`) buy 5 points + 2 culture/turn (+4 points by T150
  if built at T50) + the effect. ≈9–10 points for 200 shields, against a
  Settlers at 30 shields + 2 pop for 13–17 (next section). Wonders are a
  shield sink for cities that have no site left to found and no cap to
  lift — the T100+ item, not the T50 one.

---

## B. The economics of a city

Numbers are civ2civ3 first, classic in brackets; §B′ tabulates the deltas.

**The settler.** 30 shields, `pop_cost 2`, no shield upkeep, 1 food
(`units.ruleset [unit_settlers]`) [classic: 40 shields, 1 pop, 1 shield +
1 food]. Migrants (10 shields, 1 pop, Pottery) move population but cannot
found. A city must be size 3 to build one in civ2civ3 (size 2 in classic).

**What a new size-1 city yields.** The centre tile is auto-irrigated if the
terrain allows (`Irrigation_Pct` with `CityTile Center`, both rulesets) and
in civ2civ3 gets +1 shield (`Output_Add_Tile` centre) and an automatic road
on rivers (README). Grassland-river centre: 3/1/2, which Despotism's
`Output_Penalty_Tile 2` (−1 on any output above 2, lifted while
celebrating, or by Pyramids, or Railroad; `effects.ruleset`) cuts to
**2/1/2**. Working one grassland: 4 food, 1 shield, 2 trade, eating 2 →
**+2 food, 1 shield, 2 trade** before corruption. [Classic: the same tile
gives 2/1/1 after the penalty; `min_city_center` 1/1/0, `game.ruleset:102`.]

**Growth.** `granary_food_ini = 20,20,20,20,20,30,30,40`, `inc 0`
(`game.ruleset:105`): 20 food to grow at sizes 1–5, 30 at 6–7, 40 at ≥8.
`Growth_Food 50` applies free below size 4 and with a Granary thereafter
(`effects.ruleset`), so after the first growth every step to size 4 costs
**10 food**: at +2 surplus a new city is size 2 at +10 turns, 3 at +15, 4
at +20. [Classic: `20 + 10·(size−1)`, no free keep: 20, 30, 40 … — size 4
at +45 turns on +2 surplus. Granary 50% at every size; Pyramids +25%
everywhere.]

**Happiness gates.** `City_Unhappy_Size 4` (both): the fifth citizen is
born unhappy and, with no happy citizen to balance it, the city riots.
Growing past 4 therefore *requires* one of: Temple (30 sh, 1 gold; +1
content, +1 more with Mysticism — `Make_Content`), martial law (Despotism/
Monarchy 1 per unit, max 3, `Martial_Law_By_Unit`/`_Max`), 2 luxury per
step (`happy_cost 2`), or an entertainer (2 lux). Aqueduct at **8** (60
shields 1 gold; **20 shields 0 gold** on a river or lake —
`[building_aqueduct_river]`/`_lake`), Sewer at **16** (`Size_Adj 8` +
`Size_Unlimit`). [Classic: Aqueduct 60 sh 2 gold, `Size_Adj 4` → 12 per
`effects.ruleset [effect_aqueduct]`; Sewer unlimited; no cheap variants.]
Baseline's T150 contested empire is 5.3 cities of mean size 5.7: **sitting
on the Temple gate**, not the Aqueduct gate.

**Corruption and waste** (`city_waste()`, `common/city.c`): civ2civ3
Despotism 20% trade + 10% shields base, **+1%/tile** from the nearest
government centre on each, **doubled** until Trade (shields) / The
Corporation (trade) (`Output_Waste`, `Output_Waste_By_Distance`); Palace
halves it, Courthouse halves it again. [Classic: trade only — Despotism
**37% + 4%/tile**, Monarchy/Republic 15% + 2%/tile, Communism 20% flat,
Democracy 0; *no shield waste*.] A civ2civ3 city 8 tiles out loses 36% of
trade and 26% of shields under Despotism; the same city in classic loses
69% of trade and no shields.

**Unit allowance.** civ2civ3 Despotism: 2 units free per city, further
ones **1 gold** each (`ShieldToGold` + `Shield2Gold_Pct 100`); Monarchy 3
free; every unit also eats 1 food above a per-city free allowance of
max(4, size) (`Unit_Upkeep_Free_Per_City` food ladder). [Classic: 3 free
*shield* units under Despotism/Monarchy/Communism, 1 shield each beyond;
Republic/Democracy 0 free; settlers 1 food, 2 under Republic/Democracy via
`Upkeep_Pct`.]

### B.1 The break-even

Founding costs the parent 2 citizens now (−2 points) and 30 shields; it
regrows them in 10–15 turns (30 food at +2…+3). The child is size 1 on
arrival (+1), 2 at +10, 3 at +15, 4 at +20. **Pop break-even at ~+15
turns; thereafter the child is a second growth engine running in
parallel** — the parent's food box does not grow faster with more
citizens, but two boxes fill twice as fast. That parallelism, with
citizens worth 1 each and techs their integral, is the whole ICS
argument, and the score function rewards it linearly. Monarchy moves the
break-even by 2–3 turns (lifts the −1 on irrigated/special/mined tiles,
≈15–20% of early output; 3 free units). The dominant variable is not the
government but the **turns remaining**: measured +17.8/13.0/6.0 points for
cities founded T1–25/26–50/51–75 (`marginal-city-2026-08-30.md` §1), and
the stacked cell converted its added cities at 16 (war) and 6.9 (econ)
points apiece (`stacked-lever-2026-08-31.md` §2).

### B.2 Does civ2civ3's anti-ICS design bite at T150?

The README states the intent ("Settlers cost 2 population", "Corruption
and Waste evened", "tech upkeep assigned to cities", citymindist 3).
What binds in the venues:

- `citymindist 3` + `init_city_radius_sq 5` (21-tile fat cross, both):
  cities two tiles apart share 60% of their catchment; the *pattern* that
  maximises worked tiles at size ≤ 6 is spacing 3–4, not the maximum
  spread. Baseline settles at 5.3 cities on ~1/8 of a 32×32 map; land,
  not the rule, is the constraint (city ledger §5: 72% of deep's T51–75
  searches find no legal tile).
- Empire-size unhappiness (`Empire_Size_Base/Step` Despotism 10/10,
  Monarchy 14/14, Federation 24) **never fires** in either venue
  (5.3 and 9.4 cities per seat). It binds in Europe (20–30 cities).
- Tech upkeep (`tech_upkeep_style "Cities"`, divider 6000) is
  "0 during the middle ages" (README) — inert at ≤ 54 techs.
- Distance waste doubles until Trade/Corporation — a 20–30% haircut on a
  city whose existence is worth +1 per citizen. It changes *where*, not
  *whether*.

**Verdict: no.** At the T150 horizon the ruleset's ICS penalties are
second-order against a first-order linear reward. Where spreading is
genuinely punished is the venue, not the ruleset: lategame-80's
`migration` deletes a size-1 city on its fifth turn 0.6 times per seat
per game (city ledger §4), and Europe's world is full by T60.

---

## B′/C′. Where classic differs, and why it matters

| rule | civ2civ3 | classic | doctrine consequence |
|---|---|---|---|
| Settlers | 30 sh, **2 pop**, 0 shield upkeep (`units.ruleset`) | 40 sh, 1 pop, 1 shield upkeep | classic founds from size 2 and keeps the parent's tile count; civ2civ3 needs size 3 and moves two tiles. Classic ICS is cheaper per city. |
| Despotism tile penalty | −1 above 2 on F/S/T; lifted by celebration, **Pyramids**, Railroad | −1 above 2; lifted by celebration only | Pyramids (200 sh) is a Despotism-era Monarchy substitute in civ2civ3; in classic Pyramids is +25% granary. |
| Celebration / rapture | **No `Rapture_Grow` anywhere.** +1 trade on trade tiles under Monarchy/Republic/Democracy/Federation (`Output_Inc_Tile_Celebrate`); penalty lift under Despotism/Tribal | **`Rapture_Grow 3` under Republic/Democracy** (+1 pop/turn with food surplus, `rapturedelay 1`); +1 trade under Monarchy/Communism; penalty lift under Despotism | **The dominant classic mid-game lever does not exist in civ2civ3.** In civ2civ3 celebration is a 20–50% trade bonus; in classic it is +1 citizen per city per turn. |
| Governments | 9 incl. Tribal (start), Fundamentalism, Federation | 6 | baseline moves 43% of seats to Tribal by T25 (appendix); no such option in classic |
| Corruption model | trade *and* shields, base 5–30% by gov, +1%/tile, doubled until Trade/Corporation | trade only; Despotism 37% + 4%/tile, Monarchy 15% + 2%/tile, Democracy 0 | classic Despotism strangles distant trade (−77% at 10 tiles) but not production: classic says "Monarchy early or stay compact"; civ2civ3 says "Courthouse/Palace matter for shields too" |
| Unit upkeep | gold under Despotism/Monarchy/Fund./Fed./Democracy (×2 for the modern three via `Upkeep_Pct`), shields under Tribal/Republic/Communism; 1–4 free; **food 1 per unit** above max(4,size) | shields everywhere; 3 free under Despotism/Monarchy/Communism, 0 under Republic/Democracy; settlers only eat food | civ2civ3 armies drain the treasury (deep: 31 gold vs baseline 267 at T150, appendix); classic armies drain production |
| Building gold upkeep | `gold_upkeep_style "Mixed"` (nation pays buildings, city pays units); Aqueduct 1, Amphitheater 3, Marketplace 0, Walls 0 | `"City"`; Aqueduct 2, Colosseum 4, Marketplace 0, Walls 0 | both: Walls are free to keep; Colosseum is the most expensive thing to keep |
| citymindist / radius | 3 (ruleset settings) / `radius_sq 5` | 2 (server default) / 5 | classic allows cities one tile apart; the packing optimum is denser |
| Granary / Aqueduct | 20/20/20/20/20/30/30/40, 10 kept below 4 for free; Aqueduct 8, river/lake variant 20 sh 0 gold; Sewer 16 | 20+10·(n−1), no free keep; Aqueduct 8→12 per `effects.ruleset`, Sewer unlimited | civ2civ3 small cities grow ~2× faster; classic growth past 4 is slow *unless rapturing* |
| Tech cost | Linear ×3: 30·reqs (Monarchy 120, Republic 150, Philosophy 210, Sanitation 600, Space Flight 1800) | `20·(1+n)^1.5/2` (Monarchy 112, Republic 147, Sanitation 764, Space Flight 4764) | early trees cost the same (±10%); classic's late tree is 2–3× dearer |
| Tech leakage | `"Embassies"`: cost cut by `techleak`% × share of embassied civs knowing it | `"None"` | civ2civ3 rewards embassies (startunits include a Diplomat; Marco Polo = all embassies); classic does not |
| Free tech at Philosophy | `free_tech_method "Cheapest"` | `"Goal"` | the Philosophy slingshot (research Monarchy/Republic, get it free) exists in **classic only** |
| Trade routes | national 0%, foreign 100–200%, bonus gold only; max 2 routes | national 100%, foreign 200–400%, one-time bonus **gold and bulbs** | classic caravans are a research engine; civ2civ3 caravans are wonder fuel |
| Culture / achievements | `History` on 7 buildings + wonders; 7 achievements at 100–5000 culture | **none** | 13% of the T150 score and all of the 20-point lumps exist only in civ2civ3 |
| Migration | `migration_pml 50`, off by default; lategame-80 enables it | same rule, same default; never enabled in a classic venue | venue, not ruleset |
| Barbarians | `onsetbarbs 120` (ruleset settings) | 60 (server default) | classic sees barbarians at T60 in a T150 game; civ2civ3 at T120 |
| Animals / disasters | `animals 14`, `disasters 100` (ruleset settings) | server defaults (disasters 10) | civ2civ3 settlers die to animals in the opening (inferred: W05's loss census never split the cause); plague above size 4 (`illness_on`) |
| City defence | every city +50% vs ground, +50% more above size 8; Walls +100% (30 sh); terrain forest 25 / hills 50 / mountains 100 / river 25 | no inherent bonus; Walls **+200%** (60 sh); forest 50 / hills 100 / mountains 200 / river 50 | an unwalled classic city on flat ground falls to a Catapult 94% of the time; an unwalled civ2civ3 town only 74% |
| Tired attack | on (`tired_attack TRUE`) | off | civ2civ3 one-move attackers must start adjacent and fresh |
| Veterancy | same 4 levels ×1.5/1.75/2.0; workers can gain levels | same, no work levels | — |
| Occupy / raze | occupy 100%, raze 10% | occupy 0%, raze 20% | civ2civ3 attackers walk in after a kill; classic attackers must move in next turn |

---

## C. Government, and the rapture question

Reachable early (`governments.ruleset`, `techs.ruleset`): Despotism and
Tribal at start; **Monarchy** at 4 techs / 240 cumulative bulbs
(Alphabet, Code of Laws, Ceremonial Burial, Monarchy); **Republic** at 5 /
420 (Alphabet, CoL, Writing, Literacy, The Republic); Communism,
Fundamentalism (Theology), Federation (Economics), Democracy are 14–22
techs out. Baseline's trajectory (appendix): 100% Despotism at T1; 43%
Tribal at T25; Monarchy on 23% of seats at T75, 57% at T100, **76% at
T150**, Republic 9%. Deep is the same within noise. In lategame-80 with
50 techs, 90% of both AIs' seats are **Federation** from T10.

Effects that matter, civ2civ3 (README governments table, verified against
`effects.ruleset`):

| | Despotism | Tribal | Monarchy | Republic |
|---|---|---|---|---|
| tile penalty | −1 > 2 | −1 > 2 | none | none; **+1 trade on land tiles with trade** |
| celebration | lifts penalty | lifts penalty | +1 trade/trade tile | +1 more trade, land and sea |
| max rate | 60 | 60 | 70 | 80 |
| corruption / waste | 20 / 10 | 30 / 0 | 10 / 20 | 25 / 5 |
| free units; cost | 2; 1 gold | 2; 1 shield | 3; 1 gold | 1; 1 shield |
| martial law | 1/unit, max 3 | none (+1 content) | 1/unit, max 3 | none; 1 unhappy per aggressive unit beyond 1 |
| Palace bonus | +75% gold | — | +50% gold | — |
| empire size | 10 | 12 | 14 | 16 |

[Classic: Despotism 60% / 37%+4%/tile / 3 free shield units; Monarchy 70%
/ 15%+2% / 3 free / celebration +1 trade; Republic 80% / 15%+2% / 0 free /
+1 trade on *all* trade tiles / **rapture grow**; Democracy 100% / 0 / 0
free / rapture / 2 unhappy per aggressive unit.]

**Celebration mechanics** (`common/city.c`): a city celebrates when size ≥
`celebratesize` 3, no unhappy or angry citizens, and happy ≥ ⌈size/2⌉
(`city_happy`). `pcity->rapture` counts consecutive celebrating turns
(`cityturn.c:3790`); `city_rapture_grow()` adds a citizen when
`rapture % rapturedelay(1) == 0`, food surplus > 0 and `EFT_RAPTURE_GROW
> 0`. **civ2civ3 defines no `Rapture_Grow` effect** (the type is absent
from `effects.ruleset`; README "CORE CHANGES: No Rapture Growth"). The
project's silence on rapture was therefore *correct for its venues* and
*wrong for the user's classic games*.

**Cost to trigger and sustain.** Making ⌈n/2⌉ citizens happy costs
2 luxury each after every unhappy one is bought back (`happy_cost 2`):
a size-6 city with a Temple needs 6 lux/turn; 8 trade at 60% lux through
a Marketplace (+50%) gets there. So celebration costs roughly a city's
whole trade output while it runs.

- *civ2civ3 payoff:* Monarchy +1 trade per trade tile ≈ +4–6 trade for a
  size-6 city — roughly what the lux cost. Break-even to mildly positive;
  stronger under Republic (two +1s). Despotism: lifts ≈ 2–3 output. **A
  second-order lever, correctly ignored so far.** Baseline runs `luxrate
  0` on every sampled seat-turn and 1.4 happy citizens of 30; it never
  celebrates.
- *Classic payoff:* **+1 citizen per city per turn.** Eight size-5
  Republic cities rapturing for 10 turns add ~60 citizens (60 points),
  for ~500 trade of luxury that would otherwise have bought ~2 techs
  (4 points). **Fifteen to one.** Rapture is bounded only by food surplus
  (> 0 suffices — the box is bypassed) and the Aqueduct/Sewer caps. The
  classic game is: expand to the land limit under Despotism/Monarchy,
  take Republic (~400 bulbs), Temple + Marketplace, then convert trade
  into citizens at 1:1 per turn until the caps, then Aqueducts. Nothing
  in `ai/deep` models this (§G); `ai/default` does, crudely
  (`daihand.c:456–508`: celebrate if more than half the cities can).

---

## D. Tech: order and value

Costs from §A.2 (civ2civ3 first; classic within ±10% in the early tree).

| goal | own cost | cumulative | what it buys |
|---|---:|---:|---|
| Bronze Working | 30 | 30 | Colossus (200 sh: +1 trade per trade tile — the best civ2civ3 wonder for a coastal capital), Currency |
| Alphabet → Code of Laws | 30, 60 | 90 | Courthouse (60 sh: halves waste, +1 content) |
| Ceremonial Burial → Mysticism | 30, 60 | 90 | Temple (30 sh, +1 content, 1 culture/turn); +1 more with Mysticism; Temple of Artemis |
| **Monarchy** | 120 | **240** | the tile penalty, 3 free units, 70% rates |
| Currency → **Trade** | 60, 120 | 240 | Marketplace (60 sh, 0 upkeep); Caravans (50 sh, `HelpWonder`); halves distance shield waste; Great Library, Marco Polo (all embassies → leakage) |
| Writing → Literacy | 60, 120 | 240 | Library (60 sh: +50% science, 1 culture) |
| **The Republic** | 150 | 420 | +1 trade on every land trade tile |
| Philosophy | 210 | 570 | one free tech (cheapest available in civ2civ3; your research goal in classic) |
| Masonry (30) → Construction | 150 | 270 | Walls (30 sh, 0 upkeep); Pyramids; Aqueduct (plain); Amphitheater |
| Pottery → Seafaring | 30, 120 | 240 | Harbour (+1 food on ocean), Explorer, Great Lighthouse |

Baseline's measured pace is 4.8 techs at T50, 9.7 at T75, 13.8 at T100,
20.6 at T150 — Monarchy arrives around T75–100, which the tables above
say is ~30 turns late for a 240-bulb target when 6 bulbs/turn are
available from T40.

**Value per shield at T150** (inferred from the rules, not measured):
Settlers 30 sh + 2 pop → 13–17 points if placed by T50 (measured). Temple
30 sh → +1 citizen (the gate at 4) + 2 culture points over 100 turns ≈
3–4 points, in any city at the content cap. Library 60 sh → +3 bulbs/turn
on a 6-bulb city ≈ 1.5–2 techs ≈ 3–4 points + 2 culture ≈ 5–6. Granary 40
sh + 1 gold → ≈ 0 below size 5 (free effect). **Amphitheater 70 sh + 3
gold/turn** → +3 content it does not need in a size-5 empire; its upkeep
alone over 100 turns is 300 gold ≈ the purchase price of 3–5 Settlers.
Deep's 23% Amphitheater share of T100–125 picks with riots ≈ 0 (deficit
census) is the clearest case of a want with no top-down price: the rules
make it the single worst early building per shield-and-gold.

**Beelines.**
*T150, civ2civ3:* Bronze → Alphabet → CoL → Ceremonial → **Monarchy by
~T50** (revolution 1–5 turns, `revolentype RANDQUICK`), then Currency →
Pottery → Trade (caravans into one 200-shield wonder per 2 cities,
Colossus or Pyramids first), Writing → Literacy → Mysticism → Philosophy
(free tech), Masonry → Construction (Walls before `onsetbarbs` 120), then
Republic *only if* at peace with ≤ 1 aggressive unit per city. Map Making
early enough for Triremes to claim `Map_Known` (+20) before T150.
*T150, classic:* Bronze → Alphabet → CoL → Writing → Literacy → **Republic
by ~T60** (skip Monarchy; classic Republic has 0 free units but rapture
pays for everything), Ceremonial → Temple, Currency → Marketplace,
Philosophy with Republic/Monarchy as the Goal if timing allows.
*T300, either:* add Mathematics → Astronomy → University (Literate in
civ2civ3; Newton), Monotheism (Michelangelo), Theology (J.S. Bach: +2 lux
in every city = the only cheap way to celebrate everywhere), Bridge
Building → Sanitation (size 16+, Metropolis), Railroad (lifts the
penalty, +1 shield per tile with Pyramids' effect), Economics → Federation
or Democracy, The Corporation (halves trade distance loss), Theory of
Gravity. Culture infrastructure (Temple + Library + Amphitheater +
University in every city) by T150 is worth 6–8 culture/turn/city ×
150 turns ≈ 20 points per city at T300 — comparable to the city itself.

---

## E. War as an economic act

**The combat model** (`common/combat.c`): `win_chance()` is a
binomial over 10-HP, firepower-1 rounds with per-round
P(attacker wins) = A/(A+D). Attack = strength × veteran factor (×1.5/1.75/
2.0, `units.ruleset [veteran_system]`) × moves-left fraction (civ2civ3
`tired_attack`). Defence = strength × veteran × (1 + terrain) × (1 +
river) × (1 + Σ `Defend_Bonus`) × (1 + fortify 50%). Units in cities
count as fortified (`Fortify_Defense_Bonus` with `CityTile Center`).

P(attacker wins), computed from `win_chance()` with each ruleset's
multipliers (green units):

| defender, situation | D | Archers 3 | Legion/Knights 4 | Catapult 6 |
|---|---:|---:|---:|---:|
| **civ2civ3** Phalanx, open fortified | 3.0 | .50 | .74 | .94 |
| Phalanx in a town (≤8) | 4.5 | .19 | .40 | .74 |
| Phalanx, town + **Walls (30 sh)** | 7.5 | .02 | .08 | **.31** |
| Phalanx, town + Walls + hills | 11.25 | .00 | .01 | .08 |
| Pikemen (3), town + Walls | 11.25 | .00 | .01 | .08 |
| **classic** Phalanx, open fortified | 3.0 | .50 | .74 | .94 |
| Phalanx, city, **no walls** | 3.0 | .50 | .74 | **.94** |
| Phalanx, city + Walls (60 sh) | 9.0 | .01 | .04 | .19 |
| Phalanx, city + Walls + hills | 18.0 | .00 | .00 | .01 |

(veteran Catapult, 9: .66 vs civ2civ3 walled town, .50 vs classic walled
city. Classic Pikemen double against horse, not modelled above.)

**What this demands.** A walled civ2civ3 town with two Phalanxes (20 + 20
+ 30 = 70 shields) needs ~6.5 Catapults *arriving together and fresh*
(≈260 shields plus escorts) for P(take) ≈ 0.9; against anything but
Catapults the ratio is 12:1 or worse. `killstack` is on (both rulesets),
so the stack adjacent to the city dies whole if its best defender loses
one fight. An eta-2 approach is a turn spent adjacent at D = 1 for the
Catapult: this is why S-OPXRATE found contact only at eta 0–1 (30/30
strikes) and **0 of 117 launches at eta ≥ 5 ever struck**, and why 354
deaths bought 2 kills at the walls. The launch model's P(take) 0.88–0.93
against a realised 4.4% is not a combat-model error; it prices
P(win | adjacent, fresh, together) and the ruleset makes that conjunction
the entire problem.

**When conquest is positive-sum.** A city taken is worth its citizens
(+3.4 measured) plus the one-off `Multicultural` 20 points (civ2civ3
only) plus the opponent's loss (−7 to −14, which in summed self-play is
a *zero-sum transfer* — it raises rank, not the arm's summed score). A
defended town costs ≥ 300 shields to take under both rulesets; 300
shields is ten Settlers' worth of production. **Against a defended town
in a T150 game, founding dominates conquest by an order of magnitude**
whenever a legal site exists. Conquest is positive-sum in exactly three
cases: (1) walk-ins — undefended or size-1 cities (8 of deep's 13 takes;
razing a size-1 city costs the victim 6–17 points and you nothing); (2)
the first `Multicultural` city in civ2civ3 (+20, once); (3) when the land
is full (Europe by T60; every city after that is taken from someone).
Barbarians (`onsetbarbs` 120 civ2civ3 / 60 classic) and huts
(`default.lua:146–167`: 1/14 barbarians, 3/14 tech, 5/14 gold, 1/14 a free
city) are the only free military score early.

**Defence is the cheap side.** Walls at 30 shields and 0 upkeep (civ2civ3)
make a Phalanx town untakeable by anything before Cannon/Musketeers; one
Phalanx + Walls per border city (50 shields) removes the enemy's
positive-sum cases (1) and (3). In classic Walls cost 60 and the unwalled
city is soft (.94 to a Catapult), so **the wall decision is pivotal in
classic and merely cheap in civ2civ3**. Deep's measured 7× military at T16
and 3× at T50 (appendix: 3.05 vs 0.94 units/seat) buys none of this —
`unitsbuilt`/10 pays 0.1 point per unit and the gold upkeep (1/unit
beyond 2) is what pins deep's treasury at 31 against baseline's 267.

---

## F. A phase doctrine

Baseline numbers are per seat, 20 holdout games per venue (appendix);
"target" is what §A–E imply a strong player reaches, and is **inferred,
not measured** — it is the hypothesis a doctrine-aware deep should be
scored against. **RI** = ruleset-invariant (hard-wire), **RS** =
ruleset-specific (probe at game start — W08's ruleset probe,
`LATER-PHASES.md:18` — or carry in knobs).

### Opening, T1–~40: convert the start into the maximum number of growth engines

Objective: cities. Every city founded before T25 is 17.8 points at T150
and the T1–25 band is where deep is cheapest behind (−0.10 cities,
−1.7 points, marginal-city §7).

Decisions: (1) found the capital on turn 1 (any 2/1/2-class tile; README
"TIP"); (2) **Settlers is the default build at size 3** (civ2civ3) / size
2 (classic) until no legal site is within ~10 tiles — Warriors only as
the escort the danger map actually demands (RI); (3) explore with the
free Explorer and the free Diplomat (civ2civ3 `cwsx`: embassies feed
leakage; `Land_Ahoy` is 2 points) (RS); (4) research toward Monarchy
(civ2civ3) or Republic (classic) and nothing else (RS).

| milestone | baseline contested | deep | target |
|---|---:|---:|---:|
| cities at T25 | 1.61 | 1.49 | ≥ 2 |
| settlers alive at T25 | 0.34 | 0.57 | 1 (never 0, never hoarded) |
| military units at T25 | 0.35 | **1.19** | ≤ 0.5 |
| cities at T50 | 3.24 | 3.14 | ≥ 4 |
| techs at T50 | 4.8 | 4.9 | 4 (Monarchy's prerequisites) |
| lategame-80: cities at T10 from 8 settlers | 5.68 | 5.01 | 8 by T6 |

### Expansion, T40–~100: fill the land, lift the −1, open the content gate

Objective: the last legal site, every city past size 4.

Decisions: (1) **Monarchy by T50–60** (240 bulbs; RS — classic: Republic
by T60 and begin rapture); (2) Temple in every city that reaches 4 —
nothing else makes the fifth citizen legal for 30 shields (RI, the gate
exists in both); (3) Walls + one defender in every border city before
`onsetbarbs` and before any neighbour fields Catapults (RI in kind, RS in
timing: T120 vs T60); (4) one Trade-era wonder for the cities that have
no site left (Colossus/Pyramids/Temple of Artemis), fed by Caravans (RS:
classic caravans should instead run trade routes for the one-time bulbs);
(5) Harbour in ocean-heavy cities (+1 food on 1/0/2 tiles makes them
2/0/2 — the best growth tiles in both rulesets).

| milestone | baseline | deep | target |
|---|---:|---:|---:|
| Monarchy adopted (share of seats) at T75 | 23% | 48% | 100% by T60 |
| cities at T75 / T100 | 4.32 / 5.13 | 3.69 / 4.39 | ≥ 5 / ≥ 6 |
| pop at T100 | 21.5 | 20.9 | ≥ 28 (every city ≥ 5 with a Temple) |
| gold at T100 | 274 | **38** | ≥ 150 (a buy reserve, no more) |
| military at T100 | 8.0 | 10.6 | ≤ 6 + walls |
| techs at T100 | 13.8 | 13.9 | ≥ 15 |

### Consolidation, T100–150: caps, culture, the free 20s

Objective: pop at the Aqueduct cap, every cheap culture source placed,
the achievement lumps collected.

Decisions: (1) river/lake Aqueduct (20 sh, 0 gold) in every eligible city
at size 7; plain Aqueduct only where food surplus ≥ 3 (RS: classic 60 sh
2 gold, cap 12); (2) Library everywhere (science + culture); (3) finish
the map: Triremes along every coast and an Explorer per continent —
`Map_Known` is +20 points, a fifth of the T150 score, for ~100 shields
(RS: civ2civ3 only); (4) Republic if the empire is at peace; (5) **no
Amphitheaters** unless `riots > 0` in that city (RI by cost, the upkeep
is 3–4 gold in both).

| milestone | baseline | deep | target |
|---|---:|---:|---:|
| pop at T150 | 30.4 | 26.5 | ≥ 40 (6 cities × 7) |
| techs at T150 | 20.6 | 19.6 | ≥ 24 |
| culture at T150 | 620 | 526 | ≥ 1600 (Temple + Library by T75 in 6 cities ≈ 900, + `Map_Known` 1000) |
| wonders | 0.16 | 0.29 | 1 |
| score at T150 | 91.5 | 84.7 | ≥ 115 (sum of the rows above) |

### Late game, T150–300: culture, government, and the war you did not start

Culture is ~30% of a T300 score and grows linearly with
buildings × turns, so the T150 culture stock is the T300 lead. Decisions:
(1) University + Amphitheater + Cathedral where the gold exists (Theology
→ J.S. Bach's +2 lux everywhere is the only cheap empire-wide
celebration); (2) Sanitation for size 16+ and one `Metropolis` (+20);
(3) `Cultured_City` (+20) in the capital by stacking wonders there; (4)
Railroad; (5) **war only as denial or walk-in** until Cannon/Artillery,
and only against the leader (rank in mixed games is what head-to-head
measures, §H); (6) spaceship only in a game that will reach `SSHIP_ARRIVED`
— the launch alone is +100 culture-points (5000) in civ2civ3, which is why
`Enable_Space` is the one late tech with a flat 100-point payoff. Derby
evidence (STATUS Derby-4/6/7): deep's T300 losses were an economy failure
(a stock runaway reaching 100–213 cities) contained only when deep's
garrison floors were zeroed; deep's culture flow per city was half
stock's at T601 (hindsight §1). Classic T300: the same, minus culture,
plus rapture — every Republic city at its Aqueduct cap by T120.

---

## G. Where this meets the project

| doctrine item | root cause | implementing mechanism in `ai/deep` today | gap |
|---|---|---|---|
| Settlers as default at size 3 until no site | RC1, RC4 | `expansion_gate_enabled`, `founder_pool_per_site`, the five adjudicated gates (`mil_opportunity_gate`, `founder_site_cap_gate`, `founder_join_guard_gate`, `frontier_transit_gate`, `boat_shadow_gate`), `founder_floor_*`, `scale_settler` | mechanism exists and is measured (+0.15/+0.60 cities). No *price* on the first military unit vs the first settler: 7× military at T16 is the un-priced term; `site_settler_cost_weight` charges the settler, nothing charges the Warriors it displaces |
| Monarchy by T60 / Republic by T60 | RC1 | `gov_valuation`, `gov_anarchy_weight`, `gov_rate_ceiling_gain`, `techgoal_deep`, `techgoal_distance_power`; `deepgov.c` reads unit upkeep per candidate | **no time-horizon term**: the valuation compares arranged output today, so a 240-bulb beeline is never *scheduled*; research order is stock's `ai_better` chain |
| Temple at the size-4 gate, nothing at riots = 0 | RC1 | `horizon_content_weight` (W03), `cm_happy_factor`, `cm_*` | content credit is priced per citizen freed, not per *gate*; Amphitheater's 3-gold upkeep is read by `upkeep_gold_weight` but 23% of picks says the weight is wrong — a tuning target with a now-known direction |
| Walls + 1 defender per border city, before onset | RC2, RC6 | `garrison_floor*`, `wall_fear_weight`, `floor_border_weight`, `barb_anticipation_turns`, `defensive_techwall_weight` | the floor is a *count* of units; the doctrine wants a *fortification* (50 shields, 0 upkeep) — the currency is wrong. Derby-6 zeroed the floor family and flipped −809 to +337 |
| Culture as a planned term; `Map_Known`, `Multicultural`, `Literate` | RC1 | `horizon_culture_weight` (W03), `explore_discovery_value`, `conquest_culture_gain` | **no mechanism knows an achievement exists.** `deepstance.h` declares culture inert "because stock prices it"; stock prices `History` at its per-turn value and nothing prices a 1000-culture lump. 20 points for finishing the map is the cheapest unclaimed score on the board |
| Celebration (classic rapture) | RC1, RC4, RC5 | **none.** `deeprates.c` defers celebration to stock explicitly ("gets its own session"); `deepcm.c:394` and `deeptuning.h:1337` state "civ2civ3 has no rapture" as the reason the CM food weight is the only growth lever | correct on civ2civ3, silently wrong on classic: `rates_*`, `cm_grow_*` have no rapture branch |
| Conquest only as walk-in / first `Multicultural` / full land | RC2, RC6 | `op_launch_odds`, `op_take_target`, `op_arrival_gate`, `op_launch_eta_cap`, `conquest_worth_mult`, `weak_prey_gate` | `deep_take_odds_cost()` prices P(win | arrived); nothing prices P(arrive) (S-OPXRATE); nothing values the +20 lump or the zero-sum nature of the victim's loss in summed self-play |
| Monarchy/Republic choice by aggressive-unit count | RC4 | `gov_valuation` reads upkeep; `deepdefence.c:2160` knows the 30% Democracy figure | no war-weariness forecast |
| Caravans: wonders (civ2civ3) vs routes (classic) | — | `caravan_discount`, `caravan_foreign_trade`, `trade_merit*`, `deeptrade.c` reads the `[trade]` table | reads the ruleset; adequate |

**Silent civ2civ3 assumptions in deep** (grep `civ2civ3` in `ai/deep/`,
20 files): no government, building or tech is named by string anywhere —
everything goes through `*_by_rule_name`/effects, which is right. The
assumptions are in *comments that became design*: `deepcm.c:394`,
`deeptuning.h:1337` (no rapture → food weight is the only growth lever),
`deepgov.c:144` (early governments charge "almost no unit gold" — true of
civ2civ3's 1 gold, false of classic's shields), `deepsite.c:1948`
(pop_cost 2 reasoning), `deeptrade.c:717,935` (civ2civ3's route table),
`deepdiplomacy.c:626` (Embassies leakage). And every default in
`deeptuning.h` — 300+ knobs — was adjudicated on civ2civ3 venues only
(the canonical portfolio has no classic cell). A knob like
`cm_grow_below` tuned where growth is 10 food per step will be mis-set
where it is 20 + 10·n and rapture exists.

**What the bottom-up diagnosis missed by never having this view.** Three
months of expansion epics priced *the settler*; none priced *the Warriors
built instead of it*. The deficit census found the treasury at 1/8 of
baseline's and read it as a symptom; §B says it is the direct bill of
the 1-gold-per-unit rule on a 3× army. The Amphitheater spree was read
as a happiness-model bug; §D says it is a building whose upkeep over 100
turns buys five Settlers. Culture was filed as "metric gaming" from a
T150 view; §A shows a fifth of the baseline's T150 score sits in two
achievement lumps nobody claims. And the war venue's 2% take rate was
attributed to a march model; §E says the ruleset makes a defended town
cost ten Settlers and the doctrine answer is "do not", with three
exceptions.

**Where this memo is speculative and the measurements should win.** Every
"target" column in §F; the claim that Monarchy by T60 pays more than
Tribal-then-Monarchy (baseline's 43% Tribal at T25 is a choice the stock
evaluator makes for reasons I have not read — it may be right about
shield-upkeep armies); the value of early Walls against AI opponents who
rarely siege (deep's op take rate says they don't, baseline's 0.075
taken/seat/game says they occasionally do); and the whole of §C's classic
rapture arithmetic, which has never been run on this project's harness.
If a milestone target in §F costs score when implemented, the target is
wrong, not the measurement.

---

## H. On measurement

**Summed self-play score** (the primary metric, `00-context.md` §6)
measures *societal output under symmetric opposition*: citizens, their
tech integral, culture stock, plus a small military residue. It cannot
see a zero-sum transfer — a city taken from a same-module seat adds its
citizens on one line and subtracts them on another. It rewards ICS, early
Monarchy, Temples, exploration and culture infrastructure exactly as §A
weights them, and it rewards war only through the +20 `Multicultural`
lump and kills/3. **Rank in mixed games** measures something else: the
ability to take *and deny*, where the victim's −14 is as good as your
+14. The Derby trajectory (+70 at T50, crossover T100–150, −337 at T300)
is a rank statement; the canonical suite stops where rank starts to
diverge from output. Both are legitimate; they should not be confused,
and a doctrine item (§F late game, "war only against the leader") can be
right for one and neutral for the other.

**Cheap intermediate meters** (all readable from `scorelog.txt` tags
already logged for both arms; `cityledger.py`/`marginalcity.py` read
them): cities at T50 (tag 3; SD 0.32 per seat — the project's own 5.8×
cheaper meter), **Monarchy share at T60** (tag 23 `gov`, a milestone no
cell has ever reported), pop at T100 (tag 0), Temple-gate compliance
(pop ÷ cities ≥ 5 at T100), culture at T100 (tag 29; a jump of 1000 on
one seat is an achievement), gold at T100 (tag 14; ≥ 150 and ≤ 500 —
both tails are failures), military ÷ cities at T50 (tags 5/3; ≤ 1.0). A
T50/T100 panel of these for every cell costs nothing and would have
caught the 7× army, the Amphitheater bill and the missing `Map_Known`
without a single new game.

**A classic generalisation check.** No canonical venue has run classic.
The cheapest one exists already: `contested-classic.conf` (8p, 32×32,
T150, `RULESET=classic`), paired against `contested`. Run it at n = 20
both arms as a *generalisation* cell, not a tuning cell. What §B′ says
should move: the score level rises (classic Despotism's 37% + 4%/tile
corruption is harsh, but Settlers are 1 pop and classic has two starting
settlers; expect ~4–5 cities at T50 for both arms); culture is 0 for
everyone (the 13% term vanishes, so the deep−baseline gap shrinks by its
culture share, ~2 points); **the arm that celebrates wins the T100–150
pop race** — stock's `daihand.c` celebration branch exists and deep
defers to it, so the gap should not widen on that account, but any deep
CM posture that overrides luxury (`cm_grow_*` with `rates_enabled`) will
suppress rapture and show up as a *pop* deficit that does not exist on
civ2civ3; barbarians at T60 instead of T120 will raise `unitslost` for
both and test the garrison floor's timing knobs; `citymindist 2` will
raise city counts for whichever site scorer tolerates dense packing
(`site_own_overlap`). If deep's ratio to baseline on classic is within
2 points of its civ2civ3 ratio, the knob set generalises; if it is worse
by more than the culture share, the civ2civ3-only adjudication is the
cause and the first suspect is the growth/CM family named above.

---

## Appendix: provenance of the baseline columns

Per-seat means over the 20 archived holdout games per venue (seeds
101–120), read from `scorelog.txt` tags (`data <turn> <tag> <player>
<value>`); the gov ids are civ2civ3's `governments.ruleset` order
(0 Anarchy, 1 Tribal, 2 Despotism, 3 Monarchy, 4 Communism,
5 Fundamentalism, 6 Federation, 7 Republic, 8 Democracy).

contested, `ai/baseline`:

| turn | cities | pop | techs | score | gold | munits | settlers | culture | techout | gov (seats of 160) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 25 | 1.61 | 2.9 | 2.0 | 9.0 | 94 | 0.35 | 0.34 | 125 | 3.0 | Tribal 69, Desp 91 |
| 50 | 3.24 | 7.7 | 4.8 | 20.0 | 177 | 0.94 | 1.42 | 171 | 6.2 | Tribal 31, Desp 128 |
| 75 | 4.32 | 14.4 | 9.7 | 39.7 | 244 | 3.67 | 2.46 | 238 | 9.7 | Tribal 53, Desp 68, Mon 37 |
| 100 | 5.13 | 21.5 | 13.8 | 59.3 | 274 | 7.98 | 2.95 | 380 | 13.4 | Mon 91 |
| 150 | 5.34 | 30.4 | 20.6 | 91.5 | 267 | 12.70 | 2.43 | 620 | 21.9 | Mon 121, Rep 14 |

contested, `ai/deep` (s6-nogates): T25 1.49 / 2.5 / 1.8 / 8.1 / 25 /
1.19 / 0.57; T50 3.14 / 9.05 / 4.9 / 21.5 / 47 / 3.05 / 0.59; T75 3.69 /
16.3 / 10.1 / 41.3 / 51 / 6.19 / 0.55; T100 4.39 / 20.9 / 13.9 / 56.9 /
38 / 10.6 / 0.50; T150 4.69 / 26.5 / 19.6 / 84.7 / 31 / 15.5 / 0.19,
culture 526; Monarchy 77 seats at T75, 127 at T150.

lategame-80, `ai/baseline`: T10 cities 5.68, pop 8.3, settlers 2.33,
Federation 108/120 seats; T25 6.53 / 22.1; T50 8.21 / 35.1 / techs 51.6
/ score 146.3; T80 9.41 / 44.1 / 53.5 / 174.6, culture 672, munits 18.1.
`ai/deep`: T10 5.01; T25 6.07 / 24.9; T50 7.06 / 34.2; T80 8.39 / 41.7 /
53.0 / 167.3, culture 661, gold 34 (baseline 113). `luxrate` is 0 and
`riots` ≤ 0.01 on every sampled seat-turn in both venues and both arms.

Reader (Python 3, stdlib): for each file, for each `data` line with tag in
{0,3,4,5,6,7,9,14,15,16,17,18,19,22,23,25,26,27,28,29}, accumulate by
(turn, tag) across players and files; print means at the turns above and
a `Counter` of tag-23 values. Band-edge convention as in
`city-ledger-2026-08-30.md` §0 (the sample at turn *t* is the start of
turn *t*).
