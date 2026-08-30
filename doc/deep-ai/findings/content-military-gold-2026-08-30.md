# Content, military and gold: the three secondary signatures, localized (2026-08-30)

Subject: hypotheses 3 and 4 and the "Secondary signatures" section of
`defaults-deficit-census-2026-08-25.md`. **Log census on finished games
only** — the 20-seed gates-off holdout arms of `tune-postfix`
(`s6-nogates-war` = contested 32x32, 8 players, T151;
`s6-nogates-econ` = lategame-80 40x40, 6 players, T81), each paired
against its own baseline arm on the same seeds and maps, plus three
probe games run only to verify this session's instrument commit.
Tool: `benchmark/contentcensus.py`.

Units, throughout: **per player**, not per empire. The 2026-08-25
census quoted empire sums over 8 players ("munits 7.3 vs 1.0",
"treasury 250-450 vs ~2000"); those are 0.92 vs 0.12 and 32.4 vs 264.7
per player. Every number below is reproduced at the per-player scale.
Errors are paired per-seed SEs (n = 20).

---

## 0. What the score gap is, and when it opens

`get_civ_score()` = citizens + 2 x techs + 5 x wonders + built/10 +
killed/3 + culture/50. At T151, contested, deep - baseline:

| term | deep | base | diff |
|---|---|---|---|
| citizens | 26.54 | 30.53 | **-3.99** |
| techs x2 | 39.29 | 41.53 | **-2.24** |
| culture/50 | 10.63 | 12.70 | **-2.07** |
| wonders x5 | 1.52 | 0.85 | +0.67 |
| units built/10 | 5.03 | 4.99 | +0.04 |
| units killed/3 | 3.30 | 3.10 | +0.20 |
| **score** | **85.11** | **92.44** | **-7.33** |

(The arm means differ by -7.33; the paired per-seed estimate, which is
the one with an error bar, is **-7.50 +/- 2.84**. The two differ only
because seeds carry slightly different numbers of surviving players.)

Citizens per city: deep 5.67, baseline 5.72. **The three negative terms
are one quantity.** Everything scales with the city count:
cities 4.68/5.34 = 0.876, citizens 0.869, settled area 0.883,
culture 0.837. There is no independent size, tech or culture defect to
find.

And the city gap is created in one 25-turn window and never changes
again:

    cities, deep - base    T25  -0.12 +/- 0.06
                           T50  -0.11 +/- 0.07
                           T75  -0.63 +/- 0.14
                          T151  -0.65 +/- 0.16

Econ venue, same shape one venue-clock earlier: -0.47 +/- 0.10 (T25),
**-1.15 +/- 0.20 (T50)**, -1.05 +/- 0.21 (T81); score -8.11 +/- 2.41.

**T50-T75 (contested) / T20-T50 (econ) is the causal window.** A
signature that is absent from that window cannot be the lever, however
large it is later. That test decides the ranking at the end.

---

## 1. Content: the Colosseum

### It is deep's want, not stock's

The pick is real: `deep-want` winners by band, share of city-turns
(a line per city per turn — the item the city is holding, not a start):

    T  1-25  Settlers 79.8%  Warriors 7.2%  Diplomat 4.5%
    T 26-50  Settlers 44.5%  Diplomat 12.4%  Workers 10.2%  Phalanx 9.5%
    T 51-75  Settlers 14.9%  Phalanx 13.9%  Diplomat 11.9%  City Walls 11.5%
    T 76-100 Phalanx 16.4%  Settlers 14.8%  Colosseum 11.1%  Chariot 9.3%
    T101-125 Colosseum 23.1%  Chariot 11.9%  Phalanx 11.6%  Barracks 9.0%
    T126-150 Colosseum 18.7%  Chariot 10.3%  Barracks 8.3%  Courthouse 8.1%

**Colosseum is 0% of picks before T76.** The 23% is real and it is the
single largest queue occupant of the last third of the game.

The seat is `deep_content_credit()` (`ai/deep/deephorizon.c:657`),
called from the horizon pass at `deephorizon.c:2227`. Stock prices a
Make_Content building at exactly zero — `advdata.c`'s `impr_calc`
switch has the whole happiness family `#if 0`'d out under a TODO, so
`base_want()` returns 0 on its first line and the building falls
through to `ADV_IMPR_ESTIMATE`. `deep-decay-sum` separates the two
authors on every line (`pre` = stock's array value, `add` = deep's
credit); summed over 20 games:

