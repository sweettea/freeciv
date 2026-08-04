Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md -- start at "The lift's parameter was never the
lever" and read forward. Branch epic/w03-horizon-content.

## 0a. BANKED: the rendezvous protocol is fixed (mechanism win)

`c5c7d27939` + `92d9c4009a`, measured over 20 games on lategame-80:
gather-timeouts 31 -> 0, sail rate 67% -> 98.6%, muster 2.22 -> 0.70
turns, 93% of lifts loading and sailing the turn the two ends meet,
delivered+arrived 73 -> 97. Score -1.80 (t=-0.07), FLAT.

**Human call 2026-08-03: this is banked as a mechanism win and the
flat score is not a defect.** Do not re-litigate it, do not tune it
chasing score, and do not revert it on the headline. 00-context
section 8 is the standing authority for that.

## 0. THEN: four changes are committed and UNVALIDATED

`a4af2f21e4`, `2aed19c427`, `8b94854e62`, `e35c0ba323`, `b338a7eeef`
and `8c59eeac3a` changed lift behaviour. **No benchmark has run on any
of them.** Tests and tune-audit are green; that is all. Rule 6 says
nothing about them may be claimed until measured, and the honest prior
is that they land flat -- every lift change so far has.

**Measure in this order, each against the previous as control**, on
ONE pinned binary with tuning-file switches (they interact):

1. rendezvous: `lift_manifest_max = 1`, `lift_hitch_wait_ratio = 0`,
   `lift_op_slots = 0` -- vs the pre-session baseline
2. manifest: `lift_manifest_max = 3`, wait still 0
3. patience: `lift_hitch_wait_ratio = 0.25`
4. slots: `lift_op_slots = 4`

**On the island cell, do (4) FIRST.** The array is full on 61.1% of
lift-running turns there against 36.0% on lategame-80, and lifts hold
3.26 of 3.44 live ops -- they saturate it against themselves. Also
note (2) cannot bind there at all: 95.4% of hulls carry at most 2, so
lift_manifest_max = 3 is unreachable and hitch-a-ride can add ONE
passenger onto an empty boat. Consider tuning that constant per cell;
lategame-80's techlevel 50 gives capacity-8 Transports instead.

**(1) IS DONE**: `benchmark/results/lategame-80-f138f69c9d1c`, 20/20,
score -3.70 (t=-0.11). The rendezvous does NOT shorten the muster --
that claim is falsified, see STATUS. Start at (2).

**Run both cells.** `lategame-80` for continuity with every lift number
on record; `isolated-150` because it carries ~10x the demand density
and is where a transport change can actually show. A change that helps
on one and not the other is information.

## 1. What was settled, so you do not re-derive it

**`lift_gather_turns` measured flat in 4..16 BEFORE the protocol fix,
and that result is superseded** (human, 2026-08-03). 60 games, timeouts
55 -> 12, deliveries 74/73/72, score flat -- but against 2.22-turn
musters with 31 timeouts, where it was only a give-up timer. Now
musters run 0.70 turns, timeouts are 0, so it does not bind at 8, and
with manifest > 1 it becomes a real gathering window for the first
time. TUNABLE AND UNTUNED. Re-measure it after the manifest, not
before.

**The rendezvous was one-sided and that was the defect.** The boat did
all the travelling while `deep_op_holds()` froze the passenger: ~570
held-idle passenger-turns per 20 games, 57% on lifts that never
sailed, and every gather-timeout burned the entire window (mean 9.0
against a cap of 8). Those musters were not slow, they were
impossible.

**The two-sided rendezvous does not do what its commit says.** Muster
duration unchanged, idle time up, timeouts still burning the full
window. The boat outruns a settler several times over, so the walking
never closed the gap. What it did buy is `unreachable` 4 -> 0, from the
formation-time reachability check. Keep the gate, drop the reasoning:
hitch-a-ride needs the meeting point NAMED and STABLE, not fast.

**Section 10's "do not build hitch-a-ride" is SCOPED** (see the
correction appended to findings/settler-missions.md). It judged
`lategame-80` and a design where the BOAT detours. With a fixed
rendezvous the passenger walks and the boat detours zero, which moves
the binding rung from `d3` (0.4/game) to `cont` (2.9/game there,
122/game on the island cell). That is why it was rebuilt.

