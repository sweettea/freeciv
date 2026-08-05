Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md -- start at "The gate confirmed at power" (the
last section) and read backward as far as you need. Branch
epic/w03-horizon-content.

**Section 0 is the job.** Sections 1-2 are why, and what has already been
falsified so you do not redo it. Anything below section 5 is history kept
for provenance: correct when written, NOT to be followed now.

## 0. THE JOB: size W36 step 0, then decide whether to build it

`doc/deep-ai/workstreams/W36-build-commissioning.md` is drafted and its
**step 0 is a sizing pass that may retire half the file**. Do that
before writing any commissioning code. The frame it belongs to is
`findings/plans-commission.md` (human sketch, 2026-08-04: plans
commission, cities arbitrate, everything amortized).

Three numbers, all from the existing hull census plus one new log line
(the turn an order is placed and the city's shield stock):

1. When a hull was ordered, was there a city on the SAME water with a
   materially shorter ETA? If most orders would land at the same city
   anyway, the award machinery is ceremony and only cancellation and
   the ETA are worth building.
2. How often does a demand find no producer at all -- the `lift-noboat`
   population, 7-10 per 20 games on lategame-80. Measure it on the
   island cell, where hulls are scarce.
3. **The wait distribution**: turns from `deep_lift_wants_boat` true to
   enrolment as a lift's core. §1d has asked for this twice and nobody
   has produced it. Hitching cannot be priced without it.

## 0a. FIRST, the small thing: read the bid-time A/B

`a81d509b4a` (bid-time withdrawal) has a 20-seed isolated-150 A/B that
was launched 2026-08-04 evening: `boat_gate_at_bid` 0 vs 1, gate ON in
both, pinned binary in the session scratchpad (GONE -- tmpfs -- so if
the results dirs are incomplete, re-pin from HEAD and rerun).
Analyse with `benchmark/paircmp.py OFF_DIR ON_DIR`. Cell 3 in STATUS
is the number to read it against; the re-ask arm there carries a
tech-want double-count that the bid-time form does not.

Check the counters first, as always: `deep-watergate-bid` (domestic
withdrawals), `deep-colony` (deep's own bids, should FALL at closed
water), and the backstop `deep-watergate`, **which should read zero.
A nonzero backstop means a third producer of ferry bids exists that
nobody has accounted for** -- that is a finding, not a nuisance.

## 1. What was settled this session, do not redo

- **The useful-water gate is CONFIRMED as a mechanism and FLAT on
  score.** 40 paired seeds, isolated-150: refusals 15.75/game against
  0 (t=+8.69, 40/40), arrivals +4.42 (t=+4.58), cities +0.76
  (t=+2.26), unitsbuilt +5.82 (t=+2.91), score **+1.82 (t=0.67),
  18/40**. The 20-seed +4.65 is RETIRED -- do not quote it.
- **The gate is INERT on lategame-80.** 20 seeds: score -0.07
  (t=-0.16), arrivals -0.35, while firing 10.65/game. Its value lives
  on the island cell or nowhere. Do not average the cells.
- **Refusing at bid time beats vetoing the winner**, and a probe
  proved the difference matters: the traced refusal came from deep's
  own colony want, so the earlier re-ask (`d95aedf67a`) zeroed a
  POSITIVE founder want and gave a Temple (97.6) to a city whose best
  candidate was Settlers (750.1). `a81d509b4a` withdraws both
  producers' bids at formation. `boat_gate_at_bid` replaced
  `boat_gate_second_best`.
- **`benchmark/paircmp.py` exists now.** It also fixes a bias every
  earlier hand-computed verdict may carry: civil-war splinters show up
  as extra seats of the arm they split from and score far below it.
  Filtering to P1..Pn reproduces STATUS's hull-pricing table exactly.

## 2. What NOT to do next, and why

- **Do not build more transport.** Section 2's standing verdict: five
  delivery mechanisms have worked and the aggregate followed none.
  W36 is about placing and cancelling hulls, not adding them.
- **Do not price the fleet COUNT.** Falsified from the aggregate (idle
  share invariant 75-80% across five interventions) and from the hull
  (idleness concentrated where demand is absent).
- **Do not add a demand term to the build GATE.** The geometry test
  works precisely because it does not move with demand; 84% of idle
  hull-turns have no waiting passenger and demand is BURSTY. Demand
  belongs in the AWARD (which city, when), not in the veto.
- **Do not invent a cost model for the remaining idleness.** Its
  honest price is the WAIT a passenger faces without the hull --
  number 3 above.

## 3. Two open leads with no owner

- **Rates.** science/tax/luxury from the amortized value of techs.
  deep READS the tax rates when valuing tiles (`deephorizon.c:439`)
  and never sets them. No workstream file exists.
- **Trade routes on merit.** Stock prices a caravan on *initial*
  income, undiscounted -- there is no `amortize()` anywhere in
  `daidomestic.c` -- then clamps to 0.1 when caravans pile up, and the
  fallback ladder builds one at want 1 exactly when the city wants
  nothing. Deep's `caravan_discount` is spent on choosing the
  destination, not on the build want. Human call 2026-08-04: merit
  should amortize the same way as everything else.

## 4. TUNING is owed

Human call 2026-08-04: tune, regardless of what score says about any
one mechanism. `boat_water_gate`/`boat_gate_at_bid` are 0/1 and not
tunable, but the lift cluster is untuned and now finally binding:
`lift_gather_turns` (re-measure AFTER the manifest -- manifest is 8
now, and the old flat result predates the rendezvous fix),
`lift_hitch_wait_ratio`, `lift_drop_radius`, `boat_idle_damping`, and
`colony_lift_weight`, which is the want the gate filters. Training
seeds via `tune.py`, reported on holdout. It wants the machine to
itself.

## 5. HARNESS rules that cost batches

- **The tuning file must have a `[tuning]` section.** Without it every
  key is ignored, both arms run compiled defaults, and the result key
  still differs so nothing downstream notices. `run.sh` now refuses
  such a file, and any key absent from `tuning-params.ini`
  (`70acda4798`). Prove every off-switch by its execution counter.
- PIN the binary (server + libfreeciv.so to a scratch dir; RUNPATH is
  `$ORIGIN`) and pass `--server`. New pin per binary. /tmp is tmpfs
  and IS wiped between sessions.
- Check DONE counts on the SPECIFIC result dir before analysing and
  before sequencing: `until [ "$(find $dir -name DONE | wc -l)" -ge
  $games ]`. "No freeciv-server running" is not a completion test --
  run.sh dispatches through `xargs -P`.
- `pgrep -x`, not `pgrep -f`; `ps` inside the tool sandbox sees an
  empty PID namespace and is not evidence a batch died.
- **Measured locally 2026-08-04: isolated-150 runs ~13 min/game at
  12-way, so 40 games is ~45 min per arm and lategame-80's 20 games
  took 10.** Earlier estimates of an hour per 20-game arm were
  inflated by probe games competing for cores. Budget from this.
- `meson test -C build` is the suite; `python3 benchmark/tune-audit.py`
  must print CLEAN (108/108/108/108 as of 2026-08-04).
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 -- DIFFERENT
  REGIONS). EBS caches preserved. NEVER terminate.

## 6. METHOD NOTES THAT EARNED THEIR PLACE

- **A verdict inherits its venue.** 30x between cells, and this
  session added a cell where a working mechanism is worth exactly zero.
- **Read the execution counter before the score.** Four changes have
  now measured as "no effect" while never executing.
- **Twenty seeds are direction; forty are a claim.** Three estimates
  have now moved under power (rendezvous +29.90 -> +11.28, slots
  +22.30 -> -13.50, this gate +4.65 -> +1.82).
- **Size a mechanism, not a feature**, and re-derive the ladder when
  the cost model changes rather than re-running it.
- **A mechanism that provably works can still be worth zero.** Five
  times now.
- **Filter before the comparison, not after it.** Vetoing a winner
  forces you to reconstruct a runner-up that was already computed and
  freed -- and the reconstruction chose wrong.
