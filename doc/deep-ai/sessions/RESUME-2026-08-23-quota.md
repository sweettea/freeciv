# Resume point — 2026-08-23 ~14:15, 6-hr quota block at 94%

Orchestrator wake armed for 15:41 local (quota rollover). If resuming
in a fresh session instead, this file is the state.

## In flight (all state on disk; detached games survive quota-out)

- **epic/poach-fix** (Opus, worktree .worktrees/poachfix, tip
  a05a5489b7, ports 7300/7400): waiting on fix-vs-base smoke games.
  The war-gap fix — its verdict decides corridor-forts vs poach
  follow-up next.
- **epic/site-improvable** (Opus, .worktrees/improvable, tip
  c87374674e, ports 7600s): corpus phase.
- **epic/rail-upgrade** (Opus, .worktrees/railup, tip d9dd39423b,
  ports 7650s).
- **epic/grow-worlds** (Sonnet, .worktrees/growworlds, tip d72c96bacf,
  ports 7250-7299): smoke games mid-run, air w6001 ~T160/250, naval
  w5001 ~T130/250 as of 14:06 (~75/110 min to finish). Watcher
  bnl7alo09 armed inside the agent (never end a turn without a live
  watcher — it parked 3x on this). Then: validate pick-snapshot.py,
  launch full batch `setsid bash ./benchmark/grow-worlds.sh go` from
  worktree root.
- **G-tune round 2/3** (ports 7500s, nice 10): TUNE-DONE watcher
  bq4ihih1j in main session.

## On rollover

1. Check each agent's task output tail; any that died mid-API-call:
   SendMessage resumes it (ids: poach-fix a153b77223484249c,
   improvable ad3edff2b96275920, railup a0db1a74057c1c56a, grower
   a56e2764ae8264246).
2. Merge each epic as it lands: keep-both STATUS recipe, meson test,
   tune-audit + ini dedupe (awk seen[] dup check) after each.
3. After grow-worlds merges: bluewater census Opus agent
   (.worktrees/bluewater ready, brief S-BLUEWATER fully amended,
   ports 7200-7249, jobs <=2) — the ONE queued spend at 74% weekly.
4. Base branch epic/w03-horizon-content tip d34e995a1f; user's live
   game port 5560 on .derby13-bin, all batch work at nice 10.