## 1b. Two human leads (2026-08-03), neither built

**Boat supply is not measured where the lift needs it.** Colony demand
does reach the shipyard -- `dp->colony_want` feeds `lift_demand`
(`deepdefence.c:2443`) -- but the berth-SHORTAGE test next to it
(`berths < needed`) counts only MUSTER ops with a real `target_city`,
and a lift sets `target_city = 0` (`deepops.c:763`). So
`game_city_by_number(0)` is NULL and **every lift contributes zero to
measured hull demand**: the want named `colony_lift_weight` is fed by
amphibious assaults only. Proximity is not modelled either -- the gate
is `is_terrain_class_near_tile(TC_OCEAN)`, any coastal city, with no
weighting toward where passengers are actually waiting. "Bias boat
building toward the rendezvous, especially when short of hulls" is
therefore two separate gaps, and the second is arguably a defect.

Size it before building it: `lift-noboat` runs 7-10 per 20 games on
lategame-80, which is small. The island cell is where hulls are
scarce (95.4% of them carry <=2) and is the place to measure it.

**The passenger walk is UNVERIFIED, not just ineffective.** "The walk
fires and does not help" and "the walk never fires" produce identical
aggregates, and nothing distinguishes them. Ordering says it should
fire -- ops drive in first_activities before `done` is set, with full
moves -- but that is reasoning, not measurement, and this session
punished exactly that. Log passenger displacement during MUSTER before
reasoning any further from the flat result.

Note also that "the boat outruns the settler" is only true on
unimproved terrain: roads give 3 tiles/turn and rails are effectively
free, so the walk may matter in developed late-game positions and not
at all early. The better-supported explanation for the flat muster is
that successful musters already average 2.22 turns against a mean
formation gap of 4.48 tiles -- near the floor the boat's own travel
sets -- while the failures are the impossible-rendezvous class that
walking cannot fix either.

## 1c. THE TARGET THIS SESSION ACTUALLY FOUND: the boat build signal

Not a transport mechanism. `colony_want` drives boat production and is
measurably wrong in THREE directions, all on the same 20-game batches:

- **No term for idle hulls.** 73% of the fleet is idle per player-turn
  and only 18% carries anything, against 0.53 waiting passengers --
  3.1 hulls each, ~5 at the peak. The carrying fraction FALLS from
  23.9% to 13.0% as the fleet grows: overbuilding.
- **No term for fleet productivity.** The rendezvous fix raised
  crossings per hull 32% and the fleet did not shrink (-0.04
  boats/turn, noise). Freed capacity is not harvested.
- **No term for lifts going unserved.** The berth-shortage test counts
  only ops with a real `target_city`, and a lift sets it to 0
  (`deepops.c:763`), so lifts are invisible to the scarcity signal
  that is named `colony_lift_weight`.

- **No term for WHICH WATER (human, 2026-08-03).** The want is gated
  on `is_terrain_class_near_tile(ptile, TC_OCEAN)` -- "am I coastal" --
  and nothing asks whether that particular body of water is the one
  the demand needs. A hull is only useful to a lift if its ocean
  touches BOTH ends: the rendezvous with the settler AND the
  settler's destination. A boat launched into a lake, or into a sea
  that does not reach the target island, is pure waste and is
  currently indistinguishable from a useful one.

  **The infrastructure for this already exists and is unread.** Ocean
  bodies are numbered -- `tile_continent()` is negative for water and
  `assign_continent_numbers()` maintains it across terrain change --
  and `adv->oceans[]` is rebuilt every phase (`advdata.c:313`), of
  which 00-context section 3 notes only `.threat` is ever read. So the
  test is cheap: does the candidate city touch the ocean body that
  also touches the beachhead. deep_lift_best_meeting() already proves
  hull reachability with real pathfinding at formation; this is the
  same question asked one step earlier, at BUILD time.

It cannot see surplus, scarcity, or geography, and it is upstream of
everything the lift epic touched. Size it before building: the counts
above are hulls, not shields, so price the waste first -- and count
how many existing hulls sit on an ocean that reaches no wanted site,
which is the number this would actually recover.

## 1a. SETTLED overnight (2026-08-04), do not redo

- **Straying is fixed.** `b699ddefa1`: the settler_run hook now honours
  the operation hold. strayed 109 -> 4, and 0 in later runs.
