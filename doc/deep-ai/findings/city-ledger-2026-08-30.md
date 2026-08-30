# The city ledger: nothing kills them, they are never founded (2026-08-30)

S-CITYLEDGER is the instrumentation task
[the fearsink verdict](fearsink-verdict-2026-08-30.md) §9 closed on, in
its own words:

> **The binding constraint is not how many settlers deep builds, and not
> what it does with them at the end. It is that the T50 cities it does
> gain do not survive to T75.** … either the cities are not being founded
> to begin with, or they are being founded and lost to something that
> does not show in `unitslost`. Instrument the T50–75 city *births and
> deaths* before building anything else.

**No instrumentation was needed and none was written.** The census is
readable from data already on disk, and the answer is the first branch,
categorically: **the cities are never founded.** In the war venue the
whole T51–75 band contains 0.006 city deaths per seat — one city across
20 games and 160 seats — and the gain that evaporates is repaid
*founding for founding* in the next band.

Reader: `benchmark/cityledger.py`. Corpora: the archived holdout
`tune-postfix/s6-nogates-{war,econ}` (identical to
`w25-screen-corpus/{war,econ}` — the same games, both arms), and the
`fearsink`, `fearseat` and `siteplan` A/B cells in their agents'
worktrees. **All read-only. No games were run.**

---

## 0. How a death is visible at all, given the server logs none

`remove_city()` and `transfer_city()` announce through `notify_player()`
(`citytools.c:2045-2119`), which is a client packet. At `LOG_NORMAL`
`game.log` therefore carries **no city event of its own**, and grepping
it for "captured"/"destroyed" finds nothing. Two readers exist instead,
and `cityledger.py` runs both:

**The scorelog reader** works for *every* arm, `ai/baseline` included,
and survives the log filtering the recent A/B corpora applied, because
`scorelog.txt` carries tag 3 (`cities`) per player per turn. A
per-player rise is a city gained, a fall is one lost. The **all-player
total** then splits a loss: conserved means the city changed hands,
down means `remove_city()` ran.

**The `deep-city` reader** works on unfiltered logs of `ai/deep` arms.
`ai/deep`'s `city_got`/`city_lost` hooks (`deepai.c:843`, `:894`) print
the city id, size and `turn_founded`, so a gain with `founded == turn`
is a founding and one with `founded < turn` is a conquest, and every
loss carries the dead city's size and age.

One calibration the two readers forced: **the score snapshot is taken at
the start of a turn**, so a city founded during turn *t* first appears
in the sample for *t+1*. Uncorrected, every band edge is off by one turn
and the readers disagree by 1.4 foundings a game. Corrected, they agree
band for band (the `[deep: …]` column in the tables below is the second
reader, printed beside the first).

The reconstruction is checked against the scorelog's own count at every
band edge and closes in 19/20 or 20/20 games per arm; the exceptions are
events in the last two turns, after the final score sample.

---

## 1. The headline: deep vs baseline, contested/war

20 paired seeds, 8 seats, deep self-play against baseline self-play on
the same maps. Per seat.

| band | deep found | deep died | baseline found | baseline died |
|---|---:|---:|---:|---:|
| T1–25 | 1.575 | **0.000** | 1.675 | 0.006 |
| T26–50 | 1.587 | **0.006** | 1.594 | 0.019 |
| **T51–75** | **0.556** | **0.006** | **1.087** | 0.062 |
| T76–100 | 0.700 | 0.019 | 0.825 | 0.206 |
| T101–125 | 0.200 | 0.062 | 0.181 | 0.062 |
| T126–150 | 0.075 | 0.037 | 0.050 | 0.037 |
| **held at end** | **4.625** | | **5.269** | |

Paired, per seed:

