# CONSEQ + WRACE — consumers for signals we already had, and the wonder race

2026-08-07. Paired session on `epic/w03-horizon-content`, base
`85c7dfbad4`. Thirteen commits: one BALANCE parity fix, six CONSEQ, four
WRACE, two doc. `ai/deep`, `benchmark/` and doc only — zero
`ai/default`, `ai/baseline` or `server/advisors` changes.

Every mechanism ships at a compiled default of off. Every number below
was taken on a pinned binary with `--server`, `--arms deep`, and the
result key recording the tuning file.

**Pins.** `9619996eb4818e` (through the write-off ledger),
`724e885dc2ab61` (through the expiry item), `bdd23bb77f8b04` (through
the last-ditch ladder). `/tmp` was wiped by the host restart after the
last analysis, so the result directories are gone and every table here
is the analysis output, not a re-derivable artefact. The commands and
the arms are archived under `benchmark/tuning/`.

---

## 1. THE SESSION'S HEADLINE: the write-off signal is an alarm, not a verdict

The counter-claim machinery has declared cities unholdable since W14.
Its precision had never been measured. Everything the brief wanted to
hang off it destroys value irreversibly, so the census came first.

### 1a. Raw precision and recall

One ledger entry per city-turn written off, adjudicated
`survival_horizon` turns later against "is the city still ours". First
verdict per city, so a ten-turn siege scores as one call made ten turns
early rather than one call made correctly at the end.

    20 seeds, one arm, pin 9619996eb4818e

    cell          writeoff     first    lost   PRECISION   recall
                  city-turns   calls
    contested        2 326       964      27     0.028     31/32 = 0.969
    raging-120       3 279     1 430     195     0.136    228/336 = 0.679

Slices of the raw signal, contested:

    p < 0.001 (== p == 0)   n=455   lost=20   prec=0.044
    wave >= 8               n=263   lost= 4   prec=0.015
    wave >= 12              n=143   lost= 1   prec=0.007

**A bigger wave is LESS likely to take the city.** A large wave is a
passing army; the write-off's own "certainty" axis runs the wrong way.
Nothing in the raw signal reaches usefulness: the best slice is 4.4% on
contested and 23.0% (p == 0) on raging-120, the loss-richest cell in the
suite.

**Recall is the other half and it is the good half**: 96.9% of cities
lost on contested were flagged first. The signal is a near-perfect
ALARM and a near-useless VERDICT — 2 326 city-turns of alarm per 20
games to catch 31 of 32 losses.

### 1b. Why the raw number is the wrong one, and the conditional table

