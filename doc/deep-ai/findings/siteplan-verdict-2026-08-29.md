# The founder site cap and the join guard, built and adjudicated (2026-08-29)

S-SITEPLAN is the response to
[the settler-pipeline census](settler-pipeline-2026-08-29.md): deep is
at exact city parity with baseline to T45 and then falls 5-7 cities
behind, and 27-39% of every founder it delivers is dissolved into an
existing city by `deep_settler_join_city()` rather than founding one.

Two gates, shipped as ONE arm because they are one mechanism:

- **`founder_join_guard_gate`** stops deep spending a founder while a
  site may open before it could build another one;
- **`founder_site_cap_gate`** stops it hoarding founders instead --
  baseline never dissolves one and ends its games holding 19-40 idle
  settlers, which is not the state we want to buy.

Factorial with `founder_run_hold`, as the census required: run_hold
puts more settlers into the pipeline and the guard decides what happens
at the end of it, so two concurrent arms could not attribute either.

Cell: `benchmark/siteplan.sh`, 40 TRAINING seeds a cell (60 for the
headline c10 contrast in the war venue), both venues
(contested.conf and lategame-80.conf), pinned binary
`.pin-bin/fix/freeciv-server` (`14c7c62ecf3acd97`), ports 7850-7899,
this worktree only. Results in `benchmark/results/siteplan/`.

## 1. Identity: both gates are inert at their defaults

Three contested games (seeds 101-103) at
`benchmark/tuning/defaults.ini`, scorelogs compared against the
**archived** `tune-postfix/s6-nogates-war/holdout-report/cfg0` games
that the settler-pipeline census was taken on. Config, tuning and
runner shas all match that run's `provenance.txt`, so the server binary
is the only difference; the `id` line is a per-run random string and is
excluded.

**3 identical, 0 different**, with four guards against a vacuous pass:
the arm must emit `tbskip=`, must emit `deep-joinguard`, and must NOT
emit `deep-joinguard ... held=1 gate=1` or `deep-sitecap ... capped=1
gate=1`. All four held.

## 2. What the new instrument says, and it corrects the census

The settler-pipeline census closed with one link it could not measure.
`deep_settler_tile_behavior()` refuses every non-allied tile and pf
drops a `TB_IGNORE` node before queuing it, so `seen` on
`deep-siteladder` is already post-truncation and "the horizon is
truncated" could not be turned into a number. `tbskip=` is that number.

43 919 searches over the 40 gate-off war games (`siteladder.py`), per
search:

| population | n | seen | off | offpeace | mindist | nomin | scored | **tbskip** | tbown | tbunit | **tbland** |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| all | 43919 | 306.1 | 68.6 | 66.9 | 5.8 | 47.9 | 2.5 | **4.7** | 3.8 | 0.9 | **3.2** |
| found a site | 9146 | 380.1 | 55.5 | 55.4 | 11.2 | 50.2 | 12.2 | 4.3 | 3.2 | 1.0 | 2.8 |
| came home empty | 34773 | 286.6 | 72.0 | 69.9 | 4.4 | 47.3 | 0.0 | 4.8 | 4.0 | 0.8 | 3.3 |

And per 25-turn bucket the two refusals move in opposite directions:

| turn | 0 | 25 | 50 | 75 | 100 | 125 |
|---|---:|---:|---:|---:|---:|---:|
| searches | 1933 | 4132 | 6853 | 10072 | 10288 | 10226 |
| seen | 39.7 | 77.9 | 252.5 | 343.2 | 363.6 | 386.7 |
| **offpeace** (the ladder refuses) | 0.0 | 17.8 | 55.9 | 72.1 | 82.8 | **85.2** |
| **tbland** (pf never offered) | 3.7 | 6.0 | 4.3 | 2.8 | 2.5 | **2.4** |

**The truncation is a thin perimeter, not the missing map.** On a
failed search the pathfinder withheld 3.3 land tiles while the ladder
itself refused 69.9 on a peace partner's border and 47.3 inside
citymindist of a real city. The census left open the possibility that
the unmeasured removal was the whole story; it is not, and the refusal
that matters is deep's own *"land theft does not make for good
neighbors"* rule, which is policy, not pathfinding. (The three
identity games agree: 5.6 tbland against 67.5 offpeace over 2903
searches.)

