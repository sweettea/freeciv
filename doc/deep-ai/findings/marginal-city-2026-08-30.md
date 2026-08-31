# What a marginal city is worth, and why four epics could not see it (2026-08-30)

S-MARGINAL is the task
[the frontier verdict](frontier-verdict-2026-08-30.md) §6 closed on, in
its own words:

> **The next epic on this chain should stop adding foundings and go
> find out what a marginal city is worth**, because four independent
> interventions now agree that at the margin it is worth approximately
> zero.

**It is not worth approximately zero.** In the war venue a city founded
in T51–75 is worth **+6.98 ± 0.59 score at T150**, and one founded in
T1–25 is worth **+17.13 ± 0.94**. The four epics moved the city count by
+0.02 to +0.15 per seat. At the measured price that is **+0.1 to +2.2
score points**, against cells whose score SE was **0.85 to 2.15**. Every
one of those cells was built to fail: the effect it was looking for was
one third to one half of its own noise floor before the first game ran.

The conserved-founder-queue reading is not refuted — foundings *were*
moved in time more than added — but the sentence it was carried on is.
**"The marginal city is worth approximately zero" was never measured.
It was inferred from a score column too wide to hold the answer.**

Reader: `benchmark/marginalcity.py`. Corpora: the archived holdouts
`tune-postfix/s5-defaults-{war,econ}` and `s6-nogates-{war,econ}` with
their baseline arms (`w25-screen-corpus/war` is **byte-identical** to
`s6-nogates-war`, 20/20 scorelogs with the `id` line excluded, and is
not counted twice), plus every A/B cell of S-FEARSEAT, S-SITEPLAN,
S-FEARSINK and S-FRONTIER in their agents' worktrees. **All read-only.
No games were run.**

---

## 0. The estimator, and what it can and cannot claim

One observation per **seat** per game. Regress an end-of-game outcome on
the number of cities that seat *founded* in each 25-turn band, with a
fixed effect for **(arm, seed)** — so the map, the ruleset, the turn
count and the opponent mix cancel exactly and only *between-seat
variation inside one game* identifies the coefficient. Cities **taken**
and cities **lost** enter as their own regressors, so a seat that
conquered its empire is not credited to founding. SEs are
cluster-robust on the game.

Three things check the estimator before any of its numbers are used.

1. **The `cities` row.** Running the same regression with the end city
   count as the outcome returns **1.00 ± 0.00 for every band** and
   −0.99 ± 0.01 for `lost`. The event extraction reconstructs the city
   ledger exactly; nothing is double-counted or dropped.
2. **`get_civ_score()` closes.** Score = citizens + 2·techs + 5·wonders
   + units + culture/50 (`server/score.c:352`). The separately-estimated
   citizen, tech and culture coefficients sum back to the score
   coefficient to within **0.16 points at T51–75** (4.45 + 2×1.00 +
   18.4/50 = 6.82 against 6.98), the residual at earlier bands being
   wonders and units — which is where wonders and units belong.
3. **Deep and baseline agree.** Two AIs that choose entirely different
   sites, at entirely different rates, price a same-band city
   identically (§2). A selection artefact would have to be the same
   artefact in both.

**What it is not.** This is an observational coefficient. A seat that
founds three cities in T51–75 is also a seat with more land to found
them on, and the land is not a regressor, so the number is an **upper
bound on the causal value**. §3 supplies the experimental cross-check —
the A/B arms themselves, where the treatment is the instrument — and it
lands in the same range, which is why the upper bound is quoted as a
value at all.

---

## 1. The price list

**contested / war**, gate-off training games from all four epics
(`frontier/war/c00` + `fearsink/war/c00` + `siteplan/war/c00` +
`fearseat/war/off`), **120 games, 941 seats**, outcome at T150:

