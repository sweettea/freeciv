# Session brief — DECAY slice 2: the scheduled exponential, all three rectangles, bell-truncated

Human go (2026-08-14): "let's go on and do all the rectangles. let's do
the rate schedule." Slice 1's census (STATUS 2026-08-14, merged
c393f0f3e1) proved the knob is load-bearing — 28.1% of domestic
argmaxes flip at the measured late rate, 5.4% floor at 1/MORT — and
forced three design constraints this slice MUST honor:

1. **Bell truncation ships now, not later.** Untruncated exponentials
   double the low-rate flip share (56.5% at 0.002) with phantom value
   past the scored end of game. The duration sum is truncated at the
   scored bell from day one (slice 4's clamp folds in here).
2. **The rate is per-phase from the start.** The early game is the
   most rate-sensitive phase and the measured world rate collapses
   ~20× between T1-50 and T101+. A single rate would be wrong in one
   phase or the other by construction.
3. **All three rectangles.** The census found the same MORT-24
   rectangle in two more auctions: deepgov.c:536 and
   deepdefence.c:7092. One discount function, applied at all three
   sites through ONE shared helper in ai/deep — no per-site drift.

## Knobs (names are FIXED — the orchestrator writes a derby tuning
## file against them; do not rename)

- `horizon_decay_gate` (int, default 0 = byte-identity: every site
  keeps the legacy rectangle exactly).
- `horizon_decay_early` (double, PLACEHOLDER 0) — per-turn rate for
  turns <= `horizon_decay_break_start`.
- `horizon_decay_late` (double, PLACEHOLDER 0) — per-turn rate for
  turns >= `horizon_decay_break_end`.
- `horizon_decay_break_start` (int, PLACEHOLDER 50) and
  `horizon_decay_break_end` (int, PLACEHOLDER 100) — the transition
  band; between them the rate interpolates linearly in turn number.
  The 50/100 placeholders are the census's measured phase boundaries,
  not invented values; say so in the registration comment.

When the gate is on: a duration credit of per-turn value b becomes
sum over t of b·(1−r(turn))^t, truncated at the scored bell
(game end / endturn as the endgame machinery already knows it), where
r(turn) is the scheduled rate at the CURRENT turn. Delay discounting
(deep_amortize) is already exponential and is NOT this slice's
business — do not touch it, and do not double-discount duration by
routing it through amortize.

## Scope and rules

- The shared helper lives in ai/deep (deephorizon.c or a small new
  home); deephorizon's rectangle, deepgov.c:536's and
  deepdefence.c:7092's rectangles all call it. NEVER touch
  advtools.h's MORT or anything in ai/default or server/advisors.
- Counters: the slice-1 census instrument already logs stream shapes;
  add the applied rate and truncated-vs-rectangle value to the
  existing `deep-decay` lines (or a sibling line) at each of the
  three sites so a knob-on game shows the discount actually applied
  per site. A site whose counter never fires knob-on is a miss —
  report it, don't ship it.
- Byte-identity 10/10 at defaults including one mixed cell.
- meson test 5/5; register all five knobs in struct tuning +
  params/blocks; tune-audit CLEAN (expect 242); knobcensus entries.

## A/B

Venue tech50war-200 mixed, >=16 mirrored seeds, ONE binary pinned
WITH libfreeciv.so. On-arm: gate 1 with the MEASURED anchors —
early 0.037, late 0.002, break 50/100 (document the derivation from
the slice-1 anchor table; measured, not invented). Control: compiled
defaults. Report paired score diff, per-seed table, and the era-slide
checkpoints (the lean-lattice analysis format: deep share of techs /
mfg / cities at T50/T100/T150/T200) — this knob's whole thesis is the
late-game window, so the checkpoint trajectory matters as much as the
final score. Flag both rates for a proper tuning pass in STATUS.

## Not this slice

No live-measured rate (slice 3 stays queued — the human chose the
schedule first). No changes to the pin tracker, amortize, or any
ai/default code. No tuning sweep — one measured-anchor arm only.

Usual rules: one commit one thing, off-switch proof, pasted numbers,
STATUS update on your branch, stop after the A/B.
