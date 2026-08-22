# Session brief — a venue that can resolve corridor effects

Three W12 slices (trunk roads, beyond-ceiling sourcing, ally
endpoints, trade dividend — STATUS entries for all) have returned
score NULLs on lategame-80 with the mechanism proven live each time
(counters strongly significant, score se ~1.8 over 16 seeds). Before
any fifth corridor term (a military dividend is queued), we need a
venue where a corridor effect of a few score points is resolvable at
feasible n — or evidence that no reasonable venue resolves it.

## The epic

1. Read the three W12 STATUS entries and the configs in
   benchmark/configs/. Diagnose WHY lategame-80 is corridor-hostile
   (small map? short horizon? gap-1 ally borders? roads already
   saturated by autoworkers?) — pull the numbers from the retained
   W12 result dirs (benchmark/results/w12*) rather than guessing:
   corridor lengths, ally-link gaps, trunk-picks variance.
2. Design ONE candidate config (e.g. corridor-150): larger land-
   connected map, horizon >=150 turns, geometry that makes corridors
   long, alliances still forming (lategame-80 proved deep-deep
   alliances happen; preserve whatever makes that true). Start from
   an existing config file; document every changed setting and why.
3. Pilot: >=8 mirrored seeds, W12 family ON (trunk_road_gain,
   trunk_source_gain, trunk_ally_gate, trunk_trade_gain at their
   probed on-values from the merged A/B inis) vs all-off, one binary.
   Report: counter levels (are corridors long/numerous here?), score
   se, and the implied n to resolve a +3 score effect at t>=2.
4. Verdict: either "this venue is powered at n=N" (and the factorial
   W12 read runs there later — do NOT run it in this session), or
   "corridor effects are below X at any feasible n" with the pilot
   numbers. Both are good outcomes; do not torture the config hunting
   a positive.

## Guards

No ai/ code changes at all — configs, scripts, docs only. Own
worktree + build (explicit -Daimodules=classic,tex,baseline,deep),
own pinned binary WITH libfreeciv.so, ports 7500-7549, jobs <=4 (one
sibling epic shares the box). Longer games cost real wall time —
pilot before committing to big n, and prune saves once scored.
Tracked-background Bash dies at ~60 min; resume off DONE markers.
STATUS entry with pasted numbers; one commit one thing.