| improvement | stock `pre` | deep `add` | deep's share |
|---|---|---|---|
| Colosseum | 914 195 | 2 836 855 | **75.6%** |
| Temple | 707 112 | 2 698 587 | 79.2% |
| Courthouse | 873 166 | 2 194 546 | 71.5% |
| Hanging Gardens | 12 538 | 1 456 947 | 99.1% |
| Aqueduct | 840 689 | 120 195 | 12.5% |

### Two thirds of the credit is paid for citizens who do not exist yet

`deep_content_credit()` computes
`load = feel[UNHAPPY][FINAL] + entertainer specialists`,
`arriving = horizon / turns-to-grow`, `freed = MIN(amount, load +
arriving)`, and amortizes with `delay = (load > 0 ? 0 : ttg)`. That
`delay` is already in the log (`deep-decay fam=content`) and it is the
diagnostic: **delay == 0 means somebody is on pacification duty today;
delay > 0 means nobody is, and the credit is for citizens the growth
clock is expected to deliver.**

| improvement | credit lines | load>0 | speculative | spec% | mean credit | mean delay |
|---|---|---|---|---|---|---|
| Courthouse | 36 090 | 9 526 | 26 564 | 73.6% | 63.7 | 7.3 |
| Temple | 34 965 | 8 533 | 26 432 | 75.6% | 80.1 | 6.9 |
| Hanging Gardens | 23 008 | 5 226 | 17 782 | 77.3% | 63.3 | 6.8 |
| **Colosseum** | **18 654** | **6 296** | **12 358** | **66.2%** | **161.0** | **8.1** |
| Mausoleum | 14 488 | 4 486 | 10 002 | 69.0% | 66.3 | 8.5 |
| Temple of Artemis | 2 301 | 285 | 2 016 | 87.6% | 105.9 | 7.9 |

Econ venue, where cities are already large: 51.8% speculative
(Colosseum), 52.3% (Temple).

The **ordering** is sane and is not the defect. Mean credit ranks
Colosseum (161) > Temple of Artemis (106) > Temple (80) > Mausoleum
(66) > Courthouse (64) > Hanging Gardens (63), which is the ruleset's
own `EFT_MAKE_CONTENT` amount ordering (Colosseum 3, Temple 1+1)
against the same per-citizen tile value. Cathedral has **0 starts** in
either venue (Monotheism is never reached). Marketplace is not a
content building and competes on its own credit (60.4% deep-authored).

### The gold bill on the credit is a placeholder

Colosseum upkeep is 3 gold/turn (civ2civ3 `build_cost` 70). The credit
charges `city_improvement_upkeep x deep_decay_duration(MORT) x
upkeep_gold_weight` = `3 x 24 x 0.125` = **9 want-units against a mean
credit of 161** (5.6%). `upkeep_gold_weight = 0.125` is marked
`PLACEHOLDER, untuned` at `ai/deep/deeptuning.c:160`, and its own
comment says the knob "owes the tuning pile a value". A gold is
currently worth one eighth of a shield-equivalent to the building
auction.

### But the outcome falsifies the hypothesis as stated

The 2026-08-25 census called this "preemptive content-building at
enormous scale". The build *intent* is exactly that. The *outcome* is
not.

**Deep ends the game with fewer content buildings than the arm that
prices them at zero.** End-state inventory from the final saves, per
player: Colosseum deep **1.48** vs baseline **2.00**; Temple 1.22 vs
1.63. Per citizen: 0.056 vs 0.066. Deep is under-supplied, not over.

**And deep's realized pacification load is at parity or better.** Riots
are 0.00-0.01 per player all game in both arms; luxrate is 0 in both.
Load = unhappy-after-everything + entertainer-equivalent specialists,
deep - baseline:

    T25 +0.00   T50 +0.49   T75 +0.72   T100 +0.57
    T125 -0.15  T150 -0.81   (with deep carrying 3.87 FEWER citizens)

Per citizen the two arms pacify identically. The Colosseums bought a
wash.

