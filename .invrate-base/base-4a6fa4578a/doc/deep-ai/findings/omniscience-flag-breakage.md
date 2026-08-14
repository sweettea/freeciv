# What honest information actually breaks — the runtime census

Epic S5 phase 2. The deliverable
`sessions/S-omniscience-flag.md` item 3 asked for and
`findings/omniscience-census-2026-08-11.md` §9 left QUEUED: the flag was
built, proven inert at 0, and **had never been executed non-zero**. It has
now been executed in **84 games**, and this document is what happened
against what the static census predicted.

Branch `epic/s5-phase2`, cut from `epic/w03-horizon-content` @ `046c662b5d`.
One binary for every game (sha `3b99ff36…`, this worktree's `build/`), one
config per cell, one seed list per cell. **No code was changed.** The only
new file besides this one is `benchmark/tuning/s5-honest.ini`.

Arms, in every cell:

| arm | tuning file | difference |
|---|---|---|
| **OFF** | `benchmark/tuning/principled-v1.ini` | — |
| **ON** | `benchmark/tuning/s5-honest.ini` | `honest_info = 1`, and nothing else |

`diff` of the two files is one knob line plus the comment header. The
result key differs only in `tuning=` (`key.txt` in each result dir), so
every number below is the handicap bits and nothing else.

Results under `benchmark/results/s5-phase2/`:
`contested-99b0d4a4cf0f` (OFF) / `contested-68d491f535db` (ON),
`isolated-150-3f8161a8b1b2` (OFF) / `isolated-150-4a8b3833d310` (ON).

---

## 0. Headline

**1. The switch executed, and the proof is unambiguous.** 252
`deep-honest` lines across the ON games, **every one of them
`turn=1 imposed=3 map=1 fog=1 targets=1`**, exactly once per deep seat per
game (8×20 contested + 4×10 isolated + 4×12 mixed + 4 smoke = 252). Zero
in every OFF game. In the mixed cells only the four deep seats emit it, so
the imposition is deep-only as designed. The edge-triggered counter did the
job it was added for.

**2. Nothing crashed.** 84 games, two cells, three arm shapes, zero
assertions, zero segfaults, zero timeouts, zero pairing violations. Deep
does not fall over when blinded; it plays a different, smaller game.

**3. The A/B delta is not a collapse, and in one cell it is positive.**

| cell | seats | seeds | score OFF → ON | delta (se, t) |
|---|---|---|---|---|
| `contested` (8p, 32×32, T150) | all deep | 20 | 83.17 → 81.02 | **−2.15** (2.65, −0.81) |
| `isolated-150` (4p, 56×56 islands, T150) | all deep | 10 | 176.70 → 190.10 | **+13.40** (7.93, +1.69) |
| `contested` mixed vs `baseline` | 4 deep / 4 base | 6×2 mirrors | gap +11.19 → +1.83 | **−9.35** (6.15, −1.52) |

Score in an all-deep cell is nearly useless here: every seat is blinded at
once, so the handicap cancels. **The head-to-head cell is the one that
prices honesty**, and it says deep's lead over `ai/baseline` falls by
roughly 80% — with the land going straight to the sighted opponent
(baseline seats gain +0.85 cities, t +5.36, **6/6**, and +7375 landarea,
t +4.76, **6/6**, in the very same games).

**4. Expansion is what breaks, and the biggest single casualty is not the
site gate — it is the naval pathfinders.** Lifts formed per game: contested
12.55 → 8.35 (t −2.76); isolated **30.5 → 2.1, a 93% collapse**
(t −7.68), with ferry-wait settler stalls 162.9 → 3.4. On the island cell
the home-continent expansion tempo is **untouched** (turn to 2nd city 19.98
→ 20.50, to 4th 37.85 → 38.27, both n.s.) while total cities fall 40%.
Honest deep settles its own island on schedule and then never crosses
water again.

**5. Every *dangerous* prediction failed to appear, and the census's own
headline #2 says why.** Melts 2.35 → 2.55 (n.s.), melt-blocks ~0 in both,
cities lost 0.17 → 0.19 (n.s.), eliminations **down** (delplayer 0.20 →
0.00, t −2.18). The wave census is ungated, and it measurably kept
working: `deep-wave n_att` 3.43 → 3.98, `n_def` 2.25 → 2.26. **The 148
ungated reads are a safety net that the flag cannot remove, so the measured
breakage is a strict lower bound** — exactly as census §6 warned, now with
numbers.

**6. The one mechanism written for the honest world is inert.**
`deepexplore.c:176`'s discovery term woke up on cue — `unknown` 0 → 37.97
per line (t +6.77, **20/20**) — and bought nothing: explorers alive 0.78 →
0.77 (n.s.), mean explore want 0.09 → 2.13 against a turn-1 settler want of
**640**. On the island cell it goes the *wrong way*: honest info removes
omniscient hut sighting (`huts_vis` 0.18 → 0.02) faster than the discovery
term replaces it, and explore want **falls** 1.52 → 0.29 (t −2.24).

---

## 1. Execution evidence

```
$ grep -rh deep-honest benchmark/results/s5-phase2 | grep -o 'turn=[0-9]*' | sort | uniq -c
    252 turn=1
$ grep -rho 'imposed=3 map=1 fog=1 targets=1' … | wc -l
    252
$ grep -rc deep-honest …/contested-99b0d4a4cf0f …/isolated-150-3f8161a8b1b2   # OFF
      0
```

Smoke game first, before any batch:
`results/s5-phase2/smoke-on/seed-1/game.log` — four seats, all three bits,
turn 1, game ran to `DONE`. The 40-game batch lost in 2026-08-04 to an
unverified switch is why this was done first, and the counter is why it
took one grep.

---

## 2. Cell 1 — `contested`, 8 deep seats, 20 paired seeds

`benchmark/run.sh --count 20 --arms deep --config contested.conf --jobs 2`,
both arms, same binary. `paircmp.py`, deep seats only:

| metric | OFF | ON | delta | se | t | wins |
|---|---|---|---|---|---|---|
| score | 83.17 | 81.02 | −2.15 | 2.65 | −0.81 | 9/20 |
| **cities** | 4.66 | 4.18 | **−0.47** | 0.12 | **−3.84** | 5/20 |
| **landarea** | 50775 | 47594 | **−3181** | 1024 | **−3.11** | 5/20 |
| pop | 25.52 | 23.79 | −1.74 | 0.99 | −1.76 | 5/20 |
| mean_city_size | 5.54 | 5.91 | +0.37 | 0.21 | +1.77 | 13/20 |
| munits | 14.31 | 13.96 | −0.35 | 0.66 | −0.53 | 10/20 |
| unitslost | 15.66 | 15.53 | −0.13 | 2.11 | −0.06 | 10/20 |
| unitskilled | 10.90 | 10.96 | +0.06 | 1.72 | +0.04 | 10/20 |
| techs | 18.98 | 18.77 | −0.21 | 0.81 | −0.26 | 8/20 |

Mechanism streams (per game, all deep seats; `deep-*` counters):

| stream | OFF | ON | delta | t | wins |
|---|---|---|---|---|---|
| settler site picks (`deep-settler … pick`) | 58.60 | 51.60 | −7.00 | −2.74 | 6/20 |
| settler missions | 337.45 | 299.25 | −38.20 | −2.48 | 6/20 |
| stalls "arrived" | 78.45 | 70.80 | −7.65 | −2.87 | 4/20 |
| **turn of 4th city** | 95.18 | 103.91 | **+8.72** | **+2.60** | 15/20 |
| turn of 2nd city | 40.81 | 36.62 | −4.19 | −1.57 | 5/20 |
| **lifts formed** | 12.55 | 8.35 | **−4.20** | **−2.76** | 5/20 |
| lifts arrived | 7.10 | 4.85 | −2.25 | −2.47 | 6/20 |
| `deep-op … lift` | 12.10 | 7.50 | −4.60 | −3.15 | 5/20 |
| ops formed | 10.20 | 11.70 | +1.50 | +0.53 | 11/20 |
| ops aborted | 7.80 | 9.90 | +2.10 | +0.96 | 11/20 |
| melts (`deep-rotate`) | 2.35 | 2.55 | +0.20 | +0.27 | 9/20 |
| melt blocks | 0.05 | 0.00 | −0.05 | −1.00 | 0/20 |
| worker flees | 284.50 | 345.35 | +60.85 | +1.31 | 14/20 |
| cities lost (Σ negative deltas of scorelog tag 3) | 0.17 | 0.19 | +0.01 | +0.31 | 6/20 |
| seats eliminated (`delplayer`) | 0.20 | 0.00 | −0.20 | −2.18 | 0/20 |
| `deep-wave n_att` | 3.43 | 3.98 | +0.55 | +1.21 | 11/20 |
| `deep-wave n_def` | 2.25 | 2.26 | +0.01 | +0.10 | 9/20 |
| `deep-explore unknown` | 0.00 | 37.97 | +37.97 | +6.77 | 20/20 |
| `deep-explore want` | 0.09 | 2.13 | +2.03 | +3.79 | 19/20 |
| explorers alive | 0.78 | 0.77 | −0.02 | −0.41 | 11/20 |

The shape: **10% fewer cities, 6% less land, the same army, the same
losses, the same wars, and no extra dead cities.** The 2nd city arrives
*earlier* and the 4th nearly nine turns *later* — the frontier deflates
inward exactly as census §4 said it would, and the near-centre site
survives the deflation.

At 10 seeds (the first half of this batch) ops formed read +6.00 (t +2.06)
and aborted +5.30 (t +2.14). At 20 seeds both fall to noise. Recorded
because it is a live example of rule 6: a 10-seed movement in a small
counter is not a result.

---

## 3. Cell 2 — `isolated-150`, 4 deep seats, 10 paired seeds

The island cell is where the flag bites, and where score inverts.

| metric | OFF | ON | delta | se | t | wins |
|---|---|---|---|---|---|---|
| score | 176.70 | 190.10 | **+13.40** | 7.93 | +1.69 | 6/10 |
| **cities** | 14.32 | 8.57 | **−5.75** | 0.86 | **−6.68** | 0/10 |
| **landarea** | 185575 | 109025 | **−76550** | 10187 | **−7.51** | 0/10 |
| settlers alive | 2.35 | 0.30 | −2.05 | 0.49 | −4.21 | 1/10 |
| **mean_city_size** | 6.57 | 9.20 | **+2.63** | 0.32 | **+8.34** | 10/10 |
| **wonders** | 1.32 | 2.80 | **+1.48** | 0.23 | **+6.30** | 10/10 |
| techs | 27.40 | 30.23 | +2.83 | 1.32 | +2.15 | 7/10 |
| pop | 85.08 | 76.90 | −8.18 | 4.03 | −2.03 | 2/10 |
| unitsbuilt | 87.50 | 56.30 | −31.20 | 4.64 | −6.72 | 0/10 |
| munits | 22.82 | 14.50 | −8.32 | 2.40 | −3.47 | 3/10 |
| unitslost | 6.78 | 1.02 | −5.75 | 1.24 | −4.63 | 0/10 |

| stream | OFF | ON | delta | t |
|---|---|---|---|---|
| **lifts formed** | 30.50 | 2.10 | **−28.40** | **−7.68** |
| lifts arrived | 18.90 | 1.80 | −17.10 | −6.48 |
| settler stalls waiting on a ferry | 162.90 | 3.40 | −159.50 | −5.07 |
| settler site picks | 107.10 | 49.40 | −57.70 | −7.49 |
| **turn of 2nd city** | 19.98 | 20.50 | +0.53 | +0.73 |
| **turn of 4th city** | 37.85 | 38.27 | +0.42 | +0.56 |
| ops formed | 5.60 | 0.80 | −4.80 | −4.71 |
| `deep-wave` lines | 1234.80 | 69.50 | −1165.30 | −3.22 |
| `deep-kill` lines | 1521.30 | 78.10 | −1443.20 | −5.96 |
| cities lost | 5.97 | 3.12 | −2.85 | −6.83 |
| explore want | 1.52 | 0.29 | −1.23 | −2.24 |
| huts visible (per line) | 0.18 | 0.02 | −0.16 | — |

Read it as one causal chain: the H_MAP-gated lift and ferry pathfinders
(`deepops.c:1404`, `:1715`, the ten in `deepsettler.c`) cannot plot a route
across unknown ocean → **overseas settlement stops dead** → half the
cities, 41% of the land → no contact → no wars (waves −94%, kills −95%,
city losses −48%) → the shields that would have been hulls and garrisons
go into buildings → **more wonders, more techs, much bigger cities, and a
higher score.**

The score rise is not honest play being good. It is this venue rewarding
tall play, and deep's omniscient wide expansion here being worth less than
it costs. That is a real, separable finding — deep may be over-expanding on
`isolated-150` — and it is **not** in scope to act on it here. It is
recorded so nobody reads "+13.40" as "honesty is free".

---

## 4. Cell 3 — head-to-head, the number that matters

`contested`, `--arms mixed_db,mixed_bd`, 6 seeds × 2 mirrors, both
tunings. Deep seats get `honest_info`; baseline seats are untouched by
deep's tuning file and are the control.

**Gap = deep-seat mean − baseline-seat mean, in the same game:**

| metric | OFF gap | ON gap | delta | se | t | wins |
|---|---|---|---|---|---|---|
| **score** | +11.19 | +1.83 | **−9.35** | 6.15 | −1.52 | 2/6 |
| **cities** | +0.19 | −1.17 | **−1.35** | 0.34 | **−4.03** | 0/6 |
| **pop** | +3.67 | −4.10 | **−7.77** | 1.57 | **−4.93** | 0/6 |
| **landarea** | +5708 | −6542 | **−12250** | 3214 | **−3.81** | 0/6 |
| techs | +1.62 | −0.15 | −1.77 | 0.89 | −1.98 | 1/6 |
| munits | +3.38 | +1.98 | −1.40 | 2.01 | −0.69 | 2/6 |

Absolute, same games:

| | deep OFF → ON | baseline OFF → ON |
|---|---|---|
| score | 92.88 → 92.10 (−0.77) | 81.69 → **90.27 (+8.58, t +2.59, 5/6)** |
| cities | 4.77 → 4.27 (−0.50, t −2.24) | 4.58 → **5.44 (+0.85, t +5.36, 6/6)** |
| landarea | 53292 → 48417 (−4875, t −2.79) | 47583 → **54958 (+7375, t +4.76, 6/6)** |
| pop | 29.00 → 26.31 (−2.69, t −2.26) | 25.33 → **30.42 (+5.08, t +6.74, 6/6)** |
| unitslost | 12.17 → **14.69 (+2.52, t +2.01, 5/6)** | 19.27 → 16.29 (−2.98) |

This is the finding the all-deep cells cannot show. **Blinding deep does
not destroy deep; it transfers deep's land to whoever can still see.** The
baseline seats did not change one line of code or one knob between the two
arms and gained +8.58 score, and the deep seats' absolute score barely
moved. Deep's *lead* is what evaporates.

The one place danger amnesia shows at all is here: honest deep loses 21%
more units (t +2.01, 5/6) against a sighted opponent, while losing fewer
against equally-blind peers.

---

## 5. Prediction scorecard

Against `findings/omniscience-census-2026-08-11.md` §4 and
`honest-information.md` §4.

| prediction | verdict | evidence |
|---|---|---|
| **"Settling stops"** → census refined it to *monotone frontier deflation, strongest at the frontier* | **CONFIRMED, in the refined form** | contested: cities −10% (t −3.84), 4th city +8.7 turns (t +2.60) while 2nd city is *earlier*; isolated: cities −40% but home-island tempo unchanged (2nd 19.98 → 20.50 n.s.). Deflation is spatial, not a stop. |
| **Frontier deflation is worst where sites are far** | **CONFIRMED and sharpened: "far" means *across water*** | the deflation localises entirely at the water's edge — ferry stalls −98%, lifts −93%, land-neighbour tempo unmoved |
| **"Danger amnesia" — settlers walk back into danger, cities fall to unseen attackers** | **NOT OBSERVED at outcome level** | cities lost +0.01 (n.s.); eliminations 0.20 → 0.00; unitslost n.s. in the symmetric cells. Only the asymmetric cell shows it, and mildly: deep unitslost +2.52 (t +2.01) |
| **"Targeting starvation"** (falsified statically for deep) | **STAYS FALSIFIED** | ops formed 10.20 → 11.70 in contested (n.s.). The isolated collapse (5.60 → 0.80) is *no contact*, not *no target*: waves −94% |
| **"Active self-harm rather than stalls" — unseen wave = 0 → garrison melts, empty-looking garrison → assaults everywhere** | **NOT REACHABLE BY THE FLAG, therefore untested** | melts 2.35 → 2.55 (n.s.), melt-blocks ~0, `deep-wave n_att` 3.43 → 3.98 and `n_def` 2.25 → 2.26 — the survival census is ungated and measurably unchanged. The prediction is about the *knowledge layer*, and only a `known_force` model can test it |
| **Explorer discovery term "returns in full the moment deep is honest"** (`deepexplore.c:163-175`) | **FIRES, BUYS NOTHING** | `unknown` 0 → 37.97 (t +6.77, 20/20), want 0.09 → 2.13, explorers 0.78 → 0.77. On islands it is net-negative: hut sighting lost faster than discovery gained, want 1.52 → 0.29 |
| **Flag breakage is a lower bound** (census §6) | **CONFIRMED, and larger than it reads** | the ungated wave keeps the defensive layer intact; ~80% of deep's competitive lead is nonetheless gone in the head-to-head |

---

## 6. Severity ranking — what actually mattered

The census counted **148 ungated reads** and **70 gated sites** and was
explicit that they are disjoint sets. Worth restating, because the phrase
"rank the 148" invites a category error: **the flag closes gated sites
only. This experiment ranks those, and ranks the 148 only by inference —
from which predicted failures did *not* happen because an ungated read
silently covered for the missing knowledge.**

**Ranked by measured damage (gated sites — what the flag closed):**

1. **The naval/ferry pathfinders — `deepops.c:1404`, `deepops.c:1715`, and
   the ten `parameter.omniscience` sites in `deepsettler.c`.** By far the
   largest effect in the run: lifts −93% (t −7.68) and ferry stalls −98% on
   islands, lifts −38% (t −3.15) even on a land map. The census listed
   these as ordinary rows in the gate table; they are the single most
   load-bearing thing the flag touches. A pathfinder that refuses unknown
   tiles does not degrade, it *disconnects the map*.
2. **The site gate — `deepsite.c:526/589-598` and `:988-992`.** Real and
   significant (cities −10%, land −6%, 4th city +8.7 turns on the land
   cell) and exactly the mechanism §4 described, but second in magnitude,
   and locally survivable: near-centre sites still score.
3. **`deepexplore.c:176`, the discovery term.** Third by *importance*,
   last by *effect*: it is the designed compensator and it is inert at pv1
   magnitudes. A compensator that never buys a unit is a compensator that
   does not exist.
4. **`server/advisors` + stock's ~42 forwarded gates.** No isolable
   signature in any counter stream. Whatever they did, it is inside the
   noise of the above.
5. **`deepsettler.c:144-148` (the module's only `H_FOG` gate) and
   `deepdefence.c:476` (free-land share).** No measurable outcome. Worker
   flees +21% in contested (t +1.31, n.s.); nothing downstream moved.

**Ranked by *latent* severity (ungated reads — what the flag could not
touch, ordered by how much they carried the AI):**

1. **`deep_city_survival_core()` (`deepcombat.c:349-394`), 43 consumers.**
   Top of the list, and the run proves it by *absence*: wave sizes and
   defender counts are statistically identical between arms, and every
   defensive failure the census predicted therefore failed to appear. This
   is the load-bearing wall. Closing it without a sightings model first is
   the one change that could turn a −9 score gap into a rout.
2. **Enemy `->worth` and the take-odds economy** (`deepops.c:127/2620/3495/4127`,
   `deepcombat.c:597/616-623`, `deepdefence.c:4844/5067`). Invisible in this
   experiment — `opodds.def_n` 2.44 → 2.36, odds unmoved. Latent, not
   benign: they are why ops still priced targets correctly while blind.
3. **`deepsite.c`'s live tile reads** (yields, resources, owners, and
   `tile_worked` sitting *beside* the handicap clause at `:591`). The
   measured deflation is what remains **after** these leak the real value
   of every ever-seen tile. An honest site evaluator will deflate further.
4. **`deepwonder.c:198-220` (rival production) and `deepdiplomacy.c`'s
   score/love reads.** Untested here; the isolated cell's wonder count
   *doubled* under the flag, so the wonder race ran on omniscient rival ETAs
   in a world where deep could not see the rivals at all — an internally
   incoherent state nobody would notice from the outside.

---

## 7. Sequencing recommendation for when honest-info work opens

The evidence reorders census §7. Its item ordering (`known_force` first,
`known_city` second, priors third) is right about *size*, but the run says
something about *order of operations* that the static read could not:

**A. Nothing about honesty ships before the pathfinders can cross unknown
ground.** This is the whole ballgame and it is not in census §7 at all.
`H_MAP` on a pathfinder does not make it cautious, it makes the far side of
the fog *non-existent*, and deep's entire overseas game runs through two
`deepops.c` calls and ten `deepsettler.c` calls. The fix is a passability
*policy* for unknown tiles (traverse at a cost, with the cost a tunable),
not a knowledge structure — and it is cheap, local, and independently
benchmarkable today.

**B. Priors over unknown tiles (`honest-information` §5.4) land with, not
after, any honest site evaluation.** Confirmed by measurement:
`deepsite.c:589-598` has nowhere to put an expected value, so honest
expansion deflates monotonically. Ship A and B together or expansion
regresses in exactly the way this run measured.

**C. Recalibrate the explore economy in the same epic, or the compensator
stays dead.** Two concrete defects the run exposes: the discovery term's
magnitude is ~2 orders below the wants it competes with (want 2 vs settler
want 640), and the hut term reads *live* hut extras, so turning honesty on
*removes* explore want on hut-rich maps (0.18 → 0.02 visible huts, want
1.52 → 0.29). Fix the hut read to remembered extras and re-tune
`explore_discovery_value` against these cells. This is the cheapest item on
the list and it gates whether any of the rest is self-correcting.

**D. `known_city` next** (census §7 item 2) — still the cheapest large
unblock, still mechanical, and now also the prerequisite for the wonder
race and the site pressure/contest reads to stop being incoherent.

**E. `known_force` last, and never partially.** The run's clearest
structural message: the ungated wave is what kept deep alive while blind.
The moment sightings replace it, every defensive prediction in census §4
becomes live at once across 43 call sites. Build sightings + decay +
staleness projection (census §7 items 1 and 5) as **one** shipped unit, and
re-run this exact A/B as the regression gate.

**F. Measure with the head-to-head cell, not the symmetric one.** Method
finding, and it changes what future S5 work should report: the all-deep
cells said −2.15 (n.s.) and *+13.40*; the mixed cell said the lead fell
from +11.19 to +1.83 with the land moving to the sighted opponent. Blinding
everyone measures almost nothing. `--arms mixed_db,mixed_bd` on
`contested`, both mirrors, is the venue for every honest-info number from
here on.

**G. Keep `honest_info` at 0 everywhere** — unchanged from STATUS's
standing note, and now with a price attached: −9.35 head-to-head score and
−1.35 cities against a sighted rival.

---

## 8. What this run did not do

- **No `smoke` / `isolated-150` byte-identity proof of the OFF state.**
  Still pending from phase 1 (`STATUS.md`, "The identity gate"). This
  session's OFF arm is `principled-v1` on this worktree's binary, which is
  a *paired control*, not an identity proof against `063a86bdf7`.
- **No `tech50war-200` or other war-heavy venue.** `contested` is 8 seats
  on 32×32; unknown tiles are largely resolved by t76 (`deep-explore
  unknown` 120 → 11 by turn band), so it understates blindness. A large-map
  war cell would price the take-odds reads that stayed invisible here.
- **Nothing was fixed, and no constant was invented.** Per the brief and
  CLAUDE.md rule 7, the two magnitudes this run indicts
  (`explore_discovery_value`, and the absent unknown-tile passability cost)
  are named as tuning work, not given values.
- **The Tier-3 stock layer stayed omniscient throughout** (`dai_war_desire`,
  `daitech.c`, `daidata.c` — 0 handicap checks). Every number here is a
  lower bound on what a genuinely honest deep would suffer.

## The vision economy (human, 2026-08-12) — required alongside any honest-info fold

Once visibility is real, vision is a produced good and must be priced.
Four producers the want machinery must credit, none of which exist
today because omniscience made their product worthless:

a) **Explorers earn their vision.** The explore want currently prices
   huts (+ a discovery term measured 2 orders too small). Under honest
   info, tiles revealed and kept-current is the product — and the S5p2
   census showed the compensator is dead without this (explorers 0.78
   -> 0.77 while blind).
