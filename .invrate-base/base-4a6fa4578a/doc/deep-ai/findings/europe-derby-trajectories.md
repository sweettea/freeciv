# Europe derby: the seven deep nations' trajectories (2026-08-06)

Forensic read of the finished Europe head-to-head derby — 7 `ai/deep` vs
7 `ai/default` ("classic"), 600 turns, the first deep game ever taken to
the late era. This half covers the **deep nations' economic arcs and
production plans**. Diplomacy/coalitions are a companion report.

Material (session scratchpad, not in-tree):

- `europe/freeciv-score.log` — SCORELOG2, 31 tags × 14 players × 601 turns
- `europe/saves/` — 61 autosaves, `europe-derby-T####-*.sav.zst`
- `europe/game.log` — 66 MB deep instrument stream
- `europe/derby.serv` — setup

Final standing (T600, `score` tag):

| # | nation | score | cities | pop | techs | wonders | mfg | bnp | built | lost | killed |
|---|--------|------:|-------:|----:|------:|--------:|----:|----:|------:|-----:|-------:|
| 1 | **Deep_Carthago** | 1092 | 18 | 180 | 87 | 4 | 126 | 210 | 544 | 415 | 459 |
| 2 | Def_Iberia | 940 | 11 | 91 | 71 | 2 | 54 | 114 | 548 | 477 | 545 |
| 3 | Def_Hungaria | 869 | 10 | 120 | 63 | 1 | 108 | 135 | 612 | 463 | 310 |
| 4 | **Deep_Anglia** | 827 | 7 | 66 | 87 | 8 | 37 | 75 | 212 | 134 | 93 |
| 5 | **Deep_Rus** | 728 | 15 | 139 | 79 | 1 | 84 | 134 | 677 | 557 | 366 |
| 6 | Def_Scot | 726 | 10 | 113 | 76 | 2 | 38 | 114 | 389 | 293 | 356 |
| 7 | Def_Norge | 690 | 12 | 89 | 71 | 5 | 43 | 102 | 421 | 363 | 373 |
| 8 | **Deep_Hellas** | 632 | 8 | 64 | 79 | 0 | 54 | 76 | 301 | 250 | 358 |
| 9 | Def_Italia | 577 | 8 | 72 | 57 | 1 | 61 | 77 | 327 | 237 | 215 |
| 10 | Def_Germania | 563 | 5 | 51 | 78 | 3 | 36 | 70 | 234 | 195 | 256 |
| 11 | Def_Polska | 556 | 9 | 73 | 71 | 1 | 34 | 87 | 428 | 375 | 408 |
| 12 | **Deep_Iceland** | 474 | 11 | 80 | 63 | 0 | 25 | 75 | 579 | 468 | 305 |
| 13 | **Deep_Osmanli** | 418 | 8 | 56 | 70 | 0 | 39 | 73 | 309 | 248 | 164 |
| 14 | **Deep_Gallia** | 190 | 3 | 17 | 47 | 0 | 12 | 23 | 286 | 259 | 34 |

Deep takes 1st, 4th, 5th, 8th, 12th, 13th, 14th. Team score 4361 vs 4921.
**Deep lost the derby on aggregate and won it on the top slot.**

---

## 0. Geometry warning — read before reusing any script

`derby.serv` asks for `topology WRAPX` / 128×88, but the ruleset overrode
it. The actual map (see the settings table in any save) is:

    "topology","ISO|HEX"       "wrap","WRAPX|WRAPY"
    "xsize",46                 "ysize",92

So: **iso-hex, wrapping on both axes.** Consequences that bit me and will
bite the next reader:

- Adjacency is 6-neighbour, not 8. In native coords the valid steps are
  `(0,-1) (1,-1) (-1,0) (1,0) (-1,1) (0,1)` — iso-hex forbids NW `(-1,-1)`
  and SE `(1,1)` (`common/map.c:is_valid_dir_impl`).
- Distance is `map_vector_to_real_distance`, not Euclid, and
  `map_vector_to_sq_distance` returns `d*d` on hex maps. So the default
  `city_radius_sq=5` is a **19-tile** work radius, not the 21-tile fat
  cross — and it is a different 19 tiles than `dx²+dy²<=5` gives.
- `y` wraps too.

