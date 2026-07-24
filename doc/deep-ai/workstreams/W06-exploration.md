# W06 — Exploration and huts

**Phase 1. Minimal W01 rework (adds a new want). MUST precede W04's flag flip.**
Root causes: RC1, RC6.

## Problem

In hut-heavy games exploration is one of the strongest early investments. The AI
never makes it — and destroys the explorers it happens to own.

## Evidence

**There is no explorer production want anywhere in `ai/default/`.** The advisors
want defenders, attackers, workers, settlers, caravans, trade-route units and
ferries. Exploration isn't among them. It happens only as a fallback: units with
`AIUNIT_NONE` may be switched to auto-explore (`dai_switch_to_explore`,
`daiunit.c:3579`).

**It actively disbands them** — `daicity.c:542`:
```c
/* Disband explorers that are at home but don't serve a purpose.
 * FIXME: This is a hack, and should be removed once we
 * learn how to ferry explorers to new land. */
```
An explorer that returns home is scrapped for shields.

**The hut bonus is a tiebreaker on already-visible tiles** —
`autoexplorer.c:199-204`:
```c
#define OWN_CITY_SCORE  (BEST_NORMAL_TILE + 1)
#define HUT_SCORE       (OWN_CITY_SCORE + 1)
```
Two points above the best possible ordinary tile. And it applies only when
`map_is_known(ptile, pplayer)` — so the AI beelines huts it has **already seen**
and never explores *toward* hut country.

`README.AI`: *"The AI will also not build units to explore new islands, leaving
huts alone."*

## Why this gates W04

An honest-information AI that can't scout can't expand — `city_desirability()`
returns NULL for unknown tiles. W04's flag flip is blocked on this working.

## Design

1. **An explorer want**, scaled to expected return:
   ```
   explore_want = huts_visible_unclaimed * HUT_TECH_VALUE
                + unknown_tiles_on_our_continent * DISCOVERY_VALUE
                + contest_pressure * FRONTIER_VALUE      /* W07, later */
   ```
   `HUT_TECH_VALUE` must read `game.server.huts` (density) and the ruleset's hut
   outcomes — in a hut-free game the want should be **zero**. Check `H_LIMITEDHUTS`
   and `H_HUTS`, which gate what the AI gets and knows.

2. **Delete or gate the disband hack** on `explore_want == 0`.

3. **Make `HUT_SCORE` proportional**, not `+2` — scale by expected hut value in
   shields/bulbs so it can outweigh several turns of travel. Same `amortize`
   treatment as everything else.

4. **Value expected huts in *unknown* area.** Hut density is a known server
   setting, so unexplored area carries expected hut value even without `H_HUTS`.
   One line in `explorer_desirable`, and it turns exploration from map-filling
   into a resource race.

5. Ferrying explorers waits for W13.

## Expect a dip

Explorers cost shields that used to buy settlers. Early expansion may slow before
it improves. Measure it, record it, **don't counter-tune hastily** — it likely
resolves once W08 can decide how much exploration is worth.

## Acceptance

- Hut-heavy config: techs from huts by T50 **up ≥3×**
- Explored fraction at T80 **up ≥50%**
- **No explorer disbanded while `explore_want > 0`**
- Hut-free config: explorer want is zero, behaviour unchanged

## Commits

```
ai/deep: add an explorer production want
ai/deep: scale explorer want by hut density and unexplored area
ai/deep: stop disbanding explorers while exploration is wanted
ai/deep: make the hut exploration bonus proportional to expected value
ai/deep: value expected huts in unexplored territory
test: add metric — techs from huts and explored fraction by turn 80
test: assert explorer want is zero when huts are disabled
```
