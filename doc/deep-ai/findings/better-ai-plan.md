# A Better Freeciv AI — source analysis and implementation plan

Based on `github.com/freeciv/freeciv` @ `0896b34` (master, Apr 2026). All line
numbers refer to that tree.

---

## 0. The one architectural decision that matters

**Do not fork `ai/default`. Register a new AI module.**

Freeciv has a proper AI vtable: `struct ai_type::funcs`, populated in
`ai/classic/classicai.c:618–698`. `ai/tex` (`ai/tex/texai.c:670`) is a working
example of a *second* module that reuses `ai/default`'s guts and overrides only
what it wants. That's your template.

Wire-up is three lines in `meson.build:285–303` plus a `--aimodules=` option;
each module is ~700 lines of thin forwarding shims. Call yours `ai/deep` or
whatever. Then `create Alexander` / `set aitype deep`.

The vtable hooks you'll actually need:

| Hook | Gives you |
|---|---|
| `settler_run` / `settler_cont` / `settler_reset` | **Total** control of settlers *and* workers (see §3 — `auto_worker_findwork` calls `CALL_PLR_AI_FUNC(settler_run,...)` at `autoworkers.c:1261`) |
| `build_adv_init` / `build_adv_adjust_want` / `choose_building` | City production choice |
| `phase_begin` / `first_activities` / `last_activities` | Per-turn empire-level planning — this is where your campaign planner lives |
| `unit_turn_end`, `unit_task` | Unit-level behaviour |
| `city_alloc` / `city_free` / `player_alloc` | Your own per-city / per-player state blobs |

Everything you keep unchanged is a one-line forward to `dai_*`. You can migrate
subsystem by subsystem and A/B test against `classic` in the same game, which is
exactly what `H_EXPERIMENTAL` was built for.

**Caveat worth knowing up front:** `server/advisors/` (autoworkers, infracache,
advbuilding) is shared with *human* auto-workers. You cannot change
`adv_workers_road_bonus()` in place without changing what human players' auto-workers
do. You must either override at `settler_run` (clean) or add new functions
alongside (also fine). Don't edit the shared ones.

---

## 1. Why it builds out instead of up

This is the single highest-leverage bug, and it's about 40 lines of code.

### 1a. A city site is scored on **two tiles**

`cityresult_fill()` — `ai/default/daisettler.c:275–460`. For each tile in the
candidate radius it computes

```c
ptdc->sum = ptdc->food  * adv->food_priority
          + ptdc->trade * adv->science_priority
          + ptdc->shield* adv->shield_priority;
ptdc->sum *= PERFECTION / 2;          /* PERFECTION == 3, line 104 */
if (ptdc->food >= 2) ptdc->sum *= 2;  /* line ~343 */
```

Then it keeps exactly **two** tiles at full weight — the city centre and the
single best other tile — and dumps everything else into `result->remaining`
divided by `GROWTH_POTENTIAL_DEEMPHASIS`, which is **8** (line 109).

```c
result->total = result->city_center.tdc->sum + result->best_other.tdc->sum;
...
cr->total += cr->remaining;           /* daisettler.c:772 */
```

So a 20-tile catchment is valued as roughly **4.25 tile-equivalents**. The code
even confesses at line 407:

> `/* Baseline is best extra tile only. This is why making new cities is so darn good. */`

**Consequences, all of which match what you're seeing:**

- Two whales, a gold hill and a wine special four tiles out are worth 12.5% each.
  A site with one grassland-shield and one grassland-resource beats it.
- The `food >= 2` doubling makes it a grassland-seeking missile. Specials that
  are shield or trade heavy (gold, gems, wine, coal, iron) get systematically
  under-weighted relative to plain wet flatland.
- Long-run potential (river trade after Bridge Building, mines after Railroad,
  irrigation chains) is invisible — everything is scored at *current* tech and
  *current* terrain.

### 1b. Improvements are scored with a **zero time horizon**

`base_want()` — `ai/default/daicity.c:1530`. The method is honest and clever:
add the improvement, re-measure, subtract, remove it.

