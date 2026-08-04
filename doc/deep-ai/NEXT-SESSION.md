Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md -- start at "The lift epic's premise is falsified"
and read forward to the end. Branch epic/w03-horizon-content.

## 0. The head-to-head is STILL current. Do not re-take it.

Taken 2026-08-03 evening on `da57ed6206` against a pinned control at
`0f126c33b7`; the series is in STATUS. Everything that landed since
(the lift, the census) measured neutral, so the next head-to-head is
worth taking after something MOVES, not before.

## 1. What last session established, so you do not re-derive it

**The lift epic's stated design call was falsified before it was
built.** The call was "make the cap count TRIPS, not settlers". Measured
first: worth **1.6%**. There were no trips to share -- boats carried
cargo on 11.1% of 8384 boat-turns.

**The same census found the real defect: the RENDEZVOUS.** 95.6% of
capped waiting unit-turns had a rideable boat with a free slot; 0.6%
ever stood on the same tile, which is where loading happens.

**The lift operation was then built against THAT, and it works.** 165
formed / 110 sailed / 92 delivered per 20 games, against a negative
control that logs zero. **And the score is flat** (-1.35, t=-0.36;
cities -0.07, t=-0.16).

So: delivery is fixed and it did not buy cities. Section 8 had already
found the absolute overseas yield was ~11 cities per game whether the
delivery rate was 12% or 28%. **Stop treating delivery as the
constraint on the count.**

## 2. THE EPIC: hitch-a-ride, then tune the lift

Human design call, 2026-08-03, and it is now correctly ordered: the
census that killed trip-counting measured a world with no trips in it.
The lift changes that denominator, so re-ask the question the lift
answered for one passenger, for a second one:

- A unit wanting passage inspects boats **already servicing someone**
  bound for that continent.
- It joins if the detour costs **less than one extra turn of boat
  waiting**. That gate is what keeps this from becoming "loosen the
  cap", which is separately ruled out.
- `members[]` is already a manifest, the beachhead is already named,
  and `deep_op_claiming()` already keeps the army's claim ahead of the
  lift's. The structure is there.

**Re-run the census first** (`benchmark/liftcensus.py`, the
`deep-lift` ladder) with the lift ON. The `loaded` rung is the sharable
demand; it was 1.6% before the lift existed and that number is now
stale. Size it before building it -- that discipline is what saved the
last session from building a 1.6% manifest.

Then tune, and this is likely the larger lever of the two:
`lift_gather_turns` (8) governs the 31 gather-timeouts and 24 hollowed
ops out of 165; `lift_form_radius` (8) governs how far a boat will be
sent; `lift_cross_turns` (20) barely binds (2 events). ALL THREE ARE
PLACEHOLDERS and none has ever been tuned. They are in the ECON block.

## 3. Then: is the colony worth founding at all?

Delivery is solved and cities did not move, which points at what is
being delivered TO. Site quality, not transport. Join City target
quality (section 2 of the previous handoff) is still specified and
still unbuilt.

## WHAT NOT TO REBUILD

- **A trip-counting cap as the headline.** Falsified at 1.6%
  (findings/settler-missions.md section 9). Hitch-a-ride is the same
  idea correctly gated and correctly ordered -- AFTER the lift.
- **Loosening the 1:1 cap.** Passengers it admits are not collected
  either; that was true before the lift and the lift does not change
  the argument. It trades a colony for a road.
- **Routing ferries into stock's manager.** Refused twice on
  measurement (section 8). The lift owns the crossing instead.
- **Site scoring to change city spacing.** W02b falsified it.
- **The head-to-head.** Section 0.

## METHOD NOTES THAT EARNED THEIR PLACE

- **Size the design call BEFORE building it.** The trip-counting cap
  was specified twice in handoffs and died to one 20-game census. That
  census cost far less than the manifest would have.
- **A ladder names the constraint; a single number does not.** "1.6%
  can share a trip" and "95.6% have a free slot 5 tiles away" came off
  the same census. The second rung is what the epic became.
- **Check the predicate, not just the denominator.** The first census
  used `can_unit_load()`, which opens with `same_pos()` -- every
  distance rung would have collapsed to `d0` and "confirmed" the
  premise was worthless for a purely mechanical reason. `d0` is kept as
  the permanent tripwire: `d0` at 0.6% while `d3` reads 13.0% proves
  the predicate is not silently collapsing.
- **Do not name a branch for the outcome you hope it has.** The
  empty-boat case was labelled `lift-landed` on an assumption and read
  as a two-thirds loss rate. Instrumented: 9 of 11 were standing
  exactly on the beachhead, all 11 alive. It is `lift-delivered` vs
  `lift-strayed` now.
- **A mechanism that provably works can still be worth zero.** Both
  the cross-water worker and the lift fixed real, measured defects and
  moved nothing. Record the proxy AND the aggregate (00-context section
  8).

## BENCHMARK DISCIPLINE

- PIN the binary (freeciv-server + libfreeciv.so + tuning to a scratch
  dir; RUNPATH is `$ORIGIN`) and pass `--server`. New pin per binary.
  With every batch pinned, rebuilding the main tree mid-run is safe.
- **A/B a feature with a tuning-file switch on ONE pinned binary**
  (`lift_form_radius = 0` is the lift's off-switch). Attribution is
  then exact -- the arms cannot differ by anything else.
- **`--quick` on a mirrored pair is 20 games, not 10.** A wait loop
  gated on 10 or 12 reads partial results.
- **This machine suspends.** A batch can come back with games truncated
  mid-turn and no error in the log ("OOM killer disabled/enabled" in
  journalctl is the signature). Check `DONE` counts equal the game
  count before analysing; truncated games bias every per-turn census
  toward the early game.
- /tmp is tmpfs and IS wiped between sessions. Anything worth keeping
  goes in a commit.
- Long batches must use the harness's background mechanism.
- `meson test -C build` is the suite; `python3 benchmark/tune-audit.py`
  must print CLEAN (100/100/100/100 now).

## STATE

- deep-best.ini: the site block is NOT TUNED, and neither is the lift
  block. `lift_form_radius` 8, `lift_gather_turns` 8,
  `lift_cross_turns` 20, `site_stream_weight` 1.0,
  `site_travel_discount` 0.9 (human-set), `founder_pool_per_site` 0.06,
  `site_enough_result` 250.0, `settler_guard_tolerance` 1.0,
  `worker_cross_continent_discount` 1.0 -- all placeholders.
- `worker_cross_continent_discount` is still NOT worth tuning until the
  lift's own parameters are: it governs a comparison only 316 of 6834
  attempts ever reached.
- `lategame-80` STAYS in the campaign (human call 2026-08-03).
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 -- DIFFERENT
  REGIONS). EBS caches preserved. NEVER terminate.
