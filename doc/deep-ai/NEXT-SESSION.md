Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md -- start at "W36 step 0: the sizing retires the
commission" (the last section) and read backward as far as you need. Branch
epic/w03-horizon-content.

**Section 0 is the job.** Sections 1-2 are why, and what has already been
falsified so you do not redo it. Anything below section 5 is history kept
for provenance: correct when written, NOT to be followed now.

## 0. THE JOB: the tuning pass that is owed

**DEFERRED TO THE CLOUD PILE — human call, later on 2026-08-04:** "we can
tune on cloud machines, later, as one of a pile." Do not run this pass
locally. The parameter list below stays correct and goes into the cloud
campaign when it runs. The current arc is the session ladder in
`doc/deep-ai/sessions/README.md`: W34 phase A (tuning separability), the
omniscient-AI flag, and the difficulty ladder.

Original call, kept for the campaign design: **tune, regardless of what
score says about any one mechanism.** The lift cluster is untuned and now
finally binding. `boat_water_gate`/`boat_gate_at_bid` are 0/1 and not
tunable; these are:

- `lift_gather_turns` -- re-measure AFTER the manifest. The manifest is
  8 now and the old flat result predates the rendezvous fix.
- `lift_hitch_wait_ratio` -- and see section 1 on where hitching can
  possibly pay.
- `lift_drop_radius`
- `boat_idle_damping`
- `colony_lift_weight` -- the want the water gate filters.

Training seeds via `benchmark/tune.py`, reported on **holdout**. The
harness rules in section 5 have each cost a batch; read them first.

## 1. W36 IS CLOSED. Do not build the commission.

Step 0 ran on two cells, 20 seeds each (`findings/commission-sizing.md`,
verdict block at the top of `workstreams/W36-build-commissioning.md`).
It retired the spine:

- **The award by ETA.** On production capability alone the ordering city
  is already best or equal-best on its water 65.4% / 61.7% of the time
  and the median rival is a turn SLOWER. The 2-3 turn "advantage" that
  shows up when a rival is credited with its shield stock is that
  rival's SAVINGS -- an accounting transfer whose cost the ETA does not
  show.
- **The fall-through cascade.** Its stated case -- a commission with no
  producer -- is 12 waiting unit-turns in 20 games on the island cell
  and ZERO on lategame.
- **Cancellation.** An order lives a median of 2 turns (island) / 1
  (lategame) and 85% of production runs already end without a hull.
  Build churn is de facto cancellation.

**The wait is finally measured**, and it is the number section 1d asked
for three times: median 0 in both cells, mean 2.28 / 0.98, p90 7 / 3,
and 57% of spells never enrol at all. Half of all passengers are
enrolled the same turn they first want a boat. **Hitching cannot pay in
the median case.** All of its room is the tail -- a fifth of spells hold
three-fifths of the waiting turns on the island cell -- and that is the
only part of W36 still standing.

`lift-noboat` was corrected upward and downgraded in importance: 7.30
spells/game on the island cell against the "7-10 per 20 games" the old
brief carried, but those spells END WITHIN A TURN and never enrol.

## 2. What NOT to do next, and why

- **Do not build more transport.** Five delivery mechanisms have worked
  and the aggregate followed none.
- **Do not price the fleet COUNT.** Falsified from the aggregate (idle
  share invariant 75-80% across five interventions) and from the hull.
- **Do not add a demand term to the build GATE.** The geometry test
  works precisely because it does not move with demand.
- **Do not re-open the award.** Section 1. Measured on two cells.

## 3. Two open leads with no owner

- **Rates.** science/tax/luxury from the amortized value of techs. deep
  READS the tax rates when valuing tiles (`deephorizon.c:439`) and never
  sets them. No workstream file exists.
- **Trade routes on merit.** Stock prices a caravan on *initial* income,
  undiscounted -- there is no `amortize()` anywhere in `daidomestic.c`
  -- then clamps to 0.1 when caravans pile up, and the fallback ladder
  builds one at want 1 exactly when the city wants nothing. Deep's
  `caravan_discount` is spent on choosing the destination, not on the
  build want. Human call 2026-08-04: merit should amortize the same way
  as everything else.

## 3a. A capability the gate forecloses (human, 2026-08-04)

