# Part 20 — Worker stacking

**Extends Part 18 and W12.** Baseline: `freeciv/freeciv` @ `0896b34`.

---

## 1. The mechanic works. The AI forbids it.

Stacking genuinely accelerates work — `server/unittools.c`:

```c
static int total_activity(struct tile *ptile, enum unit_activity act,
                          struct extra_type *tgt)
{
  int total = 0;
  unit_list_iterate(ptile->units, punit) {
    if (punit->activity == act
        && (!tgt_matters || punit->activity_target == tgt)) {
      total += punit->activity_count;
    }
  } unit_list_iterate_end;
  return total;
}
```

`activity_count` accumulates per unit per turn and `total_activity` **sums across
every unit on the tile**. N workers is N× progress. Late arrivals contribute from
arrival onward, so the mechanic handles staggered assignment naturally.

Now `server/advisors/autoworkers.c:490`:

```c
/* Do not go to tiles that already have workers there. */
unit_list_iterate(ptile->units, aunit) {
  if (unit_owner(aunit) == pplayer
      && aunit->id != punit->id
      && unit_has_type_flag(aunit, UTYF_WORKERS)) {
    consider = FALSE;
  }
} unit_list_iterate_end;

if (!consider) {
  continue;
}
```

Not deprioritised — **`continue`**. If one of your own workers is standing on a
tile, no other worker will ever evaluate it.

And the reservation structure can't express anything else anyway:

```c
struct workermap {
  int enroute;  /* Unit ID of worker en route to this tile */
  int eta;
};
```

**One** unit ID. Singular by construction.

## 2. Why this is a value bug, not an efficiency bug

Worker-turns are conserved either way — 36 turns of work costs 36 worker-turns
whether one worker takes 36 turns or three take 12. So stacking looks like it
shouldn't matter.

It matters enormously, because value is **discounted by delay**.
`amortize()` applies `(23/24)` per turn (`MORT = 24`, `advtools.h:19`):

| Job | Workers | Turns | Discount factor |
|---|---:|---:|---:|
| **36 wt** — Ocean→Grassland, Grassland→Lake | 1 | 36 | **0.216** |
| | 2 | 18 | 0.465 |
| | 3 | 12 | **0.600** |
| | 6 | 6 | 0.775 |
| **96 wt** — Mountains→Hills→Plains→Lake | 1 | 96 | **0.017** |
| | 2 | 48 | 0.130 |
| | 3 | 32 | 0.256 |
| | 6 | 16 | **0.506** |

A single worker on a 96-turn chain delivers **1.7% of nominal value**. Nothing
scores above noise at 1.7%, so the AI correctly concludes it isn't worth doing —
and never terraforms anything expensive.

Six workers put the same job at **50.6%**, which is competitive with ordinary
improvements.

**So the one-worker rule is a large part of why the AI's terraforming is
inert.** It compounds with Part 18: the AI evaluates only the first step of a
chain, *and* discounts that step as though one worker will grind it out alone.
Two independent bugs, both crushing the same behaviour, each sufficient on its
own.

The same arithmetic applies to Part 18's canals and land bridges. A three-tile
canal through hills is 180 worker-turns. One worker per tile: 60 turns each,
discount 0.079. Three per tile: 20 turns, discount 0.427 — a **5.4× swing** in
whether the plan is ever chosen.

## 3. The divisor point is right

Waste from the final partial turn is `N × ceil(T/N) − T`, which is **zero exactly
when N divides T**.

For a 36-worker-turn job: N = 1, 2, 3, 4, 6, 9, 12, 18, 36 all waste nothing.
N = 5 gives `ceil(36/5) = 8` turns × 5 workers = 40 worker-turns — **4 wasted**.
N = 7 wastes 6.

So the assignment rule has two parts:

1. **Cap N** where the marginal discounted gain stops beating the worker's
   opportunity cost elsewhere.
2. **Snap N down to a divisor** of the remaining work, so the last turn isn't
   half-idle.