- **Hitch-a-ride is NEUTRAL and defaults off.** Measured three times,
  fairest last (multi-drop live, 20 seeds): -8.25, t=-0.47, 10/20 up.
  Keep `lift_manifest_max = 1`.
- **Cutting boat production HURTS.** Two arms agree: damping -20.00,
  quartering the want -14.65 with deliveries -8%. The "Too Many Boats"
  reading is doubtful -- idle hulls are inventory, not waste.
- **The rendezvous fix is +11.28 (t=0.88) at 40 seeds**, not a win.
  Mechanism banked (gather-timeouts 545 -> 28, deliveries +31%).
- **`lift_op_slots` defaults OFF.** Relieving the array ceiling works
  (saturation 8% -> 0%, formations +26%, arrivals +22%) and costs
  score, -13.50 (t=-0.88).
- **TEN SEEDS ARE MISLEADING ON THIS CELL.** Two estimates flipped or
  collapsed under power: rendezvous +29.90 -> +11.28, slots +22.30 ->
  -13.50. Use 20 minimum for direction, 40 for a claim. A 20-game
  paired batch costs ~11 minutes.
- **Read the EXECUTION COUNTER before the score.** Three changes this
  session measured as "no effect" while never executing: hitch-a-ride
  (manifest pinned at 1), the drop routing (behind an early return),
  and the SSA suspension (byte-identical scorelogs).

## 1d. Two open design calls (human, 2026-08-04), both SIZED FIRST

**The hitcher decides on feasibility, not on comparison.**
deep_lift_enroll() asks "can this unit reach the rendezvous in time"
and never asks "what would it get by waiting for its own boat". The
alternative is often cheap -- hulls are in 3x surplus and mostly idle
-- which may be exactly why hitching shows no value. But idle-hull
count is NOT the expected wait, because lift formation is capped at
one per player per turn, so a queue drains slowly however many boats
are free.

DO NOT invent the cost model. The missing number is the wait itself:
log the turns between a unit first wanting a boat (deep_lift_wants_boat
true) and being enrolled as a lift's core, then compare that against
the hitch cost, which is now walk-to-rendezvous plus at most
lift_drop_radius. If the wait is short the feature is worth little at
any tuning.

**Overseas settlers can still found on top of each other.**
`citymap_reserve_city_spot()` (daitools.c:710) hard-blocks the exact
chosen tile -- deepsite.c:589 treats `reserved < 0` as worthless -- and
only increments a soft crowding counter over the city radius. Nothing
prevents founding at citymindist, and STATUS already measures deep's
median founded city sitting exactly there, with W02b moving spacing
0.07 tiles. The lift does not cause this but amplifies it overseas by
delivering more settlers to one fresh continent.

Note the reservation is also fragile for lifts: it is keyed on the
unit holding AIUNIT_BUILD_CITY, so a passenger re-tasked mid-crossing
drops its claim -- which is what the straying bug did until
b699ddefa1.

Size it before building: measure whether OVERSEAS cities are founded
closer together than mainland ones, and whether pairs delivered by the
same lift cluster worse than pairs that walked. If overseas spacing
matches mainland spacing, this is the known citymindist finding and
not a lift problem at all.

## 2. THE EPIC IS OVER AND ITS ANSWER IS NEGATIVE

Delivery is not the constraint. The cleanest single statement:

    lift_op_slots: +26% formations, +22% arrivals, -13.50 score

Five mechanisms now -- cross-water worker, the lift, the rendezvous
protocol, the hold fix, the slot relief -- have repaired, tripled and
un-capped delivery, and the aggregate has never followed. Section 8
measured ~11 overseas cities a game whether delivery ran at 12% or
28%; that number survived everything done to it.

Something downstream of arrival eats the gain. Either the colonies
that land are not worth their cost, or making them displaces something
better. **Do not build more transport.** The next epic is site quality
or settler cost.

## 2b. Then: the thing nobody has sized

**Is the colony worth founding at all?** Delivery has now been fixed
twice and cities did not follow either time (~11 overseas cities per
game whether the delivery rate is 12% or 28%). Site quality, not
transport. Join City target quality is still specified and unbuilt.

