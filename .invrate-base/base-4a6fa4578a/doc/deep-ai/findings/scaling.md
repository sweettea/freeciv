# Part 17 — Performance at scale

**Revises PRD v2 §6.2/§6.3 and Part 14 §3.** Baseline: `freeciv/freeciv` @ `0896b34`.

Scenario: ~20 AI players, ~2000 cities total, multi-minute turns.

---

## 1. I was wrong about the ceiling

Part 14 §3.6 argued threading probably wasn't worth it, on the grounds that unit
movement is inherently serial and would cap the speedup near 1.6×.

That reasoning assumed a small game where per-unit work dominates. **At 100
cities per player the profile inverts** — per-city work dominates, and per-city
work is the *most* parallelizable thing in the AI. The conclusion doesn't
survive the scenario.

But threading at this scale would parallelize two costs that are quadratic in
game size, and one of them collapses algorithmically. Do that first, or you're
buying hardware to run a bad algorithm faster.

---

## 2. The two quadratic terms

### 2.1 `assess_danger` — O(own_cities × all_units), every turn

`dai_assess_danger_player()` (`daimilitary.c:649`) iterates the player's cities.
Inside, `assess_danger()` (`daimilitary.c:827`):

```c
players_iterate(aplayer) {
  ...
  unit_list_iterate(units, punit) {
    /* distance / move-rate evaluation against this city */
  } unit_list_iterate_end;
} players_iterate_end;
```

So per player: `own_cities × (players × their_units)`. With 100 own cities and
~5,000 units across 20 players, that's **500,000 unit-vs-city evaluations per
player, 10 million per turn** — and it runs every turn, on no cadence.

### 2.2 `base_want` — O(own_cities² × player-range improvements)

`base_want()` (`daicity.c:1556–1566`):

```c
city_add_improvement(pcity, pimprove);
city_range_iterate(pcity, pplayer->cities,
                   adv->impr_range[improvement_index(pimprove)], acity) {
  final_want += dai_city_want(pplayer, acity, adv, pimprove)
              - def_ai_city_data(acity, ait)->worth;
} city_range_iterate_end;
city_remove_improvement(pcity, pimprove);
```

`impr_range[]` is the widest requirement range for that improvement. For
anything with **player range** — great wonders especially — `city_range_iterate`
walks *every city the player owns*. And `base_want` is called **per improvement
per city**.

At 100 cities and ~25 player-range improvements:
`100 × 25 × 100 = 250,000` `dai_city_want()` calls per player per recalc, each
iterating that city's worked tiles and improvements. Call it **5 million tile
operations per player**, per building recalculation.

`building_wait` spreads the cost across turns but doesn't reduce it.

---

## 3. The 100× algorithmic fix

For a **player-range** improvement, the empire-wide effect is largely the same
regardless of *which* city builds it. A great wonder's player-range effect
applies to the empire either way; what differs is mostly local (shield surplus,
build time, defensibility).

So the loop structure is wrong. Instead of:

```
for each candidate city C:            /* 100 */
    for each city A in empire:        /* 100 */
        evaluate A with improvement in C
```

do:

```
evaluate the empire-wide delta ONCE   /* 100 city-wants */
for each candidate city C:            /* 100 */
    add C-local terms only            /* O(1) */
```

**That collapses C² to C — a 100× reduction on the dominant term at this
scale**, and it's a strictly better model too, because it separates "is this
wonder worth building" from "where should we build it," which are different
questions the current code conflates.

Same treatment for `assess_danger`: build a per-continent spatial index of
threatening units once per turn and query it per city, rather than scanning every
unit for every city. That turns `own_cities × all_units` into
`all_units + own_cities × nearby_units`.

**These two changes alone may be most of the multi-minute problem**, with no
threading at all.

---

## 4. The threading blocker is one idiom, repeated

I've now found the same pattern three times, and it's the reason `ai/tex` runs
one thread with a shadow world.

**`cityresult_fill()`** — `daisettler.c:292–301`:
```c
pplayer->government = adv->goal.govt.gov;              /* writes the PLAYER */
tile_set_owner(result->tile, pplayer, result->tile);   /* writes the MAP    */
```

**`base_want()`** — `daicity.c:1556–1573`:
```c
city_add_improvement(pcity, pimprove);                        /* writes the CITY   */
game.info.great_wonder_owners[improvement_index(pimprove)]    /* writes GLOBAL GAME STATE */
pplayer->wonders[improvement_index(pimprove)] = ...;          /* writes the PLAYER */
```