| band | d(found) | d(died) | d(cities held) |
|---|---:|---:|---:|
| T1–25 | −0.100 ± 0.065 | −0.006 ± 0.006 | −0.100 ± 0.065 |
| T26–50 | −0.006 ± 0.082 | −0.013 ± 0.013 | −0.113 ± 0.076 |
| **T51–75** | **−0.531 ± 0.128** | **−0.056 ± 0.025** | **−0.631 ± 0.144** |
| T76–100 | −0.125 ± 0.132 | −0.188 ± 0.053 | −0.706 ± 0.186 |
| T101–125 | +0.019 ± 0.077 | +0.000 ± 0.033 | −0.669 ± 0.166 |
| T126–150 | +0.025 ± 0.058 | +0.000 ± 0.029 | −0.644 ± 0.147 |

Cumulative missing foundings: −0.106 ± 0.075 through T50,
**−0.637 ± 0.143 through T75**, −0.719 ± 0.160 to the end — against a
final stock gap of −0.644 ± 0.147.

**Deep's entire city deficit is foundings it never made, and every
death term runs the other way.** Deep loses 0.131 cities per seat per
game; baseline loses 0.394, three times as many. Through T50 deep and
baseline are level. **The fork is one band wide: in T51–75 baseline
founds 1.087 cities a seat and deep founds 0.556.**

## 2. The same shape in the econ venue

lategame-80 (40×40, 6 seats, T81, `migration enabled`), 20 paired seeds.

| band | deep found | deep died | base found | base died | d(found) | d(died) |
|---|---:|---:|---:|---:|---:|---:|
| T1–25 | 6.317 | 0.317 | 6.825 | 0.267 | −0.508 ± 0.099 | +0.050 ± 0.082 |
| T26–50 | 1.167 | 0.275 | 1.692 | 0.217 | −0.525 ± 0.173 | +0.058 ± 0.046 |
| T51–75 | 1.208 | 0.467 | 1.358 | 0.608 | −0.150 ± 0.167 | −0.142 ± 0.102 |
| T76–81 | 0.208 | 0.067 | 0.150 | 0.150 | +0.058 ± 0.056 | −0.083 ± 0.028 |

Cumulative missing foundings **−1.183 ± 0.232 through T75**, against a
stock gap of −1.142 ± 0.203. Attrition is again net *favourable* to
deep from T51 on. Same verdict, different band: here the loss is made
in the opening.

## 3. The fearsink hypothesis, decomposed

This is the experiment that produced the question. `c10`
(`mil_opportunity_gate`) minus `c00`, contested, 20 paired seeds, per
seat:

| band | d(found) | d(died) | d(cities held) |
|---|---:|---:|---:|
| T1–25 | +0.075 ± 0.032 | +0.000 | +0.075 ± 0.032 |
| T26–50 | +0.087 ± 0.060 | +0.006 ± 0.006 | **+0.163 ± 0.064** |
| **T51–75** | **−0.100 ± 0.072** | **−0.013 ± 0.015** | **+0.062 ± 0.066** |
| T76–100 | +0.006 ± 0.094 | +0.013 ± 0.015 | +0.050 ± 0.088 |

Cumulative d(found) is +0.163 ± 0.064 through T50 and +0.062 ± 0.066
through T75 — **identical, to three decimals, to the city curve it
explains.** The gain arrives as foundings and leaves as foundings not
made. The death term never once works against the treated arm; in
T51–75 it is −0.013, i.e. `c10` loses *fewer* cities than the base.

`c11` (all three gates) says the same thing: T51–75 d(found)
−0.119 ± 0.104 against d(died) −0.019 ± 0.010. So does the econ venue,
where the effect is four times bigger: `c10` − `c00` cumulative d(found)
+0.242 ± 0.163 through T50 and **−0.117 ± 0.314 through T75**, with
d(died) −0.108 ± 0.099 in that band. (`fearseat`'s `price` − `off` arms
give the war table to three decimals; they are the same games.)

**The fear verdict's "+0.19 cities at T50 that are gone by T75" was
never attrition, in any cell, in either venue.** The founder queue has a
fixed T1–75 output on a given map and anything that pulls foundings
forward is repaid out of T51–75. The conservation is visible in deep's
own site accounting: at T51–75 `c10` reports `sites` 2.802 remaining
against `c00`'s 3.092 and runs 74.3% empty site searches against 68.9% —
it consumed the sites earlier, so it has fewer left.

