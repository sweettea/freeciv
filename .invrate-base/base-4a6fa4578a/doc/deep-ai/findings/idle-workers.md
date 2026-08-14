# Workers do nothing when nothing is demanded (2026-08-02)

Measured on `lategame-80`, 10 train seeds, deep arm, final-save census.
Prompted by a human question while looking at repurposed settlers: *do
workers do anything if it isn't demanded and they'd otherwise be idle?*

**No. They stand still, permanently, and for an AI player there is no
fallback behaviour at all.** And it is not a settler problem — it is
most of the terrain economy.

---

## 1. The census

Every worker-capable unit alive at game end, by activity:

    worker   IDLE           341   34.1/game   71% of its kind
    worker   FORTIFYING      45    4.5/game    9%
    worker   IRRIGATE        42    4.2/game    9%
    worker   MINE            30    3.0/game    6%
    worker   BASE            21    2.1/game    4%
    worker   TRANSFORM        2    0.2/game    0%

    founder  IDLE           134   13.4/game   82% of its kind
    founder  FORTIFYING      10    1.0/game    6%
    founder  IRRIGATE         9    0.9/game    5%
    founder  BASE             6    0.6/game    4%
    founder  MINE             5    0.5/game    3%

    founders 164 (16.4/game), dedicated workers 481 (48.1/game)

**475 of 645 worker-capable units — 74% — are idle.**

A save is written at turn end, so "IDLE" could in principle mean "in
transit to work, goto already converted to IDLE". It does not:

    worker   no goto destination at all      341  34.1/game  100%
    founder  no goto destination at all      107  10.7/game   80%
    founder  goto set, still travelling       26   2.6/game   19%

**Every single idle dedicated worker has no destination.** They are
parked, not walking. Only 19% of idle founders are in transit.

So ~45 units per game — roughly 34 workers and 11 founders — are
standing still at the final whistle with nothing assigned.

## 2. Why: demand is only ever measured inside city work radii

`worker_evaluate_improvements()` (`server/advisors/autoworkers.c:542`):

    city_list_iterate(pplayer->cities, pcity) {
      city_tile_iterate_index(nmap, city_map_radius_sq_get(pcity),
                              pcenter, ptile, cindex) {
        if (!in_use && !city_can_work_tile(pcity, ptile)) continue;

The unit's whole world is the union of its cities' work radii, and
within that only tiles a city *could* work. That is **RC3** verbatim.
Invisible by construction:

- roads *between* cities (only `adv_workers_road_bonus` inside a radius)
- any terrain outside every radius, including future city sites
- tiles beyond the current radius that a larger city would reach

## 3. Why: the want is a delta, floored at zero, with no minimum

    if (!improve_worked) {
      best_newv = amortize((best_newv - best_oldv + best_extra)
                           * WORKER_FACTOR, best_delay);
    }
    best_newv /= WORKER_FACTOR;
    best_newv = MAX(best_newv, 0);

The value is the *improvement* in that tile's worth to that city,
discounted by travel plus work turns. The threshold is strictly `> 0`
and there is no floor above it, so a worker will take arbitrarily
marginal work if any exists. **Idle therefore means the delta really is
zero everywhere it can see.**

On `lategame-80` that happens early: `techlevel 50` hands out every
improvement from turn one, the empires are small and crowded, the radii
get improved out, and after that every enumerated tile scores zero.

This is also **RC1's corollary**: anything needing a *sequence* —
terraform then irrigate, road a corridor, prepare a site for a city
that does not exist yet — scores zero at every single step, so it never
starts.

## 4. Why they then do nothing: the AI has no fallback

When nothing scores, `worker_evaluate_improvements()` returns 0 with
`*best_tile = NULL`. What happens next depends on who owns the unit:

- **Human auto-worker**: `auto_worker_findwork()` sees the NULL and
  calls `adv_seek_shelter()` — walk to a friendly city.
- **AI player**: that line is never reached.
  `auto_workers_player()` (`autoworkers.c:1327`) branches on
  `is_ai(pplayer)` and calls the `settler_run` **hook** instead of
  `auto_worker_findwork()`.

deep's hook ends:

    } else {
      UNIT_LOG(LOG_DEBUG, punit, "cannot find work");
      dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);
      goto CLEANUP;
    }

