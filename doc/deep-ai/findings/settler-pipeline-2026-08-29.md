# The delivered settler is eaten by the join, not by the road (2026-08-29)

The prod-keep verdict closed with an open question: `founder_run_hold`
raises settler delivery 24.8% -> 30.7% and score does not move, so the
expansion stall is **downstream of delivery** — escort, travel, site
legality, target churn, founding refusal, or the settler being spent
some other way. This is that census.

Data: **finished games only, no new benchmark batch.** The 20-seed
gates-off holdout arms of `benchmark/results/tune-postfix/`
(`s6-nogates-war`, contested.conf, 8 deep seats, 150 turns;
`s6-nogates-econ`, lategame-80), each paired against its `baseline/`
counterpart on the same seeds and the same maps. Three probe games
(seeds 101–103, contested, defaults.ini) were run only to verify two
new log lines; **all three reproduce the archived holdout scorelogs
byte for byte**, so they are the same games with more writing.

Everything below re-derives with `./benchmark/settlerpipe.py DIR`.

---

## 1. The answer in one table

Every founder unit that reaches `deep_auto_settler_run()`, followed to
its end. 20 games per venue.

| end | war venue | | econ venue | |
|---|---:|---:|---:|---:|
| units entering the run | 1312 | 100% | 1669 | 100% |
| **founded a city** | 750 | **57.2%** | 1094 | **65.4%** |
| **joined into a city** | 510 | **38.9%** | 451 | **27.0%** |
| killed / starved / lost at sea / bribed | 26 | 2.0% | 61 | 3.7% |
| alive at the end, or unresolved | 26 | 2.0% | 63 | 3.8% |

**Two in five delivered settlers in the war venue, and one in four in
the econ venue, are dissolved into an existing city instead of
founding one.** The `deep-loss` ledger closes on this: 1259 founder units end with
`reason="used"` in the war venue, against 750 foundings and 511
`deep-join ... done=1` events. Nothing else consumes a settler.

Violence is not a link. Settlers killed, starved, bribed or drowned
are **2.0%** of the population (1.3 per game). Escort wait is not a
link either: `deep-wait` holds a settler for 11.2 unit-turns per game
in the war venue and 16.9 in the econ one, against 308 and 531
mission-turns.

## 2. Deep and baseline diverge exactly where the joins start

Means over the same 20 seeds, both arms, war venue:

| turn | deep cities | base cities | deep settlers alive | base settlers alive |
|---:|---:|---:|---:|---:|
| 15 | 7.8 | 8.2 | 1.1 | 0.6 |
| 30 | 15.1 | 15.2 | 4.8 | 4.5 |
| 45 | **23.2** | **23.2** | 6.0 | 9.8 |
| 60 | 27.1 | 28.5 | 3.4 | 13.8 |
| 75 | 29.4 | 34.4 | 4.4 | 19.6 |
| 90 | 32.8 | 38.8 | 5.0 | 23.6 |
| 105 | 35.8 | 41.2 | 3.0 | 23.2 |
| 150 | **37.0** | **42.1** | **1.5** | **19.2** |

pop 209.4 vs 239.8, score 668.8 vs 723.0 at T150. Econ venue, same
shape and larger: cities 49.5 vs 57.0 and settlers 5.1 vs **40.4** at
T75.

