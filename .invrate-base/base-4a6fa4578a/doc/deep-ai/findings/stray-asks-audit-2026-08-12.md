# Stray-asks audit (2026-08-12)

Two user asks with no confirmed landing, checked against the current tree on
`epic/w03-horizon-content`. Read-only audit; no code changed.

---

## Ask 1 — barbarian terror: start before onset, scale with frequency and empire size

**Verdict: LANDED.**

This shipped as a two-commit unit during the M4-adjacent GROUND/derby-2 work,
and is live in the current tree, not reverted:

- `812f43cea7` (doc) — specs the fix from Derby-2 evidence: deep lost 107/207
  cities in 70 turns to an uprising storm a flat, onset-stepped term never
  anticipated. Calls for ramping terror before onset, mirroring the server's
  own willingness formula (`server/barbarian.c:581-613`) for frequency and
  empire-size scaling.
- `3cd7a9b99a` — `ai/deep: shape barbarian terror on the server's own uprising
  formula`. Implements `deep_barb_empire_scale()`
  (`ai/deep/deepdefence.c:1014`), shared by both consumers
  (`deep_barbarian_fear()` at `:1110` and the garrison floor's barb class at
  `:1093`). Four factors, all mirrored from `server/barbarian.c` rather than
  invented:
  1. **Anticipation** — `deep_barb_ramp()` (`:995`) linearly ramps terror over
     `barb_anticipation_turns` (tunable, `deeptuning.h:429`) *before*
     `game.server.onsetbarbarian`, replacing the old flip-at-onset step.
  2. **Frequency** — scales by `(barbarianrate - 1)`, the server's own attempt
     count; zero for `BARBS_DISABLED`/`BARBS_HUTS_ONLY`.
  3. **Willingness (empire size)** — `MIN(1, city_count * (rate-1) / 30)`,
     mirroring the server's `fc_rand(30)+1 <= city_count*(rate-1)` gate
     (`:1032`), further scaled by `EFT_CIVIL_WAR_CHANCE`.
  4. **Horde size** — `1 + uprise(n) * rate` expectation.
- `4993254d5c` (earlier, 2026-07-27) — the original onset-gated flat fear term
  this later replaced (`ai/deep: fear barbarian uprisings from the onset
  turn`).
- `1a30f36019` — separate, complementary ramp on *pre-contact* density fear
  (not the barbarian term itself; explicitly excluded from that ramp because
  barbarian onset "already gates on capability").

Verified live in the current source tree: `ai/deep/deepdefence.c:940-1160`
contains the full `deep_barb_ramp()` / `deep_barb_empire_scale()` /
`deep_barbarian_fear()` chain, with `barbarian_fear` and
`barb_anticipation_turns` registered as tunables in `ai/deep/deeptuning.c:85-86,
340-341` and `ai/deep/deeptuning.h:423,429`. STATUS.md documents a measured
result (`STATUS.md:10046-10162`, "The barbarian re-shape: mechanism yes, score
no"): byte-identity proved in a no-barbarian cell (10/10 scorelogs identical,
0 `deep-barb` lines), and the reshape fires and truncates correctly in a
barbarian-heavy cell, though the *tuning* (the base `barbarian_fear` level,
`barb_size_ref_cities` normalization) remains PLACEHOLDER, unmeasured for
score impact. That untuned-magnitude caveat does not change the verdict: the
mechanism the ask requested — start before onset, scale with frequency and
empire size — is built, wired into both consumers, and live.

---

## Ask 2 — workers bias toward nearby jobs; trunk road value should go up

**Verdict: PARTIAL** (worker-nearby-bias: partial, fallback-only; trunk-road
value: MISSING, unbuilt workstream).

### 2a. "Workers should bias toward nearby jobs"

No commit or doc matches this phrase verbatim; the closest landed work is
narrower than the ask.

