# Part 15 — Where each change actually belongs

**Correction to PRD v2 §3.1 and Part VII.** Baseline: `freeciv/freeciv` @ `0896b34`.

---

## 1. The error

PRD §3.1 says "build a module, don't fork," then Part VII contains commits like:

```
ai/deep: copy city site evaluation into the module ahead of changes
ai/deep: copy improvement want calculation into the module
ai/deep: copy kill desire into the module ahead of changes
```

That *is* forking, one function at a time. Worse, it isn't even possible cheaply.

`ai/default/daisettler.h` exports six symbols:

```c
void dai_auto_settler_init/free(struct ai_plr *ai);
void dai_auto_settler_reset/run/cont(struct ai_type *, ..., struct unit *, struct workermap *);
void contemplate_new_city(struct ai_type *ait, struct city *pcity);
```

`cityresult_fill()`, `city_desirability()`, `settler_map_iterate()`,
`find_best_city_placement()`, and the `tdc_plr_*` tile cache are all **`static`**.
`daicity.h` is the same story — `base_want()` is private, only `dai_city_want()`
is exported.

**So changing how a city site is scored means reimplementing `dai_auto_settler_run`
and everything under it** — settler dispatch, worker dispatch, the tile cache, the
citymap reservation, the pathfinder setup. About 1,400 lines, to change forty.

## 2. What the vtable granularity is telling us

The hooks are `phase_begin`, `first_activities`, `choose_building`,
`settler_run`, `unit_turn_end`. Those are **empire-and-turn-level** seams, not
decision-level ones.

That's not an accident, and it's the answer to where the split goes:

> **`ai/default` should be a *correct tactical engine*.
> `ai/deep` should be the *empire-level strategy layer* on top of it.**

Which means most of what the PRD assigns to `ai/deep` is misfiled.

## 3. The rule

**Does this change require state that only the new module has?**

- **No** → fix it in `ai/default`, or lift the machinery into `server/advisors/`
  or `common/`. Everyone benefits, including `ai/deep`, which inherits it for
  free instead of copying it.
- **Yes** → it belongs in `ai/deep`.

Apply that and roughly **45% of the PRD relocates.**

### Tier 1 — Fix in `ai/default`

Needs no new state. Strictly better. `ai/deep` inherits it.

| Workstream | What moves |
|---|---|
| Appendix A | All 20 bugs — inverted defence bonus, shore-defence pond, killstack, danger aggregation, dead nag ladder, missing retraction, unwired `player_save`, doc fixes |
| **W2** | Full-catchment scoring, specials term, cluster bonus, potential term, retiring `GROWTH_POTENTIAL_DEEMPHASIS` |
| **W3** | Horizon-aware `base_want`, capacity-gate credit |
| **W5** | Settler routing risk, `is_pos_dangerous`, the long/short inversion, path-sampled bodyguards |
| **W6** | Explorer want, deleting the disband hack, proportional `HUT_SCORE` |
| **W10** | Combat model — killstack gating, compounding survival, superlinear danger |
| **W11** | En-route defender accounting, emergency arbitration |
| **W15** | Coastal-access requirement, border defensibility, continent-threat magnitude, overseas safety term, allied danger assessment, allied bodyguards, tech gifting to non-team allies |
| **W16** | Call-to-arms retraction, the alliance guard, tiered asks, cohesion gradient |
| **W23** | README corrections |

Nobody thinks scoring a city site on two tiles is correct, or that mountains
should score below grassland for defensibility. These aren't strategy choices —
they're defects, and they should be fixed where the defect is.

### Tier 2 — Lift into `server/advisors/` or `common/`

Shared machinery, plus the narrow extension points strategy needs.

| From | What |
|---|---|
| **W1** | `want_t`, output→shield conversions, `adv_choice` priority tier |
| **W12** | MST and articulation-point analysis → `common/aicore/` (pure graph algorithms) |
| **W10** | Extract the superlinear force aggregation used by `assess_defense_quadratic` |
| **W26** | Planning purity — government as parameter, per-player citymap, per-player RNG |

Plus — and this is the piece that replaces most of the forking:

**Narrow extension points, so strategy can steer tactics without reimplementing
them.**

```c
/* server/advisors/autoworkers.h — worker search consults a plan if provided */
typedef adv_want (*adv_extra_tile_want_cb)(const struct tile *, void *ctx);
void adv_workers_set_plan_cb(adv_extra_tile_want_cb cb, void *ctx);
```