The reason both are true at once is the delivery rate:

    Colosseum  1 645 runs started   263 completed (16.0%)   1 266 abandoned
               115.1 shields/player-game delivered
                26.0 shields/player-game burnt by change-production penalties
               183.4 shields/player-game left carried in runs that never delivered
                 8 of the delivered ones later SOLD by deep-fin at 2-4 gold

Econ venue: 993 starts, 79 built (**8.0%**), 45.7 shields/player-game.

**The Colosseum is not an over-investment. It is a want large enough to
hold a fifth of the queue and too short-lived to finish, and it arrives
after the deficit is already frozen.** By band, delivered content
shields per player-game (contested): 0.0 / 2.6 / **12.8** / 35.7 / 72.2
/ 78.9 — 12.8 shields, less than half a settler, in the band that
creates the entire city gap.

---

## 2. Military: the early army

### The numbers

Standing military units per player (`munits`):

| turn | deep | base | paired diff |
|---|---|---|---|
| 10 | 0.74 | 0.07 | |
| 15 | 0.89 | 0.11 | |
| 25 | 1.19 | 0.35 | **+0.84 +/- 0.10** |
| 50 | 3.05 | 0.94 | **+2.11 +/- 0.12** |
| 75 | 6.20 | 3.68 | +2.52 +/- 0.36 |
| 151 | 15.55 | 12.73 | +2.82 +/- 0.99 |

Econ: T20 2.27 vs 0.44, T25 **3.98 vs 1.07** (+2.92 +/- 0.14), T30 5.52
vs 1.83. **Baseline has built literally zero units by T10 in the
contested venue** (`unitsbuilt` 0.00) while deep has built 0.67.

### The want path, measured at the seat

The archive could not answer this. `deep-def` reports **stock's**
`assess_danger()` and nothing else — `deep_log_defence()`
(`deepdefence.c:143`) is called from `deep_do_first_activities()`
immediately after `dai_assess_danger_player()` and *before*
`deep_credit_incoming_defence()` injects anything, by design, "so
before and after arms measure need on the same scale". Every injected
term was invisible: `deep_overseas_danger`, `deep_boat_shadow_fear`,
`deep_survival_deficit_fear` and `deep_apply_capital_stake` write no
per-city term anywhere; `deep_density_fear` writes `deep-density`
**only when `n_near > 0`**, so a zero is indistinguishable from an
absence; `deep_density_prior()` (`deepdefence.c:731`) has **no log line
at all**; and `deep-floor` writes 76 lines a game, so the garrison
floor is not the volume driver.

This session's `deep-fear ... site=build` line closes that (§5). Three
probe games, 11 395 build-seat evaluations, share of them carrying each
term:

| band | n | `pre` (stock) | `density` | `shadow` | `barb` | `surv` | `prior` | `over` | post>0 |
|---|---|---|---|---|---|---|---|---|---|
| T1-25 | 552 | 12.5% | **54.9%** | 0.0% | 0.0% | 0.2% | **0.0%** | 0.0% | 27.2% |
| T26-50 | 1 468 | 4.5% | **72.1%** | 3.3% | 0.0% | 0.5% | **0.0%** | 0.0% | 20.4% |
| T51-75 | 2 022 | 15.3% | **71.7%** | **30.9%** | 0.0% | 5.7% | **0.0%** | 0.0% | 20.9% |
| T76-100 | 2 269 | 30.8% | 70.9% | 42.3% | 0.0% | 9.2% | 0.0% | 0.1% | 35.9% |
| T101-125 | 2 528 | 37.9% | 65.9% | 40.5% | 56.0% | 3.9% | 0.0% | 0.3% | 41.5% |
| T126-150 | 2 556 | 27.0% | 55.6% | 42.1% | 100.0% | 3.1% | 0.0% | 0.0% | 29.0% |

Mean magnitude per evaluation (and, in brackets, per evaluation where
the term is present):

    T  1-25   pre 0.57 [4.6]   density 0.99 [1.8]                      post 2.03
    T 26-50   pre 0.37 [8.2]   density 2.08 [2.9]   shadow 0.82 [24.8] post 1.83
    T 51-75   pre 5.26 [34.4]  density 2.10 [2.9]   shadow 7.72 [25.0] post 7.38
    T 76-100  pre 12.71 [41.3] density 2.14 [3.0]   shadow 10.58[25.0] post 14.85