## 4. What kills the cities that do die

`cityledger.py` names each death by joining the two readers: `razed`
means the all-player total dropped, so `remove_city()` ran; `taken`
means it was conserved, so `transfer_city()` ran. `fought` means a unit
died on that tile that turn or the one before.

The split validates itself against the server: conquest of a size-1 city
runs `remove_city()` (`citytools.c:2042`) and conquest at size ≥ 2
transfers (`:2135`), and that is exactly the shape that comes out.

**contested / war**, 20 games × 8 seats:

| cause | n | per seat/game | mean size | size 1 | median age | fought |
|---|---:|---:|---:|---:|---:|---:|
| taken | 12 | 0.075 | 4.75 | 0% | 82 | 83% |
| razed | 11 | 0.069 | 2.09 | 55% | 54 | 73% |

Twenty-three cities in 160 seat-games, almost all of them taken by
force, almost all after T75, none of them in the band where the T50
gain evaporates. **In the war venue there is nothing to defend.**

**lategame-80 / econ**, 20 games × 6 seats:

| cause | n | per seat/game | mean size | size 1 | median age | fought |
|---|---:|---:|---:|---:|---:|---:|
| taken | 70 | 0.583 | 4.86 | 0% | 45 | 51% |
| razed | **74** | **0.617** | **1.00** | **100%** | **5** | **1%** |

Here there *is* a large attrition channel, and it is not war. Every one
of the 74 razed cities was size 1, 73 of 74 had no unit die on the tile,
and the median age at death is **exactly 5 turns**.

That is citizen migration. `lategame-80.conf` sets `migration enabled`
and `mgr_distance 6`; `mgr_turninterval` defaults to **5**
(`common/game.h:527`) and the interval is counted *from the city's
founding turn*, which is why the mode is exactly 5 and 74% die by age 5.
`do_city_migration()` handles the size-1 case by **removing the city**
and rehoming its units (`server/cityturn.c:4279`).

So deep founds a city, its single citizen walks to a better neighbour on
the first migration check, and the city is deleted. **0.617 cities per
seat per game**, against the 2.583 deep founds after T25 in that venue —
a 24% loss rate on its post-opening expansion. `lategame-80.conf`'s own comment already says "neither
`ai/default` nor `ai/deep` models migration (that is W17)", and it is
applied equally to both paired arms, so this is not by itself a
deep-vs-baseline deficit. It is 0.617 cities of deep's own work
destroyed by a rule deep does not read.

**It is still not the fearsink answer.** The econ evaporation
(§3) is founding-side too: `c10` − `c00` d(died) is *negative* in every
econ band.

## 5. Where the foundings stop: the funnel

`cityledger.py --why` reads `deep-expand` (the founder funnel, one line
per player-turn, fields documented in `ai/deep/deepexpand.h`) and
`deep-siteladder` (the site-search refusal ladder, `deepsite.c:2360`).
Numbers below are `siteplan/war/c00` — contested, defaults, **60 games**
— and `s6-nogates-war` (20 games) reproduces them within 2 points.

| band | fresh searches /pl-turn | **came home empty** | held /pl-turn | `lostdom` | prod | founders alive | `sites` |
|---|---:|---:|---:|---:|---:|---:|---:|
| T1–25 | 0.165 | **6.1%** | 0.902 | 0.001 | 0.727 | 0.346 | 9.28 |
| T26–50 | 0.374 | **44.0%** | 1.513 | 0.043 | 1.083 | 0.615 | 5.50 |
| **T51–75** | 0.511 | **71.6%** | 0.987 | 0.220 | 0.599 | 0.590 | 3.24 |
| T76–100 | 0.601 | 74.0% | 1.097 | 0.413 | 0.476 | 0.509 | 3.60 |
| T126–150 | 0.669 | 97.2% | 0.151 | 0.074 | 0.051 | 0.201 | 3.26 |

And the ladder, which says what a failed search saw. Bucket columns are
means over the **empty** searches only, and they partition `seen`:

