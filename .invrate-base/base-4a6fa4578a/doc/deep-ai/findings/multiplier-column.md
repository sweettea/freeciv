# The multiplier column of the horizon (2026-08-01, third session)

The last uncredited column of the building ladder, the cell that can
actually see it, and what the census says about the three credits
already shipped.

Companion to `happiness-ladder.md`, which closed §6 with "the
multiplier column: not established, measure first on a cell large
enough that baseline builds them in double digits". This is that
measurement.

---

## 1. No cell in use could see the question

The previous session's ladder work all happened in `lategame-80`, the
tuning cell. Baseline builds essentially no multipliers there, so the
column was invisible by construction:

    cell          turns   baseline MULTIPLIER/100c   baseline Marketplace (raw)
    lategame-80      80          7.7                       1  (of 52 cities)
    lategame        120          3.2                       4
    long            250         20.1 / 32.3               56 / 105

`lategame` is techlevel 50 from turn 1 and still shows nothing, so it
is **turns, not tech availability**, that gates the multiplier column:
the AI's build queue does not reach these buildings until late, however
early the ruleset unlocks them.

`long.conf` (250 turns, 4 players, 40x40) is the smallest cell in the
suite where the question exists. It is also cheap -- 20 paired games
run in ~40 minutes on 10 jobs.

**Method note.** `buildmix.py` matches players by benchmark naming
(`P1`, `P2`, ...) and therefore returns nothing on the real-nation
mixed games the tuning cells use. `benchmark/ladder.py` (added here)
pools by the save's own `ai_type=` field instead, and skips the
server-generated Barbarian / Pirate / Animal Kingdom players, which
carry an `ai_type` like anyone else and otherwise contaminate whichever
arm shares the server default.

## 2. The census

20 paired `long` games at `d7846cccd0`, train seeds 1-10, both mirrored
mixed arms, `tuning=none`:

    per 100 cities        baseline    deep
    CAPACITY                  46.4    83.4      1.8x  credited (W03)
    CONTENT                   98.9   124.6      1.3x  credited 622f34ca76
    MULTIPLIER                31.3    10.7      0.3x  NEVER CREDITED

    Marketplace          16.4(114)  5.1( 26)
    Bank                  4.0( 28)  0.8(  4)
    Harbour               4.6( 32)  0.2(  1)
    Library               8.8( 61)  4.2( 21)
    Granary               4.7( 33) 29.5(149)
    Temple               33.5(233) 44.8(226)

**Three credits fired, three columns moved; the uncredited column did
not move at all.** Against the two older `long` runs deep sat at 10.7
and 12.6 per 100 cities; it is at 10.7 today while baseline held at
20.1 / 32.3 / 31.3. Content went from 24 points BEHIND baseline to 26
ahead over the same period.

## 3. Two competing explanations, both tested and both dead

**City density -- dead.** Deep has fewer cities (12.6 vs 15.8 per
player), so "deep packs tighter, catchments overlap, each city has less
trade, and stock's base_want correctly prices the Marketplace lower"
was a live alternative. Measured:

                              baseline    deep
    nearest OWN city, mean        3.47    3.38
    nearest OWN city, share <=3    73%     74%
    nearest ANY city, mean        2.32    2.25

Identical spacing. Deep runs a SMALLER empire at the SAME density, with
slightly larger cities (5.88 vs 5.66).

Worth recording as a methods point: on a single arm (`mixed_bd` alone,
10 games) the same measurement read deep's median nearest-own-city as
2.0 against baseline's 3.0 and would have supported the overlap story.
The mirrored arm cancelled it entirely. **A one-arm census carries seat
bias large enough to invent a mechanism.**

**Gold poverty -- dead, for a different reason.** Deep ends with 36
gold per player to baseline's 456, which reads as "too poor to carry
building upkeep, correctly declining Marketplaces". But deep's *trade
per city is higher* (bnp/city 7.49 vs 6.84) with *lower* corruption
(12.2 vs 20.6), so a Marketplace is worth MORE per city to deep, not
less. And baseline's gold is largely dead capital:
`civ2civ3-adjustments.md:279,375` records classic's dead buy loop and
2000-2479 gold hoards. A gold pile is not an achievement.

