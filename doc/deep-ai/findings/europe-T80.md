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

## 2. The land is there

Measured as a BFS from every tile Carthage owns, expanding ONLY through
unowned land -- i.e. land it can walk a settler into without crossing
anybody's border:

    within  5 tiles     70 tiles   (~7 cities)
    within 10 tiles    131         (~13 cities)
    within 15 tiles    182         (~18 cities)
    within 50 tiles    468         (~46 cities)
    total reachable   6709

**Four wrong measurements preceded that one, and the pattern is worth
more than the number.** Each was internally correct and answered a
question nobody asked:

1. bounding box of its cities +/- 6 tiles -> **45 tiles**. Window too
   tight; the free land begins at distance 5-14.
2. continent flood fill -> **7844 tiles**. Counted land across other
   players' territory that Carthage cannot reach.
3. rendered map crop at `x40` on a **177**-wide map -> **~60 tiles**.
   Cut off the entire western region.
4. BFS from its own borders through no-man's-land -> **6709**. The
   question actually being asked.

**Lesson: when measuring "can it expand", the unit is land reachable
without crossing a border, not area, not continent, not neighbourhood.**

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

**Ablation to confirm:** all seven `horizon_*` weights to 0.0 vs
shipped placeholders, same cell, measuring Settlers share and final
city count. If Settlers recover, the credits are the cause and the fix
is arbitration -- an objective layer that prices a city site and a
building on one scale -- not a sixth credit.

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
