Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md (start at "W02b landed and was falsified" — that's
last session). Branch epic/w03-horizon-content at 2e6030be17.

Your epic: **make deep keep expanding on contested maps.** Not site
scoring. Read doc/deep-ai/findings/settler-missions.md in full first;
it has the measurements and one failed attempt already recorded.

THE ONE NUMBER THAT MATTERS

Head-to-head, deep vs ai/default seated in the SAME game, mirrored
seats, 10 seeds, score margin by turn:

    lategame-80   T20 +1.3  T40 +7.7  T60 +8.9  T80 -1.7
    primary-120   T20 +0.0  T40 +1.9  T60 +9.2  T80 +23.1  T120 +37.7

Deep out-develops ai/default early on BOTH cells and then stops
expanding on the crowded one. Cities by turn, deep/default,
lategame-80: T60 7.7/8.6, T70 7.8/9.3, T80 7.8/9.9 — deep adds 0.1
cities in twenty turns while default adds 1.3, and gets passed. It is
not being conquered: city count never falls, unitslost is 17.9 vs 16.8.

That is europe-T80's "17 cities, room for 18 more, zero settlers for
eight turns" reproduced under measurement. Fix the stall and deep beats
ai/default on both cells instead of one.

WHAT THE STALL IS, AND THE ATTEMPT THAT FAILED

52% of settler mission-turns on lategame-80 are a unit standing on the
tile it stood on last turn while holding a city mission (73% for the
player at war with three neighbours, 3-6% for its peaceful ones). Deep
sets `is_pos_dangerous` on the settler pathfinder (W05, correct, it cut
losses); `goto_is_sane()` knows nothing about danger, so a mission whose
every route is vetoed still reports sane=1 and the continuation block —
which only tests whether the TARGET is still buildable — never calls it
off.

**Do not repeat the obvious fix.** Dropping the mission and re-searching
(mirroring `drop-boatless`) was landed as 6ab78f8d98 and REVERTED as
cab01f2614: it fired 5070 times, tripled site evaluation, and 3958 of
4940 re-picks chose the tile just dropped, because
`deep_find_best_city_placement()` applies the SAME risk parameter as the
mover. Stationarity went UP, 52% to 62%.

START WITH INSTRUMENTATION, NOT A FIX. Search and execution share
`deep_settler_apply_risk()` and should agree; that they don't is
unexplained, and guessing cost a benchmark cell. There is no log line
saying why `deep_settler_gothere()` failed. Add one, run one cell, and
let it tell you whether the mover is refusing a step the search thought
was safe, waiting on a ferry, or something else. THEN fix.

The fix, when you know: make the SEARCH stop proposing targets the mover
will not walk to (RC5). Not another retry loop.

WHAT W02b SETTLED, SO YOU DON'T RE-DERIVE IT

Spacing is pinned by `citymindist`, not by the objective. Over
self-founded cities, the MEDIAN founded city sits exactly at the
ruleset minimum on every cell, 80 land/player and 380 alike (29.4% /
27.4% / 25.5% exactly on the floor; 80-87% within 4 tiles). That is why
a 25x sweep of `site_overlap_weight` moved spacing 0.2 tiles and why
replacing the entire catchment valuation with a citizen-turn stream
moved it 0.07. Site scoring can only speak for the minority of cities
off the constraint. The stream landed anyway (score flat on all three
cells, four tuned constants deleted); it is not the lever for spacing
and nothing further should be spent trying to make it one.

W02b's REMAINING commits are unaffected — settler cost from
growth-blocked cities and Join City are about settler SUPPLY, not site
ranking, and Join City is the natural partner to the stall work
(upstream-bugs #31; 637 of 647 surviving founders end on worker duty).

MEASUREMENT — the head-to-head is now the honest test

`--arms mixed_dc,mixed_cd` seats deep against ai/default in one game,
mirrored so each module plays every seat. Ten seeds per cell. Analysis
script: scratchpad `h2h.py` pattern — pair by SEAT, average the mirror,
paired t over seeds. Separate-game score ratio and head-to-head agreed
in direction on both cells this session, so the primary metric is not
broken — but the head-to-head is what answers "would it win".

Report the score-margin-BY-TURN series, not just the endpoint. The
endpoint hid the entire story on lategame-80: deep led at T60 and the
final number said it lost.

Pre-register a falsification before running anything. F1 fired on W02b
and that was the session's most useful half-hour.

BENCHMARK DISCIPLINE — all of these cost real time this session

- PIN the binary (copy freeciv-server + libfreeciv.so to a scratch dir,
  RUNPATH is $ORIGIN) and pass run.sh --server.
- NEVER edit deep-best.ini or a config while a batch is in flight.
  run.sh hashes the tuning file at batch START but every game re-reads
  it; a mid-batch edit silently changes the arm. This cost a discarded
  batch this session.
- NEVER `pkill -f freeciv-server` unscoped. Scope it to the pinned path.
- Filter non-benchmark players (barbarian nations appear in some games
  and not others) before averaging — they moved baseline's city count.
- `meson test -C build` is the suite. `python3 benchmark/tune-audit.py`
  must print CLEAN (93/93/93/93 now).

STATE

- deep-best.ini: the site block is NO LONGER TUNED. site_stream_weight
  sits at placeholder 1.0, four weights were deleted, and
  site_travel_discount is the human-set 0.9 (range [0.75, 1.0]).
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 — DIFFERENT
  REGIONS, a combined describe-instances reports both as nonexistent).
  EBS caches preserved. NEVER terminate: DeleteOnTermination=True.
- largepox-200.conf is now committed, with its falsified premise
  rewritten in the header.

KNOWN-OPEN, NOT YOURS UNLESS YOU CHOOSE IT

- Is `lategame-80` a good tuning cell? Twelve starting settlers,
  techlevel 50, 80 turns, 80 land/player — and deep's stall is worst
  exactly there. Three of four campaign cells are crowded.
- Deep builds 102 dedicated worker units where baseline builds 677;
  repurposed founders satisfy the worker demand test at pop_cost 2.
- `deep-op-stall`: 53% of events are MARCH (units released to the stock
  attack path that then don't move), 41% of signatures persist >= 5
  turns. Same shape as the settler freeze, in the operations layer. The
  log line records unit TYPE but not unit ID, so individuals can't be
  tracked — one line to fix. FREECIV_DEEP_QUIET=1 silences metric lines
  in a live game.
- `deep_growth_arrivals()` is not L0-testable as written (reads
  get_city_bonus, city_granary_size, game.info). Splitting the
  arithmetic from the lookups makes W02b's two test commits cheap.
