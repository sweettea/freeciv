Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md (start at "The expansion stall was settlers walking
to islands", then "The ferry lift" — both are last session). Branch
epic/w03-horizon-content.

Your epic: **make the colony lift an operation.** Read
doc/deep-ai/findings/settler-missions.md §8 in full first. It has the
measurements and TWO fixes that were built, measured and refused.

SIZE IT BEFORE YOU BUILD IT

deep founds ~11 overseas cities per game on lategame-80. A perfect lift
is worth some fraction of the ~1.3 cities/player still missing in the
head-to-head — not all of it. If the sizing says the epic cannot pay,
say so and pick something else; that is a good outcome.

Do NOT quote "12% of overseas settlers arrive, so recover the other
88%". That ratio was measured and then corrected: two thirds of its
denominator was the same settlers re-picking. The honest number is the
absolute one, ~11 cities/game, and it barely moved when the churn was
removed.

WHAT IS ESTABLISHED, SO YOU DON'T RE-DERIVE IT

- Not boat supply: 95% of settler waiting-turns had a boat that stock's
  own is_boat_free() says would carry that settler.
- Not the settler: it flags FERRY_WANTED at the coast with full moves.
  Its own search radius is one tile (aiferry_find_boat_nearby), so it
  cannot go to a boat 2-8 tiles away. The boat is meant to come.
- The boat does not come. aiferry_findcargo()'s scan was reproduced
  exactly and finds the waiting settler every time — it is never run
  for civ2civ3 Transports, because dai_manage_unit() dispatches on
  worker/cityfounder before ferry (upstream-bugs #33).
- Routing those boats to dai_manage_ferryboat() anyway makes it WORSE
  (Transport turns carrying 488 -> 196). Stock's ferry manager parks an
  empty boat rather than fetching anyone. Do not repeat this.
- deep's own drop-boatless test is `ferryboat <= 0` and FERRY_WANTED is
  -1, so it drops the mission of a settler that just asked for a boat,
  and the drop clears the flag. Correcting it to `== FERRY_NONE` is a
  PREREQUISITE for any lift work — land it WITH the operation, not
  before: on its own it was measured at score -1.50 (t=-0.99), because
  the churn was a crude escape hatch and an honest queue that nothing
  serves is worse than giving up.

WHERE IT BELONGS

`deepops.c`. The operations layer already has what a lift needs: slots,
state machine, member claims, and holds that survive dai_manage_units()
clearing `done`. A lift is a named boat + a named settler + a named
beachhead + an abort condition. That replaces two parties hoping to
bump into each other with a commitment, which is what RC2 is about.

Watch for: the boat must not be re-tasked mid-lift (that is what the
op hold is for); the beachhead must be a tile the settler can actually
found on AFTER landing (§7's lesson — ask the question with the mover's
own knowledge, deep_settler_can_walk_there()); and the op needs an
abort so a lift to a site that stops being worth it releases both units.

MEASUREMENT

`--arms mixed_dc,mixed_cd` on lategame-80, 10 seeds, mirrored, plus a
pinned control on the same seeds so you can take the
difference-in-differences — that is what turned a t=1.55 raw margin into
a t=2.09 result two sessions ago. Report the score margin BY TURN.

Pre-register a falsification. Last session's D2 passed its own stated
criterion and was still wrong about the mechanism, and the "12%" framing
survived two rounds of analysis before the denominator was checked.
Write the instrumentation so it can name a cause you have not thought
of, and check your denominators.

BENCHMARK DISCIPLINE — all of these cost real time

- PIN the binary (freeciv-server + libfreeciv.so + tuning file to a
  scratch dir, RUNPATH is $ORIGIN) and pass run.sh --server. New pin
  directory per binary; never overwrite one a batch is using.
- /tmp is tmpfs on this box and it WILL be wiped between sessions.
  Anything you want to keep goes in a commit, not in the scratchpad.
- Long batches must run under the harness's own background mechanism; a
  `nohup ... &` inside a tool call is reaped and dies silently mid-run.
- NEVER edit deep-best.ini or a config while a batch is in flight.
- Filter non-benchmark players (barbarians make some games write 61 or
  64 rows for 60 seats).
- `meson test -C build` is the suite. `python3 benchmark/tune-audit.py`
  must print CLEAN (93/93/93/93 now).

STATE

- deep-best.ini: the site block is NOT TUNED. `site_stream_weight` is at
  placeholder 1.0, `site_travel_discount` is the human-set 0.9.
- deep beats ai/default head-to-head on lategame-80 as of §7: T80 score
  margin -1.49 -> +9.73, difference-in-differences t=2.09, up 7/10.
  primary-120 cross-check flat, no regression.
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 — DIFFERENT
  REGIONS). EBS caches preserved. NEVER terminate.
- Spacing is pinned by `citymindist`, not by the site objective (W02b,
  falsified). Do not spend an epic on site scoring hoping to move it.
