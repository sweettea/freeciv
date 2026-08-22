# Session brief — why deep's army stands idle: employment counters

The war-gap decomposition (STATUS 2026-08-22, merge 4f62416b87)
localised the deficit's leading edge: at T60 deep has 1.06x the
military alive and converts it to kills at 0.541 vs classic's 0.669
(t −6.83); kill:loss falls 0.94→0.65 across the game while classic
rises to 1.08. Deep picks fewer fights, then starts losing them. The
capacity (−6%) and allocation (−13%) legs are measured; the
employment leg (−33%) has no counters. This epic builds them and runs
the specified probe. **Diagnosis only — no behaviour changes, no new
tunables** (counters only; tune-audit count unchanged).

## Counters (log-only), at the real decision sites

Read deepcombat.c, deepops.c, deepdefence.c first and instrument the
seams that exist, not a parallel model:

1. **Disposition census** per player-turn: military units by state —
   garrisoned, in a forming op, in a launched op, in transit,
   fortified idle, unassigned idle. The −33% employment claim needs
   this denominator.
2. **Attack-opportunity funnel**: targets evaluated, priced above
   zero, declined — by cause at the decline site (odds below floor,
   tired_attack_floor, pinned by threat, deterrence, no path, muster
   wait, op quota) — and attacks actually launched. Mirror the
   worker-funnel design (deepfunnel.c) that worked.
3. **Op lifecycle**: ops formed, muster duration, launched, aborted
   by cause (op_abort_floor, target gone, odds fell), and per-op
   kills/losses on resolution.
4. **The wait ledger**: for units that spent the turn idle-with-
   orders, which gate held them (op_muster_wait, op_wave_window,
   survival floors). The suspicion from the ranking is that deep's
   own caution gates — odds floors, muster waits, pin discounts —
   compound into standing armies; the counters must be able to
   separate "no target worth hitting" from "target priced but
   vetoed" from "waiting for a wave that never fills".

## Proof and probe

- Counters observe, never steer: scorelog byte-identity 10/10 vs
  base on a short mixed config.
- The probe the decomposition specified: tech50war-200 MIXED
  (deep vs classic), seeds 17-36, 40 games, ports 7300-7349,
  jobs <=4, one pinned binary. Report the funnel and disposition
  tables against the kill:loss trajectory (per-side kills/losses at
  T40/60/80/100), and name the dominant employment leak with
  numbers — same deliverable shape as the worker funnel: per-stage
  counts, attrition, causes ranked.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (pin() compares shas — never cp onto a running
binary), tracked background dies ~60 min (DONE-marker resume),
verify launches by scorelog growth. Games are byte-deterministic in
(config, seed, module, tuning): n is the seed count. One commit one
thing; meson test all suites; STATUS entry with the tables; stop
after the probe. Commit on branch epic/employ-military-idle in your
OWN worktree — do not commit on or check out branches in the main
tree.