```c
city_add_improvement(pcity, pimprove);
city_range_iterate(pcity, pplayer->cities, adv->impr_range[...], acity) {
  final_want += dai_city_want(pplayer, acity, adv, pimprove)
              - def_ai_city_data(acity, ait)->worth;
} city_range_iterate_end;
city_remove_improvement(pcity, pimprove);
```

But `dai_city_want()` (`daicity.c:1452`) sums output over **currently worked
tiles at current city size, this turn**. So:

- **Aqueduct**: measured delta ≈ 0. Its entire value is "size 8 → size 16
  *later*". Invisible.
- **Granary, Harbour, Sewer, Supermarket**: same. The README's own TODO list
  says *"Fix the AI valuation of supermarket. (It currently never builds it)."*
- **Marketplace/Library/Bank**: these *do* score, because they're multipliers on
  present output. Which is why the AI builds those and little else.

Then it gets hit twice more:
- multiplied by the BUILDER trait: `cur->want *= (0.5 + trait/TRAIT_DEFAULT/2)` — `daidomestic.c:620`
- capped at `DAI_WANT_DOMESTIC_MAX` = 750 — `daitools.h:33`
- recomputed only every Nth turn (`city_data->building_turn` / `building_wait`)

Meanwhile `founder_want` gets *multiplied* by the EXPANSIONIST trait
(`daidomestic.c:558`) and by `pplayer->ai_common.expand`. The scales are rigged
toward settlers on both sides.

### 1c. The fix

**Replace the site evaluator with a full-catchment, horizon-aware model.**

1. Score **all** workable tiles, not two. Sort tile `sum` values descending, and
   weight tile *i* by the probability the city ever works it:

   ```c
   /* target_size ~ what this site can sustain given food, govt, aqueduct tech */
   w[i] = (i < target_size) ? 1.0 : 0.15;
   ```

   Estimate `target_size` from cumulative food surplus rather than a constant.
   This alone flips "one great tile" sites into "consistently good catchment"
   sites, and makes specials count properly wherever they sit.

2. Add an explicit **specials/resource term**. Iterate the catchment and count
   `tile_resource(ptile) != NULL`, weighted by what the resource yields *under
   the government you're heading toward* (`adv->goal.govt.gov` is already loaded
   at `daisettler.c:292` — reuse it). Reward clusters: three specials in one
   radius is worth more than three specials spread across three cities, because
   one set of buildings serves all three.

3. Add a **potential** term: what the catchment is worth after irrigation/mine/
   road on every improvable tile, discounted by build time. `adv_calc_extra()`
   in `server/advisors/infracache.c:188` already computes post-extra tile value
   — call it speculatively at settle time.

4. Raise `GROWTH_POTENTIAL_DEEMPHASIS` toward 2–3, or delete it in favour of the
   `w[i]` scheme above.

5. **Give improvements a horizon.** Wrap `base_want` in a projection: instead of
   measuring the delta at today's size, measure it at
   `min(current_size + growth_over_N_turns, size_cap_with_this_building)`, then
   `amortize()` it — `amortize()` already exists and is documented in
   `doc/README.AI`. An aqueduct in a size-7 city with +4 food should show a want
   proportional to the ~8 extra worked tiles it unlocks over the next 30 turns.

6. **Add a diminishing-returns term on city count.** Compute a per-empire
   marginal settler value that falls as `n_cities` rises relative to available
   good land, and as corruption/waste at the frontier rises. `city_waste()` is
   already being called at `daisettler.c:426`; make it steer the *count*
   decision, not just the site score.

A useful invariant to test against: **the AI should stop founding cities when
the marginal city's discounted output is below the marginal improvement's in
its best existing city.** Right now those two numbers aren't even on the same
scale.

---

## 2. Roads and railroads

### What's actually there

Two separate, both-inadequate mechanisms.

**Search scope** — `server/advisors/autoworkers.c:471–801`:

```c
city_list_iterate(pplayer->cities, pcity) {
  city_tile_iterate_index(nmap, city_map_radius_sq_get(pcity), pcenter, ptile, cindex) {
```

Workers **only ever consider tiles inside a city's work radius.** README.AI
confirms it as a known defect: *"AI won't build cross-country roads outside of
city radii."* Cities more than 2·radius apart are simply never connected on
purpose. Any connection that exists is an accident of two radii overlapping.