**The honest caveat, stated because the counter cannot say it.**
`tbskip` counts the tiles the callback was ASKED about, and a refused
tile also blocks passage, so an arbitrary area can hide behind a
three-tile perimeter. This measures the perimeter, not the area. The
area -- `tbfree` in the census's proposal, unowned land reachable only
*through* refused tiles -- needs a second pf run with the callback
removed and is still unbuilt.

## 3. What the guard's own census says before any arm ran

`deep-joinguard` is emitted with the gate off, so the counterfactual is
readable from the c00 arm itself: **5412 join attempts reached the
guard across 40 games** (135.3 a game, of which 25.1 became an actual
join), at a mean replacement cost of **11.3 turns**, and the three
signals fired

| signal | fires on |
|---|---:|
| we founded a city inside the window | 13.4% |
| a walking founder's target was cancelled inside the window | 38.5% |
| either of those two | **44.8%** |
| a search saw temporarily-blocked land | **99.6%** |
| any of the three -- what the gate acts on | 99.6% |

So at `founder_join_guard_mult = 1.0` **the guard as built is
equivalent to switching the join off**: a settler's search essentially
always sees a peace border or a reservation it could one day settle,
and only 22 attempts in 5412 saw none. That is a true answer to the
question as posed -- "is there a chance we will want to found again" is
almost always yes -- and it makes the arm a clean test of what the join
is worth. What it cannot do is distinguish a *discriminating* guard
from *no join at all*. The 44.8% line is what the narrower rule would
have declined, measured, so the next cell can be chosen without another
probe.

## 4. The A/B: 2x2 x two venues, 40 training seeds a cell

Every contrast is paired against c00 on the same seed, so the map
cancels exactly. `c10` is this epic's two gates, `c01` is
`founder_run_hold` alone, `c11` is all three.

### War venue -- contested.conf, 8 deep seats, T150

c10 was extended to **60 seeds** (1-60) after the 2x2 landed, because
it is the headline contrast and 40 left it at 1.4 SE; c01 and c11 stand
at 40. Each contrast is paired on the seeds its own two cells share.

| metric | c10 - c00 (n=60) | se | t | c01 - c00 (n=40) | se | t | c11 - c00 (n=40) | se | t |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| **score** | **+16.32** | 9.15 | +1.78 | +2.12 | 10.12 | +0.21 | -0.72 | 10.90 | -0.07 |
| cities T45 | +0.01 | 0.02 | +0.71 | +0.04 | 0.02 | +2.16 | +0.04 | 0.02 | +2.11 |
| cities T100 | +0.01 | 0.07 | +0.16 | -0.02 | 0.08 | -0.19 | +0.05 | 0.08 | +0.60 |
| cities end | -0.09 | 0.05 | -1.93 | -0.03 | 0.05 | -0.76 | -0.04 | 0.07 | -0.66 |
| citizens end | -0.13 | 0.38 | -0.33 | -0.15 | 0.45 | -0.34 | -0.31 | 0.47 | -0.66 |
| **joins** | **-24.23** | 1.17 | -20.69 | **+4.12** | 1.59 | +2.60 | -24.85 | 1.51 | -16.47 |
| settlers built | -6.70 | 0.88 | -7.58 | +0.80 | 1.09 | +0.74 | -4.45 | 0.95 | -4.70 |
| settler starts | -11.63 | 1.94 | -5.99 | -8.38 | 2.53 | -3.31 | -18.80 | 2.01 | -9.35 |
| founders alive end | +1.25 | 0.09 | +14.33 | -0.02 | 0.04 | -0.61 | +1.45 | 0.13 | +11.53 |
| culture end | +50.97 | 27.24 | +1.87 | +7.07 | 27.28 | +0.26 | +11.71 | 30.67 | +0.38 |

