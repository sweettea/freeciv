# Action speculation in worker evaluation: the cost was a blocker scan, not the enablers (2026-11-01)

Follows `perf-workers-2026-11-01.md` section 5, which left
`action_speculate_unit_on_tile()` inside `worker_evaluate_improvements()`
(server/advisors/autoworkers.c) as the largest remaining piece of the
worker chain: 9.6% of the late turn, 2.34M calls. The brief for this
epic was an inert extension point in server/advisors (rule 4's
extension-point case) through which ai/deep would memoise the
speculation. Same method as the earlier perf epics: profile first,
oracle before the change, byte identity as acceptance. Branch
`epic/perf5-advisor-hook`, based at a9be757fed.

**Outcome: no advisors hook was built.** The profile showed that the
premise (repeated requirement evaluation) does not hold. The cost was
a linear scan in common/actions.c that never depended on the game
state. Removing that scan is exact by construction, needs no signature
and no invalidation, and makes human auto-workers, stock, baseline and
deep all faster. server/advisors/ and ai/ are untouched. The change is
in `common/`, under the zero-behaviour-change infrastructure clause
(00-context section 4), like S-PERF's change to common/aicore.

## 1. Where the 9.6% went

Callgrind of the late Europe T251 -> T252 turn (`late-geo-cg.out`
from perf3; the per-function counts match this tree's own base
profile exactly):

| function (under worker_evaluate_improvements) | Ir | calls |
|---|---|---|
| `action_speculate_unit_on_tile` | 15.69G | 2,335,622 |
| - `is_action_enabled_unit_on_tile_full` | 15.60G | 2,335,622 (1,345,207 pass `unit_can_do_action`) |
| - - `is_action_possible` | 16.00G | 1,348,076 |
| - - - **`action_is_blocked_by`** | **15.11G** | 1,346,769 |
| - - - - `action_would_be_blocked_by` | 9.24G | **188,548,127** |
| - - - - `action_get_actor_kind` | 0.77G | 192,588,443 |
| - - - - actual blocker enabler tests | 1.0G | 52,438 tile + 22,442 extras |
| - - - `actres_possible` | 0.51G | 1,343,026 |
| - - `action_enablers_allow` (the enablers proper) | 0.32G | 612,280 (are_reqs_active 1.6M) |

So the enabler requirement vectors cost 0.2% of the turn.
`action_is_blocked_by()` cost 9.2%: for every query it ran
`action_iterate` over all 143 actions and asked whether each one
blocks the action being tested.

## 2. Counts (trace build, late T251 -> 253; not committed)

- civ2civ3 defines 143 actions. Only 18 of them have a non-empty
  `blocked_by` (Enter Marketplace, the two disbands, the attacks,
  Conquer City/Extras, Transport Disembark, Unit Move). None of the
  worker actions (cultivate, plant, mine, irrigate, transform, road,
  base, pillage, clean) can be blocked.
- Per turn: 1.06M-1.27M `action_speculate_unit_on_tile` calls and
  671k-780k `action_is_blocked_by` calls. Of those, 635k-735k (92-99%)
  are for an action whose `blocked_by` is empty.