My first pass used square geometry and produced a landmass of 154 tiles
under Anglia (two islands glued through an illegal diagonal) and a
5-tiles-wrong work radius. **Every placement number below is the redone
iso-hex version.** This is the same class of error as the correction in
`europe-T80.md` §2; the analysis scripts keep making it.

Correct flood-fill gives 38 landmasses: `179, 177, 135, 134, 69, 69, 69,
69, 68, 68, 68, 68, 67, 67, 67, 66, 64` then rubble. 1532 land tiles,
36% of the map. Twelve nations got a private ~67-tile island; Iceland
and Polska share the 135; Carthago's island later hosted intruders.

---

## 1. The load-bearing question: does deep's slowness buy placement?

**No. Verdict: same cities, slower, fewer, and tighter-packed.**

The premise — deep expands slower than stock — is confirmed but is
*narrower* than it looks. Cumulative cities **ever founded**:

    turn      5   10   20   25   30   35   40   50  100  600
    deep     33   56   57   61   65   67   70   72   74   76
    stock    38   58   60   64   71   78   83   85   85   89

The first wave (cities 1–7) is a near-tie: deep is 0.5–1.0 turns behind
per city. **The entire deficit opens between T25 and T45** — stock's
second settler wave. After T50 both teams stop; deep founds 2 more cities
in 550 turns, stock 4.

Now the three quality tests.

### (a) Site quality at matched founding rank

Pooled over every own-founded city ever (deep n=75, stock n=86), using
the true 19-tile iso-hex work radius:

| metric (mean per city) | deep | stock |
|---|---:|---:|
| founding turn | 18.97 | **16.59** |
| workable land tiles | 10.57 | **10.69** |
| grassland+plains tiles | 6.21 | **6.60** |
| resource specials | 1.77 | **1.83** |
| river tiles | 1.03 | **1.14** |
| hills+mountains | **1.95** | 1.93 |
| coastal | 0.92 | 0.92 |
| centre on grass/plains | 0.77 | 0.77 |

Deep is worse or tied on seven of eight, and 2.4 turns later on average.
Rank-matched (deep's k-th city vs stock's k-th city) shows no rank where
deep is systematically better — deep leads on `good` at ranks 1, 2, 6, 10
and trails at 3, 4, 5, 7, 8, 9, 11, 13, 14. It is noise around zero.

### (b) Density on matched islands

Restricting to nations whose home island held **no foreign city** — four
deep, four stock, islands of 64–69 tiles, i.e. a controlled comparison
(T0060 save):

| nation | team | island land | cities | land/city | island covered |
|---|---|---:|---:|---:|---:|
| Osmanli | deep | 67 | 8 | 8.4 | 83.6% |
| Hellas | deep | 68 | 7 | 9.7 | 83.8% |
| Anglia | deep | 64 | 6 | 10.7 | 89.1% |
| Rus | deep | 68 | 8 | 8.5 | 83.8% |
| Norge | stock | 68 | 9 | 7.6 | 95.6% |
| Hungaria | stock | 69 | 8 | 8.6 | 89.9% |
| Iberia | stock | 69 | 10 | 6.9 | 89.9% |
| Scot | stock | 66 | 9 | 7.3 | 90.9% |
| **mean** | **deep** | 67 | **7.2** | 9.3 | **85.1%** |
| **mean** | **stock** | 68 | **9.0** | 7.6 | **91.6%** |

Same island size, 1.8 fewer cities, ~10 tiles never worked vs ~6.

### (c) Spacing

Mean real (hex) distance from each city to its nearest own city:

    T0060   deep 3.65   stock 4.06
    T0100   deep 3.96   stock 4.19
    T0300   deep 4.18   stock 3.45

Deep's cities sit **closer together** than stock's while being fewer.
That kills the "deliberately spread for better sites" reading outright:
if deep were paying turns for quality it would place *farther apart* and
score higher on the site metrics. It does neither. It clusters near the
capital and stops.

### The per-city-output mirage

Deep does look better per city early — and it is an artifact:

    tag/team          T20    T40    T60    T100   T200   T400   T600
    pop/city  deep    3.58   4.52   5.85   6.35   7.18   8.53   8.60
              stock   2.75   3.73   4.99   6.76   8.27   8.75   9.37
    bnp/city  deep    5.15   5.63   5.93   7.00  10.34   9.64   9.51
              stock   4.55   4.63   5.12   7.11  10.29  10.07  10.75
    total pop deep     186    280    398    432    488    614    602
              stock    165    295    389    507    620    595    609