(Cities, citizens and founders are per seat; joins and settlers are per
game over all eight seats.)

### Econ venue -- lategame-80.conf, 6 deep seats, T80

| metric | c10 - c00 | se | t | c01 - c00 | se | t | c11 - c00 | se | t |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| **score** | +7.97 | 6.26 | +1.27 | -2.02 | 8.40 | -0.24 | **+13.78** | 8.02 | +1.72 |
| cities T40 | +0.19 | 0.07 | **+2.75** | +0.09 | 0.06 | +1.59 | +0.27 | 0.08 | **+3.20** |
| cities T60 | **+0.37** | 0.08 | **+4.68** | +0.12 | 0.09 | +1.34 | **+0.50** | 0.11 | **+4.71** |
| cities end | +0.23 | 0.11 | +2.01 | -0.15 | 0.11 | -1.34 | +0.35 | 0.13 | +2.81 |
| citizens end | **+1.45** | 0.37 | **+3.91** | +0.18 | 0.43 | +0.43 | +1.45 | 0.47 | +3.07 |
| cities founded | +0.82 | 0.83 | +1.00 | -1.48 | 0.77 | -1.91 | +2.30 | 0.86 | +2.68 |
| **joins** | **-21.68** | 0.84 | -25.78 | **+17.38** | 1.64 | **+10.57** | -21.70 | 0.84 | -25.88 |
| settlers built | -7.22 | 0.91 | -7.95 | -1.50 | 0.97 | -1.55 | -6.12 | 0.97 | -6.30 |
| settler starts | -17.95 | 3.53 | -5.08 | -20.85 | 2.67 | -7.80 | -33.73 | 3.45 | -9.78 |
| founders alive end | +1.41 | 0.12 | +11.64 | +0.01 | 0.07 | +0.19 | +2.19 | 0.13 | +16.84 |

### Four things this says

1. **The mechanism does exactly what it was built to do.** Joins fall
   from 24.3 and 21.7 a game to 0.03 and 0.05; founders standing rise
   by 1.3-1.4 per seat; settlers built fall 6-7 a game and settler
   production starts fall 11-18. The cap holds the hoard to about ten
   founders a game against baseline's 19-40, so the guard did not buy
   baseline's problem.

2. **`founder_run_hold` alone confirms the settler-pipeline finding
   exactly.** It moves score not at all (+2.12 and -2.02) and it moves
   **joins UP** -- +4.12 in the war venue (t +2.60) and **+17.38 in
   the econ one (t +10.57)**. Delivering more settlers with the join
   in place delivers them to the join. That is the prediction the
   census made in so many words, measured on its own cell.

3. **The city deficit closes in the econ venue and not in the war
   one.** Econ cities per seat +0.37 at T60 (t +4.68) and +0.23 at the
   end, citizens +1.45 (t +3.91): about 2.2 more cities a game against
   the 7.5-city deficit the census measured, so roughly a third of it.
   The war venue moves nothing at any cut. Whatever stops deep
   expanding after T45 on contested, **it is not the join** -- removing
   the join entirely leaves the curve where it was.

4. **The interaction is real and it changes sign between venues.** War:
   c10 +15.28 on the 40 seeds c11 also has (+16.32 on 60), c11 -0.72, so adding run_hold costs about 16 points.
   Econ: c10 +7.97, c11 +13.78, so adding it gains about 6. This is
   precisely the attribution the census demanded a factorial for; two
   concurrent single-factor arms would have reported the war venue's
   c11 as a failure of gates that were not failing.


## 4. Verdict

**Both gates stay 0.** The score effect is positive in three of the
four headline cells and never reaches 2 SE -- +16.32 (se 9.15, n=60)
and +7.97 (se 6.26) for the pair alone, +13.78 (se 8.02) for the pair with
run_hold in the econ venue, -0.72 (se 10.90) for the same combination
in the war venue. Under rule 6 and the brief's bar -- a gate ships at 1
only if it clears its SE by a reasonable margin -- none of them does.

