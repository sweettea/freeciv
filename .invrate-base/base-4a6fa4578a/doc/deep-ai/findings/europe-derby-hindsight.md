# The Europe derby, in hindsight (2026-08-06)

Seven `ai/deep` seats against seven `ai/default` seats, one map, 600
turns, played to the bell. Deep took first place (Carthago 1093) and
lost the cohort total 4364 to 4930. This is the post-mortem half: what
each seat did wrong, which mechanism already on the ladder would have
caught it, and — the part worth reading — what nothing on the ladder
covers.

Material: `scratchpad/europe/` — `freeciv-score.log` (per-turn, all 31
tags), 61 autosaves at 10-turn spacing plus the T601 final,
`game.log` (973k lines, the full deep instrument stream), `derby.serv`.
Server binary built 2026-08-06 08:50, i.e. HEAD through `1783550091`
(PEACE's love-bar removal); it does **not** contain `cede5b9dbd`
(conquest denial, 12:33). No tuning file: every knob at its compiled
default.

Final standings:

    Deep_Carthago  1093  (1st)      Def_Iberia      942  (2nd)
    Def_Hungaria    871  (3rd)      Deep_Anglia     828  (4th)
    Deep_Rus        728             Def_Scot        726
    Def_Norge       690             Deep_Hellas     632
    Def_Italia      577             Def_Germania    563
    Def_Polska      556             Deep_Iceland    474
    Deep_Osmanli    419             Deep_Gallia     190  (last)

---

## 0. Four things about this game that must be stated before any of it

**(a) The map is not the one `derby.serv` asked for.** The file sets
`xsize 128 / ysize 88`; the civ2civ3 ruleset sets `mapsize=PLAYER`, and
the save records `xsize 46, ysize 92, topology ISO|HEX, wrap
WRAPX|WRAPY`. 1532 land tiles for 14 players = **109 land tiles per
player**, against the `primary.conf` target regime of 400. This is
`cramped.conf` density (~90) with fourteen players on it.

**(b) The world was full by turn 60 and stayed full.** Unowned land
went 673 tiles (T10) → 393 (T60) → 377 (T601) and never moved again.
Of those 377 leftovers, **309 are Glacier and 38 Tundra**; 17 tiles of
grass/plains/forest existed unclaimed on the whole map at the end. So
the collapse of the founder want after T50 (share of city-turns with a
positive founder bid: 46-86% before T50, 0-24% after, exactly 0.0% in
T150-250 and most later windows) is **correct behaviour**, not a
defect. Every city gained after T60, by anyone, was taken from someone.

**(c) The horizon is double anything ever measured.** No scoring config
in `benchmark/configs/` exceeds `ENDTURN=300`, and no scoring config
exceeds 8 players. This game is 600 turns and 14 players. §1 shows the
score's composition changes monotonically with horizon, so this is not
"more of the same game".

**(d) Deep alone paid taxes stock did not, with every correction off.**
`deep-payswitch imposed=1` on all seven deep seats: 10 239
`deep-switchcost` debits, 3 243 447 want-points charged, and the want
was driven to exactly zero in **7628 of them (74%)**. Meanwhile
`scale_settler … scale_ferry` were all 1.0 (class rescale not applied),
`emergency_value_cap = 0.0` (off), `trade_merit = 0` (off), and there
is no ENDGAME mechanism at all — nothing in either AI knows the game
ends at T600. One seed. Nothing below is a measurement of deep-vs-stock
strength; it is a forensic reading of one long game.

---

## 1. The finding that reframes the rest: the score is mostly culture

`get_civ_score()` = citizens + 2×techs + 5×wonders + built/10 + kills/3
+ **culture/50**. Culture is `player_culture()`, which sums city
`history`, and `city_history_gain()` compounds: `history +=
EFT_HISTORY×… + history × history_interest_pml/1000`. In civ2civ3, 38
effects carry History — Temple 1, Library 1, Colosseum 2, Cathedral 3,
University 2, Research Lab 3, Palace 1, wonders 2-6.

Composition of total world score, by turn:

    turn   culture  kills   techs   built  wonders  citizens
    T 60      7.3%   0.0%   43.4%    3.1%    1.1%     45.0%
    T100      7.9%   1.1%   42.0%    4.5%    2.0%     42.4%
    T150     14.5%   5.2%   36.5%    5.3%    3.0%     35.5%
    T200     19.5%   9.0%   33.3%    5.9%    2.8%     29.5%
    T300     29.5%  12.7%   27.6%    6.3%    2.2%     21.6%
    T400     35.6%  13.9%   24.6%    6.4%    2.0%     17.6%
    T600     42.5%  15.2%   21.5%    6.2%    1.5%     13.0%

At the tuning cells (T150) the metric is a tech-and-citizens metric. At
the longest scoring cell (`primary`, T300) culture is already the
second-largest term at 29.5% and citizens have fallen to 21.6%. At T600
culture is the plurality and citizens are noise.

Deep prices none of it. `deepstance.h` declares the culture axis
**inert** on the grounds that "culture is worth culture/50 in
`get_civ_score()`, and the lever is already priced by stock";
`findings/deep-dive-2026-08-05/economy-effects.md:64` files History as
"partial metric-gaming … report, don't lead"; `S-ENDGAME-horizon.md` §3
lists "no culture/History chasing as a headline" under **what NOT to
do**. All three calls were made from a 150-300-turn view of the metric.

The cohort consequence, from the T601 save (deep 70 cities, stock 65):

    building        deep   stock          building     deep  stock
    Temple             7      28          Marketplace     6     34
    Colosseum         10      25          Aqueduct*       8     20
    Cathedral          5      17          Bank            3     20
    University         6      13          City Walls     67     63
    Library           17      19          Harbour         1      9

    History flow/turn: deep 99 (1.41/city), stock 190 (2.92/city)

    (*plain Aqueduct only; counting the Lake/River variants, deep 22/70
     have any aqueduct, stock 47/65.)

Stock generates **2.1× deep's culture flow per city**, into a
compounding stock, for 600 turns. Final culture: Carthago 25 579,
Anglia 24 786, Iberia 23 145, Hungaria 22 709 — which is, to within one
swap, the final scoreboard. Gallia 2044, Iceland 5532, Osmanli 6900 —
the bottom three deep seats.

---

## 2. Gallia: eaten by its own cohort, having built diplomats

### 2.1 The timeline

    T 43   9 cities  (peak)          T203   7 cities (Rouen retaken)
    T 47   8                         T223   6   Marseille -> Carthago
    T 51   7                         T225   5   Toulouse  -> Carthago
    T 53   6                         T343   4
    T 77   5                         T344   3   Montpellier -> Carthago
    T 77-T200: five cities, 123      T530   2   Toulon -> Hellas -> Rus
      turns, no change               T601   3   (17 pop, 47 techs, 190)

Every one of the nine cities Gallia founded was taken by force at some
point, and the save's `acquire_t` is `CACQ_CONQUEST` (0) on all of them
— no migration, no gift. The tracer, from the city tables:

    Paris      T6  -> Hellas T50   -> Iceland T540
    Nantes     T9  -> Hellas T50
    Nice       T3  -> Hellas T60   -> back to Gallia T210
    Rouen      T22 -> Hellas T80   -> Gallia T200 -> Carthago T350 -> Gallia T540
    Marseille  T5  -> Carthago T230
    Toulouse   T8  -> Carthago T230   (size 15 at capture; size 22-23 after)
    Montpellier T9 -> Carthago T350
    Toulon     T42 -> Hellas T530  -> Rus T550
    Lille      T3  -> held throughout

**Gallia was destroyed entirely by other `ai/deep` seats.** Hellas took
four cities in the T45-T80 window; Carthago took the three best
(Marseille, Toulouse, Montpellier) between T223 and T350. Not one
Gallian city was ever lost to a stock player.

### 2.2 Did it strangle itself, or was it strangled? Both, in that order

*Strangled by geography, mildly*: Gallia's owned land was 55 tiles at
T10 and 58 at T60 while Rus went 69→127 and Hungaria 60→116. It never
had room. But it had eight cities on T40 — a normal start — and lost
them.

*Self-strangled, decisively*: what it built. From the autosaves, its
whole-empire production menu:

    T20  Colosseum, Pikemen×4, Diplomat
    T30  Pikemen×6, Diplomat, Settlers
    T40  Pikemen×4, Settlers×2, Diplomat×2      <- Hellas invades T45
    T50  Diplomat×2, Colosseum×2, Pikemen×2
    T60  Colosseum, Pikemen×2, Diplomat×2, Settlers
    T80  Colosseum, Diplomat×3, Musketeers      <- five cities left
    T100 Colosseum, Diplomat×2, Barracks II, Musketeers
    T250 Musketeers×2, Diplomat×3
    T500 Riflemen×2, Knights                     <- three cities
    T601 Barracks II, Riflemen, Diplomat

Two pathologies, both visible from turn 20.

**Diplomats.** Gallia switched production to Diplomat **191 times**.
Across the cohort, `Diplomat` is the single most frequent
`deep-prodswitch` target in the game (857 of ~11 000 switches) and the
second most frequent `deep-want` pick (2437). The wants are enormous:
median `deep-want` value for a Diplomat pick is **3524**, p90 7355, max
16 467, against a building anchor whose corpus median is 91-191
(`want-corpus-2026-08-06.md` §1). The channel is
`dai_choose_diplomat_offensive` plus deep's embassy-science credit at
`deepdefence.c:3264`, whose reachability guard asks only "shared
continent, or do we own a boat" — it never asks whether an embassy can
be established with a player we have been at total war with for 400
turns. It cannot, so the want never clears, so the city builds another
one. **Gallia gained two embassies between T100 and T601** (4→6) while
holding 3-7 diplomats permanently; Rus gained four (3→7) while
accumulating 17 diplomats and 246 diplomat switches. This is verbatim
the disease the code's own comment says it cured for the island case
("an unsatisfiable want … pumps units forever; seed 205 built 11
diplomats on an island with no boats") — cured for *unreachable*, not
for *unenterable*.

**Colosseum before Temple.** Colosseum is deep's 5th most-wanted build
(2050 picks) and appears in Gallia's queue at T20, 50, 60, 80, 100 — in
cities of size 1-8, at 70 shields and 3 gold upkeep, when a Temple is
30 shields and 1 gold, and gives content earlier and History all the
same. Gallia's gold sat between 8 and 200 for the entire game, and below
60 at every sample after T140.

**And no walls until it was over.** `deep-def` for Gallia: mean
defenders per city 0.05 (T0-24), **0.40 (T25-49, the window Hellas
invaded in)**, 1.24 (T50-74). It held 13 Pikemen at T40 across 8
cities; on average fewer than one of them was in a city. City Walls
first appear in a Gallian queue at T200, at want 1603, in Rouen — a
city it had already lost once and just retaken.

**Its threat model said it was fine.** Mean `deep-surv` p for Gallian
cities: 0.898 (T50-74), 0.963, 0.985, 0.916, **0.978 (T150-174) while
`deep-def danger` averaged 1073**, 0.993 at T225-249 with danger 4452,
0.973 at T275-299 with danger 4512. Two threat signals in the same
binary, disagreeing by orders of magnitude, for 500 turns.

---

## 3. Osmanli and Iceland: the two middles that were never in a game

**Osmanli (419).** Peaked at 10 cities on T56, held them 88 turns, then
bled 10→9→8→**7** between T144 and T192 — pop 76→46 — and never got
above 9 again. It built Colosseums from beginning to end: **six of its
ten cities were building a Colosseum simultaneously at T80**, and a
Colosseum is in the queue at T40, 80, 200, 260, 300, 400 and 500. It
finished **zero wonders in 600 turns** (its one serious attempt,
Copernicus' Observatory, is in a `deep-fallback` line at T48 with 106
shields in the box). It paid the switch tax 3108 times — second-highest
in the cohort — which is the mechanical statement of "started the wrong
thing, then was charged for noticing". 38 improvements across 8 cities
at the end; history 3808.

**Iceland (474).** The opposite failure: it *did* expand, to 16 cities
by T192 — more than anyone in the game — and then lost six of them
between T256 and T363. Its final unit inventory is the whole story:
**57 Marines out of 73 units**, in an eleven-city empire, at T601. The
naval invasion go/no-go evaluated eleven times for Iceland and said
*hold* eleven times (p = 0.003, 0.045, 0.189 …). Across all seven deep
seats there were **117 holds and 6 gos in 600 turns**. Nothing ever
told the build side that the operation the Marines exist for has not
launched once in five centuries. Iceland's culture ended at 5532 — its
45 improvements over 11 cities is the thinnest development of any seat
that held double-digit cities.

Both seats also paid the diplomat tax: Iceland 198 diplomat switches,
12 diplomats standing at the end.

---

## 4. Carthago: what the winner actually did

Carthago is not a better-tuned version of the others. It played a
different game, in three acts.

**Act I (T1-T126): expand, then stall.** 8 cities by T8, then 9 by
T126 — and then **96 turns of nothing** (9 cities T126→T222). Its
early build is unremarkable except in one respect: it built hulls.
21 Triremes at T80, 13 Triremes + 9 Frigates at T140. Its
mean available boat count over the whole game is **10.8**, against
1.1 (Gallia), 3.2 (Anglia), 4.0-4.6 (Hellas, Iceland, Osmanli) and
7.7 (Rus).

**Act II (T223-T374): the conquest engine.** 9 cities → 20 in 150
turns. Marseille and Toulouse (T223-225), Gdańsk from Polska (~T280),
Borgarnes from Iceland, Tønsberg from Norge, Białystok from Polska,
Montpellier (T350), Dundee from Scot. `unitskilled` 55 (T220) → 141
(T300) → 308 (T400). `mfg` 44 → 109 → 127. It ran **248
`deep-raid` events**, the most in the cohort, on a Galleon/Ironclad +
Rifleman/Cavalry package that upgraded to Transport/Marines/Battleship
by T350. Note what it did *not* use: the invasion op fired *go* for
Carthago zero times. The wins came from the raid/sortie path, not the
operations layer.

**Act III (T374-T601): hold and compound.** City count flat at 18-20;
`history` 11 871 → 21 535. Leptis Magna ends at size 24 with 23
improvements and 7074 history. Toulouse — taken from Gallia at size 15
— ends at 22 with 16 improvements and 3307 history. Gdańsk, taken from
Polska with a **2445-history stock already on it**, ends at 6408. That
capture is visible from Polska's side as a culture *drop*, 5592 (T240)
→ 4644 (T280); Carthago inherited a compounding annuity and ran it for
320 more turns.

What Carthago did right, stated as differences from its cohort:

1. **It bought sea mobility before it needed it**, and was the only
   seat with enough hulls to be opportunistic for 400 turns.
2. **It took developed cities, not empty ones.** Its top three cities
   at the end include two conquests, both captured large and
   building-rich.
3. **It barely paid the two cohort taxes**: 544 switch-cost debits
   (Hellas 3234, Osmanli 3108) and 37 diplomat switches (Rus 246,
   Iceland 198, Gallia 191).
4. **It kept a real capital.** Leptis Magna went from 3 improvements at
   T80 to 23, uninterrupted, for 520 turns.

And what it still got wrong, because it matters for the prescription
list: 14 Diplomats standing at T601; Leptis Magna, its best city, on
`Coinage` for hundreds of turns from T200 (154 of the game's 345
"every producer bid zero" city-turns are Carthago's); and its own
aqueduct coverage is only average.

Anglia (828, 4th, and the highest score-per-city in the game) is the
control that proves the point from the other side: **six cities all
game, one conquest, zero raids that mattered, 8 wonders, 52
improvements over 7 cities, history 20 739.** It out-scored five stock
seats by building.

---

## 5. The team gap: where the 560 points went

Decomposed at T600 (score = citizens + 2×techs + 5×wonders + built/10 +
kills/3 + culture/50):

    cohort   score  citizens  techs×2  wond×5  built/10  kills/3  cult/50
    deep      4361       602     1024      65       287      591     1792
    stock     4921       609      974      75       292      818     2153
    delta     -560        -7      +50     -10        -5     -227     -361

**The entire gap is two terms: culture (-361) and kills (-227).**
Citizens, techs, wonders and units built are within noise. Deep was not
out-grown, out-teched or out-produced — cohort `mfg` at T600 is 377
(deep) vs 374 (stock).

### 5.1 Culture (-361): §1, and the churn on top of it

The per-city History flow gap (1.41 vs 2.92) is the flow half. The
stock half is that deep's cities changed hands constantly, and a city
in transition builds nothing and loses improvements.

### 5.2 Kills (-227): deep attacked, stock defended

Whole-game unit ledger, from the score log:

    cohort  units lost  units killed  ratio
    deep          2331          1779   0.76
    stock         2403          2454   1.02

Deep's exchange ratio is **0.76**; stock's is **1.02**. Deep is the
attacker in this world, and the score pays 1/3 point per kill, so
attacking is charged twice — once in the units, once in the metric.
(The combat E2 work that prices expected attacker losses landed at
03:24 that morning and *is* in this binary; `conquest_worth_mult` is
not.)

### 5.3 The thing underneath both: deep spent its war on deep

Ownership transfers observed at 10-turn granularity (a lower bound —
transfers that reversed inside one window are invisible):

    deep -> deep   32        stock -> stock    4
    stock -> deep  16        deep -> stock     9
    barb involved   3

**Half of all conquest in this world was deep taking cities from deep.**
Deep beat stock on the cross-cohort exchange (+16/-9) and then spent
twice that effort eating itself. And it is not because deep declared
more wars: the war census says both cohorts were at near-total war —
14 of 21 deep-deep pairs and 16 of 21 stock-stock pairs, from T100 to
T601 — the difference is that **deep converted its wars into conquest
and stock's were inert.**

Gallia is the accounting entry: 190 points, on land that Carthago and
Hellas turned into 1093 and 632. On a cohort total, that is a wash
minus the buildings burned and the units lost taking it.

### 5.4 And nobody ever made peace after turn 97

`deep-peaceseek` fires between T29 and T97 and **never again**; almost
every line reads `sent=0` with `why=love / allyswar / enemyally /
countdown`. Comparing the diplomatic state across all 61 saves: after
**T150 not one pair anywhere in the world changed state** — 66 war
pairs and 25 peace pairs, frozen for 450 turns. The PEACE work (the
cease-fire window pre-pass, the love bar removal, the embassy
re-pricing) is in this binary and it halved the cease-fire trap in its
own A/B; it has no reach into a war that has been static since T97.

### 5.5 Iberia (942) and Hungaria (871) vs Rus (728) and Hellas (632)

The honest like-for-like. Rus finished with **more** citizens than
Iberia (139 vs 91), more techs (79 vs 71), more cities (15 vs 11) — and
214 fewer points, because Iberia had 462 culture-points to Rus's 237
and 181 kill-points to Rus's 122. Hungaria beat Hellas 869-632 while
holding fewer techs (63 vs 79), on 454 culture to 261 and 120 citizens
to 64.

Stock's middle class won by being *boring*: it held the same cities for
500 turns, filled them with Temples, Marketplaces, Aqueducts and Banks,
and let the compounding do the work. Deep's middle class spent the same
600 turns fighting its neighbours to a draw. The metric pays the first
strategy.

**Confounds, restated so this section is not over-read:** one seed; a
map at a quarter of the target density and double the maximum measured
horizon; the switch tax paid by deep only, 10 239 times, zeroing the
want in 74% of them; class scales, emergency cap and trade merit all at
their inert defaults; and no ENDGAME mechanism in either AI. A cohort
delta of -560 on one game is not a measurement, and none of the
mechanism diagnoses above depend on it being one.

---

## 6. Prescriptions: symptom → diagnosis → owner

Owners are session briefs in `doc/deep-ai/sessions/` per the README
ladder; **NEW** means no brief on the ladder covers it.

| # | Symptom (with the number) | Diagnosis | Owner |
|---|---|---|---|
| 1 | Deep 7 Temples / 6 Marketplaces vs stock 28 / 34 at T601; History flow 1.41 vs 2.92 per city | The horizon has credit terms for cap-lifters, growth, bundles and health and **no term at all** for a Temple or a Marketplace; they get stock's base want and lose to injected fear | **Orphan.** Diagnosed in `HANDOFF-2026-08-01.md` §3 with a prediction to test; no session on the ladder owns it. Adjacent: class rescale (#6) rescales classes, not this |
| 2 | 22/70 deep cities have any aqueduct vs 47/65 stock; **13 deep cities parked at exactly size 8** | `deep_capacity_credit()` bails whenever the food box is pinned or the CM sheds food to surplus 0 — the credit refuses to pay in exactly the city it was written for | **D3 / D2** (`mechanism-map.md` D-C, counterfactual-ttg fix). Buy half: `findings/growth-emergency.md` |
| 3 | Diplomat is the #1 production switch in the game (857); median want 3524 vs a 91-191 anchor; Gallia +2 embassies in 500 turns holding 7 diplomats | The offensive-diplomat want's reachability guard tests continent/boats but not *enterability*; at permanent war the want is unsatisfiable and pumps forever | Partly **PEACE** (embassy re-priced as research discount, landed) and `emergency_value_cap` (landed, **default 0.0 = off**). The at-war satisfiability guard is **NEW** |
| 4 | Colosseum is the 5th most-wanted build, appearing in size-1 cities; Temple almost never | Want ordering inside the happiness ladder; Colosseum 70s/3g beats Temple 30s/1g on a want that ignores upkeep-adjusted content-per-shield and History | Same orphan as #1; the scale half is **class rescale (#6)** |
| 5 | 10 239 switch-cost debits, 74% zeroing the want; Hellas 3234 and Osmanli 3108 are two of the three worst deep seats | The switch price is correct and landed **before** the class rescale and before the corpus verdict was acted on: its cost is proportional to the error rate of the *initial* pick, so it locks in miscalibrated first choices | **HEDGE** (landed) + **class rescale (#6)**: re-measure the price after the rescale; human call keeps `switch_cost_gain` at 1.0 |
| 6 | Iceland: 57 Marines standing, 11 invasion evaluations, **0 launches**; cohort-wide 117 holds / 6 gos | The op-level go/no-go works; nothing feeds "this op has never launched" back to the build want that manufactures its passengers | Go/no-go is **ladder #8**. The build-side feedback loop is **NEW** |
| 7 | Gallia mean 0.40 defenders per city in the window it was invaded (T25-49) while holding 13 Pikemen | Garrison assignment, not garrison sizing: units exist and are not in the cities | **G1** (`S-G1-garrison-floor.md`) sizes the floor; the *placement* half needs stating in that brief |
| 8 | `deep-surv` p = 0.97-0.99 for Gallian cities across 400 turns in which they kept falling, while `deep-def danger` read 1000-13 000 | Two threat verdicts in one binary that never reconcile | **D2** ("one shared threatened verdict", `mechanism-map.md`) |
| 9 | No diplomatic state changed anywhere after T150; `deep-peaceseek` silent after T97 | The peace pre-pass is anchored on the cease-fire window; a war frozen for 400 turns is never re-approached | **PEACE** (landed, incomplete). The re-approach path for a static war is **NEW** |
| 10 | Osmanli 0 wonders in 600 turns; deep 13 wonders vs stock 15, and 8 of deep's 13 are Anglia's | No race awareness; shields sunk into wonders that never complete | **WRACE** |
| 11 | 345 city-turns where every producer bid zero (Anglia 175, Carthago 154); Leptis Magna on Coinage from T200 | The auction has nothing to sell a developed city in the late game | **ENDGAME** (score-dense sinks) + **HEDGE** (liquidity legs) |
| 12 | Founder bid = 0.0% of city-turns from T150; 377 free tiles, of which 309 Glacier | **Not a defect.** The map was full. Any future reading of "deep stopped expanding" must first check the free-land terrain mix | Measurement hygiene; cite §0(b) |

---

## 7. The prize: gaps no brief covers

**N1. Culture is unpriced, it is the plurality of the metric at long
horizons, and it is an early-game build-order lever, not an endgame
one.** Culture/50 is 29.5% of world score at T300 (the longest scoring
cell) and 42.5% at T600, against citizens at 21.6% and 13.0%. It
accrues from Temple/Library/Colosseum/Cathedral/University/Research
Lab/Palace and wonders, and `city_history_gain()` compounds the stock,
so the value of a Temple built on turn 40 is not the value of the same
Temple on turn 400. Three separate places on record deprioritise it —
`deepstance.h` ("already priced by stock"),
`deep-dive-2026-08-05/economy-effects.md:64` ("report, don't lead"),
`S-ENDGAME-horizon.md` §3 ("what NOT to do") — and all three were
reasoned at a horizon where the term is 7-15% of score.
`S-DARKFX-dark-effects.md` item 7 is the nearest owner and it is
scoped as a "border-push check … if any are dark and material". They
are material. **This is a sizing error in a human-adjacent call, not a
missing mechanism; it needs to go back to the human with the table in
§1 rather than be quietly re-scoped by a session.**

**N2. Nothing prices the culture stock a conquest transfers.** Gdańsk
carried 2445 history when Carthago took it and 6408 by the end; Polska's
culture visibly fell in the same window. `conquest_worth_mult` (landed
after this game) doubles the *city worth* to count denial, but city
worth is built from what the city produces for us, not from the
compounding annuity sitting on it. Capturing a 20-improvement city and
capturing a size-6 city with two buildings are not the same act and the
want cannot tell them apart.

**N3. No closed loop from "the operation never launches" to "stop
building its passengers."** 117 holds, 6 gos, 57 Marines. The go/no-go
is a per-decision gate; there is no per-empire feedback that an entire
production line has produced inventory for 400 turns with a
zero-launch rate. The same shape would catch the diplomat pump (N-adj
#3): **a build want that has never been consumed by the mechanism it
feeds should decay.** Neither the pump guard in `deepdefence.c:3306`
(a static reachability test) nor the emergency cap (a value clamp) is
this; both are open-loop.

**N4. The harness cannot see any of this.** No scoring config exceeds
`ENDTURN=300`; none exceeds 8 players; `cramped.conf` covers the
density (90 tiles/player, 3 players, 300 turns) but not the horizon.
Every acceptance verdict in this project has been taken in a regime
where the metric is 36% techs and 35% citizens. At 600 turns it is 43%
culture and 13% citizens. **The mechanism set has never been scored in
the regime where its worst gap is the largest term.** A long cell —
even one, unpaired, as a watch cell — would have surfaced N1 a month
ago.

**N5. Mixed seating measures cohort politics, not AI strength — and
this game shows what that costs.** 32 of 64 city transfers were
deep-on-deep against 4 stock-on-stock, with identical war-declaration
rates (14/21 vs 16/21 pairs). Deep's distinguishing competence — it
actually prosecutes the wars it declares — is scored as a cohort
liability when the cohort is the unit of account. `benchmark/README.md`
non-negotiable #3 already says the primary metric is score ratio in
*separate* games; this derby is the demonstration of why, and the
demonstration is worth keeping. It is **not** an argument for teaching
deep who its cohort is: in a real game there is no cohort. It is an
argument for never quoting a mixed-seating cohort total as a strength
measurement, including in this document.

**N6. Colosseum-vs-Temple is an upkeep-blind comparison, and deep is
chronically broke.** Deep seats ran on 5-160 gold for the whole game
(Gallia 8-130, Anglia 0-97, Hellas 5-157) while stock banked thousands
(Hungaria 26 849, Iberia 6784). Deep is not converting gold into
anything — it is not solvent enough to. The want that picks a 3-gold
Colosseum over a 1-gold Temple in a size-1 city is the mechanism; the
empty treasury and the 345 zero-bid city-turns are the symptom at the
other end. DARKFX item 3 (buy on gold-per-shield) assumes there is gold
to spend.

---

## 8. What this document does not establish

- **Nothing about deep-vs-stock strength.** One seed, one map at a
  quarter of the target density, double the measured horizon, and deep
  paying a 10 239-debit tax stock does not pay. The cohort delta is
  reported for its *composition* (§5), which is a structural reading,
  not for its sign.
- **Nothing about the mechanisms landed after 08:50** — combat E2's
  loss-charging is in this binary; `conquest_worth_mult` is not.
- **Nothing about the tuned regime.** Every knob here is a compiled
  placeholder, and the three corrections most relevant to §2-§4
  (class scales, `emergency_value_cap`, `trade_merit`) were all at
  their inert defaults. Turning the emergency cap on alone would clip
  the offensive-diplomat wants that drove §2's central pathology.
- **Turn resolution is 10 turns** for anything read from the autosaves;
  the conquest ledger is a lower bound.