**Connectivity heuristic** — `adv_workers_road_bonus()`, `autoworkers.c:132–297`.
It's a hand-coded pattern matcher over a fixed 12-tile neighbourhood
(`dx[12]`/`dy[12]`), returning an integer **0–8**, with this comment in the
middle of it:

```c
/* FIXME: if you can understand the algorithm below please rewrite this explanation! */
```

It has no concept of "city A", "city B", or "path". It is a purely local "does
this square continue a line" detector. It is then scaled by
`INFRA_WEIGHTING = TRADE_WEIGHTING * 0.75` = **13.5**, which is small next to a
tile's own output value.

**Railroads** get no strategic treatment at all. Their value enters only via
(a) the tile's own shield bonus and (b) a local move-cost multiplier at
`autoworkers.c:673–688`. Nothing values a rail *network* for troop redeployment
— which is the entire point of railroads in a Civ-like.

### The fix: a real road network planner

Override `settler_run` and add an empire-level planner in `phase_begin`.

**Step 1 — maintain a city graph.** Each turn (or on city founding/loss), build
a graph over your cities. Edge weight = pathfinder cost of the best available
road-capable route. `common/aicore/pf_tools.h` gives you everything; you're
already paying for PF maps all over the AI.

**Step 2 — compute a target network.** Don't connect all pairs. Compute a
**minimum spanning tree** over the city graph, then add shortcut edges where
they materially cut the diameter (a light Steiner-ish pass — greedily add the
edge that most reduces total pairwise distance, until marginal gain < threshold).
Cache the resulting set of tiles as `plr_data->road_plan[]`, a
`bv_tile`-style bitvector plus a per-tile priority.

**Step 3 — make workers see it.** In your `settler_run` override, iterate
*plan tiles* in addition to city-radius tiles. Give each plan tile a want of

```
want = INFRA_WEIGHTING_STRATEGIC
     * (traffic_estimate(edge) )         /* how many pairs route through here */
     * (is_last_gap_in_segment ? 3 : 1)  /* finish what you started */
     / build_time
```

The `is_last_gap` bonus matters enormously in practice: a 90%-complete road is
worth nothing, so the AI must finish segments rather than spreading effort.
This is the single behaviour that makes an AI *look* competent.

**Step 4 — railroads as a mobility asset.** Once Railroad is known, re-run the
same planner with a second objective: minimise worst-case turns from your
**military staging city** (see §4) to any border city. Weight rail on the
trunk routes of the MST 3–5× above rail on leaf spurs. Also: rail under
mines/hills for the shield bonus is a *separate*, already-working want — keep it,
just don't let it starve the trunk.

**Step 5 — cap it.** Add a worker-budget allocator so the network plan can't
consume every worker. Something like 30–40% of worker-turns to network,
remainder to tile improvement, with the split shifting toward network when the
empire diameter grows.

Worker count itself: `city_data->worker_want` (set in
`contemplate_terrain_improvements`, `daicity.c:765`) will need to rise, since
you're creating a lot more work. Scale it with total unbuilt plan length.

---

## 3. Concentrating force

### What's there

`find_something_to_kill()` — `ai/default/daiunit.c:1214–1520`, ~300 lines, and
the comment at 1284 says:

> `/* This horrible piece of code attempts to calculate the attractiveness of enemy cities as targets... */`

**Each unit independently** builds a PF map, iterates every enemy city, and
picks its own max-want target. There *is* a shared signal —
`reinforcements_cost_and_value()` fills `city_data->attack` and
`city_data->invasion.attack/occupy` (`daicity.h:31`) — but it's advisory: a unit
reads how many friends are heading somewhere and adjusts its own want. Nobody
*decides* anything. There is no plan object, no muster point, no synchronised
arrival, no "wait three turns for the artillery".

Add `dai_military_rampage()` (`daiunit.c:669`) — greedy hit-and-run on anything
profitable within reach — and you get exactly the trickle you're describing.
README.AI's own TODO: *"Teach the AI to coordinate the units in an attack (ok,
this one is a bit big...)"*.

`struct ai_plr` (`daidata.h:69`) has room for essentially nothing: a
`diplomacy.war_target` pointer and some stats. There's no campaign state.

