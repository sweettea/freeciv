# Session brief — take the blade only on the exit that deploys it

The poach ledger (STATUS 2026-08-23, S-POACHPRICE entry) adjudicated
6667 poaches: **76.0% paid for nothing** — `deep_op_poach_defender()`
removes the blade from its operation, then the same pass concludes the
city fails `survival_target` even with it and deploys nobody. The blade
is lost to the wave AND never defends. Only 2.3% of poaches were
needed. The ledger epic sketched the fix seams; this epic ships them.

## The mechanism (gated)

1. **Sequencing fix** (deepdefence.c:3635 area — re-find the exact
   seam, line numbers may have drifted): split "find the blade" from
   "take the blade". Evaluate whether the claim would deploy FIRST;
   remove the unit from its operation only on the exit that actually
   deploys it. The 76% paid-for-nothing exit must leave the wave
   intact. Gate knob in struct tuning (e.g. poach_deploy_gate),
   default off. This is a control-flow change — no new constants.
2. **Marginal-survival pricing** (deepops.c:4392 area), second commit,
   separately gated: the poach today compares TOTAL city risk; the
   garrison draft (deepops.c:3905 area) already prices the marginal
   survival a single blade buys. Reuse that arithmetic so the poach
   claims only when this blade's marginal contribution clears the
   assault's priced share. Reuse, do not copy — visibility exception
   applies only to ai/default, everything here is deep's own code.
   If the reuse does not close cleanly, ship commit 1 alone and write
   the design into STATUS.

## Proof

- Identity 10/10 at defaults (both gates off), incl. one mixed cell.
- **Ledger shift** (the shipped deep-employpoach instrument is the
  acceptance instrument): re-run the poach corpus arm
  (`benchmark/poach-price.sh` ON-arm shape, tech50war-200) with gate 1
  and show paid-for-nothing collapsing and wave membership accumulating
  (baseline: 0.28% of ops ever gain a member; 93.9% of dissolves are
  own-side dismantles). Paste before/after tables.
- **Score A/B**: 16 training seeds, tech50war-200, gate off vs on, same
  pinned binary. This targets the out-of-sample war gap (holdout
  showed deep -134.8/seed vs stock at pv3); a real employment fix
  should move war-venue score. Paste per-seed pairs and the mean.
  Ports 7300-7349 and 7400-7449 are yours, jobs <=4 (the G-tune owns
  7500s until TUNE-DONE appears; do not touch its tree).
- Europe rider: gate(s) on in derby5/tuning-derby7.ini per dedupe
  rules (never append an existing key; awk-dedupe after).

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare before pin; never cp onto a running
binary). Batch drivers at benchmark/*.sh launched detached from the
worktree root as `setsid bash ./benchmark/<script>.sh go` (exact
shape; no cd-prefixed compounds). DONE markers record SUCCESS vs FAIL.
Power-loss sweep on inherited dirs (exact final-turn block count).
Games byte-deterministic. tune-audit CLEAN at the new knob count;
knobcensus for the new gates. One commit one thing (~4 commits:
sequencing, pricing, driver, STATUS+rider); meson test all suites;
STATUS with pasted numbers; standard trailer; do NOT merge — stop
after the STATUS commit on your branch.