## 4. Mechanism

Marketplace / Library / Bank / Factory are `ADV_IMPR_CALCULATE`, so
unlike the happiness family they are NOT structurally zero --
`base_want()` runs for them. But `dai_city_want()` (`daicity.c:1443`)
measures them against `acity->citizen_base`: the output of the tiles
this city works **today**.

Every other credit in `deephorizon.c` is buying the citizens that will
work **more** tiles tomorrow. Nothing connected the two. Deep bought
the growth and then declined the buildings that would compound it.

`deep_multiplier_credit()` prices the citizens arriving within `MORT`,
their tile output, run through the building's own percentage, valued at
`adv->*_priority`.

Two implementation notes worth keeping:

- The bonus must be read as a **vector over output types**.
  `get_potential_improvement_bonus()` filters on effect type alone and
  would fold a Marketplace's +50% gold and +50% luxury into a
  meaningless 100. `Output_Bonus` effects carry an `OutputType`
  requirement at Local range naming the output they scale, so
  `deep_output_bonus_pct()` reads that requirement directly rather than
  passing it to `is_req_active()` -- it is evaluated against the output
  being computed, not against a city.
- Valuing through `adv->*_priority` rather than a flat rate is what
  makes the credit robust to the gold question above. A tile yields
  food, shield and trade; trade splits into gold, luxury and science at
  the current tax rates -- the same conversion `dai_city_want()`
  performs via `add_tax_income()` -- and each lands on its own advisor
  priority. If this empire has no use for gold, the gold half of a
  Marketplace is correctly worth little, and that falls out of the
  advisor's own valuation instead of being asserted in deep.

## 2a. THE A/B: mechanism landed, score did NOT

20 paired `long` games per arm on the crash-fixed binary, train seeds
1-10, both mirrored arms, ONE binary and two tuning files (legitimate
here because `horizon_multiplier_weight` is brand new and
`deep_multiplier_credit()` is its only consumer -- verified as an EXACT
no-op at weight 0: 8 players across 2 games identical on score, cities,
techs, pop and gold).

**Seed 3 dropped**: its `mixed_db` game segfaulted (see §8), so 9 paired
seeds, not 10.

    pooled ratio    OFF 0.8638   ON 0.9213   delta +0.0575
    mean per-seed delta +0.0727  (sd 0.1958)
    seeds improved: 5/9

    deep score      10858 -> 11014   +1.4%
    base score      12570 -> 11955   -4.9%
    deep techs       1440 ->  1453   +0.9%
    deep literacy    7035 -> 23595  +235.4%
    deep cities       441 ->   468   +6.1%

Full census, deep, per 100 cities (OFF = 20 games, ON = 19; the
baseline column moves between them because the game sets differ and
the arms interact):

                        OFF      ON     ON baseline
    MULTIPLIER         10.7    23.3          25.4
      Library           4.2    16.0           9.5
      University        0.6     1.2           0.3
      Marketplace       5.1     5.1          12.4   <- UNCHANGED
      Bank              0.8     0.8           3.1   <- UNCHANGED
      Harbour           0.2     1.2           4.6

The column closed from 0.34x of baseline to 0.92x. **Every point of
that came from Library and University.** Marketplace is 26 buildings
in both arms -- bit-identical -- and Bank is 4 in both. The credit did
not move the gold/luxury multipliers by a single building.

**NO SCORE CLAIM IS MADE.** Three independent reasons:

1. **5/9 seeds.** The CM war gate, the only clean win of 2026-08-01,
   was 9/10 (p ~ 0.011). Five of nine is a coin flip.
2. **Variance swamps it.** SE = 0.1958/3 = 0.065, t ~ 1.11, p ~ 0.30.
   The pooled move is carried by two opposed outliers: seed 10 +0.47,
   seed 9 -0.24.
3. **It is mostly baseline FALLING.** deep +1.4%, baseline -4.9%. In
   mixed games the ratio moves when either side moves; the CM war gate
   qualified because deep's ABSOLUTE score rose.

**The pre-registered falsification test failed.** Before running it,
the prediction recorded was: if the libraries do genuine work the tech
gap must open; if library count triples and techs stay flat, deep has
bought upkeep and shields for nothing. Literacy tripled (+235%);
**techs moved +0.9%**.

