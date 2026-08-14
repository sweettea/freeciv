# Part 25 — Execution setup

How to get from these documents to working code, with an agent doing most of the
typing.

---

## 1. Why not one big PRD

Three failure modes, all predictable:

**Context waste and scope drift.** An agent implementing W5 (settler routing)
does not need W19 (coalitions) in context. Having it there costs tokens and
invites "while I'm here, I noticed…" — which is exactly the bundling the PRD
forbids.

**Drift over long horizons.** ~280 commits is far past where an agent holds a
plan coherently. It will start well and gradually lose the thread — most visibly
by forgetting the placement rule and starting to modify `ai/default`.

**Unverifiable claims.** An agent will report success. Without a harness *it can
run itself*, every acceptance criterion becomes a self-assessment. This is the
single biggest risk in the whole approach.

---

## 2. The shape: a repo, not a document

**Put the plan inside the fork.** Version-controlled alongside the code, updatable
by the agent, and naturally in context.

```
freeciv-deep/                       # your fork
  CLAUDE.md                         # invariants — read every session (§3)
  doc/deep-ai/
    00-context.md                   # PRD Parts I–IV. ALWAYS loaded.
    01-conventions.md               # commits, tests, build/run commands
    STATUS.md                       # progress; updated after every epic
    workstreams/
      W00-scaffolding.md
      W05-settler-routing.md
      W11-defence-bookkeeping.md
      ...                           # one per workstream, self-contained
    findings/                       # these 25 documents, as reference
  benchmark/
    run.sh  configs/  seeds/  results/  report.py
  ai/baseline/                      # vendored frozen AI. NEVER MODIFIED.
  ai/deep/                          # the work
```

**Session context = `CLAUDE.md` + `00-context.md` + one workstream file.** That's
a manageable load and it stays constant no matter how large the project grows.

Split PRD v3 mechanically: Parts I–IV become `00-context.md`, each Part VI
workstream becomes its own file, appendices go in `findings/`.

---

## 3. `CLAUDE.md` — drop this in the repo root

This is the highest-leverage file in the setup. Claude Code reads it every
session; it's where the invariants live.

```markdown
# Freeciv Deep AI — working agreement

## What this project is
A new AI module (`ai/deep`) that is measurably better than the stock Freeciv AI.
Full plan: `doc/deep-ai/`. Read `00-context.md` before anything else.

## Hard rules — violating these means the work gets reverted

1. **NEVER modify `ai/baseline/`.** It is a frozen copy of the stock AI at commit
   0896b34 and it is our measurement control. If it changes, every benchmark
   number in the project becomes meaningless.

2. **NEVER modify `ai/default/` except to make a `static` function non-static and
   declare it in the header.** No behaviour changes. No bug fixes. If you find a
   bug there, note it in `doc/deep-ai/findings/upstream-bugs.md` and implement
   correct behaviour in `ai/deep` instead.

3. **NEVER copy a function into `ai/deep` in order to modify it, without first
   checking whether it can be reused.** If it's `static`, rule 2's visibility
   exception applies. Copy only as a last resort, and say so in the commit
   message.

4. **`server/advisors/` changes only for the two approved workstreams** (W28
   worker stacking, W29 transform chains) or to add an extension point that is
   inert until registered. That code is shared with human auto-workers.

5. **One commit does one thing.** ≤~400 substantive lines. Builds and passes
   tests on its own. Describable in one line without "and".
   Title: `<area>: <imperative verb> <object>` — e.g.
   `ai/deep: weight catchment tiles by probability the city works them`

6. **Never claim an acceptance criterion is met without running the benchmark.**
   `./benchmark/run.sh --quick` during development, full suite before closing a
   workstream. Paste the numbers in the commit or PR body.

7. **Do not tune constants by guessing.** Constants live in `struct tuning`
   loaded from a file. Tuning happens via `benchmark/tune.py` against held-out
   seeds. If you find yourself picking a number because it "seems reasonable",
   stop and flag it for a tuning pass.

## Build and test
```
meson setup build && ninja -C build          # full build, ~minutes
ninja -C build                                # incremental
./benchmark/run.sh --quick                    # 10 paired seeds, ~5 min
./benchmark/run.sh --full                     # 100 paired seeds, ~hours
./build/tests/unit_tests                      # L0 unit tests
```

## Session protocol
1. Read `CLAUDE.md`, `doc/deep-ai/00-context.md`, and the one workstream file.
2. Check `STATUS.md` for where things stand.
3. Implement **one epic** (5–10 commits), not a whole workstream.
4. Run tests and `--quick`. Record numbers.
5. Update `STATUS.md`: what landed, what the numbers were, what's next.
6. Stop. Do not start the next epic in the same session.

## When to stop and ask
- The plan is ambiguous or seems wrong for what you found in the code
- A change would require modifying `ai/default` or `ai/baseline`
- Benchmark numbers regress and you can't explain why
- You want to bundle two workstreams
- You'd be inventing a constant
```