No shelter, no sentry, no fortify, no move. The unit stays exactly
where it is and is re-evaluated **from scratch next turn** — a
pathfinder map over the map, plus every tile of every city radius, plus
(for a founder) a full site search — to reach the same conclusion. ~45
units per game paying that every turn is a CPU cost as well as a
strategic one.

`ai/default` has the same shape; this is not a deep regression.

## 5. Size, and what it is not

~45 parked units per game against ~8.7 cities per player. The waste is
two-sided: the units themselves (shields, and `pop_cost 2` for every
founder), and every improvement that is invisible rather than absent —
the corridors, the terraforming, the sites prepared before the settler
arrives.

Bounding it honestly: this is **not** evidence that 45 units of work
exist and is being missed. It is evidence that the AI cannot *see* work
outside a city radius, and that when it sees none it does nothing at
all. How much real value is out there is the thing to measure next, and
the cheapest probe is to widen the enumeration by one ring and count
how many tiles acquire a positive delta.

## 6. Where this already sits in the plan

- **W28** worker stacking and **W29** transform chains — the sequence
  problem in §3.
- **W12** empire graph — the corridor problem in §2.
- **RC3** generally: the worker world model is the union of city radii.
- `upstream-bugs.md` #31 (Join City) recovers the founder half only:
  ~11 parked founders per game are 22 citizens that could be handed
  back, but it does nothing for the 34 parked workers.

## 7. Both remedies built and measured (2026-08-03)

Human calls: *worker-workers should always do some work rather than
stand around*, and *settlers doing worker duty should join cities*.
Both landed; the numbers are mixed and are recorded as they came out.

### The worker fallback is inert

`deep_idle_fallback_work()` (`df9309d5ad`) takes the soonest-finished
legal road, irrigation or mine inside our own radii, but only where the
unit was about to stand still. Ranked by travel plus work turns, so no
constant.

    lategame-80   2267 assignments   bnp -0.02   mfg +0.00
                  score -2.20 (t=-1.04)   idle unit-turns 12273 -> 9810
    primary-120     20 assignments   score +0.80 (t=0.99), all noise

**The improvements really are worth nothing where they were declined.**
`lategame-80` cities average size 5.8 and never grow to work the outer
tiles, so `city_tile_value()` at today's size was right and §3's charge
of RC1-style myopia does not hold in this regime. Landed as doctrine,
recorded as neutral.

### The idle pool is largely one cell's artefact

    cell           idle unit-turns/10g   cities/plr   idle per city-turn
    lategame-80              12273           8.68          0.2946
    primary-120                429          22.65          0.0026

**113x.** `lategame-80` gives 8.7 cities `techlevel 50`, so every
improvement is available at turn one and the radii are improved out
almost immediately; `primary-120` has 22.65 cities and a normal tech
curve, and its workers are never short of jobs. §1's "74% of
worker-capable units are idle" is true and is mostly a statement about
that cell -- more evidence for the standing question of whether
`lategame-80` is a good tuning cell.

### Join City works and costs land

`deep_settler_join_city()` (`5933c431d0`), fired when the site search
returns nothing at all:

    settlers alive at end   2.73 ->  0.32   -2.42   t=-8.87
    joins completed                   397   (~40 per game)
    cities                  8.68 ->  8.47   -0.22   t=-1.57
    landarea               99917 -> 97117   -2800   t=-2.35
    score                 189.22 -> 187.77   -1.45   t=-0.60

The objective is met decisively; `landarea` is the only significant
term in the table and it is negative. The mechanism is honest rather
than a bug: joining is irreversible, and a settler with no site today
sometimes has one in five turns once borders move.

Tightening the trigger from "not founding this turn" to "no site at
all" was built and measured on a separate binary and came out
**byte-identical over the same ten seeds** -- the looser case never
occurs -- so the land cost is intrinsic to joining, not to an eager
trigger.

### The remedy was a pool sized by the land still unclaimed

Human calls: keep a founder pool rather than dissolving every settler,
and **scale it by how much free space the world still has**. Both were
built and measured; the second is what mattered.

`lategame-80`, 10 paired seeds, every variant against the same base:

    metric        base |  join-all    t | pool-flat    t | pool-scaled   t
    score       189.22 |     -1.45 -0.60|    -0.65 -0.22 |     -0.28 -0.10
    cities        8.68 |     -0.22 -1.57|    -0.42 -2.05 |     -0.08 -0.41
    landarea     99917 |     -2800 -2.35|    -3750 -2.47 |     -2167 -1.41
    settlers      2.73 |     -2.42 -8.87|    -1.68 -5.68 |     -2.42 -9.10
    unitslost    15.58 |     -1.45 -1.72|    +1.50 +2.30 |     -1.13 -1.86
    joins            0 |       397      |      213       |       384