Transports beat walking on unimproved terrain, and the water gate
refuses same-continent coastal hops along with the dead lakes. That
costs nothing TODAY only because deep's lift model is a continent
inequality (`deepops.c:478`, `:770`, `:909`) and cannot want a hop at
all. W36 records what would have to change together -- the gate
generalising from "reaches another continent" to "shortcuts anything"
(still geometry, still demand-free), the three continent tests becoming
route comparisons, and a break-even that is terrain-dependent and must
be measured in both cells rather than assumed.

## 4. Instrumentation now available

- `deep-hull-order` (`when=choice` / `when=prod`) -- the order event
  with the ordering city's ETA, the best rival ETA after the
  production-switch penalty, and `bestfresh` (no stock). Orders are
  reconstructed offline as consecutive-turn runs; no stored state.
- `deep-wait` -- one line per waiting cross-continent passenger per
  turn, with the hull count that could load it and the producer-city
  counts on its own water and anywhere.
- `benchmark/commission.py RESULTS_DIR...` -- both, plus the wait
  distribution with its censored tail reported rather than dropped.

Both censuses are proven behaviour-neutral: the wait stream is
bit-identical across the two binaries built on 2026-08-04.

## 5. HARNESS rules that cost batches

- **The tuning file must have a `[tuning]` section.** Without it every
  key is ignored, both arms run compiled defaults, and the result key
  still differs so nothing downstream notices. `run.sh` now refuses such
  a file, and any key absent from `tuning-params.ini` (`70acda4798`).
  Prove every off-switch by its execution counter.
- PIN the binary (server + libfreeciv.so to a scratch dir; RUNPATH is
  `$ORIGIN`) and pass `--server`. New pin per binary. /tmp is tmpfs and
  IS wiped between sessions.
- Check DONE counts on the SPECIFIC result dir before analysing and
  before sequencing: `until [ "$(find $dir -name DONE | wc -l)" -ge
  $games ]`. "No freeciv-server running" is not a completion test.
- `pgrep -x`, not `pgrep -f`; `ps` inside the tool sandbox sees an empty
  PID namespace and is not evidence a batch died.
- Background batches are reaped at ~70 minutes (measured, D2 2026-08-05);
  `setsid nohup` dies immediately. Plan long cells as relaunches of
  `run.sh` — the DONE cache makes the resume free.
- **Loading ANY tuning file quantises every double tunable to float**
  (E34.3 2026-08-05: `secfile_lookup_float_default` takes a `float def`,
  so keys the file never mentions shift too; two identity arms differing
  only in file-vs-none diverged 12/20 games). Until the precision fix
  lands (session H1): **compare file-vs-file with identical unrelated
  keys, never file-vs-none.** E34.1/2/2b pinned arms carried this; their
  byte-identity results are unaffected (both arms same treatment), their
  pinned-vs-free deltas have a small quantisation component.
- **Measured 2026-08-04: isolated-150 is ~12 min for a 20-game arm at
  12-way, lategame-80 ~9.** Earlier per-game estimates were inflated by
  probe games competing for cores. Budget from this.
- `meson test -C build` is the suite; `python3 benchmark/tune-audit.py`
  must print CLEAN (108/108/108/108 as of 2026-08-04).
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 -- DIFFERENT
  REGIONS). EBS caches preserved. NEVER terminate.

## 6. METHOD NOTES THAT EARNED THEIR PLACE

- **Size the machinery against the population it would act on, and
  check that the population is not already handling itself.** Three of
  W36's four steps targeted populations that were empty (no-producer),
  self-resolving (noboat spells end in a turn), or already handled by an
  existing mechanism (churn cancels orders).
- **A measurement can be wrong in its own output.** The first award
  number credited a rival with its whole shield stock and made a
  wonder-city look like a free shipyard; the tell was the rival
  advantage tracking the ordering city's own ETA almost exactly.
- **Log at the seam where the decision COMMITS.** Watching production
  saw 229 of 1359 hulls, because a bought hull is ordered and delivered
  inside one turn.
- **A verdict inherits its venue.** 30x between cells, and one cell
  where a working mechanism is worth exactly zero.
- **Read the execution counter before the score.** Four changes have
  measured as "no effect" while never executing.
- **When a change fires on a minority of seeds, report the fired-on
  subset and the tie count.**
- **Twenty seeds are direction; forty are a claim.** Three estimates
  have moved under power (rendezvous +29.90 -> +11.28, slots +22.30 ->
  -13.50, the water gate +4.65 -> +1.82).
- **A mechanism that provably works can still be worth zero.** Five
  times now.
- **Filter before the comparison, not after it.**
