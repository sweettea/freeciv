# W02b — Catchment as a growth stream

**Phase 2, `ai/deep`. Supersedes the catchment half of W02.**
Root cause: RC1 (no time horizon) and RC4 (incommensurable wants), both in
one place. Design conversation 2026-08-02; evidence in
`findings/scaling-profile.md` §-none, `findings/europe-T80.md`, and the
measurements recorded in STATUS for 2026-08-02.

## The problem in one look

`deep_catchment_average()` (`deepsite.c:130`) returns `vsum / wsum` — a
**weighted average** of catchment tiles. Overlapped tiles are down-weighted in
the numerator *and* the denominator, so as `site_overlap_weight → 0` they do
not count against the site, they **drop out of the calculation**. A site with
20 free tiles and one with 2 free tiles of equal quality score identically.

The site score measures tile **quality** and is structurally blind to tile
**quantity**. Measured consequence — `largepox-200`, 380 land/player, 5 seeds:

    site_overlap_weight   0.05    0.01    0.002
    cities/player         23.1    23.2     22.9
    tiles/city            16.4    16.4     16.6

A 25-fold change in the parameter moves spacing by 0.2 tiles. **Spacing is not
tunable**, and no weight added to an average will make it so.

Two further defects sit on top:

- Nothing in the site score grows with **city count**, so nothing ever decides
  to stop expanding.
- `founder_want` and building want are on **different scales with no exchange
  rate**, which is the `europe-T80` arbitration failure: a city wanting a
  settler at 514 with an empty build queue builds a Colosseum anyway. Settler
  count sat at 26 across four arms and two tunings whose site parameters differ
  by 7x — a quantity invariant to every site-scoring knob is not being set by
  site scoring.

## Design

Value a city the way W03 already values a building: **discounted citizen-turns
of tile output**. `deep_capacity_credit()` is
`amortize(per_citizen * steps * weight, ttg)`; a city is the same object — a
stream of citizens arriving over time, each earning tile output.

    V(site) = amortize(center_yield, 0)                  /* free from turn 0 */
            + Σ_k amortize(yield(tile_k), ttg(k))        /* citizen k earns  */
            − settler_cost                               /* contextual, below */
            − Σ_cities amortize(entertainer_loss, 0)     /* empire size, if any */

- **`tile_k`** — the k-th best tile *this* city will actually work. A tile a
  neighbouring own city will genuinely reach never enters this city's stream, so
  overlap needs no weight and shared-by-N falls out for free. Today
  `near_own` is a boolean with an early `break` (`deepsite.c:358`), so a tile
  contested by three own cities is recorded like one shared with one.
- **`yield`** — current yield plus improvement potential, scaled by **worker
  throughput**: idle worker-turns available over the horizon against outstanding
  improvement work. One scalar per player per turn. It still ranks sites,
  because sites differ in how much of their value *is* potential.
- **`ttg(k)`** — simulated food accumulation using `city_granary_size()` for the
  foodbox and `city_can_grow_to()` for the caps. Granary retention compounds
  into every later `ttg(k)`, not just the first.
- **`K`** — falls out of the discount. No target-size cutoff constant.

`amortize()` is a discount rate (MORT=24), **not a deadline**. It must stay that
way: a horizon that shortened toward `endturn` would be fitting the benchmark,
since our cells stop at 80/120/200 turns and the AI must not know that.

### Settler cost is contextual

`server/cityturn.c:932` — a city that cannot grow does not merely stop banking
food, `aqueductloss` **burns down the stock it already holds** every turn:

```c
if (!city_can_grow_to(pcity, size + 1)) {
    new_food = granary * (100*100 - aqueductloss * (100 - savings_pct)) / (100*100);
    pcity->food_stock = MIN(pcity->food_stock, new_food);
```

So from a **blocked** city the settler's population is nearly free — the food
that regrows it was being destroyed. From an unblocked city it costs a genuine
`pop_cost` citizens plus the growth displaced. Same unit, same ruleset, marginal
cost differing by an order of magnitude.

`deep_capacity_credit()` already fires exactly on `!city_can_grow_to(size+1)`
and spends it on the cap-lifter. A blocked city has **two** exits — lift the cap,
or export the population — and only the first is credited. When the cap-lifter is
unavailable (no tech, or too distant), the settler is the *only* exit.

