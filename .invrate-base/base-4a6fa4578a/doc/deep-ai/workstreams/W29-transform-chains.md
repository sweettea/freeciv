# W29 — Transform chains

**Phase 2 (with W02/W03). Approved in-place `server/advisors` change
per CLAUDE.md rule 4 (shared defect: human auto-workers are equally
chain-blind). Extracted from PRD-v3 Part VI (W29) +
findings/terraform-plans.md on 2026-07-25. MUST precede W12.4.**

## Problem

The transform evaluator sees exactly one step of a chain, so any
terraform whose first step looks unappealing alone never starts —
Mountains are never worked toward Plains even when Plains is exactly
what the city wants. Combined with W28 (which fixed only the
*speed* half): stock never builds long transforms at all
(terraform.py: 0 changed tiles over every measured game).

## Evidence (verified at 0896b34)

`adv_calc_transform()` — `server/advisors/infracache.c:142`:
`new_terrain = old_terrain->transform_result` (one step),
`city_tile_value()` (tile yield only), `struct city *` (city-radius
bound). Chains in civ2civ3: Hills→Plains 2 steps/60wt,
Mountains→Hills→Plains 3/96wt, Jungle 4/108wt; Lake↔Grassland is a
genuine 2-cycle (walkers need a visited set). The transform graph is
functional — each terrain has one successor — so "all-pairs" collapses
to walking one chain per terrain.

## Design

1. Expose the chain walk in advisors: `adv_transform_chain()` (steps +
   cumulative worker-turn costs, visited-set bounded, excludes
   returning to the start terrain) and `adv_transform_cost(from, to)`.
   On-demand walk instead of the PRD's load-time table: chains are
   <= ~5 steps over ~a dozen terrains, so precomputation buys nothing.
2. `adv_calc_transform()` values the transform action as the BEST
   yield reachable along the chain, with the remaining chain time
   beyond the first step amortized (worker-turn points converted at
   the green single-mover rate, points/SINGLE_MOVE — an engine
   constant, not an invented one). When the immediate result is the
   best reachable, the value is identical to stock — one-step
   transforms are untouched.
3. Future-step feasibility (surroundings gates on later steps) is
   assumed optimistically; the walker re-evaluates every turn as
   terrain actually changes, which self-corrects.

No new tuning constants.

## Acceptance (sanity-checked against instruments)

- Multi-step transform starts appear at all: terraform.py by-origin
  counts on isolated/long configs show changed tiles whose original
  terrain was a multi-step start (m/h/f/j) — stock baseline is
  exactly 0 total transforms, so ANY nonzero is the demonstration.
  (Smoke rarely reaches Engineers; it serves as the behaviour guard.)
- Mountains-toward-plains: covered by the same by-origin metric
  (origin 'm' counts), not a bespoke corpus position.
- Score guard on paired seeds, smoke + isolated.

## Commits

```
advisors: expose the terrain transform chain walk
advisors: value transforms against the best reachable terrain
benchmark: break terraform counts down by origin terrain
doc: record W29 results
```
