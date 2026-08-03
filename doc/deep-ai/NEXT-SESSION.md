Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md (start at "The expansion stall was settlers walking
to islands" — that's last session). Branch epic/w03-horizon-content.

Your epic: **make the ferry pipeline actually deliver settlers.** Read
doc/deep-ai/findings/settler-missions.md §7 in full first — it has the
measurements, the fix that landed, and the number below.

THE ONE NUMBER THAT MATTERS

    lategame-80, deep arm, per 10 games
      picks land      655  ->  arrived 461   70%
      picks overseas  919  ->  arrived 114   12%

Nine of every ten settlers that commit to a boat trip never arrive.
2398 of 7390 mission moves are a settler waiting on a lift — a third of
all the settler-time in the game. Land expansion is fine; sea expansion
is a queue that does not drain.

That number was 7% before last session's fix, so the fix helped and did
not solve it. Every stalled overseas settler now correctly reports
`reason=ferry` in the `deep-settler ... stall` metric line, so this is
measurable per turn from the benchmark logs without new instrumentation.

WHAT LAST SESSION SETTLED, SO YOU DON'T RE-DERIVE IT

`goto_is_sane()` pathfinds NON-omnisciently and was being used as the
boat-versus-walk switch, so unmapped water read as passable and settlers
walked into coastlines: 5656 of 6323 stalled mission-turns on
lategame-80. `deep_settler_can_walk_there()` (0f126c33b7) asks the
question at the player's real handicap level, which is what the site
search and the mover already do. Head-to-head vs ai/default, T80 score
margin -1.49 -> +9.73 (t=2.09 paired, up 7/10). **deep now finishes
ahead of ai/default on lategame-80**, the cell it was losing.

Do NOT re-open the "settlers freeze in war zones" theory. The danger
veto accounts for 0.3% of stalls. That reading is marked superseded in
findings §3 and the fix built on it is reverted (cab01f2614).

WHERE TO START LOOKING

`aiferry_gobyboat()` / `aiferry_find_boat()` in ai/default/daiferry.c —
the system whose own docs call it "statistical mechanics rather than
logic" (RC2). Deep does not own it; it calls it. Decide early whether
this epic is a deep-side ferry planner or a narrower fix, and say which
in the first commit message.

The throttle now works — `plr_data->stats.passengers >= boats` caps
overseas picks, and overseas commitment fell 1320 -> 919 because of it.
So the question is not "stop wanting islands", it is "deliver the ones
we commit to". Consider: does the settler wait at the wrong tile, does
the boat get retasked, is there a boat at all? Count before fixing.

SMALLER, ADJACENT, NOT THIS EPIC

- `behaviour` stalls: 523 turns on lategame-80, a settler whose corridor
  closed mid-walk with no re-search. This is what findings §5.1 wanted,
  aimed at a case a tenth the size it was thought to be. It is the third
  attempt at that shape — the first two were reverted — so it needs its
  own falsification, not a bolt-on.
- W02b's remaining commits: settler cost from growth-blocked cities, and
  Join City (upstream-bugs #31).

MEASUREMENT

`--arms mixed_dc,mixed_cd` on lategame-80, 10 seeds, mirrored. Analysis
scripts from last session are worth rewriting rather than hunting for;
the pattern is: pair by SEAT, average the mirror, paired t over seeds,
and report the score margin BY TURN. Then take the
difference-in-differences against a pinned control on the same seeds —
that is what turned a t=1.55 raw margin into a t=2.09 result last
session, and it costs one extra 20-game batch.

Pre-register a falsification before running anything. Last session's D2
passed its own stated criterion and was still wrong about the mechanism;
write the ladder so it can name a cause you have not thought of.

BENCHMARK DISCIPLINE — all of these cost real time

- PIN the binary (copy freeciv-server + libfreeciv.so + the tuning file
  to a scratch dir, RUNPATH is $ORIGIN) and pass run.sh --server. Pin a
  NEW directory per binary; never overwrite one a batch is using.
- Long batches must run under the harness's own background mechanism.
  A `nohup ... &` inside a tool call is reaped and the batch dies
  silently at whatever turn it had reached.
- NEVER edit deep-best.ini or a config while a batch is in flight.
- Filter non-benchmark players (barbarian nations show up in some games
  and not others — primary-120 wrote 64 rows for 60 seats).
- `meson test -C build` is the suite. `python3 benchmark/tune-audit.py`
  must print CLEAN (93/93/93/93 now).

STATE

- deep-best.ini: the site block is NOT TUNED. `site_stream_weight` sits
  at placeholder 1.0, `site_travel_discount` is the human-set 0.9.
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 — DIFFERENT
  REGIONS, a combined describe-instances reports both as nonexistent).
  EBS caches preserved. NEVER terminate: DeleteOnTermination=True.
- Spacing is pinned by `citymindist`, not by the site objective (W02b,
  falsified and recorded). Do not spend another epic on site scoring
  hoping to move spacing.