Generalise to **growth blocked by any mechanism**: hard cap, happiness soft cap,
or no food surplus. The soft cap is the one binding in our cells — deep's cities
average 5–6 against civ2civ3's size-8 gate, while it buys 33 temples per 100
cities.

### The mirror: Join City

- blocked city + good site → **export** pop as a settler, cost ≈ 0
- unblocked city + no site → **import** pop via Join City, gain = `pop_cost`

Population arbitrage in one currency, opposite signs. Neither exists
(`upstream-bugs.md` #31; the hook is already named at `deepsite.c:610`,
*"consider immigration"*, returning NULL). Implement as one term.

### Ruleset dependence — do not bake constants

The largepox/smallpox optimum is a **ruleset property**, not a preference:
illness scales with size, empire-size unhappiness with count, cap-lifters bound
size, and the radius may itself grow with size. All four are ruleset data, and
they differ sharply between civ2civ3 and classic. Read them:

| Term | Source, not a constant |
|---|---|
| settler pop / shields / founding size | `pop_cost`, `city_size` (default 1), Found City enablers |
| catchment shape | `city_radius_sq`, **which may vary with size** |
| tile yield under government | `Output_Inc_Tile`, `Output_Penalty_Tile` |
| growth | `food_cost`, `foodbox`, `city_granary_size()`, `Growth_Food` |
| size caps | `Size_Adj`, `Size_Unlimit` |
| empire cost | `Empire_Size_Base`/`Step`, `Make_Content`, `Force_Content` |
| corruption | `Output_Waste`, `Output_Waste_By_Distance`, distance to capital |
| risk | `illness_on`/`illness_base_factor`, migration settings |

**Empire-size unhappiness costs output, not score.** `get_civ_score()` uses
`total_player_citizens()`, which sums happy + content + **unhappy** + angry +
specialists — an unhappy citizen scores exactly like a happy one. The cost is a
citizen becoming an entertainer instead of working a tile, and only in cities
without happiness slack. Deep, at 33 temples and 31 colosseums per 100 cities
against baseline's 11 and 11, has more slack than anyone: **going past empire
size is usually right, and for deep especially.** Compute the term; do not
assume it.

## What this deletes

`site_tail_weight`, `site_overlap_weight`, `site_overlap_crowding`,
`site_potential_discount`, the target-size cutoff, and
`deep_catchment_average()` itself — six tuned constants replaced by a simulation
whose only free parameters are the ones already governing buildings.

## Acceptance

- **Spacing responds to land.** cities/player tracks land/player across
  `largepox-200` (380), `primary-120` (400) and `lategame-80` (80). The AI
  choosing different densities for different maps is the deliverable; nothing we
  have today can do it at all.
- **Settlers respond to blocked growth.** Settler production correlates with
  the count of growth-blocked cities. Today it sits at ~26 regardless.
- **Arbitration becomes arithmetic.** A city with healthy `founder_want` and an
  empty queue builds the settler (`europe-T80`).
- **Ruleset portability.** `primary-120-classic` improves alongside the
  civ2civ3 cells, not after a separate tuning pass.
- Score is **not** the primary criterion here. Four cells this session moved
  mechanism decisively with flat score; state the mechanism metric first.

## Commits

```
ai/deep: count how many own cities share a catchment tile, not whether any do
ai/deep: simulate city growth to a horizon instead of a target-size cutoff
ai/deep: value a site as discounted citizen-turns, replacing the catchment average
ai/deep: exclude tiles a neighbouring city will actually work
ai/deep: scale improvement potential by available worker throughput
ai/deep: credit settler production from growth-blocked cities
ai/deep: join a city when no site beats the population it would return
ai/deep: price the empire-size step as output lost, read from effects
test: assert site value responds to catchment size at equal tile quality
test: assert spacing differs between largepox-200 and lategame-80
```

First epic is the middle three: the stream, `ttg(k)`, and the counterfactual
overlap. They are the pair that finally makes spacing expressible, and they are
testable in one session with the sweep harness already built.

## Dependencies

W03 (horizon credits) — landed, and supplies the currency. W01 (want
unification) would give food/shield/trade a principled exchange rate; until then
this is still weighing incommensurables, in a better-shaped frame. W26/W27 if
the per-site growth simulation proves too slow — site evaluation is already in
the quadratic family.
