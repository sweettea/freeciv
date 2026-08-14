# W02 — Site evaluation

**Phase 2, after W01. Extracted from PRD-v3 Part VI (W2) +
findings/better-ai-plan.md §1 on 2026-07-25.**
Root cause: RC1 (greedy one-step evaluation). Placement: `ai/deep`
(the PRD's commit titles predate the placement revision).

## Problem

A city site is scored on **two tiles**. The AI builds out instead of
up because a 20-tile catchment is valued as ~4.25 tile-equivalents.

## Evidence

`cityresult_fill()` — `daisettler.c` (verified at 0896b34):

- Per candidate tile: `sum = food*food_priority + trade*science_priority
  + shield*shield_priority`, `*= PERFECTION/2`, and `*= 2` if
  `food >= 2` — the grassland-seeking missile.
- Exactly two tiles kept at full weight (city centre + best other);
  everything else lands in `remaining / GROWTH_POTENTIAL_DEEMPHASIS`
  (= 8). The code confesses at the `best_other` selection:
  *"Baseline is best extra tile only. This is why making new cities is
  so darn good."*
- Consequences: specials-rich sites lose to wet flatland; shield/trade
  specials systematically under-weighted; long-run potential (post-
  improvement yields) invisible.
- `contemplate_new_city()` (founder_want producer) uses the same
  evaluator through find_best_city_placement — production decisions
  and settler routing share the disease (noted in W05's STATUS).

## Design

Deep derivatives of `city_desirability()` / `cityresult_fill()` (and a
`settler_map_iterate` / `contemplate_new_city` chain that calls them),
per the PRD:

1. Score **all** workable tiles, sorted descending, weighted by the
   probability the city ever works them:
   `w[i] = i < target_size ? 1.0 : tail_weight` (tail_weight a tuning
   placeholder, PRD suggests 0.15).
2. `target_size` derived from cumulative food surplus of the sorted
   tiles (no constant — the site's own food determines how many tiles
   it can ever man).
3. Retire `GROWTH_POTENTIAL_DEEMPHASIS` and the `food >= 2` doubling —
   both replaced by the weighting scheme.
4. Explicit specials term under `adv->goal.govt.gov` (already loaded
   for the search), with a cluster bonus (tuning placeholders).
5. Potential term: post-improvement tile yield via speculative
   `adv_calc_extra()`, discounted by build time.
6. Marginal-city diminishing returns: decay founder want as city count
   rises against available land / frontier waste (tuning-shaped).

Items 4-6 may land as follow-up commits after the core weighting
proves itself; the founder-want *scale* conversion stays gated on W03
per the W01 conversion-gating map.

## Acceptance

- Mean city size at T150 **up ≥40%** (extract.py mean_city_size,
  contested config; plan-time number — re-baseline against the first
  measurement per project practice).
- City count down (same source).
- Score guard: no regression on paired seeds.
- The AI stops founding when the marginal city's discounted output
  falls below the marginal improvement in its best city (directional;
  fully checkable only after W03 makes improvement values honest).

## Commits (adapted to ai/deep placement)

```
ai/deep: score all workable tiles instead of centre plus best other
ai/deep: weight catchment tiles by probability the city works them
ai/deep: derive target city size from cumulative food surplus
ai/deep: retire the growth-potential deemphasis divisor
ai/deep: add an explicit resource-special term to site scoring
ai/deep: value post-improvement tile yield discounted by build time
ai/deep: decay founder want as city count rises against available land
test: add metric — mean city size and city count at turn 150
```
