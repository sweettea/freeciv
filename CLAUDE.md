# Freeciv Deep AI — working agreement

## What this project is

Building `ai/deep`: a new Freeciv AI module that is **measurably, reproducibly
better** than the stock AI. Not a rewrite of the stock AI — a new module beside it.

**Read `doc/deep-ai/00-context.md` before doing anything else.** It is short and
it contains the reasoning that makes everything else interpretable.

Full plan: `doc/deep-ai/PRD-v3.md`. Per-workstream detail:
`doc/deep-ai/workstreams/`. Supporting investigation: `doc/deep-ai/findings/`.

---

## Hard rules

Violating these means the work gets reverted. They exist for specific reasons
documented in `00-context.md` §5.

### 1. NEVER modify `ai/baseline/`

It is a frozen copy of the stock AI at commit `0896b34`, and it is the
measurement control for the entire project. If it changes, every benchmark
number ever taken becomes incomparable. Treat it as read-only forever.

### 2. NEVER modify `ai/default/` except for visibility

The **only** permitted change is making a `static` function non-static and
declaring it in the corresponding header, so `ai/deep` can reuse it. That is
provably behaviour-neutral and you must demonstrate it with a benchmark run.

**No behaviour changes. No bug fixes.** If you find a bug in `ai/default`:
1. Append it to `doc/deep-ai/findings/upstream-bugs.md`
2. Implement the *correct* behaviour in `ai/deep`
3. Do not fix it in place

We want `ai/default` to keep its bugs so it remains a stable comparison, and so
that fixes can be offered upstream separately without this project depending on
them landing.

### 3. Prefer reuse over copying

If you need to change a `static` function in `ai/default`, first check whether
rule 2's visibility exception lets you call it instead. Copy the function into
`ai/deep` only as a last resort, and say so explicitly in the commit message with
the reason.

### 4. `server/advisors/` is shared with human auto-workers

Changes there affect human players using auto-workers. Permitted only for:
- **W28** (worker stacking) and **W29** (transform chains) — both are defects
  that harm humans equally
- Adding an **extension point** that is inert until a consumer registers

Anything else: put it in `ai/deep`.

### 5. One commit does one thing

- ≤ ~400 substantive lines
- Builds and passes tests on its own
- Describable in one line without the word "and"
- Title format: `<area>: <imperative verb> <object>`
  - Good: `ai/deep: weight catchment tiles by probability the city works them`
  - Bad: `ai/deep: improve site evaluation and fix worker assignment`

Areas: `ai/deep`, `ai/default`, `advisors`, `common`, `server`, `utility`,
`build`, `test`, `doc`.

### 6. Never claim an acceptance criterion is met without running the benchmark

`./benchmark/run.sh --quick` during development. Full suite before closing a
workstream. **Paste the actual numbers** in the commit body or PR description.

"Looks better" is not a result. A 3% move over 10 games is noise.

### 7. Never invent a constant

Tunable constants live in `struct tuning`, loaded from a file. Tuning happens via
`benchmark/tune.py` against **training** seeds, and results are reported on
**held-out** seeds.

If you catch yourself picking a number because it "seems reasonable", stop, add
it to `struct tuning` with a placeholder, and flag it in `STATUS.md` for a tuning
pass.

---

## Build and test

```sh
meson setup build && ninja -C build     # full build (minutes)
ninja -C build                          # incremental
./build/tests/unit_tests                # L0 unit tests (fast)
./benchmark/run.sh --quick              # 10 paired seeds (~5 min, directional)
./benchmark/run.sh --full               # 100 paired seeds (hours, resumable)
./benchmark/report.py                   # table + CIs from results/
```

Benchmark runs are resumable — the result cache is keyed on
`(tuning_hash, config, seed)`, so an interrupted `--full` picks up where it left
off rather than restarting.

**Always pin the binary for a benchmark in flight.** Every run takes
`--server <path>`, and that path must not be rebuilt until the run finishes. The
failure this prevents: later games silently exec a new binary and the run is
tainted (it happened; the result dir had to be deleted).

Pinning, not abstaining, is the rule — "never rebuild" was the old wording and it
was stricter than the invariant. A build that cannot reach a pinned path cannot
taint anything, so **work may proceed in parallel with a run** provided each
worker has its own worktree with its own `build/`, its own `--server` path, and
its own port block. `.worktrees/bench` is the long-standing instance of the
pattern; the sha-keyed cache keeps provenance exact either way.

What this still forbids: rebuilding the exact binary some run is executing.
If you cannot say which binary a live run is pinned to, do not build.

---

## Session protocol

1. Read `CLAUDE.md`, `doc/deep-ai/00-context.md`, and **one** workstream file.
2. Check `doc/deep-ai/STATUS.md` for current state.
3. Implement **at most one epic** (5–10 commits) **per subagent session**. Not a
   whole workstream. Not a phase.
4. Run `./build/tests/unit_tests` and `./benchmark/run.sh --quick`.
5. Update `STATUS.md` with what landed, the numbers, and what's next.
6. **Stop.** Do not begin the next epic in the same session.

The budget is per subagent session, not per wall-clock session: several subagents
may each own an epic at the same time, under the pinning rule above (own
worktree, own `build/`, own `--server`, own port block). What stays forbidden is
one session sliding from its epic into the next one — that is what step 6 is for,
and it is unaffected by how many sessions are running.

**Parallelise scheduling, not attribution.** Two epics on the same causal chain
still cannot be read from two separate A/Bs; if their effects can interact, they
want one factorial design, not two concurrent arms.

---

## Stop and ask the human when

- The code doesn't match what the plan's evidence says — **the plan may be stale,
  and you should question the plan rather than bend the code to it**
- A change seems to require touching `ai/default` or `ai/baseline`
- Benchmark numbers regress and you cannot explain why
- You want to combine two workstreams
- You'd be inventing a constant
- A workstream's design seems wrong given what you found in the source

Flagging one of these is a good outcome, not a failure.

---

## Known agent failure modes on this project

Stated so you can watch for them in yourself:

- **Declaring success on insufficient data.** Rule 6.
- **Inventing plausible constants.** Rule 7.
- **Scope drift into `ai/default`.** Rules 1–4 all guard this.
- **Bundling workstreams** because two touch the same file. They still ship
  separately.
- **Bending code to match a stale plan.** The line references were accurate at
  commit `0896b34`. If they don't match, say so.