**Two anticipatory terms, and both are step functions of geography
rather than of threat.**

- **`deep_density_fear()` owns the opening.** It is present on 54.9% of
  T1-25 evaluations against stock's 12.5% — 4.4x as often — and 72.1%
  at T26-50 against 4.5%, where it supplies 2.08 of a 2.45 pre+density
  sum (85%). It is a sum over non-allied cities within
  `density_fear_radius` (8) at `density_fear_value (6) / (1 +
  distance)`, with no capability test on land at all.
- **`deep_boat_shadow_fear()` joins it at T51 and is the largest single
  term in the causal window.** Its conditional value is exactly 25.0 —
  the `boat_shadow_fear` knob — because it *is* a flat constant:
  `deepdefence.c:453` returns the knob for **every coastal city** as
  soon as **any player we are at war with, anywhere on the map, owns
  any transport-capable ship**. No distance, no count, no sizing, no
  decay. It fires on 30.9% of T51-75 evaluations and 42% thereafter.

Two things the instrument *corrected*, and both were live errors in
this investigation before the probe ran:

- **`deep_density_prior()` never fires — 0.0% in every band, all three
  seeds.** It returns 0 the moment any met non-allied player exists,
  and on a 32x32 eight-player map that is turn one. This census's own
  working hypothesis was that the prior drove the opening. It does not.
- **A single-seed read is not enough here.** Seed 101 alone put density
  at 88.2% of T1-25 evaluations and showed no boat shadow at all; the
  three-seed figures above are 54.9% and 30.9%. The seat conclusion
  survives; the magnitudes did not.

Corroborating from the archive, on a different population: of the 400
T1-25 city-turns whose `deep-want` *winner* was a military unit, 94.0%
came from a city whose `deep-def` (stock-assessed) danger was 0 that
turn, 96.2% at T26-50 and 82.4% at T51-75 (econ T1-25: 83.8% on 10 181
picks). The mix is Warriors 46% / Phalanx 22% / Archers 20% at T1-25
(contested) and Riflemen 63% / Musketeers 11% (econ) — the defender
ladder, not the attacker one; `deep-kill` (the attacker want) is first
written at T7 and carries `want=0`.

`density_fear_ramp_turns` — the knob whose own comment says "a
neighbour's mere existence is not a capability... deep paid T5
insurance for a T60 storm in the Europe playtest" — is **0 in
`defaults.ini`**, i.e. the density term runs at full weight from turn
one. It does not ramp the boat shadow, which is deliberately excluded
from the ramp as an "evidence-based term" that "already gates on real
capability signals" — and the gate it actually applies is *one enemy
transport existing anywhere in the world*.

### The danger was not real

Combat outcomes, deep - baseline, paired:

| | T25 | T50 | T75 |
|---|---|---|---|
| units lost | **-0.04 +/- 0.02** | +0.03 +/- 0.08 | -0.04 +/- 0.23 |
| units killed | -0.04 +/- 0.02 | +0.03 +/- 0.07 | +0.13 +/- 0.21 |

Econ T25: lost -0.04 +/- 0.03, killed -0.02 +/- 0.04. **Deep's 3-10x
army is indistinguishable from baseline's on every combat outcome
through T75, in both venues.** The earliest `deep-loss` entries are
`reason="barb_unleash"` from huts. No city in either arm is attacked
before T50.

### What it cost — and what it did NOT cost

**Upkeep is not the cost, and the 2026-08-25 hypothesis 4's "upkeep
drag" does not exist in this ruleset at this army size.** In civ2civ3
`uk_gold` is 0 for **every** unit type (57/57), so unit gold upkeep is
identically zero; `effect_base_unit_upkeep_shield` excludes Despotism,
Monarchy, Fundamentalism, Federation and Democracy, and 139 of deep's
160 contested player-games end in Monarchy (124) or Despotism (15); and
`effect_upkeep_free_units_food_base` gives every city 4 food-free
units, against deep's 0.97 military units per city at T50. The early
army is free to keep.

The cost is entirely the shields to build it, and the timing. Delivered
shields per player-game by band, contested:

