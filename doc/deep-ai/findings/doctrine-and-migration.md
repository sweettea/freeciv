# Part 2 — Doctrine selection and migration warfare

Companion to `freeciv-better-ai-plan.md`. Same tree: `freeciv/freeciv` @ `0896b34`.

---

## 0. The finding that unifies both requests

You asked for two things. The migration code says they're the same thing.

`check_city_migrations_player()` — `server/cityturn.c:4663`:

```c
/* Score of the actual city, taking into account a persistence factor of 3 */
score_from = city_migration_score(pcity) * 3;
```

and the candidate is distance-discounted (`cityturn.c:4695`):

```c
weight    = (float)(mgr_dist + 1 - dist) / (float)(mgr_dist + 1);
score_tmp = city_migration_score(acity) * weight;
```

with migration occurring only if `score_tmp > score_from`. So to pull a citizen
out of an enemy city, you need

```
your_score  >  3 × their_score × (mgr_dist + 1) / (mgr_dist + 1 - dist)
```

`mgr_dist = sqrt(city_radius_sq) + game.server.mgr_distance`. With a radius-2
city and the default `mgr_distance = 0` (`common/game.h:536`), `mgr_dist = 2`.
At `dist = 2` the weight is 1/3, so you need **9× their score**. Even at
`dist = 1` you need **4.5×**. With `mgr_distance` cranked to its max
(`1 + CITY_MAP_MAX_RADIUS`) and adjacent cities, the floor is still ~3.3×.

**There is no cheap way to poach citizens.** You cannot do it with a swarm of
size-6 towns. Migration warfare is only available to an empire that has built
cities several times better than its neighbours' — which is largepox, executed
well. The doctrine selector and the migration strategy are one subsystem, not two.

Note also `cityturn.c:4641`:

```c
/* No migration out of the capital */
if (is_capital(pcity)) { continue; }
```

Capitals never emigrate. You can drain the ring around an enemy capital but
never the capital itself. And your *own* capital is leak-proof, which means your
magnet city should be sited near the border, not stacked on your palace.

---

## 1. What the AI currently knows about any of this

Essentially nothing.

`grep -rn "migrat" ai/ server/advisors/` returns **six** hits, and four are the
word "immigrate" in settler comments. The only real one is
`ai/default/daieffects.c:687–700`:

```c
case EFT_MIGRATION_PCT:
  /* Consider all foreign cities within set distance */
  iterate_outward(&(wld.map), city_tile(pcity), ..., ptile) {
    ...
    v += amount; /* AI wants migration into its cities! */
  } iterate_outward_end;
```

That's it. It values a migration effect as `amount × (count of nearby foreign
cities)`. It never computes its own migration score, never computes anyone
else's, has no idea whether it is a magnet or a sieve, and cannot tell that
`mgr_worldchance` is set to 0 and the entire mechanic is switched off.

Culture is nearly as bad — `daieffects.c:631–644`:

```c
case EFT_PERFORMANCE: ... v += amount / 10 + 1;   break;
case EFT_HISTORY:     ... v += amount * 5;        break;
case EFT_CULTURE_PCT: ... v += amount * 5 * 5/100; break;
```

Three hardcoded guesses, with a comment admitting the 50-turn horizon is an
assumption. No connection to migration whatsoever — which matters enormously,
for the reason in §3.

---

## 2. Doctrine selection: making largepox a *decision*

The instinct is to retune the settler evaluator toward bigger cities. Resist it.
The correct doctrine genuinely changes — by ruleset, by government, by
tech era, by how much free land is left — and a hardcoded preference is just a
different flavour of the current bug.

### 2a. What actually decides it

Four ruleset-dependent quantities:

1. **Flat-per-city vs percentage-per-citizen effects.** If the building set is
   dominated by flat bonuses and free content citizens
   (`EFT_MAKE_CONTENT`, `EFT_FORCE_CONTENT`), many small cities win — every city
   gets the full flat bonus for the same shield cost. If it's dominated by
   percentage multipliers (`EFT_OUTPUT_BONUS`, `EFT_OUTPUT_BONUS_2`), big cities
   win, because a multiplier on a big base is worth more.
2. **Waste and corruption shape.** `city_waste()` — distance-based corruption
   punishes sprawl at range; a `EFT_OUTPUT_WASTE_BY_REL_DISTANCE` or empire-size
   unhappiness punishes city *count* directly.
3. **Settler population cost.** `unit_pop_value()` — a 2-pop settler makes
   expansion genuinely expensive; a 1-pop settler makes it nearly free.
4. **Growth caps.** How hard the aqueduct/sewer thresholds bite, and how far
   `EFT_SIZE_ADJ` / `EFT_SIZE_UNLIMIT` push them.

### 2b. How to compute it rather than assume it

The AI already has the tool: `create_city_virtual()`, used inside
`cityresult_fill()` at `daisettler.c:298`. You can construct a hypothetical
city, `city_add_improvement()` it, set its size, and measure its real output
through the actual rules engine. Nobody has thought to point this at the
strategy layer.

Run a **doctrine probe** at game start and every ~25 turns thereafter:

```c
struct doctrine_eval {
  adv_want output_per_shield;   /* at horizon T */
  int      shields_invested;
  int      cities;
  int      avg_size;
};