The credit is justified by the DEFECT -- the multiplier column is
demonstrably priced against `citizen_base` while deep buys tomorrow's
citizens, readable in `dai_city_want` -- exactly as `fc7073184a` was
justified by the inverted Growth_Food condition. Not by a gain.

**This is the THIRD credit in a row whose mechanism lands decisively
and whose score does not** (content, granary, multiplier). That pattern
is now the most informative result in this file. See §5.

### The composition is the interesting part

The column closed almost entirely on the SCIENCE side: Library 4.2 ->
20.2 per 100 cities, overtaking baseline 2:1, while Marketplace barely
moved (5.1 -> 6.7) and Bank not at all. That is
`deep_output_bonus_pct()` routing through `adv->*_priority` working as
designed: a tile's future trade splits into gold/luxury/science at the
current tax rates, and deep runs a low gold priority because -- as
`civ2civ3-adjustments.md:279` records -- gold is barely usable
(classic's dead buy loop hoards 1968-2586 while deep runs 9-128).

So the credit concluded that libraries are worth buying and
marketplaces mostly are not. **That may be correct.** Baseline's 114
Marketplaces might be the error rather than the target. The tech result
above says it did not convert either way.

## 4a. Ruleset scope -- READ THIS BEFORE QUOTING SECTION 2

**Every number in this document is civ2civ3.** `long.conf` sets
`RULESET=` (empty, so the server default), and `long-classic.conf`
exists in the suite and has never been run at all.

What transfers and what does not:

    effect family per building     classic            civ2civ3
    Marketplace                    Output_Bonus       Output_Bonus
    Library                        Output_Bonus       + History
    Temple                         Make_Content       + History
    Granary                        Growth_Food        + Output_Waste_Pct
    Aqueduct                       Size_Adj/Unlimit   Size_Adj/Unlimit

- **The MECHANISM transfers.** Multipliers are `Output_Bonus` in both
  rulesets, so they are `ADV_IMPR_CALCULATE` and `base_want()` prices
  them against `citizen_base` in both. Temple is `Make_Content` in
  both, so it stays structurally zero in both. The diagnosis in §4 is
  not civ2civ3-specific.
- **The CODE is ruleset-generic by construction.**
  `deep_output_bonus_pct()` reads the effect list and the `OutputType`
  requirement; it hardcodes no building names, exactly as
  `deep_bundle_credit()` does not.
- **The MAGNITUDES do not transfer.** Percentages, upkeep, corruption
  and the availability curve all differ. civ2civ3 charges gold upkeep
  on every building; classic does not, which changes the whole
  cost/benefit of a multiplier and therefore the right weight.
- **`ladder.py`'s column membership does not transfer.** It is a
  hardcoded name list. It happens to cover both rulesets' names today,
  but a ruleset that renames or re-roles a building silently mis-files
  it.

**Named follow-up: run `long-classic` and re-census.** That is the
cheapest test of whether the multiplier credit's weight is a ruleset
constant or a ruleset variable -- and if it is a variable, the
placeholder cannot ship as one number.

## 5. What the census says about the credits as a FAMILY

This is the part that outlives the commit.

    CAPACITY    1.8x baseline    horizon_capacity_weight  1.0 PLACEHOLDER
    CONTENT     1.3x baseline    horizon_content_weight   1.0 PLACEHOLDER
    GROWTH      6.3x baseline    horizon_growth_weight    0.5 PLACEHOLDER
    MULTIPLIER  0.3x baseline    (none, until this epic)

**An earlier draft of this section read that table as evidence that
the per-column credits are the wrong architecture. That argument does
not hold, and it is recorded here as a caution rather than deleted.**

Three things were wrong with it:

1. **It treated baseline's build mix as the reference.** Baseline's
   aqueduct want is ~0 by RC1, the defect this whole workstream
   exists to fix. "1.8x baseline" is not "overshoot" -- baseline is
   the thing being beaten, not the target ratio. There is no
   independent measure of the right number of aqueducts in that
   table, so it cannot show any weight is too high.
2. **"Never jointly calibrated" was simply false.** All four horizon
   weights are in the ECON and G blocks, so `campaign-a4` is
   calibrating them jointly right now.
3. **A projected-city evaluator may not subsume the columns.** A
   multiplier is proportional to output, so a projected `citizen_base`
   expresses it naturally. But a HARD CAP is not visible in a
   projected citizen_base at all -- a city cannot be projected past a
   ceiling it cannot cross. The cap-lifter's value is a counterfactual
   ("what if the gate were not there"), a different question from
   "what will this city produce in N turns". Collapsing to one
   evaluator could lose that rather than absorb it.

There is also a positive case for separate weights: the columns may
deserve genuinely different emphasis per ruleset, and one scalar
cannot express that -- the same argument that justified splitting
`cm_war_survival` from `survival_target` (`STATUS.md`, 2026-08-01).

**What remains true, and is worth watching:** four independent
placeholder constants govern deep's building order, the search space
grows with each one, and nothing yet demonstrates the four are
separable. The open question is whether the objective surface treats
them independently or whether they trade off strongly.

**That is answerable from data already being collected.** The ECON
block tunes all four; if the campaign's per-generation candidates show
the horizon weights moving together rather than independently, they
are coupled and the per-column form is buying complexity without
expressiveness. If they move independently, the columns really are
distinct and the architecture is right. Check this at harvest -- it
costs nothing beyond reading the candidate log that already exists.

## 6. An inherited defect, now in four places

Every credit gates on `city_can_grow_to(pcity, size + 1)` -- ONE step
-- and then spends `MORT / ttg` growths. A size-7 city that jams at 8
without an aqueduct is credited for the whole horizon's growths when
exactly one is available.

`deep_multiplier_credit()` inherits this deliberately, for consistency
with the three credits beside it: the over-count is uniform, so it does
not distort their relative ordering, and bounding `arriving` by real
headroom to the next hard cap is a change to three existing functions
rather than one.

It is plausibly part of why capacity overshoots 1.8x and growth 6.3x.
Fixing it is its own commit and should be measured on its own.

## 7. Also established

- **`benchmark/report.py` does not exist.** `CLAUDE.md:97` and
  `benchmark/README.md:9` both document it as the way to get tables and
  confidence intervals. The paired ratios in this document were
  hand-rolled from `metrics.csv`. Either it was never written or it was
  lost; the docs have been promising it for some time.
- **Deep loses this cell.** 20 paired `long` games, pooled score ratio
  **0.8754**, mean per-seed 0.8858, 2/10 seeds above 1.0. This is a
  cell deep has never been tuned or validated on -- all recent work was
  `lategame-80` -- and it is NOT comparable to the older `long` runs
  (0.9743 / 0.9770), which used holdout seeds 101-112 while this used
  train seeds 1-10. **No regression claim is available from that
  difference, and none is made here.** Establishing whether `long` has
  regressed needs the same seeds on both binaries.

## 8. The segfault that cost seed 3

`long` seed 3 / `mixed_db` died at turn 156 with SIGSEGV. **It is NOT
caused by this credit** -- OFF and ON run the identical binary, and
`deep_multiplier_credit()` executes its whole body at weight 0 (only
the final product is zeroed), so both arms exercise the same code. A
different want ordering merely walked into a latent bug.

Two core dumps prove it predates today's work:

    17:41 (binary d7846cccd0, before the crash fix)
      deep_collect_defenders(pcity=0x245108c0)
      deep_take_odds_at_arrival(acity=0x245108c0)
      deep_op_drive  deepops.c:1437        <- MARCH block
    18:55 (binary with d5ce7cecfb)
      deep_op_eta(target=0x0)  deepops.c:754
      deep_op_drive  deepops.c:1419        <- MARCH block

Same function, same block, same unguarded `target`. `deep_op_drive()`
hoists `target = game_city_by_number(op->target_city)` at entry behind
a comment claiming the caller "already validated target and staging",
and the MARCH block dereferences it -- getting a freed pointer in one
core and NULL in the other.

`d5ce7cecfb` fixed the post-launch site and left this one. **That
commit's message overclaims**; it fixed one of at least three sites in
the same function. The entry hoist needs its own guard.
