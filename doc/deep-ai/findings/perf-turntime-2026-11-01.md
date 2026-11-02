# Turn time: where the wait comes from (2026-11-01)

Motivation: the live Europe game (play-1028-2157, 14 AI seats + one human)
took 13-34 s per turn by T40-T50, and benchmark cells take hours. Before
touching code, measure.

## 1. Every pinned server was compiled at -O0

The main `build/` is `buildtype=debug`, which in meson is `-O0 -g`.
`readelf --debug-dump=info` on the pinned Europe/stacked-cell binary
(`.europe-bin/freeciv-server`, sha 9cb0a80c...) shows
`DW_AT_producer: ... -g -O0`. `benchmark/run.sh` defaults `--server` to
`build/freeciv-server`, so every benchmark game to date ran unoptimised.

Fix: `project(... default_options: ['buildtype=debugoptimized'])`
(-O2 -g). Existing build dirs keep their setting; reconfigure with
`meson configure build -Dbuildtype=debugoptimized`.

## 2. -O2 is behaviour-neutral and 1.6-1.9x faster

Race on one seed, `contested-classic` (8 players, 32x32, T80), same
tuning, sequential runs measured by CPU time:

| binary | user CPU | wall | outcome vs -O0 |
|---|---|---|---|
| -O0 (.europe-bin, 9cb0a80c) | 41.7 s | 43.9 s | - |
| -O2 (perf worktree, bacb8da3) | 22.4 s | 24.4 s | scorelog `data` lines byte-identical |

Europe, loaded from the T50 save with the human seat toggled to AI
(15 AI seats, 201 cities, 198 military units), run to T60:

| binary | s/turn T53-T60 | total incl. load | deep-want census |
|---|---|---|---|
| -O0 | 23.9 .. 29.2 (mean 27) | 268 s | - |
| -O2 | 15.0 .. 19.6 (mean 17) | 192 s | identical stream |

So the live game's wait is server compute, not the client, and -O2 alone
removes about a third of it.

## 3. Of the 17 s, 14.3 s is AI and 9.1 s is city danger assessment

`-Daitiming=true` buckets at T60 on the -O2 Europe run (per turn):

| bucket | s/turn |
|---|---|
| Total AI | 14.32 |
| Cities | 9.13 |
| - Danger | 9.10 |
| - Military want | 6.23 (nested; includes the 2nd assess_danger) |
| - Worker want | 2.63 |
| Workers | 1.65 |
| fstk | 1.52 |
| Units | 0.30 |
| Tech | 0.17 |

## 4. Mechanism

`assess_danger()` (ai/default/daimilitary.c:709) runs once per city per
dangerous player and builds a `pf_reverse_map`. Despite the name, that
map runs a FORWARD Dijkstra from each enemy unit's tile toward the one
city (`pf_reverse_map_pos`, path_finding.c:3740: `param->start_tile =
unit_tile(punit)`, `get_action` returns ATTACK only at `target_tile`),
early-stopped when the city is popped or cost reaches
`move_rate * (assess_turns + 1)`. The map is created and destroyed per
(city, player), so nothing is shared across cities: cost is
cities x hostile units Dijkstras, each allocating and zeroing a
map-sized lattice. Deep calls this sweep twice per turn: in
`deep_do_first_activities` (deepdefence.c:4377, via
`dai_assess_danger_player`) and per city inside
`deep_military_choose_build` (deepdefence.c:5471). On Europe that is
~200 x ~200 x 2 = 80k bounded Dijkstras per turn.

The result for a (unit, city) pair does not depend on the other cities:
every non-target city of ours is non-enterable in both formulations and
the target is reached only through the ATTACK action, which ends the
path. One forward map per hostile unit therefore yields the same
`pf_position` (turn, total_MC) for every one of our cities at once, and
the per-city loop with all its side effects (city_data, wallvalue,
`dai_wants_defender_against`, `dai_reevaluate_building`) can stay in its
exact order reading from that table. Expected: Danger 9.1 s -> ~1 s,
Military want similarly, Europe turn 17 s -> ~5 s at -O2.

