# Part 21 — Pathfinding caches

**Extends Part 17 §3 and W4.** Baseline: `freeciv/freeciv` @ `0896b34`.

---

## 1. What's there now

**No caching at all.** 90 `pf_map_new()` / `pf_map_destroy()` call sites across
`ai/` and `server/advisors/`. Every one builds a map, walks it, throws it away.
The "cached values" in `path_finding.c` are per-node memoization *within* a single
map's lazy Dijkstra — nothing survives the call.

`find_something_to_kill()` builds one **per unit, per turn**, and iterates every
enemy city, so it explores most of the reachable map each time. Five musketeers
sitting in one city build five identical maps.

## 2. The constraint that decides the architecture

Under W4 (honest information), **a shared pathfinding structure is an
information-leak hazard.**

If you precompute distances or paths over the real map and let each AI read them,
the AI learns the cost of terrain it has never seen. A precomputed distance from
A to B *encodes* what lies between them. Reachability is worse — "can I get
there" over the real map reveals a land bridge you haven't scouted.

So the rule is:

> **Share the primitives. Never share the products.**

| Shareable | Not shareable |
|---|---|
| Per-tile, per-unit-class move cost | Distances |
| Per-tile terrain/extra derived data | Paths |
| Ruleset-derived move rules | Reachability / components |

A shared *table* of move costs is fine even though it covers unseen tiles,
because the AI's traversal never reads an entry for a tile it doesn't know. It's
shared **storage** with gated access. A shared *distance* can't be gated, because
the number itself is the leak.

## 3. So: both, with different jobs

### 3.1 Shared — a move-cost table

Your instinct about invalidating on road-building is right, and this is the layer
it applies to.

```c
/* Indexed by [tile][direction][unit_class] — or lazily, per class */
static uint16_t *move_cost_cache;
```

Terrain plus extras determine move cost, and both change rarely. Invalidate on:
terrain transform, extra built or removed (road, rail, river work), and pollution
or global warming. In practice that means **at turn change, after activities
resolve** — exactly the cadence you described.

The win is modest — it removes repeated `map_move_cost_unit()` work from every
traversal — but it's shared across all 20 AIs and it's the only thing that
*safely* can be.

### 3.2 Per-AI — a pf_map cache, and this is where the win is

Cache whole `pf_map` objects **within a turn**, keyed on the parameter tuple:

```c
struct pf_cache_key {
  const struct tile      *start;
  const struct unit_type *utype;
  const struct player    *owner;
  int   moves_left_initially;
  void *get_MC, *get_TB, *get_EC, *get_action;   /* callback identity */
  uint32_t param_flags;
};
```

`dai_fill_unit_param()` derives parameters from unit type + task + player, so
**units of the same type, same task, same start tile produce byte-identical
maps.** A stack of five musketeers in a city is one map, not five. Across a
20-player game with large stacks this is a large constant-factor cut on the
single most-called expensive routine.

Per-AI by construction, so no leak. Flushed at turn end, since units move.

One prerequisite: the callbacks must be deterministic functions of their inputs.
Most are. `get_EC` for risk avoidance reads `param->data` (the `adv_risk_cost`),
so the key needs to cover that too — or you cache per (unit, turn) rather than
per (type, turn), which is a smaller but still real win.

### 3.3 Per-AI — reachability, for the early-out

Part 17 §5 wants "early-out on hopeless targets." That's a union-find over the
AI's **known** tiles, per unit class:

```c
/* Maintained incrementally: on each newly-revealed tile, union with
   known passable neighbours. O(α) amortised. */
int known_component[MAP_INDEX_SIZE];
```

Before building any PF map, ask: is the target in the same component as the
start, for this unit's class? **O(1) rejection**, and it kills the common case
where `find_something_to_kill` evaluates enemy cities on other continents that a
land unit could never reach.

Per-AI, over known tiles only — which is exactly why it can't be shared, and also
why it's *correct*: an AI that hasn't scouted the isthmus genuinely shouldn't
know the two landmasses connect.

### 3.4 Later, if still needed — landmarks

Pick K landmarks, run K Dijkstras over known tiles, use the triangle inequality
as an admissible A* heuristic. Standard ALT. It can cut exploration dramatically
on long paths.

Per-AI, and I'd hold it — measure after 3.2 and 3.3, because those are far
cheaper to build and the campaign object (W13) removes most of the per-unit
target search that makes long paths common in the first place.

## 4. Two invalidation clocks

Worth stating explicitly, because getting this wrong reintroduces the leak:

- **The shared cost table** is invalidated on **actual** terrain/extra change.
- **Each AI's derived caches** are invalidated on **observed** change.

An enemy builds a railroad you haven't seen: the shared table updates, your
reachability and pf_map caches do not, and your paths continue to price that tile
at its last-known cost. That's correct behaviour, not a staleness bug — and it
falls out naturally if the per-AI structures key off the knowledge layer's
`last_updated` rather than the world clock.

It also means the AI can be *surprised* by infrastructure, which is a small piece
of the deception surface W4 opens up.

## 5. Ordering

1. **Measure.** `AIT_FSTK` and the PF buckets, on the 20-player case. Part 17
   predicts danger and building want dominate — PF may be third. Don't build a
   cache for a cost you haven't confirmed.
2. **§3.3 reachability early-out.** Cheapest thing here, biggest asymptotic
   effect on `find_something_to_kill`, and it needs no cache invalidation logic
   at all.
3. **§3.2 pf_map cache.** Straightforward, per-AI, turn-scoped.
4. **§3.1 shared cost table.** Only if profiling shows `get_MC` is hot.
5. **§3.4 landmarks.** Probably never, after W13.

## 6. Commits

```
common: add a per-player known-tile connectivity index per unit class
common: maintain the connectivity index incrementally as tiles are revealed
ai/default: reject unreachable targets before building a pathfinder map
common: add a turn-scoped pathfinder map cache keyed on parameter identity
ai/default: reuse cached pathfinder maps for units sharing start and parameters
common: flush pathfinder caches at turn change
common: add a shared move-cost table invalidated on terrain and extra change
test: assert cached and uncached pathfinding agree on fixed seeds
test: assert an AI cannot path through terrain it has not observed
```

That last test is the one that matters. It's the guard against someone later
"optimising" by sharing a distance table and silently handing every AI perfect
map knowledge — undoing W4 without anyone noticing.
