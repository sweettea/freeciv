# Part 24 — Building for demonstrable superiority

**Reframes PRD v3 phasing.** Baseline: `freeciv/freeciv` @ `0896b34`.

Goal, stated plainly: **`ai/deep` should be measurably, reproducibly, and
arguably better than `ai/default`.** Everything else — upstream adoption,
research-ecosystem benefit, code hygiene — is secondary and must not compromise
that.

---

## 1. Vendor a frozen baseline. This is the most important practical change.

Freezing `ai/default` in the world is neither possible nor necessary. Freezing it
**in your harness** is both.

```
ai/baseline/          <- vendored copy of ai/default + ai/classic at a pinned commit
                         NEVER modified. Registered as aitype "baseline".
ai/default/           <- upstream's, whatever it becomes
ai/deep/              <- yours
```

Copy `ai/default/` and `ai/classic/` at `0896b34`, rename the symbols, register as
`baseline`, and never touch it again. Then:

- **You can rebase on upstream freely** and take their fixes, without your
  measurements shifting under you.
- **Every result is comparable across the project's whole lifetime.** A Phase 1
  number and a Phase 4 number mean the same thing.
- **Upstream can do whatever it likes.** Their AI improving doesn't invalidate
  your claim; it just means you should *also* report against current `default`.
- **The Appendix A fixes become fully decoupled.** Submit them upstream as
  courtesy. If they land, `ai/default` improves and `ai/baseline` doesn't — which
  is correct, because "better than the AI as it was" and "better than the AI as
  it is" are two different claims and you want to be able to make both.

Cost: one directory, a symbol-renaming pass, and a build entry. Maybe a day.
Value: every measurement you take for the next two years remains meaningful.

**Report both numbers.** `deep vs baseline` is your progress metric.
`deep vs current default` is your public claim.

## 2. Define the win condition before writing code

"Better" needs to be specific or it can't be defended. Four measures, and they
answer different questions:

| Measure | Question it answers | Watch out for |
|---|---|---|
| **Score ratio, separate games, paired seeds** | Does it build a stronger civilisation? | Score weights may not match your intuition of "good" |
| **Head-to-head, same game** | Does it beat the other AI? | **Not independent, and gameable** — see §2.1 |
| **Survival / elimination rate** | Is it robust? | Slow to move; high variance |
| **Win rate vs a human** | Is it actually fun and hard? | Expensive, subjective, but it's the real goal |

**Report score ratio on paired seeds as the primary number.** It's independent,
low-variance under pairing, and can't be gamed by exploiting one specific
opponent.

### 2.1 The trap: overfitting to `ai/default` specifically

An AI tuned to beat `ai/default` head-to-head can be *worse in general*. Concrete
example from this analysis: `ai/default` never stacks defenders correctly and
over-requisitions (W11). An AI that learned to punish exactly that will look
brilliant against it and mediocre against anything else.

Three guards:

- **Primary metric is separate-game score**, not head-to-head.
- **Include self-play** in the suite: `deep` vs `deep` at varying difficulty. If a
  change helps against `baseline` but not in self-play, suspect exploitation.
- **Vary the opponent mix**: baseline-only, mixed baseline/deep, and — if you can
  stomach the setup — a CivRealm agent, since that ecosystem exists and benchmarks
  against the same engine.

### 2.2 Vary the conditions, or the claim is narrow

A result on one map size and one ruleset is a result about that map and ruleset.
The minimum matrix:

- **Map size / player count**: small (4p), medium (8p), large (20p)
- **Ruleset**: civ2civ3 and classic, at minimum — this is what W8.1's probe is
  *for*, and it's how you find out whether you've overfitted
- **Settings**: default, plus one aggressive-expansion config and one
  migration-heavy config
- **Difficulty**: hard, since that's where information handicaps are absent today

That's ~12 cells × 100 paired seeds. At ~30s/game and 16 cores, roughly overnight.

## 3. The demonstration artifact is a deliverable

If the goal is *clearly* better, the claim has to be reproducible by someone
else. Build this in Phase 0, not at the end:

```
benchmark/
  run.sh                 # one command, pinned seeds, emits results.csv
  configs/               # the ~12 cells from §2.2
  seeds/train.txt        # tuning
  seeds/holdout.txt      # reporting — never tuned against
  results/               # content-addressed by (aitype, config, seed)
  report.py              # table + confidence intervals + effect sizes
```