| founded in | **score** | citizens | techs | culture | sum-check |
|---|---:|---:|---:|---:|---:|
| **T1–25** | **+17.13 ± 0.94** | 7.02 ± 0.27 | 2.58 ± 0.25 | 92.3 ± 16.7 | 14.03 |
| **T26–50** | **+12.44 ± 0.60** | 6.16 ± 0.16 | 1.73 ± 0.13 | 48.4 ± 10.9 | 10.59 |
| **T51–75** | **+6.98 ± 0.59** | 4.45 ± 0.25 | 1.00 ± 0.11 | 18.4 ± 6.8 | **6.82** |
| T76–100 | +5.97 ± 0.43 | 4.83 ± 0.21 | 0.72 ± 0.11 | −1.3 ± 7.2 | 6.24 |
| T101–125 | +5.24 ± 0.55 | 4.00 ± 0.22 | 0.72 ± 0.12 | 6.5 ± 8.9 | 5.57 |
| T126–150 | +5.18 ± 2.29 | 2.92 ± 0.54 | 0.85 ± 0.44 | 21.9 ± 43.5 | 5.06 |
| *taken* | +24.99 ± 1.55 | 3.43 ± 0.55 | 0.99 ± 0.33 | 761 ± 38 | |
| *lost* | −14.16 ± 1.04 | −7.20 ± 0.38 | −2.04 ± 0.33 | −152 ± 23 | |

**lategame-80 / econ**, same construction, **100 games, 604 seats**,
outcome at T80:

| founded in | **score** | citizens | techs | culture |
|---|---:|---:|---:|---:|
| **T1–25** | **+6.13 ± 0.44** | 5.23 ± 0.22 | 0.18 ± 0.03 | −26.8 ± 11.4 |
| **T26–50** | **+4.36 ± 0.48** | 5.35 ± 0.32 | −0.06 ± 0.04 | −45.1 ± 16.2 |
| **T51–75** | **+3.04 ± 0.30** | 2.18 ± 0.22 | 0.16 ± 0.02 | −1.8 ± 11.5 |
| *taken* | +14.31 ± 0.62 | 4.48 ± 0.30 | 0.40 ± 0.05 | 406 ± 18 |
| *lost* | −7.05 ± 0.33 | −5.83 ± 0.20 | −0.15 ± 0.02 | −7.2 ± 12.3 |

**Read.** A city is an annuity and the coefficient is the *remaining
term*. Contested, founding value falls from 17.1 to 7.0 as the founding
slides from the opening to the T51–75 band, then flattens at ~5–6: a
city has about seventy-five turns of compounding available to it and
after that it is simply a city. The econ venue stops at T81, so the
same curve is compressed into a third of the length and the whole scale
is a third of the size — **the two venues do not disagree about what a
city is worth per turn, they disagree about how many turns are left.**

The tech column is where they separate: contested pays 1.00 ± 0.11 techs
(= 2.0 score) for a T51–75 city and lategame-80 pays 0.16. Techs are the
compounding half, and lategame-80 ends before they compound.

**A conquered city is worth +24.99 ± 1.55 and a lost one −14.16 ± 1.04**,
each more than a founded one, because both are partly proxies for
winning and losing a war. They are in the regression to keep them *out*
of the founding coefficients, not to be believed as causal.

### The same table on the held-out corpora

Independent games (seeds 101–120), 20 a corpus, so wider — but the same
numbers, and notably the same numbers for `ai/baseline`:

| corpus | T1–25 | T26–50 | T51–75 | T76–100 |
|---|---:|---:|---:|---:|
| war, deep `s6-nogates` | 15.60 ± 2.67 | 12.26 ± 1.04 | **8.25 ± 2.08** | 3.75 ± 1.31 |
| war, deep `s5-defaults` | 19.23 ± 2.31 | 11.14 ± 1.34 | **8.06 ± 1.28** | 3.98 ± 1.24 |
| war, **`ai/baseline`** | 17.92 ± 1.59 | 11.99 ± 1.06 | **7.87 ± 1.15** | 5.57 ± 1.04 |
| war, training pool (n = 120) | 17.13 ± 0.94 | 12.44 ± 0.60 | **6.98 ± 0.59** | 5.97 ± 0.43 |
| econ, deep `s6-nogates` | 6.49 ± 0.61 | 3.81 ± 1.08 | 3.01 ± 0.46 | — |
| econ, deep `s5-defaults` | 5.95 ± 0.70 | 3.88 ± 0.76 | 3.59 ± 1.19 | — |
| econ, **`ai/baseline`** | 6.89 ± 0.85 | 3.08 ± 0.88 | 2.37 ± 1.07 | — |
| econ, training pool (n = 100) | 6.13 ± 0.44 | 4.36 ± 0.48 | 3.04 ± 0.30 | — |

