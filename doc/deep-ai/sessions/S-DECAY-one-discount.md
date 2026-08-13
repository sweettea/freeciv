# Session brief — DECAY: one discount function, with a measured rate

Human design conversation, 2026-08-13. Origin: "we are measuring value
over a fixed timescale but maybe an exponentially dropping off benefit
over time is better?" — plus the human's own counterpoint, which is a
design constraint below: "downside of exponential value: how do we
decay it? that needs to be a tunable. rank flips are dependent
thereon."

## The inconsistency, located

The want system is a hybrid of two time models:

- DELAY is exponential everywhere: `amortize(v, d) = v * (1-1/MORT)^d`
  (advtools.h:19, MORT 24, half-life ~17 turns). Every wait, march,
  lift and commissioning discount already uses it; deepsite.c:212 even
  states "Turns are not bounded by a horizon constant."
- DURATION is a rectangle: deephorizon.c's file comment — "Every
  credit in this file counts a per-turn value across MORT turns and
  discounts once for the delay." A b/turn stream is worth b*24, flat,
  hard zero after.

The rectangle is not rate-free: a hard window at 24 turns IS a decay
function (a step), with its timescale inherited from MORT and frozen —
untunable because it is a shape, not a knob. Unifying on
`sum b(t) * q^t` makes delay and duration one formula (delay = leading
zeros), makes the endgame clamp a closed-form geometric tail, and
turns "how myopic are we" into a parameter that can be tuned at all.

For a CONSTANT perpetual stream the swap is nearly a no-op: rectangle
b*24 vs exponential perpetuity b*(MORT-1) = b*23 — a uniform 4%. All
the signal is in time-SHAPED credits (growth chains, capacity steps,
back-loaded payoffs), which is what slice 1 measures.

## The human's constraint: rank flips depend on the rate

A census that logs "rectangle value vs exponential value at one rate"
bakes in the rate choice and answers nothing. Therefore:

**Log shapes, not values.** The census line per horizon credit carries
the stream shape the code already has in hand — per-turn amount,
start delay, instalment count/spacing (the b, ttg, steps variables).
Rank flips are then computed OFFLINE as a function of q from one
corpus pass: a flip-fraction-vs-rate curve. It answers, in one pass:

1. which q reproduces the rectangle's ranking (the behaviour-neutral
   anchor);
2. whether flips are sparse-and-extreme (shape is a 4% constant, STOP
   — the retune is not worth buying) or dense near plausible rates
   (the choice is load-bearing and deserves gradient);
3. which credit classes flip (localizes the modelling claim).

## Measured anchors (2026-08-13, extractable and extracted)

An exponential rate decomposes into two measurable hazards:
`rate ~ g + hazard` — the empire's own output growth rate (a shield
reinvested compounds at g) plus the per-turn risk the paying asset is
lost. Both were read from the existing corpus (98 scorelogs: muster-ab
3x32 mixed tech50war-200 + derbies 4/5/6; tags 2 and 3; hazard is a
net-downtick lower bound). `benchmark/decay-anchors.py` reproduces:

    phase     |  g deep  g stock | hz deep hz stock | rate deep rate stock
    T  1-50   |  0.0362  0.0360  | 0.0005  0.0017   |  0.0367    0.0377
    T 51-100  |  0.0028  0.0048  | 0.0018  0.0023   |  0.0047    0.0071
    T101-150  |  0.0009  0.0007  | 0.0013  0.0010   |  0.0022    0.0017
    T151-200  |  0.0002 -0.0009  | 0.0015  0.0011   |  0.0017    0.0002
    T201-300  | -0.0054 -0.0035  | 0.0063  0.0025   |  0.0009   -0.0010

    stock 1/MORT = 0.0417/turn (half-life 17 turns)
    measured overall ~0.011/turn (half-life ~63 turns)

**The headline: MORT's rate is right for the first 50 turns and then
10-20x too myopic.** The measured world's rate collapses an order of
magnitude after T100 while the discount stays fixed — the era-collapse
myopia, quantified from our own games. This also says a single
constant rate is wrong as a shape; the honest knob family is a rate
with era/phase structure (or one anchored to the LIVE measured g +
hazard, which the empire can compute about itself every turn).

Caveats recorded: g is realized growth (war destruction included), not
pure investment return; hazard is a net lower bound (simultaneous
found+loss cancels); the T201-300 row is derby-only (3 games).

## Slices

1. **Shape census** (read-only, byte-neutral): a `deep-decay` log line
   per horizon credit with (producer, b, delay, steps, spacing);
   offline flip-curve analysis per §2. STOP if flips are sparse.
2. **The switch**: `horizon_decay_rate` knob (0 = rectangle,
   byte-identity 10/10; >0 = exponential duration at that rate).
   Default 0. The measured anchors define the SEARCH RANGE, not a
   compiled constant (rule 7: the default is off, and the anchor
   numbers are derived, not invented).
3. **Live anchor variant**: `horizon_decay_live` — rate from the
   empire's own trailing g + hazard (both cheap from data the AI
   already has), so the discount tracks the phase structure the table
   shows. Own knob, own arm; slice 2 is the control.
4. **Endgame clamp**: re-express W03's `endgame_horizon` clamp as the
   closed-form truncated geometric sum; identity at rectangle mode.

## Guards

- Byte-identity at defaults, all slices, 10/10.
- **This is a global rescale of every horizon credit and invalidates
  tuned weights above it.** Schedule: AFTER the lean-lattice A/B
  banks; lands immediately BEFORE the extensive-tuning finale (#8) so
  the retune is bought once. Do not land mid-campaign.
- Gradient only exists on venues long enough for time-shape to matter:
  tech50war-200 and longer. The 80-150 canon is structurally blind to
  this knob (same blindness that hid the era collapse) — a null there
  is not evidence.
- No invented constants: rate knobs default off; anchors are measured;
  the flip-curve, not an argument, decides whether slice 2 is bought.

## Relations

- W01/want_t: orthogonal. This fixes the TIME axis inside deep's own
  currency; want_t is the CROSS-currency exchange. Neither blocks the
  other.
- R1 rates: `rates_science_gain`/`rates_luxury_gain` bridge the same
  seam; a live-anchored rate (slice 3) would feed them too.
- ENDGAME (#32) owns the bell; slice 4 only re-expresses its clamp.
- The era-collapse arc (derby-4/5/6, tech50-collapse findings): this
  brief is the pricing-side response — the world's discount rate
  falls as the game matures and deep's does not.