- **What exists:** `df9309d5ad` — `ai/deep: give an idle worker the soonest
  legal improvement` (2026-08-03, human call: "worker-workers should always
  do some work rather than stand around"). Implements
  `deep_idle_fallback_work()` (`ai/deep/deepsettler.c:1215`), which — **only
  when a worker was about to go idle with nothing else assigned** — picks the
  soonest-finishing legal road/irrigation/mine inside the player's own city
  radii, ranked by "travel turns plus work turns" (an ordering, not a
  threshold, per rule 7). This is a real nearness bias, but it fires only as
  a last resort, not on the primary demand-driven task selection path.
  Measured INERT on both benchmark cells (`lategame-80`: score -2.20,
  t=-1.04; `primary-120`: score +0.80, t=0.99 — both noise;
  `doc/deep-ai/findings/idle-workers.md:142-163`).
- **What exists but is orthogonal:** `671d7a374f` / `e305b9e1c2` added an
  inert task-value hook to `server/advisors/autoworkers.c` and a deep
  consumer (`deep_worker_task_value()`, `ai/deep/deepsettler.c:2173`) — but
  that consumer only applies a **danger** discount (exposed/frontier tiles),
  not a distance/nearness adjustment.
- **What is untouched:** the primary worker-task valuation loop
  (`worker_evaluate_improvements()` in `server/advisors/autoworkers.c`,
  frozen baseline shape) already folds travel delay into `amortize()`
  (`autoworkers.c:410-421,483-490`), which is *some* implicit nearby bias,
  but this is stock behavior predating the project, not something the ask's
  ai/deep work added, and no ai/deep code strengthens or overrides it for the
  common (non-idle) case.
- **Rule-4 note:** the only thing added to `server/advisors/` for this was
  the inert extension-point hook (`671d7a374f`), which is explicitly
  permitted under rule 4's "extension point that is inert until a consumer
  registers" carve-out. A general nearby-jobs bias on the *primary* path
  would most naturally live as a second consumer on that same hook (weight
  candidates by distance from the assigned worker, gated so baseline stays
  bit-identical when no ai/deep consumer is registered) — so it would not
  collide with rule 4, it would extend the already-approved pattern. Rough
  size: small, 1-2 commits, since the hook and the ranking-by-travel-turns
  logic pattern (`deep_idle_fallback_work`) already exist as a template to
  adapt.

### 2b. "Trunk road value connecting cities should go up"

No implementation anywhere. This is the unbuilt **W12 empire graph**
workstream (`doc/deep-ai/workstreams/LATER-PHASES.md:21`, PRD-v3.md:1002):
"Roads/rail/chokepoints/land bridges/canals as one planner," explicitly
gated on W30 (extension points, landed piecemeal as needed) + W29 (transform
chains, landed: `STATUS.md:540-541`). `git grep` across `ai/deep/` for
`road_plan`, `deep_road`, `trunk` returns nothing; the only `EC_ROAD`
references in `ai/deep/deepsettler.c:941,1076` are the idle-work census from
2a, unrelated to road-network valuation. The phrase "trunk route" appears
only in a findings doc (`doc/deep-ai/findings/better-ai-plan.md:240-242`,
"trunk routes of the MST 3-5x above rail on leaf spurs") sketching what W12
should eventually do — an MST-style road plan that values trunk edges
between cities higher than leaf spurs. That sketch has not been promoted to
a workstream file (W12 is one of the "not yet extracted" entries in
`LATER-PHASES.md`) and has zero commits.

Sketch of where it would go: a new `ai/deep/deepempiregraph.c` (or similar)
building a `road_plan` structure (per PRD-v3.md:477, "recomputable") —
compute a minimum-spanning-tree (or similar) over city locations, tag edges
as trunk vs. spur, and feed trunk-edge tiles a value multiplier into the
existing worker task-value hook (`aw_task_value_hook`) so trunk-connecting
tiles outrank ordinary terrain improvements. This is genuinely medium-large:
it needs W29's transform-chain costing (satisfied) and ideally sits behind
W30's extension-point discipline; PRD-v3 estimates it around the scale of a
full workstream (5-10+ commits: graph construction, MST/chokepoint algorithm
in `common/aicore/` per `where-changes-belong.md:127`, the road_plan struct,
wiring into worker want, and its own benchmark venue). Not started.

---

## Summary

| Ask | Verdict | Evidence |
|---|---|---|
| 1. Barbarian terror ramps pre-onset, scales w/ frequency+empire size | **Landed** | `3cd7a9b99a`, `deep_barb_empire_scale()` at `ai/deep/deepdefence.c:1014` |
| 2a. Workers bias toward nearby jobs | **Partial** | `df9309d5ad`, `deep_idle_fallback_work()` at `ai/deep/deepsettler.c:1215` — fallback path only, measured inert |
| 2b. Trunk road value up | **Missing** | W12 empire graph never built; zero road-plan code in `ai/deep/` |
