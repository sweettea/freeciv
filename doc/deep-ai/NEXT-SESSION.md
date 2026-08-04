Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md -- start at "The lift's parameter was never the
lever" and read forward. Branch epic/w03-horizon-content.

## 0. FIRST: four changes are committed and UNVALIDATED

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

**(1) IS DONE**: `benchmark/results/lategame-80-f138f69c9d1c`, 20/20,
score -3.70 (t=-0.11). The rendezvous does NOT shorten the muster --
that claim is falsified, see STATUS. Start at (2).

**Run both cells.** `lategame-80` for continuity with every lift number
on record; `isolated-150` because it carries ~10x the demand density
and is where a transport change can actually show. A change that helps
on one and not the other is information.

## 1. What was settled, so you do not re-derive it

**`lift_gather_turns` is not a lever in 4..16.** 60 games. Timeouts
55 -> 12, deliveries 74/73/72, score flat both directions. Do not
tune it again; it was inert because `n_members` was pinned at 1, so
"gather" gathered nothing.

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
- **`lift_gather_turns`.** Section 1.

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
- Check DONE counts equal the game count before analysing.
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