| class | T1-25 | T26-50 | **T51-75** | T76-100 | T101-125 | T126-150 |
|---|---|---|---|---|---|---|
| military units | 16.1 | 33.4 | **59.7** | 103.2 | 106.8 | 99.2 |
| military buildings | 0.6 | 13.1 | **71.4** | 51.9 | 69.9 | 82.7 |
| diplomat | 7.7 | 26.8 | 36.0 | 27.6 | 19.9 | 20.1 |
| **expansion** | 3.8 | 18.2 | **15.4** | 19.3 | 9.6 | 5.8 |
| civilian | 0.6 | 14.4 | 25.8 | 6.0 | 5.4 | 4.6 |
| content | 0.0 | 2.6 | **12.8** | 35.7 | 72.2 | 78.9 |

**In T51-75 — the band that creates the whole city deficit — deep
delivers 131 shields per player of military hardware against 15 shields
of settlers (half a settler), and 12.8 of content.** Econ T1-25: 186.0
military vs 36.7 expansion, a 5:1 split in the first 25 turns of a game
that starts with eight settlers in hand.

Shield surplus (`mfg`, per player) is 20-40% below baseline through the
whole opening: 1.51 vs 2.42 (T5), 2.85 vs 3.50 (T15), 4.41 vs 5.58
(T25, -1.17 +/- 0.16), 11.19 vs 12.35 (T50). Since unit upkeep is free
here, that gap is fewer and differently-arranged cities, not the army's
maintenance — it is the *consequence* of the shields, not a second
charge for them.

Composition at the end tells one more thing. Summed over 20 contested
games, deep holds Phalanx 1 393 / Chariot 509 / Trireme 483 / Warriors
120 / Archers 112; baseline holds Phalanx **1 760** / Trireme 652 /
Chariot **7** / Warriors 10 / Archers 35 — plus **Settlers 383** (deep
under 43) and Workers 1 304 (deep 878). Baseline is not less defended.
The late army surplus is Chariots, an attack unit; the *early* surplus
is defenders, and that is the one inside the causal window.

### Ruled out: the army is not what makes deep unhappy

Under Monarchy and Federation civ2civ3 does charge `uk_happy`, so the
field army could in principle be manufacturing the unhappiness the
Colosseum credit then prices. Paired on (seed, player, turn), the
correlation between the army surplus and the pacification surplus is
r = -0.16 to +0.40 and tracks the *citizen* surplus almost exactly
(econ T25: army +2.92, pacification +2.74, citizens +2.87). No support;
the loop is not closed.

---

## 3. Gold: the ledger

`bnp` is the corruption-adjusted trade surplus and `taxrate` is logged,
so `revenue = sum_t bnp_t x tax_t / 100` is a gross-tax proxy and
`outflow = revenue - treasury delta` is everything the treasury paid.
Deep's buys are itemized (`deep-buy`); baseline logs nothing, so its
buys stay inside its outflow. Per player-game, contested (160
player-games/arm):

| | revenue | treasury delta | outflow | itemized buys | outflow - buys | gold at end | gold mean |
|---|---|---|---|---|---|---|---|
| deep | 1 215 | **-17** | 1 232 | **421** | 811 | **32.4** | 38.5 |
| base | 1 265 | **+218** | 1 047 | (unlogged) | 1 047 | **264.7** | 198.6 |