Deep's per-city lead at T20–T60 is a **selection effect**: the average is
taken over its 7–8 best sites while stock's average is dragged down by
its marginal 9th–12th. Compare cities at the same founding rank (T0100
save) and the lead vanishes — deep rank-1..5 mean size 8.14/7.71/6.86/
7.29/6.29 vs stock 7.86/8.29/8.29/8.00/7.86. Deep's cities are *not*
bigger at matched rank; it just has fewer bad ones. Total pop, the thing
that actually scores, is behind from T40 to T350.

**Plain-words verdict for the human: same cities, slower.** The delay is
not bought quality. What deep actually bought with those turns was 1.8
fewer cities per island and 15% of its home island permanently unworked.

---

## 2. Cross-nation instrument table

Whole-game totals from `game.log` (production-switch targets — what the
city *chose to build next*, not completions) and saves.

| | Carthago | Osmanli | Gallia | Hellas | Anglia | Rus | Iceland |
|---|---:|---:|---:|---:|---:|---:|---:|
| prodswitch events | 2178 | 1281 | 785 | 1677 | 1129 | 1945 | 2039 |
| **shields forfeited to switches** | **15 686** | **58 238** | 8 180 | **58 730** | 32 225 | 8 758 | 8 974 |
| — of which "dom" (economy) | 9 734 | 40 901 | 5 736 | 43 239 | 23 930 | 6 796 | 6 466 |
| buildings held T600 | **159** | 38 | 9 | 49 | 52 | 80 | 45 |
| gov changes / anarchy turns | 18/9 | **48/24** | 18/9 | **41/21** | 42/21 | 14/7 | 22/11 |
| settler "pick" events, whole game | 21 | 29 | 14 | 15 | 16 | 22 | 25 |
| — of which after T100 | 0 | 2 | 0 | 4 | 2 | 0 | 3 |
| Settlers chosen | 37 | 38 | 38 | 44 | 27 | 48 | 50 |
| Workers+Engineers chosen | 4 | 0 | 0 | 4 | 7 | 8 | 5 |
| Diplomat+Spy chosen | 81 | 106 | **191** | 65 | 63 | **246** | **198** |
| Marines chosen | 47 | 42 | 0 | 18 | 3 | 87 | **304** |
| Coastal Defense chosen | 87 | 21 | 13 | 85 | 22 | 49 | **165** |
| Battleship+Cruiser+AEGIS | **558** | 129 | 0 | 146 | 60 | 341 | 88 |
| Library+University chosen | 15 | 85 | 3 | 69 | 38 | 16 | 16 |

Stock comparison for buildings held: Hungaria 128, Iberia 107, Scot 80,
Norge 71, Italia 70. **Team totals T600: deep 432 buildings over 70
cities (6.2/city), stock 553 over 65 (8.5/city).** At T100 it was deep
109/68 = 1.6 vs stock 200/75 = 2.7. Deep is chronically under-built.

---

## 3. Per-nation arcs

### Deep_Carthago — 1st, 1092. Conquest-financed, and the only deep nation that also built.

    turn      10   30   50   80  100  126  200  250  300  350  380  600
    cities     8    8    8    7    7    9    9   11   15   17   20   18
    pop       11   38   50   41   39    -   65   93  131  145    -  180
    mfg       18   31   39   36   21    -   41   79  109  107    -  126
    buildings  -    -    3    -    8    -   31   -    88    -    -  159

Three regimes.

1. **T1–T29 land grab, then a 100-turn freeze.** Nine cities founded by
   T29, then nothing — ever. Its own island (landmass 32, 69 tiles) held
   7 Carthaginian cities and by T0100 also one Polska and one Scot city
   planted on it. Carthago lost a home city at T78 and its mfg halved
   (39→21 between T80 and T100) — the trough is an invasion, not an
   economic choice.
2. **T100–T222 grind.** Retakes the island (holds a captured Scot city
   from T130, `T0130` save), builds the first wonder at T70 and the
   second at T140. Cities 7→9. This is the only deep nation whose
   *buildings* curve climbs steeply here (8 → 31 by T200).
