# CM completion: the posture change gets a price (2026-08-07)

Session CM-COMPLETION, task #28, branch `epic/cm-completion` off
`85c7dfbad4`. Ran in the bench worktree in parallel with CONSEQ+WRACE,
which owned the main tree and `deepdefence/deephorizon/deepcity/deepops`.

Three items, in the brief's priority order: posture-counterfactual
surpluses (the centerpiece), production-vs-growth arbitration,
specialist factors from solvency (DARKFX's deferred item 4).

---

## 0. Headline

**The posture counterfactual is not zero, and the two cells agree.**
18 904 flip candidates priced across 40 games (lategame-80 and
garden2b-150, 20 each). On the pairs where both arrangements solve, the
war footing changes the surplus vector in 31% / 35% of cases, and the
mean trade it makes is

    lategame-80    food -0.31   shield +0.67   trade -0.44
    garden2b-150   food -0.30   shield +0.59   trade -0.23

per priced city-turn. That is the first time in this project that the
cost of putting a city on war footing has been a number rather than an
assumption. At the maximum charge it flips 5.9% / 6.8% of the
priced-and-valid population.

**The bill is ~1.1% of game time.** 1 434 extra `cm_query_result()`
calls per game on lategame-80 (498 on garden2b), 5.05 s CPU per game
against a ~450 s game, worst single turn 0.292 s. No cache or bail
beyond the one already in the design was needed. The reason it is that
cheap is structural and is stated in §4.

**One design item was wrong and the census caught it.** Item 2 was
briefed to read "the auction's own top want at the city". That want
does not exist at this seam: `city_data->choice.want` is reset to -1 by
the buy pass at the end of the turn, and the first probe read
`build=-1.0` on **554 of 554** evaluations and fired on every one. The
correction is in §3.

**Score is inside noise on both cells** at the all-on arm -- -0.77
(t -0.51) on lategame-80, +3.92 (t +0.93) on garden2b-150 -- which is
the expected result for three untuned placeholders pinned at 1.0. The
growth column points the same way on both. Numbers in §5.

---

## 1. What was built

Four commits of mechanism, one of refactor, one of visibility.

- **`deep_cm_peace_posture()` / `deep_cm_apply_posture()`** — the
  W09-lite if-chain split so the chain can be evaluated twice for the
  same city. Pure refactor; the war branch still short-circuits the
  expensive `assess_defense()`/survival calls in grow-hard.
- **`set_default_city_manager()` made non-static** (`server/cityturn.h`).
  Visibility only. Necessary, not cosmetic: see §2.
- **`deep_cm_counterfactual()`** — both postures solved, cached per
  city per turn, surplus vectors remembered, growth cost derived.
  `cm_war_growth_charge`.
- **`deep_cm_grow_priced()`** — marginal citizen against marginal
  build at the grow-hard entry. `cm_grow_price_blend`.
- **`deep_cm_solvency_factors()`** — gold and science CM factors from
  treasury-vs-guard and tech-upkeep-vs-bulbs.
  `cm_specialist_gold_gain`, `cm_specialist_sci_gain`.
- **`deep_cm_posture_delta()`** exposed in `deepcm.h` for the fear /
  floor machinery's satisfied-city discount. **No consumer was wired**:
  that discount lives in `deepdefence.c`, which a parallel session
  owned. The accessor returns FALSE for any city not priced this turn,
  and FALSE means "no opinion", not "costs nothing".

### Off-switch proof

All four knobs ship at 0. Smoke cell, 10 paired seeds, both arms
loading the SAME `cm-off.ini`; the only difference is the binary
(`pin-pre` = `85c7dfbad4`, `pin-new` = this session's tip):

    seed  1 pre=497f16042e13 new=497f16042e13 SAME
    seed  2 pre=15a6e206d336 new=15a6e206d336 SAME
    seed  3 pre=516442070fa0 new=516442070fa0 SAME
    seed  4 pre=0c0e8db3ccd8 new=0c0e8db3ccd8 SAME
    seed  5 pre=d26859d9f559 new=d26859d9f559 SAME
    seed  6 pre=da5324a8910c new=da5324a8910c SAME
    seed  7 pre=1947015da4da new=1947015da4da SAME
    seed  8 pre=98dea66d7a3a new=98dea66d7a3a SAME
    seed  9 pre=df5a00343695 new=df5a00343695 SAME
    seed 10 pre=375c749df24a new=375c749df24a SAME
    => 10/10 byte-identical

That covers the refactor and the visibility change as well as the three
mechanisms, since all six commits are in `pin-new`.

Execution counters, all-on arm, lategame-80, 20 games: 14 307
`deep-cmpost`, 19 183 `deep-cmgrow`, 9 328 `deep-cmspec`. Non-execution
and byte-identity are different claims and both are made.

---

## 2. Why the visibility change was load-bearing

The alternative posture a threatened city is priced against:

                                          lategame-80   garden2b-150
    alt=0  no deep posture, stock's default    11 116  78%   2 719  59%
    alt=4  grow-hard                            1 829  13%   1 788  39%
    alt=2  growth-capped                        1 010   7%      21   0%
    alt=3  wonder                                 352   2%      69   2%

**Between 59% and 78% of the priced population has no deep posture at
all.** A mature
city above `cm_grow_below`, uncapped and not building a wonder, is
exactly the city that goes to war footing, and its alternative is
stock's own parameter. Without `set_default_city_manager()` the
counterfactual would have been blind on most of its population, or
would have compared war footing against deep's BASE parameter — an
arrangement that never actually happens.

---

## 3. The seam does not have the auction's want

Item 2 as briefed: "marginal citizen value vs marginal build value (the
auction's own top want) at the city".

`deep_set_city_manager_params()` runs in `first_activities`, at the top
of the turn. `city_data->choice.want` is set to `-1` in the buy pass
(`deepdefence.c:4660`) at the END of the previous turn, for every city
that pass reaches — which is every city with a positive want. So at
this seam the auction's chosen want is gone.

First probe, smoke cell, `cm_grow_price_blend = 1.0`:

    build=-1.0 on 554 of 554 evaluations,  fires=1 on 554 of 554

The test was vacuous: any positive citizen value beats -1.

**The fix uses `pcity->server.adv->building_want[]`**, the standing
building auction. It is live at this seam, it is what the build
decision actually reads, and D2 made it honest (deep applies and
reverts its credit around the build pass, so between passes the array
holds stock's own want).

It is also better dimensioned than the briefed number.
`infracache.h:28-32`: "the units of `building_want` are output
multiplied by a priority" — which is exactly what
`deep_unworked_tile_value()` returns. **Both sides are in one
currency**, so `cm_grow_price_blend` is a genuine trade-off weight and
not the unit conversion W01's unbuilt `want_t` was going to be. That is
a smaller claim than the one the brief's framing implied, and a sounder
one.

Same probe after the fix: 89 of 545 fire; on the sampled city,
citizen 110.0 against build 34.8.

**The gap this leaves.** `building_want[]` covers buildings only. A
city whose best build is a UNIT — settlers, a defender — has that want
in `choice`, which is gone, so the test sees a smaller build side there
and leans toward growth. It is bounded by the size floor (the test is
an OR, so it can only ADD cities to grow-hard) and by the
garrison-coverage conjunct, but it is real and it is the first thing to
fix if this mechanism is ever taken further. Fixing it properly means a
snapshot of `choice` taken before the buy pass clears it, which lives
in a file this session did not own.

---

## 4. The counterfactual, measured

lategame-80 / garden2b-150, 20 games each, all-on arm
(`cm_war_growth_charge = 1.0`). The two cells agree on every ratio,
which is worth saying out loud given how often a verdict in this
project has inherited its venue.

                                          lategame-80   garden2b-150
    flip candidates priced                     14 307         4 597
      both arrangements solved (valid=1)        8 592  60%    2 360  51%
      one arrangement infeasible                5 715  40%    2 237  49%
    of the valid ones:
      surplus vector differs between postures   2 668  31%      828  35%
      growth cost > 0                           1 927  22%      598  25%
      mean cost (fraction of food surplus)       0.115          0.124
      flips revoked by the charge, % of valid            5.9%          6.8%

    mean surplus delta, war minus alternative, over valid pairs:
      lategame-80    food -0.31   shield +0.67   trade -0.44
      garden2b-150   food -0.30   shield +0.59   trade -0.23

Cost distribution over all priced city-turns, lategame-80:

    cost=0.000  12 380
    cost=0.500   1 111
    cost=0.667     305
    cost=0.333     234
    cost=0.250      74
    cost=0.750      67

**The 40% infeasible rate is the biggest open item here.** `valid=0`
means one of the two `cm_query_result()` calls found no arrangement
meeting `deep_cm_base()`'s minimal surpluses (food 0 or 1, shield 1).
The live `auto_arrange_workers()` relaxes through three fallbacks when
that happens; the counterfactual does not — it declares no opinion and
the gate flips on the unchanged threshold. That is the conservative
behaviour and it is correct, but it halves the mechanism's reach, and
mirroring the relaxation ladder is the obvious next increment.

### The solver bill

                                          lategame-80   garden2b-150
    extra cm_query_result calls per game    mean 1 434     mean 498
    counterfactual CPU per game             mean 5.046 s   mean 0.815 s
                                            max  7.791 s   max  2.315 s
    worst single turn                            0.292 s        0.099 s
    game wall time (12-way)                       ~450 s

The timer wraps the `cm_query_result()` pair only, so the figure
excludes the `deep_cm_peace_posture()` call that picks the alternative
(one `assess_defense()` plus a cached survival verdict per priced
city). That is a small understatement, stated rather than hidden.

**~1.1% of game time.** It is that cheap because of one design choice:
the counterfactual is evaluated INSIDE the free-flip test, not before
it. A charge can only ever revoke a flip — a cost does not create
demand — so a city the gate was not going to flip needs no price, and
the pair of solves runs on the flip candidates alone rather than on the
empire. Per-city-per-turn memoisation does the rest. No bail was
needed.

---

## 5. Score

lategame-80, 20 paired seeds, file-vs-file (`cm-off.ini` vs
`cm-all.ini`, all three mechanisms on at gain 1.0), both arms on the
same pinned binary:

    metric                       OFF        ON     delta      se      t   wins
    score                     168.20    167.43     -0.77    1.51  -0.51   10/20
    cities                      8.01      8.02     +0.01    0.17  +0.05    9/20
    unitsbuilt                 38.83     38.11     -0.73    0.74  -0.99    8/20
    techs                      53.08     52.91     -0.18    0.11  -1.55    6/20
    pop                        41.55     42.64     +1.09    0.73  +1.49   12/20
    mean_city_size              5.37      5.50     +0.14    0.09  +1.51   13/20
    settlers                    0.53      0.30     -0.23    0.11  -1.96    4/20
    munits                     15.59     16.17     +0.58    0.48  +1.19   13/20
    wonders                     0.24      0.21     -0.03    0.05  -0.70    6/20

Score is flat (t -0.51). The growth column moves in the direction the
mechanisms predict — `pop` +1.09 (t +1.49) and `mean_city_size` +0.14
(t +1.51) — which is what a revoked war footing and an admitted
grow-hard city are supposed to do, and `settlers` falls (t -1.96),
which is the same effect seen from the other side: a city kept in
grow-hard is a city not building settlers. Twenty seeds are direction,
not a claim, and all three gains are untuned placeholders at 1.0.

garden2b-150, 20 paired seeds, same two files, same binary:

    metric                       OFF        ON     delta      se      t   wins
    score                     252.45    256.38     +3.92    4.22  +0.93   11/20
    cities                     28.07     27.65     -0.42    0.68  -0.63    7/20
    unitsbuilt                150.97    158.03     +7.05    5.45  +1.29   12/20
    techs                      32.85     33.52     +0.68    0.58  +1.16   10/20
    pop                       113.15    116.28     +3.12    2.79  +1.12   10/20
    mean_city_size              4.37      4.48     +0.11    0.12  +0.93   12/20
    settlers                    4.15      4.15     +0.00    0.74  +0.00    8/20
    munits                     42.48     44.17     +1.70    1.88  +0.90   11/20
    wonders                     2.05      1.98     -0.07    0.21  -0.35    8/20

**This is not the venue split that has bitten this project seven
times.** Both cells are inside noise on score (-0.77 and +3.92, |t| <
1), and the growth column points the same way on both: `pop` +1.09 /
+3.12, `mean_city_size` +0.14 / +0.11. Where the two cells differ is
what growth is spent on -- lategame-80 loses settlers (t -1.96) and
garden2b builds more units (t +1.29) -- which is a downstream
allocation difference and not a disagreement about the mechanism.

Neither result is a claim. Twenty seeds are direction, three untuned
placeholders pinned at 1.0 are not a calibration, and every one of
these knobs ships at 0.

---

## 6. Specialist factors: half the design is inert on civ2civ3

lategame-80, 20 games, 9 328 player-turns:

                          lategame-80 (9 328)   garden2b-150 (5 933)
    gold factor  = 2      4 606   49%           3 718   63%
                 = 3      3 019   32%             943   16%
                 = 4      1 703   18%           1 272   21%
    science moved off 2     646  6.9%               0    0%

**The gold half works and fires on a third to a half of all
player-turns.** The
science half barely does, and the reason is the ruleset: civ2civ3's
tech upkeep at these tech levels is 0 or 1 bulbs per turn (observed
`upkeep=0` and `upkeep=1` dominate the census, `upkeep=10` on a
minority of late turns), so the shortfall fraction it scales is almost
always zero -- and on garden2b-150 it moved the factor on **zero** of
5 933 player-turns. That is a property to state, not a defect to fix — the
same shape as DARKFX's own "all three new credits are civ2civ3-only"
finding, in reverse: this one is inert *because* civ2civ3 charges
almost no tech upkeep. A ruleset that charges real upkeep would
exercise it.

**The honesty note that belongs with these two knobs**: they are the
CM's OUTPUT factors, so they weight gold and science coming off trade
tiles as well as the specialists. That is what DARKFX's design asked
for ("multiplying the two factors deep already sets") and it is
defensible — a broke empire does want its trade taken as gold — but it
is broader than the name "specialist" suggests, and a tuner reading
only the name would be misled.

---

## 7. What the next session must know

- **garden2b-150 IS reachable, but only from a tracked background
  runner.** DARKFX §8 recorded it as unreachable and that is true of
  foreground blocks: a 20-game arm makes 4 games in the first
  ten-minute block and 2 in the second, because the block is killed at
  ten minutes and a killed block loses every in-flight game (the DONE
  cache is per-game). Both arms here completed from a background
  command running `run.sh` in a relaunch loop until the DONE count
  reached 20 -- roughly 60 minutes per arm under a machine load of ~23
  on 12 cores. **That is the recipe; it is not blocked on the code.**
  isolated-150 was attempted the same way and abandoned for time, so
  it remains unmeasured for this epic.
- **The 40% infeasible rate** (§4) halves the counterfactual's reach.
  Mirroring `auto_arrange_workers()`'s relaxation ladder inside
  `deep_cm_counterfactual()` is a small, well-defined next increment.
- **The unit side of the grow-hard price** (§3) needs a snapshot of
  `city_data->choice` taken before the buy pass clears it. That is a
  `deepdefence.c` edit and belongs to whoever owns that file next.
- **`deep_cm_posture_delta()` has no consumer.** The fear machinery's
  `satisfied_fear_discount` is the intended one and the accessor is
  built for it. Note the coverage: it answers only for cities the war
  gate priced this turn.
- **Four PLACEHOLDERs for the cloud pile**: `cm_war_growth_charge`
  (0, range 0-1, subtracted from a probability so it cannot usefully
  exceed `cm_war_survival`), `cm_grow_price_blend` (0, range 0-4, a
  same-currency trade-off weight — 1.0 means "a citizen is worth what
  a building want unit is worth"), `cm_specialist_gold_gain` and
  `cm_specialist_sci_gain` (0, range 0-4). None has been searched.
- **Two counters the tuner should read before the score**, per the
  method note that has earned its place four times: `deep-cmpost`
  `kept=0` for whether the charge ever revokes anything at a given
  gain, and `deep-cmgrow` `fires=1` for whether the blend ever admits
  anything. At gain 1.0 both do (503 and 1 066); at smaller gains they
  may not, and a "no effect" reading would then be a non-execution
  reading.
</content>