**`common/aicore/citymap.c:53`** — `static int *citymap`, one array shared by all
AI players.

The idiom is **"mutate shared state, evaluate, restore."** It's how this codebase
does counterfactual evaluation everywhere. `base_want` temporarily rewrites
`game.info.great_wonder_owners[]` — *global game state visible to every player* —
in order to price one building in one city.

**That's the whole blocker.** Not architecture, not algorithmic dependency — one
idiom, applied consistently, in maybe a dozen places.

### And fixing it pays three times

1. **Correctness** — no more torn reads if anything ever runs concurrently.
2. **Cacheability** — a pure `evaluate(city, improvement, context)` is
   memoizable. The current one can't be cached at all, because it has side
   effects. Given §2.2's repetition, memoization alone is a large win.
3. **Threadability** — pure evaluation parallelizes trivially.

The mechanism is the same one already used successfully elsewhere in the tree:
`adv_calc_extra()` (`infracache.c:197`) does `tile_virtual_new()` /
`tile_virtual_destroy()` rather than mutating the real tile. **The codebase
already knows the right pattern; it just isn't applied consistently.** Extend it
with a `city_virtual` evaluation context and pass counterfactual state as
parameters instead of writing it to globals.

---

## 5. The right parallel granularity is per-city, not per-player

At 2000 cities, "one thread per AI" is the wrong decomposition:

- **20 tasks vs 2000 tasks** — far finer grain, far better load balance
- A 100-city empire and a 3-city rump are wildly unequal as tasks
- Per-city evaluation is *naturally* independent once §4 is fixed; per-player
  planning still shares the map

So: a **thread pool over per-city evaluation jobs**, with the empire-level
aggregation serial afterward. Bounded threads, work-stealing, degrades gracefully
at any player or city count. `assess_danger` per city, `base_want` per city, CM
per city — all embarrassingly parallel once evaluation is pure.

Then the existing plan/apply split still applies: parallel read-only phase,
serial apply in fixed player order, so ordering of *writes* never varies and
determinism holds.

---

## 6. Revised recommendation

**W26 (planning purity) is promoted from optional-threading-prerequisite to a
P0 performance workstream.** It now has three independent justifications and it
gates both the caching win and the threading win.

Revised order for the scaling problem specifically:

1. **Profile.** Turn on `TIMING_LOG`, run the 20-player/2000-city case, get real
   `AIT_*` numbers. Everything above is a code-reading hypothesis; it needs
   confirming. My prediction: `AIT_DANGER` and `AIT_CITY_BUILDINGS` dominate,
   not `AIT_FSTK` as I guessed earlier.
2. **W26 — planning purity.** Virtual evaluation contexts, government and
   ownership as parameters, per-player `citymap`, per-player RNG.
3. **Algorithmic fixes (§3).** Hoist player-range improvement evaluation;
   spatially index threatening units. Probably the largest single win.
4. **Memoization.** Now possible because evaluation is pure. Cache
   `dai_city_want` per (city, turn) and invalidate on change.
5. **W13 campaigns.** Removes the per-unit pathfinder explosion.
6. **Then measure again**, and only then decide on threading — which by that
   point is a small change, because W26 did the hard part.

## 7. Commits to add

To **W26**, now P0:
```
advisors: add a virtual city evaluation context mirroring tile_virtual_new
ai/default: evaluate improvement want against a virtual city, not by mutation
ai/default: pass wonder ownership as context instead of writing game.info
ai/default: pass evaluated government as a parameter
ai/default: pass virtual tile ownership into waste calculation
common: make the citymap reservation array per-player
common: give each player an independent random number stream
test: assert AI evaluation performs no writes to shared game state
```

New **W27 — Scaling**:
```
ai/default: evaluate player-range improvement want once per empire, not per city
ai/default: separate empire-wide improvement value from city-local placement value
ai/default: build a per-turn spatial index of threatening units
ai/default: query the threat index per city instead of scanning all units
ai/default: memoize city want per city per turn
test: add a 20-player 2000-city scaling benchmark
test: assert per-turn AI time scales sub-quadratically in city count
```

That last test is the real deliverable. A scaling benchmark in CI is how you
stop this regressing the moment someone adds another per-city loop.
