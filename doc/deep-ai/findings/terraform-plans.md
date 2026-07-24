# Part 18 — Multi-step terraforming

**Extends PRD v2 W12.4 and Part 16.** Baseline: `freeciv/freeciv` @ `0896b34`.

---

## 1. Confirmed: the evaluator sees exactly one step

`adv_calc_transform()` — `server/advisors/infracache.c:142`:

```c
old_terrain = tile_terrain(ptile);
new_terrain = old_terrain->transform_result;      /* ← ONE step */
...
if (!terrain_surroundings_allow_change(nmap, ptile, new_terrain)) {
  return -1;
}
vtile = tile_virtual_new(ptile);
tile_change_terrain(vtile, new_terrain);
goodness = city_tile_value(pcity, vtile, 0, 0);   /* ← TILE YIELD only */
tile_virtual_destroy(vtile);
```

Three hard limits, all confirmed:

1. **One step.** `old_terrain->transform_result` — the immediate successor, never
   the chain.
2. **Tile yield only.** `city_tile_value()` measures food/shield/trade. It cannot
   see connectivity, and connectivity is the entire point of a canal.
3. **Requires a city.** The signature takes `const struct city *pcity`, and the
   call site sits inside the city-radius loop (RC3). Tiles outside any city
   radius are never evaluated at all.

## 2. The actual chains, and what they cost

From `data/civ2civ3/terrain.ruleset`:

| From | Chain to water | Steps | Worker-turns |
|---|---|---:|---:|
| Grassland | → Lake | 1 | 36 |
| Plains | → Lake | 1 | 36 |
| Glacier | → Lake | 1 | 24 |
| Swamp | → Grassland → Lake | 2 | 60 |
| Desert | → Plains → Lake | 2 | 60 |
| Tundra | → Plains → Lake | 2 | 60 |
| Hills | → Plains → Lake | 2 | 60 |
| Forest | → Hills → Plains → Lake | 3 | 84 |
| **Mountains** | → Hills → Plains → Lake | 3 | **96** |
| **Jungle** | → Forest → Hills → Plains → Lake | 4 | **108** |

And the reverse: `Ocean → Grassland` and `Lake → Grassland`, one step, 36 turns
each. `Deep Ocean` has `transform_result = "no"` — so bridging is bounded to the
shelf, which is a convenient natural limit.

So a three-tile canal through hills is **180 worker-turns**. That's a
decades-long civil engineering project, which is thematically exactly right —
and completely invisible to a one-step evaluator.

## 3. Where greedy fails, precisely

The AI's improvement selection is a **greedy one-step hill-climb over tile
yield.** That's fine for irrigating a grassland. It fails in three distinct ways,
and the interesting thing is that they compose:

| Failure | Description | Example |
|---|---|---|
| **Chain** | The path to the goal passes through lower-valued states | `Mountains → Hills → Plains → Lake`: the last step *destroys* a good tile |
| **Set** | Value materialises only when N tiles complete together | A canal or land bridge is worth nothing at 90% |
| **Objective** | The value isn't tile yield at all | Connectivity, chokepoint control, catchment unlocking |

Farmland/supermarket (Part 16) was **chain + objective** — zero value until a
co-investment lands.

Canal-digging is **all three at once**, which is why it's never been attempted:
multi-step per tile, multi-tile per canal, and the payoff is ship movement rather
than food.

## 4. The ordering constraint nobody would guess

`terrain_surroundings_allow_change()` gates each transform on what's adjacent.
Practically: you generally can't turn a mid-ocean tile into land with no
neighbouring land, nor create an isolated water tile with no adjacent water.

**So a land bridge must be built outward from an existing shore, and a canal must
be dug inward from existing water.** You cannot start in the middle.

That means a terraform plan isn't a set of tiles — it's an **ordered sequence**,
possibly worked from both ends simultaneously, with each step unlocking the next.
The plan structure has to carry that ordering, and progress is genuinely
sequential rather than parallelizable across the whole span.

It also gives you a clean abort story: a half-built canal is a peninsula that
still works as land, so partial progress isn't catastrophic — it's just wasted.

## 5. Design: terraform plans

This is the same object as W12's road plan, with a different cost model and an
ordering constraint. Build it into the same planner.

```c
struct terraform_plan {
  enum { TF_LAND_BRIDGE, TF_CANAL, TF_CATCHMENT } kind;
  struct tile **sequence;        /* ordered — see §4 */
  int    n_tiles;
  int    total_worker_turns;     /* sum over chains, per tile */
  adv_want goal_value;           /* NOT tile yield — see below */
  int    completed;              /* progress; plan is worthless until n_tiles */
};
```

### 5.1 Value the goal, never the step

