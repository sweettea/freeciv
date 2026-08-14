# W03 — Horizon-aware improvements and co-investment

**Phase 2, after W02. Extracted from PRD-v3 Part VI (W3) on
2026-07-25.** Root causes: RC1 (zero time horizon), plus the
co-investment deadlock. Placement: `ai/deep`.

## Problem

(a) Improvement value is measured at today's city size, so buildings
whose entire value is future growth — aqueduct, granary, harbour,
sewer — score ~0 and are never built on merit. (b) Co-investment
deadlock: the civ2civ3 farmland effect requires Building Supermarket
(city range) AND Extra Farmland (tile range); each side measures the
other's absence as exactly zero. Horizon projection alone cannot fix
(b) — zero times a horizon is zero.

## Evidence

- `base_want()` / `dai_city_want()` (daicity.c) sum output over
  currently worked tiles at current size, this turn. README.AI's own
  TODO: "Fix the AI valuation of supermarket. (It currently never
  builds it)."
- Confirmed live in the W01 calibration corpus: building picks median
  10-48 while multiplier buildings (marketplace/library) dominate the
  building population — capacity buildings appear only via the
  building_advisor's non-want paths, if at all.

## Design (deep post-adjustment pass)

Stock building wants keep flowing (dai hooks untouched); deep adjusts
`pcity->server.adv->building_want[]` after building_advisor() and
before the choose-build loops in deep_manage_cities():

(a) **Capacity credit.** For improvements providing Size_Adj /
Size_Unlimit while the city cannot grow past its cap: credit the
blocked growth curve — steps creditable over the planning horizon
(H = MORT, the documented amortize horizon; growth speed from
city_turns_to_grow()) times the mean value of the best unworked
catchment tiles (same weighted-output currency as W02), amortized.
Growth-accelerators (Growth_Food) get the same credit shaped by the
granary-refill saving. Shapes carry tuning placeholders.

(b) **Bundle credit.** Scan all effects for requirement vectors
containing BOTH a Building (city range) and an Extra (tile range) —
mechanical, ruleset-general, no farmland special-case. If the city
could build the building and the catchment holds tiles that could
carry the extra, credit the building side with the joint effect value
across viable tiles (amortized). Worker-side pairing (start tile work
at ~60% completion, W30 tagging) is deferred to the W30 extension
points — recorded, not silently dropped.

## Acceptance

- Turn of first aqueduct/harbour/granary/sewer moves earlier
  (deep-want pick lines, contested T150).
- **Supermarket gets built at all** (stock: never) — closes the
  README bug.
- Score guard on paired seeds; no starvation regression.
- Farmland-before-supermarket scheduling: deferred with the
  worker-side pairing (W30).

## Commits

```
ai/deep: credit capacity-gating buildings for unlocked growth
ai/deep: credit growth-accelerating buildings for faster refill
ai/deep: detect co-investment bundles from ruleset effect requirements
ai/deep: credit bundle buildings for the joint effect value
test: add metric — first-build turns for capacity buildings
doc: record W03 results
```
