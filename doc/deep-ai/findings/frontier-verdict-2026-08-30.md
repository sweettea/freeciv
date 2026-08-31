# The frontier was never the border, it was the wall behind it (2026-08-30)

S-FRONTIER is the epic
[the city ledger](city-ledger-2026-08-30.md) §8 proposed, and its
first commit was required to establish legality before any knob was
written:

> **Establish legality first.** Before any knob: does the ruleset
> actually permit founding inside a peaceful neighbour's border, and
> what incident does it raise? If the server refuses the action, the
> whole line is dead and the census should be redirected to `nomin`
> (30.5 tiles, deep's own spacing) instead.

**The server refuses the action.** The frontier-theft axis is dead, the
`nomin` fallback is dead for the same reason, and the bucket the census
sized turns out to name the wrong player. What is *not* dead is one
level further out, and it is the difference from `ai/baseline` the
founding-rate gap has been looking for: **deep's site search cannot
path through a peace partner's territory, and stock's can.**

And opening that wall does everything the census said it would and
nothing to the score. **Both gates ship at 0**; the epic's finding is
the mechanism and the fourth confirmation of the conserved founder
queue.

Cell: `benchmark/frontier.sh`, **TRAINING seeds 1–20**, deep self-play,
four cells × two venues (`contested.conf` = 32×32/8p/T151,
`lategame-80.conf` = 40×40/6p/T81), paired on the seed so the map
cancels exactly, **160 games** plus 3 identity, pinned binary
`.pin-bin/fix2` (`41490b381285bce1`), ports 7960–7999, nice 10, jobs 3.
Results and `report.txt` in `benchmark/results/frontier/`. Readers:
`benchmark/wallcensus.py` (new), `benchmark/frontier.py` (new),
`benchmark/cityledger.py`.

**Identity:** contested holdout seeds 101–103 at compiled defaults
reproduce the archived `tune-postfix/s6-nogates-war/holdout-report/cfg0`
scorelogs byte for byte (`id` line excluded, a per-run random token):
**3 identical, 0 different**, with four vacuity guards — the binary must
emit `deep-sitewall` and `offlegal`, and must emit neither
`deep-transit` nor any `offlegal=[1-9]`. All four held.

---

## 1. Legality: the answer is no, twice over

### (a) No benchmarked ruleset permits founding on a foreign tile

`city_can_be_built_here()` (`common/city.c:1591`) drops any tile whose
owner is not the founding player unless the ruleset carries a Found
City enabler that permits `DRO_FOREIGN`:

> Cannot steal borders by settling. This has to be settled by force of
> arms.

Neither benchmarked ruleset does. `civ2civ3` (the server default,
`GAME_DEFAULT_RULESETDIR`, and what every benchmark config selects by
leaving `RULESET` empty) and `classic` both express it as two enablers
whose union is *"unclaimed, or ours"*:

| enabler | target `CityTile/Claimed` | actor `DiplRel/Foreign` |
|---|---|---|
| `enabler_build_city_pioneer` | present **FALSE** | — |
| `enabler_build_city_domestic` | present **TRUE** | present **FALSE** |

The rule is about **ownership, not stance**: it holds at war too, and
an ally's tile is as foreign as an enemy's. So deep's categorical
refusal is not merely redundant with the server's, it is strictly
*weaker*, and deleting it could not found a single extra city.

`scripts/check-found-city-foreign.sh` pins this as a test. It is not
vacuous: `granularity` really does permit founding abroad under three
of its four government-gated enablers, and the scanner separates the
two cases. **No incident is raised, because no action occurs.**

### (b) The bucket names the wrong player

`pplayers_in_peace()` is TRUE for `DS_PEACE`, `DS_ARMISTICE`,
`DS_ALLIANCE` and `DS_TEAM` (`common/player.c:1418`). But
`deep_settler_tile_behavior()` (`deepsettler.c:150`) has already
returned `TB_IGNORE` for every tile whose owner fails
`pplayers_allied()` — `DS_ALLIANCE` or `DS_TEAM` **only** — and
`path_finding.c:288` drops a `TB_IGNORE` node before it can be reached,
entered, or offered as a destination.

**A `DS_PEACE` neighbour's tile therefore never arrives at the branch
that refuses it.** Everything `offpeace` counts is an ally's or a
team-mate's border. The ledger's *"39% of what a failed search sees is
a peaceful neighbour's border"* is 39% of an **ally's** border, and
neither reading changes the legality: an ally's ground is foreign
ground.

### (c) Measured, not only read

`deep-siteladder` gained an `offlegal` field: the tiles this branch
refused that `city_can_be_built_here()` would in fact have allowed.
**Zero on every line of every game measured**, contested, 3 games. The
theft axis is dead in play as well as on paper.

### (d) The `nomin` fallback is dead too

The ledger offered `nomin` (30.5 tiles a failed T51–75 search) as the
redirect. It is not deep policy either:
`citymindist_prevents_city_on_tile()` is called from
`city_can_be_built_tile_only()` (`common/city.c:1625`), a server rule
inside the same legality check. `game.info.citymindist` is a server
setting, identical in both paired arms.

Adding up what a failed T51–75 search actually sees
(`siteplan/war/c00`, 60 games, means over empty searches; the buckets
partition `seen` and the arithmetic closes to 0.01):

| bucket | tiles | share | whose rule |
|---|---:|---:|---|
| `offpeace` (an ally's border) | 43.29 | 39.2% | **server** — foreign tile |
| `noterr` (`TER_NO_CITIES`) | 30.39 | 27.5% | **server** |
| `nomin` (citymindist) | 30.45 | 27.6% | **server** |
| `mindist` (our own reservation) | 4.09 | 3.7% | deep |
| `offbridge` (accidental land bridge) | 1.82 | 1.6% | deep |
| `nolegal` (other refusals) | 0.21 | 0.2% | server |
| `starve` | 0.19 | 0.2% | deep |
| **`scored`** | **0.00** | **0.0%** | — |
| total `seen` | 110.45 | | |

**94.5% of what a failed search sees is refused by a rule the server
enforces regardless of what deep thinks, and 5.5% by deep's own — of
which 4.09 tiles are reservations held by deep's own other settlers.**

> **Correction.** The commit message of `2246fc28ec` ("census the free
> land deep's own border wall hides") states this share as 99.6%. That
> is wrong; the figure is 94.5%. The conclusion it supports — that
> essentially none of a failed search's refusals is a valuation
> decision a price could change — is unaffected, and the row that
> carries it is `scored = 0.00`.

---

## 2. Where the frontier actually closes

Not at the siting rule. At the **pathfinder**.

`deep_settler_tile_behavior()` returns `TB_IGNORE` for every non-allied
owner's tile, so the free land *behind* a peace partner's border is not
refused by the search — it is invisible to it. That is why a failed
search can report `scored=0` with `seen=110` while standing next to
somewhere to put a city: **every bucket on `deep-siteladder` is already
post-truncation**, which the counter's own comment
(`deep_site_note_tbskip()`) said and nothing had measured the other
side of.

**Stock has no wall.** `find_best_city_placement()`'s phase 1 sets no
`get_TB` whatever (`ai/baseline/default/daisettler.c:913`); only the
phase-2 overseas leg sets `no_fights_or_unknown` (`:966`). And a
settler is `NonMil`, so `players_non_invade()` does not stop it walking
(`common/movement.c:775`, `path_finding.c:302-303`) — the legality
argument W12H already established for corridors.

So a baseline settler walks through a peace partner to the free land
beyond it, and a deep settler cannot see that the land is there. **This
is a deep-vs-baseline asymmetry deep pays for, and it has the shape the
founding-rate gap has.**

## 3. The wall census: what it hides

`deep-sitewall` re-runs the same pf map — same unit, same risk
callbacks, same everything — with the ownership test replaced by
`deep_frontier_reachable()` (peace partners open; war, cease-fire and
no-contact still shut, because the wall's error is about **risk**, not
legality, and a settler in a war enemy's territory dies there). Bounded:
once per player-turn, only on a search that came home empty, only on
the phase-1 land leg, at most 24 tiles scored nearest-first.

Contested, the 2x2's own `war/c00` cell, **20 games, 8890 censuses**.
Per empty search:

| band | empty searches with land behind | `seen` | `wseen` | `wextra` | `wland` | **`wfree`** | `wbest` | `wturns` | net |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| T1–25 | **41.0%** | 3.2 | 47.7 | 44.5 | 36.4 | **27.12** | 158.9 | 7.0 | 65.3 |
| T26–50 | **47.8%** | 40.6 | 86.9 | 46.3 | 38.4 | **10.94** | 149.5 | 13.6 | 26.3 |
| T51–75 | 10.8% | 91.8 | 105.3 | 13.4 | 11.1 | 0.39 | 5.5 | 17.8 | 0.6 |
| T76–100 | 4.5% | 94.8 | 102.2 | 7.4 | 5.7 | 0.06 | 0.0 | — | 0.0 |
| T101–125 | 2.3% | 79.3 | 84.2 | 4.9 | 3.6 | 0.03 | 0.5 | 8.0 | 0.2 |
| T126–150 | 0.7% | 77.9 | 81.5 | 3.6 | 2.8 | 0.01 | 0.0 | — | 0.0 |

Three contested HOLDOUT seeds (101–103) at compiled defaults give the
same shape independently: 52.6% / 60.3% / 12.0% of empty searches with
land behind in the first three bands, `wfree` 15.58 / 7.21 / 0.18.

`wfree` is `city_can_be_built_here()`, so ownership, citymindist and
terrain are all honoured — **founding inside the border stays refused**;
this counts only land *beyond* it. `wbest` is raw; `net` applies the
shipped `site_travel_discount = 3` at the mean `wturns`, for
comparability only.

**Read.** In T26–50, nearly half of the searches that come home with
nothing have on average 10.9 legally foundable tiles behind a peace
partner's border, worth 150 raw at a mean 14-turn walk — against zero,
which is what the search returns. By T51–75 it is 0.39 tiles and 11% of
searches, and from T76 the map is genuinely full.

**The census predicts the gate exactly.** Run the same reader over the
`c10` cell, where `frontier_transit_gate = 1`: `wextra` is **0.00 in
every band**, because the counterfactual map and the actual map are now
the same map. The gate implements precisely the quantity the census
measured, which is the strongest available check that the two are about
one thing.

**So the wall costs cities in the opening and the first half, not in
the fork band.** That is a real prediction and it is not the one the
ledger's T51–75 framing would have made: opening the wall at T30 buys a
founding at T45, because the walk is fifteen turns.

## 4. The two levers

### `frontier_transit_gate` — the wall comes down for a peace partner

`deep_frontier_transit_open()` replaces the ownership half of the tile
behaviour. Installed in `deep_settler_apply_risk()`, which the search
and the **mover** share, so planning and execution cannot disagree
about which tiles exist (RC5). Founding inside the border is untouched
and stays refused one level in at `city_can_be_built_here()`: the gate
opens ground to *walk over*, not ground to settle.

The arm is therefore wider than the site search alone, deliberately.
`deep_settler_apply_risk()` has seven call sites and all seven are
civilian pathfinding — the site search, the nearest-own-city lookup,
the route-exists test, `deep_settler_goto()`, the flee/reposition
search, and the join-city search — so under the gate every deep
civilian may cross a peace partner's border, and none of them may
found, road or work there. A gate that opened the *planner* alone would
plan cities the mover could not reach, which is RC5 exactly.

The price is pf extra cost, `frontier_transit_price * PF_TURN_FACTOR`
per foreign tile entered — the unit `adv_avoid_risks()` charges an
enemy ZOC 20 of. `deep_settler_transit_EC()` **chains** onto
`adv_avoid_risks()`'s own callback rather than replacing it:
`prefer_short_stacks()` is static in `server/advisors/advgoto.c` and
may not be made visible (rule 4), and replacing it would silently
delete the settler's whole danger pricing.

**No `reach` constant.** The brief proposed one; the price makes it
unnecessary. Transit gets steadily more expensive the deeper it runs
and `amortize()` de-ranks the sites behind it on the same curve it
de-ranks every other distant site, so nothing has to decide how many
tiles is too many.

### `founder_floor_gate` — an undoable empty search does not zero the want

`deep_contemplate_new_city()` writes `founder_want = 0` on every NULL
result, and the want is only recomputed every `AI_CITY_RECALC_SPEED`
turns, so **one** empty search takes the city out of the founder queue
until the next recalculation. `w0` reaches 72% of fresh searches at
T51–75 while `held` per city falls from 0.594 to 0.231, and the founder
stock ends at 0.531 a seat against baseline's 2.525.

But an empty search is not evidence of a full map, and
`deep_site_blocked_seen_turn()` already records when the failure was
one a turn could undo — it is the signal `founder_join_guard` reads on
the *consumption* side. This is the same signal on the *production*
side. `deep_remaining_sites()` bounds it, so an empty search with
nothing left to settle still zeroes the want exactly as today — which
matters, because §3 says the map really is full after T75.

## 5. The 2×2

### Pre-registered, written before the run finished

Stated here rather than reconstructed afterwards, so that a surprise is
readable as a surprise.

1. **`c10` should move the SEARCH first.** `empty%@26-50` falls, `found`
   rises, and `deep-transit` fires. If `empty%` does not move, the gate
   did not reach the decision and nothing downstream is interpretable.
2. **`c10`'s falsification is `unitslost`.** Settlers now walk further
   and through other people's ground. §3 says the median winning walk
   behind the wall is 15 turns; a settler exposed for fifteen turns is
   a settler that can die. If cities rise and `unitslost` rises with
   them, the gate is buying foundings with losses.
3. **`c01` should move the QUEUE, not the search.** `w0` and `empty%`
   are properties of the search and must be unchanged to three
   decimals; `floor_kept`, `alive@` and settlers built rise. Its
   failure mode is a hoard: founders alive up with cities flat.
4. **The interaction should be NEGATIVE on `floor_kept`.** The transit
   gate removes empty searches, which is exactly the event the floor
   gate fires on, so the two partially disarm each other by
   construction — the same shape S-FEARSINK found between the site cap
   and the opportunity price, and the reason this is a factorial.
5. **What a null looks like:** `c11 − c00` no larger than
   `(c10 − c00) + (c01 − c00)`, the interaction inside its SE, and no
   cell clearing its own SE by a reasonable margin in **both** venues.

### An instrumentation caveat, found by reading after the binary was pinned

`deep_site_note_transit()` increments a static that
`deep_settler_map_iterate()` resets at the top of each search, but
`deep_settler_tile_behavior()` is also the `get_TB` of the MOVER's
pathfinders (`deep_settler_goto()`, the nearest-own-city lookup, the
flee search), which do not reset it. So `deep-transit`'s `n` can carry
crossings from a mover's pf map into the next search's line, and a
search that crossed nothing can still emit a line.

`transit_lines` and `transit_tiles` are therefore **upper bounds on the
search-side crossings**, not exact counts. Nothing else is affected:
`found`, `best`, `empty%`, `cities` and every scorelog column are
per-search or per-game quantities the counter does not enter, and the
claim the counter carries -- that the gate executes -- is if anything
under-stated by being an over-count. Not fixed here: the binary was
already pinned and a live run may not be rebuilt.

### The result

**Both gates stay at 0.** Neither clears its SE by a reasonable margin
in both venues, and one of them is a large, unambiguous loss.

Score per seat, paired on the seed, n = 20 a cell:

| cell | contested (T151, 8 seats) | wins | lategame-80 (T81, 6 seats) | wins |
|---|---|---|---|---|
| `c10` `frontier_transit_gate` | −0.05 ± 2.22 (t −0.02) | 10/20 | **+1.53 ± 1.23** (t +1.24) | 11/20 |
| `c01` `founder_floor_gate` | **−10.17 ± 1.38** (t −7.36) | **0/20** | −0.77 ± 1.48 (t −0.52) | 12/20 |
| `c11` both | −7.99 ± 1.93 (t −4.14) | 4/20 | −1.06 ± 1.70 (t −0.62) | 12/20 |
| **INTERACTION** | **+2.23 ± 1.84** (t +1.21) | | **−1.82 ± 1.50** (t −1.21) | |

Interaction = `(c11 − c10) − (c01 − c00)`, taken per seed: the four
cells play the same map on the same seed, so the map cancels and the SE
is the SE of that contrast — the only honest one, because the cells are
not independent samples.

### `c10`: the mechanism is a large, clean win and the score is a wash

Contested, `c10 − c00`, per seat unless marked:

| quantity | delta | SE | t |
|---|---:|---:|---:|
| `transit_found` (searches that crossed and found, per game) | +83.70 | 5.64 | +14.85 |
| `empty%` T1–25 | −2.25 | 0.71 | −3.20 |
| **`empty%` T26–50** | **−10.79** | 1.75 | **−6.16** |
| `empty%` T51–75 | +3.35 | 3.05 | +1.10 |
| `wpos` (searches that came home with a want, per game) | +15.45 | 5.20 | +2.97 |
| **founders alive @T50** | **+0.562** | 0.108 | **+5.21** |
| settlers @T50 | +0.594 | 0.124 | +4.79 |
| cities @T50 | +0.062 | 0.035 | +1.81 |
| **cities @T75** | **+0.147** | 0.059 | **+2.48** |
| cities @T100 | −0.029 | 0.115 | −0.25 |
| cities @end | −0.030 | 0.101 | −0.30 |
| **`unitslost` @T75** | **−0.690** | 0.231 | **−2.98** |
| **score @end** | **−0.052** | 2.224 | −0.02 |

Everything the census predicted happened. The wall was closing the
search: opening it cuts the T26–50 empty rate by a quarter, nearly
doubles the founder stock at T50, and buys +0.147 cities a seat at T75
(t +2.48). The falsification did **not** fire — `unitslost` goes *down*,
significantly, in both venues, so settlers walking through other
people's ground are not dying for it; if anything the extra sites are
nearer than the ones deep was reaching for before.

And then the gain evaporates by T100 and the score does not move.

This is **the conserved founder queue for the fourth time**, and this
cell is the cleanest statement of it yet, because unlike S-FEARSEAT and
S-FEARSINK this arm did not move shields around — it added *sites*, the
one input everyone assumed was the binding constraint. The city ledger
says the same thing on the founding side: cumulative extra foundings
`c10 − c00` are **+0.113 ± 0.068 through T75** and **+0.106 ± 0.057 to
the end**, with `d(died)` never worse than −0.013 in any band. The
foundings are real, they persist, and the score does not know.

`empty%` at T51–75 goes *up* (+3.35): the sites behind the wall were
consumed earlier, so the later searches have less left. Same shape as
`c10` in S-FEARSINK, and the same conservation.

**The econ venue explains why its score is different and its mechanism
is not.** On lategame-80 (40×40, six seats) the wall barely binds:
`transit_found` is 19.55 ± 7.09 against contested's 83.70, and `empty%`
does not move at all (−0.11 / −0.26 / −0.02, every one inside its SE).
The map is roomier, so there is little behind the border to reach. The
+1.53 ± 1.23 there is therefore **not** a replication of the contested
mechanism; it is a 1.2-sigma number produced by an arm whose mechanism
hardly ran, and it should not be read as support.

### `c01`: the failure mode was the predicted one, and it is expensive

Contested, `c01 − c00`:

| quantity | delta | SE | t |
|---|---:|---:|---:|
| `floor_kept` (wants saved, per game) | +280.30 | 18.88 | +14.85 |
| `w0` (empty searches that zeroed a want) | −269.30 | 19.90 | −13.53 |
| `wpos` | +239.85 | 20.11 | +11.92 |
| settlers @T50 | +0.169 | 0.066 | +2.55 |
| **settlers @end** | **+0.195** | 0.060 | **+3.28** |
| cities @T75 | +0.012 | 0.077 | +0.15 |
| **cities @T100** | **−0.202** | 0.096 | **−2.11** |
| **cities @end** | **−0.276** | 0.132 | **−2.09** |
| `landarea` @end | −2781.5 | 1339.9 | −2.08 |
| pop @end | −4.523 | 0.702 | −6.45 |
| techs @end | −1.157 | 0.459 | −2.52 |
| **score @end** | **−10.17** | 1.38 | **−7.36**, 0/20 |

The gate does exactly what it was written to do — `w0` falls by 269
wants a game and `wpos` rises by 240 — and the result is a **hoard**:
more settlers at the end, *fewer* cities, less land, less population,
and a score that loses in twenty games out of twenty. The city ledger
confirms the direction on the founding side: cumulative foundings are
**−0.200 ± 0.124 to the end**, i.e. the arm that keeps the founder want
alive makes *fewer* cities.

This is `founder_floor_want = 1.0` behaving exactly as its own comment
said it would: the want never decays, so once a player is walled in it
keeps bidding for founders it cannot place, for the remaining
seventy-five turns of a T151 game. The econ venue, which ends at T81,
is a wash (−0.77 ± 1.48) — consistent with the same mechanism given a
third of the time to compound.

**A decayed floor is not thereby rescued.** `founder_floor_want` was
adjudicated at its placeholder, on purpose, and a sweep on an
unadjudicated mechanism would be unreadable. What this cell establishes
is that the *undoability signal is not sufficient on its own*: the
searches it saves a want on are, in the war venue, mostly searches that
were right to come home empty. `deep_remaining_sites()` was supposed to
bound that and did not — which is itself a finding about
`deep_remaining_sites()`, not only about this gate.

### The interaction: sub-additive in one venue, super- in the other, both inside SE

Contested `+2.23 ± 1.84`, econ `−1.82 ± 1.50`. Neither clears its SE.

But the interaction is *not* null on the quantities the two gates
share, and the sign is informative. Contested: cities @end
**+0.335 ± 0.132 (t +2.54)**, pop **+1.409 ± 0.584 (t +2.41)**,
`landarea` **+3038 ± 1274 (t +2.39)**. The transit gate **partially
rescues the floor gate's damage**, which is what the structure predicts:
the floor fires on empty searches, and the transit gate removes empty
searches, so with both on there are fewer occasions to hoard against.
`c11 − c01` on score is +2.18 against `c10 − c00`'s −0.05.

Pre-registration item 4 predicted a *negative* interaction on
`floor_kept` for the same reason and got **+20.85 ± 16.96**. The
prediction was wrong and the reason is worth recording: the transit
gate raises the number of searches (`search@51-75` +20.75 in `c10`,
+63.15 in `c11`), so it removes the *rate* of empty searches while
raising their *count*. Two effects in opposite directions, and the
count won.

Pre-registration item 3 was also wrong: `c01` was predicted not to move
the search, and it moved it hard (`search@26-50` +14.35, t +5.44;
`empty%@26-50` +4.48, t +3.24). The floor does not touch the search
*directly*, but it keeps more founders alive, and every extra founder
runs another search. A production-side gate reaches the search through
the founder population, which is a seam worth remembering.


## 6. Verdict

> **The frontier axis the epic was proposed on is dead by ruleset:** no
> benchmarked ruleset permits founding inside a foreign border, the
> refusal deep makes is strictly weaker than the server's, and
> `offlegal` is 0 in play. The `nomin` fallback is dead for the same
> reason. Of the 110.45 tiles a failed T51–75 search sees, 94.5% are
> refused by a server rule and `scored` is 0.00: **not one of a failed
> search's refusals is a valuation decision a price could change.**

> **The frontier that IS deep's own is the pathfinder wall**, and the
> census sized it: in T26–50 nearly half of the searches that come home
> empty have ~11 legally foundable tiles behind a peace partner's
> border. Opening it works exactly as measured — the T26–50 empty rate
> falls 10.8 points (t −6.16), the founder stock at T50 nearly doubles
> (t +5.21), cities at T75 rise +0.147 ± 0.059 (t +2.48), and
> `unitslost` falls rather than rising. **And the score does not move**
> (−0.05 ± 2.22).

> **The founder floor is a rejection, not a null**: −10.17 ± 1.38 per
> seat on contested, 0 wins in 20 games, with more settlers and fewer
> cities. `founder_floor_want = 1.0` never decays, and
> `deep_remaining_sites()` did not bound it.

> **Both gates ship at 0.**

**The fourth sighting of the conserved founder queue, and the first one
that cannot be about shields.** S-FEARSEAT freed military shields,
S-SITEPLAN stopped dissolving founders, S-FEARSINK combined them — all
three moved foundings in time and none moved the total. This arm added
*sites*, which is the input every one of those verdicts named as the
thing that was probably missing, and got the same answer: **+0.113 ±
0.068 cumulative foundings through T75 that are worth nothing by T100.**

The queue is not conserved because of the settler supply, and not
because of the join, and not because of the site supply. Whatever
converts a founding into score is saturating somewhere else, and no
expansion-side lever measured on this project has found it. **The next
epic on this chain should stop adding foundings and go find out what a
marginal city is worth**, because four independent interventions now
agree that at the margin it is worth approximately zero.

## 7. Constants owed to tuning (rule 7)

| name | value | status |
|---|---|---|
| `frontier_transit_gate` | 0 | today's wall, byte for byte. **Ships at 0.** |
| `frontier_transit_price` | 1.0 | **PLACEHOLDER**, unmeasured — one `PF_TURN_FACTOR` per foreign tile is a reference unit, not a fitted magnitude. Never swept: the gate it belongs to is a score wash at the placeholder. |
| `frontier_transit_stance_floor` | 0 | **PLACEHOLDER** — a peace partner only; 1 admits a cease-fire and 2 war. Never swept. |
| `founder_floor_gate` | 0 | today's zero, byte for byte. **Ships at 0, and is a measured loss when on.** |
| `founder_floor_want` | 1.0 | **PLACEHOLDER**, unmeasured — the limit case, and the one whose failure mode the cell then measured at −10.17 ± 1.38. A decay sweep is NOT a rescue: see §5. |
| `founder_floor_reach_ref` | 1.0 | **PLACEHOLDER**, unmeasured. The cell is also evidence that `deep_remaining_sites()` does not bound a standing want the way this constant assumed. |

Constants the brief named that were **not** created, and why:
`frontier_theft_price`, `frontier_theft_stance_floor` and
`frontier_theft_reach` price an action the server does not permit;
`frontier_transit_reach` is subsumed by `frontier_transit_price`.

## 8. Two things not touched

- **`migration_seed_*`.** W17's, kept separate by the ledger's own
  instruction. The econ venue's 0.617 razed size-1 cities a seat are
  still unmodelled.
- **`ai/default` and `ai/baseline`.** Unmodified. Nothing here is an
  upstream bug: stock's *"Land theft does not make for good
  neighbors"* is redundant rather than wrong, and stock's missing
  `get_TB` on the phase-1 search is the *permissive* side of the
  comparison, so `findings/upstream-bugs.md` gains nothing.

## 9. The free by-product

`ai/deep` now wires `funcs.city_destroyed` (`deepai.c`), which the
ledger's §9 named. `remove_city()` calls `city_lost` then
`city_destroyed` for the same city (`server/citytools.c:1728-1729`)
while `transfer_city()` calls `city_lost` alone, so a loss followed by a
destruction of the same id in the same turn is a **razing** and a loss
without one is a **hand-over**. `cityledger.py` had to infer that from
whether the all-player total was conserved; it is now named at the
source, for every removed city in any game, `ai/baseline`'s included
(the hook is `CALL_FUNC_EACH_AI`). `dpai_city_lost()`'s comment — *"the
server does not say which here"* — was wrong and is corrected.

Not in the binary this 2×2 pinned: it is a log line and takes no
decision.

## 10. Retention

`benchmark/results/frontier/` — 163 games (160 cell + 3 identity),
saves deleted, logs filtered to six deep census tags and
zstd-compressed. `report.txt` carries every table above plus the wall
census and the city ledger for both venues.
