# Part 16 — Co-investment deadlocks (farmland/supermarket)

**Correction to PRD v2 W3.** Baseline: `freeciv/freeciv` @ `0896b34`.

---

## 1. Why W3 doesn't fix the supermarket

W3 says: measure improvement value at a projected future city size and
`amortize()` it. That fixes aqueducts, granaries, harbours and sewers, whose
value is small-but-positive today and large later.

**It does nothing for the supermarket, because the supermarket's value today is
exactly zero — and so is its value at every future time.** Projection multiplies
zero by a horizon and gets zero.

Here is the effect, `data/civ2civ3/effects.ruleset`:

```
type    = "Output_Per_Tile"
value   = 50
reqs    =
    { "type", "name", "range"
      "Extra",    "Farmland",    "Tile"
      "Building", "Supermarket", "City"
    }
```

**Both requirements must hold.** Farmland alone does nothing; Supermarket alone
does nothing.

Now trace both sides of the decision.

**Worker side** — `adv_calc_extra()`, `infracache.c:188`:

```c
struct tile *vtile = tile_virtual_new(ptile);
tile_add_extra(vtile, pextra);
...
goodness = city_tile_value(pcity, vtile, 0, 0);
```

It builds a virtual tile with farmland on it and evaluates output through the
real rules engine — against **the actual `pcity`**, which has no Supermarket. The
effect doesn't fire. The virtual tile yields exactly what the real tile yields.
**Delta: exactly 0.** The worker never builds farmland.

**City side** — `base_want()`, `daicity.c:1530`: add the Supermarket, re-measure
the city, subtract. With no farmland anywhere in the catchment the
`Output_Per_Tile` effect has nothing to apply to. **Delta: exactly 0.** The city
never builds the Supermarket.

A perfect mutual-zero deadlock. Neither side is under-valued — both are
*correctly* valued at zero, given the other doesn't exist.

And your framing was precise: the fix is **pre**-building. The worker has to
start before the building lands, or the building lands on nothing.

## 2. The README pointer is doubly stale

`doc/README.AI`'s bug entry reads:

> *Fix the AI valuation of supermarket. (It currently never builds it). See
> `farmland_food()` and `ai_eval_buildings()` in `advdomestic.c`*

`grep -rn "farmland" ai/ server/advisors/` returns **nothing**. `farmland_food()`
doesn't exist. `advdomestic.c` doesn't exist (it's `ai/default/daidomestic.c`
now). So the AI has *zero* farmland awareness anywhere, and the README's pointer
is to two dead symbols in a dead file.

Add to W23's doc corrections — that's the third stale README entry, after
`locally_zero_minimap` and the `military_amortize` "mystery."

## 3. This is a class, not a special case

The distinguishing feature is **an improvement with zero independent value,
gated on a building with zero value without it.** Contrast:

| Pair | Deadlock? | Why |
|---|---|---|
| Farmland ↔ Supermarket | **Yes** | Farmland does literally nothing alone |
| Roads ↔ Super Highways | No | Roads carry independent trade value, so they get built anyway |
| Ocean tiles ↔ Harbour | No | Ocean tiles exist without investment; Harbour's value is measurable immediately |
| Ocean tiles ↔ Offshore Platforms | No | Same |

So farmland/supermarket may be the only live instance in civ2civ3 — **but any
ruleset can create more**, and hardcoding a farmland special case would violate
PRD §6.1's ruleset-adaptivity principle. Fix the class.

## 4. Design: co-investment bundles

### 4.1 Detect at probe time

This falls straight out of W8.1's ruleset probe. Scan every effect's requirement
vector for one containing **both** a `Building` requirement at `City` range
**and** an `Extra` requirement at `Tile` range. Each such effect is a candidate
bundle.

```c
struct coinvestment {
  const struct impr_type  *building;
  const struct extra_type *extra;
  int      min_tiles;      /* tiles that must carry the extra to pay off */
  adv_want joint_value;    /* per-turn output gain with BOTH present */
  int      building_cost;  /* shields */
  int      extra_turns;    /* worker-turns per tile */
};
```

Mechanical, ruleset-general, computed once. `requirement_vector_iterate` already
gives you everything.

### 4.2 Evaluate jointly

The key move: value the **bundle**, not either half.

```
joint_want = (output_with_BOTH − output_with_NEITHER)
           / (building_cost + min_tiles × extra_turns × worker_shield_equiv)
```

Evaluate `output_with_BOTH` the same way `adv_calc_extra` already does — virtual
tiles — but with the building speculatively present in the city too. All the
machinery exists; nothing new is needed except doing both at once.

If `joint_want` beats competing wants, emit **both** halves together: a building
want on the city *and* a tagged worker want on the best `min_tiles` candidate
tiles.

**Evaluating jointly rather than speculatively is the important part.** The naive
fix — "let the worker assume a supermarket is coming, and let the city assume
farmland is coming" — makes each side optimistically depend on the other, and
both can be wrong simultaneously. A bundle makes it one decision with one owner.

### 4.3 Schedule the overlap

Neither ordering is right on its own:

- Farmland first → workers spend turns on tiles producing nothing, wasted if the
  building slips
- Supermarket first → shields sunk in a building doing nothing until tiles catch up

What a competent human does is **overlap them so they complete together.** Start
worker effort when:

```
building_turns_remaining ≈ (min_tiles × extra_turns) / available_workers
```

That's the "pre-building" you're describing, and it needs the bundle to carry a
*schedule*, not just a value. It also wants a guard: if the building's production
is displaced (emergency, campaign demand), pause the farmland work rather than
finishing tiles that will idle.

### 4.4 The tag matters

The worker want must be **tagged as bundle-driven**, because the ordinary tile
evaluation will keep returning zero for those tiles right up until the building
completes. Without a tag, the normal want computation overwrites it every turn
and the worker wanders off.

This is the same shape as W12's road-plan tiles — a plan-driven want that
survives local evaluation saying "this tile is worthless." Both should use the
extension-point mechanism from Part 15 §3 (`adv_extra_tile_want_cb`), not two
separate hacks.

## 5. Where it goes

Per Part 15's rule — **does this need state only the new module has?**

Bundle *detection and valuation* need only the ruleset and the city: **`ai/default`**,
alongside W3. The `min_tiles` heuristic and joint valuation are ordinary want
computation.

Bundle *scheduling* — deciding when to start worker effort against a production
queue — is empire-level coordination and benefits from doctrine and worker-budget
state, so the scheduler is happier in **`ai/deep`**. But a simple version
("start when the building is ≥60% complete") works fine in `ai/default` and
closes the deadlock on its own.

**Ship the simple version in `ai/default`.** It closes a README bug that has been
open for roughly two decades, and it doesn't need anything from the strategy
layer.

## 6. Commits

Add to W3:
```
ai/default: detect co-investment bundles from ruleset effect requirements
ai/default: value building and tile-extra bundles jointly
ai/default: emit paired building and worker wants for a chosen bundle
advisors: let plan-driven tile wants survive local zero valuation
ai/default: start bundle tile work once the building is mostly complete
test: assert farmland is built ahead of supermarket completion
test: assert supermarket is built in a farmland-viable city
doc: correct the stale supermarket entry in README.AI
```

That last test is the acceptance criterion for a twenty-year-old bug, and the one
before it is the thing W3 alone would have silently failed.
