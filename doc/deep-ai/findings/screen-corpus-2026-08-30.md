# The screen corpus retake, and the mixed venue's own sigma (2026-08-30)

Two follow-ups to `findings/knob-screen-2026-08-30.md`, both of which
that document named as outstanding.

1. **The RESCREEN knobs.** Eight knobs became reconstructible offline
   when `ai/deep` started logging the four `deep-decay-sum` credit
   components, the `milclass` producer tag on `deep-bid`, and the
   `deep-milcap` pre-cap value. The 40-game corpus the screen ran on
   predates all three, so `knobscreen.py` reports those eight as
   **UNMEASURED** there rather than measuring them. Part 1 re-takes
   the corpus and prices them.
2. **The mixed venue's per-seed sigma.** `screen-campaign.sh` screens
   on `contested.conf --mixed` and stated its SE budget from
   `sigma = 115`, which is the sigma of the **paired** contested gap.
   Its own header said so and called the substitution out. Part 2
   measures the mixed sigma from 1 120 games already paid for.

**Games bought: 40, and they are the same 40 games.** Part 2 bought
none.

---

## Part 1 — the corpus retake

### This is not optimisation against holdout seeds

The retake re-plays the exact 40 games the screen already used:
compiled defaults (`benchmark/tuning/defaults.ini`), `contested.conf`
and `lategame-80.conf`, the 20 seeds of `benchmark/seeds/holdout.txt`,
via `tune.py --report-holdout`. That path evaluates **one fixed tuning
file** and contains no optimiser — nothing is chosen, ranked, or moved
by it. The tuning file is the compiled defaults, so there is nothing
to have been fitted. `benchmark/rescreen-corpus.sh` is the runner and
its header says the same thing.

The 40 `ai/baseline` games are hardlinked in from `s6-nogates-*`
rather than re-run (the arm is frozen and tuning-independent), exactly
as `benchmark/nogates-holdout.sh` does. Only the 40 deep games were
bought.

### The games are byte-identical to s6

Every one of the 40, not a sample. `sha256` of `scorelog.txt` with the
`id` line excluded (it is a per-run random token), and the turn-1
terrain `maphash` beside it:

    arm    seeds 101..120 scorelog sha (first 16), new == archived s6
    war    ef13d47c64f28130  7445677e29b7c6a1  d2b3504fc380f009
           e2f479a6a42eb3e5  12a8066003eabbb6  18245354eadf86e9
           34491a798caa2f82  61c8b8c24a8a5fbe  6ff600825eb8978c
           d3e08a3c6ecd85f9  726befe4c7113824  345b880ac028d4b6
           ef68dc1ddcda8bc1  00a9b61628df9b01  107f372206198758
           1634989f9f43fa45  4e570c0cf149b55a  9d618d6fd7eb8338
           425860bb94fd006e  a171bf66d3ed65c6
    econ   c6ddaf6c691729c4  3b78695426491ef6  cab331364b425a87
           9f932f116d16af70  11b1b8ee8dd82a76  2fe26c1fb8ccb561
           0db60bdc7b8ee13b  5a839419424d0cc7  9c5a0883153fb82f
           d0b8e3d26506a04c  179be94216b0dee1  36b7e504c91b8d77
           30e0afdcfee6a8e8  88bdae192d171136  9737888514376bf6
           e42876dd6d206590  10f8609091989864  2e6a116c4a98517b
           55107f5a3a7f0b93  e8b8453876b294a7

**40 of 40 match, and all 40 maphashes match.** The holdout report
reproduces s6's numbers exactly as a consequence:

    contested     mean paired diff  -58.4 +/- 21.2  (20 seeds)
    lategame-80   mean paired diff  -57.9 +/- 16.3  (20 seeds)

Binary sha `7f4a6473b05362d6` against s6's `24867bef2364d609`; config
sha `6ad23579`, runner sha `61aa448d`, `tuning=defaults.ini` sha
`8b363d05` all unchanged. The only difference between the two corpora
is what the server wrote to `game.log`: **4 471 631 `deep-` lines
across 117 tags**, against 4 452 k across 116 — one new tag
(`deep-milcap`) and five new fields.

### Measured flip rates for the eight RESCREEN knobs

`knobscreen.py` over the new corpus, 40 games. `flip rate` is flips
divided by the decisions the rule touched, which is the scale-free
number; `flips/game` is what ranks them.