Econ: deep revenue 798 (**above** baseline's 735), delta -18 vs +61,
outflow 816 vs 674, buys 312, gold at end 31.6 vs 111.4.

**Revenue is at parity. The gap is entirely outflow.** And it is not
upkeep:

| | buildings/player | gold bill/turn | cities | bill/city |
|---|---|---|---|---|
| deep | 13.82 | **10.73** | 4.62 | 2.54 |
| base | 21.20 | **17.02** | 5.27 | 3.25 |

Deep's standing building bill is **37% below** baseline's (econ: 4.17
vs 10.42). Unit gold upkeep is zero for both (every civ2civ3 `uk_gold`
is 0). `deep-spy` reports `incite=0` in every sampled game; no bribe,
tribute or gift line ever fires. **With equal revenue and a lower bill,
deep still ends on one eighth of the treasury, because it buys.**

Deep's purchases, per player-game (contested): 22.2 buys, **421.1
gold**, converting 61 698 gold into 25 575 shields at **2.41 gold per
shield** — a fair rate, not a wasteful one. 50.5% of buys carry
`urgency=0` (not an emergency). The band peak is T51-75 (879 of 3 304
sampled buys), again the causal window. By item:

    Phalanx     119.5 gold    Settlers    113.6    Diplomat  43.5
    Trireme      21.0         City Walls   17.3    Barracks  16.4
    Chariot      15.4         Warriors     13.4    Workers   12.8
    Archers       8.2         Temple        5.8    Granary    3.8

**211.2 of 421.1 gold (50.2%) buys the same military and military
infrastructure the want ordered**, and another 43.5 (10.3%) buys
Diplomats; 113.6 (27.0%) buys Settlers. Econ: Riflemen 115.2 of 312.1.

And the treasury flips at exactly the causal boundary. Buys by band
(19 seeds, 152 player-games), top line first:

    T  1-25   400 buys 10 465g   Settlers 242 (7 639g)  Warriors 93 (1 074g)
    T 26-50   772 buys 11 033g   Settlers 321 (5 434g)  Workers 87  Phalanx 86
    T 51-75   879 buys 14 853g   **Phalanx 212 (3 990g)**  Settlers 137 (2 889g)
    T 76-100  625 buys 12 735g   Phalanx 299 (5 902g)   Trireme 73  Settlers 57
    T101-125  381 buys  7 658g   Phalanx 207 (3 846g)   Diplomat 48
    T126-150  247 buys  4 954g   Phalanx 111 (1 877g)   Diplomat 36

Through T50 the treasury is an expansion instrument: 563 of 1 172 buys
are Settlers, 13 073 of 21 498 gold. **From T51 it is a garrison
instrument** — Phalanx overtakes Settlers in the same band in which the
city gap opens, and never gives the lead back.

The one independent cost is the fire sale. `deep-fin` sells 0.47
buildings per player-game (75 over 20 games) with the treasury at 2-4
gold and projected income negative: Temple 32, City Walls 16, Barracks
12, **Colosseum 8**, Aqueduct 3, Granary 2, Courthouse 2. Just over
half of what it sells is the content stock it built at 30-70 shields a
piece — the only place in this census where the content habit and the
gold squeeze touch directly.

Caveat on the ledger: `revenue` overstates both arms by the per-city
floor of `trade x tax / 100` (roughly 0.5 gold/city-turn, ~350 per
player-game each), and `outflow` bundles coinage inflow and civ2civ3's
tech upkeep. The residual is therefore not a clean upkeep figure. None
of the four claims above depends on it: revenue parity, the treasury
gap, the building-bill inversion and the itemized buy total are each
measured directly.

---

## 4. Ranked verdict

**1. The early military want. It is the only one of the three that is
present in the window where the entire deficit is created.**

The whole score gap is 0.65 fewer cities per player, and that gap is
built between T50 and T75 (contested) / T20 and T50 (econ) and never
moves afterward. In that band deep delivers 131 shields per player of
military hardware and 15 of settlers, and its treasury flips from
buying Settlers to buying Phalanxes in the same band. The seat is now
named, and it is two flat geographic terms: `deep_density_fear()` is on
54.9% of opening build-seat evaluations against stock's 12.5% (72.1%
vs 4.5% at T26-50), and from T51 `deep_boat_shadow_fear()` adds a flat
25 to **every coastal city** whenever any enemy anywhere owns one
transport — 30.9% of evaluations in the causal window, the largest
single injected term in it. Combat outcomes are
indistinguishable from baseline's to +/- 0.03 units through T75 in both
venues. The army is also free to keep in this ruleset, so the entire
cost is the shields, taken at the moment they are worth the most; and
it is upstream of the other two signatures: half the gold is spent
buying it, and Phalanx is the top settler pre-emptor in the 2026-08-25
prod-switch census.

