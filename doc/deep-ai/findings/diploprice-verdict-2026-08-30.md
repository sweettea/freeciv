# S-DIPLOPRICE — pricing the Diplomat and the mil-infra lines

Epic brief: `findings/content-military-gold-2026-08-30.md` §4 closed on
two shield lines it had measured and never priced. This document is the
census of what those shields bought, the two gated mechanisms built on
it, and the factorial that adjudicates them.

Tooling: `benchmark/diploprice.py DEEPDIR [BASEDIR]`.

---

## 1. The census (Phase 1 — finished data only)

Corpus: the archived holdout sets `tune-postfix/s6-nogates-war`
(contested.conf, 20 seeds 101–120, T150) and `s6-nogates-econ`
(lategame-80.conf, 20 seeds, T80-equivalent), deep arm and its paired
baseline arm on the same seeds and maps. Deep self-play, eight players,
so "enemy" always means another instance of the same AI. The deep arm
is 160 / 120 player-games, baseline the same.

Validation: [D1] and [D2] reproduce the content census's published
figures exactly — 12.4% of T26–50 picks, 11.9% at T51–75, 138.0
delivered shields and 43.5 gold of buys per player-game, and the same
band split 7.7 / 26.8 / 36.0 / 27.6 / 19.9 / 20.1. The reader is
reading the same thing the earlier one did.

### 1.1 The Diplomat is bought, garrisoned, and never used

| | contested | lategame-80 |
|---|---|---|
| Diplomat runs started | 1785 | 2253 |
| … that delivered a unit | 736 (**41.2%**) | 781 (**34.7%**) |
| … abandoned mid-run | 1020 | 1386 |
| delivered shields / player-game | **138.0** | **193.6** |
| shields burnt by the abandoning switch | 8.6 | 22.8 |
| gold spent buying Diplomats | 43.5 | 42.0 |
| diplomat-turns / player-game (`deep-spy seen`) | **227.1** | **194.0** |
| … standing where an action was LEGAL (`at_seam`) | 14.5 (**6.4%**) | 5.8 (**3.0%**) |
| … at the seam with no positive deep price | 10.4 | 1.7 |
| spy actions deep took | 0 | 0 |
| diplomats alive at the end / player | 2.64 | 4.88 |
| … standing inside one of **our own** cities | **85.3%** | **82.6%** |
| mean age of a surviving diplomat | 56.6 turns | 26.7 turns |

`deep_spy_manage()` runs as a pure census at the compiled default
(`spy_enabled = 0`), so the zero in the action row is deep declining to
bid, not deep failing — stock's ladder owns every action. What the row
above it says is that the question rarely arises: **94–97% of all
diplomat-turns are spent somewhere no action is possible**, and five
sixths of the surviving stock is standing in a home city.

Baseline does the same thing with the same units (2.78 / 3.25 alive,
92.6% / 91.5% garrisoned), which places the line's origin exactly where
the brief put it: it is stock's, deep inherits it through
`dai_choose_diplomat_defensive()`, and deep's only contribution is the
`diplomat_defence_value = 300` emergency-tier conversion at
`deepdefence.c:5304`.

### 1.2 What the garrison is guarding against

`dai_choose_diplomat_defensive()` fires on `city_data->diplomat_threat`,
documented as "Enemy diplomat or spy is near the city"
(`daicity.h:58`). It is set at `daimilitary.c:905` by

    if (utype_acts_hostile(unit_type_get(punit)) && 2 >= move_time)

and `utype_acts_hostile()` is TRUE for ACTRES_ATTACK, ACTRES_PILLAGE,
ACTRES_BOMBARD and ACTRES_CONQUER_CITY as well as for the spy families
(`common/actres.c`). **An approaching Warriors sets the diplomat flag.**
Recorded as upstream bug 50.

[D6] measures the consequence on the end state. Per city, the nearest
foreign hostile-capable unit and the nearest foreign diplomat, at real
map distance r (iso-hex, wrapped; a `move_time` test of 2 is r=2 for a
Warriors and r=4 for a Chariot, so the table is given at four radii):

    contested, deep arm (740 cities)
      r   hostile near   diplomat near   P(diplomat | hostile)
      1        5.5%           2.0%              36.6%
      2       16.1%           4.6%              28.6%
      3       45.9%          23.9%              52.1%
      4       64.9%          37.3%              57.5%

    lategame-80, deep arm (1016 cities)
      r   hostile near   diplomat near   P(diplomat | hostile)
      1        7.4%           1.5%              20.0%
      2       20.9%           4.4%              21.2%
      3       42.8%          20.5%              47.8%
      4       60.5%          34.1%              56.3%

