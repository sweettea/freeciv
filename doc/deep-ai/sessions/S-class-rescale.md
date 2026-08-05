# Session brief — one scale for what a city can do

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md` (RC4 is this session's disease),
`NEXT-SESSION.md` §5, the W01 sections of STATUS.md (calibration corpus +
conversion-gating map), `workstreams/W01-want-unification.md`, and
`ai/deep/deepwant.c` (the merge seam and the calibration log line).

## The ask (human, 2026-08-04)

"Adjust the tuning for the different things a city can do — building,
settling, trade routing, worker-making, military-making — so they're at
least vaguely in the same scale," valued as amortized benefit over the
horizon. This is the pragmatic form of W01's unfinished conversion program:
per-class deflators at the merge seam, MEASURED from a fresh calibration
corpus, applied behind a switch, refined later by the cloud pile.

## Why now, and the one prior failure that shapes everything

W01 quantified the scale disease (July corpus, 45k decisions): buildings
median 10-48, workers 47, defenders 155 (p90 1224), settlers 311
(trait-rigged), ferries max 63505. The founder conversion was gated then
because the building anchor was dishonest — W02 v1 deflated founders onto
it and score cratered (contested 0.879, t=-4.75). Since then the anchor
gained horizon credits, gold-upkeep charging and the growth ladder — the
gate is now plausibly open, and THIS SESSION IS THE TEST of that.

## THE JOB

1. **Re-measure the corpus on today's binary.** The July ratios are dead —
   horizon credits, upkeep, the bug round and rotation all moved want
   magnitudes. Re-run the W01 calibration capture (the deep-want line per
   final build decision) across ~10 contested + ~10 isolated-150 games;
   produce the per-producer-class scale table (median/p90 per class) and
   commit it to `findings/` beside the July one, dated.
2. **Class deflators at the merge seam.** In `deepwant.c`'s merge, a
   multiplicative scale per producer class (settler, worker, defender,
   attacker, ferry, caravan, building-anchor stays 1.0). `struct tuning`
   keys (`scale_settler`, `scale_worker`, ...), compiled defaults 1.0 =
   today's behaviour byte-identical; corpus-derived values shipped in a
   tuning FILE (the same delivery W01 used for diplomat_defence_value).
   Emergency tier is exempt — a grave-danger defender must still win
   (the tier system already encodes this; do not let a deflator leak
   across tiers).
3. **Ferry/attacker spikes get a cap, not a calibration.** The 63505-class
   wants are kill_something_with outputs gated on the combat model (W10
   lineage); a corpus-derived deflator for them is defensible only as a
   clamp to the emergency-tier boundary. Say exactly what was done.
4. **Measure like it could fail, because it did once.** The W02-v1 tell
   was expansion collapse. Cells: isolated-150 + contested, 20 paired
   seeds direction, then 40 on whichever cell moved for any claim. Guard
   metrics BEFORE score: foundings/100t, cities@end, settler count curve,
   first-build distribution (firstbuild.py), worker count. Smoke guard.
   If expansion holds and score is neutral-or-up: the gate W01 named is
   open, and the finding is worth more than the score move.
5. **Explorer re-pricing rides along** (W01's map: it follows founder
   deflation, not self-inflation) — check the explore want's standing in
   the rescaled auction on a huts cell; adjust its scale key only if the
   corpus says so.

## Rule 7 posture

Every scale is a tunable with default 1.0; the shipped values are
corpus-DERIVED (measured medians, method written down next to the numbers),
not invented; all flagged for the cloud pile, which owns refinement. This is
the W01-diplomat precedent applied per class.

## Acceptance

- Fresh corpus table committed with capture method.
- Defaults byte-identical (10 smoke + 10 contested); counters prove the
  scales engage under the tuning file.
- The A/B at the powers above with guard metrics reported before score.
- `meson test` passes; `tune-audit.py` CLEAN at its new count.
- STATUS.md dated section as final commit, including a fold/no-fold
  recommendation per class — the human owns the fold decision.

## Scope fences

One epic (6-9 commits). No per-mechanism conversions (attacker/combat stays
gated on combat E2), no rates/trade changes (their own sessions land first
and this session's corpus is taken AFTER them). `ai/default`/`ai/baseline`
untouched. Leave NEXT-SESSION.md and sessions/ alone.

## Stop conditions

Expansion collapses at corpus-derived scales (report the per-class
attribution — which deflator did it — do not hand-adjust past it);
the corpus shows the July disease has materially healed on its own
(possible after horizon+upkeep; then the finding is "rescale unnecessary"
and the session ends early, honestly).
