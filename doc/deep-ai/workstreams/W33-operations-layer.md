# W33 — Operations layer

**Phase 3, ai/deep. BUILD FIRST — the spine W31/W32/W35 attach to. W13 folds in.**
Root cause: RC2 escalated — the AI has no representation of a multi-turn operation.
Full reasoning + line refs: `findings/operations-layer.md`.

## The problem in one look
- `struct ai_invasion {int attack; int occupy;}` (`daicity.h:29`) is zeroed every
  turn at `daiunit.c:1302` and rebuilt by scanning which units already point at a
  city. A per-turn census of emergent drift, not a plan.
- `AIUNIT_ATTACK` (`daiunit.h:27`) is a role a unit re-decides every turn via
  `find_something_to_kill`, not a plan it belongs to.
- So nothing persists, sequences, or commits across turns.

## Design
A persistent, phased, committed operation that owns its units:
`{ type, phase, objective, committed[], need, staging, plan_horizon,
   expected_value, sunk, stall_turns }`.
Three things it buys: **commitment** (committed units aren't re-tasked by the
per-unit loop), **phase sequencing** (advance only when a phase precondition is
met), **cross-turn value + deliberate abort** (amortize objective over horizon;
abort on stall/negative-EV/objective-changed, banking survivors).

Per-turn loop becomes: review ops → issue committed units' phase orders → THEN run
the ordinary per-unit loop for UNCOMMITTED units only. Opportunism survives, demoted
to what idle units do. (This split is also W20's serial/parallel boundary.)

## Acceptance
- Committed fleet ignores an opportunistic target (L1 corpus).
- Amphibious op gates EXECUTE on bombardment (op log order).
- Op started turn T still coherent at T+5 (L2).
- Stalled/negative op aborts and withdraws survivors (L1 — the current AI's most
  conspicuous failure).
- Uncommitted units still behave opportunistically.
- Metric: multi-turn operations completed per game vs baseline ≈0.

## Commits
See `findings/operations-layer.md` §8 (12 impl commits + 4 tests). Key ordering:
struct+lifecycle → commit/exempt → phase-advance+value+abort → land-assault phase
list (absorbs W13) → run-before-opportunistic-loop.

## Dependencies
Needs W04, W10/W11, W30 (campaign_hint pin). Must precede coordinated W31/W32/W35.