At the radius the flag is actually written for, **seven to eight of
every ten trigger-eligible cities have no enemy diplomat near them at
all.** The proxy is deliberately conservative: the hostile set is
approximated from attack strength, class pillage capability and the
Diplomat flag, so every type it misses would *raise* the false-positive
rate, not lower it.

And the loss the garrison exists to prevent barely occurs. Cities held
at the end whose `original` owner was someone else — conquest **and**
incite together, an upper bound on incites suffered — run **0.08 per
player-game (contested)** and 0.53 (lategame). Baseline, with the same
diplomat stock, suffers *more* of them (0.23 and 0.65). Nothing in the
finished record shows the defensive diplomat paying for itself, and
nothing shows deep losing cities for want of one.

### 1.3 Mil-infra is not overpriced. It is unfinished.

    contested (deep arm)      starts  built  aband  deliv%  delivered  burnt
      Barracks                  2013    885   1082   44.0%      165.9   20.8
      Barracks II                140     75     32   53.6%       14.1    0.5
      City Walls                1496    575    895   38.4%      107.8   19.2
      (Coastal Defense: never started on this config)
      by band  T1-25 0.6  T26-50 13.1  T51-75 71.4  T76-100 51.9
               T101-125 68.1  T126-150 82.7

    lategame-80 (deep arm)    starts  built  aband  deliv%  delivered  burnt
      Barracks II                595    150    424   25.2%       37.2   15.4
      City Walls                1510    617    801   40.9%      153.0   49.9
      Coastal Defense           1261    263    848   20.9%      130.4   67.0
      by band  T1-25 3.5  T26-50 98.9  T51-75 184.5  T76-100 33.7

287.8 delivered shields per player-game on contested reproduces the
brief's 290, and 71.4 of it lands in T51–75 as stated. But the shape
underneath is not an overvaluation. **Three fifths to four fifths of
every mil-infra run is abandoned before it delivers**, and Coastal
Defense on lategame-80 burns more shields in the change-production
penalty (67.0) than any other item in this census.

The end state is what makes it unarguable:

    standing at the end, per player      deep     baseline
      contested   Barracks               1.64       3.23     (35.5% vs 61.3% of cities)
                  Barracks II            0.42       0.72
                  City Walls             3.18       4.39     (68.8% vs 83.4%)
                  veteran mix v0/v1     47.3/51.0  19.3/76.6
      lategame-80 Barracks II            0.77       7.43     ( 9.1% vs 77.8%)
                  City Walls             4.66       2.75
                  Coastal Defense        1.96       0.79
                  veteran mix v0/v1     91.0/8.5   32.0/66.1

Deep starts far more Barracks runs than baseline and **ends with half
as many Barracks on contested and a tenth as many on lategame-80**,
and its surviving army is 47% / 91% raw recruits against baseline's
19% / 32%. The shields are being spent; they are not arriving. This is
the same signature the content census found on the Colosseum — "a want
large enough to hold the queue and too short-lived to finish" — and it
means **a gate that lowers the mil-infra want cannot be justified as
correcting an overvaluation.** Its only defensible job is to stop the
runs that were never going to finish from starting.

### 1.4 The scorelog context both gates are measured against

    contested        T50            T75            end
      cities    d 3.14  b 3.24  d 3.69  b 4.32  d 4.69  b 5.34
      settlers  d 0.59  b 1.42  d 0.55  b 2.46  d 0.19  b 2.43
      munits    d 3.05  b 0.94  d 6.19  b 3.67  d15.47  b12.70
      unitslost d 0.17  b 0.15  d 0.91  b 0.96  d14.46  b13.89
      score     d21.53  b20.02  d41.30  b39.65  d84.65  b91.53

    lategame-80      T50            T75
      cities    d 7.06  b 8.21  d 8.17  b 9.34
      settlers  d 1.07  b 7.98  d 0.84  b 6.61
      munits    d10.86  b 7.96  d14.88  b16.57
      score     d146.28 b146.32 d163.97 b169.51

### 1.5 What the census could NOT see

Every one of these is an "did it matter" question, and the archive
holds no line that answers it. They are the instrumentation commit's
job:

1. how often `diplomat_threat` is true **per city-turn**, and whether
   the unit that set it was a diplomat — [D6] is an end-state snapshot
   standing in for a per-turn flag;
2. how many enemy diplomat actions were **attempted against deep**, how
   many succeeded, and whether a diplomat-class defender was present;
3. what fraction of the 227 diplomat-turns is garrison duty as against
   travel (`deep-spy` counts the turn before it skips the garrison
   case, so the two are summed);
4. whether a walled city was ever **attacked**, and whether the wall's
   defence bonus was applied in a resolved combat;
5. whether a unit built in a Barracks city ever **fought**, and at what
   veteran level.

---

## 2. What landed

*(filled in below as the epic proceeds)*