---

## 4. Session protocol

**One epic per session.** Not one workstream, definitely not one phase. An epic
is 5–10 commits ending in a measurable change.

The `STATUS.md` update at the end of each session is what makes the next session
work. It should be terse and factual:

```markdown
## W05 — Settler routing
- [x] E5.1 tile-behaviour callback (4 commits) — settler losses 41/100 turns → 16
- [ ] E5.2 is_pos_dangerous for civilians
- Note: `adv_danger_at` takes `struct unit *`, so risk cost needed widening.
```

---

## 5. Phase 0 is the part a human must verify

**Everything downstream is measured by tools Phase 0 builds.** If the harness is
subtly wrong — unpaired seeds, tuning-set contamination, a metric that doesn't
mean what it says — every subsequent result is garbage and you won't find out for
months.

Check by hand:

- **Does `deep` really equal `baseline` at the end of W0?** It must, within ±2%.
  If it doesn't, something is already different and you don't know what.
- **Are the seeds actually paired?** Run the same config twice and confirm
  identical results. Then run A and B and confirm they saw the same maps.
- **Is the held-out set genuinely held out?** Grep the tuning code for the
  holdout seed file. It should not appear.
- **Does a deliberately-broken change show up?** Introduce an obvious regression,
  confirm the harness catches it, revert. This is the single best test of the
  test.

Budget a day on this. It's the cheapest insurance in the project.

---

## 6. What the agent will do well vs badly

**Well:** the mechanical majority. W0 scaffolding, W26 purity refactoring, the
extension points, the `advisors` changes, test writing, most of the tactical
workstreams where evidence is specific and verification is a unit test. Code
archaeology — it can confirm line references and find the sites I've cited.

**Badly, and where you must stay in the loop:**

- **"Is this better?"** It will declare success on a 3% move over 10 games. Hold
  it to the stated sample sizes and held-out numbers.
- **Tuning.** It will invent plausible constants. Rule 7 exists for this.
- **Knowing when a design is wrong.** If the code doesn't match the PRD's
  evidence, the PRD may be stale — an agent will more likely bend the code to
  the plan than question the plan.
- **Scope discipline.** Rules 1–4 are all guarding against the same instinct.
- **Long-horizon coherence.** The `STATUS.md` + one-epic-per-session structure is
  the mitigation.

---

## 7. Suggested first three sessions

**Session 1 — vendor the baseline.** Copy `ai/default` + `ai/classic` to
`ai/baseline`, rename symbols, register the aitype, build, confirm
`create Foo baseline` works and plays. Purely mechanical, no judgment, and it
proves the environment works end to end.

**Session 2 — W0 harness.** Batch runner with paired seeds, metric extraction,
result cache. Then *you* verify per §5.

**Session 3 — W0 module skeleton.** `ai/deep` forwarding everything to `dai_*`.
Then run the harness: `deep` must equal `baseline`. That's your first real gate,
and passing it means the whole apparatus is trustworthy.

Only then start Phase 1 (W5, W11, W28, W6, W9-lite).

---

## 8. One practical caution

Freeciv builds take minutes and a 100-seed batch takes hours. Neither fits
comfortably in an interactive session.

Structure around it: `--quick` (10 paired seeds) during development for a
directional signal, `--full` kicked off at the end of an epic and read at the
start of the next session. The content-addressed result cache (W25) means a
`--full` run that gets interrupted resumes rather than restarting, which matters
more than it sounds.

And keep a single-threaded, fixed-seed mode permanently available. When a result
looks strange, the first question is always "is this real or is it nondeterminism"
— and you want to be able to answer that in one command.
