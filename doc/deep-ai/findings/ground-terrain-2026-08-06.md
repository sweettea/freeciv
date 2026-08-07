# Ground value that is not tile yield (S-GROUND, 2026-08-06)

The session that built the first want family whose currency is not
food/shield/trade. Four mechanisms, one of them retired on its own
census before it was built, plus one re-shape of an existing term.
This file holds the FACTS the round established; the numbers and the
verdicts are in `STATUS.md` under the same date.

---

## 1. The base-valuation hole, and the channel asymmetry nobody had

`action-surface.md` established that a base's want is exactly zero
because `adv_calc_extra()` -> `city_tile_value()` sums food, shield and
trade and a base changes none of them. That is correct and it is now
built against.

**What it did not have, and what cost this session one wrong commit:
the two benchmark rulesets deliver a fortress's defence through
DIFFERENT CHANNELS.**

| | how the defence arrives | readable by |
|---|---|---|
| classic | `defense_bonus = 100` on the Fortress extra (`data/classic/terrain.ruleset:1489`) | `tile_extras_defense_bonus()` |
| civ2civ3 | **no `defense_bonus` field at all** on Fort or Fortress; +50% and +50% are `Defend_Bonus` EFFECTS (`data/civ2civ3/effects.ruleset:273-290`) | only the effects system |

A want written against `extra->defense_bonus` is therefore correct in
exactly one venue, and the primary ruleset is the other one. The first
cut of the base want did exactly that and measured **zero base
evaluations against 625 candidate offers in two civ2civ3 games**.

The general lesson, and it generalises past bases: **a ruleset fact
that exists as a FIELD in one ruleset can exist as an EFFECT in
another, and only the ruleset's own evaluator sees both.** The fix is
`get_virtual_defense_power()`, which takes a tile and unit types and
therefore answers counterfactually on a virtual tile.

Related asymmetries this confirms (from the same audit, unchanged):
classic's fortress claims borders (`border_sq = 5`) and carries
`NoStackDeath`; civ2civ3's does neither, forbids Fortress on rivers,
and stages it behind a Fort.

### The counter that found it

`deep-fort` counts `offers` BEFORE any gate and `evals` after them.
"The advisor offered a base candidate at all" and "we priced it" are
different questions, and a zero want has to answer the first one
first. One smoke run replaced what would otherwise have been a guess.

---

## 2. Capture Units: the ruleset gate is open and the population is
   empty

Instrument-before-mechanism, as briefed. `deep_capture_census()`
counts, both directions, tiles where the action is legal THIS INSTANT
-- which is the right unit of count, because Capture Units takes a
whole stack and its hard requirement is that every unit on the target
tile is capturable, so `is_action_enabled_unit_on_stack()` answers the
protection question for free.

`deep-capture-venue inuse=1` on **all 20 games** of the civ2civ3 A/B
confirms the enabler is live. Against that:

    exposed tiles (ours, an enemy capturer could take now)      0
    offered tiles (theirs, our capturer could take now)         0

**Zero, in 40 civ2civ3 games of 150 turns.** Neither half of the
mechanism has a population, so neither half was built -- which is what
instrumenting first is for. The census stays in the tree: it is the
sizing any future capture work has to start from, and the enabler's
requirements are the reason the population is what it is --

```
[enabler_capture]  actor: Capturer flag, AT WAR, foreign, 1 move frag
                   target: Capturable flag, not transporting,
                           not a city centre
```

and only three unit types in civ2civ3 carry `Capturable`: Migrants,
Caravan, Freight (`units.ruleset:560, 2638, 2691`). Nineteen capturer
types against three capturable ones is not the surface the ranked
opportunity list implied.

---

## 3. Pillage: the standing population is a tenth of the raid

The shipped mechanism acts only on a unit ALREADY STANDING on an
at-war-owned improved tile. The census counts the larger population it
does not act on: blades within `raid_radius` of an eligible tile.

Sizing at power, per game, 20 paired seeds each cell:

    counter                        civ2civ3    classic
    standing (unit-turns)             54.05     125.10
    pillages started                   7.80       9.90
    nearby   (the raid population)   837.70    1420.10

**The raid is 15x (civ2civ3) / 11x (classic) the population of the
thing that shipped**, and the shipped half fires 8-10 times a game.
That is the W36 lesson applied before building rather than after: the
cheap half is measured, and the expensive half is sized while it is
still free to not build it.

---