| knob | default | sigma | decisions/game | flips/game | flip rate | replay rule |
|---|---|---|---|---|---|---|
| `horizon_content_weight` | 1.0 | 0.667 | 2329.3 | **753.23** | 32.3% | building_anchor |
| `horizon_multiplier_weight` | 1.0 | 0.667 | 2329.3 | **144.62** | 6.2% | building_anchor |
| `scale_attacker` | 1.0 | 0.492 | 3570.7 | **112.90** | 3.2% | auction_merge |
| `horizon_bundle_weight` | 1.0 | 0.667 | 2329.3 | **83.88** | 3.6% | building_anchor |
| `horizon_growth_weight` | 0.5 | 0.333 | 2329.3 | **78.85** | 3.4% | building_anchor |
| `emergency_value_cap` * | 0.0 | 333.3 | 4008.9 | **51.27** | 1.3% | emergency_cap |
| `scale_defender` | 1.0 | 0.492 | 3570.7 | **42.75** | 1.2% | auction_merge |
| `scale_ferry` | 1.0 | 0.492 | 3570.7 | **16.15** | 0.45% | auction_merge |

**All eight flip something. None is inert.** Seven go to tier A; the
starred one goes to tier C, for a reason given below that is about
what its flip *is*, not about how many there are.

Replay self-checks on this corpus — the base-value reconstruction
against the outcome the server itself recorded:

| rule | decisions checked | agreement |
|---|---|---|
| `auction_domestic` | 50 892 | **100.0%** |
| `building_anchor` | 93 172 | **100.0%** |
| `emergency_cap` | **17 530** | **100.0%** |
| `expansion_gate` | 4 189 | **100.0%** |
| `founder_pool` | 4 140 | 99.9% |
| `garrison_floor` | 7 115 | 99.9% |
| `op_formation` | 19 852 | **100.0%** |
| `switch_cost` | 54 326 | **100.0%** |

The `emergency_cap` row is new at scale: the smoke corpus had 606
checks, this has 17 530, all agreeing.

### The three-game smoke numbers were wrong, and that is the point

`STATUS`'s W25 entry of 2026-08-30 printed indicative flip rates from
a three-game smoke corpus and said in bold that they rank nothing.
They did not:

| knob | 3-game smoke | 40-game corpus | verdict on the smoke |
|---|---|---|---|
| `horizon_bundle_weight` | **0.0 (called inert)** | 83.88 | wrong sign of the answer |
| `scale_attacker` | 40.7 | 112.90 | under by 2.8x |
| `scale_defender` | 83.0 | 42.75 | over by 1.9x; order with attacker inverted |
| `emergency_value_cap` | 8.3 | 51.27 | under by 6x |
| `horizon_content_weight` | 823.7 | 753.23 | close |
| `horizon_multiplier_weight` | 189.3 | 144.62 | over by 1.3x |
| `horizon_growth_weight` | 94.3 | 78.85 | close |

A knob called **inert on three games flips 84 decisions a game on
forty**, and the two military scales swap places. This is what the
"indicative only" label was protecting against and it is worth having
on the record, because the cheapest possible mistake here would have
been to pin `horizon_bundle_weight` on the smoke number.

### Where each RESCREEN knob goes

**Tier A — the four horizon credit components.**
`horizon_content_weight` (32.3%), `horizon_multiplier_weight` (6.2%),
`horizon_bundle_weight` (3.6%), `horizon_growth_weight` (3.4%). Each
is exactly linear in its own weight inside the credit sum, each moves
the building anchor's argmax over ~93 k credited improvements, and the
`building_anchor` replay reproduces the server on 100.0% of them.
`horizon_content_weight` is now the **largest single flip rate in the
whole screen after `garrison_floor_danger`** — it flips a third of
every credited improvement's argmax, which is more than
`switch_cost_gain`, the knob the previous screen ranked first.

**Tier A — the three military scale keys.** `scale_attacker` (3.2%),
`scale_defender` (1.2%), `scale_ferry` (0.45%). These were
**dead-path** before — the merge's `mil` field was a bid with no
producer tag, so the screen could not tell which key applied. With
`milclass` they are ordinary two-sided continuous knobs on the
`auction_merge` seam, with 3 570 merges a game to move.

**Tier C — `emergency_value_cap`, pinned.** It flips 51.27 times a
game and it still should not be in a parameter search, for three
reasons that are all in the data:

