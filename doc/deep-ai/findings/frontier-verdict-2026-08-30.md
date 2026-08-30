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

Cell: `benchmark/frontier.sh`, TRAINING seeds 1–20, deep self-play,
four cells × two venues, paired on the seed, pinned binary
`.pin-bin/fix2` (`41490b381285bce1`), ports 7960–7999, nice 10, jobs 3.
Results in `benchmark/results/frontier/`. Readers:
`benchmark/wallcensus.py` (new), `benchmark/frontier.py` (new),
`benchmark/cityledger.py`.

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

Contested, 3 games, 1248 censuses. Per empty search:

| band | empty searches with land behind | `seen` | `wseen` | `wextra` | `wland` | **`wfree`** | `wbest` | `wturns` | net |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| T1–25 | **52.6%** | 1.5 | 26.5 | 25.0 | 21.2 | **15.58** | 220.5 | 7.7 | 82.5 |
| T26–50 | **60.3%** | 44.9 | 96.1 | 51.2 | 44.5 | **7.21** | 145.7 | 15.1 | 21.1 |
| T51–75 | 12.0% | 73.5 | 98.8 | 25.3 | 21.7 | 0.18 | 4.5 | 21.7 | 0.3 |
| T76–100 | 2.2% | 86.1 | 94.1 | 8.0 | 6.4 | 0.02 | 0.0 | — | 0.0 |
| T101–125 | 1.7% | 76.9 | 81.4 | 4.5 | 2.7 | 0.02 | 0.0 | — | 0.0 |
| T126–150 | 0.3% | 88.4 | 89.7 | 1.3 | 1.1 | 0.00 | 0.0 | — | 0.0 |

`wfree` is `city_can_be_built_here()`, so ownership, citymindist and
terrain are all honoured — **founding inside the border stays refused**;
this counts only land *beyond* it. `wbest` is raw; `net` applies the
shipped `site_travel_discount = 3` at the mean `wturns`, for
comparability only.

**Read.** In T26–50, three fifths of the searches that come home with
nothing have on average 7.2 legally foundable tiles behind a peace
partner's border, worth 146 raw at a mean 15-turn walk — against zero,
which is what the search returns. By T51–75 it is 0.18 tiles and 12% of
searches, and from T76 the map is genuinely full.

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

### The result

PENDING

## 6. Constants owed to tuning (rule 7)

| name | value | status |
|---|---|---|
| `frontier_transit_gate` | 0 | today's wall, byte for byte |
| `frontier_transit_price` | 1.0 | **PLACEHOLDER**, unmeasured — one `PF_TURN_FACTOR` per foreign tile is a reference unit, not a fitted magnitude |
| `frontier_transit_stance_floor` | 0 | **PLACEHOLDER** — a peace partner only; 1 admits a cease-fire and 2 war, and they exist so a tuner can find out |
| `founder_floor_gate` | 0 | today's zero, byte for byte |
| `founder_floor_want` | 1.0 | **PLACEHOLDER**, unmeasured — the limit case, and the one with the obvious failure mode |
| `founder_floor_reach_ref` | 1.0 | **PLACEHOLDER**, unmeasured — one counted site is the unit case |

Constants the brief named that were **not** created, and why:
`frontier_theft_price`, `frontier_theft_stance_floor` and
`frontier_theft_reach` price an action the server does not permit;
`frontier_transit_reach` is subsumed by `frontier_transit_price`.

## 7. Two things not touched

- **`migration_seed_*`.** W17's, kept separate by the ledger's own
  instruction. The econ venue's 0.617 razed size-1 cities a seat are
  still unmodelled.
- **`ai/default` and `ai/baseline`.** Unmodified. Nothing here is an
  upstream bug: stock's *"Land theft does not make for good
  neighbors"* is redundant rather than wrong, and stock's missing
  `get_TB` on the phase-1 search is the *permissive* side of the
  comparison, so `findings/upstream-bugs.md` gains nothing.
