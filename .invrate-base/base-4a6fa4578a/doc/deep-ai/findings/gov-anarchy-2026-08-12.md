# The government valuation prices a revolution at zero (2026-08-12)

Diagnosis of G3 (task #61): `gov_valuation = 1` cost **−26.4/seed**
(t −2.92, 16/20 worse) on the lategame-80 holdout while the liveness
read said the machinery executed exactly as designed. Three suspects
were named in STATUS. This is the term-level answer from the 20 real
games already on disk
(`benchmark/results/nightbatch/g3-gov-lategame/`), re-derivable with
`benchmark/govcensus.py`.

**Verdict: suspect 3, revolution timing — but not because the timing
model is mistimed. The anarchy charge is structurally dead, and it is
dead because of a false premise about `H_REVOLUTION` written into
`deepgov.c`'s own comment.** Suspect 1 (upkeep) is a real but second
order bias. Suspect 2's ceiling half is not blind, it is switched off.

---

## 1. What the arms actually did

20 seeds, lategame-80 (techlevel 50, classic ruleset, `hard`, 6 AI
seats, T80). Arm A is `pv2-gov.ini` (`gov_valuation = 1`, every other
gov knob at its default); arm B is the `ai/baseline` control.

Turns spent under each government, as player-turns over all 20 games:

| gov | deep, `gov_valuation=1` | stock `ai/baseline` |
|---|---|---|
| Federation | 4110 (42.3%) | 7307 (**75.0%**) |
| Democracy | 1705 (17.5%) | 550 (5.6%) |
| Monarchy | 1501 (15.4%) | 235 (2.4%) |
| Communism | 1100 (11.3%) | 661 (6.8%) |
| Despotism | 651 (6.7%) | 610 (6.3%) |
| **Anarchy** | **535 (5.5%)** | **358 (3.7%)** |
| Republic | 118 (1.2%) | 28 (0.3%) |

Stock parks in Federation and stays there. Deep is spread across
everything. The signature is not a *bias* toward one government — it
is that deep never settles.

The hypothesis in STATUS was that deep would sit in Monarchy while
stock reached Republic/Federation, which would have convicted the
upkeep charge. **That is not what happened.** Deep spends *three
times* as long as stock in Democracy, the single most upkeep-expensive
government on the board. A valuation biased toward cheap-and-poor
governments does not do that.

## 2. The count that decides it

| | deep `gov_valuation=1` | stock |
|---|---|---|
| government changes per game | **26.8** | 17.8 |
| player-turns of Anarchy per game | **26.8** | 17.9 |
| Anarchy turns per government change | **1.00 ± 0.00** | 1.01 ± 0.02 |

Paired change-count delta **+9.0 per game, t +6.51, 20/20 seeds** —
deep revolves half again as often as stock, in every single seed.

And the middle row is the finding: **every revolution costs exactly one
full turn of empire production under Anarchy, in 20 seeds out of 20,
at `hard`.** Deep charges zero for it. `ancost` is `0.0` on all 8712
candidate lines in the corpus.

## 3. Why the charge is zero, and why that is wrong

`deep_gov_anarchy_turns()` returned `0.0` whenever the player lacked
the `H_REVOLUTION` handicap, on this reasoning (its own comment):

> H_REVOLUTION is set at AWAY, NOVICE/RESTRICTED and EASY only, so at
> the `hard` skill every benchmark cell runs at, this term is exactly
> zero and the government valuation is a pure stream comparison.

The premise is false. `H_REVOLUTION` decides `turns` — the number of
**extra** turns served — not whether the empire passes through anarchy
at all:

- `handle_player_change_government()` sets
  `pplayer->government = game.government_during_revolution`
  **unconditionally** (`server/plrhand.c:615`), after the handicap
  branch at `:592` has already set `turns = 0`.
- It then sets `revolution_finishes = game.info.turn + turns`, i.e.
  the current turn.
- The only call to `update_revolution()` is at **begin** phase
  (`server/srv_main.c:1381`).

So a revolution started during the AI's phase on turn T leaves the
empire under Anarchy for turn T's end-of-turn city production, and it
does not leave until turn T+1 begins. The in-source notification
("will switch at the end of the player phase", `plrhand.c:631`) is
itself misleading about this.

One turn, always, at every difficulty. The savegame's own
`changed_to_times` counter and the scorelog's government tag agree on
it to two decimal places in 40 games.

## 4. How big the un-priced cost is

On the 13 bursts that happened to be evaluated while the player was in
Anarchy, `base` *is* the anarchy output, so the gap to the chosen
government is the cost of the anarchy turn in the same want currency
the decision is made in:

- forgone output **26.6% mean / 28.4% median** of the government's own
  per-turn output — median **850 want**.

Against that, the margin that actually triggers a revolution:

- 780 of 1439 bursts (54.2%) pick a new government;
- median winner-minus-incumbent margin **2439 want**;
- **19.6%** of those revolutions have a total margin smaller than the
  median charge.

Replaying every burst with the charge applied (`1 turn × k × base`):

| k | revolutions | vs shipped |
|---|---|---|
| 0.10 | 741 | −5.0% |
| 0.20 | 681 | −12.7% |
| **0.266 (measured)** | **645** | **−17.3%** |
| 0.35 | 607 | −22.2% |

At the measured value the charge removes about a sixth of deep's
revolutions and moves it from 26.8/game toward stock's 17.8 without
touching the ones that clearly pay. It is proportionate by
construction: the charge scales with `base`, so an obviously-correct
early move out of Despotism (gain 121/turn × 24 = 2904 want against a
247 charge) is untouched, while a marginal one is not.

## 5. Why zero was so damaging: there is no other brake

`adv_best_government()` starts `best_val = 0` and takes any government
scoring above it (`advdata.c:996`, `:1081`). Deep's design deliberately
makes "stay where we are" score exactly zero — which is the right
grammar, but it means **the anarchy charge is the only hysteresis in
the system.** At zero, any positive delta revolts.

And the deltas are noisy. Re-scoring the government the player is
*already under* — which a correct valuation must price at exactly 0 —
gives:

    n 1312 bursts   mean −16.8   median −14.5   sd 52.4   negative 67.1%

per turn of empire output, i.e. **±52/turn of pure measurement noise,
amplified ×24 by the horizon multiplier into ±1250 want.** The noise
comes from `auto_arrange_workers()` being path-dependent: `base` is
computed from the live, converged arrangement, while every candidate
is one pass away from the previous candidate's arrangement. With a
zero-cost revolution, that noise floor alone is enough to revolt on.

This is the mechanism behind the occupancy table in §1. Deep revolts
to Democracy 132 times across the corpus and finishes there once.

## 6. The other two suspects, for the record

**Suspect 1, the upkeep charge — real, second order, not the cause.**
The bill does span 6× across governments as STATUS said (mean charged
per burst: Democracy 215.4, Fundamentalism/Federation 116.8, Republic
97.7, Monarchy 32.3, Communism 29.6). Adding the bill back and
re-ranking changes the burst winner in **187/1452 = 12.9%** of bursts,
106 of them Communism-instead-of-Federation — the right direction for
the hypothesis, and worth revisiting. But it cannot explain deep
spending 3× longer than stock in Democracy, and it is a bias in *which*
government, not in *how often*. The dimensional accounting is sound:
both `base` and `cand` are net of the bill, so the subtraction is
differenced, not double-counted.

**Suspect 2, the dropped effect sum — the ceiling half is simply
off.** `ceil` is `0.0` on all 8712 candidate lines, because
`gov_rate_ceiling_gain` defaults to `0.0` and `pv2-gov.ini` sets only
`gov_valuation = 1`. The G3 arm therefore never tested that term at
all. The genuinely-dropped military effects (veteran chance, HP regen,
bribe cost) remain unmeasured and remain the valuation's stated blind
spot; nothing here indicts or clears them.

There is a live secondary correlate worth naming: deep ends with
**+3.0 corruption** (t +4.08) and **−74.5 gold** (t −5.83) per seat,
which is what spending 42% instead of 75% of the game in Federation —
the classic ruleset's corruption-immune government — buys you.

## 7. The fix, and what still has to be proven

`ai/deep: charge a revolution the turn of anarchy the server always
serves` — `deep_gov_anarchy_turns()` now floors at 1.0 rather than
returning 0.0 when the player has no `H_REVOLUTION` handicap, and at
`MAX(1.0, deep_revolen_expected())` when it does. No new constant:
`gov_anarchy_weight` already exists and already defaults to 1.0
("charge the measured loss at face value"), and the loss itself is
measured, not assumed.

A player already in anarchy needs no special case: `base` is then the
anarchy output, so `MAX(0.0, base - anarchy_out)` in the caller is zero
and the charge vanishes on its own.

Identity at knob 0: `deep_gov_value()` returns to `dai_gov_value()`
before any path that reaches this function, so the change is inert by
construction. Demonstrated anyway — smoke, 10/10 seeds, `--arms deep`,
`TUNING=pv2.ini`, fixed binary vs a `git archive` build of the base
commit `c93649faf4`: `metrics.csv` **byte-identical over 40
player-rows**. `meson test` 5/5 including `baseline-frozen`.

**Nothing here is a result yet.** The fix is a correctness argument
with a term-level measurement behind it, not a score. The gate is the
G3 re-run, unchanged from the original design:

    A: benchmark/tuning/pv2.ini          (gov_valuation = 0)
    B: benchmark/tuning/pv2-gov.ini      (gov_valuation = 1, fixed)
    venue: lategame-80, holdout seeds, 20 paired
    read: B - A per seed; as shipped this was -26.4, t -2.92, 16/20 worse

Deep must at minimum stop losing before `gov_valuation` is a candidate
for any default. The secondary read is the census: government changes
per game should fall from 26.8 toward stock's 17.8, and Federation
occupancy should rise from 42.3%. If the change count falls but the
score does not recover, the residue is suspect 1 and the next GOV
session prices the upkeep bill properly rather than netting it.
