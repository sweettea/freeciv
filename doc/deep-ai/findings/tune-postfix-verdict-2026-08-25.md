# Post-fix tuning campaign: holdout verdict and post-mortem (2026-08-25)

The 2026-08-23..24 `tune-postfix` campaign (systemd unit `deep-tune`,
binary `.tune-postfix-bin` sha `24867bef2364d609`, five gates on,
started from `benchmark/tuning/postfix-start.ini` = compiled defaults +
gates) completed all four stages. **The holdout rejects the tuned
vector.** No part of `s3-mil-r2/best-tuning.ini` should ship.

## Numbers

Training ("block best", each = max over that block's candidates):

| stage | venue | incumbent (gen0 cand0) | block best |
|---|---|---|---|
| s1 MIL | mixed contested, 8 train seeds | **−7** | +104.4 |
| s2 ECON | lategame-80 + isolated-150 blend | **−40.2** (s1 winner) | +28.6 |
| s2 G | same | +28.6 carried | +28.6 (delta +0.0) |
| s3 MIL r2 | mixed contested | **−6** (s2 winner) | +62.5 |

Holdout (`s3-mil-r2/best-tuning.ini`, 20 held-out seeds, paired
all-deep vs all-baseline):

| config | mean paired diff | median | wins | per-seed σ |
|---|---|---|---|---|
| contested | **−52.5** | −61.0 | 6/20 | 115 |
| lategame-80 | **−32.6** | −23.5 | 7/20 | 64 |

The loss is broad (14/20 and 13/20 seeds), not a few blowups.

## Diagnosis: the optimiser selected order-statistic noise

All of this is in cached data; no new games were run.

1. **The objective's noise floor swamps the effect.** Per-seed paired
   σ ≈ 115 (contested) ⇒ an 8-seed training objective has SE ≈ 41.
   A block evaluates 30–40 candidates and takes the **max of the
   means, never re-evaluated** (`tune.py run_block`). The expected
   max of ~40 draws of N(μ, 41) is ≈ μ + 90. Observed: incumbent −7,
   "best" +104.4. The headline training gains are the order statistic
   of noise, nearly exactly.
2. **Cross-stage re-evaluations already showed it.** Each stage's
   gen0 cand0 re-scores the previous winner unmutated: the s1 winner
   (+104.4 on its own venue) scored −40.2 on the econ blend; the s2
   winner (+28.6) scored −6 back on mixed contested — i.e. after two
   full stages the true mixed-war performance was unchanged from the
   start (~−7). The +104/+62 never existed.
3. **The final vector is ~200 random perturbations.** 195 of 269
   params moved from defaults; 76 sit pinned at a bound (60 at LO,
   16 at HI — e.g. `overseas_fear=0`, `wall_fear_weight=0`,
   `upkeep_gold_weight=0`, `floor_border_weight=0`). With mutation
   σ = range/6 and no selection signal, each block appends large
   random moves to ~80 params. The holdout −52/−33 is consistent with
   defaults degraded by accumulated noise, not with a fitted optimum
   that failed to transfer.
4. **Train and holdout also differ in kind for MIL**: training was
   `--mixed` (head-to-head seats in one game), the war holdout is
   paired separate games. Even a real mixed-game gain need not move
   the paired metric. This compounds, but the order-statistic problem
   above is sufficient on its own.

## What is still unknown

**Where defaults+gates stand on the holdout.** The incumbent's ≈ −7 on
mixed training seeds is the only anchor; there is no paired-holdout
number for `postfix-start.ini`. Until that runs, "tuning made it
worse" and "post-fix deep is behind baseline and tuning didn't help"
cannot be separated. The 40 baseline holdout games are cached, so this
costs only 40 deep games (~2 h at 8 jobs). **This is the first action
after the quota reset**, and it gates everything: if defaults+gates
also score ≈ −50, the problem is in the gates or the binary, not the
tuner.

## Tuner fixes required before the next campaign (W25)

- **Never select by unreplicated max.** Re-evaluate the top 2–3 of
  each generation on fresh seeds before accepting, or accept a
  challenger only on a paired test against the incumbent on shared
  seeds. `block best` must be a replicated number.
- **Match free parameters to evaluation budget.** ~40 noisy scalars
  cannot set ~80 params. Tune hand-picked blocks of ≤10 params, or
  raise seeds per candidate by an order of magnitude.
- **Print the SE next to every objective** (σ across seeds / √n) so a
  +48.9 with SE 41 reads as what it is.
- **Log the incumbent every generation** (it is cached and free) so
  drift of the population away from a still-unbeaten incumbent is
  visible live.
- **Sigma = range/6 is a first-generation setting**, not a standing
  one, when the start point is hand-tuned defaults: early generations
  are dominated by candidates far worse than the incumbent (see ECON
  g0: 9 of 10 candidates between −40 and −108).

## Retention

`benchmark/results/tune-postfix/` — ~1400 games, all `game.log`s
zstd-compressed in place (deep-tune-compact.timer), scorelogs intact;
every number above re-derives from `tune.py`'s cache layout plus the
journal of the `deep-tune` unit (`journalctl --user -u deep-tune`).