3. **T223–T380 the boom.** 9 → 20 cities in 157 turns, entirely by
   conquest and **mostly at deep expense**: 4 cities originally Gallia's
   (T343–T354), 3 originally Iceland's (T266–T298), plus Polska 2,
   Scot 1, Germania 1, Norge 1 (`T0380` save). It switches to Communism
   at T250, right in the middle. Naval regime from T350: 292 Battleship,
   204 AEGIS Cruiser, 101 Transport, 129 SDI, 125 Airport chosen.

Its distinguishing habit is **discipline**: 544 switch events and only
15 686 shields forfeited over 600 turns — one quarter of Osmanli's
churn with twice the cities. Opportunity cost with hindsight: the
T30–T126 freeze. It had nine cities and a 69-tile island for 100 turns;
one more settler wave into the 15 uncovered tiles (T0100: 81.2%
coverage) would have compounded for 500 turns.

### Deep_Anglia — 4th, 827, on seven cities. The tall outlier.

    turn      10   35  100  200  300  400  500  600
    cities     7    6    6    6    7    7    7    7
    techs     21   23   29   47   66   83   87   87
    wonders    0    0    0    2    4    7    8    8
    techout    -    -   27  124  262   72  223   48

Founded 8 cities (T1–T34) on a 64-tile island, lost two early, and never
founded another. Its island coverage is **89.1%** — the best of any deep
nation. It is not blocked by land: it is blocked by never going
overseas. `deep-colony` demand was evaluated only 11 times in 600 turns
(last T488) and Anglia's settler pipeline emitted 2 picks after T100.

