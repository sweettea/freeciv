# W28 — Worker stacking

**Phase 1/2. No dependency on W01. Lands in `server/advisors/` (approved exception).**
Root cause: RC1 corollary (greedy one-step evaluation).

## Problem

The AI puts **one** worker per tile. Stacking works mechanically and would be
worth far more than it looks.

## Evidence

**Stacking accelerates work** — `server/unittools.c`:
```c
static int total_activity(struct tile *ptile, enum unit_activity act, ...)
{
  int total = 0;
  unit_list_iterate(ptile->units, punit) {
    if (punit->activity == act && (!tgt_matters || punit->activity_target == tgt))
      total += punit->activity_count;
  } unit_list_iterate_end;
  return total;
}
```
Sums across **every unit on the tile**. N workers = N× progress. Late arrivals
contribute from arrival onward.

**The AI forbids it** — `autoworkers.c:490`:
```c
/* Do not go to tiles that already have workers there. */
unit_list_iterate(ptile->units, aunit) {
  if (unit_owner(aunit) == pplayer && aunit->id != punit->id
      && unit_has_type_flag(aunit, UTYF_WORKERS)) consider = FALSE;
} unit_list_iterate_end;
if (!consider) continue;
```
Not deprioritised — **`continue`**. And the reservation struct can't express
anything else: `struct workermap { int enroute; int eta; }` — one unit ID.

## Why it's a value bug, not an efficiency bug

Worker-turns are conserved. But value is **discounted by delay** — `amortize()`
applies `(23/24)` per turn (`MORT = 24`, `advtools.h:19`):

| Job | 1 worker | 3 workers | 6 workers |
|---|---:|---:|---:|
| 36 wt (Ocean→Grassland, Grassland→Lake) | **0.216** | 0.600 | 0.775 |
| 96 wt (Mountains→Hills→Plains→Lake) | **0.017** | 0.256 | **0.506** |

A single worker on a 96-turn chain delivers **1.7% of nominal value**. Nothing
scores above noise at 1.7%, so the AI correctly concludes it isn't worth doing —
**and never terraforms anything expensive.** Six workers put it at 50.6%.

This compounds with W29: the AI evaluates only the first step of a chain, *and*
discounts that step as if one worker will grind it out alone. Two independent
bugs, each sufficient on its own.

## Design

```c
struct workermap {
  int assigned[MAX_WORKERS_PER_TILE];
  int n_assigned;
  int eta[MAX_WORKERS_PER_TILE];
  int rate_committed;      /* sum of activity rates, not unit count */
};
```

**Marginal assignment:**
```c
turns_now  = ceil(remaining_activity / rate_committed);
turns_next = ceil(remaining_activity / (rate_committed + this_worker_rate));
marginal_want = amortize(tile_value, turns_next) - amortize(tile_value, turns_now);
```
Compare against the worker's best *unassigned* tile — the displacement logic at
`autoworkers.c:1029-1055` already handles "is this worker better used elsewhere."

**Snap to a divisor.** Waste is `N * ceil(T/N) - T`, **zero exactly when N divides
T**. For 36 wt: N ∈ {1,2,3,4,6,9,12,18,36} waste nothing; N=5 wastes 4 worker-turns.

**Weight by rate, not count.** `get_activity_rate()` scales with `move_rate` and
veteran level, so Engineers work at 2× Workers. Two Engineers + one Worker is 5
rate-units, not 3.

**Cap by danger.** A six-worker stack is one target; under `killstack` one attack
takes all. Feed `adv_danger_at()` into the cap — same signal W05 uses.

**Cap by breadth.** Stacking wins when the job is long (steep discount) or the
tile is on a plan's critical path (partial completion worth nothing). So:
**stack on plan tiles, spread on opportunistic improvement.**

## Placement note

`server/advisors/autoworkers.c` is shared with human auto-workers — **who also
refuse to stack**, which is equally a defect for them. This is one of the two
approved `advisors` exceptions in `CLAUDE.md` rule 4.

## Acceptance

- Three workers complete a 36-turn transform in 12 turns (L0 test)
- Stacking declined when the marginal saving is only a partial turn
- Long transforms (>60 wt) actually get built at all — currently ~never

## Commits

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
