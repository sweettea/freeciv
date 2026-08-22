# Session brief — pv4 candidate: pv3 minus the guilty GCM block

The autopsy (STATUS 2026-08-22, merge 44b089ef21) blamed pv3's
econ-venue deficit on G and specifically GCM — the 17 cm_* citizen
manager weights (−24.2 of the −46.5, 12/16 seeds negative, t −1.80
at n=16 on TRAINING seeds). GDIPLO was a weak second (−3.0, 0/8).
This epic is the confirmatory read on HELD-OUT seeds. No code
changes; ini generation + runs + verdict only.

## Arms

Generate by script — reuse the autopsy's committed generator
(benchmark/, commit 5155faa594) and the G sub-block definitions in
tuning-blocks.ini (9960d99a28); verbatim line copies, awk-dedupe,
validate_tuning, and read-back audit one game per arm:

1. pv3.ini (control)
2. pv4 = pv3 minus GCM keys (compiled defaults for those 17)
3. pv4d = pv3 minus GCM minus GDIPLO (the second candidate stacked)

## Runs

- tune.py --report-holdout for each arm on BOTH venues
  (isolated-150 and tech50war-200), --seeds benchmark/seeds/
  holdout.txt --count 16. That is 6 report runs; the per-venue
  baseline arm is shared by cache within each out dir — keep one out
  dir per venue so it is computed once, not three times.
- Per-seed paired diffs between arms (the baseline cancels), sign
  counts, and medians alongside means — the autopsy showed single
  seeds can carry a mean.

## Pre-registered decision rule (report, do not crown)

pv4 confirms if it beats pv3 on the econ venue holdout AND does not
regress the war venue beyond noise. If pv4d beats pv4 on econ
without a war regression, say so separately. The orchestrator
adjudicates crowning; your deliverable is the table.

## Guards

Own worktree + build (explicit -Daimodules=classic,tex,baseline,deep
— a bare meson setup silently omits ai/deep), own pinned binary WITH
libfreeciv.so, ports 7300-7349, jobs <=4 (one sibling epic shares the
box). Tracked-background Bash dies at ~60 min here — structure runs
to resume off DONE markers and relaunch. STATUS entry with the full
table; one commit one thing.