It converted the constraint into a science/wonder economy: 87 techs
(tied best in the game), 7 wonders still held at T601 (Apollo, Internet,
Newton's, Manhattan, Michelangelo, Temple of Artemis, UN), Democracy
T140–T400. Built 212 units all game — by far the fewest — and lost 134.
Score-per-city 118, the highest in the game (Carthago 61, Iberia 85).

Opportunity cost: **the tall play was correct given seven cities and
wrong given the map.** Its eight sites average 12.0 workable land tiles,
the most of any nation in the derby, and it sat on them. Anglia is the
cleanest proof that the settler pipeline dying at T100 (§4) costs
real score — a nation that plays every other system well still caps at
4th because it has 7 cities.

### Deep_Rus — 5th, 728. Fastest expander, then permanent attrition.

    turn      10   29   41   72  138  153  182  259  304  600
    cities     9   13   14   15   16   15   12   14   13   15

Best deep opening by a distance: 14 cities by T41, 15 own-founded by T72
— more than any nation in the game except Iberia (16). At T60 its home
island coverage was **100%**, the only nation to fully cover its island.
Peak pop 107 at T140, bnp 139.

Then it stopped and bled. T138–T182 it drops 16 → 12: Iceland is holding
2 Rus cities by T0180 and Anglia one by T0310. From T150 its production
becomes near-pure infantry-and-spy churn: **97 Musketeers and 69
Diplomats chosen in T150–T199 alone**, whole-game 200 Musketeers and
246 Diplomat/Spy — the highest espionage spend in the game, on a nation
that captured 6 cities all game. 677 units built, 557 lost: it built and
lost a unit every turn for 600 turns. 80 buildings across 15 cities.

Opportunity cost: it had the map position to win. Its share of
production that went to Diplomats alone would have paid for ~50
buildings. Techs 79 vs Carthago's 87 with equal city counts is the tell.

### Deep_Hellas — 8th, 632. Early conquest, then 400 turns of coastal defence.

    turn      11   43-77  124  160  200  326  403  546  600
    cities     8   →13     12   13   12    9    11    8    8

Real early success: 8 cities by T11, then **eats Gallia** — 4 Gallian
cities captured T43–T80 (`T0080` save), plus one Italian. Peak T160:
13 cities, pop 101, bnp 121, techs 45.

The inflection is T150–T200 and it is a *production* inflection, not a
military one. In T150–T199 its top choices are **Coastal Defense 41,
Riflemen 36, Colosseum 21** — 41 coastal-defence buildings for a
13-city nation. Whole game: 85 Coastal Defense, 108 Colosseum, 83 Port
Facility. It spent the mid-game fortifying and its bnp went sideways
from T160 (121) to T600 (76) while Carthago's tripled.

Two compounding self-inflictions: **58 730 shields forfeited to
production switches** (highest in the game, 74% of it on the economy
side) and **41 government changes / 21 anarchy turns**. It ran
Communism↔Democracy ping-pong from T260 to T600, flipping 14 times.

Rus took two Hellenic cities (T0330, T0490) and the T544–T546 collapse
(11→8) ends it. Zero wonders.

### Deep_Iceland — 12th, 474. The Marines nation.

    turn       9   60  111  192  256  298  363  521  600
    cities     9   13   13   16   15   11   10   12   11

Shares landmass 1 (135 tiles) with Polska and later four others, so its
"coverage" numbers are contested rather than idle. Peak T192: 16 cities,
including 3 originally Germania's and 2 originally Rus's. Best deep bnp
per city at T200 (135 total).

Then Carthago dismantles it: 3 Icelandic cities in Carthago's hands by
T0300, a fourth by T363. Iceland's answer was to build an amphibious
army and never land it:

    era        T300-349  T350-399  T400-449  T450-499  T500-549  T550-599
    Marines        29        63        57        48        48        59

**304 Marines chosen after T300, and its city count at T300 (11) equals
its city count at T600 (11).** Add 165 Coastal Defense and 189 Frigate
whole-game. It ends with 59 live Marines (T0600 save), 45 buildings
across 11 cities, 0 wonders, 63 techs — worst deep tech but for Gallia.
579 units built, 468 lost.

This is the sharpest opportunity-cost case in the derby: a permanent
standing invasion force that produced zero net territory over 300 turns,
funded by not building libraries.

### Deep_Osmanli — 13th, 418. Churn.

    turn      10   56  144  163  192  256  295  517  600
    cities     9   10    9    8    7    8    9    8    8

Solid opening (10 cities by T56 on a 67-tile island, 83.6% coverage),
peak at T140 (bnp 89, mfg 42, pop 76), then a 60-turn slide to bnp 51 /
mfg 17 at T200 as it loses three cities (T144, T163, T192) to Iceland
and Scot. It never recovers: bnp is lower at T600 (73) than at T140.

The mechanism is visible in two counters and they are the two worst in
the game:

- **48 government changes, 24 anarchy turns** — Republic ↔ Federation ↔
  Communism ↔ Democracy, flipping every 10–20 turns from T200 to T530.
- **58 238 shields forfeited to production switches**, 3108 switch
  events — 5.7× Carthago's switch count on half the cities.

It is the only deep nation whose `deep-colony` demand stayed hot the
entire game (61 samples, demand 400–601, last at T599) while founding
exactly one city after T55. Its build list is not stupid — Colosseum
143, Library 59, University 26, Port Facility 100 — it just never
finishes anything. 38 buildings held at T600 across 8 cities.

### Deep_Gallia — 14th, 190. Eaten twice.

    turn      10   43   51-77   200-203   223-225   343-344   530   600
    cities     7    9    →5        →7        →5       →3       2     3

Founded 10 cities (last T42) but was at war with Rus (T30), Carthago
(T40), Hellas (T40) and Iberia by T50 — four fronts before T50, from a
6-city base. Hellas took four of its cities T43–T80; Carthago took four
more T343–T354. It ends with 3 cities, 17 pop, 47 techs — 24 techs
behind the next-worst deep nation.

Its production plan is the most striking artefact in the derby.
Whole-game choices: **191 Diplomats**, 108 Riflemen, 85 Musketeers, 46
Pikemen, 16 Frigate, and **9 buildings held at T600**. From T250 onward
its `prodswitch` kind histogram is essentially pure `U` — T300–349:
`{I:2, U:83}`; T400–449: `{U:34}`; T500–549: `{U:17}`. It stopped
building infrastructure entirely for the last 350 turns and produced a
Diplomat roughly every four turns. It killed 34 units in 600 turns.

The instructive part is that Gallia's *sites* were fine — its ten
own-founded cities average 11.9 workable land tiles, 6.8 grass/plains and
2.0 specials, the second-best land of any nation in the derby and well
above both team means. It lost on everything downstream of placement.

---

## 4. Cross-nation patterns

### 4.1 The settler pipeline dies at T100 and the demand signal never notices

`deep-settler ... pick` events, whole game:

    Carthago 21   Osmanli 29   Gallia 14   Hellas 15
    Anglia   16   Rus     22   Iceland 25

**All but 11 of those 142 picks happen before T100.** Across seven
nations and 500 turns after T100 the deep settler system emitted eleven
site picks. Meanwhile `deep-colony` kept reporting overseas demand of
400–660 right through the end of the game:

| nation | colony samples | last sample | last with demand>0 |
|---|---:|---:|---:|
| Carthago | 1 | T3 | T3 |
| Osmanli | 61 | T599 | T599 |
| Gallia | 12 | T269 | T269 |
| Hellas | 65 | T600 | T600 |
| Anglia | 11 | T488 | T488 |
| Rus | 72 | T595 | T595 |
| Iceland | 68 | T597 | T597 |

Four nations spent 500 turns reporting "I want an overseas colony worth
~500" and shipped nothing. Three (Carthago, Gallia, Anglia) stopped
*evaluating* almost immediately — Carthago's last colony evaluation was
**turn 3**. Both failure modes are silent. Nothing in the instrument
stream flags "demand is high and supply is zero", which is why this ran
for 600 turns unnoticed.

Lift is not the bottleneck. `deep-lift` shows passengers-with-no-boat on
0–23 turns per nation across 600 turns; `wait>0` on 56–195 turns but
with waits of 8–23, which is a queueing cost, not starvation. The
settlers were never *built*: whole-game Settlers choices are 27–50 per
nation, essentially all before T60.

### 4.2 Production churn is the strongest single correlate of failure

Shields forfeited to mid-build switches vs final score:

    Hellas   58 730  →  632        Carthago 15 686  →  1092
    Osmanli  58 238  →  418        Iceland   8 974  →   474
    Anglia   32 225  →  827        Rus       8 758  →   728
                                   Gallia    8 180  →   190

Not monotone (Gallia is poor and quiet; Anglia churns and scores well),
but the two nations with 58k+ of churn are 8th and 13th, and the winner
churns least per city by a factor of four. The churn is *not* a size
artifact — Carthago had the most cities and the second-fewest switch
events (544 vs Osmanli's 3108).

The churn is concentrated on the **economy** side (`who="dom"`): 74% for
Hellas, 70% for Osmanli, 74% for Anglia. The military scheduler is
comparatively stable; it is the building queue that thrashes.

### 4.3 Government thrash tracks production thrash

    nation      gov changes  anarchy turns
    Osmanli          48           24
    Anglia           42           21
    Hellas           41           21
    Iceland          22           11
    Carthago         18            9
    Gallia           18            9
    Rus              14            7
    --- stock ---
    Germania         50           25
    Scot             28           14
    Polska/Hungaria  14            7
    Italia           12            6
    Iberia           10            5
    Norge             8            4

Deep team: 203 changes, 102 anarchy player-turns. Stock: 136 and 68.
Deep gives away ~34 extra turns of zeroed taxes and science, and the
same three nations (Osmanli, Hellas, Anglia) top both the government
churn and the production churn lists — one unstable preference signal
is plausibly driving both.

### 4.4 Deep under-builds infrastructure, everywhere but Carthago

    buildings held      T50  T100  T200  T300  T400  T500  T600
    deep team (7)        21   109   193   336   376   417   432
    stock team (7)       41   200   366   434   488   524   553

Deep is at 55% of stock's building stock at T100 and 78% at T600, and
159 of its 432 final buildings belong to Carthago alone. Strip Carthago
out and the remaining six deep nations hold 273 buildings across 52
cities (5.3/city) against stock's 8.5/city.

Worker counts tell the same story: whole-game Workers/Engineers *chosen*
are 0–8 per deep nation. At T0200 Gallia has 2 live Workers, Rus 3;
Hungaria has 12 and Iberia 16.

### 4.5 More than half of deep's conquests were of each other

Cities captured and held, by original owner (`T0380`, `T0600` saves):

- Carthago ← Gallia 4, Iceland 3, Polska 2, Scot 1, Germania 1, Norge 1
- Hellas ← Gallia 4, Iberia 1–2, Italia 1
- Rus ← Iberia 1–3, Hellas 2, Iceland 1, Gallia 1
- Iceland ← Germania 2–3, Rus 1–2, Norge 1–2, Osmanli 1, Gallia 1
- Anglia ← Rus 1; Osmanli ← Iberia 1

Roughly 19 deep-on-deep captures against roughly 15 captures from stock.
Deep-on-deep wars start early — Carthago–Gallia and Gallia–Hellas both
by T40 (`T0040` save) — and by T100 essentially every pair is at war.
Carthago's winning boom is 7 of its 12 held captures taken off Gallia
and Iceland. The team's aggregate loss to stock is partly just this:
deep spent its military production on the deep team.

### 4.6 The regime sequence is the same for six of seven

Reading the `prodswitch` top-lists era by era, six deep nations follow
one script:

1. **T0–T50** Settlers + a cheap happy building (Colosseum) + Phalanx/Pikemen
2. **T50–T150** Musketeers + City Walls + Barracks II + a galley class
3. **T150–T250** Coastal Defense + Riflemen + Frigate/Ironclad — *the
   defensive plateau, and the point where every nation's bnp curve flattens*
4. **T250–T400** Port Facility + Destroyer/Cruiser + Marines — an
   amphibious posture
5. **T400–T600** Battleship/AEGIS + SDI + Airport, or (Iceland, Gallia)
   more of step 4 forever

Only Carthago escapes step 3's plateau, and it escapes by conquering.
Anglia skips steps 3–4 into Supermarket/Freight/Airport and wonders.
The plateau is where the derby was lost. Team totals:

    T150   bnp  deep 704  stock 702   |  mfg  deep 323  stock 392
    T200   bnp  deep 703  stock 772   |  mfg  deep 330  stock 437
    T250   bnp  deep 677  stock 763   |  mfg  deep 390  stock 402

Deep is level with stock on bnp at T150 and 9% behind by T200, having
gained nothing in 50 turns. The seven deep nations' production choices
in T150–T249 are, in order: **Coastal Defense 259, Riflemen 244,
Diplomat 243, Frigate 217, Musketeers 215**, Barracks II 131, Ironclad
98, Colosseum 90. Six of the top eight are defensive or espionage; the
one economic entry is a happiness building.

---

## 5. What I would instrument next

1. **A "colony demand unmet" counter.** `deep-colony` reporting demand
   ~500 for 500 turns with zero settlers built is the single largest
   unforced error in this game and nothing alarms on it.
2. **Forfeited shields as a first-class per-nation metric**, not a
   per-event log line. Osmanli and Hellas each threw away ~58k shields —
   comparable to their entire late-game building stock — and no report
   surfaces it.
3. **Home-landmass coverage %** as a running gauge. Deep sits at 85% on
   uncontested islands from T60 to the end; stock reaches 92%. The gap is
   ~2 cities per nation, compounding for 540 turns.
4. **Government-change rate.** 48 changes / 24 anarchy turns for Osmanli
   is a bug-shaped number.
5. Fix the analysis scripts' geometry (§0) before any of the above.

## 6. What this does *not* say

- Nothing here explains *why* deep nations declared war on each other by
  T40 — that is the diplomacy half.
- `prodswitch` counts *choices*, not completions; a nation that thrashes
  inflates its counts. That is why §4.2 uses forfeited shields (a real
  resource) and §4.4 uses buildings actually held.
- 14 players, one map, one seed. Every number here is a hypothesis
  generator, not a result. Per CLAUDE.md rule 6, none of it is an
  acceptance criterion.

---

## Derby 2 (real Europe scenario, classic ruleset) — the barbarian storm, 2026-08-06

Live-monitor digest trail, deep vs stock cohort totals (7v7, endturn 600):

| Turn | deep cities | stock cities | deep mil | stock mil | deep gold | stock gold | notes |
|---|---|---|---|---|---|---|---|
| 60 | crossover | — | | | | | deep passes stock on cities |
| 110 | **207** (peak) | ~145 | ~320 | | | | deep +43% cities; score 1.07× at T138 |
| 130 | 192 | 144 | 263 | 229 | 1418 | 5759 | storm underway (l'Ollonais empire) |
| 140 | 175 | 144 | 235 | 204 | 350 | 5973 | Rus 52→44; Carthago Africa −5 |
| 150 | 153 | 145 | 221 | 174 | 558 | 5786 | |
| 160 | 133 | 150 | 195 | 178 | 575 | 5606 | Rus 35→23 |
| 170 | 124 | 155 | 179 | 184 | 411 | 5914 | mil lead lost |
| 180 | **100** | 152 | 153 | 189 | 331 | 5928 | Rus gone from top-3 (Hellas 14 is 2nd-best deep); Carthago 43 alone intact |

Deep lost **107 of 207 cities in 70 turns** — essentially all to barbarian
uprisings/pirates and the successor states they spawned, not to stock.
Stock's cohort sat stable (144→152) behind compact, developed, garrisoned
cores. Carthago (all-African, sea-buffered, contiguous) is the one deep
empire the storm couldn't touch; Rus (52 cities, sprawling east) was
annihilated; Gallia had already been bleeding since T110.

**Mechanism, and why this was predictable:** classic's uprising
probability scales with *own city count*. The diplomacy sweep had already
flagged this as an unpriced cost ("nobody prices 'my Nth city buys me
barbarian pressure'"). Derby 2 is the demonstration at scale: the wide
game generated the storm that consumed it. The hidden tax on expansion is
roughly proportional to expansion itself, and no want in either AI's
codebase sees it.

**Queue consequences (hypothesis-generators, per rule 6):**
1. Uprising-pressure pricing joins culture as a derby-mandated term —
   a per-city expected-loss debit on colony/expansion wants, scaled by
   ruleset uprising mechanics and own garrison state. Natural home:
   EXPANSION-doctrine epoch (#39) demand-side, or a small CONSEQ-shaped
   consumer if a readable signal exists.
2. Consolidation doctrine (garrison-before-grow) is now evidenced twice:
   Gallia's expand-but-can't-absorb collapse and the cohort-wide storm
   loss. G1's floor exists; magnitudes are untuned (the pile).
3. Contrast venue: same cohort that lost archipelago derby 1 at 0.89×
   while fighting itself led real Europe 1.07× at T138 with zero
   fratricide — the peace/love/war-target mechanisms work; the losses
   after T140 are storm, not statecraft.
- One seed, one map; the storm's severity has RNG in it. Derby 3
  (post-DARKFX binary) re-rolls the experiment.

---

## Derby 3 FINAL (T300, classic Europe, EVEN traits + trait-seeded stances, post-DARKFX binary)

**Stock 2008, Deep 1793 — 0.89×, the same ratio as derby 1 by the
opposite road.** Champion: **Def_Iberia 393** — the empire that was a
5-city rump at T96 and finished at 54 cities by grinding its own rebel
successor state back down. Podium: Iberia, Deep_Rus 343 (second despite
losing ~60 cities to the Polubotok uprising — banked tech and culture
don't burn), Deep_Iceland 326 (the hermit: 7 cities and ~50 buildings,
the best per-city score in three derbies).

Arc: deep won the opening (city crossover T32 vs derby-2's T60; peak
223 cities at T96) and lost the middlegame to the same storm as always
— but the NEW finding is timing asymmetry: **stock's storm landed early
(Iberia, T72) leaving 200 turns of recovery runway; deep's landed late
(Rus T104-168, Osmanli/Gallia after) leaving none.** Early disaster is
recoverable; late disaster is fatal. Reconquest was the single most
profitable activity on the map — Iberia won the derby on it — and deep
barely does it (its rallies at T200-224 were small and kept reversing).

Personality table (deep): the dove-builders finished 2nd/3rd/5th; the
two hawks (Gallia 196, Anglia 171) finished last among deeps — hawk
axes without tuned force-sizing bought wars and floors, not score. The
balance experiment (§ venues 6a) has its first data point.

Queue confirmations, nothing new needed: uprising-pressure pricing +
consolidation (queued), deterrence economy (queued), floor magnitudes
(pile), storm-150 venue (campaign). One NEW item → EXPANSION #39:
**reclamation doctrine** — pricing the retaking of one's own (and
anyone's) rebel-held cities as the discount real estate it demonstrably
is. One seed, one map; hypotheses, not results (rule 6).