b) **Worldmap/seamap trades and shared vision, priced accurately.**
   Diplomacy can buy sight (map exchanges, embassies as score/tech
   sight, shared vision). Shared vision is ONE-DIRECTIONAL (human,
   2026-08-12): usually exchanged bidirectionally, but not always —
   so it prices as two separate legs. GIVING vision is an intel
   concession (they see your realm: your gaps, your musters, your
   undefended coast) and belongs in the gift/love currency; RECEIVING
   is the benefit, worth more the worse your own coverage is — and it
   partially substitutes for scouting, so item (a)'s explorer want
   must net out vision already received. Accepting a bidirectional
   swap is NOT automatically a wash: the legs differ with relative
   map knowledge and posture. Revoking a given leg is a tell —
   cancellation timing interacts with war planning. Cross-border
   throughout: the §11.4 mixed-arm rule applies, and the
   ally-directed give is exactly the "wash as alliance benefit"
   case.
c) **Boats see.** Naval units as pickets/scouts — hull positioning and
   idle-hull disposition should carry a vision term (the lift/ferry
   pool doubles as a radar fence). Interacts with MOVETYPE honest
   naval pricing.
d) **Vision-platform units (AWACS-likes).** Large-vision-radius units
   are worthless under omniscience and load-bearing without it; their
   build want needs the vision credit or they will never be built —
   the same never-built trap the air venue just exposed for Bombers.

Sequencing: these are fold-companions, not fixes — they belong in the
same epoch as (A) pathfinder passability and (B) honest site eval from
the existing roadmap, because turning on honest info without paying
vision producers locks in the blindness the flag creates.
