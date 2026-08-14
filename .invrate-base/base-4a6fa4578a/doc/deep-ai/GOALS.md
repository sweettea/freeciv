# /goal conditions

One goal per **epic**, run in order. Paste the block after `/goal`.

## Why not one goal for the whole project

Per the official docs, a condition needs *one measurable end state*, *a stated
check*, and *constraints* — and the evaluator only sees the transcript. "The AI
is better than baseline" spans ~280 commits and several human decisions; there is
no single observable output that proves it. A mega-goal drifts, burns tokens, and
reports false completion.

Chain epic-sized goals instead. Each is 5–10 commits with a checkable finish.

## Template

```
GOAL: <one sentence, a state not a task>

DONE WHEN all of these appear in the transcript with real output:
1. <observable check> — paste the command and its output
2. ...

VERIFY: run each check and paste actual output. Never assert success without it.

CONSTRAINTS:
- <what must not change>
- Follow CLAUDE.md: one thing per commit, ≤400 substantive lines
- Update doc/deep-ai/STATUS.md as the final commit

STOP after N turns and report status if incomplete.
```

Pair with auto mode so turns run unattended. `/goal` with no argument shows
turns elapsed, tokens, and the last evaluator reason. `/goal clear` cancels.

---

# Phase 0

## E00.1 — Vendor the baseline

```
GOAL: ai/baseline exists as a vendored, registered, protected copy of the stock AI.

DONE WHEN all of these appear in the transcript with real output:
1. `ninja -C build` exits 0 — paste the final lines.
2. `ls ai/baseline/` lists the vendored sources.
3. A headless server run shows `create Testbot baseline` succeeding and the game
   reaching turn 20 with that player alive — paste the server output.
4. Save and reload preserves the aitype — grep the savegame for `ai_type` and
   show the baseline entry.
5. A CI check or git hook fails when files under ai/baseline/ change — demonstrate
   by touching a file there, showing the check fail, then reverting.

VERIFY: run each check and paste actual output. Never assert success without it.

CONSTRAINTS:
- Do not modify ai/default/, common/, or server/ except meson.build wiring.
- Do not modify ai/baseline/ after vendoring.
- Follow CLAUDE.md commit rules.
- Update doc/deep-ai/STATUS.md as the final commit.

STOP after 25 turns and report status if incomplete.
```

## E00.3–00.5 — Benchmark harness

```
GOAL: benchmark/run.sh produces paired, resumable, reproducible results.

DONE WHEN all of these appear in the transcript with real output:
1. `./benchmark/run.sh --quick --arm baseline --arm baseline` completes and
   report.py prints a table showing a near-zero difference between two identical
   arms — paste the table.
2. Running the same command twice produces byte-identical results/ contents —
   show a diff or checksum comparison proving it.
3. Both arms saw the same maps — the per-game log includes a map hash, and those
   hashes match across arms. Paste the comparison.
4. Interrupting a run and restarting it skips completed cells — show the log line
   count or a "skipping cached" message.
5. `grep -rn holdout benchmark/tune.py` returns nothing.

VERIFY: run each check and paste actual output.

CONSTRAINTS:
- Do not modify any AI module. This epic touches benchmark/ and test wiring only.
- seeds/holdout.txt must not be referenced anywhere in the tuning path.
- Follow CLAUDE.md commit rules.
- Update doc/deep-ai/STATUS.md as the final commit.

STOP after 40 turns and report status if incomplete.
```

**→ HUMAN GATE. Do not continue autonomously past this point.**
See `workstreams/W00-harness.md` §5. A human must confirm the sensitivity check:
introduce a deliberate regression, confirm the harness catches it, revert.

## E00.6–00.8 — Module skeleton