Read the columns in order, because each one corrects the last:

- **join-all** meets the objective and costs land: `landarea` -2800 at
  t=-2.35, the only significant term.
- **pool-flat** — hold `founder_pool_per_city * cities` — is *worse on
  every count*. The founders it keeps are exactly the ones the search
  found no site for, so retaining them buys wandering units that die:
  `unitslost` flips to **+1.50 at t=+2.30**. Holding inventory, not an
  option.
- **pool-scaled** — multiply that target by `deep_free_land_share()` —
  meets the objective in full (`settlers` -2.42, t=-9.10, the same as
  joining everything) and **has no significant negative term left**.
  `landarea` t=-2.35 → -1.41, `cities` t=-1.57 → -0.41, `unitslost`
  back below zero. Score dead flat at -0.28 (t=-0.10).

The mechanism is the whole point: free land *shrinks over a game*.
Early, the pool is held and those settlers found the cities that
`join-all` was throwing away; late, the target falls toward zero and
the settlers that would have wandered and died are banked as citizens
instead. A pool sized by a **static** prior cannot do that, which is
why `pool-flat` failed — and why `deep_map_crowding()` and
`deep_land_pressure()`, both computed once from lobby settings, were
the wrong instruments.

Landed at `5d422d0b44`. It is **not a score improvement** — it is the
objective met at no measurable cost, which is what "free" looks like.
`founder_pool_per_city` stays a PLACEHOLDER at 0.31 pending a tuning
pass.

### Ruleset sensitivity, recorded because the right answer moves

- The homeless bias switches **off** when `game.server.killunhomed > 0`:
  there a homeless unit loses hitpoints every turn until it dies
  (`unittools.c:501`), so "free to keep" becomes "dying slowly" and
  preferring to keep them would be backwards. Inverting is probably
  right, but it is unmeasured, so it switches off rather than guessing.
- Join City is queried through `action_prob_vs_city()`, so a ruleset
  with no enabler simply never joins. civ2civ3 (Settlers, Migrants,
  Fanatics) and classic (Settlers) both carry `AddToCity`.
- `upstream-bugs.md` #33 is civ2civ3-only: it is the sole shipped
  ruleset with a ferry carrying the `Workers` flag, so the `-classic`
  cells cannot reproduce it.

## 8. Open

1. Measure whether work exists outside the radii before building
   anything to find it. One extra ring of enumeration, counted, not
   acted on.
2. A do-nothing fallback is a bug independent of that: even
   `adv_seek_shelter()`, which humans already get, is better than
   standing in the open. Cheap, and it needs no new constant.
3. The re-evaluation cost is real: full search per idle unit per turn
   to conclude nothing. Worth a cheap negative cache if 2 does not
   already fix it.

### Sized by sites remaining, not by cities owned (2026-08-03, later)

`pool-scaled` above still keyed off city count, and on the roomy cell
that inverts: 23 cities drove the target to ~3.6 while the land could
still hold ~175 more, so the gate never held and joining dissolved
settlers that were expanding rather than idling. Replaced by
`deep_remaining_sites()` -- unclaimed land divided by our OWN land per
city, so it is self-calibrating on whatever spacing the ruleset,
terrain and objective actually produce.

The scale the old formula was hiding:

    sites remaining     lategame-80        primary-120
      median                   13.2              174.7
      range             10.5 - 30.2       131.8 - 266.5

`primary-120`, 10 paired seeds, against no-join:

    metric        no-join | join-all    t | pool-cities  t | pool-sites   t
    score          159.13 |    +0.30 0.11 |     +0.15 0.05 |    +0.98  0.57
    cities          23.00 |    -0.50-0.83 |     -0.60-0.76 |    -0.05 -0.47
    landarea       312500 |    -4483-0.62 |     -8083-0.84 |     +533  0.26
    pop             76.63 |    +0.35 0.20 |     -0.07-0.03 |    +1.98  4.14
    mfg             73.77 |    -2.05-1.18 |     -2.02-0.81 |    +3.05  2.11
    settlers        10.42 |    -2.32-3.80 |     -2.32-3.60 |    -2.45 -4.27
    joins               0 |      164      |       164      |      125

