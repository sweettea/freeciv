# The knob screen and the truncated-venue check (2026-08-30)

Two offline questions, both answered from games already paid for. **No
new games were run for this document.**

1. **Which of the 277 tunables can move a decision at all?** The
   post-fix campaign mutated ~80 at once and selected order-statistic
   noise (`findings/tune-postfix-verdict-2026-08-25.md`). Before the
   next campaign picks parameters, we need to know which ones a
   parameter search could even in principle fit.
2. **Would a T100-truncated venue be an acceptable proxy?** Cheaper
   games buy more seeds; the question is whether the truncated gap
   still ranks candidates the way the final gap does, and whether the
   saving is real.

Tools: `benchmark/knobscreen.py` (new, this session) and
`benchmark/turncorr.py` (extended for tune.py's campaign layout).

---

## Part 1 — the zero-game sensitivity screen

### Method

For every knob, take tune.py's own mutation step, **σ = range/6**
(`tune.py mutate()`), and ask whether moving the knob by ±1σ changes a
decision **that a log line already contains every input of**. Base
values come from the game's own `deep-tuning` line, so the screen is
taken around the values the corpus actually ran under, not around a
remembered default.

A **flip** is a change in a *discrete* outcome the log itself reports —
a verdict, an auction winner, a gate, an integer count. It is a
necessary condition for a knob to matter, not an effect size. A knob
that flips nothing in 40 games cannot be fit from 8 noisy seeds; a
knob that flips a lot may still be worthless.

Twelve replay rules were written by reading the emitting function in
`ai/deep` and are listed in `knobscreen.py`'s header. Ten carry a
**self-check** — the base-value replay compared against the outcome the
same line recorded — and seven of those had decisions to check in this
corpus (the other three sit on mechanisms that never ran; see the
dead-path section):

| replay rule | decisions checked | agreement |
|---|---|---|
| `auction_domestic` (founder transform leg) | 50 892 | **100.0%** |
| `building_anchor` (per-improvement credit) | 93 172 | **100.0%** |
| `expansion_gate` | 4 189 | **100.0%** |
| `founder_pool` | 4 140 | 99.9% |
| `garrison_floor` | 7 115 | 99.9% |
| `op_formation` | 19 852 | **100.0%** |
| `switch_cost` | 54 326 | **100.0%** |

These are not "the model looks right". They are the reconstruction
reproducing the server's own recorded answer on ~230 000 decisions.
The two 99.9%s are 5 and 9 lines out of thousands, both at exact
integer-truncation boundaries.

### Corpus

`benchmark/results/tune-postfix/s6-nogates-{war,econ}/holdout-report`
— 40 games, binary sha `24867bef2364d609`, `tuning=defaults.ini`
(i.e. compiled defaults, every gate as compiled), contested.conf and
lategame-80.conf, 20 holdout seeds each. 4.45 M `deep-` log lines,
116 distinct tags.

Replicated on `s5-defaults-*` (same 40 seeds, `postfix-start.ini` =
defaults + the five adjudicated gates). The shortlist's **membership is
identical**, no entry moves more than two places, and the seven largest
entries agree within 7%. Three smaller entries move more —
`horizon_capacity_weight` 11.2 → 7.7 flips/game, `floor_border_weight`
53.3 → 62.2, `floor_sea_weight` 63.4 → 55.2 — which is the size of the
between-arm variation to expect on a 40-game read, and is a reason to
treat the ranking *within* the shortlist as indicative rather than
ordered.

### The shortlist — knobs that provably flip decisions

Ranked by flips per game. `flip rate` is flips ÷ decisions the rule
touched, which is the scale-free number.

| knob | default | σ | decisions/game | flips/game | flip rate | replay rule |
|---|---|---|---|---|---|---|
| `switch_cost_gain` | 1.0 | 0.483 | 1358 | **274.7** | 20.2% | switch_cost |
| `founder_tier_gate` * | 0 | — | 3571 | **256.9** | 7.2% | auction_merge |
| `scale_settler` | 1.0 | 0.330 | 7141 | **114.2** | 1.6% | auction_domestic + merge |
| `garrison_floor_danger` | 40 | 33.3 | 178 | **113.1** | 63.5% | garrison_floor |
| `garrison_floor` * | 1 | — | 178 | **113.0** | 63.5% | garrison_floor |
| `upkeep_gold_weight` | 0.125 | 0.208 | 2329 | **93.2** | 4.0% | building_anchor |
| `floor_sea_weight` | 1.0 | 0.5 | 178 | **63.4** | 35.6% | garrison_floor |
| `floor_border_weight` | 1.0 | 0.5 | 178 | **53.3** | 30.0% | garrison_floor |
| `scale_worker` | 1.0 | 0.492 | 7141 | **51.6** | 0.7% | auction_domestic + merge |
| `expansion_war_survival` | 0.9 | 0.167 | 105 | **15.3** | 14.6% | expansion_gate |
| `horizon_capacity_weight` | 1.0 | 0.667 | 2329 | **11.2** | 0.5% | building_anchor |
| `founder_pool_per_site` | 0.06 | 0.083 | 104 | **9.9** | 9.6% | founder_pool |
| `op_take_target` | 0.7 | 0.158 | 496 | **4.9** | 1.0% | op_formation |
| `floor_barb_weight` | 1.0 | 0.5 | 178 | **0.6** | 0.3% | garrison_floor |

`*` — `founder_tier_gate` and `garrison_floor` are 0/1-style keys whose
σ step lands on the other setting, so their "flip" count is what the
*other mode* would change, not a tuning gradient. They belong in a
gate A/B, not in a parameter search. `founder_tier_gate` off→on would
change 7.2% of all city-turn merges — a large change, and one that
**has since been adjudicated**: an 80-game 2×2 (two seat orderings ×
20 seeds) came back null on the pre-registered cities gap in both
orderings (Δ −0.35, t −0.20 and Δ −1.15, t −0.51 at T201) and on
score. The gate stays at 0. A high flip rate is a necessary condition
for a knob to matter and this is what it looks like when the
condition is met and the effect is not there; the screen's claim is a
claim about reachability, never about value.

### Inert under replay — three knobs that provably cannot move anything

| knob | decisions/game | why |
|---|---|---|
| `emergency_value_cap` | 3571 | The merge is tier-first. A domestic want is clamped to `DAI_WANT_DOMESTIC_MAX` = 750.075 and is always NORMAL tier; a military want in the EMERGENCY tier therefore wins **whatever the cap does to its value**. In 143 k logged merges, no ±1σ move of the cap changes one. The cap can only reorder *military candidates against each other*, and that comparison is not logged. |
| `expiry_building_gain` | 2329 | Over 93 k city-turns of credited improvements the obsolescence ETA `ob` is ≥ 0 rarely enough, and the `keep` factor small enough, that no ±1σ move changes the building anchor's argmax. |
| `floor_air_weight` | 178 | `wair` is 0.000 on every logged floor line in this corpus — no rival has air, so the class is never live and the knob multiplies a zero. |

`floor_barb_weight` is on the shortlist only nominally: 0.3% flip rate,
and its `wbarb` component is also usually dominated.

### The inert set — 166 knobs that cannot be shown to move anything

| class | count | meaning |
|---|---|---|
| **dead-path** | **92** | The instrument exists in `ai/deep` and **its tag never appeared in 40 games**. The mechanism did not run. |
| **not-logged** | 54 | Nothing in `ai/deep` evidences the knob's arithmetic at the metric log level (knobcensus `NO-INSTRUMENT`). |
| partial, never fired | 20 | An instrument exists, its tag appears, and the knob's own predicate never fired. |
| mode | 28 | Switch or degenerate range: pinned per experiment, never optimised. |
| partial, fires | 64 | The mechanism runs; the log carries one side of the comparison, not both. Screenable only by experiment. |
| reconstructible | 17 | 14 flip something, 3 do not. |
| unmapped | 2 | `weak_prey_gate`, `weak_prey_gain` — landed after `knobcensus.py`'s map. |

**92 + 54 + 20 = 166 of 277 knobs (60%) could not have contributed
anything to the post-fix campaign's objective.** They were mutated
anyway: the campaign moved 195 of 269 parameters and 76 ended pinned at
a bound. Most of that motion was applied to code that never executed.

The dead-path list is dominated by whole subsystems compiled off at
defaults — the trunk/road family (10 knobs), the chatty family (7),
the trade and wonder merit pair (6), spy (3), rates (3), gov (3), the
gift family (5), `endgame_*` (4), the three military `scale_*` keys,
`unit_upkeep_charge`, `prod_keep_gain`/`prod_keep_gate`,
`peace_seek_gain`, `capital_stake_*`, `site_improvable_gate`,
`site_starve_price_gate`, `poach_marginal_gate`. The full list is
`knobscreen.py`'s own output.

Two of those deserve naming because the screen has a *rule* for them
and the rule found nothing to replay: `unit_upkeep_charge` /
`prod_keep_gain` / `endgame_unfinished_gain` / `peace_seek_gain` each
have a validated replay waiting, and **their log tag never appeared**,
so the mechanism is off, not insensitive. Turn the gate on and the
screen prices them the same afternoon, at zero game cost.

### Tier B — the mechanism runs but the log cannot price the flip

These knobs' instruments fire; the log records the mechanism executing
but not both sides of the comparison the knob enters, so a flip is not
computable. Ranked by instrument firings per game (top of a 64-entry
list; `knobscreen.py` prints all of them):

| knob | fires/game | | knob | fires/game |
|---|---|---|---|---|
| `garrison_menace_slope` | 3306 | | `survival_target` | 1086 |
| `menace_overseas_weight` | 3306 | | `survival_horizon` | 1086 |
| `satisfied_fear_discount` | 2427 | | `settler_fearfulness_mult` | 892 |
| `conquest_worth_mult` | 2389 | | `lift_op_slots` | 841 |
| `invasion_reinforce_rate` | 2387 | | `explore_explorer_claim` | 698 |
| `attack_value_weight` | 2384 | | `invasion_embark_turns` | 641 |
| `attack_loss_gain` | 2384 | | `survival_fear_value` | 607 |
| `invasion_reinforce_slope` | 2348 | | `colony_lift_weight` | 457 |
| `horizon_growth_weight` | 2323 | | `worker_flee_radius` | 427 |
| `horizon_bundle_weight` | 2323 | | `base_fort_gain` | 371 |
| `horizon_content_weight` | 2323 | | `site_naval_weight` | 367 |
| `horizon_multiplier_weight` | 2323 | | `barb_size_ref_cities` | 156 |

A firing rate is weaker evidence than a flip rate and must not be read
as one: `horizon_growth_weight` fires on every credited improvement
because `raw` is a sum and its components are not logged separately.
The fix is cheap and is the single highest-leverage instrumentation
change this document recommends — **log the four horizon credit
components separately on `deep-decay-sum`**, exactly as `capadd`
already is, and four ECON knobs move from tier B to a measured flip
rate without a single new game.

---

## Part 2 — is a T100 venue an acceptable proxy?

### Method

`turncorr.py --layout tune --cost` over the whole
`benchmark/results/tune-postfix` tree: **2 200 paired games** across
five venues, both tune.py shapes (paired deep-vs-baseline dirs, and
`--mixed` seat-mirrored games where the gap is the within-game
deep-side minus baseline-side total). Per game the score gap is taken
at T60/T80/T100/T120 with the same last-known-carry semantics
tune.py's `final_score()` uses, and correlated against the final gap.

### Correlations

The clean readings are the three holdout arms: one tuning vector, 20
seeds, so all the variance is seed variance — the noise a tuner
actually fights.

**contested.conf, T150, paired separate games (n = 20 each)**

| arm | T | r | ρ | sign agree | σ(gap at T) |
|---|---|---|---|---|---|
| s4 tuned | 60 | +0.65 | +0.71 | 45% | 39.6 |
| | 80 | +0.82 | +0.87 | 65% | 70.2 |
| | **100** | **+0.86** | **+0.90** | **70%** | 80.1 |
| | 120 | +0.92 | +0.96 | 85% | 86.8 |
| s5 defaults+gates | 60 | +0.59 | +0.62 | 55% | 24.3 |
| | 80 | +0.72 | +0.79 | 60% | 50.4 |
| | **100** | **+0.77** | **+0.82** | **85%** | 64.8 |
| | 120 | +0.86 | +0.87 | 85% | 72.7 |
| s6 defaults | 60 | +0.68 | +0.74 | 60% | 28.2 |
| | 80 | +0.69 | +0.79 | 60% | 49.7 |
| | **100** | **+0.77** | **+0.82** | **75%** | 61.1 |
| | 120 | +0.86 | +0.87 | 80% | 67.6 |

σ(final gap) is 115.3 / 105.4 / 94.9 in the three arms — the verdict
doc's σ ≈ 115 reproduces.

**Large-n venues (candidates pooled with seeds; the between-candidate
variance is small, see the verdict doc, so these are mostly seed
variance too)**

| venue | end | n | T100 r | T100 ρ | sign | T120 r |
|---|---|---|---|---|---|---|
| contested mixed (s1) | 151 | 640 | **+0.93** | +0.92 | 85% | +0.97 |
| contested mixed (s3) | 151 | 480 | **+0.92** | +0.90 | 81% | +0.97 |
| isolated-150 (s2 cfg1) | 151 | 480 | **+0.76** | +0.72 | 69% | +0.87 |
| lategame-80 (s2 cfg0) | 81 | 480 | T60: +0.88 | +0.76 | 71% | — |

lategame-80 already ends at T80, so the only truncation available
there is T60, and it is poor in the clean arms (r = 0.36 / 0.61 / 0.56
at n = 20). **Reject T60 everywhere.**

### The cost side, and why it is a bracket

Per-game wall clock is recoverable from the save mtimes (T1 autosave to
final save): median **192 s** across 2 200 games, 176–224 s by venue.
What is *not* recoverable offline is how that clock is spread across
turns — no game in this corpus ended early (every contested game ran to
exactly T151), and there are no intermediate saves.

Two models bound it. **Turn-linear** (every turn costs the same) gives
share(T100) = 0.66 on a 151-turn game. **Work-weighted** — cumulative
units + cities from the scorelog, the load the AI phases actually carry
— gives 0.42. The work model's own credential is weak: regressed
against measured seconds it scores r = +0.47 to +0.62 within the
20-game venues, +0.09 to +0.47 in the large ones, and −0.02 pooled,
because contended 8-way `nice 10` wall clock is mostly scheduler noise
and fixed per-turn overhead. So the tool reports a **bracket** and
never a point estimate.

Surrogate efficiency versus the full-length venue is r² ÷ clock share —
how much precision per second the truncated objective buys:

| venue | T | r² | clock share | efficiency vs full |
|---|---|---|---|---|
| contested (s6 defaults) | 100 | 0.59 | 0.42–0.66 | **0.90 – 1.41** |
| contested (s5) | 100 | 0.59 | 0.42–0.66 | 0.90 – 1.41 |
| contested (s4 tuned) | 100 | 0.74 | 0.43–0.66 | 1.12 – 1.73 |
| contested mixed (s1) | 100 | 0.86 | 0.40–0.66 | 1.29 – 2.12 |
| contested | 120 | 0.74 | 0.63–0.79 | 0.93 – 1.18 |
| isolated-150 | 100 | 0.58 | 0.32–0.66 | 0.87 – 1.81 |
| lategame-80 | 60 | 0.31 | 0.59–0.74 | 0.42 – 0.63 |

### Answer

**T100 is an acceptable ranking proxy on the 150-turn contested venue,
and it is not worth adopting.**

- *Acceptable*: ρ = 0.82–0.90 and 70–85% sign agreement on held-out
  seeds; +0.92/+0.93 on the two 480–640-game mixed venues. A T100 gap
  ranks candidates the way the final gap does, most of the time.
- *Not worth adopting*: the efficiency bracket is **0.90–1.41** on the
  venue that matters. It straddles 1.0. The data cannot establish that
  a T100 venue is better than a full one, and it does establish that it
  is not dramatically better. Truncation buys at most ~40% more games
  per hour of information, against a campaign whose failure mode was
  selecting the max of 40 unreplicated noisy draws — a defect that more
  games of the same objective do not fix.
- **T60 is rejected** on every venue (r ≤ 0.74, sign agreement 45–75%,
  and on lategame-80 the efficiency is *below* 1.0 outright).
- T120 is nearly information-free to adopt (r 0.86–0.97) and nearly
  saving-free (efficiency 0.93–1.18). Not worth the complexity either.

The one real finding on the venue axis is elsewhere in the same table:
**the mixed contested venue has by far the best surrogate properties**
— T100 r = +0.92/+0.93 against +0.77/+0.86 paired, because both arms
share the game's shocks. That is a reason to *screen* on mixed and
*report* on paired, which is what the campaign already did; it is not a
reason to truncate.

---

## Recommendation for the next campaign

### Venue

Screen on **contested.conf `--mixed`, full length**, report on
**contested.conf + lategame-80 paired holdout, full length**. No
truncation. This is the shape tune-postfix already used; the change is
in what gets tuned and how a winner is accepted, not in the venue.

### Parameters — a designed group screen of ≤ 40, in three tiers

**Tier A — 11 continuous knobs, fit directly.** Every one has a
validated replay and a measured flip rate:

```
switch_cost_gain        scale_settler          scale_worker
garrison_floor_danger   floor_sea_weight       floor_border_weight
upkeep_gold_weight      horizon_capacity_weight
expansion_war_survival  founder_pool_per_site  op_take_target
```

Two natural blocks that do not interact through a shared seam:
**AUCTION** (`switch_cost_gain`, `scale_settler`, `scale_worker`,
`upkeep_gold_weight`, `horizon_capacity_weight`) and **DEFENCE/OPS**
(`garrison_floor_danger`, `floor_sea_weight`, `floor_border_weight`,
`expansion_war_survival`, `op_take_target`, `founder_pool_per_site`).
Five and six parameters against 8–20 noisy seeds is a defensible ratio;
eighty was not.

**Tier B — 26 knobs, group-screened, not fit.** The top-26 by
instrument firing rate listed above. A Plackett–Burman or
fractional-factorial design at ±2σ in *groups* asks "does this block of
knobs move score at all" with a handful of arms, instead of asking
26 separate questions the seeds cannot answer. Only blocks that clear
their own replicated test earn an individual pass.

**Tier C — everything else: pinned, not tuned.** 166 knobs cannot be
shown to move a decision in 40 games, and 92 of them sit on code that
never executed. Pin them at compiled defaults and say so in the
campaign's own log.

### Three cheap instrumentation changes, in priority order

1. **Split `raw` on `deep-decay-sum`** into its four components the way
   `capadd` already is. Moves `horizon_growth_weight`,
   `horizon_bundle_weight`, `horizon_content_weight` and
   `horizon_multiplier_weight` — four ECON knobs firing 2 323 times a
   game — from tier B to a measured flip rate, offline, from the corpus
   that already exists.
2. **Log the military choice's class on `deep-bid`.** `mil` is a merged
   bid with no producer tag, so `scale_defender`, `scale_attacker` and
   `scale_ferry` cannot be screened even when their gate is on. The
   want corpus flagged the same hole in 2026-08.
3. **Log the pre-cap military value.** `emergency_value_cap` is provably
   inert at the domestic/military merge; whether it reorders military
   candidates *against each other* is not observable at all today.

### Tuner changes still outstanding

Unchanged from `findings/tune-postfix-verdict-2026-08-25.md` and not
superseded by anything here: never select by unreplicated max, print
the SE beside every objective, log the incumbent every generation, and
treat σ = range/6 as a first-generation setting. This screen answers
*which* parameters to feed that machinery; it does not fix the
machinery.

---

## Reproducing

```sh
# Part 1
benchmark/knobscreen.py \
  benchmark/results/tune-postfix/s6-nogates-war/holdout-report \
  benchmark/results/tune-postfix/s6-nogates-econ/holdout-report \
  --jobs 8 --validate            # add --md for the tables above
# replication arm: s5-defaults-{war,econ}/holdout-report

# Part 2
benchmark/turncorr.py --root benchmark/results/tune-postfix \
  --layout tune --jobs 8 --cost
```

Both read the archived corpus in place (zstd game logs are streamed;
nothing is decompressed to disk) and neither can start a game.