Each `kind` gets its own objective, and none of them is `city_tile_value`:

- **`TF_LAND_BRIDGE`** — reuse W12's network value: turns saved per crossing ×
  expected crossings, plus land unlocked beyond, **minus** the invasion-route
  cost from the chokepoint map. A bridge you create is a chokepoint you must
  garrison.
- **`TF_CANAL`** — the naval equivalent: shortest sea route between two water
  bodies before vs after, times expected naval traffic. Directly reuses the same
  graph machinery, just over the water network. A canal that turns a
  circumnavigation into three tiles is enormous; one that saves four tiles is
  not worth 180 worker-turns.
- **`TF_CATCHMENT`** — the one case where tile yield *is* the objective, but
  summed over a whole city's catchment rather than one tile: filling a bay so a
  future city site becomes viable. Reuses W2's full-catchment scoring.

### 5.2 Chain-aware costing

Replace the one-step lookahead with a shortest-path search over the terrain
transform graph, which is small, static, and computable once at ruleset-probe
time (W8.1):

```c
/* Precomputed at probe time: transform_dist[from][to] = {steps, worker_turns} */
```

That's a dozen terrains and an all-pairs shortest path — trivial, and it makes
the ruleset-general answer available everywhere at zero runtime cost. It also
tells the planner to route a canal through grassland rather than mountains,
which is a 36-vs-96-turn difference per tile and the single biggest lever on
whether a canal is affordable at all.

### 5.3 Tagged wants that survive local valuation

Same requirement as Part 16 and W12: the worker want on a plan tile must be
**tagged**, because `adv_calc_transform` will keep returning −1 or a negative
delta for `Grassland → Lake` right up until the canal opens.

Use the single extension point from Part 15 §3 —
`adv_extra_tile_want_cb` — for all three consumers (road plan, co-investment
bundles, terraform plans). One mechanism, three users, rather than three hacks.

### 5.4 Gate it hard

Terraform plans are enormous investments and the AI should almost always decline:

- Only under `ECON_CONSOLIDATE` / `ECON_ENRICH`, never `LANDGRAB`
- Only with surplus worker capacity beyond the road plan's budget
- Only when `goal_value / total_worker_turns` beats the marginal road segment
- Abort and bank the partial progress if doctrine shifts or the frontier moves

An AI that digs one spectacular canal in a long game is a delight. An AI that
starts four and finishes none is a bug.

## 6. Where it belongs

Per Part 15's rule — **does this need state only the new module has?**

- **`ai/default`**: the chain-aware transform costing (§5.2). It's a strictly
  better version of `adv_calc_transform` that answers "what's the cheapest path
  to terrain X and what does it cost," and it improves ordinary in-catchment
  terraforming too — today the AI won't transform Mountains toward Plains even
  when Plains is what it wants, because step one looks unappealing in isolation.
- **`ai/deep`**: the plans themselves. They need the road/water graph, the
  chokepoint map, doctrine state, and the worker budget.

## 7. Commits

To **`ai/default`** (extends W3-era work):
```
advisors: precompute all-pairs terrain transform distances at ruleset load
advisors: expose cheapest transform path and cost between two terrains
ai/default: value terraforming against the best reachable terrain, not the next one
test: assert mountains are transformed toward plains when plains is wanted
```

To **W12** in `ai/deep`:
```
ai/deep: add a terraform plan with ordered tile sequence and goal value
ai/deep: plan land bridges valued by network gain minus invasion exposure
ai/deep: plan canals valued by naval route shortening
ai/deep: route terraform plans through the cheapest transform chain
ai/deep: respect surroundings constraints by building outward from existing shore
ai/deep: emit tagged worker wants for terraform plan tiles
ai/deep: restrict terraform plans to consolidate and enrich doctrines
ai/deep: abandon terraform plans on doctrine change and bank partial progress
test: assert a canal is only planned when route shortening justifies the cost
```

---

## 8. The pattern worth naming in the PRD

Parts 16 and 18 are the same bug wearing different clothes, and it's worth
stating once in Part II as a root-cause corollary:

> **RC1 corollary — greedy one-step evaluation.** The AI evaluates each
> improvement independently, one step ahead, against local yield. Anything
> requiring a *sequence* of steps, a *set* of coordinated investments, or an
> objective other than tile yield is invisible — not undervalued, but exactly
> zero or negative at every point along the path.

That single sentence covers farmland/supermarket, canals, land bridges, road
networks, and rail trunks. And the fix is the same shape in every case: **plan
objects with goal-level value, and tagged wants that survive local valuation
saying the next step is worthless.**

Which means the extension point from Part 15 §3 isn't a convenience — it's the
mechanism that makes an entire class of behaviour possible.