Anyone should be able to clone, run `./run.sh`, and get your table. That artifact
is what convinces a maintainer, a reviewer, or the CivRealm authors — far more
than a PRD does.

**Always report held-out numbers.** Tuning and reporting on the same seeds is the
single most common way game-AI claims turn out to be wrong.

## 4. Reordered for early, visible wins

PRD v3 put W1 (want unification) first because it's architecturally load-bearing.
That's correct for *composability* and wrong for *demonstrating progress* — it's
weeks of refactoring with, by design, zero behavioural gain.

Four workstreams produce visible improvement and need **little or no rework**
after W1, because they don't primarily manipulate want magnitudes:

| Workstream | Why it's W1-independent | Expected effect |
|---|---|---|
| **W5 settler routing** | Pathfinding parameters, not wants | Settler losses down ≥60% |
| **W11 defence bookkeeping** | Danger accounting and emergency ordering | Defender over-build down ≥40%; less production oscillation |
| **W28 worker stacking** | Activity scheduling; uses `amortize` directly | Long improvements actually get built |
| **W6 exploration** | Adds a *new* want; small conversion later | Techs from huts up ≥3×; map explored up ≥50% |

Add **W9-lite**: set `pcity->cm_parameter` from simple city state — at war →
shields, below growth cap → food, magnet → happiness — without waiting for full
doctrine. The mechanism is honoured and unused; this is the highest
leverage-to-effort item in the whole document and it doesn't need W1.

### Revised phasing

| Phase | Contents | Purpose |
|---|---|---|
| **0** | W0 harness, **`ai/baseline` vendoring**, benchmark artifact (§3), W25 tuning harness, Tier B visibility changes | You cannot measure without this |
| **1** | W5, W11, W28, W6, W9-lite | **First demonstrable win.** All in `ai/deep`, no want refactor needed |
| **2** | W1 want unification, then W2, W3, W29 | Economy — the biggest score mover |
| **3** | W27 scaling, W10 combat model, W13 campaigns, W14 war ledger | Military effectiveness; also unblocks large-game benchmarking |
| **4** | W4 knowledge, W8 doctrine, W7 geometry, W9 full, W12 empire graph | Strategy layer |
| **5** | W15–W24 | Breadth |
| **∥** | Appendix A fixes upstream | Parallel, decoupled, courtesy |

Phase 1 should produce a **measurable, defensible score improvement over
`baseline`** with maybe six weeks of work. That's the thing that tells you the
harness works, the metrics are sensitive enough, and the project is real —
before committing to the want refactor.

## 5. Expect the first result to be ambiguous

Two honest warnings.

**Some fixes won't show up in score.** W5 cuts settler losses 60% — but if the AI
was replacing them anyway, score may barely move. That's not failure; it means
the proxy metric moved and the aggregate didn't. Record both, and don't discard a
correct fix because the headline number was flat.

**Some fixes will make things worse before better.** W6 builds explorers, which
costs shields that used to go to settlers. Under `ECON_LANDGRAB` with no doctrine
yet, that could reduce early expansion. Expect it, measure it, and don't chase it
with a hasty counter-tune — it likely resolves once W8 can decide how much
exploration is worth.

This is why §2's *matrix* matters more than any single number. A change that
helps on large maps and hurts on small ones is information, not noise.

## 6. What "clearly better" would look like

A defensible claim, roughly:

> Across 12 configurations spanning two rulesets, three map sizes, and four
> settings, on 100 held-out map seeds per cell, `ai/deep` achieves a mean score
> ratio of X against a frozen `ai/default` baseline (95% CI [a, b]), with no cell
> showing regression. Head-to-head in shared games it wins N% of the time.
> Self-play results show the gains are not opponent-specific. Turn processing
> time at 20 players / 2000 cities is reduced by Y%.

Every element of that sentence is a thing the harness must be built to produce.
Design it backwards from there.

And the ancillary claim, which may matter as much:

> `ai/deep` plays with the same information available to a human player. The
> baseline does not.

That one is unanswerable, and it's why W4 is worth its cost even though it will
initially *lower* your score against an omniscient baseline. Report it as its own
line: **score under honest information vs. baseline under omniscience** is a
harder and more interesting comparison than like-for-like, and winning it is a
much stronger claim.
