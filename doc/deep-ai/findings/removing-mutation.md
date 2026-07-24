# Part 19 — Removing shared-state mutation

**Makes W26 concrete.** Baseline: `freeciv/freeciv` @ `0896b34`.

Short answer: **yes, and it's about a week and a half.** Ten sites, five
patterns, and three of the five have existing infrastructure.

---

## 1. The complete inventory

```
grep -rn "tile_set_owner\|city_add_improvement\|city_remove_improvement\|->government *=\|game\.info\.[a-z_]* *=\|->wonders\[" \
     ai/ server/advisors/ common/aicore/ --include=*.c | grep -v virtual
```

| # | Site | Mutates | Blast radius |
|---|---|---|---|
| 1 | `daisettler.c:292, 455` | `pplayer->government` | player |
| 2 | `daisettler.c:301, 458` | `tile_set_owner()` | **map-global** |
| 3 | `daicity.c:1556, 1566` | city improvements | city |
| 4 | `daicity.c:1553, 1573` | `pplayer->wonders[]` | player |
| 5 | `daicity.c` (same block) | `game.info.great_wonder_owners[]` | **game-global** |
| 6 | `daitech.c:243` | `game.info.global_advance_count` | **game-global** |
| 7 | `daidata.c:546` | `pplayer->government` | player |
| 8 | `advdata.c:1030, 1067` | `pplayer->government` | player |
| 9 | `daidiplomacy.c:1410–1421` | `pplayer->government` | player |
| 10 | `citymap.c:53` | `static int *citymap` | **process-global** |

**Ten sites.** That's the whole problem. (Three other `grep` hits —
`daieffects.c:803`, `daidiplomacy.c:577`, `daisettler.c:431` — are `==`
comparisons, not assignments.)

They collapse into five patterns:

| Pattern | Sites | Question being asked |
|---|---:|---|
| **Government swap** | 1, 7, 8, 9 | "what if we had government X?" |
| **Improvement swap** | 3, 4, 5 | "what if this city had building X?" |
| **Tile ownership swap** | 2 | "what if we owned this tile?" |
| **Global counters** | 5, 6 | "what if we had this tech/wonder?" |
| **Static scratch** | 10 | (not counterfactual — just a shared buffer) |

---

## 2. Government swap — easier than expected

Four of the ten sites, and there is **a single chokepoint**.
`common/requirements.c`:

```c
is_gov_req_active(const struct civ_map *nmap,
                  const struct req_context *context, ...)
{
  if (context->player == nullptr) {
    return TRI_MAYBE;
  } else {
    return BOOL_TO_TRISTATE(government_of_player(context->player)
                            == req->source.value.govern);
  }
}
```

One function, one read. And `struct req_context` — the evaluation context that
already threads through requirement checking — is right there:

```c
struct req_context {
  const struct player *player;
  const struct city   *city;
  const struct tile   *tile;
  /* local-range: unit, unittype, building, extra, output, ... */
};
```

**Add one optional field:**

```c
  const struct government *government;   /* override; null → player's actual */
```

and one line in the evaluator:

```c
const struct government *gov = context->government
                             ? context->government
                             : government_of_player(context->player);
```

Then the AI passes the *goal* government in the context instead of assigning it
to the player. One other direct read exists — `common/city.c:2319`, the
revolution check — and needs the same treatment.

**~20 lines, plus threading the context through the AI's evaluation calls.**

## 3. Improvement swap — the only medium one

`base_want()` is the awkward case, because it needs the *real* city's worked
tiles, population and existing improvements, then asks "and one more building."
`create_city_virtual()` builds a *fresh* city, not a copy of an existing one.

Two options:

**(a) `city_virtual_copy()`** — duplicate the city struct including `built[]`
and worked tiles, mutate the copy. This is the established pattern —
`tile_virtual_new()` / `tile_virtual_destroy()` in `infracache.c` already does
exactly this for tiles, and `adv_calc_extra` uses it correctly. Extending it to
cities is the natural move; the work is handling the pointer members (unit
lists, worklist) safely.