**2. Gold — the same lever in a second currency, not an independent
defect.** Magnitude is real (421 gold per player-game, 1/8 of
baseline's treasury) but the diagnosis in the 2026-08-25 census —
"upkeep of 2x military and the building habit" — is wrong on both
counts: unit gold upkeep is identically zero in civ2civ3, and deep's
building bill is 37% *below* baseline's. Revenue is at parity. What is
left is buying, at a fair exchange rate, mostly military. Fixing the
military want fixes most of this line automatically. The residual
independent item is the fire sale (0.47 buildings/player-game), which
is worth a `deep-fin` epic on its own but is a fraction of a point.

**3. Content — real as a want, falsified as an over-investment.** The
Colosseum genuinely holds 23% of city-turns at T100-125 and 75.6% of
its want is deep-authored against a stock valuation of exactly zero,
and two thirds of the credits are paid for citizens who do not exist
yet. But deep ends with **fewer** Colosseums per citizen than baseline
(0.056 vs 0.066), its realized pacification load is at parity or lower,
and it delivers 12.8 shields per player in the causal band and zero
before T50. It is a delivery/churn problem (16.0% of Colosseum runs
complete; 8.0% in econ) inside a game that has already lost, not the
cause of the loss. **The 2026-08-25 hypothesis 3 should be restated:
not "content overvaluation" but "a late want large enough to hold the
queue and too short-lived to finish".** The correct next question for
it is the one S-PRODKEEP asked of settlers, not a re-pricing.

Two things that are outside the brief and that this census kept
tripping over, recorded so they are not lost:

- **The Diplomat is the third-largest shield line in the game** — 138.0
  shields/player-game delivered plus 43.5 gold of buys, 12.4% of picks
  at T26-50 and 11.9% at T51-75, i.e. squarely in the causal window,
  4.6 delivered per player-game. It comes from
  `dai_choose_diplomat_defensive`, whose stock want is a self-described
  "totally arbitrary 16000" that deep converts to an emergency-tier
  want at `diplomat_defence_value = 300`. Nobody has priced it.
- **"mil-infra" (Barracks + City Walls + Coastal Defense) is 290
  shields/player-game**, larger than content and second only to units.
  71.4 of it lands in T51-75.

---

## 5. The top lever's epic: S-FEARSEAT (instrument, then fix)

Instrument first, because the seat that orders the early army genuinely
cannot be named from the archive: `deep-def` reports stock's danger,
`deep-density` is silent when it is zero, and `deep_density_prior()`
has never written a line.

### Landed this session (observation only)

- `ai/deep: log the injected fear vector at the build seat` —
  `deep-fear ... site=build` carrying `pre` (stock's assess_danger),
  `over`, `shadow`, `density`, `prior`, `barb`, `surv`, `credit`,
  `updisc`, `sat`, `fmult`, `prefloor`, `post`. The block already
  computed every one of these in this order; the commit stores three of
  them in locals and prints. **All three probe games reproduce the
  archived `s6-nogates-war` scorelogs byte for byte** (seeds 101-103,
  `id` line excluded; maphashes match), on the same config and runner
  shas as the archived provenance.

### Still to instrument before any knob moves

- the same line at the once-per-turn seat
  (`deep_credit_incoming_defence`, `site=ledger`): the build seat's
  `pre` is nonzero on 12.5% of T1-25 evaluations and rises to 37.9% by
  T101 while `deep-def` reports 0 on 94% of the early picks. Both are
  `assess_danger()` (which ASSIGNS, `daimilitary.c:981`, so neither is
  an accumulation), read at different moments in the turn, and until
  both seats print the same vector the difference is unexplained;
- `deep-density` unconditionally, so a zero is a measured zero rather
  than a missing line;
- the buy path. `deep-buy` carries `want` and `urgency` but not the
  danger the want came from, and the T51 Settlers-to-Phalanx flip in §3
  is currently a correlation with no seat attached.

### Then, knobs — all defaulting to identity so the off arm is byte-identical

**No values chosen** (rule 7); ranges go in `tuning-params.ini`.

- `density_fear_ramp_turns` — **already exists, already documented,
  set to 0.** Its own comment describes exactly this failure ("deep
  paid T5 insurance for a T60 storm... while classic raced naked").
  Giving it a value is the cheapest test in the epic and must be the
  first cell, because if it moves nothing the rest of the diagnosis is
  in doubt.
- `density_fear_capability` — the term counts *cities*, never units.
  `deep_density_fear_raw()` already runs a reachability test for the
  cross-continent case (does the neighbour float a transport); this
  extends the same idea to land: weight a neighbour by observed
  offensive capability rather than by existing.
- `boat_shadow_distance` / `boat_shadow_count` — the boat shadow is the
  largest injected term from T51 and it is a flat 25 for every coastal
  city as soon as one enemy transport exists anywhere. It needs at
  minimum a distance term (is that hull within reach of *this* city)
  and a count term (one transport is not a landing). This is a model
  change, like `mil_opportunity_price` below, and must not be reduced
  to scaling the constant.
- `fear_evidence_floor` — a floor of observed capability below which
  anticipatory terms do not convert into a defender want at all. The
  falsifiable form of "88% of opening evaluations are priced off a
  neighbour count and 0% off a threat".
- `fear_defender_conversion` — the exchange rate from danger units to a
  defender want is stock's and has never been calibrated in deep's
  currency. It is the knob that makes the fear terms commensurable with
  the founder bid, which is the actual competitor.
- `garrison_floor_turn` — hold the token garrison floor off until
  turn N. Small (76 `deep-floor` lines a game) but it is the term that
  makes a zero-danger city order a unit anyway.
- `mil_opportunity_price` — the missing term, and the one that is a
  model change rather than a fudge. The defender want is priced against
  the city's own danger and never against what the shields would
  otherwise buy; this charges the marginal defender the price of the
  settler it displaces, using the founder bid already printed on
  `deep-bid`.

### Pre-registered falsification

An arm that ramps anticipatory fear over the opening must show fewer
T10-T50 military deliveries and **no increase in `unitslost` before
T75**. Both arms currently agree there to +/- 0.03 units, so there is
nothing for the army to lose. **If `unitslost` rises, the diagnosis is
wrong and this document gets re-read, not the tuning re-swept.**

Second pre-registration, on the seat itself: `density_fear_ramp_turns`
must reduce the share of build-seat evaluations carrying a nonzero
`density` in T1-50 (currently 54.9% / 72.1%). If `deep-fear` shows that
share unchanged, the knob is not reaching this seat and the arm's score
means nothing.

Sample-size note for whoever runs this: three probe games were enough
to name the seat and to kill the prior, and **not** enough to size the
terms — seed 101 alone showed density at 88.2% and no boat shadow. Any
arm that reads `deep-fear` for magnitudes wants the full holdout set.

Secondary pre-registration: if the shields freed do not turn into
cities, the defect is downstream and belongs to S-SETTLERSINK, not
here — expansion delivery is only 24.2% and the freed shields could
land in the same churn.

### Design constraints

- `density_fear_ramp_turns`, `fear_evidence_floor` and
  `mil_opportunity_price` all move *which cities order a defender in
  the same window*. They are one causal chain and want a **factorial**
  cell, not three concurrent arms.
- This epic and **S-SETTLERSINK (B)** both act on what the T50-75 queue
  produces. Running them as separate concurrent A/Bs cannot attribute
  either. They want one design or a strict ordering.
- `upkeep_gold_weight` (0.125, PLACEHOLDER) is flagged for the tuning
  pile by §1 but must **not** be swept in the same cell: it moves the
  building auction, which is the T100+ window, and mixing it in would
  make the military result unreadable.

---

## 6. Tooling

`benchmark/contentcensus.py DEEPDIR [BASEDIR] [--section content|military|gold|all]`

- reads `deep-want`, `deep-decay`, `deep-decay-sum`, `deep-prodswitch`,
  `deep-buy`, `deep-fin`, `deep-def`, `deep-density` from the deep arm;
- reads both arms' `scorelog.txt` and both arms' final savegames (city
  improvement bitvectors, unit rosters, treasury, government), which is
  the only way to compare against baseline at all: **the baseline arm's
  `game.log` is 461 lines and contains no AI decision line of any
  kind** — stock logs nothing, so every deep-vs-baseline statement here
  comes from the scorelog or the save;
- prices builds from `data/civ2civ3/buildings.ruleset` and
  `units.ruleset`.

Two parsing traps it handles, and which any future save reader must:
the savegame's `improvement_vector` contains names with commas
("Aqueduct, Lake") and the `c={` header contains
`"cma_minimal_surplus,1"`, so both must be split on commas *outside*
quotes; and `units.ruleset` names carry translator qualifiers
(`?unit:Workers`).
