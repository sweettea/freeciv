# Session brief — W12 funnel: where 255 picks become 14 roads

The corridor-150 pilot (STATUS 2026-08-22, merge 85a1c0c818) measured
255 corridor task picks per game buying +14 net roaded tiles map-wide,
alongside techs t −3.18 — the family pays the opportunity cost and
does not collect the road. Slice 3 saw the small version (one seed's
29 picks were 2 tiles re-picked). Before ANY further corridor term,
instrument the funnel. This epic is counters + one pilot-scale run.
**No behaviour change anywhere.**

## The funnel to instrument

Per player per game, stages of one corridor task's life:

1. picks — task assigned to a worker (exists: trunk-picks)
2. distinct tiles ever picked (dedup — is 255 mostly re-picks?)
3. worker-turns en route to a corridor task
4. arrivals — worker reaches the tile and starts the activity
5. activity-turns accumulated vs abandoned mid-build (preempted by
   re-evaluation, danger flee, task switch — count WHY, by cause,
   at the site where the task is dropped)
6. completions — road actually finished by a corridor-tasked worker
7. net roads on former corridor tiles built by OTHERS (autoworkers
   finishing what deep walked away from)

The suspected disease is churn: per-turn re-evaluation reassigning
the worker before arrival or mid-activity. If so, stage 3/5 will
dwarf stage 6 and the cause counter will say which preemption path.
Read how slice 1's locality assignment interacts with per-turn
re-evaluation in deepsettler.c/deeptrunk.c before writing counters —
instrument the actual decision sites, not a parallel bookkeeping
guess.

## Proof and run

- Counters must not perturb play: scorelog byte-identity 10/10 vs
  base on a short config, family ON in both arms (this is an on-path
  identity — the counters observe, never steer).
- One measurement: corridor-150, family ON (the pilot's exact on
  values, benchmark/tuning/ w12 inis), 8 seeds, reusing
  benchmark/venue-corridor.sh machinery. Score is not the deliverable
  — the funnel table is. If the pilot's cached ON games can be reused
  for any stage (e.g. stage 2/7 from savegames), reuse rather than
  re-run; stages needing new counters need the new binary.
- Deliverable: STATUS entry with the funnel table (counts per stage,
  attrition ratio stage-to-stage, preemption causes ranked) and a
  one-paragraph diagnosis naming the dominant leak with numbers. A
  fix is NOT in scope; the diagnosis shapes it as its own epic.

## Guards

Own worktree + build (explicit -Daimodules=classic,tex,baseline,deep),
own pinned binary WITH libfreeciv.so, ports 7500-7549, jobs <=4 (the
pv4 epic shares the box on 7300-7349). corridor-150 games run ~20 min
each — budget wall time, prune saves after reading them. Tracked
background dies at ~60 min; resume off DONE markers. tune-audit CLEAN
unchanged (no new knobs expected — counters only). One commit one
thing; meson test all suites; stop after the STATUS commit.