## 4. The barbarian re-shape: every factor is the server's, and the
   per-city division cancels

Deep's two barbarian terms were step functions -- `fc->barb` flipped
exactly at `game.server.onsetbarbarian`, `deep_barbarian_fear()`
returned a flat value from that turn. The derby-2 storm (deep lost 107
of 207 cities in 70 turns) is what a flat, late, unscaled term misses.

The shared scaling mirrors `server/barbarian.c` and invents nothing:

| factor | server | mirrored as |
|---|---|---|
| attempts per turn | `n * (rate - 1)`, refused for DISABLED and HUTS_ONLY (`:742-757`) | `(rate - 1)`, 0 below BARBS_NORMAL |
| willingness | `fc_rand(30) + 1 <= cities * (rate - 1)` (`:584-586`) | `MIN(1, cities * (rate-1) / 30)` |
| government gate | `fc_rand(100) > EFT_CIVIL_WAR_CHANCE` | `cwc / 100`, 20 (Federation) to 99 (Anarchy) in civ2civ3 |
| horde size | `fc_rand(3) + uprise * rate`, ladder at `:605-613` | `1 + uprise(n) * rate`, integer truncation included |
| anticipation | *(none -- the server has no anticipation)* | linear ramp over `barb_anticipation_turns` before onset |

**Only the anticipation is deep's own**, and it exists for a reason
the server does not have to care about: the garrison must EXIST on the
onset turn and a defender takes about ten turns to build.

### Why there is no per-city division

The brief asked for the empire exposure to be divided across
unrest-eligible cities. It turns out the division is already done by
the server's own geometry. `try_summon_barbarians()` throws a dart at
a uniformly random tile and the victim is whoever owns the NEAREST
city, so P(the victim is us) grows with our city count in the same
proportion as the number of our cities the exposure would be divided
among. **The two city counts cancel, and the honest per-city figure is
the willingness probability itself** -- rising with empire size,
saturating at `n = 30/(rate-1)`. That is the derby's "my Nth city buys
barbarian pressure" tax, priced exactly once.

What survives of the site-finder is ELIGIBILITY, not division: a city
with no unrest annulus (`[MIN_UNREST_DIST, MAX_UNREST_DIST]`, an empty
known land tile in it) can never be the victim site and pays nothing.
The approximation drops the "are we actually the nearest city" test
and the sea-raider branch; both make it permissive, never restrictive.

### Normalisation, and what it protects

The raw product ranges over ~50x between a one-city empire and a
saturated one, so shipping it unnormalised would have multiplied the
existing bases by an arbitrary factor and measured the LEVEL instead
of the SHAPE. `barb_size_ref_cities` sets the empire size at which the
scaling is 1.0, so `barbarian_fear` and `floor_barb_weight` keep the
magnitude they shipped with. All three are PLACEHOLDERS and they must
be tuned together.

---

## 5. New upstream-bug candidates

- **`adv_calc_extra()` cannot value a base.** Already filed as #36 in
  spirit by the action-surface audit; this session confirms it by
  building the want outside the advisor and watching fortresses appear
  for the first time.
- **`extra->defense_bonus` is not the whole defence story.** Any AI
  code reasoning about tile defence from that field alone is blind on
  civ2civ3. Not a bug in `ai/default` as such -- `ai/default` never
  reasons about it -- but it is a trap laid for anything that starts.

---

## 6. The attribution nobody would have guessed

Three mechanisms shipped together on civ2civ3 contested, 20 paired
seeds. Isolating the fortify hold (the cheap "small consumer", item 4)
against the two want mechanisms (items 1 and 2, the expensive ones):

    base want + pillage, no fortify hold   -0.17   se 0.89   t -0.19
    ... plus the fortify hold              +4.20   se 2.14   t +1.96

**The whole cell's move is the hold.** Teaching deep to BUILD
fortresses is worth nothing on its own -- and costs a little
expansion (-0.06 cities, t -2.03; -619 landarea, t -1.94, the
worker-turns going into bases instead of tiles). Teaching a blade
already standing on good ground to stay there is worth all of it.

The caveat that keeps this honest: the hold fires on chokepoints as
well as on bases, and a chokepoint exists whether or not anyone built
anything. So the hold's +4.20 is NOT attributable to the fortresses
it was designed to consume. The 2x2 -- `base_fort_gain` x
`fortify_ground` -- is owed, and it is the first thing the tuning pass
on this family should run.
