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

It cannot see surplus OR scarcity, and it is upstream of everything
the lift epic touched. Size it before building: the counts above are
hulls, not shields, so price the waste first.

## 2. Then: the thing nobody has sized

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