The write-off fires in the first-activities pass, BEFORE the defence
machinery answers. Most flagged cities survive precisely BECAUSE the
answer works. So the ledger was enriched to record, at the moment of
the call, everything a consumer would have to condition on: `relief`
(the best survival the whole reachable pool including the poach
reaches), `pool`, `adj` (at-war military units on an adjacent tile) and
`buy` (whether the treasury could rush this city's queue that turn).

    contested, 40 games (both arms of the ladder A/B), 1 966 adjudications

    slice                                       n     lost   precision
    RAW write-off                            1 966      53     0.027
    p < 0.01                                 1 143      45     0.039
    p < 0.01 & relief <= p (no reinforcement)  841      41     0.049
    ... & enemy ADJACENT                       109      22     0.202
    EXHAUSTED (... & rush unaffordable)         71      14     0.197
    enemy adjacent alone                       195      24     0.123
    adj >= 2 & p < 0.01 & relief <= p           38      12     0.316
    adj >= 3 & exhausted                         6       4     0.667

Three things this table settles:

- **Adjacency carries the entire lift.** 0.049 → 0.202 is a 7x move and
  it is all the `adj` term. Adding "the rush is unaffordable" moves it
  to 0.197, i.e. nothing: the treasury condition is not informative and
  should not be in a gate.
- **Even the most exhausted slice this ledger can name holds two times
  in three.** An irreversible act at 0.20–0.32 precision loots two
  survivors for every casualty.
- **The prior for any last-ditch decision is "it holds".** That is the
  reason rung 2b below is a priced comparison and not a gate.

---

## 2. The last-ditch defence ladder

Human design, 2026-08-07: "sell a bldg then buy defense unit as a
lastditch", refined into a ladder because the rungs have different
bars.

### 2a. Rung 1, SELL TO DEFEND — built, executes, 100% conversion

A defensive move, not a concession: the building is spent to KEEP the
city, so the misfire cost is one building against a saved city and the
trigger can be loose — the emergency-buy SHORTFALL plus the write-off
family's own below-target-against-a-live-wave read. Stock's sibling
fires only on `grave_danger` with literally no defence present.

    contested, 20 paired seeds, pin bdd23bb77f8b04
    identity.ini  vs  conseq-lastditch.ini (sell_defend_gain 1.0,
                                            late_buy_gate 1)

    metric                  OFF        ON     delta      se      t   wins
    score                 85.10     84.46     -0.64    0.53  -1.22   4/20
    cities                 4.59      4.63     +0.04    0.03  +1.44   6/20
    mean_city_size         5.94      5.77     -0.17    0.09  -2.00   5/20
    pop                   26.82     26.57     -0.26    0.21  -1.20   7/20
    unitslost             13.24     12.89     -0.35    0.41  -0.86   7/20
    CITY LOSSES            1.60      1.55     -0.05    0.34  -0.15   4/20
      shortfalls seen      0.00     41.05    +41.05    6.68  +6.14  18/20
      buildings sold       0.00      2.65     +2.65    0.66  +4.03  15/20
      ... that bought      0.00      2.65     +2.65    0.66  +4.03  15/20
      late buys            0.00      0.00     +0.00    0.00    n/a   0/20

**Verdict: the mechanism is exactly right and worth about -0.6 score at
face value on this cell.** Conversion is perfect — every one of the
2.65 sales a game completed the buy, which is what the design promised
and what a shortfall-triggered rung should do. The loss guard passes
(city losses -0.05). The cost is visible and attributable:
`mean_city_size -0.17` at t -2.00, which is the buildings themselves.

What it sold, 20 games: **44 Barracks, 3 Temple, 2 Aqueduct, 1
Colosseum.** Barracks is 83% of the population, which says the bar
(`sale price <= gain x rush cost`) is mostly selecting the cheapest
militarily-idle building — the right instinct, and an argument that the
tuning question is the BAR and not whether to do it.

Twenty seeds are direction. The sign is negative and the mechanism is
sound, which is a tuning question (the bar, and the venue) rather than
a design one.

### 2b. Rung 2b, THE DENIAL SALE — design and prior, deliberately not built

The brief's original item 2 ("in the final window, sell the highest-
value sellable building") is retired **as a gate** on this session's own
census: at 0.20 precision it would loot four survivors in five, and at
the tightest nameable slice (adj >= 2) still two in three.

What replaces it is a priced comparison, and §1b is its prior: **the
stand-and-fight branch wins by default and the write-off branch must
clear a measured bar.** The design, for whoever builds it:

    value(fight)  = P(hold) x city worth
                  + (1 - P(hold)) x [ what resisting COSTS the taker ]
    value(deny)   = P(hold) x (city worth - sale value destroyed)
                  + (1 - P(hold)) x [ gold banked
                                      + denial of the building to the taker ]
    act only when value(deny) - value(fight) > sell_deny_margin

`P(hold)` is `1 - precision(slice)`, and the slice is nameable from the
ledger this session shipped: it is `adj`, `relief` and `p`, and the
table in §1b is its calibration.

**Two server mechanics verified for the "what resisting costs the
taker" term, because the human's presumption needed checking:**

- **Capture costs the city exactly one size point, fixed.**
  `unit_conquer_city()` calls `city_reduce_size(pcity, 1, pplayer,
  "conquest")` once (`server/citytools.c:2141`). A longer siege does not
  by itself deliver a smaller city.
- **But each successful attack does.** With `killcitizen` set (the
  server default) and the attacker's unit class carrying
  `UCF_KILLCITIZEN`, and absent `EFT_UNIT_NO_LOSE_POP`, every successful
  attack on the city runs `city_reduce_size(pcity, 1, ...)`
  (`server/unithand.c:4601-4607`). So resisting DOES shrink the prize,
  per successful attack rather than per turn.
- **Razing is a separate, ruleset-scaled term.** `raze_city()` destroys
  each sellable improvement with probability `razechance` (civ2civ3:
  10, +30 for barbarians), so a fraction of the buildings the denial
  sale would sell are destroyed on capture anyway and must not be
  double-counted as denial.

`sell_deny_margin` is the one PLACEHOLDER the item needs and it is not
registered here, because a knob for an unbuilt mechanism is noise in
the audit.

### 2c. Rung 2a, CIVILIAN EVACUATION — built, not yet A/B'd

Split from the sale precisely because the bars differ. A civilian
contributes exactly zero to the defence (`deep_collect_defenders()`
skips `UTYF_CIVILIAN`, so the survival probability the pass just
computed is unchanged by its leaving) and dies with the city. Being
wrong costs a few unit-turns of walking, so this half keeps the LOOSE
compound gate.

Refuge is the nearest own city on the same continent that is not itself
below survival target — walking a worker out of one doomed city into
another is not an evacuation. Counters `deep-evac`, `deep-evac-city`,
`deep-evac-sum`; arm archived as `benchmark/tuning/conseq-evac.ini`.
**The A/B was not run** (the host restart took the pins and `/tmp`).

### 2d. Rung 3, THE FINAL DEFENCE BUY — built, ZERO POPULATION

Spends gold the turn ADDED since the main buy pass, on the triage
population only, at the last statement of `deep_do_last_activities()`.

**It never fired: 0.00 `deep-latebuy` lines a game across 20 games, and
zero `deep-latebuy-sum` lines, meaning the budget was never positive.**
In today's phase ordering nothing credits gold between the main buy
pass (with the watermark taken after `deep_balance_treasury()`) and the
end of deep's phase. Rung 1's own sale is spent inside
`deep_spend_gold()` the moment it happens, so rung 3 is not what banks
it.

The pass stays, off, as the seam for any future reordering that moves
an earner after the buy — and the counter is what will say so. Size the
machinery against the population it would act on: this one is empty
today.

---

## 3. CONSEQ item 6 — the birth guard priced

The inherited first-defender rule pins danger to a flat 100 when
`urgency == 0` and fires `DWT_EMERGENCY`. Replaced by
`P(walk-in) x ai_city.worth x birth_guard_gain` at `DWT_NORMAL`,
through the stance fear gate, where `P(walk-in)` is the union of the
survival model's reachability verdict and the server's own three
barbarian probability factors.

    contested, 20 paired seeds, pin 724e885dc2ab61
    identity.ini vs conseq-birth.ini (birth_guard_gain 1.0)

    metric                  OFF        ON     delta      se      t   wins
    score                 85.10     84.99     -0.11    2.49  -0.05   8/20
    cities                 4.59      4.74     +0.16    0.12  +1.29  15/20
    munits                13.39     14.48     +1.09    0.70  +1.57  13/20
    mean_city_size         5.94      5.65     -0.29    0.18  -1.64   6/20
    CITY LOSSES            1.60      1.80     +0.20    0.47  +0.42  11/20
    cities gained         38.25     39.75     +1.50    1.01  +1.48  15/20
      priced firsts        0.00    747.75  +747.75   56.94 +13.13  20/20
      census player-turns 197.40    476.65  +279.25   23.95 +11.66  20/20

**The mechanism's own arithmetic is the finding, not the score.** Over
14 955 priced first-defender decisions in the ON arm:

    P(walk-in) == 0 exactly        88.6% of all pricings
    mean P(walk-in)                0.03      median 0.00
    mean p_mil / p_barb            0.02 / 0.01
    mean city worth              339.65      median 308.00
    mean priced want              13.11      median 0.00
    priced BELOW the flat 100      96.0%

**The inherited rule paid a flat, emergency-tier 100 on ~14 000
decisions where nobody could reach the city and no uprising was
possible.** That is derby 3's turn-2 Warriors cohort, explained and
priced. Correcting it is score-neutral on contested (-0.11, t -0.05)
and buys a small, consistent expansion move (cities +0.16 on 15/20,
cities gained +1.50) with the loss guard passing (+0.20, t +0.42).

The brief demanded both numbers before trading safety for tempo. Both
are here and the trade is roughly free on this cell; the tempo it buys
is small because contested is a cell where deep holds ~4.6 cities.

---

## 4. CONSEQ items 1, 4, 5 — built, execution proven, unmeasured for score

Execution probe: contested, 10 seeds, pin `724e885dc2ab61`,
`benchmark/tuning/conseq-wrace-probe.ini` (every new knob at a
face-value 1.0 — the term's own arithmetic, no tuned level). Scores
from this arm are not claims; the counters are.

    counter                       per game
    deep-birth (priced)             806.5
    deep-birthguard (player-turns)  497.9
    deep-defwall (tech routings)    226.5
    deep-damper pump=attacker       163.2
    deep-damper pump=diplomat       184.2
    deep-wrace (evaluations)        490.2
    deep-wrace-end (resolved)         3.5
    deep-wrace-abandon                0.3
    deep-wrace-feed                   0.0   <- see §5c
    deep-expiry                    1545.8   (15 398 building / 60 unit)

**The defensive tech wall (item 1)** routes 226 credits a game into
`tech_want[]` for the advance gating a better defender. H9's hole,
closed on the military side.

**The never-launches dampers (item 5)** both fire in three figures a
game. The attacker damper's saturation is a rate (ops formed that never
reached MARCH, over all formed); the diplomat damper's is a count of
agents walking plus queued — the explorer divisor's shape on two more
pumps, with no success-rate estimate invented for either.

**Expiry-aware amortization (item 4)** is 15 398 building truncations
to 60 unit deferrals — a 250:1 split. The unit half is rare because it
requires a successor whose full requirement conjunction lands inside
the current unit's build time AND a city with no danger and no urgency;
the tier discipline is doing exactly what it was written to do. The
building half is common because civ2civ3 puts a Player-range `Tech`
`obsolete_by` on 16 buildings that are buildable early and often.

**Obsolescence semantics, verified and stated** (the brief asked):

- An obsolete **improvement is AUTO-SOLD** by the server for
  `impr_sell_gold()` (`remove_obsolete_buildings_city()`,
  `server/cityturn.c:236`).
- An obsolete **wonder is NOT sellable at all** —
  `is_building_sellable()` is `is_improvement()`
  (`common/improvement.c:1122`) — so the monument stands and pays
  nothing.
- **Either way the effects lapse**: a `VUT_IMPROVEMENT` requirement
  whose building is obsolete evaluates `TRI_NO`
  (`common/requirements.c:2360`), and
  `can_city_build_improvement_now()` refuses it (`common/city.c:868`).
- **Whose research triggers it: the OWNER's.** `improvement_obsolete()`
  builds a `req_context` carrying `.player` and nothing wider
  (`common/improvement.c:413`). All 16 civ2civ3 `obsolete_by` tech
  requirements are `REQ_RANGE_PLAYER`. Wider ranges are legal in the
  vector; `deep_impr_obsolete_eta()` skips rather than guesses at them.

---

## 5. WRACE — the estimator, its calibration, and one empty consumer

### 5a. What was built

`ai/deep/deepwonder.c`, a new module: `deep_tech_eta()` (own research
only — stock's obsolescence estimator takes a MIN over every player's
bulb rate, which is a rival model wearing a minimum sign),
`deep_impr_obsolete_eta()` (MINIMUM over the disjunctive `obsolete_by`),
`deep_utype_successor_eta()` (MAXIMUM over the successor's conjunctive
build requirements), `deep_wonder_rival_eta()` — **the one function W04
substitutes** — `deep_wonder_p_win()`, and the race ledger.

The composed valuation the human wrote down on 2026-08-05 is now one
expression: `P(win) x want x completion-ETA discount x obsolescence
truncation`, with the two truncations this session owed built and the
other factors arriving from DARKFX and ENDGAME.

### 5b. The calibration, and it is the stop condition's question

    contested probe, 10 seeds: 4 902 evaluations, 261 declined (5.3%)
    estimated P(win): mean 0.938, median 1.000
    composition factor at face-value gains: mean 0.292, median 0.256

    races resolved (wonder became unavailable):  35
      won  13        lost  22        observed win rate 0.371
      losses recorded at estimated p >= 0.75:  2 of 22

**The estimator is over-optimistic and the reason is structural and
known: it only counts rivals who have ALREADY STARTED.** A rival who
starts tomorrow is invisible today, so the median estimate is 1.000
("uncontested") and the observed win rate among resolved races is
0.371. This is exactly the calibration counter the brief asked for, and
it fires.

It does not trip the brief's stop condition in the form stated — only 2
of 22 losses were recorded at p >= 0.75, so the estimator's *confident*
predictions are not the ones failing; what fails is its default of
"nobody is racing me". **The go/no-go therefore UNDER-declines**, which
is the safe direction for a mechanism that ships off, and the fix is a
prior on unstarted rivals rather than a different margin.

Note also that the resolved set includes wonders deep merely evaluated
and never started, which inflates the loss count. The honest reading is
"the estimator says uncontested far too often", not "deep loses 63% of
the races it enters".

### 5c. Race-to-win has no population on contested

`deep-wrace-feed` is **0.0 a game** in both probes. Diagnosis, in order:

1. The first probe did not set `wonder_merit`, and the help-wonder
   caravan pass is gated on it — the probe's fault, not the
   mechanism's. Fixed and re-run.
2. With `wonder_merit = 1`: 659 caravan evaluations over 10 games with
   a real wonder city (`wcity` set), and still zero feeds. The
   multiplier returns 1.0 whenever there is no rival building the same
   wonder, and on contested **there never is** — which is the same fact
   the median `P(win) = 1.000` reports from the other side.

**Race urgency is a mechanism with no population in this venue.** The
brief named garden2b/long/primary as the venues where wonders
concentrate, and:

3. **primary-120 could not be run.** Ten seeds returned 0 DONE after
   two full foreground blocks; the DONE cache is per-game so a killed
   block loses everything in flight. This is DARKFX's and ENDGAME's
   standing finding, unchanged: **every long-horizon question in this
   project is blocked on the harness, not on the code.** It was made
   worse here by a parallel session driving the box to load average 34
   on 12 cores.

---

## 6. Item 3 (home-city upkeep redistribution) — not built

Not reached. No census, no code, no claim. The brief's stop condition
("the rehome action's server constraints don't match the design") was
not tested and remains open.

---

## 7. Off-switch proof

    smoke cell, 10 paired seeds, identity.ini on both binaries
    pin 9619996eb4818e  (through the write-off ledger)
    pin 724e885dc2ab61  (through the expiry item)

    10/10 scorelogs BYTE-IDENTICAL.

That covers the tech wall, both dampers, the whole WRACE module and
both expiry consumers. The three commits before the first pin are inert
by construction and by counter: the alliance clamp is unreachable at
`gift_love_gain = 0` (`deep_love_gift()` is never called), the birth
guard returns FALSE before reading anything at `birth_guard_gain = 0`,
and the write-off ledger only reads and logs. The last-ditch ladder and
the evacuation were pinned after this proof and are gated the same way;
their off-arm counters are 0.00 in the A/B above.

`meson test -C build` **4/4 OK**. `python3 benchmark/tune-audit.py`
**CLEAN 192/192/192/192** (was 177 at session start).

---

## 8. Fifteen new knobs, all PLACEHOLDER, all off

    birth_guard_gain           0.0   P(walk-in) x worth, first defender
    defensive_techwall_weight  0.0   ratio: research turns per fear turn
    attacker_hold_damper       0.0   1/(1+g x unlaunched share)
    diplomat_pump_damper       0.0   1/(1+g x agents walking+queued)
    wrace_margin_turns          10   the P(win) ramp SCALE, not a bar
    wrace_pwin_gain            0.0   the race term
    wrace_eta_gain             0.0   completion-ETA discount
    wrace_obsolete_gain        0.0   obsolescence truncation
    wrace_urgency_gain         0.0   help-wonder race urgency
    expiry_building_gain       0.0   truncate a credit at its own void
    expiry_unit_gain           0.0   successor-imminent deferral
    expiry_unit_gap              2   its hysteresis, in turns
    sell_defend_gain           0.0   rung 1's BAR (sale <= g x rush)
    late_buy_gate                0   rung 3 on/off
    evacuate_gate                0   rung 2a on/off

First runs owed, in order of what the evidence already points at:

1. **`sell_defend_gain`, the bar.** The mechanism converts perfectly
   and costs city size; the question is which buildings clear the bar,
   and 83% of today's sales are Barracks.
2. **`birth_guard_gain`.** Face value is score-neutral with a small
   expansion gain. The sweep is against `garrison_floor` and
   `barbarian_fear`, which fund the same body from the other side.
3. **`wrace_*` as a block, in a venue that has races.** Not contested.
4. **`expiry_building_gain`.** 15 398 truncations a game is the largest
   new population in the session and it has never been scored.

---

## 9. What the next session must know

- **The write-off signal's precision is 0.027 raw and 0.202 on its best
  nameable slice, with recall 0.969.** Any future consumer must
  condition on ADJACENCY — it carries the entire lift — and must not
  condition on the treasury, which carries none. The prior for a
  last-ditch decision is "the city holds".
- **Rung 3 has zero population.** The end-of-phase seam finds no money
  in today's ordering. Do not build on it; the counter will say when
  that changes.
- **The wonder-race estimator's default is its bug.** "Nobody has
  started, therefore I win" produces a median P(win) of 1.000 against
  an observed 0.371. A prior on unstarted rivals is the fix, not a
  different margin.
- **Race urgency, and probably the whole wonder round, needs a venue
  this harness cannot reach.** primary-120: 0/10 DONE after two blocks.
  garden2b: DARKFX's same wall. This is now three sessions.
- **A parallel session on the same box changes benchmark wall-clock by
  3x** (load average 34 on 12 cores). Budget accordingly, or the
  10-minute foreground block silently becomes a 3-game block.
- **`/tmp` is wiped by a host restart, pins included.** Everything in
  this document is analysis output that cannot be re-derived without
  re-running the games.
- **Item 3 (home-city upkeep redistribution) was never reached.**