If the four changes above also land flat, that is three mechanisms
proven to work and worth zero, and the constraint is somewhere else
entirely. Say so rather than tuning them.

## WHAT NOT TO REBUILD

- **A trip-counting cap.** Falsified twice (1.6%, then 0.4/game).
- **Loosening the 1:1 cap.** Passengers it admits are not collected.
- **Routing ferries into stock's manager.** Refused three times.
- **Site scoring to change city spacing.** W02b falsified it.

## METHOD NOTES THAT EARNED THEIR PLACE

- **A census sizes a MECHANISM, not a feature.** When the cost model
  changes -- who pays for the detour -- the ladder must be re-derived,
  not re-run. This is how a correct census produced a verdict that
  stopped being true.
- **A verdict inherits its venue.** 30x between cells, unstated.
- **Read the mechanism before spending the machine on its parameter.**
  60 games confirmed a knob that one line of code had already made
  inert.
- **`can_unit_exist_at_tile()` is not reachability**, and neither is
  `real_map_distance()`. Both bugs this session came from treating
  them as such; both would have been invisible in the aggregate.
- **Prove the off-switch, do not assume it.** One seed proved
  `lift_op_slots = 0` bit-identical AND caught `opfull` silently
  redefining itself when the array grew.
- **A mechanism that provably works can still be worth zero.** Now
  twice over.

## RUN CADENCE (human, 2026-08-04)

**Human attention is the scarce resource, not machine time.** While the
human is engaged, build and check with QUICK tests -- a few games, a
mechanism proxy off existing logs, the unit suite. Save full-power runs
(40+ seeds, multi-cell sweeps, tuning passes) for an explicit go, so
they run while the human is asleep or elsewhere.

Costs, measured 2026-08-03, so nobody rations cheap measurements again:
**a 20-game paired batch is ~11 minutes at 10 jobs**; 40 seeds x 2 arms
is about an hour. This session mis-estimated that as 1.5 hours per
batch and skipped measurements accordingly -- including never once
executing hitch-a-ride, which sat four commits deep with
`lift_manifest_max = 1` in every run.

Corollary: quick tests are cheap enough that no mechanism should be
committed without at least one run in which its code takes a branch.

## BENCHMARK DISCIPLINE

- PIN the binary (server + libfreeciv.so + tuning to a scratch dir;
  RUNPATH is `$ORIGIN`) and pass `--server`. New pin per binary.
- **A/B with a tuning-file switch on ONE pinned binary.** There are
  now four: `lift_form_radius=0`, `lift_op_slots=0`,
  `lift_manifest_max=1`, `lift_hitch_wait_ratio=0`.
- `--quick` on a mirrored pair is 20 games, not 10.
- `pgrep -x`, not `pgrep -f`: the wrapper command lines contain the
  server path and inflate the count 5x. And `ps` inside the tool
  sandbox sees an empty PID namespace -- it is not evidence a batch
  died.
- Check DONE counts equal the game count before analysing -- AND
  before sequencing. "Wait until no freeciv-server is running" is NOT
  a valid test that a batch finished: run.sh dispatches through
  `xargs -P N`, so the gap between one game exiting and the next
  spawning is indistinguishable from completion, and a waiter built
  that way fires early and runs the next batch concurrently. Use
  `until [ "$(find $dir -name DONE | wc -l)" -ge $games ]` on the
  specific result directory. Oversubscription does not corrupt results
  -- each game is its own process with a pinned binary and a fixed
  seed -- but it can push games past TIMEOUT_S, and lategame-80's 2700s
  is already close at 10-way concurrency.
- /tmp is tmpfs and IS wiped between sessions.
- `meson test -C build` is the suite; `python3 benchmark/tune-audit.py`
  must print CLEAN (102/102/102/102 now).

## STATE

- Four new placeholders, none tuned: `lift_op_slots` 4,
  `lift_manifest_max` 3, `lift_hitch_wait_ratio` 0.25, joining
  `lift_form_radius` 8 / `lift_gather_turns` 8 / `lift_cross_turns` 20.
  Tune only after the mechanisms above are shown to be worth anything.
- `CLAUDE.md` cites `./benchmark/report.py`, which does not exist.
- `lategame-80` STAYS in the campaign (human call 2026-08-03).
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 -- DIFFERENT
  REGIONS). EBS caches preserved. NEVER terminate.
