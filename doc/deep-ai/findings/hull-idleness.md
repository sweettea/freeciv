# Why any individual hull is idle

Measured 2026-08-04. 20 paired seeds, `isolated-150`, `--arms deep`,
pinned binary, 20/20 DONE, 0 pairing violations. Instrument:
`deep_hull_census()` (one `deep-hull-turn` line per hull per
player-turn) reconstructed per hull by `benchmark/hullcensus.py`.

The epic that preceded this established that idleness is INVARIANT to
fleet size: five interventions moved the fleet between 4.57 and 6.24
hulls per player-turn and left the idle share at 75-80% every time,
and cutting hulls cut arrivals with it. A per-turn fleet count cannot
say why a particular boat is idle, so this asks per hull, over its
whole life.

## The census

    1244 hulls over 20 games, 48397 hull-turns (38.9 turns of life each)

    carrying              11329   23.4%
    assigned, empty        4585    9.5%
    op-claimed, empty       328    0.7%
    idle                  32155   66.4%

    never carried anything, ever          590 hulls   47.4%
    turns from birth to first cargo       median 7, mean 10.4, max 104

## The answer: idle hulls are mostly idle because there is nothing there

Of the 32155 idle hull-turns:

    with a unit waiting to cross ITS water     5141   16.0%
    ... that unit within 10 tiles               2418    7.5%
    ... within 5 tiles                          1031    3.2%

**Eighty-four per cent of idle hull-turns have no waiting passenger
anywhere on that hull's body of water**, and 96.8% have none within
five tiles. This is not an assignment failure. A matching bug would
show idle hulls beside waiting passengers, and that population is
3.2% of idle time.

The three-way ladder the handoff posed resolves like this:

    wrong harbour   255 hulls  20.5%   demand NEVER appeared on its
                                       water, in its entire life
                                       (15.1% of all idle turns)
    missed it       360 hulls  28.9%   idle while demand stood
                                       somewhere on its water -- but
                                       only 23.4% of hulls ever had
                                       that unit within 10 tiles
    mistimed          6 hulls   0.5%
    worked          654 hulls  52.6%   carried at least once

"Missed it" is the weakest of these categories and must not be read as
a matching defect: one ocean on an island map can be the whole world
sea, so "demand on its water" means only that somebody, somewhere,
wanted a crossing. The distance columns are the honest version, and
they say the hull and the passenger are rarely near each other.

## What ends an idle spell

    crossing     551   19.5%
    assigned    1166   41.2%
    claimed      123    4.3%
    death        164    5.8%
    gameend      827   29.2%

Only one idle spell in five ends in a crossing. Twenty-nine per cent
never end at all -- the boat sits until the horizon. And 5.8% end with
the hull dying; 109 hulls (8.8%) died having never carried anything.

The 9.5% of hull-turns spent "assigned, empty" -- a passenger named,
nobody aboard -- is a second, smaller waste the idle predicate hides:
those turns count as BUSY. For every 2.5 turns a hull spends carrying,
it spends one holding an assignment with an empty hold.

## What this decides

**Fleet COUNT is the wrong lever, confirmed from the hull side.** The
fleet is not sitting idle beside unserved demand; it is sitting idle
where and when there is no demand. Cutting the count removes hulls
from the burst as readily as from the lull, which is exactly why five
attempts moved the share not at all and cost arrivals.

**The cleanly recoverable slice is the wrong-harbour class: one hull
in five is built on a water nothing ever wants to cross**, and those
hulls own 15.1% of all idle time. That is the "useful oceans" gap
already spec'd (NEXT-SESSION section 1c): the boat want is gated on
`is_terrain_class_near_tile(TC_OCEAN)` -- "am I coastal" -- and never
asks whether that particular water touches the demand. The
infrastructure is present and unread (`tile_continent()` is negative
for water, `adv->oceans[]` is rebuilt every phase).

**The rest is a timing problem, not a placement or matching one.**
Demand is bursty; the fleet is standing inventory sized against a
peak. Whether that inventory is worth its shields is a pricing
question, and the honest way to ask it is against the wait a
passenger would face without it -- not against the idle count, which
is what every attempt so far has priced.

## Caveats on the instrument

- Demand is `deep_ferry_demand_on()`'s predicate: a unit not
  transported, with no ferry assigned, whose `goto_tile` is on another
  continent, adjacent to that water. **A unit that wants a crossing
  but has not been given a goto tile yet is not counted**, so latent
  demand is undercounted and the wrong-harbour class is, if anything,
  slightly overstated.
- The census is logging only, guarded by the metric log level; it
  changes no decision.
- One cell (`isolated-150`) and one arm configuration. The idle share
  here (66.4%) sits just below the 75-80% band the fleet aggregate
  reported on the same cell, because this denominator counts every
  hull-turn of every ferry-capable hull rather than the per-turn
  snapshot's population.

---

## Addendum: how much of the wrong harbour is decidable in advance (2026-08-04)

"Demand never appeared on its water" is a fact about the game that
ran. A build-time gate needs a test that holds before the shields are
spent, so the census gained one: `bridge=`, 1 when some water the hull
touches reaches a second land continent at all.

No demand enters this test. It is the same question
`get_lake_surrounder()` has answered since map generation --
`assign_continent_numbers()` calls `recalculate_surrounders()` on every
terrain change -- and nothing in the AI has ever read it.

Four `isolated-150` games, `--arms deep`, 217 hulls:

    closed-water hulls        15    6.9% of hulls
    their idle hull-turns    664   11.2% of all idle time
    of those, never carried   13 of 15
    carried anyway             2    same-continent hops

    (wrong harbour, same run: 24 hulls, 639 idle turns)

Two things worth stating plainly:

- **The geometry class owns MORE idle time than the demand class**
  (664 against 639) while containing fewer hulls. A hull on a closed
  sea can sit beside a unit that wants a crossing it could never
  provide, so it is scored as "demand stood on its water" and misses
  the wrong-harbour bucket entirely while being just as useless.
- **The cost of refusing them is two hulls in 217.** That is the
  measured price of the gate's one false positive class,
  same-continent coastal hops.

A single-surrounder sea needs no `home` argument to judge: every city
adjacent to it stands on the one continent it touches, so it is
useless to all of them.