Eight independent estimates of a T51–75 contested city, over two
tunings, two AIs and two seed pools, spanning 6.98 to 8.25. **The value
of a marginal city is one of the better-determined quantities this
project has measured, and nobody had measured it.**

## 2. Deep and baseline convert a city at the same rate

The per-band coefficients differenced, deep (both tunings, 40 games)
minus baseline (20 games), contested at T150:

| band | T1–25 | T26–50 | T51–75 | T76–100 | T101–125 | T126–150 |
|---|---:|---:|---:|---:|---:|---:|
| deep − base | −0.79 ± 2.46 | −0.11 ± 1.44 | **+0.26 ± 1.75** | −1.56 ± 1.53 | −1.37 ± 2.27 | −10.29 ± 6.52 |

Econ at T80: −0.73 ± 0.98, +1.11 ± 1.06, +0.87 ± 1.23. Every cell is
inside its SE except T126–150, a cohort of 0.075 foundings per seat.

**There is no yield defect.** Given a city founded in the same band,
`ai/deep` turns it into exactly as much score as `ai/baseline` does.
That is the content/military/gold census's *"there is no independent
size, tech or culture defect to find"* re-derived at the level of the
individual city, and it forces the entire deep-vs-baseline gap into the
**count**.

It goes there, and the price list closes it. Deep's missing foundings
per seat, from [the city ledger](city-ledger-2026-08-30.md) §1 and §2,
valued at this table:

    contested:  -0.100(17.13) -0.006(12.44) -0.531(6.98)
                -0.125(5.97)  +0.019(5.24)  +0.025(5.18)   =  -6.01
    observed paired score gap, s6-nogates-war:                -7.05 +/- 2.80

    econ:       -0.508(6.21)  -0.525(4.16)  -0.150(3.41)
                +0.058(0.17)                                =  -5.84
    observed paired score gap, s6-nogates-econ:               -7.33 +/- 2.48

**Eighty to eighty-six per cent of deep's entire score deficit against
`ai/baseline` is the foundings it does not make, priced at the rate
deep's own cities earn.** The crude exchange rate says the same thing in
one number: −7.05 score over −0.643 cities is **10.97 score per city**
in the war venue and **7.21** in the econ one, which is just the
band-weighted average of the table.

## 3. The four epics, re-read at that price

For every A/B arm on this chain, the paired per-band founding delta
times the price list gives a **predicted** score delta, with an SE
propagated from both factors. Beside it, the observed one.

**contested / war**, per seat, value table = the 120-game training pool:

| arm | predicted | observed | obs − pred |
|---|---:|---:|---:|
| `mil_opportunity_gate` (fearseat `price` = fearsink `c10`) | **+2.15 ± 1.28** | −1.05 ± 2.12 | −3.19 ± 2.48 |
| `founder_site_cap`+`join_guard` (fearsink `c01`) | +0.20 ± 1.00 | +2.08 ± 1.82 | +1.88 ± 2.08 |
| the same pair (siteplan `c10`, n = 60) | −0.51 ± 0.59 | +2.36 ± 1.30 | +2.88 ± 1.43 |
| `frontier_transit_gate` (frontier `c10`) | **+0.77 ± 1.13** | +0.12 ± 2.15 | −0.65 ± 2.42 |
| `boat_shadow_gate` | +0.04 ± 0.35 | −0.46 ± 0.85 | −0.49 ± 0.92 |
| fearseat `all` | +1.53 ± 1.19 | +0.62 ± 1.91 | −0.91 ± 2.25 |
| fearseat `ramp` | −1.04 ± 1.09 | −3.35 ± 1.50 | −2.30 ± 1.86 |
| fearseat `evid` | −0.81 ± 1.08 | −2.63 ± 1.37 | −1.82 ± 1.75 |
| cap+guard+price (fearsink `c11`) | +0.98 ± 1.57 | −0.90 ± 1.82 | −1.88 ± 2.40 |
| `founder_run_hold` (siteplan `c01`) | +0.37 ± 0.76 | −0.63 ± 1.42 | −1.00 ± 1.61 |
| siteplan `c11` | +0.17 ± 0.77 | −0.31 ± 1.47 | −0.48 ± 1.66 |
| `founder_floor_gate` (frontier `c01`) | −0.97 ± 0.93 | **−10.17 ± 1.38** | **−9.20 ± 1.67** |
| transit+floor (frontier `c11`) | +1.61 ± 1.22 | **−7.99 ± 1.93** | **−9.60 ± 2.28** |