Constraints: ai/default is read-only, so the restructured sweep is a deep
copy of `assess_danger()` + `assess_danger_unit()` (rule 3 last resort:
the data structure changes; say so in the commit). Acceptance is
byte-identity, not a benchmark: scorelog `data` lines and the deep census
stream equal to the unmodified binary on the same seeds.

Scratch artefacts: scratchpad perf/ (o0-s1, cpu-o0, cpu-o2, eu-o0,
eu-o2b, eu-cg3 callgrind).

## 5. Callgrind confirms it (one -O2 Europe T50 turn, incl. save load)

`scratchpad perf/eu-cg3/callgrind.out`. Inclusive:
`pf_reverse_map_unit_position` 49.6% of all instructions;
`assess_danger` 27.5% (the first-activities sweep 16.9%, the rest the
build-choice sweep); `pf_normal_map_iterate` 31.5%; `pf_normal_map_new`
24.6%. Self: `__memset` 24.6% -- the per-Dijkstra map-sized lattice
calloc in `pf_normal_map_new` -- then `pf_normal_map_iterate` 7.5%,
`tile_move_cost_ptrs` 5.1%, `is_native_to_class` 4.6%,
`action_would_be_blocked_by` 4.1%, `player_slot_next` 3.6%,
`map_pos_to_tile` 3.5%. A quarter of the turn was zeroing lattices.

## 6. The fix: shared reverse-map searches in common/aicore (landed)

### The plan's identity argument was wrong

§4 claimed "every non-target city of ours is non-enterable" so one map
with every city marked ATTACK equals the per-city maps. It is not:

1. `pf_get_move_scope()` gives `PF_MS_CITY` on any city tile when
   `utype_can_take_over()` (most land military), and a land unit's
   scope on a land city tile is `PF_MS_NATIVE` anyway. Conquerors path
   THROUGH non-target cities; marking them ATTACK (`TB_DONT_LEAVE`)
   cuts those paths.
2. Entering an ATTACK node costs `SINGLE_MOVE` (or `move_rate` for
   all-MP / suicide attackers), not `get_MC`, so a plain map's cost AT
   a city tile is not the targeted cost either.
3. Knowledge is the attacker's (`owner`), omniscience the defender's
   `H_MAP`: any sharing is keyed by both.

### What landed instead (4 commits)

