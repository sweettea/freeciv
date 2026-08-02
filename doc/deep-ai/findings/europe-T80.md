# Europe T80-T87: deep does not expand (2026-08-01)

Live human game, 7 `ai/deep` vs 7 `ai/default` vs 1 human on the Europe
map, binary `d7846cccd0`. Companion to `europe-T76.md`.

The headline is one number: **Deep_Carthago holds 17 cities, has room
for ~18 more inside 15 tiles of its own borders on land nobody
contests, and has built zero settlers for eight consecutive turns.**

---

## 1. The trajectory

    turn   cities  settlers  workers  Musketeers  units
    T0080     17       0        11        35        68
    T0082     17       0        11        38        70
    T0084     18       0        11        39        71   <- pirate city TAKEN
    T0086     18       0        11        39        72
    T0087     18       0        11        40        75

Settlers pinned at zero throughout. Five Musketeers built. The one city
gained was captured, not founded.

## 2. The land is there -- but far less of it than first reported

**CORRECTED.** Every earlier figure in this section was wrong. The
Europe map sets `topology=""` and `wrap=""` -- it does NOT wrap in x --
and the analysis scripts computed neighbours as `(x + dx) % xsize`,
gluing the west edge to the east edge and merging Africa into Eurasia.

The real island, flood-filled without wrapping:

    Carthage's landmass          624 land tiles
      other players on it          0        <- Carthage is ALONE
      owned by Carthage          204  (33%)
      unowned                    420  (67%)
        workable (plains, hills) 180
        desert / mountain        240

So the expansion room is roughly **9 cities' worth of workable land on
a mostly-desert island**, not the "~18 cities within 15 tiles, 6709
tiles reachable" this file previously claimed.

**FIVE wrong measurements preceded the right one**, and the pattern is
worth more than the number:

1. bounding box of its cities +/- 6 tiles -> 45 tiles. Window too tight.
2. continent flood fill (wrapped) -> 7844. Counted land across other
   players' territory, and merged two continents.
3. rendered map crop at `x40` on a 177-wide map -> ~60. Cut off the west.
4. BFS through no-man's-land (wrapped) -> 6709. Right question, wrong
   topology.
5. Flood fill WITHOUT wrapping -> 624-tile island, Carthage alone on it.

**Lessons, in order of how much they cost:**

- **Read the map's topology before computing on it.** Four of the five
  errors survived because `%xsize` looks like defensive coding. It is
  an assumption, and this map does not hold it.
- The unit for "can it expand" is land reachable without crossing a
  border -- but only after the adjacency model is right.
- `ai/deep` itself was AUDITED for this and is clean: no hand-rolled
  coordinate arithmetic, every distance through `real_map_distance` /
  `sq_map_distance` / `map_distance`, continent membership from
  freeciv's own `assign_continent_numbers()`. **The defect was in the
  analysis, never in the AI.**

### 2a. What this does NOT explain

`site_rival_pressure` (added 2026-08-01) filters on
`tile_continent(acity) == here`. Carthage's continent holds no
non-allied city at all, so that term contributes exactly **0** and is
NOT the cause of anything observed here. An earlier draft blamed it on
the strength of the wrapped flood fill; that is retracted.

**`founder_want = 0` in all 18 Carthaginian cities at T122 remains
unexplained.** Not rival pressure, not a full island: 180 workable
unowned tiles, no competitor for them. That is the open question.

## 3. Aggregate: both AIs under-expand, but deep is worse

At T80, 75% of the map's land is unowned.

    ai        cities  cities/player  settlers
    deep          81       11.6          2
    classic      133       19.0         17

**All 17 classic settlers belong to Def_Norge.** The other six classic
players have zero, and Def_Italia sits on 465 free tiles with none. So
"classic expands and deep does not" is false: *one* classic player
expands. Excluding Norge, classic is 14.8 cities/player to deep's 11.6
-- a ~22% gap, not the 8x the raw settler counts suggest.

Both modules stop expanding while land is abundant. **This is a shared
defect; deep's version is worse but not different in kind.**

## 4. Where deep's production goes instead

    per 100 cities      deep   classic
    military             196       106
    Musketeers           112        22
    Explorer              31         2
    Workers               83        39
    Pikemen+Phalanx       31        71

Deep wins every quality comparison that was measured -- mean city size
4.89 vs 4.03, 25 obsolete defenders vs 89 (rotation upgrades work), 25
explorers vs 3 (W06 works), 25 Granaries + 11 Temples + 21 City Walls
against classic's ONE building in total (the horizon credits work).

**It builds a better empire and a third fewer of it.**

## 5. The likely mechanism, and how to test it

`deep_contemplate_new_city()` sets `founder_want` from the site
evaluator; `deepdefence.c:3065` then scales it by
`(1 + settler_rush_weight * map_crowding)` -- at most about 2x.

Meanwhile every recent epic has inflated what settlers compete against:

- **Defenders** gain `barbarian_fear` (+30 FLAT, every city,
  unconditionally), `density_prior` (up to +20), plus density fear,
  boat shadow, overseas danger and survival deficit -- all added to
  `city_data->danger` AFTER the `deep-def` log line.
- **Buildings** gain four horizon credits, now five.
- **Settlers** gain one multiplier capped near 2x, and nothing else.

So two of the three competitors have been systematically inflated and
the third has not. That is RC4 (incommensurable wants) being *widened*,
one credit at a time.

**The test, and it is cheap:** log `founder_want` beside the chosen
build in the `deep-def` tripwire.