A mildly negative change becomes a mildly positive one, with **pop
+1.98 (t=4.14) and mfg +3.05 (t=2.11) significant**, land area back
above zero, and the objective still met.

`lategame-80` moves the other way, all inside noise:

    metric           base | pool-cities  t | pool-sites   t
    score          189.22 |     -0.28-0.10 |    -3.30 -1.27
    pop             48.87 |     +0.30 0.35 |    -1.38 -1.12
    mfg             56.45 |     +2.03 0.85 |    -1.18 -0.38
    settlers         2.73 |     -2.42-9.10 |    -2.30 -8.22
    joins               0 |       384      |      314

**This is the 3.5x spread in the placeholder's own comment, showing up
in behaviour.** 0.06 was calibrated on `primary-120` because that cell's
pool is the one demonstrably doing work; `lategame-80` calibrates to
0.207 on its own numbers, and it is the cell that got slightly worse.
Nothing there is significant, and the formula is the part that is now
right -- the constant is what a per-cell tuning pass is for. Do not
average the spread away.

Landed with `founder_pool_per_site` (renamed from `_per_city`, which
would now be lying about what it measures).

### City density did not move, and was never going to

Checked because it is the obvious way a settler-supply change could do
damage invisibly. Nearest own-city distance, self-founded cities only,
final saves:

    run                        n    median   mean    p90   at floor
    lategame-80 base         444      3.16   5.37  11.18       43%
    lategame-80 pool-site    421      3.16   5.02  11.18       46%
    primary-120 no-join     1284      3.00   3.92   5.39       53%
    primary-120 pool-site   1270      3.00   3.88   5.39       54%

    land per city   lategame-80  11507 -> 11500     (base -> pool-site)
                    primary-120  13587 -> 13640

Median and p90 identical to two decimals on both cells; land per city
moves ~1%. **W02b holds**: the median founded city sits on
`citymindist` and about half of them are jammed against it, so spacing
is set by the ruleset and nothing in the settler-supply work could move
it. Recorded as a negative control rather than a result.

Two asides worth keeping. `primary-120` is DENSER than `lategame-80`
(median 3.00 vs 3.16, p90 5.4 vs 11.2) despite four times the land per
player -- the island cell's long tail is water forcing cities apart,
not choice, which is more evidence the two cells are different regimes
rather than one regime at two scales. And mean city size is flat
(5.81 -> 5.77, 3.44 -> 3.54), so the joined citizens do not show up as
bigger cities: ~2.4 joins per player over a whole game is ~5 population
against ~50, spread over 8-23 cities. Join City was never going to show
in city size; it shows in upkeep not paid and units not lost.

---

## 9. Workers cannot cross water at all (2026-08-03)

Human observation: *AI cities on a different continent never get
workers.* Confirmed, and it is structural rather than a valuation
failure.

`worker_evaluate_improvements()` builds its map with
`pft_fill_unit_parameter()` -- a plain land parameter. An overseas
city IS enumerated (it is in `pplayer->cities`), and then every tile in
its radius is silently discarded at
`if (pf_map_position(pfm, ptile, &pos))` because a land unit cannot
path there.

And **there is no ferry leg anywhere in the worker path.** Every
`aiferry_*` call in `deepsettler.c` sits on the BUILD_CITY mission
branch -- `deep_settler_gothere()` -> `aiferry_gobyboat()`. The
FIND_WORK branch ends at `auto_worker_setup_work()`, which walks a land
path and nothing else. Settlers can board boats; workers never can.

A colony founded overseas is therefore improved only by whatever
founders happen to land there and get repurposed, and by nothing else
for the rest of the game.

    lategame-80 final saves, distance from each city to the nearest
    worker-capable unit of its owner:

      cities 522   median 2.2   p75 5.0   p90 8.5   max 24.4
      cities with NO worker-capable unit within 8 tiles: 65 (12%)

The mainland is well served and a minority of cities are simply
abandoned.

**This closes a loop with section 8.** Overseas expansion measured as
barely worth having -- ~11 cities a game, and joining settlers cost
land. Part of the reason is that overseas cities never receive terrain
work, so they stay low-value. The colonisation pipeline and the worker
pipeline are failing each other, and fixing either alone understates
both.

### The fix, specified

The machinery exists: `pft_fill_amphibious_parameter()` and
`pft_amphibious` already carry settlers via
`aiferry_goto_amphibious()`. The worker side needs the same two
things:

