# Session brief — W29: transform chains (+ the stacking payoff)

W29 is the second pre-approved server/advisors workstream (CLAUDE.md
rule 4). Read doc/deep-ai/workstreams/W29-transform-chains.md end to
end FIRST and question it where the code has moved (W28's brief was
stale — its autoworkers refusal was already lifted; check every line
reference). New context the workstream predates: W28's second half
(branch epic/w28-stacking, merging soon — base on current tip and
coordinate through the orchestrator if you need its counters) proved
stacking never pays on 3.5-turn road tiles and pays exactly on long
jobs: `late` = 65% of join refusals because the job ends before the
joiner arrives. Transforms ARE the long jobs (36-96 worker-turns).
This epic makes deep plan them; the stacking gate then has
something to join.

## Scope

1. Whatever W29's design says, honoring rule 4's boundary: defects
   harming humans equally may be fixed in advisors; deep-specific
   valuation goes in ai/deep. Gate everything; defaults byte-identical
   including a stock-only cell (the W28 proof pattern).
2. The chain valuation must price the WHOLE chain (transform A->B
   then improve), not the first step alone — greedy one-step
   evaluation is the documented root cause (RC1). Reuse existing
   want machinery; new constants are struct tuning PLACEHOLDERs.
3. Counters per the funnel doctrine: chains planned/started/
   completed/abandoned-by-cause, worker-turns by chain stage,
   stacked-turns on transform tiles (W28's ledger already counts
   these — keep it live).

## Proof

- Identity 10/10 x three cells (deep, mixed, stock-only) vs
  git-archive base.
- Liveness: a venue where transforms actually happen — check what
  terrain/era corridor-150 and garden2b-150 offer; if no benchmark
  venue produces transform demand, prove liveness on a scripted
  smoke (engineer + transformable terrain) and SAY the score A/B is
  deferred, like W12c did. Do not bury a null from a venue that
  cannot exercise the mechanism.
- Europe rider: gates on in derby5/tuning-derby7.ini per dedupe
  rules (the user's long games are where terraforming lives).

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare). Ports 7650-7699, jobs <=2 while the
sibling A/Bs drain (tierfix 7400s, W28 7500s, employment 7300s) —
check their scorelog mtimes before batches. Batch drivers at
benchmark/*.sh launched detached (setsid, from repo root — the
sandbox exclusion covers that path). Power-loss sweep on any DONE
dirs you inherit. Disk ~6G: prune hard. tune-audit CLEAN at the new
count; knobcensus; one commit one thing; meson test all suites;
STATUS with pasted numbers; stop after liveness (or the A/B if a
venue supports it).