Cities are at **exact parity to T45** and then deep stops. Baseline
does not found faster; it founds *longer*, and the only visible
difference in what the two do with a settler is that **baseline never
dissolves one**. `ACTRES_JOIN_CITY` has no actor anywhere in stock
(deep's own comment at `deepsettler.c:1616`, upstream-bugs #31);
`deep_settler_join_city()` is deep's addition, and it fires 25.5 times
a game against a 5.1-city deficit.

Deep's founding and joining by 25-turn bucket, 20 games, war venue:

| bucket | 0–24 | 25–49 | 50–74 | 75–99 | 100–124 | 125+ |
|---|---:|---:|---:|---:|---:|---:|
| foundings | 238 | 265 | 86 | 112 | 37 | 12 |
| **joins** | 2 | 123 | **126** | **141** | **79** | **40** |
| site searches | 977 | 2107 | 3231 | 4860 | 4915 | 5083 |
| searches finding nothing | 23 | 926 | 2392 | 3739 | 4662 | 4882 |
| **nosite %** | 2% | 44% | **74%** | **77%** | **95%** | **96%** |

From T50 on, deep dissolves more settlers than it founds cities with
(386 joins against 247 foundings).

## 3. The counterfactual: 24–52% of joins are premature by deep's own record

A join is irreversible. The question is whether the settler would have
founded had it been kept. Deep answers it against itself — for each
join, did **that same player** found a city afterwards?

| | war | | econ | |
|---|---:|---:|---:|---:|
| joins | 511 | 100% | 453 | 100% |
| followed by a founding within 5 turns | 92 | 18.0% | 184 | 40.6% |
| **within 10 turns** | **124** | **24.3%** | **235** | **51.9%** |
| within 20 turns | 158 | 30.9% | 269 | 59.4% |
| any later founding at all | 202 | 39.5% | 307 | 67.8% |

6.2 premature joins per game in the war venue and 11.8 in the econ
one, against city deficits of 5.1 and 7.5. **The joins that fired
within ten turns of the same player founding a city are, on their own,
the whole deficit in both venues.**

The pool gate that is supposed to prevent this does not bind. Across
2157 `deep-pool` lines the mean founder count in hand is **1.62**
against a mean target of **0.20** — `founder_pool_per_site` is
multiplied by `deep_remaining_sites()`, which goes to zero as the map
fills, so the gate opens hardest exactly when the settler is most
worth keeping.

## 4. Why the search fails: legality, never valuation

The join fires only when `deep_find_best_city_placement()` returns
NULL. It returns NULL constantly, and the refusal ladder says why.
Real settlers (not the virtual want-probe), land phase, war venue:

| population | n | seen/search | off | nomin | noterr | mindist | **scored/search** |
|---|---:|---:|---:|---:|---:|---:|---:|
| found a site | 763 | 51.2 | 19% | 24% | 13% | 11% | 13.1 |
| **came home empty** | **1871** | 84.6 | **69%** | 21% | 6% | 3% | **0.0** |

**On every one of the 11 761 empty searches in the war venue and 9 249
in the econ one, `scored` is exactly zero.** Not one tile ever reached
the valuation stage. An empty search is never "the sites were not
worth the settler"; it is always "no tile was legal". `neg` is 0 in
the war venue, which says the same thing from the other side.

The composition of a failed search is **69% `off`** — a tile the
search will not settle because someone else owns it and we are at
peace — and 21% `nomin`, inside citymindist of a city that exists.

`off` conflated two opposite findings and now does not (commit 2 of
this session): over three probe games it is **96.8% `offpeace`** and
3.2% `offbridge`. So the refusal is diplomatic, not geographic.

And it is narrower than it looks. `deep_settler_tile_behavior()`
(`deepsettler.c:113`) returns `TB_IGNORE` for **every non-allied
tile**, and `pf_normal_node_init()` drops a `TB_IGNORE` node before it
is ever queued (`path_finding.c:288`). Stock's phase-1 site search
sets no `get_TB` at all (`ai/default/daisettler.c:845`). So the tiles
counted in `off` are the land of players deep is **allied** with — a
peace partner's land is not refused by the ladder, it is invisible to
the search that feeds it. Confirmed on the seed-101 final save: deep
holds Alliance on 42 of 72 ordered player pairs, baseline 30 Alliance
plus 10 Peace.

**What the TB removes is not counted anywhere.** `seen` is already the
post-truncation figure, so the ladder cannot say how much land deep
declined to look at. That is the one instrument this census could not
build from finished games.

## 5. Turns lost, per link, per game (war venue / econ venue)

| link | war | econ | notes |
|---|---:|---:|---|
| mission-turns, all | 308.5 | 530.8 | |
| — spent on a settler that founded | 220.2 | 379.9 | |
| — **spent on a settler that was later joined** | **56.6** | **85.3** | walked for nothing |
| — spent on a settler that died or idled out | 31.7 | 65.6 | |
| travel, first pick to founding | 6.2 mean, 5 median, 12 p90 | 6.9 / 5 / 14 | ~1 tile/turn |
| **arrival gate** | **39.4** | **53.1** | 1.05 and 1.01 wasted turns *per city founded* |
| mid-flight cancels | 14.2 | 26.6 | 96% `nomin` |
| `nopath` (the risk callback vetoes the route) | 20.7 | 79.4 | unit stands still |
| `ferry` (no land route, waiting on a boat) | 25.9 | 32.5 | |
| target churn | 12% of units re-pick; 1.55 picks per founding | | small |

**The arrival gate** is worth stating precisely. 787 of 1535 arrivals
(51%) log `reason=arrived` with `moves=x/0`: the settler's last step
consumed its whole move allowance, and `moves_left <= 0` returns
before the `same_pos` founding attempt. That is 1.05 wasted turns per
city founded in the war venue and 1.01 in the econ one — essentially
**every city deep founds is founded one turn late**. The ordering is
legitimate rather than a bug: classic's `enabler_build_city_*` require
`MinMoveFrags 1` (`data/classic/actions.ruleset:662`), so the unit
genuinely cannot found on 0 fragments. But nothing prices it. **It is
inherited: `ai/default/daisettler.c:1000` has the identical ordering,
so baseline pays the same tax** and this link does not explain the
gap. It is a free win, not an attribution.

**The mid-flight cancel** had no log line until this session; it could
only be counted by set-difference (a `mission` turn with no `stall`),
284 of 6169 mission-turns in the war venue. The new
`deep-settler ... cancel` line closes that gap exactly — 48 cancels
against 48 set-difference events over the three probe games — and its
verdict is unanimous: **46 of 48 are `nomin`**, someone founded a city
within citymindist of our destination while we were walking to it,
median remaining distance 4 tiles, 47% of the time a rival's city and
31% an ally's. At 6.2 turns of travel, deep loses races for tiles.

## 6. Ranked verdict

1. **The join sink eats the delivered settlers.**
   `deep_settler_join_city()` (`deepsettler.c:1633`, called at
   `deepsettler.c:2675` when the search returns NULL) takes 38.9% /
   27.0% of every founder deep delivers. It is deep-only — stock has
   no `ACTRES_JOIN_CITY` actor. Its magnitude (25.5 and 22.7 per game)
   dwarfs the city deficit (5.1 and 7.5), and the subset that fires
   within ten turns of that same player founding a city (6.2 and 11.8
   per game) matches the deficit on its own. Its pool gate reads 1.62
   founders in hand against a 0.20 target and never binds.

2. **The site search's legality horizon is what triggers the sink.**
   Empty searches never price a candidate — `scored` is 0 on all 21010
   of them. 69% of what a stranded settler can see is allied land it
   refuses; every non-allied tile was removed from the pf map before
   the ladder counted anything, and that removal is unmeasured. This
   is the link where the instrument is missing, not the link where the
   evidence is.

3. **The arrival gate costs one turn per city founded** — 39.4 and
   53.1 turns per game. Real, cheap, and **shared with baseline**, so
   it is an improvement rather than an explanation.

4. **Race losses.** 14–27 mission cancels per game, 96% of them
   `nomin`: the destination stopped being foundable because somebody
   founded first. Travel is ~1 tile per turn over a mean of 6.2 turns,
   which is long enough to lose the tile.

5. **Ruled out, with numbers.** Escort wait (11–17 settler-turns/game
   held), settler deaths (2.0%), target churn (12% of units re-pick),
   founding refusal at the tile (748 arrivals with moves left produced
   750 cities — `dai_do_build_city()` essentially never refuses).

This also explains the prod-keep result it was asked about, and the
explanation is testable: `founder_run_hold` delivered 30.7% instead of
24.8% of started settlers and score did not move, because the
additional settlers arrive into the same NULL search and the same join.
**Delivering more settlers cannot help while two in five are
dissolved on arrival.**

## 7. Proposed epic — S-SETTLERSINK: instrument the horizon, then price the dissolution

Instrument first (this session's rule 6 lesson from prod-keep: the
mechanism failed on REACH, not magnitude, and the census had to say so
before the fix could be designed).

**Landed here (instrumentation, inert by construction, byte-identical
scorelogs on three seeds):**

- `ai/deep: log the city mission cancelled under a walking settler`
- `ai/deep: split the site ladder's off bucket into bridge and peace`
- `benchmark: add the settler-pipeline census`

**Commit A — count what the settler is not allowed to look at.**
Add `tbskip=` to `deep-siteladder`: tiles `deep_settler_tile_behavior()`
refused on ownership before the pf map queued them, and `tbfree=`, how
many of those were unowned tiles reachable only *through* refused
ones. Without this, §4 can say the horizon is truncated and cannot say
by how much. Counter reset in `deep_settler_map_iterate()`, incremented
in the callback; no tunable, no behaviour.

**Commit B — the headline mechanism: hold the founder instead of
dissolving it.** Decline the join while the settler's failure to find a
site is plausibly temporary. Tunables to add to `struct deep_tuning`,
all PLACEHOLDER, none picked here:

- `founder_hold_gate` — switch, ships at 0.
- `founder_hold_turns` — consecutive empty searches a founder must
  post before it may be dissolved. Today the answer is 1.
- `founder_hold_pool_floor` — a floor on founders held per player,
  independent of `deep_remaining_sites()`. §3's 1.62-against-0.20 is
  the defect this addresses; the existing `founder_pool_per_site`
  cannot express a floor because it is a product with a quantity that
  goes to zero.

**Commit C — price the arrival turn.** The site score already
discounts travel; make it discount `T+1` when the pf position at the
destination arrives with 0 move fragments, which is a measured
quantity and not an invented one. Tunable `site_arrival_turn_gate`
(switch only — the +1 is read from pf, not chosen). Expected effect is
site *choice* shifting toward tiles reachable with a fragment to
spare, not a turn saved on the tile already chosen.

**Commit D — price the race.** Discount a candidate by the chance
somebody founds within its citymindist ring before we arrive, read on
the `deep-settler cancel` census this session added. Tunable
`site_race_gain` (PLACEHOLDER) plus `site_race_gate`.

**Design of the A/B, and this part is not optional.** B interacts with
`founder_run_hold` by construction — `founder_run_hold` puts more
settlers into the pipeline and B decides what happens to them at the
end of it. Two separate arms cannot attribute that; per CLAUDE.md's
"parallelise scheduling, not attribution", **B wants a 2x2 factorial
with `founder_run_hold`**, on training seeds, contested. C and D are
independent of both and ship as their own cells.

**Acceptance metrics**, in this order:
1. joins per game and **foundings per delivered settler** (today 750 /
   1261 = 59.5% war, 1094 / 1545 = 70.8% econ) — the mechanism;
2. cities at T75 / T105 / end vs the paired baseline — the deficit;
3. score. §8 of `00-context` applies: the mechanism moving without
   score is information, and the honest reading of prod-keep is that
   we have already seen delivery move without score once.

## 8. Retention

- `benchmark/results/settlerpipe/seed-{101,102,103}` — the three
  identity probes, saves dropped, `game.log` filtered to the settler
  and site lines and zstd-compressed. Their scorelogs are the identity
  proof for both instrumentation commits.
- Every table above regenerates with `benchmark/settlerpipe.py`
  against `benchmark/results/tune-postfix/s6-nogates-{war,econ}/`.