1. **See it.** When the land search finds nothing, re-run the
   enumeration with an amphibious parameter so tiles on other landmasses
   become reachable-with-a-boat rather than invisible.
2. **Reach it.** Route the assignment through `aiferry_gobyboat()`
   instead of a bare walk.

Plus `worker_cross_continent_discount` (new tunable, placeholder + a
tuning pass) applied to the cross-water candidates, so this stays
backup behaviour and workers do not queue for boats ahead of settlers.

It is also the change that would finally make
`deep_idle_fallback_work()` non-inert: "nothing to do here" and "a
whole unimproved colony over there" is exactly the case it currently
cannot see.

## 10. The settler/boat tuning surface, audited (2026-08-03)

Human request: check we have all the tuning params that make sense
around boats and settlers.

**Not one settler or boat parameter has ever been tuned.** All fifteen
are placeholders or inherited stock values:

    settler_fearfulness_mult  2.0   PLACEHOLDER   overseas_fear       0.5  PLACEHOLDER
    settler_risk_tolerance    1.0   stock bar     overseas_decay      0.2  PLACEHOLDER
    founder_pool_per_site     0.06  PLACEHOLDER   boat_shadow_fear     25  PLACEHOLDER
    site_stream_weight        1.0   PLACEHOLDER   colony_lift_weight  0.5  PLACEHOLDER
    site_travel_discount      0.9   human-set     settler_rush_weight 1.0  PLACEHOLDER
    site_travel_crowding      0.0   OFF           site_settler_cost_weight  stock
    site_boat_cost_weight     1/3   stock

Five decisions had no lever at all. Two are now fixed (`190d47afc9`):

- **`site_enough_result`** replaces stock's compile-time
  `RESULT_IS_ENOUGH = 250`. Above it the on-continent search returns
  immediately and phase 2 never runs, so one inherited constant decides
  whether colonisation is EVALUATED at all. PLACEHOLDER 250.0 = stock.
- **`settler_guard_tolerance`**. The same trip was judged by a
  parameterised danger bar for movement and a bare `adv_danger_at()`
  for protection. Both now use `deep_settler_overnight_veto()` at their
  own tolerance, so "ask for an escort before you would refuse to go"
  is expressible. PLACEHOLDER 1.0 = stock bar = previous behaviour.

Three remain open, with human design calls attached:

- **The ferry cap `passengers >= boats`** (`deepsettler.c`). Human call:
  **keep 1:1**. A settler occupies a boat for a round trip, so one
  waiting settler per boat is honest, and a capacity multiplier would
  queue eight settlers behind one Transport and recreate the
  dock-parking bug the guard was added for. The exception is a
  **settlement party** -- several settlers on ONE boat making ONE trip
  to a low-risk destination -- which is "the cap counts trips, not
  settlers", and needs the lift-as-an-operation work to exist first.
- **The idle fallback has no gate** and cannot see other continents:
  section 9.
- **Join City ranks targets by arrival time only.** Human call: it
  should also weigh **granary presence** (a stocked granary carries the
  food forward instead of evaporating) and **the citizens' value to
  that city** against **travel time and route risk**. One weight, not
  four, if value is discounted by `amortize(value, turns)` the way the
  site search already does, with route risk from the shared
  `deep_settler_overnight_veto()`.

## 11. Sizing section 9: what an idle worker cannot see (2026-08-03)

The idle census now splits by continent (`aa42dc099c`), so the metric
line says whether a worker reporting "nothing to do" had nothing to do
or merely nothing it could walk to. `lategame-80`, 10 train seeds, deep
arm, 6890 idle worker-turns:

    mean per idle unit-turn      own continent   across water
      cities                             5.55           2.36
      workable tiles                    50.64          40.71
      OPEN work (road/irrig/mine)       17.55          12.69

    idle turns with no local open work at all   2397   34.8%
    ... and open work across the water          1043   15.1%
    ... and a boat available for it             1002   14.5%
    idle turns with any boat at all             3851   55.9%

**42% of the open terrain work an idle worker's empire is holding is on
a landmass it cannot reach**, and in one idle turn in seven the unit had
literally nothing to do at home, real work waiting overseas, and a boat
free to carry it.

`xopen` is a lower bound: the split is by continent number, so
same-continent work blocked by terrain still counts as reachable.

That is the size of the section 9 defect, and it also bounds the repair:
the fallback can only act on the 15%, not on the whole 42%, because the
rest of the time the worker has local work it would do first.
