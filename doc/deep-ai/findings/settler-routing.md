# Part 6 — Settler routing: danger and territory

Companion to Parts 1–5. Same tree: `freeciv/freeciv` @ `0896b34`.

Your call is correct. The interesting part is *where* it's correct: risk
modelling exists, it's just entirely in the wrong layer.

---

## 1. Two layers, and only one of them models risk

### Layer A — choosing where to settle: no risk model at all

`find_best_city_placement()` → `settler_map_iterate()`, `daisettler.c:954`:

```c
pft_fill_unit_parameter(&parameter, nmap, punit);
parameter.omniscience = !has_handicap(pplayer, H_MAP);
cr1 = settler_map_iterate(ait, &parameter, punit, 0);
```

That is the entire parameter setup for the land search. `pft_fill_unit_parameter`
sets `get_MC = normal_move` and `get_zoc`, and nothing else relevant — no
`get_TB`, no `get_EC`, no `is_pos_dangerous`, no `adv_avoid_risks()`.

**Travel cost is computed as though the map were empty and everyone were at
peace.** And that number goes straight into the site score:

```c
turns = move_cost / parameter->move_rate;
cr->result = amortize(cr->total, PERFECTION * turns);
```

So the discount curve from Part 4 §1 — the thing that dominates site selection —
is fed a peacetime estimate. A site eight turns away across a war zone scores
identically to one eight turns away across your own farmland.

Risk enters this layer in exactly two places, both **destination-only**:

- `adv_danger_at(punit, ptile)` in `city_desirability()` (`daisettler.c:736`) —
  a binary veto, scanning only tiles *adjacent* to the destination, and
  signed off by its author as *"As good a quick'n'dirty should be -- Syela"*.
- `daisettler.c:813` — skip destination tiles owned by a player you're at peace
  with. Nothing about the path.

Note that the **overseas** branch does better. `daisettler.c:966`:

```c
parameter.get_TB = no_fights_or_unknown;
```

The ferry search avoids fights. The land search doesn't.

### The asymmetry that gives it away

`autoworkers.c:468`, the auto-worker search — dispatched from the *same*
`settler_run` vtable hook:

```c
parameter.get_TB = autoworker_tile_behavior;
```

which returns `TB_IGNORE` for any tile owned by a non-allied player. **The worker
search is territory-aware. The settler search is not.** Same subsystem, same
entry point. That reads as an oversight rather than a decision.

### Layer B — actually walking there: risk is modelled, with a hole

`dai_gothere()` → `dai_unit_goto()` → `dai_fill_unit_param()`, `daitools.c:456`.
Here there *is* real machinery:

```c
if (!uclass_has_flag(unit_class_get(punit), UCF_UNREACHABLE) && !barbarian) {
  adv_avoid_risks(parameter, risk_cost, punit, NORMAL_STACKING_FEARFULNESS);
}
```

`adv_avoid_risks()` (`advgoto.c:597`) installs `get_EC = prefer_short_stacks`,
which prices each tile as `(unit_value + stack_value) × P(killed) ×
fearfulness`, plus `enemy_zoc_cost = PF_TURN_FACTOR * 20` for tiles where you
could get stuck. That's a sensible soft penalty.

But then, with `LONG_TIME 4` (`daitools.c:456`) and
`long_path = LONG_TIME < map_distance(from, to)`:

```c
if (long_path) {
  /* ... ignore the presence of enemy units when computing the path. */
  parameter->get_zoc = nullptr;
}

if (unit_has_type_flag(punit, UTYF_WORKERS)) {
  parameter->get_TB = no_fights;
} else if (long_path && unit_is_cityfounder(punit)) {
  /* Default tile behavior;
   * move as far along the path to the destination as we can;
   * that is, ignore the presence of enemy units when computing the
   * path. */
} else if (unit_is_cityfounder(punit)) {
  /* Short path */
  parameter->get_TB = no_fights;
}
```

Read that ordering carefully:

| Settler journey | Zone of control | Hard fight avoidance |
|---|---|---|
| **Short** (≤4 tiles) | respected | `no_fights` |
| **Long** (>4 tiles) | **disabled** | **none** |

**The long trips — precisely the dangerous ones — get the least protection.**
Workers get `no_fights` unconditionally; settlers get it only when the trip is
short enough not to matter.

And immediately above, `daitools.c:527`:

```c
/* Should we absolutely forbid ending a turn on a dangerous tile?
 * Do not annoy human players by killing their units for them.
 * For AI units be optimistic; allows attacks across dangerous terrain,
 * and polar settlements.
 * TODO: This is compatible with old code,
 * but probably ought to be more cautious for non military units */
if (!is_ferry && !utype_fuel(unit_type_get(punit))) {
  parameter->get_moves_left_req = nullptr;
}
```

There is a TODO in the source saying non-military units ought to be more
cautious. Settlers are the canonical non-military unit.

### Layer C — the per-step abort, which isn't avoidance

`advgoto.c:280`:

```c
if (punit->moves_left <= mcost
    && unit_move_rate(punit) > mcost
    && adv_danger_at(punit, ptile)
    && !adv_danger_at(punit, unit_tile(punit))) {
  UNIT_LOG(LOG_DEBUG, punit, "ending move early to stay out of trouble");
  return FALSE;
}
```

This only refuses to **end a turn** on a dangerous tile when currently safe. It
doesn't reroute. Next turn the unit resumes the same path — which was planned
without hard danger avoidance — and walks in. It's a stutter, not avoidance.

