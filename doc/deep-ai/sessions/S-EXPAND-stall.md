# Session brief — the expansion stall: why 3.7 settlers vs 39.2

The war-gap decomposition (STATUS 2026-08-22, merge 4f62416b87)
rank-3 finding: on tech50war-200 mixed games, deep builds 3.7
settlers to classic's 39.2 by T60 (t −9.76), cities −3.2 (t −3.64).
An order of magnitude, causally upstream of the capacity leg of the
T60 chain. This epic is DIAGNOSIS ONLY: find where founder demand
dies. No fixes, no new tunables.

## Prior art you must not re-derive

- The autopsy's X-family finding (settler/site blocks are the ONE
  family that beats defaults, +22.6 econ) — the stall is unlikely to
  be the site evaluator's quality; suspect the gates around it.
- tuning-blocks.ini's X comment: "T76 showed deep's founder want
  firing in 12% of cities against stock's 41%: risk-priced travel is
  a prime suspect for suppressing it."
- Known gates in the founder chain: expansion_gate_enabled,
  expansion_war_survival, settler_fearfulness_mult, cm_founder_food,
  founder_pool_per_site, site_enough_result, scale_settler. Read
  deepsettler.c's founder path end to end before instrumenting.

## The funnel (log-only counters, same doctrine as deepfunnel.c)

Per city per turn on the founder path: sites known and priced above
zero -> founder want computed -> want survives the war/survival
gates -> want wins the build auction -> settler built -> settler
survives to found. Count each stage and each gate's veto separately
(which gate, at the site where it fires). The 12%-vs-41% probe
suggests the die-off is early; prove where.

## Proof and probe

- Counters observe, never steer: scorelog byte-identity 10/10 vs
  git-archive base on a short mixed config.
- Probe: tech50war-200 mixed, 20 seeds (seeds 37-56 to stay disjoint
  from the employment probe's 17-36), ~20 games. CPU STAGGER: the
  box carries two live epics (7300s employment probe, 7500s
  soft-borders). Build and prove identity FIRST; launch the probe
  only when a DONE-count check of benchmark result dirs under
  .worktrees/employ shows its 40-game probe finished, or scorelog
  mtimes there are >20 min stale. Ports 7400-7449, jobs <=4.
- Deliverable: STATUS entry with the funnel table (stage counts,
  gate vetoes ranked), the divergence turn (when deep's settler
  production falls behind), and the dominant-gate diagnosis with
  numbers. If the funnel shows demand healthy and the loss is in the
  build auction, say what outbids settlers and by how much.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare), tracked background dies ~60 min
(DONE-marker resume), verify launches by scorelog growth. Games are
byte-deterministic; n = seeds. tune-audit unchanged. One commit one
thing; meson test all suites; stop after the STATUS commit.