**lategame-80 / econ**, value table = the 100-game training pool:

| arm | predicted | observed | obs − pred |
|---|---:|---:|---:|
| `mil_opportunity_gate` | +0.28 ± 1.01 | +0.28 ± 1.86 | +0.00 ± 2.12 |
| cap+guard (fearsink `c01`) | **+1.49 ± 0.82** | +2.10 ± 1.19 | +0.61 ± 1.44 |
| cap+guard (siteplan `c10`) | **+1.22 ± 0.52** | +2.02 ± 0.87 | +0.80 ± 1.01 |
| `frontier_transit_gate` | −0.40 ± 0.63 | +1.50 ± 1.23 | +1.90 ± 1.38 |
| `boat_shadow_gate` | +0.43 ± 0.52 | −0.70 ± 0.90 | −1.13 ± 1.04 |
| fearseat `all` | +1.40 ± 1.00 | +3.26 ± 1.54 | +1.86 ± 1.84 |
| fearseat `ramp` | −0.38 ± 0.93 | +1.96 ± 1.27 | +2.33 ± 1.58 |
| fearseat `evid` | −0.16 ± 0.93 | +3.64 ± 1.47 | +3.80 ± 1.74 |
| cap+guard+price (fearsink `c11`) | +1.34 ± 0.93 | +1.52 ± 1.51 | +0.19 ± 1.77 |
| `founder_run_hold` (siteplan `c01`) | −0.18 ± 0.54 | −0.35 ± 1.04 | −0.17 ± 1.17 |
| siteplan `c11` | **+2.15 ± 0.63** | **+2.32 ± 1.12** | **+0.17 ± 1.28** |
| `founder_floor_gate` | −0.63 ± 0.77 | −0.73 ± 1.48 | −0.11 ± 1.67 |
| transit+floor | −0.78 ± 0.88 | −1.03 ± 1.70 | −0.25 ± 1.91 |

### The verdict, arm by arm

**Consistent with the predicted small positive effect, and equally
consistent with zero: `mil_opportunity_gate`, `founder_site_cap` +
`founder_join_guard`, `frontier_transit_gate`, `boat_shadow_gate`.**
Every one of their `obs − pred` residuals is inside 2.1 SE, and every
one of their observed intervals also contains 0. **The cells did not
choose between the two hypotheses. They could not have.** The largest
predicted effect on the board is `mil_opportunity_gate`'s +2.15 ± 1.28
in the war venue, against an observed SE of 2.12: even if that
prediction were exactly right, a 20-seed cell would have called it a
wash about half the time.

**Consistent with neither — the two arms whose channel is not cities:
`founder_floor_gate` and transit+floor.** Predicted −0.97 and +1.61,
observed −10.17 and −7.99, residuals −9.20 ± 1.67 and −9.60 ± 2.28. The
floor gate's damage was never a city-count effect: it is a hoard
(`settlers@end` +0.195 ± 0.060, pop −4.52 ± 0.70), and shields sunk into
settlers that never place are invisible to a founding-band model. This
is a *feature* of the check — the model correctly refuses to explain the
one mechanism that is not about foundings.