/* Model A — LANDGRAB: cities at citymindist spacing, size S,
 *   "cheap core" build set (defender + granary-tier only)
 * Model B — CONSOLIDATE: cities at 2*radius spacing, size 3S,
 *   full multiplier stack (library/market/bank/temple/aqueduct/sewer)
 *
 * Both normalized by total shields: settlers + buildings + garrison + upkeep.
 */
```

Simulate a fixed land budget — say the tiles you actually control plus a
projection of contested land — and ask which doctrine converts shields into
output faster over a 40-turn horizon. `dai_city_want()` (`daicity.c:1452`)
already turns a city into a weighted output number; reuse it as the objective so
the doctrine probe and the building evaluator agree on what "good" means.

The expected answer, and the reason this is worth doing: **LANDGRAB early,
CONSOLIDATE later.** Free land makes the marginal city nearly free; once land
runs out, the marginal city costs a settler *plus* the waste it adds *plus* the
happiness penalty, while the marginal aqueduct costs nothing but shields. The AI
should find that crossover itself and switch.

### 2c. Making doctrine a set of multipliers, not a code path

This is the maintainability point and it matters. Do **not** write
`if (doctrine == LARGEPOX) { ... }` through the want system. Define:

```c
struct doctrine {
  enum { DOCT_LANDGRAB, DOCT_CONSOLIDATE, DOCT_MAGNET } mode;
  float founder_want_mult;      /* LANDGRAB 1.4, CONSOLIDATE 0.5, MAGNET 0.3 */
  float building_want_mult;     /*          0.7,             1.3,        1.6 */
  float growth_infra_mult;      /* aqueduct/sewer/harbour specifically       */
  float culture_want_mult;
  int   min_city_spacing;       /* beyond game.info.citymindist              */
  int   target_city_count;      /* soft cap; founder_want decays past it     */
};
```

Then apply `founder_want_mult` at `daidomestic.c:558` where the EXPANSIONIST
trait multiplier already lives, and `building_want_mult` at `daidomestic.c:620`
where the BUILDER trait multiplier already lives. Those two lines are the
existing chokepoints — the scaffolding you need is already in place, it's just
currently driven by a random personality roll instead of by a decision.

`min_city_spacing` goes into `city_desirability()` (`daisettler.c:730`), which
already does a `square_iterate` reservation check at `citymindist - 1`; widen
that radius under CONSOLIDATE.

**Hysteresis is mandatory.** Doctrine flapping is worse than either doctrine.
Require the challenger to beat the incumbent by ≥15% for two consecutive probes
before switching, and freeze doctrine while a military campaign is in
`CAMP_FORMING` or `CAMP_MOVING` (Part 1 §3).

---

## 3. Migration warfare

### 3a. Reproduce the score function

Port `city_migration_score()` (`cityturn.c:4088–4163`) into the AI as a pure
estimator, `dai_migration_score(pcity)`. It's ~40 lines of arithmetic with no
hidden state. Then read the terms carefully, because **almost all of them
saturate and exactly one doesn't**:

| Term | Source line | Behaviour |
|---|---|---|
| `size + happy − 0.25·unhappy − 0.50·angry` | 4103 | linear base |
| `× (1 + (1−e^(−shieldcost/1000))/5)` | 4119 | saturates at **×1.2** |
| `× (1 + (1−e^(−trade/100))/5)` | 4121 | saturates at **×1.2** |
| `× (1 + (1−e^(−luxury/100))/5)` | 4124 | saturates at **×1.2** |
| `× (1 + (1−e^(−science/100))/5)` | 4127 | saturates at **×1.2** |
| `+ culture × culture_migration_pml/1000` | **4131** | **additive, unbounded** |
| `× (1 + clip(−10,food,20)/10)` | 4135 | 0.9 – 1.2 |
| `× illness factor` | 4140 | 0.6 – 1.0 |
| `× 1.25` wonder / `× 1.25` capital | 4145/4150 | one-shot |
| `× (1 + EFT_MIGRATION_PCT/100)` | 4154 | ruleset lever |

The four exponential factors together cap at ≈**2.07×**, and you reach ~90% of
each with quite modest surpluses. Wonder and capital are one-time 25% bumps.
Everything multiplicative is bounded.

**Culture is the only term that grows without limit.** It's added *after* the
multipliers and scales with accumulated `city_culture()`, i.e. history, which
compounds every turn forever.

So: if `game.info.culture_migration_pml > 0` in the ruleset, **migration
dominance is a culture race**, and the AI's current culture valuation is three
hardcoded WAGs (`v += amount * 5`) that don't know migration exists. Rewiring
`EFT_HISTORY` / `EFT_PERFORMANCE` / `EFT_CULTURE_PCT` in `daieffects.c:631–644`
to feed a migration objective is probably the single highest-value change in
this document, and it's maybe 30 lines.

If `culture_migration_pml == 0`, migration is a bounded game and the ceiling on
your advantage is roughly `2.07 × 1.2 × 1.25 × 1.25 ≈ 3.9×` over a baseline
city — right at the 3.3–4.5× threshold from §0. That's worth knowing, because it
means: **in a zero-culture-migration ruleset, poaching is barely achievable at
all and the AI should not chase it.** Have the probe check this and disable the
whole doctrine rather than wasting shields.

### 3b. Three gates that silently eat migrations

`do_city_migration()` (`cityturn.c:4172`) will refuse a migration the selector
already approved. The AI must pre-satisfy all three or it will build a magnet
that doesn't magnetise:

1. **Growth cap** — `if (!city_can_grow_to(pcity_to, size+1))` (`cityturn.c:4251`).
   No aqueduct → no immigration. The magnet needs growth infrastructure built
   *ahead of need*, which is precisely what the zero-horizon `base_want()` bug
   from Part 1 §1b prevents. Fix that first; this depends on it.
2. **Food** — if `game.server.mgr_foodneeded`, the receiver needs
   `surplus[O_FOOD] >= game.info.food_cost`, *or* a free workable tile that
   would get it there (`cityturn.c:4199–4222`). Note the consequence: a magnet
   should deliberately **keep spare workable food tiles unworked**. That's
   counterintuitive and no amount of generic output-maximising will discover it.
3. **Settings** — `mgr_worldchance` (default **10**, `game.h:544`),
   `mgr_nationchance` (default 50), `mgr_turninterval` (default 5),
   `mgr_distance` (default **0**). Read them in `phase_begin`. At default
   settings international migration fires on a 10% roll every 5 turns at
   distance ≤ 2. That is a slow drip, not a win condition — the AI should scale
   its investment to the settings, not assume.

### 3c. The nationality backflow trap

`cityturn.c:4718`:

```c
if (game.info.citizen_nationality) {
  if (citizens_nation_get(pcity, city_owner(acity)->slot) > 0) {
    score_tmp *= 2;
  }
}
```

Citizens pull **twice as hard** toward a city owned by their own nation. This
cuts both ways and the AI needs to model both:

- **Trap:** you capture an enemy city. Its citizens keep enemy nationality. They
  now flow *back* to the enemy at 2× attractiveness. Conquest without migration
  dominance leaks your gains away. The AI should either garrison-and-assimilate
  (build culture/happiness there fast) or accept the city as a temporary asset.
- **Weapon:** cities the enemy took from *you* are full of your nationals,
  pulling toward your cities at 2×. A captured-and-lost city is a migration
  faucet pointed at you. Target the ring around it with your magnet.

`citizens_nation_get()` is already available; the AI just never calls it.

### 3d. Internal migration as a defensive shield

`check_city_migrations_player()` checks **internal** targets first, and on
success does `continue` — skipping the international check entirely
(`cityturn.c:4749–4756`). So:

> A citizen with an attractive domestic destination will never be poached.

This is a real, cheap, non-obvious defence. Building one strong internal magnet
per frontier cluster inoculates every city around it against enemy poaching,
because they'll migrate inward before they'll ever be offered abroad. Cost: some
consolidation you probably wanted under CONSOLIDATE anyway.

### 3e. What to actually build

```c
struct migration_intel {
  float  our_score[MAX_CITIES];        /* dai_migration_score, ours          */
  float  their_score;                  /* best hostile score within mgr_dist */
  int    poach_targets;                /* enemy non-capitals we can out-score*/
  int    leak_risk;                    /* our cities losing to a foreign score*/
  float  deficit;                      /* score gap to close on the frontier */
  bool   mechanic_active;              /* mgr_worldchance > 0 etc.           */
  bool   culture_is_lever;             /* culture_migration_pml > 0          */
};
```

Each turn, for every frontier city: compute our score, compute every hostile
score within `mgr_dist`, and derive the gap needed to flip the inequality in §0.
Turn that gap into wants:

- If the binding constraint is **culture** → want history/performance buildings
  and wonders; raise `culture_want_mult`.
- If it's **happiness** → temples/colosseums/luxury rate; note the base term
  weights `happy` at 1.0 and `angry` at −0.5, so removing angry citizens is
  worth more per head than adding happy ones.
- If it's **growth cap** → aqueduct/sewer, urgently (and see §3b.1).
- If it's **the 1.25 bumps** → a wonder in the magnet city. The AI already has
  wonder-city selection (`advbuilding.c:building_advisor`); add "is this a
  migration frontier?" to the candidate scoring, which currently only looks at
  shield surplus and `downtown`.
- If `leak_risk` is high → build the internal magnet from §3d instead of
  contesting outward.

And feed `deficit` back into production the same way Part 1 §3 feeds
`required_attack - committed_attack`: a concrete, empire-level shield demand
rather than a per-city local guess.

---

## 4. Where this slots into the phasing

Revised from Part 1 §5:

- **Phase 1** (site scoring + horizon-aware `base_want`) is now a hard
  prerequisite, not just the biggest win. Migration is gated on aqueducts being
  built before they're needed (§3b.1), and doctrine switching is meaningless if
  settler want and building want aren't on a comparable scale.
- **Phase 1.5 — doctrine probe** (§2). Small; mostly a new function plus two
  multipliers at existing chokepoints (`daidomestic.c:558` and `:620`).
- **Phase 2–4** unchanged (roads, campaigns, ferries).
- **Phase 4.5 — migration intel** (§3). Depends on Phase 1 and 1.5. The
  `daieffects.c` culture rewiring can land independently and early.

**Measurement.** Add to the batch-run metrics: citizens gained/lost to migration
per 50 turns, and doctrine-switch turn number. If the AI switches doctrine at
wildly different turns across fixed-seed reruns, your hysteresis is too loose.
If it never switches, your probe horizon is too short.

---

## 5. One honest caveat

At Freeciv's default settings — `mgr_worldchance 10`, `mgr_distance 0`,
`mgr_turninterval 5` — international migration is a trickle: a 10% roll, every
5 turns, only between cities within about two tiles of each other, and only when
you've cleared a 3.3–4.5× score bar. It is a real mechanic and worth modelling,
but it will not carry a game on default settings.

Where it becomes genuinely decisive is on migration-heavy configurations
(`mgr_worldchance` 50–100, `mgr_distance` 2–3) and in rulesets with a nonzero
`culture_migration_pml`, where the unbounded culture term lets a mature empire
run away with it. So: build the intel module to *detect* whether it's playing a
migration game rather than assuming it is. `mechanic_active` and
`culture_is_lever` in §3e exist for exactly that, and an AI that correctly
declines to chase migration on a default server is doing the right thing.
