# Worker evaluation: where the late-game cost was, and the exact fix (2026-11-01)

Follows `perf-turntime-2026-11-01.md` (same method: profile first, oracle
mode before the change, byte-identity as acceptance). Branch
`epic/perf3-workers`, based at 46b076c43e (shared reverse-map searches).

## 1. Profile of the chain (late Europe T251 -> T252)

Callgrind of one turn from the T251 save (228 cities, 520 units, 7 live
AIs: 5 stock, 2 deep), loaded with the post-pathfinder-fix server;
counts cross-checked with a trace binary that logged every
`deep_worker_task_value()` call (same 427,858 calls in both).

Inclusive shares of the whole run (load included):

| function | share | calls |
|---|---|---|
| `worker_evaluate_improvements` (advisors) | 30.9% | |
| - from `contemplate_terrain_improvements` (ai/default, virtual worker per city) | 21.0% | 193 |
| - from `deep_auto_settler_run` | 5.4% | 41 |
| - from `auto_worker_findwork` | 3.2% | 75 |
| - from `dai_auto_settler_run` | 1.3% | 58 |
| `consider_worker_action` -> task-value hook | 18.4% | 565,966 |
| `deep_worker_task_value` | 18.4% | 565,966 (427,858 for deep seats) |
| - `adv_unit_att_rating` | 8.0% | 123.7M |
| - `real_map_distance` | 5.8% | 107.0M |
| - `player_slot_next` (+ slot_get) | 2.0% | 189.2M |
| - trunk / base / travel terms together | < 0.1% | |
| `action_speculate_unit_on_tile` (in `worker_evaluate_improvements`) | 8.1% | 2.34M |
| `pf_map_position` (in `worker_evaluate_improvements`) | 1.9% | 171,754 |

So 98% of `deep_worker_task_value()` was one loop: the danger discount's
"is any at-war attacker within `worker_guard_radius` of this tile?",
answered by walking every unit of every at-war player, and every one of
the 512 player slots, once per auction candidate. That loop is the hot
`players_iterate` behind `player_slot_next`'s 4.6% self share, and most
of `map_distance_vector`'s 4.0%.

Distinct inputs for the 427,858 deep calls: 324,416 came from
`contemplate_terrain_improvements`' virtual worker, 103,442 from real
workers; 8,065 distinct (player, tile, activity, extra, unit type);
every call reached the scan (value > 0, owned tile), over only 858
distinct (player, tile) and 23,893 distinct (player, tile, world
generation) across 59 generations.

Not in this chain, contrary to the brief:

- `cm_query_result` (8.8%) is called by `auto_arrange_workers` (2,432
  calls, 7.4%) and `dai_manage_taxes` (191, 1.4%). Worker evaluation
  issues no CM queries, so there was nothing for a `deepcm.c` memo to
  serve here; CM belongs to the next epic.
- `pf_map_iterate` (14.4% here) is mostly `pf_normal_map_position`
  from bombers, hunters, ferries and `building_advisor`; the worker
  chain's own pathfinding is `pf_map_position` 1.9%.

## 2. The fix: geometry instead of census

The exposure answer depends only on the units standing within the
radius. `deep_worker_enemy_near_fast()` visits the tiles within
`worker_guard_radius` (`square_iterate`: every tile at real distance
<= r, each once) and the units on them, applying the reference's
predicate unchanged (owner alive, at war, `adv_unit_att_rating > 0`,
`real_map_distance <= r`). The set of units offered is exactly the set
the scan could accept, the predicate is pure and the result is an OR,
so the answer is identical by construction: (2r+1)^2 = 169 tile visits
at the default r = 6 instead of O(enemy units).

No memo was built. A per-generation memo would have cut the scans
18x (427,858 -> 23,893); the geometric form needs no invalidation at
all, and once it landed the deep side of the chain was no longer
worth memoising (section 4).

Oracle: `FREECIV_DEEP_WORKER_CHECK=1` runs the old scan beside every
lookup, `log_error`s any difference (`worker-check mismatch ...`) and
returns the reference's answer; running totals are logged as
`worker-check turn=N checks=C mismatches=M` (not `deep-`, so check-mode
logs still compare on `3: deep-`).

## 3. Identity (acceptance)

Reference pin `.perf-bin/ref` (server b3102e31, the merged -O2 server
with shared reverse-map searches); candidate `.perf-bin/geo` (server
fefa39e8).

