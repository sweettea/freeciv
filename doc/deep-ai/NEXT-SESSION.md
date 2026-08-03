Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md — start at "The stale head-to-head, re-taken"
and read forward to the end. Branch epic/w03-horizon-content.

## 0. The head-to-head is CURRENT. Do not re-take it.

Taken 2026-08-03 evening on `da57ed6206` against a pinned control at
`0f126c33b7`, 10 train seeds, `lategame-80`, mirrored `mixed_dc`/`mixed_cd`:

    turn      pre     post    delta   t(DiD)
      40    10.87    11.85    +0.98     0.43
      60    16.12    14.88    -1.23    -0.47
      80     9.73     9.05    -0.68    -0.23

The control reproduced the previous session's series exactly. The
six-change stack neither regressed nor improved it. Only two behaviour
changes have landed since (`3647d0fea4` and the census split), and both
measured neutral, so the next head-to-head is worth taking after the
lift operation, not before it.

## 1. THE EPIC: the colony lift as an operation, and the cap counts TRIPS

This was already the specified next epic. This session made it the
bottleneck for a **second** consumer and sharpened its design.

Workers can now cross water (`3647d0fea4`) — the machinery works and it
is measured neutral, because of this:

    lategame-80, 10 games, 6834 cross-water attempts, 70 fired

      why=cap      3460   50.6%   a settler is already queued for every boat
      why=noboat   2988   43.7%   no free boat anywhere
      why=outbid    230    3.4%   overseas work existed, lost to land work
      why=nocand     86    1.3%   no reachable overseas work
      fired          70    1.0%

**94% die on lift supply.** Boat supply — not site scoring, not worker
valuation, not the discount — is what both the settler pipeline and the
worker pipeline are now starved by. The ferry fixes made it tighter, not
looser: settlers stay correctly flagged now, so `passengers >= boats` is
almost always true.

The design call is already made and is now better evidenced:

- **Keep 1:1 for ordinary settlers.** A settler occupies a boat for a
  round trip. A capacity multiplier queues eight behind one Transport and
  recreates the dock-parking bug.
- **The cap should count TRIPS, not settlers.** civ2civ3's Transport has
  capacity 8 and carries one passenger. A worker — or a second settler —
  riding a boat that is *already crossing* costs nothing. That needs a
  manifest, which needs the lift to be an operation.

Home is `deepops.c`, which already has holds that survive
`dai_manage_units()` clearing `done`. `findings/settler-missions.md` §8
has the diagnosis and the two dead ends. A named boat, a named
passenger list, a named beachhead, held against re-tasking, with an
abort condition.

Size it first: ~11 overseas cities per game today, and the remaining
head-to-head gap is ~1.3 cities/player. A perfect lift is worth some
fraction of that, not all of it.

## 2. Join City target quality

Unchanged from last session; still specified, still not built. Currently
ranked by soonest arrival only. Human call — it should weigh **granary
presence**, and **the citizens' value to that city** (a small city gains
more per citizen), against **travel time and route risk**. One weight,
not four, if value is discounted by `amortize(value, turns)` the way the
site search already does, with route risk from the shared
`deep_settler_overnight_veto()`.

## 3. Tune the settler/boat block — but check the denominator first

Still true that **not one of the eighteen parameters there has ever been
tuned**. Highest-leverage first: `site_enough_result` (250.0, decides
whether colonisation is evaluated at all), `founder_pool_per_site`
(0.06; measured per-cell calibrations disagree 3.5x — **report per-cell
values, do not average the spread**), `settler_guard_tolerance` (1.0),
then `colony_lift_weight`, `overseas_fear`, `overseas_decay`,
`site_travel_discount`, `site_stream_weight`.

**Do NOT tune `worker_cross_continent_discount` yet.** Only 316 of 6834
attempts ever reached the comparison it governs and only 230 were
decided by it; tuning it now measures noise. It comes after the lift.

## WHAT NOT TO REBUILD

- **Site scoring to change city spacing.** W02b falsified it; the
  negative control confirmed it. Median founded city sits on
  `citymindist`.
- **A founder pool sized per city.** Superseded; history at `5d422d0b44`.
- **A ferry capacity multiplier on the 1:1 cap.** See §1 — trips, not a
  multiplier.
- **Loosening the worker's ferry cap so workers bid against settlers.**
  That trades a colony for a road. The 94% is an argument for more
  trips, not for a weaker guard.
- **The head-to-head.** §0.

## METHOD NOTES THAT EARNED THEIR PLACE

- **Two fixes repairing opposite ends of one broken protocol cannot be
  measured separately.** Both ferry fixes measured negative alone and
  +4.37 (t=1.84) together.
- **Instrument before fixing, and build the diagnostic as a LADDER**
  that relaxes every candidate constraint, not the two you suspect.
- **Instrument the fix's own refusals, not just its firings.** The
  cross-water pass shipped with a one-line reason log on every decline,
  and that log — not the score table — is what identified the real
  bottleneck. A neutral result with no refusal breakdown would have been
  uninterpretable.
- **Check your denominators.** "12% of overseas settlers arrive" survived
  two rounds before two thirds of the denominator turned out to be the
  same settlers re-picking. Likewise "14.5% of idle turns had a boat
  available" used `available_boats`; the cap asks the stricter
  `passengers < boats`, and that is where the population went.
- **A pre-registration can pass its own stated criterion and still be
  wrong about the mechanism.** D2 did exactly that.

## BENCHMARK DISCIPLINE

- PIN the binary (freeciv-server + libfreeciv.so + tuning file to a
  scratch dir; RUNPATH is `$ORIGIN`) and pass `--server`. New pin
  directory per binary. With every batch pinned, rebuilding the main
  tree mid-run is safe — but only then.
- **/tmp is tmpfs here and IS wiped between sessions.** Anything worth
  keeping goes in a commit, not the scratchpad.
- Long batches must use the harness's background mechanism; a
  `nohup ... &` inside a tool call is reaped and dies silently mid-run.
- NEVER edit `deep-best.ini` or a config while a batch is in flight.
- Filter non-benchmark players — barbarians make some games write 61 or
  64 rows for 60 seats.
- `meson test -C build` is the suite; `python3 benchmark/tune-audit.py`
  must print CLEAN (97/97/97/97 now).

## STATE

- deep-best.ini: the site block is NOT TUNED. `site_stream_weight` 1.0,
  `site_travel_discount` 0.9 (human-set), `founder_pool_per_site` 0.06,
  `site_enough_result` 250.0, `settler_guard_tolerance` 1.0,
  `worker_cross_continent_discount` 1.0 — all placeholders.
- `lategame-80` STAYS in the campaign (human call 2026-08-03). Island,
  tech-saturated, 60% of settler targets overseas; it surfaced every bug
  the last two sessions found. The guard is to report per-cell tuned
  values and treat sharp disagreement as overfit evidence.
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 — DIFFERENT
  REGIONS). EBS caches preserved. NEVER terminate.