**What was bought is not nothing, and it is the part worth carrying
forward.** The econ venue's expansion column moves on real t values:
+0.37 cities per seat at T60 (t +4.68), +1.45 citizens (t +3.91),
+2.30 cities founded a game with run_hold beside it (t +2.68). Section
8 of `00-context` applies in the direction it was written for -- a
mechanism that moves its proxy without moving score is information --
except that here the proxy move very nearly accounts for the score
move it did produce: +1.45 citizens per seat over six seats is +8.7
score against a measured +7.97. The gain is real and it is small, and
the variance in these venues is larger than it.

**The finding that costs the most to have learned:** the join is not
what costs deep its cities in the war venue. The census showed 24.3%
of joins followed within ten turns by that same player founding a city
and read it as a counterfactual; with joins removed entirely, the same
players found the same number of cities. The correlation was a
settler being dissolved near a city that was going to be founded
anyway, not a settler that would have founded it.

**And the guard as built is not the guard that was designed.** Section
3's decomposition -- 99.6% of join attempts see temporarily-blocked
land -- means this cell tested "never join", not "join only when
nothing suggests we will want a founder". The narrower rule is
measured and untested: found-or-cancel alone would have declined 44.8%
of attempts.

## 5. Constants owed to a tuning pass (rule 7)

- **`founder_site_cap_slack` -- PLACEHOLDER 1.0, UNTUNED.** One founder
  above the counted sites, because `deep_remaining_sites()` is a
  capacity estimate rather than a list of tiles and a cap set exactly
  at it refuses the founder walking toward a site that opens while it
  walks. Range [0, 5] in `tuning-params.ini`; registered in X, ECON and
  ALL. The cap fired 47 times a game in the war venue and 47 in the
  econ one, so it is reachable and a sweep would mean something -- but
  not against this result, where what is in question is the join, not
  the cap.
- **`founder_join_guard_mult` -- PLACEHOLDER 1.0, UNTUNED.** One
  settler-build-time, the break-even the guard's argument names. The
  census's ten-turn counterfactual is a different quantity on a
  different population and was deliberately NOT borrowed. Range
  [0, 4]. Sweeping it is close to pointless while the blocked signal
  fires 99.6% of the time: the window barely binds. Fix the signal
  first.
- Both gates are switches and their ranges are degenerate on purpose.
- `tune-audit.py`: CLEAN, 281/281.

## 6. Retention

- `benchmark/results/siteplan/` -- 363 games (3 identity + 360 A/B),
  saves deleted, `game.log` filtered to the nine deep census tags the
  tables need and zstd-compressed. Every table above regenerates with
  `benchmark/siteplan.py` and `benchmark/siteladder.py`.
- Drivers: `benchmark/siteplan.sh` (identity / ab / report),
  `benchmark/siteplan-more.sh` (extra seeds for named cells),
  `benchmark/waitdone.sh`.
- Cell inis: `benchmark/results/siteplan/c{00,10,01,11}.ini`, each one
  or two keys away from `benchmark/tuning/defaults.ini`.

## 7. Next, in order

1. **The narrower guard.** Decline the join on found-or-cancel
   recency alone (44.8% of attempts) rather than on blocked-land
   (99.6%). It needs one mode selector in `struct tuning` and
   therefore a new binary, so it needs its own identity proof against
   the c00 games kept here -- which is cheap, because those games and
   their inis are retained.
2. **Ask the war venue's question separately.** Cities are at parity
   to T45 and flat after it in that venue whatever is done to the
   join, so the stall there is elsewhere: the `offpeace` refusal (69.9
   tiles a search, 85.2 by T125) and the `nomin` ring (47.3) are what
   an empty search is made of, and neither is a valuation. The first
   is deep's own policy and could be priced rather than refused.
3. **`tbfree`.** The area behind the refused perimeter, which needs a
   second pf run with the tile-behaviour callback removed. Section 2
   bounds the perimeter and cannot bound the area.
4. **The free arrival-turn fix** (`site_arrival_turn_gate`) is still
   unbuilt and still free: 51% of arrivals land with zero move
   fragments and found one turn late, which is inherited from
   `ai/default` and shared with baseline.