---

## 2. The pathfinder has a danger feature nobody uses

`common/aicore/path_finding.h:434`:

```c
/* Callback which determines whether the specified position is
 * dangerous. The unit will never end a turn at a dangerous
 * position. */
bool (*is_pos_dangerous) (const struct tile *ptile, enum known_type,
                          const struct pf_parameter *param);
```

Grep the whole tree. It is set to `nullptr` in two places
(`pf_tools.c:691, 923`), forwarded by the amphibious wrapper, and **assigned a
real function by nothing, anywhere, in the entire AI.** A first-class,
documented pathfinding feature that would do exactly what you want, unused since
it was written.

---

## 3. Bodyguards are destination-only too

`dai_gothere()` calls `dai_gothere_bodyguard()` (`daitools.c:160`), so escort
requests do happen. But the danger estimate is:

```c
unit_list_iterate(dest_tile->units, aunit) {
  if (POTENTIALLY_HOSTILE_PLAYER(ait, pplayer, unit_owner(aunit))) {
    danger += adv_unit_att_rating(aunit);
  }
} unit_list_iterate_end;
dcity = tile_city(dest_tile);
...
```

Units **on the destination tile**, plus a city at the destination. Nothing along
the route. A settler crossing a corridor full of hostile cavalry to reach an
empty meadow requests no guard, because the meadow is empty.

---

## 4. What to change

Four items, roughly in value order.

### 4a. Move risk into the search (the important one)

Apply `adv_avoid_risks()` and a tile-behaviour callback to the **search**
parameter in `find_best_city_placement()`, not just to the execution parameter.

```c
pft_fill_unit_parameter(&parameter, nmap, punit);
parameter.omniscience = !has_handicap(pplayer, H_MAP);
parameter.get_TB = settler_tile_behavior;          /* new, see 4b */
adv_avoid_risks(&parameter, &risk_cost, punit, SETTLER_FEARFULNESS);
```

The effect is automatic and exactly what you want: dangerous routes become
*longer* in move cost, `turns` rises, and `amortize(total, PERFECTION * turns)`
discounts the site accordingly. Sites behind a war zone de-rank themselves
without any special-case logic — and unreachable-safely sites drop out entirely
if `get_TB` returns `TB_IGNORE`.

Use a **higher** fearfulness than `NORMAL_STACKING_FEARFULNESS`. A settler is a
pop-cost unit whose loss also wastes every turn already spent walking; it should
be considerably more timid than a musketeer.

This composes cleanly with the contest-pressure work in Part 4 §1c — under high
pressure you flatten the distance discount to reach for contested land, and the
risk EC is what stops that from turning into suicide missions.

### 4b. A settler tile behaviour, mirroring the worker one

```c
static enum tile_behavior settler_tile_behavior(const struct tile *ptile,
                                                enum known_type known,
                                                const struct pf_parameter *param)
{
  const struct player *owner = tile_owner(ptile);

  if (owner != nullptr && !pplayers_allied(owner, param->owner)) {
    return TB_IGNORE;       /* don't route through foreign land */
  }
  if (is_non_allied_unit_tile(ptile, param->owner, FALSE)) {
    return TB_IGNORE;
  }
  return TB_NORMAL;
}
```

Consider `TB_DONT_LEAVE` rather than `TB_IGNORE` for *neutral* (unclaimed)
territory that merely borders a hostile — you don't want to make far-side land
wholly unreachable, just expensive. That's what the EC from 4a is for.

### 4c. Set `is_pos_dangerous` for civilians

The natural implementation already exists — wrap `adv_danger_at()`:

```c
static bool civilian_pos_dangerous(const struct tile *ptile,
                                   enum known_type known,
                                   const struct pf_parameter *param)
{
  return adv_danger_at(param->owner_unit, (struct tile *)ptile);
}
```

The pathfinder then guarantees the settler never *ends a turn* somewhere it can
be killed, at plan time rather than as a per-step stutter. Note `adv_danger_at`
takes a `struct unit *`, so you'll need to stash it in `parameter->data`
alongside the risk cost, or widen `adv_risk_cost`.

### 4d. Fix the long/short inversion

In `dai_fill_unit_param()`, the cityfounder branches are backwards relative to
risk. At minimum, give long-path cityfounders `no_intermediate_fights` (pass
through, don't end on, contested tiles) rather than nothing, and don't null
`get_zoc` for civilians — the ZoC-ignoring rationale in the comment is about
*attackers* pushing toward a target that may clear, which doesn't apply to a
settler that simply must not die.

And extend `dai_gothere_bodyguard()` to sample danger along the planned path,
not only at the endpoint. You already have the path object at that point.

---

## 5. Why this compounds with everything else

This bug is load-bearing for several others:

- **Part 4 §1** — the whole site-selection discount runs on `turns`. Feeding it
  peacetime travel estimates means every conclusion about expansion geometry is
  drawn from wrong numbers.
- **Part 4 §1c** — flattening the distance discount under contest pressure is
  actively dangerous until routing prices risk, because "contested" is exactly
  where the enemy units are.
- **Part 3 §2** — the doctrine probe compares settler value against building
  value. If settler travel cost is systematically underestimated, `ECON_LANDGRAB`
  wins probes it shouldn't.

So I'd sequence 4a and 4b **before** the Part 4 expansion work rather than after.
It's maybe 60 lines and it makes the numbers everything else depends on honest.