- The flip its rule can see is **"the clamp engages"**, not "a
  different candidate wins". Binding is an input to a decision, not a
  decision.
- At the one comparison where a winner *is* logged — the
  domestic/military merge — the cap remains **provably inert**: the
  earlier screen found no ±1σ move changing one of 143 k merges, and
  the merge is tier-first so a clamped emergency want still outranks
  every domestic one. The reordering the cap *could* cause is
  military-against-military, which is still not logged anywhere.
- Its default is 0.0 at the bottom of its own range, so the −1σ step
  clips back to 0 and only +1σ moves anything: the measured 51.27 is
  **what turning the cap on at 333 would clamp**, not a gradient. That
  is the `founder_tier_gate` situation exactly, and it earns the same
  treatment — a gate A/B, not a search dimension.

`founder_tier_gate` and `garrison_floor` are already shortlisted and
already excluded from the fit blocks for that reason; this is the
third member of that class and it is handled the same way.

### Tier A re-confirmed, entry for entry

The eleven tier-A knobs the previous screen chose reproduce to the
digit on the retaken corpus, which they must, because the games are
byte-identical and their rules do not read the new fields:

| knob | flips/game | flip rate | | knob | flips/game | flip rate |
|---|---|---|---|---|---|---|
| `switch_cost_gain` | 274.70 | 20.2% | | `floor_border_weight` | 53.33 | 30.0% |
| `scale_settler` | 114.15 | 1.6% | | `scale_worker` | 51.55 | 0.72% |
| `garrison_floor_danger` | 113.05 | 63.5% | | `expansion_war_survival` | 15.25 | 14.6% |
| `upkeep_gold_weight` | 93.17 | 4.0% | | `horizon_capacity_weight` | 11.22 | 0.48% |
| `floor_sea_weight` | 63.40 | 35.6% | | `founder_pool_per_site` | 9.90 | 9.6% |
| | | | | `op_take_target` | 4.90 | 0.99% |

The two mode-style shortlist entries also reproduce
(`founder_tier_gate` 256.90, `garrison_floor` 112.95), as does
`floor_barb_weight` at 0.55 and the inert pair `floor_air_weight` /
`expiry_building_gain`.

### What the retake did to the census

| class | before | after | what moved |
|---|---|---|---|
| reconstructible | 17 | **24** | +3 from dead-path, +4 from partial |
| — of which flip something | 14 | **22** | the 7 promotions, plus `emergency_value_cap` off the inert list |
| — of which flip nothing | 3 | **2** | `floor_air_weight`, `expiry_building_gain` |
| dead-path | 92 | **89** | the three military `scale_*` keys now run |
| partial (fires) | 64 | **60** | the four horizon components are now measured |
| partial (never fired) | 20 | 20 | |
| not-logged | 54 | 54 | |
| mode | 28 | 28 | |
| unmapped | 2 | 2 | `weak_prey_gate`, `weak_prey_gain` |

**The inert set falls from 166 knobs to 163** — dead-path 89 +
not-logged 54 + partial-never-fired 20. Still 59% of 277, and the
three cheap instrumentation changes bought exactly the seven knobs
`knob-screen-2026-08-30.md` predicted they would, at a cost of forty
games that were re-plays.

### Tier B is unchanged, and its cut is now clean

The four horizon components used to sit at the head of the tier-B
firing ranking (2 323 fires/game each) and have left it. The top 28
by firing rate is now **exactly the four tier-B blocks**, name for
name — BFEAR (9), BASSAULT (6), BSITE (6), BCIVIL (7). The previous
cut had to be widened from 26 to 28 because a five-way tie at 73.3
straddled it; that tie (the five `site_*` entries) now sits entirely
inside the blocks, and the first name outside is `wrace_margin_turns`
at 69.7. No tier-B block changes.

### The blocks, after

`benchmark/tuning-blocks.ini`. Tier A goes from 11 knobs in two
blocks to **18 knobs in two blocks**, and both stay at the discipline
width of 9.

    AUCTION  9  switch_cost_gain scale_settler scale_worker
                upkeep_gold_weight horizon_capacity_weight
                horizon_growth_weight horizon_bundle_weight
                horizon_content_weight horizon_multiplier_weight
    DEFOPS   9  garrison_floor_danger floor_sea_weight
                floor_border_weight expansion_war_survival
                founder_pool_per_site op_take_target
                scale_defender scale_attacker scale_ferry