| band | searches | found% | seen | `off` | of which `offpeace` | `noterr` | `nomin` | `mindist` | **`scored`** |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| T1–25 | 3 114 | 94.7% | 13.6 | 0.0 | 0.0 | 3.6 | 7.0 | 2.2 | **0.00** |
| T26–50 | 6 377 | 59.1% | 50.8 | 18.7 | 18.7 | 5.7 | 21.1 | 4.9 | **0.00** |
| **T51–75** | 10 513 | **27.9%** | 110.5 | **45.1** | **43.3** | 30.4 | 30.5 | 4.1 | **0.00** |
| T76–100 | 14 687 | 18.2% | 253.6 | 62.6 | 58.9 | 140.5 | 44.8 | 4.0 | **0.00** |
| T126–150 | 15 732 | 2.0% | 381.7 | 80.5 | 79.8 | 235.5 | 58.2 | 4.8 | **0.00** |

Three things this settles.

1. **The refusal is total, not marginal.** In a failed search `scored`
   is 0.00 — *not one tile ever reaches the scorer*. This is not a
   threshold, not `site_enough_result`, and not the travel price: it is
   every candidate rejected before valuation.

2. **When a site is found it is a good one.** Mean `best_net` among
   successful searches is 357.8 in T51–75 against 387.9 in T1–25. Deep
   is not settling worse land later; it is finding none.

3. **The largest single refusal is a peaceful neighbour's border.** At
   T51–75, `offpeace` is 43.3 of the 110.5 tiles a failed search sees —
   **39%**, rising to 79.8 of 381.7 by T126–150. That is stock's *"Land
   theft does not make for good neighbors"* rule
   (`ai/default/daisettler.c:751`, mirrored at `deepsite.c:2170`), which
   is categorical, unpriced and never revisited. `ai/baseline` carries
   the identical rule (`ai/baseline/default/daisettler.c:819`), so it is
   not by itself the deep-vs-baseline differential — but it is the
   single largest thing standing between deep's settlers and a city, and
   deep's own diplomacy (`deep-peaceseek`, `deep-peacetake`) is what
   chooses the peace that closes the frontier.

## 6. What the deep-vs-baseline gap actually is: founder stock

Scorelog tag 6 (`settlers`) counts units that can do
`ACTION_FOUND_CITY` (`server/report.c:794`), so it is the same
population `deep-expand`'s `alive` counts and it is readable for both
arms. Contested, per seat:

| | T25 | T50 | T75 | T100 | T150 |
|---|---:|---:|---:|---:|---:|
| deep | 0.519 | 0.606 | **0.531** | 0.450 | 0.181 |
| baseline | 0.306 | 1.469 | **2.525** | 2.938 | 2.394 |
| paired d | +0.212 ± 0.061 | −0.863 ± 0.124 | **−1.994 ± 0.221** | −2.487 ± 0.266 | −2.212 ± 0.239 |

**Baseline holds 4.8× deep's founder stock at T75 and founds 2.0× as
many cities in T51–75.** Taking the crude mid-band stock as a
founder-turn budget, deep spends ≈19 founder-turns per city in that
band and baseline ≈46: deep converts a founder-turn to a city about
twice as well and has about five times fewer of them.

This does **not** contradict fearsink §5's *"deep is not
settler-starved"* — that was read within deep's own arms, at the margin,
where two fewer settlers still bought more cities. Against baseline the
stock difference is not marginal. Both are true: deep's marginal settler
is cheap to remove, and deep's *total* settler stock after T50 is a
fifth of the control's.

## 7. Verdict

> **Founding-rate, not attrition — and not merely as a majority, but to
> the exclusion of attrition.** In the war venue the T51–75 band holds
> 0.006 city deaths per seat and the treated arm loses *fewer* cities
> than the base in every cell of the 2×2. The +0.19 cities at T50 are
> +0.163 ± 0.064 extra foundings, and they are gone at T75 because the
> arm makes −0.100 ± 0.072 fewer foundings in T51–75. The founder queue
> has a conserved T1–75 output on a fixed map, and every intervention so
> far has moved foundings in time rather than adding any.