- Distinct speculation tuples per turn: 745k-910k when keyed on
  everything the enabler path reads (action, unit type, owner, actor
  tile, target tile, target extra, omniscience, **home city**, and the
  **actor's real tile**). Without the last two the count is only
  29k-42k, but both are read: the home city through
  `is_action_possible(..., homecity)` / `actres_possible`, and the real
  tile through `action_is_blocked_by()`, whose blocker checks call
  `is_action_enabled_unit_on_tile(actor_unit, ...)` and so use
  `unit_tile(actor)`, and through unit-state requirements (the FIXME
  in `action_speculate_unit_on_tile`). Each city's virtual worker in
  `contemplate_terrain_improvements` has its own home city and stands
  on its own city centre. An exact memo would therefore hit at most
  20-30% of the time and would still pay the full scan on every miss.

## 3. The fix (2 commits, common/actions.c)

1. `common: route action_is_blocked_by through an oracle`.
   `blocker_is_enabled()` holds the old switch body unchanged,
   `action_is_blocked_by_scan()` is the reference walk, and a fast form
   was added. With `FREECIV_ACTION_BLOCK_CHECK=1` both run on every
   call, any difference is `log_error`ed (`action-block-check
   mismatch ...`), the reference's answer is returned, and running
   totals are logged as `action-block-check turn=N checks=C
   mismatches=M`. The fast form was still the scan in this commit.
2. `common: walk only the blocking actions in action_is_blocked_by`.
   The fast form visits only the set bits of `act->blocked_by`, in
   ascending id order, and applies the same tests (unit actor kind,
   then `blocker_is_enabled()`). Every id it skips fails the scan's
   `action_would_be_blocked_by()` test, so it offers the same blockers
   to the same tests in the same order and returns the same action.

## 4. Identity (acceptance)

Reference pin `.perf-bin/ref` (a9be757fed); candidate `.perf-bin/fast`
(099bd6efe6).

| race set | mode | result |
|---|---|---|
| contested-classic deep s1-10, contested deep s1-10, contested mixed_db s1-3, contested-classic **baseline** s1-3 (T150), long.conf copy ENDTURN=200 deep s1 | default | 27/27 identical scorelog `data` + `3: deep-` census |
| same 27 | check | 0 mismatches over >= 111,979,437 checks (T200 game 12.9M; baseline arm 3.2M / 5.05M / 3.81M); scorelog 27/27 identical; census 26/27, see below |
| Europe T50 -> 60 | check | 9.8M checks, 0 mismatches; scorelog (5,115 `data`) + census (48,834) identical |
| Europe T50 -> 60 | default | identical (two runs) |
| Late T251 -> 256 | check | 5.7M checks, 0 mismatches; census (14,604 lines) identical |
| Late T251 -> 256 | default | census identical (two runs) |

The one census difference in check mode (contested-classic deep s5)
comes from the logger. In the reference, four identical consecutive
`deep-raid` lines are folded into "last message repeated N times". In
check mode an `action-block-check` totals line lands between them, so
the four lines are printed in full. This happens at two points (T83,
T112). The scorelog of that game is identical, and the game is
identical in default mode.

**Inertness.** The brief asked for proof that an unregistered hook
costs nothing. No hook exists, so instead every AI type and human
auto-workers take the same new path, and it gives the same answers.
That is shown by the three all-baseline races (identical in both
modes, 12M checks between them) and the mixed_db races (deep and
baseline seats in the same game).

## 5. Speed

Base for timing is `.perf-bin/base`: a9be757fed rebuilt with this
tree's exact meson options. The `.perf-bin/ref` pin is a different
build: it has no aitiming buckets and used about 2x the user time on
Europe, so it cannot be compared for speed. It is used for identity
only.

| | base | fast | |
|---|---|---|---|
| Late T251 -> 252 callgrind Ir (load incl.) | 123.24G | 108.68G | -11.8% |
| - `action_is_blocked_by` | 15.11G | 0.56G | same 1,346,769 calls |
| - `worker_evaluate_improvements` | 29.35G | 15.63G | |
| - `contemplate_terrain_improvements` (ai/default) | 18.06G | 9.81G | |
| - `deep_auto_settler_run` | 6.39G | 3.45G | |
| - `auto_worker_findwork` (human/stock auto workers) | 4.39G | 2.31G | |
| Europe T50 -> 60 user CPU, ABBA (load 12-16) | 159.6 / 154.6 s | 119.3 / 122.8 s | -23% |
| Europe wall | 193.5 / 158.4 s | 144.0 / 126.0 s | |
| Europe s/turn (save mtimes) | 13-24 | 10-16 | |
| - Worker want / turn | 2.8-5.4 s | 1.5-2.9 s | |
| - Workers / turn | 2.1-4.1 s | 1.1-1.8 s | |
| Late T251 -> 256 user CPU, ABBA (load 6-16) | 59.2 / 92.0 s | 39.3 / 89.1 s | noisy; use the callgrind row |
| - Worker want / turn | 1.0-3.5 s | 0.54-1.98 s | |
| Stock-only game (contested-classic baseline s1, T150) user CPU | 86.6 s | 74.4 s | -14% |
| Max RSS Europe / late | 169-170 / 188.7 MB | 169-170 / 188.7 MB | flat |

The box was shared with another agent's races during every pair, so
the wall-clock rows are directional. The callgrind row is
deterministic.

## 6. What is left in the worker chain, and the hook question

After the fix (late callgrind), `worker_evaluate_improvements` is
14.4% of the turn:

| | share | owner |
|---|---|---|
| `consider_worker_action` (incl. deep's task-value hook) | 4.75% | advisors / ai/deep |
| `pf_map_position` | 3.45% | advisors -> pathfinding |
| `action_speculate_unit_on_tile` | 1.81% | advisors -> common |
| `city_tile_value` | 1.03% | advisors |
| `is_extra_caused_by_action` (33.6M calls) | 0.96% | common |

For the coordinator:

1. **An advisors speculation-memo hook is not worth building now.**
   Even a perfect memo could save at most the remaining 1.8%. Its key
   has to include the home city and the actor's real tile (section 2),
   so it would hit at most 20-30% of the time. Its invalidation would
   also have to cover unit state, techs, government and settings,
   which the world-generation counter does not claim to cover. The
   extension-point rationale under rule 4 is unchanged: no advisors
   change was made.
2. `is_extra_caused_by_action` is called 33.6M times from the
   `aw_extra_action_iterate` loops: once per (tile, extra, action),
   and the answer depends only on the ruleset. A precomputed
   per-extra action table in autoworkers.c would be exact, but it
   would be an advisors change outside W28/W29 that is not an
   extension point, so it needs a rule-4 decision. It is worth ~1%.
3. CM (`cm_query_result`, 15.7% after this change) is still the
   largest single item. The perf4 agent is working on it.