Adding all seven to AUCTION would make it 12 wide, so the width cap
forces a cut, and the cut is at the **merge**: AUCTION is now
everything on the *domestic* side of the domestic-versus-military
comparison (the worker/founder/building bids and the horizon anchor
they are priced against), DEFOPS is everything on the *military* side
(what produces a military want, and what that want is then worth). A
block boundary belongs at a seam between two subsystems, and the
merge is that seam.

The alternative cut — anchor in one block, bids in another — would
have put the boundary *through* a comparison that runs 93 k times a
game, which is the one place it must not go. It is also why
`scale_defender` sits beside `garrison_floor_danger`: the floor
produces the defender want and the scale multiplies it, so they are
one lever applied twice and belong in one block.

`RESCREEN` stays in the file as a **retired** block recording where
its eight knobs went, so nothing that referenced it breaks and the
answer is beside the question.

The campaign's stage list, budget and game count are **unchanged**: a
block pass costs `pop x gens x seeds` games regardless of how many
constants it moves, and `tune.py --dry-run` still prints 1 656 games
per stage-1 block pass at 9 constants exactly as it did at 5.

---

## Part 2 — the mixed venue's per-seed sigma

### What the mixed objective is, exactly

A `--mixed` game has no separate baseline arm: both modules sit in the
same game, one on odd seats and one on even, and every seed is played
**twice** under mirrored seatings (`mixed_db`, `mixed_bd`) so seat
bias cancels. `tune.py eval_population()` forms one datum per
`(config, seed)`: the **sum over the two mirrored games** of
`deep_total - base_total`, each side from `mixed_sides()` over the
seats whose name matches `P\d+` (civil-war splinters belong to
neither). `blend_stats()` then averages those data across seeds.

A per-seed datum therefore costs **two games**, exactly as a paired
contested gap does (one deep game plus one baseline game). The two
venues' sigmas are directly comparable per unit of machine time,
which is the only reason this comparison means anything.

Corpus: every candidate directory under
`benchmark/results/tune-postfix/s1-mil-mixed` and `s3-mil-r2`, the two
`--mixed` MIL stages — 70 candidates x 8 training seeds x 2 mirrored
arms = **1 120 games**, all already on disk.

### Measured

| statistic | s1-mil-mixed | s3-mil-r2 | pooled |
|---|---|---|---|
| candidates / seed-points / games | 40 / 320 / 640 | 30 / 240 / 480 | 70 / 560 / 1120 |
| **pooled within-candidate across-seed sigma** | **108.0** | **99.3** | **104.4** |
| between-candidate sigma of candidate means | 43.1 | 40.2 | 44.2 |
| — of which is the 8-seed sampling SE | 38.2 | 35.1 | 36.9 |
| — true between-candidate spread | 20.0 | 19.6 | 24.4 |
| seed-effect sigma (sigma of the 8 seed means) | 68.7 | 54.3 | 59.8 |
| within-seed sigma across candidates | 89.9 | 88.8 | 91.9 |
| **paired candidate-vs-candidate per-seed sigma** | **120.1** | **118.0** | **119.2** |

The paired row differences every pair of candidates inside one
generation seed by seed (180 + 135 = 315 pairs) — the statistic
`tune.py paired_stats()` forms, and the one `--accept paired` gates
on.

The variance decomposes: `sqrt(59.8^2 + 91.9^2) = 109.7` against a raw
spread of 107.1 over all 560 seed-points.

Reference, recomputed here with `tune.py final_score()` rather than
cited — the **paired** per-seed sigma on the holdout arms:

| arm | contested | lategame-80 |
|---|---|---|
| s4 tuned | 115.3 | 64.2 |
| s5 defaults+gates | 105.4 | 72.7 |
| s6 defaults | 94.9 | 72.9 |

### Three things this says

**1. The mixed venue is not a quieter venue.** 104.4 sits inside the
paired contested range 94.9–115.3, not below it. The mixed venue's
advantage established in `knob-screen-2026-08-30.md` Part 2 was
*correlation with the final gap* (T100 r = +0.92/+0.93 against
+0.77/+0.86 paired), because both arms share one game's shocks. That
is a surrogate property, not a variance property, and the two do not
come together. `screen-campaign.sh`'s `sigma = 115` was pessimistic by
about 9%, which is the least interesting of the three findings.

