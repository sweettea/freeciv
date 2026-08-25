# Production hysteresis at the auction: built, measured, rejected (2026-08-25)

`prod_keep_gate` is hypothesis 1 of
[the defaults-deficit census](defaults-deficit-census-2026-08-25.md)
implemented and adjudicated. **It does not work, and the census says
why: 82% of the abandonments it targets never reach the comparison it
acts on.** The gate ships at 0 and should stay there.

Cell: `benchmark/prodkeep-ab.sh`, contested.conf, **20 training seeds**,
pinned binary `.pin-bin/fix` (`a759ecc5e7478928`), ports 7800-7849,
this worktree only. Results in `benchmark/results/prodkeep/`.

## The mechanism

`deep_prod_keep_credit()` (deepdefence.c), at the merge seam where the
domestic and military candidates for one city-turn both exist and
neither has been committed:

    want *= 1 + prod_keep_gain * min(stock, cost) / cost

applied to whichever candidate IS what the city is already building,
after the class scale, the endgame bell and both shield-equivalent
debits. Emergency tier exempt; the credited want is clamped below
`DAI_WANT_BELOW_MIL_EMERGENCY` so the bias cannot manufacture an
emergency.

Why this and not `switch_cost_price`, which already exists: that debit
is the shields `city_change_production_penalty()` really forfeits, and a
unit->unit switch forfeits **nothing** — the ruleset keeps the stock
inside a production class, which is the switch a settler is usually lost
to. What the pre-empted settler loses is completion TIME, which nothing
in the auction priced.

`prod_keep_gain = 1.0` is a **PLACEHOLDER** and was never tuned. The
verdict below is a verdict on the mechanism's REACH, which no gain
value changes (see §3), not on that number.

## 1. Identity: the gate is inert at its default

10 smoke seeds, pre-epic binary vs this branch, `prod_keep_gate = 0`:
**10 identical, 0 different** scorelogs, byte for byte. Guards passed:
the base binary emits no `keep=` (so the proof cannot pass by running
the fix twice), the fix binary does, and no `deep-prodkeep` line exists
with the gate off.

## 2. Score: a wash, slightly negative

20 training seeds, paired against the same baseline games:

| arm | mean diff vs baseline | se | median | wins |
|---|---|---|---|---|
| off (postfix-start.ini) | −59.2 | 26.3 | −41.5 | 7/20 |
| on (+ `prod_keep_gate=1`) | −63.0 | 26.1 | −42.5 | 6/20 |
| hold (+ `founder_run_hold=1`, diagnostic) | −66.2 | 18.3 | −33.5 | 5/20 |

Paired arm contrasts (same seeds, same maps, baseline cancels):

- **on − off: −3.8, se 9.1, t −0.42, 8/20 seeds better.**
- hold − off: −7.0, se 16.1, t −0.43, 10/20 seeds better.

Neither moves score. (The off arm's −59.2 on training seeds is
consistent with the published −41.8 holdout figure for defaults+gates;
different seeds, same order.)

## 3. The census, and the correction it needed

**Method correction first.** The original census counted every
`deep-prodswitch` away from Settlers as an abandonment. It is not: a
DELIVERED settler also ends its run and the city then picks something
else, writing the same line. `builtlast=` (`city_built_last_turn()`) is
already on the line and separates them. Corrected, gate off, 20 games:

| bucket | starts | built | abandoned | built% |
|---|---|---|---|---|
| T0+ | 320 | 24 | 144 | 14.3% |
| T25+ | 516 | 91 | 421 | 17.8% |
| T50+ | 367 | 112 | 286 | 28.1% |
| T75+ | 211 | 105 | 212 | 33.1% |
| TOTAL | 1587 | 391 | 1188 | **24.8%** |

The leak is real — three settler runs in four end without a unit — but
"starts ≈ abandonments" overstated it by counting deliveries as losses.

**With the gate on, that number does not move at all:**

| arm | starts | built | abandoned | built% |
|---|---|---|---|---|
| off | 1587 | 391 | 1188 | 24.8% |
| on | 1612 | 398 | 1210 | **24.8%** |
| hold | 1468 | 445 | 1004 | **30.7%** |

## 4. Why: the bias never reaches the decision

The mechanism is live, not dormant — 43 204 credits applied across the
20 gate-on games, median progress 0.43 at the moment of credit, 10 273
of them to Settlers at a mean credit of 239 want (the founder bid is
clamped to 750.075 by `DAI_WANT_DOMESTIC_MAX`, so the credit lifts it to
the 999 ceiling and no further).

But of the 1 608 part-built settlers abandoned in those games:

| how the run ended | n |
|---|---|
| credited by the bias and pre-empted anyway | 236 (15%) |
| **no credit despite shields in the stock** | **1 315 (82%)** |
| no credit, empty stock (nothing to protect) | 57 (3%) |

`keep=0` with `stock>0` means the settler **was not one of the two
candidates in that city-turn's auction**. A credit can only be applied
to a candidate; when the domestic advisor stops proposing Settlers — or
when the emergency/war-footing skip suppresses the advisor call
entirely — there is nothing to credit and the city switches away with
no comparison made. What those 1 315 switched to:

    Diplomat 369, Workers 177, Phalanx 151, Trireme 147,
    Barracks 129, Colosseum 85, City Walls 62, Chariot 62

**This is the finding.** Hysteresis at the want comparison cannot fix a
pipeline whose leak is upstream of the comparison. The bid disappears
before the auction, or the auction is skipped.

## 5. The diagnostic arm

`founder_run_hold` (landed 2026-08-22, commit `46d4cb9ca9`, **0 in the
standing config**) declines the switch itself rather than repricing it,
at the seam the skip goes through. On the same 20 seeds it is the arm
that MOVES THE MECHANISM: delivery rate 24.8% → 30.7% overall, 25.9% →
47.3% at T100+, 37.3% → 68.8% at T125+, on 119 fewer settler starts.

And the score is still −7.0 ± 16.1. So the pipeline can be made to
deliver more settlers without the score following — which is either
§8-of-`00-context` ambiguity (the settlers were being replaced anyway,
or arrive too late to found), or evidence that the expansion stall is
downstream of settler DELIVERY (escort, travel, site legality) rather
than at production. That is the next thing to instrument, and it is a
different question from this epic's.

This arm is one key apart from `off` and is a **diagnostic, not an
adjudication** of `founder_run_hold`; it wants its own cell.

## 6. What ships

- `prod_keep_gate = 0`, `prod_keep_gain = 1.0` (PLACEHOLDER, untuned,
  unreachable while the gate is 0). Do not tune the gain against this
  result: the mechanism's reach, not its magnitude, is what failed.
- The `deep-prodkeep` line and `keep=` on `deep-prodswitch` stay: they
  are what made §4 readable, and they cost nothing with the gate off.
- The corrected `built%` census (`prodkeep-report.py --census`) is the
  metric any future expansion-pipeline work should be read on.

## 7. Retention

`benchmark/results/prodkeep/{identity,baseline,off,on,hold}` — 20 smoke
identity games (logs zstd-compressed) and 80 contested games, saves
dropped, `game.log`s filtered to `deep-prodswitch`, `deep-prodkeep`,
`deep-want` and `deep-runhold`. Every table above re-derives with
`benchmark/prodkeep-report.py`.