Note `get_activity_rate()` scales with `move_rate` and veteran level — Engineers
at `move_rate 2` work twice as fast as Workers, and veterans faster still. So T
must be expressed in *activity points* and N in *rate-weighted* workers, not raw
unit counts. A mixed stack of two Engineers and one Worker is 5 rate-units, not 3.

## 4. Design

```c
struct workermap {
  int assigned[MAX_WORKERS_PER_TILE];  /* was: int enroute */
  int n_assigned;
  int eta[MAX_WORKERS_PER_TILE];
  int rate_committed;                  /* sum of activity rates assigned */
};
```

**Marginal assignment.** Compute the want of adding one more worker to a tile
that already has work assigned:

```c
turns_now  = ceil(remaining_activity / rate_committed);
turns_next = ceil(remaining_activity / (rate_committed + this_worker_rate));

marginal_want = amortize(tile_value, turns_next)
              - amortize(tile_value, turns_now);
```

Then compare that against the worker's best *unassigned* tile, exactly as the
existing comparison already works. No new machinery — the displacement logic at
`autoworkers.c:1029–1055` already handles "is this worker better used elsewhere."

**Snap to a divisor.** Before committing the Nth worker, check whether
`rate_committed + this_rate` divides `remaining_activity`. If not, and the next
divisor up is within reach, prefer it; if the marginal gain is only from a
partial turn, decline.

**Cap by danger.** A stack of six workers is a single target, and under
`killstack` one attack kills all of them. Feed `adv_danger_at()` into the cap:
stack freely in the interior, cap at one or two near a contested frontier. This
is the same signal W5 uses for settler routing.

**Cap by breadth.** Six workers finishing one tile in six turns is worse than six
workers finishing six tiles in thirty-six turns *if* all six tiles are equally
valuable — because you get six improvements either way, but spread over the same
horizon. Stacking wins when (a) the job is long, so the discount curve is steep,
or (b) the tile is on the critical path of a plan (Part 18's ordered terraform
sequences, W12's road segments), where partial completion is worth nothing.

That second case is the important one and it links back cleanly: **plan tiles
should stack; opportunistic tile improvement generally shouldn't.**

## 5. Where it goes

`server/advisors/autoworkers.c` is shared with human auto-workers — and human
auto-workers refuse to stack too, which is equally a defect for them. Per Part
15's rule this needs no new state, so it belongs in **`server/advisors/`** and
benefits everyone.

The plan-driven stacking preference (§4, "cap by breadth") needs the plan, so the
*policy* input comes through the same `adv_extra_tile_want_cb` extension point
from Part 15 §3 that the road plan, co-investment bundles, and terraform plans
all use. Four consumers, one hook.

## 6. Commits

```
advisors: let the worker map hold multiple assignments per tile
advisors: track committed activity rate per tile instead of a single unit
advisors: allow workers to consider tiles other workers are already working
advisors: value an additional worker by the completion time it saves
advisors: prefer worker counts that divide the remaining activity evenly
advisors: weight workers by activity rate rather than counting units
advisors: cap worker stacking near danger
advisors: prefer stacking on plan tiles over opportunistic improvements
test: assert three workers complete a transform in a third of the turns
test: assert stacking is declined when the marginal turn saving is partial
```

---

## 7. Worth adding to the PRD's root causes

This is a third instance of the same underlying shape as Parts 16 and 18, and it
sharpens the RC1 corollary I proposed there:

> **RC1 corollary — greedy one-step evaluation.** The AI evaluates each
> improvement independently, one step ahead, with one worker, against local
> yield. Anything requiring a *sequence* of steps, a *set* of coordinated
> investments, *concurrent* effort, or an objective other than tile yield is
> invisible — not undervalued, but discounted to approximately zero at every
> point along the path.

Farmland/supermarket, transform chains, canals, land bridges, road networks, rail
trunks — and now the discount curve that makes all of them look worthless even
when correctly identified. Same fix shape throughout: **plan objects carrying
goal-level value, and tagged wants that survive local valuation.**