**Explained better than expected: the econ venue.** Eleven of thirteen
econ residuals are inside 1.5 SE and the two largest predicted effects
land almost exactly (siteplan `c11` predicted +2.15 ± 0.63, observed
+2.32 ± 1.12; siteplan `c10` +1.22 against +2.02). The mean residual is
+0.47, i.e. the arms did very slightly *better* than their cities
explain — which is the fearseat verdict's own truncation effect (`ramp`
and `evid` buy the first eighty turns and lategame-80 never receives the
bill), appearing exactly where that verdict said it would.

### The experimental cross-check

Treat each arm × venue as an instrument and regress paired d(score) on
paired d(cities@end) through the origin, inverse-variance weighted:

| population | slope, score per city |
|---|---:|
| war, all 13 arms | **+19.6 ± 10.8** |
| war, 11 arms excluding the two founder-floor cells | **+6.9 ± 7.7** |
| econ, all 13 arms | **+4.82 ± 2.09** (t = 2.3) |

The econ slope is the only one with real power, it is **significantly
non-zero**, and it sits on top of the observational econ price list
(6.1 / 4.4 / 3.0 by band). Both war estimates bracket the observational
7–17. Note the two biases run opposite ways: measurement error in the
city delta **attenuates** these slopes toward zero, while the
observational estimate is **inflated** by land quality. They meet in the
middle, which is what should happen if the middle is real.

**The causal value of a marginal city is not zero, and it is close
enough to the observational price list to use the price list.**

## 4. Power: every one of these cells read the wrong meter

Paired per-seed SDs, pooled over the thirteen arms of each venue, per
seat:

| venue | sd(score) | sd(cities@end) |
|---|---:|---:|
| contested | **7.98** | **0.319** |
| lategame-80 | **6.39** | **0.816** |

Paired seeds per cell needed to see an effect at z = 2:

| target | contested | lategame-80 |
|---|---:|---:|
| score +1 | **255** | 164 |
| score +2 | **64** | 41 |
| score +4 | 16 | 11 |
| cities +0.05 | 163 | 1067 |
| cities +0.10 | **41** | 267 |
| cities +0.20 | 11 | 67 |

**The city count is a tighter meter than the score for anything whose
mechanism is expansion.** In the war venue a city is worth 10.41 score
at the observed band mix, so the city meter's score-equivalent SD is
0.319 × 10.41 = **3.32 against the score's 7.98** — a factor 2.4 in SD
and a factor **5.8 in games**. In the econ venue 0.816 × 6.26 = 5.11
against 6.39, a factor 1.25 in SD and 1.6 in games.

That is the methodological finding of this document. Four epics each ran
160–360 games, reported a city effect at t = 2.0 to 5.2, and then
adjudicated on a score column that would have needed **255 games a cell**
to resolve the effect the city column had already found. The ship rule
("clears its SE by a reasonable margin in both venues") was applied to
the noisier of the two available statistics.

### What n the stacked lever needs

If the four mechanisms with a positive city effect and no `unitslost`
cost stack additively, per seat:

| | war | econ |
|---|---:|---:|
| d(cities@end), additive | **+0.219 ± 0.113** | **+0.423 ± 0.373** |
| predicted score, additive | **+3.16 ± 2.01** | **+1.80 ± 1.54** |
| sum of the four observed score deltas | +0.70 ± 3.63 | +3.18 ± 2.68 |
| measured cap × price sub-additivity, on score | −1.83 ± 2.33 | −0.75 ± 2.16 |

So the honest pre-registration is a **range**: +3.2 if additive, +1.6 if
the measured cap × price disarming halves it (war); +1.8 / +0.9 (econ).

| n per cell | SE(score) war | SE(score) econ | SE(cities) war | SE(cities) econ |
|---|---:|---:|---:|---:|
| 20 | 1.78 | 1.43 | 0.071 | 0.182 |
| 60 | 1.03 | 0.82 | 0.041 | 0.105 |
| **100** | **0.80** | **0.64** | **0.032** | **0.082** |