| race set | mode | result |
|---|---|---|
| contested-classic deep s1-10, contested deep s1-10, contested mixed_db s1-3 (T150), long.conf copy ENDTURN=200 deep s1 | check | 24/24 identical scorelog `data` + `3: deep-` census; 0 mismatches over >= 15.9M checked lookups (13.2M in the 23 T150 games, 2.68M in the T200 game) |
| same 24 | default | 24/24 identical |
| Europe T50 -> 60 | check | scorelog (5,115 `data`) + census (48,834 lines) identical; >= 2.3M checks, 0 mismatches |
| Europe T50 -> 60 | default | scorelog + census identical |
| Late T251 -> 256 | check | census (14,604 lines) identical; >= 1.3M checks, 0 mismatches |
| Late T251 -> 256 | default | census identical |

(Check counts are the last running total each game logged, so lower
bounds. The late save has no scorelog; its census is the comparison.)

## 4. Speed

Paired, back to back, same pins, `/usr/bin/time -v`; load average
2.8-4.9 during the pairs (another agent was racing on the box).

| | ref | geo | |
|---|---|---|---|
| Europe wall, T50 -> 60 incl. load | 89.4 s | 71.5 s | -20% |
| Europe s/turn (T52-T61, save mtimes) | 7.4 .. 8.7 | 5.9 .. 7.1 | |
| Europe AI total / turn | 5.4 .. 6.3 s | 4.3 .. 5.4 s | |
| - Worker want | 2.04 .. 2.44 s | 1.15 .. 1.45 s | 1.7x |
| - Workers | 1.26 .. 1.90 s | 0.79 .. 1.17 s | 1.6x |
| Europe max RSS | 169.4 MB | 169.3 MB | |
| Late wall, T251 -> 256 incl. load | 75.6 s | 64.7 s | -14% |
| Late s/turn (T252-T256) | 11.4, 12.2, 12.7, 12.0, 10.3 | 9.6, 10.8, 10.9, 10.2, 8.5 | |
| Late AI total / turn | 8.0 .. 10.9 s | 6.7 .. 9.6 s | |
| - Worker want | 2.13 .. 2.56 s | 1.00 .. 1.26 s | 2.1x |
| - Workers | 1.02 .. 1.47 s | 0.64 .. 0.91 s | 1.6x |
| Late max RSS | 188.8 MB | 188.5 MB | |

## 5. What is left, and whose it is

Callgrind of the same late turn with the candidate (`late-geo-cg.out`
in the perf3 scratchpad): the run drops from 194.2G to 163.6G
instructions (-15.8%, load included). `deep_worker_task_value` goes
35.7G -> 5.1G inclusive (18.4% -> 3.1%); adv_unit_att_rating from it
123.7M -> 90k calls. What remains of it: `map_pos_to_tile` from the
square walk 1.05%, `get_virtual_defense_power` + `players_iterate` in
the base want 0.5%, `real_map_distance` 0.2%.

| remaining in the worker chain (after) | share | owner |
|---|---|---|
| `worker_evaluate_improvements` total | 17.9% | advisors |
| - `action_speculate_unit_on_tile` (2.34M calls) | 9.6% | advisors -> common/actions |
| - `consider_worker_action` incl. deep hook | 3.2% | advisors / ai/deep |
| - `pf_map_position` | 2.3% | advisors -> pathfinding |
| - `city_tile_value` | 0.7% | advisors |
| `contemplate_terrain_improvements` (193 calls) | 11.0% | ai/default |

For the coordinator:

1. **The biggest remaining piece is `action_speculate_unit_on_tile`
   inside `worker_evaluate_improvements`** (autoworkers.c): 2.34M
   enabler evaluations per late turn (are_reqs_active, tri_req_present,
   action_is_blocked_by), the same (unit type, tile, activity, extra)
   asked by `contemplate_terrain_improvements`' virtual worker of every
   city and again by every real worker. It is advisors code shared with
   human auto-workers, so this epic did not touch it. A fix needs
   either an inert extension point (a "can this unit do act/extra
   here" cache hook the AI registers) or a change accepted under the
   advisors rule; it is not deep-only.
2. **The pf map per virtual worker is not large**: the chain's own
   pathfinding is `pf_map_position` 2.3% (171,754 calls) plus
   `pf_map_new` 0.06% (367 maps). The 7-14% `pf_map_iterate` in the
   whole-turn profile is other callers (`pf_normal_map_position` from
   bombers, hunters, ferries, `building_advisor`). Nothing to do here.
3. **CM is outside this chain**: `cm_query_result` 10.5% after (17.1G,
   unchanged) comes from `auto_arrange_workers` and `dai_manage_taxes`.
   The next epic's.
4. A per-world-generation memo of the exposure answer would remove
   most of the 3.1% that is left (23,893 distinct (player, tile,
   generation) of 427,858 lookups), at the price of depending on the
   generation bump sites covering unit hp/veteran changes, which they
   do not claim to. Not worth that risk for ~2.5%; the oracle is in
   place if someone wants it.
