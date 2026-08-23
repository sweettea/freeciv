# Session brief — price sites by what workers can make of them

From the Carthago debrief (QUEUE SITE-IMPROVABLE, STATUS 2026-08-23):
the site evaluator's starve gate and potential term judge tiles AS-IS,
so Carthago stalled at 23 cities logging 118 mindist + 36 starve drops
per turn while its "room" was unimproved desert the user could see was
perfectly colonisable after irrigation. The user's call: "we can
improve crappy sites after we build them with autoworkers tho." Both
blockers are now merged: epic/pox-spacing (deepsite.c is free) and
epic/w29-transform-chains (the chain valuation exists to reuse).

## The mechanism (gated)

1. **Achievable-yield pricing** (deepsite.c): where the evaluator reads
   a tile's current yield, price instead the best yield achievable via
   worker improvement (irrigate/mine/road, and W29's cultivate/plant
   chains where the ruleset allows), discounted by the worker-turns to
   get there. REUSE the W29 chain valuation for what-can-this-tile-
   become and the existing worker-turn arithmetic for the discount —
   no second terrain model, no invented discount rate (derive from
   existing travel/discount knobs or the chain valuation's own
   horizon; show the arithmetic in STATUS).
2. **Starve gate becomes a starve price** (deepsite.c): a site that
   cannot feed itself TODAY but can after N worker-turns of
   improvement gets charged for the wait instead of being dropped.
   Keep a hard drop only where no improvement chain ever feeds it.
3. Gate + any knobs in struct tuning; on-values derived or PLACEHOLDER
   flagged in STATUS for the tuning pass.

## Proof

- Identity 10/10 at defaults (gate off), incl. one mixed cell.
- Liveness: on the user's game corpus shape (or a scripted map with
  desert belts), show sites previously dropped by starve/mindist now
  scored and founded — the site ladder log is the instrument; paste
  before/after drop-cause counts (baseline: 118 mindist + 36 starve
  per turn at the stall).
- Score A/B: 16 training seeds, isolated-150 (the expansion venue),
  gate off vs on, one pinned binary. Paste per-seed pairs and mean.
- Europe rider: gate on in derby5/tuning-derby7.ini per dedupe rules.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare before pin). Ports 7600-7649, jobs <=3
(poach-fix owns 7300s+7400s, G-tune owns 7500s, rail epic owns
7650s). Batch drivers at benchmark/*.sh launched detached from the
worktree root as `setsid bash ./benchmark/<script>.sh go` (exact
shape). DONE markers record SUCCESS. Power-loss sweep on inherited
dirs. Never append an existing ini key; awk-dedupe. tune-audit CLEAN
at the new count; knobcensus. One commit one thing; meson test all
suites; STATUS with pasted numbers; standard trailer; do NOT merge —
stop after the STATUS commit on your branch.
