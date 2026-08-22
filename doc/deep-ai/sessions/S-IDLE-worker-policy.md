# Session brief — idle-worker policy: reserve, disband, reclaim

The fortress instrumentation (STATUS 2026-08-22, merge f560bf4308)
proved the carpet is what idle workers do when the improvement
backlog empties: price-insensitive, ~11% of productive worker-turns,
denial a non-issue (~0.65 tile-turns/game). The fix is not a fortress
price change — it is a POLICY for post-backlog workers. Read that
entry and the funnel entry (55c8d8e208) first; the fort counters
(worker-turn split by task class) are your primary instruments and
stay live.

## The policy (gated; default OFF = today's carpet, byte-identity)

When a worker's best real task want falls below having-nothing-to-do
(the backlog-empty state the fort trace showed at T127-150):

1. **Reserve**: keep up to R workers per city-cluster idle-in-place
   (fortified near cities is fine — that is free defence, not a
   task). R is a struct tuning knob. Do not guess its value: this
   epic ALSO ships the pollution dwell-time counter (polluted
   tile-turns outstanding before cleanup begins, per city per game)
   and the measurement reports dwell at R=0 and the pollution arrival
   rate, so a later tuning pass has the curve. Ship R's on-arm value
   as the measured mean concurrent pollution events per cluster from
   your own run — a derivation, not a taste choice; show the
   arithmetic in STATUS.
2. **Disband**: surplus idle workers walk to the nearest own city and
   disband into the build queue (half shields, stock rule). Count
   shields recovered.
3. **Reclaim**: any new real demand — pollution, corridor tasks, new
   city's backlog — pulls reserve first, disbanded never. Reclaim is
   just the existing auction seeing reserve workers as available; do
   not build a second dispatcher.

Fortress tasks stay possible where they win on merit today at
knob-off; the gate only removes the by-default carpet path. Do NOT
touch the danger-discount exemption in deep_worker_task_value — that
is a separate epic; changing two mechanisms breaks attribution.

## Proof and A/B

- Byte-identity at defaults 10/10 incl. one mixed cell, vs git
  archive base.
- A/B on the fort venue (contested 8p T150, the instrumentation's
  venue), >=16 mirrored seeds, one key apart (the policy gate; R at
  its derived value). Pre-registered, in confidence order: base share
  of working worker-turns falls from ~11%; disband shields > 0;
  pollution dwell does not worsen; score direction reported whatever
  it is. Fort + dwell counters tabled for both arms.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so, ports 7300-7349, jobs <=4 (W12g shares the box on
7500-7549). Tracked background dies ~60 min; resume off DONE markers;
verify launches by scorelog growth. New knobs: struct tuning +
params/blocks + knobcensus; tune-audit CLEAN at the new count. One
commit one thing; meson test all suites; STATUS with pasted numbers;
stop after the A/B. Europe rider: gate ON in derby5/tuning-derby7.ini
(sed/append per the dedupe rules) — the user plays there and asked
for this fix personally.