> **What kills the cities that do die**, when any do: in the war venue,
> conquest, 23 cities in 160 seat-games, 78% with combat on the tile,
> median age 82 turns — nothing in the band at issue. In the econ venue,
> **citizen migration deleting size-1 cities on the fifth turn after
> founding**, 0.617 per seat per game, 100% size 1, 1% with combat,
> median age exactly `mgr_turninterval`.

> **The real deficit against baseline** is −0.531 ± 0.128 foundings per
> seat in T51–75, and behind it 72% of deep's site searches returning
> nothing with zero tiles scored, and a founder stock a fifth of
> baseline's.

## 8. Proposed epic: S-FRONTIER — price the closed frontier

The measurement points at one thing above all others, and the census
already sized it: **39% of every tile a failed T51–75 search sees is
land owned by a player deep is at peace with, and the AI refuses it
categorically without ever scoring it.**

Sequence, one commit each:

1. **Establish legality first.** Before any knob: does the ruleset
   actually permit founding inside a peaceful neighbour's border, and
   what incident does it raise? If the server refuses the action, the
   whole line is dead and the census should be redirected to `nomin`
   (30.5 tiles, deep's own spacing) instead. This is a reading commit
   with an assertion, not a benchmark.
2. **Log the declined opportunity.** `deep-siteladder` counts
   `offpeace` tiles but never scores them. Score a bounded sample of
   them and print what deep is turning down, so the price has something
   to be a price *of*. Instrumentation-only, byte-identical at defaults.
3. **The gate.** Replace the categorical refusal with a charge.
4. **The 2×2 against the founder floor** (below), because both act on
   the same T51–75 queue and — per the fearsink lesson — two concurrent
   single-factor arms cannot be subtracted from each other.

Constants, all **PLACEHOLDER, no values proposed** (rule 7):

| name | what it prices |
|---|---|
| `frontier_theft_gate` | whether a site inside a peaceful owner's border is priced rather than refused |
| `frontier_theft_price` | the diplomatic cost charged against such a site's net result |
| `frontier_theft_stance_floor` | the stance toward that owner below which the site is refused outright regardless |
| `frontier_theft_reach` | how far inside a foreign border a site may be considered |

A second, independent lever the same census names, for the factorial's
other axis — **the founder floor**. `held` per city falls from 0.594 in
T26–50 to 0.231 in T51–75 while `w0` reaches 72%: an empty search zeroes
the founder want, so a search that failed for a reason a *turn* could
undo (a border moved, a boat arrived, a war started) removes the founder
from the build queue entirely, and the stock never recovers.

| name | what it holds |
|---|---|
| `founder_floor_gate` | whether an empty site search may zero the founder want at all |
| `founder_floor_want` | the founder want that survives an empty search |
| `founder_floor_reach_ref` | the reachable free land at which that floor is fully paid |

And a third, venue-local, for the econ venue's 0.617 razed cities —
kept separate because it belongs to W17 and must not be bundled:

| name | what it guards |
|---|---|
| `migration_seed_gate` | whether a site is refused when its size-1 migration score would lose to a neighbour |
| `migration_seed_margin` | the margin by which it must lose before refusing |

## 9. Two free by-products

- **The cause split need not be reconstructed.** `cityledger.py` infers
  razed-vs-taken from the all-player total because deep does not record
  it. It could: `remove_city()` calls `city_lost` and then
  `city_destroyed` for the same city (`citytools.c:1728-1729`), and
  `ai/deep` leaves `funcs.city_destroyed` commented out at NULL
  (`deepai.c:1674`). A two-line handler would name the cause at the
  source and retire the heuristic. `dpai_city_lost`'s own comment —
  *"the server does not say which here"* — is wrong about this, and is
  the only thing in the reading that turned out to be.
- **The scorelog is a complete city ledger for any arm ever run.** Tag 3
  per player per turn plus the all-player total reconstructs every
  founding, conquest, hand-over and removal, for `ai/baseline` as well
  as `ai/deep`, in corpora whose logs were filtered to a handful of
  tags. Nothing that has been benchmarked on this project is unreadable
  on this question.