- If `founder_want` is near zero -> the site evaluator is not finding
  the western land, and it is a SCORING bug.
- If it is healthy but loses -> it is an ARBITRATION bug, and
  arbitration is the objective layer, not another credit.

### 5a. THE TEST WAS RUN. It is arbitration, and BUILDINGS win.

`88dfa15597` added `founder=` / `worker=` to the tripwire. 8536
`deep-def` rows over 3 `primary-120` games (120 turns, 6 players,
organic expansion from scratch):

    turns    rows  founder_want  raw danger  %Settlers  %military  %other
     0-19     184       429           0          99%        1%        0%
    40-59     881       336           0          57%       27%       16%
    80-99    2338       191           4          18%       28%       53%
   100-119   3030       163           6          13%       25%       62%

    founder_want <= 0 : 1591/8536 (19%)
    founder_want > 100: 6257/8536 (73%) -- of those only 39% build Settlers

**`founder_want` is healthy.** Above 100 in 73% of city-turns, still
averaging 163 at T100-119. The site evaluator finds land and prices it.
Not a scoring bug.

**And the winner is NOT the military.** Military production holds a
flat 25-31% for the whole game. What climbs is *other* -- buildings --
0% -> 62%, tracking the Settlers collapse 99% -> 13% almost exactly.

**So the hypothesis in §5 above was half wrong and is corrected here.**
The injected defender fears are real and they are documented, but they
are not what is crowding out expansion in this measurement. **Deep's
own horizon building credits are.** Five credits now add want to
buildings; `founder_want` receives one <=2x multiplier and nothing
else; they share one scale.

That is self-inflicted, and it implicates every credit in the family
including `c6d39b6081`, committed the same night.

### 5b. THE ABLATION RAN. Mechanism yes, SCORE NO.

All seven `horizon_*` weights at 0.0 vs the shipped placeholders,
primary-120, **10 paired seeds, both mirrored arms** (40 games),
51,379 tripwire rows:

    arm          %Settlers  %military  %other  cities/plr  score/plr
    credits ON        29%       30%      40%      16.3        136
    credits OFF       42%       33%      26%      18.8        135

    Settlers by band  0-19  40-59  60-79  80-99  100-119
    ON                 93%    60%    36%    19%     13%
    OFF                93%    63%    50%    41%     26%

    ON  pooled ratio 1.1383   deep 8141  base 7152  cities  979
    OFF pooled ratio 1.1186   deep 8121  base 7260  cities 1128
    mean delta -0.0252 (sd 0.1495, SE 0.0473)   4/10 seeds

**The mechanism is confirmed and the score is not.** Turning the credits
off moves the build queue hard -- Settlers 29% -> 42%, buildings 40% ->
26%, city count +15% -- and moves the outcome by nothing: deep 8141 vs
8121, t ~ -0.5.

**A 4-seed version of this same ablation read +0.13 ratio and +9% deep
absolute score, and it did not replicate.** That number was reported
before the 10-seed run existed and is retracted here. The lesson is the
one already in `multiplier-column.md` and the CM-war-gate note: on this
project, four or five seeds can manufacture an effect of exactly the
size we are hunting.

**So the credits are not "misbalanced early game" either.** They are
SCORE-INERT while visibly reshaping what gets built. Five commits'
worth of credit machinery reliably moves the mechanism and never moves
the outcome -- the same pattern as content, granary and multiplier
individually, now shown for the family as a whole.

That reframes the open question. It is not "what weight should these
have"; it is **why a 15% swing in city count does not show up in
score at 120 turns**, which is either a scoring-horizon artifact of the
cell or evidence that city count is not what is limiting deep. Neither
is answered here, and a sixth credit answers neither.

## 6. Two tripwire defects found while reading this game

- **`deep-def` logs PRE-INJECTION danger.** `deep_log_defence()` runs
  "before any deep adjustment so before and after arms measure need on
  the same scale", which is right for A/B comparability and actively
  misleading for diagnosis: cities logged `danger=0 urgency=0` while
  building Musketeers are not cities deep thought were safe. Every
  injected fear lands after this line. Worth logging both.
- **`rotation_power_ratio < 1.0` degenerates.** The gate at
  `deepdefence.c:1887` is
  `best_buildable >= best_present * rotation_power_ratio`. A city can
  always build the unit type it already fields, so for any ratio below
  1.0 the condition is trivially true and "replace obsolete defenders"
  becomes "always want another defender", in every zero-danger city,
  forever. The compiled default is 2.0 and safe; the registry range is
  **0.0-6.0**, and campaign-a4 has already sampled 0.542, 0.566, 0.615,
  0.689 and 0.962. **The lower bound should not be below 1.0.**

## 7. Corrections recorded on purpose

Three claims made from this save that were wrong, kept so the failure
modes are not repeated:

- *"Carthage is boxed in, so war is the correct read"* -- from
  measurement 1 above. False; it has room for ~18 cities.
- *"There is an enemy enclave inside Carthage driving the Italia war"*
  -- the tiles at (54-56, 90-92) are **Henry Morgan the pirate**, city
  `Cartenna`, size 2. Not a player war.
- *"It has not bothered to take the pirate city"* -- read from the T80
  snapshot alone. It took it at T84. **Read the trajectory, not the
  frame.**

The same failure appeared elsewhere in the session: `ls ~/campaign-*.log
| head -1` sorts alphabetically and silently returns the campaign that
was killed that morning. A stale read looks exactly like a live one.