**n = 100** is both what the pessimistic case needs (war, half-additive:
1.58 / 0.80 = z 2.0) and the largest training pool that exists —
`benchmark/seeds/train.txt` is seeds 1–100, and 101–200 are holdout. At
n = 100 the *city* endpoint is decided at z ≥ 3.4 (war) and z ≥ 2.6
(econ) even under half additivity, so the cell answers its mechanism
question whatever the score does.

## 5. The stacked-lever cell

`benchmark/stacked-lever.sh`. **Two cells × two venues × 100 TRAINING
seeds = 400 games**, plus 3 identity. **Written, NOT LAUNCHED.**

    off   every gate at its compiled 0             = today's binary
    on    mil_opportunity_gate                     S-FEARSEAT
          founder_site_cap_gate                    S-SITEPLAN
          founder_join_guard_gate                  S-SITEPLAN
          frontier_transit_gate                    S-FRONTIER
          boat_shadow_gate                         S-FEARSEAT

Every constant stays at its **PLACEHOLDER** (rule 7):
`mil_opportunity_price` 1.0, `founder_site_cap_slack` 1.0,
`founder_join_guard_mult` 1.0, `frontier_transit_price` 1.0,
`frontier_transit_stance_floor` 0, `boat_shadow_distance` 1.0,
`boat_shadow_count` 1. A sweep on top of an unadjudicated stack would be
unreadable, which is the reason S-FEARSINK gave for the same choice.

**Why these five and not the others.** Each has a city effect that
cleared its SE in at least one venue with `unitslost` flat or falling in
both: `mil_opportunity` cities@50 +0.188 ± 0.064 (war, t 2.9) and
+0.392 ± 0.167 (econ, t 2.3); cap+guard cities@T60 +0.37 ± 0.08 (econ,
t 4.7); `frontier_transit` cities@75 +0.147 ± 0.059 (war, t 2.5) with
`unitslost`@75 −0.690 ± 0.231; `boat_shadow` cities@end +0.142 ± 0.073
(econ) with `unitslost` down in both venues and twice measured inert on
score. **`founder_floor_gate` is excluded**: −10.17 ± 1.38, 0 wins in
20, and §3 shows its damage is not a city channel at all.
**`founder_run_hold` is excluded**: its score is −0.63 and −0.35, and
its war-venue interaction with cap+guard cost about 16 points
(siteplan §4).

**Interaction warnings, carried in the header rather than discovered
afterwards.** S-FEARSINK §4 measured the cap and the price *partially
disarming each other*: with the cap on, `milopp_fired` falls
−41.70 ± 9.80 a game (t −4.25) and `cap_fired` rises +22.55 ± 8.55,
because the cap withdraws the founder bid the price would have charged
against. The transit gate raises the founder population, which raises
the number of searches, which raises the occasions on which the cap
binds — untested, sign unknown. This cell therefore measures **the
corner, not the factorial**: it answers "is the stack worth shipping",
not "which gate did it". A five-cell factorial at n = 100 is 1000 games
and is not affordable; if the corner clears, the decomposition is the
next cell and it will be worth its games. If the corner is a wash *and*
the city delta is also flat, the additive model is wrong and §3's
arithmetic is what needs re-reading.

**Pre-registered, before the run.**

1. **Primary endpoint: cities per seat at the venue's end turn.**
   Expected +0.219 ± 0.113 (war) and +0.423 ± 0.373 (econ) if additive,
   half that if the cap × price disarming carries. At n = 100 that is
   z ≥ 3.4 / 2.6 in the pessimistic case. If the city delta is inside
   its SE, the stack is a null on its own mechanism and the score does
   not need reading.
2. **Secondary endpoint: score per seat.** Expected +3.2 / +1.8
   additive, +1.6 / +0.9 half-additive, against SE 0.80 / 0.64.
3. **Falsification: `unitslost`.** All five gates had it flat or falling
   individually. A rise at T75 clearing its SE in either venue means the
   stack bought its foundings with losses, and the corner is void
   regardless of what the score did.
4. **What a null looks like:** cities@end inside its SE in both venues;
   or a score delta smaller than half the additive prediction *with* the
   city delta at full size — which would mean cities stopped converting
   at the stacked margin, and would be a new finding rather than this
   one.