- `common: count changes to the world that pathfinding reads` and
  `server: bump the world generation where pathfinding inputs change`:
  `map_world_generation()`, bumped by the common setters (terrain,
  extras, tile owner, city centre appearing/going, transport
  load/unload, orders freed, unit removed), the server mutation sites
  (unit placed/moved/transformed/re-owned, orders set, city
  transferred, diplstate set and counted down, extras owner, tile
  knowledge on an actual change) and the OUTERMOST entry/exit of every
  AI callback. Nested callbacks must not bump: `assess_danger()` reaches
  `consider_plr_dangerous` per (city, hostile player) through
  `adv_is_player_dangerous()`, and bumping there made every lookup a
  miss (100000 lookups, 100000 searches, found by tagging the last
  bumper's return address). Re-setting a city centre in
  `tile_set_worked()` (citizen arrangement does it) must not bump
  either.
- `common: reconstruct reverse-map positions from shared searches`:
  one target-free search per key (map, owner, omniscience, start tile,
  move rate, moves_left_initially, EXACT utype, max_turns), shared by
  every `pf_reverse_map` within a generation; each target's position
  is rebuilt: start tile -> start node; target unknown to the owner ->
  its own node (the targeted map never asks get_action there); a
  non-civilian that may not invade -> unreachable; otherwise min over
  expandable neighbours n of cost(n) + MIN(attack_cost,
  moves_left(cost(n))), unreachable unless below the bound. Exact even
  when the shared graph routes through the target: the neighbour such
  a path entered the target from has the same cost in both graphs, is
  expandable, and is no costlier than n; the candidate is
  non-decreasing in cost(n). Guard: no sharing while any two cities are
  adjacent (the only case where `pf_get_move_scope()`'s
  previous-scope test makes node properties expansion-order
  dependent). Exact-utype keys leave the per-map equivalence hash in
  charge of picking the representative unit, so representatives are
  unchanged. `dir_to_here` of a rebuilt ATTACK position is the first
  minimising neighbour's (tie-break may differ; no caller reads it).
- `common: make shared reverse-map searches the default`;
  `FREECIV_PF_REVERSE_SHARED=0` restores the per-target searches,
  `FREECIV_PF_REVERSE_CHECK=1` runs them as an oracle beside every
  lookup and `log_error`s any difference in (reachable, total_MC,
  turn, moves_left, total_EC, fuel_left).

ai/default and ai/baseline are untouched and get the speedup too
(baseline seats ran through it in the mixed_db races, identical).

### Identity (acceptance)

Reference: unmodified -O2 pin `.perf-bin/o2` (bacb8da3).

| race set | mode | result |
|---|---|---|
| contested-classic deep s1-10, contested deep s1-10, contested mixed_db s1-3, T150 | check | 23/23 identical scorelog `data` + `3: deep-` census; 0 mismatches / >= 10.8M lookups |
| same 23 | default | 23/23 identical |
| Europe T50->60 | check | >= 1.4M lookups, 0 mismatches, 175k searches; scorelog + census identical |
| Europe T50->60 | default | scorelog + census identical |
| Late T251->256 | check | >= 400k lookups, 0 mismatches; census identical on the common span |
| Late T251->254 | default | census identical |

### Speed (paired, quiet machine, same pins)

| | ref -O2 | shared | |
|---|---|---|---|
| Europe wall, T50->60 incl. load | 122.2 s | 63.1 s | 1.9x |
| Europe s/turn | 10-12 | 5-6 | |
| Europe AI / turn | 9.2 s | 4.4 s | |
| - Danger | 5.8 s | 1.0 s | 5.8x |
| - Military want | 4.0 s | 1.7 s | |
| Europe max RSS | 168.5 MB | 169.9 MB | |
| Late s/turn (T251-253) | 39, 39 | 9, 8 | 4.5x |
| Late AI / turn | 38 s | 7.6 s | |
| - Danger | 34 s | 3.2 s | 10x |
| Late max RSS | 188.4 MB | 188.7 MB | |

The earlier single-run references (eu-o2b 17 s/turn; late-o2 2:43 per
turn) were taken on a loaded machine; only the paired rows above are
comparable.

### Superseded first attempt

`perf-deepcopy-backup` (5 commits): a deep-only copy of
`assess_danger()` reading a per-sweep table built with the same
neighbour reconstruction (not the all-ATTACK map). Verified
(23/23 identical on both sweeps; Europe 1.1M cross-checks, 0
mismatches, 0 stale-row fallbacks) but dropped in favour of the
common/aicore version, which needs no ai/default visibility changes,
no copy, and speeds up stock and baseline alike.

### Next candidates (not started)

- Europe's remaining AI time: Worker want 1.66 s, Military want 1.67 s
  (now mostly non-pathfinding), Workers 1.06 s, fstk 0.97 s, Danger
  0.91 s.
- Each shared search still callocs a map-sized lattice (the 24.6%
  memset, now ~8x rarer on Europe): a reusable scratch lattice reset
  only on touched nodes would remove most of what is left of it.
- Sharing stops at every top-level AI callback; the 8 x share ratio on
  Europe is per player. Sharing across players' callbacks would need
  proof that no callback mutates the world without a bump -- the check
  mode is the instrument for that.
- `dai_hunter`/bodyguard reverse maps (daiunit.c:3137) already benefit;
  not measured separately.
