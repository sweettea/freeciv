# Workers do nothing when nothing is demanded (2026-08-02)

Measured on `lategame-80`, 10 train seeds, deep arm, final-save census.
Prompted by a human question while looking at repurposed settlers: *do
workers do anything if it isn't demanded and they'd otherwise be idle?*

**No. They stand still, permanently, and for an AI player there is no
fallback behaviour at all.** And it is not a settler problem — it is
most of the terrain economy.

---

## 1. The census

Every worker-capable unit alive at game end, by activity:

    worker   IDLE           341   34.1/game   71% of its kind
    worker   FORTIFYING      45    4.5/game    9%
    worker   IRRIGATE        42    4.2/game    9%
    worker   MINE            30    3.0/game    6%
    worker   BASE            21    2.1/game    4%
    worker   TRANSFORM        2    0.2/game    0%

    founder  IDLE           134   13.4/game   82% of its kind
    founder  FORTIFYING      10    1.0/game    6%
    founder  IRRIGATE         9    0.9/game    5%
    founder  BASE             6    0.6/game    4%
    founder  MINE             5    0.5/game    3%

    founders 164 (16.4/game), dedicated workers 481 (48.1/game)

**475 of 645 worker-capable units — 74% — are idle.**

A save is written at turn end, so "IDLE" could in principle mean "in
transit to work, goto already converted to IDLE". It does not:

    worker   no goto destination at all      341  34.1/game  100%
    founder  no goto destination at all      107  10.7/game   80%
    founder  goto set, still travelling       26   2.6/game   19%

**Every single idle dedicated worker has no destination.** They are
parked, not walking. Only 19% of idle founders are in transit.

So ~45 units per game — roughly 34 workers and 11 founders — are
standing still at the final whistle with nothing assigned.

## 2. Why: demand is only ever measured inside city work radii

`worker_evaluate_improvements()` (`server/advisors/autoworkers.c:542`):

    city_list_iterate(pplayer->cities, pcity) {
      city_tile_iterate_index(nmap, city_map_radius_sq_get(pcity),
                              pcenter, ptile, cindex) {
        if (!in_use && !city_can_work_tile(pcity, ptile)) continue;

The unit's whole world is the union of its cities' work radii, and
within that only tiles a city *could* work. That is **RC3** verbatim.
Invisible by construction:

- roads *between* cities (only `adv_workers_road_bonus` inside a radius)
- any terrain outside every radius, including future city sites
- tiles beyond the current radius that a larger city would reach

## 3. Why: the want is a delta, floored at zero, with no minimum

    if (!improve_worked) {
      best_newv = amortize((best_newv - best_oldv + best_extra)
                           * WORKER_FACTOR, best_delay);
    }
    best_newv /= WORKER_FACTOR;
    best_newv = MAX(best_newv, 0);

The value is the *improvement* in that tile's worth to that city,
discounted by travel plus work turns. The threshold is strictly `> 0`
and there is no floor above it, so a worker will take arbitrarily
marginal work if any exists. **Idle therefore means the delta really is
zero everywhere it can see.**

On `lategame-80` that happens early: `techlevel 50` hands out every
improvement from turn one, the empires are small and crowded, the radii
get improved out, and after that every enumerated tile scores zero.

This is also **RC1's corollary**: anything needing a *sequence* —
terraform then irrigate, road a corridor, prepare a site for a city
that does not exist yet — scores zero at every single step, so it never
starts.

## 4. Why they then do nothing: the AI has no fallback

When nothing scores, `worker_evaluate_improvements()` returns 0 with
`*best_tile = NULL`. What happens next depends on who owns the unit:

- **Human auto-worker**: `auto_worker_findwork()` sees the NULL and
  calls `adv_seek_shelter()` — walk to a friendly city.
- **AI player**: that line is never reached.
  `auto_workers_player()` (`autoworkers.c:1327`) branches on
  `is_ai(pplayer)` and calls the `settler_run` **hook** instead of
  `auto_worker_findwork()`.

deep's hook ends:

    } else {
      UNIT_LOG(LOG_DEBUG, punit, "cannot find work");
      dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);
      goto CLEANUP;
    }

No shelter, no sentry, no fortify, no move. The unit stays exactly
where it is and is re-evaluated **from scratch next turn** — a
pathfinder map over the map, plus every tile of every city radius, plus
(for a founder) a full site search — to reach the same conclusion. ~45
units per game paying that every turn is a CPU cost as well as a
strategic one.

`ai/default` has the same shape; this is not a deep regression.

## 5. Size, and what it is not

~45 parked units per game against ~8.7 cities per player. The waste is
two-sided: the units themselves (shields, and `pop_cost 2` for every
founder), and every improvement that is invisible rather than absent —
the corridors, the terraforming, the sites prepared before the settler
arrives.

Bounding it honestly: this is **not** evidence that 45 units of work
exist and is being missed. It is evidence that the AI cannot *see* work
outside a city radius, and that when it sees none it does nothing at
all. How much real value is out there is the thing to measure next, and
the cheapest probe is to widen the enumeration by one ring and count
how many tiles acquire a positive delta.

## 6. Where this already sits in the plan

- **W28** worker stacking and **W29** transform chains — the sequence
  problem in §3.
- **W12** empire graph — the corridor problem in §2.
- **RC3** generally: the worker world model is the union of city radii.
- `upstream-bugs.md` #31 (Join City) recovers the founder half only:
  ~11 parked founders per game are 22 citizens that could be handed
  back, but it does nothing for the 34 parked workers.

## 7. Open

1. Measure whether work exists outside the radii before building
   anything to find it. One extra ring of enumeration, counted, not
   acted on.
2. A do-nothing fallback is a bug independent of that: even
   `adv_seek_shelter()`, which humans already get, is better than
   standing in the open. Cheap, and it needs no new constant.
3. The re-evaluation cost is real: full search per idle unit per turn
   to conclude nothing. Worth a cheap negative cache if 2 does not
   already fix it.
