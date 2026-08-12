# Session brief — ERA 3b: MUSTER, or the odds test becomes a launch gate

Successor to 3a, whose falsification is the design input. Prerequisite
reading: findings/era-probe-evidence-2026-08-12.md (the odds histogram
and pool table), the 3a commit 35afc4c2b5 on epic/era-3a-massing (the
marginal test and why its shape is wrong), findings/
tech50-collapse-2026-08-10.md.

## What 3a proved

The take-odds curve is flat at zero until pool ~6-8 (P(odds>0) at
pool 1 is 5.4%; mean odds at pool 8 is still 0.065 against a 0.65
bar). Any decision that asks each blade to justify itself
incrementally — floor or marginal, doesn't matter — cannot bootstrap
across that flat region: a SET of eight blades is valuable while every
increment is worthless. Formation is also memoryless (0.88 attempts
per player-turn, each asking "can I win NOW"), so the pool never
accumulates across turns either. Two independent reasons nothing
masses.

## The doctrine (human, 2026-08-12)

"At gunpowder era it's generally insane to do attacks." The odds model
is honest; deep's refusal to attack is CORRECT at parity. The venue is
lost on defense: deep barely attacked and was still out-killed 2:1 —
it died defending and lost its expansion piecemeal. In a
defense-dominant era the kill credit flows to whoever makes the other
side attack. Therefore:

- MUSTER exists to make the RARE sane attack possible (decisive force
  against weak/overextended targets), not to raise attack frequency.
- SUCCESS METRICS for the A/B are kill-exchange ratio, cities held,
  and score. NOT forms/strikes counts — those may legitimately stay
  low or even fall. 3a's pre-registered "forms rise, strikes rise"
  scorecard is explicitly retired for 3b.

## The mechanism

A formation slot that clears a WORTH gate (target value, from numbers
already computed by op targeting) may enter MUSTER instead of
dissolving on the odds test:

1. While in MUSTER the slot accumulates: units that join are committed
   to the muster point (rotation/rehoming machinery already moves
   units; reuse it), and the pool persists across turns instead of
   re-forming from scratch.
2. Each turn the slot re-evaluates: launch iff take-odds at the
   CURRENT pool clears the bar (op_take_target unchanged — the odds
   test becomes the launch gate it already pretends to be).
3. Abandon iff: target's worth collapses, the muster stalls (no growth
   for N turns where N comes from an observed cadence, not a picked
   constant — flag as PLACEHOLDER if no honest source exists), or the
   defense demand at home spikes (the garrison draft's cost side
   already prices this).
4. The 3a marginal release feeds MUSTER rather than instant
   formation: a blade whose marginal cost is ~0 joins the muster even
   when its marginal gain is 0 — the flat-region problem dissolves
   because the muster's value is evaluated at the SET level at launch
   time, not per increment.

## Guards

- Default OFF, byte-identity 10/10 at the compiled default.
- Mustered units are not free: they still garrison where they stand
  (stack survival already counts present units), and the disarmament
  lens from NETTHREAT applies — A/B must report cities-lost alongside
  any offense gained.
- No double-spend with deterrence/sortie prizes (M4b's bookkeeping
  rule).
- The launch-rate reality check (CONSEQ N3): report muster→launch and
  muster→abandon rates; a mechanism that musters forever and never
  launches is a unit sink, and the counters must make that visible.
- Venue: tech50war-200 mixed, ≥16 mirrored seeds (n=8 gave SEM 81).
- Attribution: 3b lands AFTER the 3a×3c factorial reads out, or as an
  arm of it — 3c's reach correction deflates the wave, moves the
  draft's cost side, and changes MUSTER's home-defense abandon
  trigger. One causal chain, one design (CLAUDE.md: parallelise
  scheduling, not attribution).

## Deferred-not-dead ledger (human, 2026-08-12)

Recorded so they are not re-litigated: the odds bar (no gradient while
84.5% of refusals sit at 0.000 — unblocks after massing moves), the
expansion gate never-block arm (knob landing separately; factorial
candidate), the sea clause (needs NAVAL-VENUE, task #57).