```
GOAL: ai/deep forwards every hook to the default AI and measures identical to baseline.

DONE WHEN all of these appear in the transcript with real output:
1. `ninja -C build` exits 0.
2. `create Testbot deep` succeeds and reaches turn 50.
3. `./benchmark/run.sh --quick --arm deep --arm baseline` shows mean score ratio
   within ±2% — paste the table with confidence intervals.
4. Save and reload of a deep player round-trips all per-player state — show a test
   asserting it, and its passing output.
5. The per-turn decision vector appears in the log, one line per player per turn —
   paste three consecutive lines.

VERIFY: run each check and paste actual output. The ±2% in check 3 is the gate;
if it fails, do not proceed — report what differs.

CONSTRAINTS:
- ai/deep must contain no behaviour of its own yet. Every hook forwards to dai_*.
- Do not modify ai/default/ or ai/baseline/.
- Wire player_save and player_load (they are commented out in classicai.c:628).
- Follow CLAUDE.md commit rules.
- Update doc/deep-ai/STATUS.md as the final commit.

STOP after 40 turns and report status if incomplete.
```

## E-B.1 — Delete the dead requirements mutex

```
GOAL: is_req_active no longer takes a global mutex, with no behaviour change.

DONE WHEN all of these appear in the transcript with real output:
1. The trdata lookup block in common/requirements.c (around line 6756) is removed
   or replaced with thread-local storage — paste the diff.
2. A grep proves the removed value was unused: show that `trdata` had no reads
   between assignment and function end in the original code.
3. `ninja -C build` exits 0 and the full unit test suite passes.
4. `./benchmark/run.sh --quick --arm deep --arm baseline` still shows ±2% — paste
   the table. This change must be behaviour-neutral.
5. Turn timing before and after, on the same config — paste both numbers.

VERIFY: run each check and paste actual output.

CONSTRAINTS:
- Behaviour must not change. If the benchmark moves outside ±2%, revert and report.
- Do not modify any AI module.
- Follow CLAUDE.md commit rules.
- Update doc/deep-ai/STATUS.md as the final commit.

STOP after 20 turns and report status if incomplete.
```

---

# Phase 1 — first behaviour changes

## E05.1 — Settler routing risk

```
GOAL: ai/deep settlers avoid dangerous routes during site selection, not just during travel.

DONE WHEN all of these appear in the transcript with real output:
1. ai/deep installs a settler tile-behaviour callback and applies adv_avoid_risks
   to the SEARCH parameter — paste the diff.
2. is_pos_dangerous is set for civilian pathfinding — paste the diff.
3. `ninja -C build` exits 0 and unit tests pass.
4. A corpus position with a hostile corridor between start and the best site shows
   the settler declining that route — paste the assertion and its passing output.
5. `./benchmark/run.sh --quick` on a contested config shows settlers-lost-to-
   hostiles per 100 turns reduced — paste before and after numbers.
6. Mean turns-to-found on a PEACEFUL config is not worse by more than 5% — paste
   the comparison. This guards against over-timidity.

VERIFY: run each check and paste actual output. Report both metrics in check 5
and 6 even if one moved the wrong way.

CONSTRAINTS:
- All changes in ai/deep. Do not modify ai/default/, ai/baseline/, or advisors/.
- Do not tune constants by guessing — add them to struct tuning with a placeholder
  and note it in STATUS.md for a tuning pass.
- Follow CLAUDE.md commit rules.
- Update doc/deep-ai/STATUS.md with both metrics as the final commit.

STOP after 30 turns and report status if incomplete.
```

## Pattern for remaining epics

Each workstream file's **Acceptance** section is already written as observable
checks. Convert it directly:

- Acceptance bullet → a numbered DONE WHEN item with "paste the output"
- Workstream's placement note → CONSTRAINTS
- Always add: "do not modify ai/default or ai/baseline", "do not invent
  constants", "update STATUS.md as the final commit"
- Turn cap: ~20 for mechanical epics, ~30–40 for ones with benchmark runs

Ask Claude to draft it: *"Read workstreams/W11-defence-bookkeeping.md and write a
/goal condition for epic E11.1 following the template in GOALS.md."*