That single hook lets `ai/deep`'s road planner inject plan tiles into the
existing worker search — instead of reimplementing `auto_settler_findwork`.
Same pattern for the others:

- a `campaign_hint` field on `unit_ai` so strategy can suppress rampage and pin
  a unit to a plan, without owning unit movement
- `adv_choice` priority tiers so strategy can inject empire-level production
  demand, without owning `choose_building`
- a settler-site-score callback, so contest pressure can adjust site value
  without owning `cityresult_fill`

Each is a handful of lines in shared code and removes hundreds of lines of
duplication.

### Tier 3 — Genuinely new, stays in `ai/deep`

Requires state that doesn't exist today.

| Workstream | New state |
|---|---|
| **W4** | `known_city` / `known_force` — the knowledge layer |
| **W7** | Contest pressure (depends on W4 + W8) |
| **W8** | `struct doctrine`, `ruleset_probe` |
| **W9** | CM parameters derived from doctrine |
| **W12** | `road_plan`, `chokepoint_map` — the *plans*, not the algorithms |
| **W13** | `struct campaign` |
| **W14** | War ledger (arguably Tier 1 — `ai_dip_intel` already exists and persists) |
| **W17** | `migration_intel` |
| **W19** | `coalition`, `coalition_ledger` |
| **W24** | Conversational goal negotiation |

`ai/deep` becomes **much smaller** — a strategy layer over a tactical engine
that is now correct, rather than a parallel AI that reimplements everything.

## 4. The counter-argument, taken seriously

There's a real reason for a separate module that isn't code hygiene: **changing
`ai/default` changes the game for everyone.**

- Stable branches and longturn communities have calibrated expectations
- CivRealm and freeciv-bot benchmark *against* the current AI — a step change in
  strength invalidates published baselines
- The maintainer (87% of commits) has to own the support burden

So the split isn't "bugs vs features," it's **risk of behavioural surprise**:

| Change class | Where | How |
|---|---|---|
| Bug fix — no reasonable person defends current behaviour | `ai/default` | Immediately, standalone PRs |
| Shared machinery / extension points | `advisors`, `common` | Immediately, no behaviour change |
| Behaviour-changing tactical improvement (W2, W3, W5, W6) | `ai/default` | **Developed and validated in `ai/deep` first, then migrated** as a separate, separately-reviewed event per subsystem |
| Empire-level strategy | `ai/deep` | Stays; may become the default at a major version |

That middle row is the graduation path. `ai/deep` is where a tactical
improvement *proves itself* against `classic` in paired batches — and once the
numbers are in, it moves down into `ai/default` with the evidence attached.
That's a much easier review conversation than "here is a new AI."

## 5. Revised delivery order

This actually **improves** the roadmap, because Tier 1 is front-loadable and
individually shippable:

**Phase −1 — Credibility (new, first).** The ~20 Appendix A bug fixes as small
standalone PRs to `ai/default`. Several are under five lines. They fix
twenty-year-old README entries. They need no PRD buy-in, no architecture
discussion, and no new module — and given the bus-factor finding, they're how
you earn the review bandwidth for everything after.

**Phase 0 — Shared foundation.** W1 want unification and W26 planning purity, in
`advisors`/`common`. No behaviour change; pure enablement.

**Phase 0.5 — Extension points.** The four hooks in §3 Tier 2. Small, no
behaviour change, and they make Tier 3 possible without forking.

**Phase 1 — Tactical correctness in `ai/default`.** W2, W3, W5, W6, W10, W11.
Each validated in paired batches. This is where the AI stops being embarrassing
and it benefits every existing player.

**Phase 2+ — `ai/deep` as strategy layer.** W4, W7, W8, W9, W12, W13, W17, W19 —
now a genuinely new module rather than a fork, consuming the extension points.

## 6. What this changes in the numbers

Roughly the same total work, redistributed — but the risk profile is much better:

- **~45% lands in `ai/default`** and helps everyone immediately, including the
  research ecosystem benchmarking against it
- **`ai/deep` shrinks to the strategy layer**, maybe 45% of the original scope,
  which makes it a far more plausible thing for one person to finish
- **The minimum viable slice changes.** It was W0+W1+W2+W3 in a new module.
  It's now: Appendix A fixes → W1 → W2 → W3 **in `ai/default`**, with no new
  module at all. Same 10–14 weeks, but every week of it ships to actual players
  rather than sitting behind an `aitype` nobody selects.

That last point is the real argument. A better default AI that nobody has to opt
into is worth considerably more than an excellent optional one.