### The fix: an explicit campaign object

Put this in your player data blob:

```c
struct campaign {
  int target_city_id;
  struct tile *muster_tile;      /* friendly city or fortified tile, 2-4 turns out */
  enum { CAMP_FORMING, CAMP_MOVING, CAMP_ASSAULT, CAMP_DONE } phase;
  int required_attack;           /* shield-equivalent power needed */
  int committed_attack;          /* what's assigned */
  int committed_occupy;          /* can_unit_take_over units */
  int deadline_turn;             /* abort if we haven't launched by then */
  struct unit_list *assigned;
};
```

**Target selection (once per turn, empire level, not per unit).** Score enemy
cities by `value / defence`, and *heavily* weight:
- capital / wonder / high-production cities
- **cluster potential**: a city whose neighbours are also weak, so one
  breakthrough cascades. Compute `sum over cities within N tiles of (value/defence)`.
  This is what "concentrating on a single point of pain" actually means
  mathematically — pick the point where a breach unzips the most.
- distance from *your* production centre, not from the individual unit

Pick **one** primary target (maybe two if you're large). Everything else gets
defensive posture.

**Force requirement.** `required_attack` = enough that expected losses leave you
holding the city, computed against the *defender the city can build in
`move_time` turns* — `dai_choose_defender_versus()` at `daimilitary.c:89`
already does this projection, reuse it. Add a margin (1.5–2×) because Freeciv
combat is high-variance. Require at least 2 `can_unit_take_over` units.

**Muster, then move.** In `CAMP_FORMING`, units with a campaign assignment go to
`muster_tile` and **fortify** — they do *not* rampage, do *not* pick their own
targets. Suppress `dai_military_rampage` for assigned units entirely; that's a
one-line guard and it's most of the win. Transition to `CAMP_MOVING` only when
`committed_attack >= required_attack`.

**Move as a stack with synchronised arrival.** Compute ETA for the slowest unit;
faster units hold. Arrive together on the tile adjacent to the target, ideally
one that's defensible. Then `CAMP_ASSAULT`: attack in ascending order of
attacker value (cheapest first — soak the good defender), occupy with the
reserved units.

**Production feedback.** This is what makes armies *huge*: `required_attack -
committed_attack` is a direct, unambiguous shield demand. Feed it back into
`military_advisor_choose_build()` (`daimilitary.c:1791`) as a want on the
specific unit types the campaign is short of, spread across your high-shield
cities. Right now unit wants are computed per city from local danger, so nothing
ever says "the empire needs eleven more musketeers." A campaign object says
exactly that.

Note `DAI_WANT_MILITARY_EMERGENCY = 1000.1` and `DAI_WANT_DOMESTIC_MAX = 750`
(`daitools.h:31–33`) — the ceiling structure is already there for you to slot
campaign want in between domestic and emergency.

**Abort conditions.** If `deadline_turn` passes without launching, or the target
gets reinforced past `required_attack * 1.5`, or a home city hits
`grave_danger` — dissolve the campaign and release units. Without this you get
deadlock.

---

## 4. The boats

You're right that it's the worst-looking behaviour. `doc/README.AI` describes
the design in its own words:

> *"The ferry system of Freeciv is probably better described by statistical
> mechanics than by logic. Both ferries and prospective passengers move around
> in what looks like a random fashion, trying to get closer to each other. On
> average, they succeed."*

and

> *"Each turn both boats and PPs forget all about prior arrangements."*

`daiferry.c:449–463` *does* check remaining capacity, so packing is
mechanically possible. The problem is that nobody batches: each passenger
independently calls `aiferry_find_boat()` (`daiferry.c:497`), and once one
passenger boards it takes the helm and sails (`README.AI`: *"one of the
passengers is given full control"*).

**Fix, in ascending order of effort:**

1. **Don't sail until full-ish.** A loaded boat waits in port until
   `loaded >= min(capacity, waiting_passengers)` or a timeout (~3 turns).
   Roughly 30 lines. Fixes 80% of the visual absurdity.
2. **Persist reservations across turns.** Kill the forget-everything rule; keep
   `ferryboat`/`passenger` bindings and only re-plan on invalidation. The
   diplomat code already does this and `README.AI` points to it as the model.
3. **Make overseas invasion a campaign phase.** With §4's campaign object, an
   amphibious operation is just `CAMP_FORMING` with `muster_tile` = a coastal
   city and a transport requirement in `required_attack`. The boats become part
   of the force requirement rather than an independent subsystem.

---

## 5. Sequencing

Each phase is independently shippable and independently testable.

**Phase 0 — scaffolding (1 evening).** Copy `ai/classic/classicai.c` to
`ai/deep/deepai.c`, forward everything to `dai_*`, add to `meson.build`. Verify
you can run `set aitype deep` and it plays identically to classic. Set up a
headless N-game batch runner scoring final `player->score`.

**Phase 1 — build up, not out (§1).** Full-catchment site scoring + specials
term + horizon-aware `base_want`. Biggest single win, and it's mostly arithmetic
in two functions. Expect fewer, much larger cities.

**Phase 2 — road network (§2).** MST planner + `settler_run` override. Visible
immediately; also silently boosts Phase 1 by making big cities' tiles better and
Phase 3 by making redeployment fast.

**Phase 3 — campaigns (§3).** Biggest code volume, biggest strategic payoff.
Ship the muster-and-suppress-rampage core first; add synchronised arrival and
production feedback after.

**Phase 4 — ferries (§4).** Once campaigns exist, most of this falls out.

**Phase 5 — tuning.** Everything above introduces constants. Expose them in a
struct, then hill-climb: `PERFECTION`, `GROWTH_POTENTIAL_DEEMPHASIS`, horizon
length, network budget %, force margin, muster timeout.

### On measurement

Don't trust eyeballing. Batch-run 50+ headless games per config
(`freeciv-server --read script.serv`), fixed map seeds, and track: final score,
average city size, cities founded vs. improvements built ratio, road tiles per
city, turn of first city capture, units lost per city captured. Freeciv games
are high-variance; 10 games will lie to you.

Gate every change behind `H_EXPERIMENTAL` (`ai/handicaps.h`) so you can run your
AI against `classic` in the same game and diff the score curves directly.

---

## 6. Two things worth deciding early

**Ruleset generality.** `doc/README.AI` states the project's long-term goal as
an AI that *"can handle all the rules possibilities that Freeciv can offer."*
Everything above can be written either generically (query effects/requirements
at runtime) or with civ2civ3 assumptions baked in. Generic is maybe 30% more
work and is the difference between a patch upstream might take and a personal
fork. Your call, but decide before Phase 1, not after.

**CPU budget.** Full-catchment scoring at every candidate tile is ~10× the
current cost of `cityresult_fill()`, and it's already flagged in-source as
`/* Burn CPU, burn! */` (`daisettler.c:755`). The existing `tdc_plr_get`/
`tdc_plr_set` tile cache (`daisettler.c:514–560`) is your friend — extend it
rather than bypassing it. The MST planner should run once per turn per player,
not per worker. If it gets bad, `ai/tex` shows how to move AI work onto a
separate thread.

---

## Quick reference — files you'll touch

| File | Lines | What lives there |
|---|---|---|
| `ai/default/daisettler.c` | 1402 | Site scoring, settler dispatch, worker dispatch entry |
| `ai/default/daicity.c` | 2261 | Building want, `base_want`, city production, emergencies |
| `ai/default/daidomestic.c` | 696 | Settler/worker/caravan want, trait multipliers |
| `ai/default/daiunit.c` | 3589 | `find_something_to_kill`, rampage, unit tasks |
| `ai/default/daimilitary.c` | 2068 | Danger assessment, attacker/defender selection |
| `ai/default/daiferry.c` | 1332 | Boats |
| `server/advisors/autoworkers.c` | 1365 | Worker tile search, `road_bonus` — **shared with humans** |
| `server/advisors/infracache.c` | 512 | Per-tile activity value cache |
| `server/advisors/advbuilding.c` | 346 | Wonder city selection, building want plumbing |
| `ai/classic/classicai.c` | 719 | The module template to copy |
| `doc/README.AI` | 342 | Read it in full; the TODO list is a roadmap |