**(b) An overlay in `req_context`** — a "pretend this city also has building X"
field. Smaller, but it's a set-difference overlay rather than a single pointer,
and it has to be honoured everywhere improvement presence is read.

**(a) is cleaner and reusable.** It also lets you delete the
`game.info.great_wonder_owners[]` and `pplayer->wonders[]` writes, because both
exist only to make the virtual improvement look real — on a copy, you set them
on the copy.

Call it 2–3 days including tests.

## 4. The remaining three — trivial

**Tile ownership** (site 2): the write exists so `city_waste()` computes waste
against the right owner. Give `city_waste()` an owner parameter. Contained.

**Global counters** (sites 5, 6): `global_advance_count` and
`great_wonder_owners[]` are written to make a hypothetical look real. Both become
parameters or context fields, and site 5 disappears entirely with §3(a).

**Static citymap** (site 10): move `static int *citymap` into `struct ai_plr`.
Purely mechanical.

---

## 5. Verifying it stays fixed

The important deliverable isn't the cleanup — it's making purity a *maintained*
property. Two mechanisms, and I'd take the second:

**Flag-and-assert.** A global "in planning phase" flag, asserted in each mutating
setter. Catches the sites you know about, misses the ones you don't.

**State checksum.** In debug builds, hash the map, players, and cities before and
after the AI planning phase and assert equality:

```c
#ifdef FREECIV_DEBUG
  uint64_t before = game_state_checksum();
  run_ai_planning_phase();
  fc_assert(game_state_checksum() == before);
#endif
```

That catches **unknown unknowns** — including any of the ten I've miscounted, and
any new one someone adds in two years. It's also cheap to write and it's the
thing that makes the property survive the project.

---

## 6. Effort, honestly

| Task | Estimate |
|---|---|
| Government override in `req_context` + evaluator + revolution check | 1 day |
| Thread `req_context` through AI evaluation call sites | 2 days |
| `city_virtual_copy()` + convert `base_want` | 2–3 days |
| Tile ownership → `city_waste()` parameter | ½ day |
| Global counters → parameters | ½ day |
| Per-player `citymap` | 2 hours |
| Per-player RNG streams | ½ day |
| State-checksum assertion + tests | 1 day |
| **Total** | **~1.5 weeks** |

For which you get, in order of how soon it pays:

1. **Correctness.** `base_want` currently writes *global game state visible to
   every player* to price one building. That's a latent bug even
   single-threaded — anything that reads `great_wonder_owners[]` mid-evaluation
   sees a lie.
2. **Cacheability.** Pure evaluation is memoizable. Given Part 17 §2.2's
   `O(cities² × wonders)`, memoization is probably a larger win than threading
   and it lands first.
3. **Threadability.** Per-city planning jobs become embarrassingly parallel.

---

## 7. Where it goes

Per Part 15's rule, all of this is `common/` and `ai/default/` — **no new module
needed, and everyone benefits.** It's also unusually reviewable: each of the five
patterns is a separate PR, none changes behaviour, and the checksum assertion
proves that.

Which makes it good early work. It sits naturally right after the Appendix A bug
fixes in Phase −1: no design debate, no behavioural risk, immediately useful, and
it unblocks both the scaling fixes and any threading you later decide you want.

## 8. Commits

```
common: add an optional government override to the requirement context
common: honour the government override in the government requirement evaluator
common: honour the government override in the revolution check
ai/default: pass the goal government by context instead of assigning to the player
advisors: pass the goal government by context instead of assigning to the player
common: add city_virtual_copy mirroring tile_virtual_new
ai/default: evaluate improvement want against a city copy
ai/default: drop the wonder-ownership writes now made unnecessary
common: give city_waste an explicit owner parameter
ai/default: pass virtual tile ownership to waste instead of setting tile owner
ai/default: pass hypothetical advance count instead of writing game state
common: move the citymap reservation array into per-player AI state
common: give each player an independent random number stream
common: add a game-state checksum for debug assertions
test: assert AI planning leaves game state unchanged
```

Fifteen commits, none behaviour-changing, all independently reviewable. That last
test is the one that matters — it's what stops the idiom coming back.
