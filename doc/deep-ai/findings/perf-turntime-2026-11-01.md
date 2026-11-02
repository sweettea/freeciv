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