5. **Identity.** The `off` arm at compiled defaults must reproduce the
   archived `s6-nogates-war` scorelogs byte for byte (`id` excluded),
   with vacuity guards: the binary must emit each gate's census line and
   none of them may have fired.

**Ship rule, unchanged and now affordable.** A gate ships at 1 only if
the corner clears its SE by a reasonable margin in **both** venues. At
n = 100 that rule is finally being applied to a cell that can satisfy
it.

## 6. Verdict

> **A marginal city is worth +6.98 ± 0.59 score at T150 when founded in
> T51–75 and +17.13 ± 0.94 when founded in T1–25** (contested, 120
> games, 941 seats), and +3.04 ± 0.30 / +6.13 ± 0.44 at T80 in
> lategame-80. Eight independent estimates across two tunings, two AIs
> and two seed pools put the contested T51–75 figure between 6.98 and
> 8.25. The citizen, tech and culture coefficients sum back through
> `get_civ_score()` to within 0.16.

> **`ai/deep` converts a same-band city into score exactly as well as
> `ai/baseline` does** — every per-band difference is inside its SE — so
> the whole deep-vs-baseline gap is the count, and the count at this
> price explains **86% of the contested score deficit** (−6.01 predicted
> against −7.05 ± 2.80 observed) and **80% of the econ one**.

> **The four expansion epics' score results are consistent with the
> small positive effect their city counts imply AND with zero, and
> cannot separate the two.** Predicted deltas are +0.04 to +2.15 points;
> observed SEs are 0.85 to 2.15. The only arms the model fails are the
> two founder-floor cells, whose damage runs through hoarded settlers
> rather than through cities — which is the model behaving correctly.

> **Every one of those cells adjudicated on the noisier of its two
> statistics.** Score needs 255 games a cell to resolve +1 point in the
> war venue; the city count resolves the same effect in 41. The city
> meter is 2.4× tighter in SD and **5.8× cheaper in games** for an
> expansion lever.

> **The conserved founder queue is not refuted and is not the whole
> story.** Foundings really were moved in time more than added — the
> cumulative deltas at T75 are +0.06 to +0.15, not +0.19 — but "worth
> approximately zero at the margin" was a statement about the *price*,
> and the price is 7 to 17.

## 7. What to do next, in order

1. **Run `benchmark/stacked-lever.sh` at n = 100.** 400 games. It is the
   only design on the board that puts the four surviving mechanisms
   above the noise floor of the venue they are judged in.
2. **Adopt cities-per-seat as the primary endpoint for every expansion
   cell**, with score secondary, and say so in the cell's header. It
   costs nothing and is worth a factor of six in games. It does *not*
   generalise to military or economy levers, whose score channel is not
   cities; for those the score remains the only sufficient statistic.
3. **Re-read the shelved gates against their predicted effects rather
   than against zero.** `boat_shadow_gate` is predicted +0.04 ± 0.35
   (war) and +0.43 ± 0.52 (econ): it is a correctness fix that was never
   going to move the score, and it should be adjudicated on `unitslost`
   and on the 81.2% of firings it deletes, not on a column that cannot
   see it.
4. **The T1–25 band is where the money is** — 17.13 a city against
   6.98 — and nothing on this chain has moved it: the four epics' T1–25
   founding deltas run −0.008 to +0.075. `ai/deep` founds 1.575 cities a
   seat in T1–25 against baseline's 1.675, and that −0.100 ± 0.065 is
   worth −1.71 score. It is the cheapest *unattempted* city on the
   board.
5. **Not a supply problem, still.** Nothing here reopens the settler,
   site or join questions; all three were answered and the answers
   stand. What changed is the exchange rate they were converted at.

## 8. One correction to the record

The frontier verdict's closing line — *"four independent interventions
now agree that at the margin it is worth approximately zero"* — should
read: *four independent interventions moved the city count by an amount
whose score consequence was three to ten times smaller than the SE of
the column they were read in.* The interventions agree about the
foundings. None of them measured a value, and the value is not small.