**2. Most of the apparent spread between candidates is the seed
sample.** The candidate means scatter with sigma 44.2, and 36.9 of
that is the SE of an 8-seed mean; the true between-candidate spread is
**24.4**. The post-fix post-mortem's diagnosis — "the campaign
selected the max of ~40 unreplicated draws from an objective whose SE
was ~41" — reproduces on the mixed venue as a variance decomposition:
the optimiser was ranking sampling error against a real signal roughly
two-thirds its size.

**3. The paired contrast does not shrink on the mixed venue. It
grows.** `paired_stats()`'s docstring said the within-seed contrast's
SE is "well below either objective's own". On a paired venue that is
true: the two candidates share a baseline term and it cancels. On the
mixed venue **there is no shared term to cancel** — the opponent lives
inside each game and diverges chaotically with the candidate's own
tuning — so pairing removes only the seed effect (sigma 59.8) while
differencing two nearly independent games adds variance. The measured
correlation between two candidates' objectives at the same seed is
**rho = 0.16**, and 119.2 > 104.4 follows.

This does not make `--accept paired` wrong. Removing the seed effect
is still right, and the contrast is still the quantity the acceptance
question is about. It makes it **not free**, and the campaign's budget
was written as though it were. `tune.py`'s docstring and its
`--dry-run` footer both said so and are corrected.

### The restated SE budget for `screen-campaign.sh`

Every row is `sigma / sqrt(configs * seeds)` with the sigma measured
above, for the campaign as the script stands (20 training seeds, 1
config, `--race 8`, `--reeval-seeds 8`).

| statistic | sample | sigma | SE | was (sigma 115) |
|---|---|---|---|---|
| block objective | 20 seeds x 1 config | 104.4 | **23.3** | 25.7 |
| race rung | 8 seeds | 104.4 | **36.9** | 40.7 |
| reeval objective | 8 fresh seeds | 104.4 | **36.9** | 40.7 |
| **acceptance contrast** — champion minus incumbent, the number `--accept-z 2` gates | 8 fresh seeds | **119.2** | **42.2** | not budgeted |
| holdout report, contested, paired | 20 seeds | 94.9–115.3 | 21.2–25.8 | 25.7 |

The last mixed row is the one that decides whether the campaign
accepts anything, and it was never in the budget.
`--accept paired --accept-z 2` requires a champion to beat the
incumbent by **>= 84 points** of summed mirrored-game score on 8 fresh
seeds. The effects on the table are the ~16–24 point gate contrasts of
2026-08-25.

**So the campaign as budgeted cannot accept an effect of the size this
project has been finding**, and that is now measured rather than
carried across venues. The header's own warning — "this budget can
detect a large block effect and CANNOT resolve a small one" — was
right, and understated the bar by about a factor of two.

The lever is `--reeval-seeds`, at 2 games per candidate per seed:

| `--reeval-seeds` | acceptance SE | bar at `--accept-z 2` | extra games per generation (K=3) |
|---|---|---|---|
| 8 (as written) | 42.2 | 84 | — |
| 16 | 29.8 | 60 | +64 |
| 32 | 21.1 | 42 | +192 |

`benchmark/seeds/train.txt` holds 100 seeds and `--count` is 20, so
`--reeval-seeds` can go to 80 without touching the seed file; the
whole cost is games. Nothing here is a recommendation to launch: the
campaign is still un-launched by design, and this is one more number a
human should see before typing `yes`.

---

## Reproducing

```sh
# Part 1 -- the retake (40 deep games; baselines are hardlinked in)
benchmark/rescreen-corpus.sh /path/to/pinned/freeciv-server

# Part 1 -- the screen over the retaken corpus (no games)
benchmark/knobscreen.py \
  benchmark/results/w25-screen-corpus/war/holdout-report \
  benchmark/results/w25-screen-corpus/econ/holdout-report \
  --jobs 6 --validate            # add --md for the tables above

# Part 2 -- no games; reads the archived --mixed stages in place
#   pooled within-candidate across-seed sigma of the mixed objective,
#   its decomposition, and the paired candidate-vs-candidate sigma,
#   all from tune.py's own mixed_sides() / final_score().
```

Part 2's arithmetic is a short script over
`benchmark/results/tune-postfix/{s1-mil-mixed,s3-mil-r2}` that imports
`tune.py` and reproduces `eval_population()`'s `--mixed` datum
(`sum over the two mirrored arms of deep - base` per `(cfg, seed)`).
Nothing it does can start a game.
