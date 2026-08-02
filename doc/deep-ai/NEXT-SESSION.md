Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md (start at "Site scoring cannot express spacing" —
that's last session). Branch epic/w03-horizon-content at 6c7e940139.

Your epic: the first three commits of W02b. Read
doc/deep-ai/workstreams/W02b-catchment-stream.md in full before writing code;
it was written from a long design conversation and it contains the reasoning
that makes the epic interpretable.

    ai/deep: simulate city growth to a horizon instead of a target-size cutoff
    ai/deep: value a site as discounted citizen-turns, replacing the catchment average
    ai/deep: exclude tiles a neighbouring city will actually work

WHY THIS AND NOT SOMETHING EASIER

Two site-scoring hypotheses were killed by measurement last session, in the
direction we were hoping for both times. `deep_catchment_average()` returns
`vsum/wsum` — a weighted AVERAGE — so overlapped tiles are down-weighted in
numerator and denominator and DROP OUT rather than counting against a site. A
25x change in `site_overlap_weight` moved spacing by 0.2 tiles. No weight can
fix an average that discards magnitude. The epic replaces the average with a
stream; that is the point, and a version of this epic that keeps the average
and adds a term has missed it.

THE CURRENCY IS ALREADY DECIDED — DO NOT INVENT ONE

`deep_capacity_credit()` in deephorizon.c is
`amortize(per_citizen * steps * weight, ttg)`. A city is the same object: a
stream of citizens arriving over time, each earning tile output. Use that
currency and that discount. `amortize()` is a discount rate (MORT=24), NOT a
deadline — a horizon that shortens toward `endturn` is fitting the benchmark,
since our cells stop at 80/120/200 turns.

The payoff beyond spacing: this puts `founder_want` and building want in ONE
currency for the first time, which is the europe-T80 arbitration failure (a
city wanting a settler at 514 with an empty queue builds a Colosseum anyway).
Settler count sat at 26 across four arms and two tunings differing 7x on the
site parameters — a quantity invariant to every site knob is not being set by
site scoring.

MEASUREMENT — the mechanism metric comes FIRST

Acceptance is spacing responding to land: cities/player tracking land/player
across largepox-200 (380 land/plr), primary-120 (400) and lategame-80 (80).
Nothing we have can do that at all today. Report cities/player and tiles/city
BEFORE score. Four cells last session moved mechanism decisively with flat
score, and one moved score with no mechanism at all.

Pre-register a falsification before running anything.

BENCHMARK DISCIPLINE — all of these cost real time

- Ten paired seeds minimum. Two 4-seed results failed to replicate at 10.
- Every benchmark gets a PINNED binary: copy freeciv-server + libfreeciv.so to
  a scratch dir (RUNPATH is $ORIGIN, so the copy is self-contained) and pass
  run.sh --server. Rebuilding into a live run has happened three times.
- NEVER run large games concurrently when timing anything. Four at once
  inflated each other's CPU time 60% and inverted an A/B.
- `startunits` is capped at 21 chars; longer is REJECTED with one log line and
  the setting silently stays at the ruleset default of ONE settler. Check city
  counts, not exit status.
- New constants go in struct tuning as PLACEHOLDER, registered in
  tuning-params.ini and the blocks; `python3 benchmark/tune-audit.py` must
  print CLEAN (currently 96/96/96/96).
- `meson test -C build` is the suite (deepodds, baseline-frozen). CLAUDE.md's
  `./build/tests/unit_tests` does not exist — fix that line if you touch it.
- A profiling build exists: `meson setup build-prof -Dbuildtype=release
  -Daitiming=true -Daimodules=classic,baseline,deep -Dclients= -Dfcmp=
  -Dtools=`. `benchmark/profile.py` reads its per-turn AI timing.

CLOUD BOXES — both idle, both harvested, DO NOT TERMINATE

campaign-a6 (us-east-1, i-0e9a257ef3b781349) and campaign-b6 (us-east-2,
i-05162168cf393cb20). Note the DIFFERENT REGIONS — a combined
describe-instances call reports both as nonexistent. Both finished:

    a6  training +76.1 -> holdout +62.2
    b6  training +94.5 -> holdout +58.8

deep-best.ini is now b6 at 96 params. BOTH AUTO-SHUTDOWN WATCHDOGS WERE DEAD
(bare background shells, no systemd unit); a6 idled 80 minutes before anyone
noticed. Re-arm as the systemd oneshot AWS.md prescribes before the next
campaign, and add primary-120-classic to the blend — it is the ruleset guard,
and all 96 constants are currently conditioned on civ2civ3 alone.

Stopping is safe and preserves the EBS cache. NEVER terminate:
DeleteOnTermination=True would destroy every result. Read-only ssh needs
dangerouslyDisableSandbox; Monitor runs sandboxed with no network.

KNOWN-OPEN, NOT YOURS UNLESS YOU CHOOSE IT

- Build arbitration (europe-T80). The evidence points here from three
  directions and it is the natural NEXT epic after this one. W02b makes it
  arithmetic; it does not by itself fix it.
- `site_overlap_crowding` = 16.0 and `site_rival_crowding` = 0.9 are live code
  defaults that MEASURED FLAT (t<1 on both tunings). deep-best.ini zeroes them.
  The code defaults still need a decision — ask, do not quietly revert human-set
  values. W02b deletes both terms anyway.
- upstream-bugs #31: no AI ever performs Join City. The hook comment is already
  at deepsite.c:610. It is the same term as capped-city settler export with the
  opposite sign — implement them together, not separately.
- Deep's score advantage is majority-CULTURE in all four cells measured, while
  material position is flat or behind. Culture is 11% of score; citizens+techs
  are 82%. Nobody has explained this and it may be the most important open
  question in the project.
- garden2b is RETIRED for head-to-head (conquest lottery, sd 710 on a mean of
  -138). largepox-200 exists but is uncommitted and its header argues a premise
  the first probe falsified — rewrite or drop it.

There is a live human game on port 5556 (cwd /home/sweettea/freeciv) — never
kill it.
