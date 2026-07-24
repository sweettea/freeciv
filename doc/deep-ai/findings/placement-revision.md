# Part 23 — Revising the placement rule

**Supersedes PRD v3 §4.2 and Part 15.** Baseline: `freeciv/freeciv` @ `0896b34`.

---

## 1. Conceding, and the argument I should have made

Part 15 argued that ~45% of the PRD should land in `ai/default`. The reasoning
was mechanical — the vtable's granularity is empire-level, `daisettler.h`
exports six symbols, so overriding site scoring means reimplementing 1,400 lines.

That constraint is real. My conclusion from it was wrong. Four reasons, the last
of which I created myself and didn't notice.

### 1.1 The dependency risk I introduced

PRD v3 makes Phase 1 (`ai/default` tactical fixes) a **dependency** of Phase 3
(`ai/deep` strategy layer). W8 depends on W2 and W3. W13 depends on W10 and W11.

So if upstream declines Phase 1 — or takes eighteen months to review it — **the
entire strategy layer is blocked on someone else's decision.** Against a project
where one author writes 87% of commits and 2025 saw 591 commits total, that is a
serious single point of failure, and I built it into the plan without flagging it.

A design where the ambitious work can proceed regardless of upstream's appetite
is strictly better, and that's what you're asking for.

### 1.2 A/B integrity — the argument you didn't make

If `ai/default` improves, **your control arm moves.**

The whole measurement plan (Part VIII) rests on paired comparison against
`classic`. Improve `ai/default` and every result becomes "new AI beats a
different old AI than last month." You lose the ability to attribute gains, you
can't compare a Phase 3 result against a Phase 1 result, and the regression
corpus needs regenerating each time the baseline shifts.

**A frozen `ai/default` is worth real money as a measurement instrument**,
independent of any politics. That's probably the strongest single argument and I
missed it.

### 1.3 Divergence cost is much lower than I implied

I invoked "two copies will diverge." Check the actual rate: `ai/` +
`server/advisors/` received **1,294 added and 1,004 deleted lines across 44
commits in a year** — about 3.6% of the project's C churn. And that's the whole
subsystem; the specific functions `ai/deep` would replace (`cityresult_fill`,
`base_want`, `kill_desire`) see close to zero.

Divergence cost is proportional to how much the original moves. It barely moves.
Copy-and-rewrite is cheap here in a way it wouldn't be in an actively-developed
subsystem.

### 1.4 Rollback and blast radius

A module can be abandoned at zero cost. Behaviour changes to `ai/default` are
entangled with stable branches, longturn communities, and — since freeciv-web
builds the upstream C server — CivRealm's published baselines. That last one is
a genuine research-community harm: papers calibrated against a known opponent.

## 2. What I still think is true, in much narrower form

The mechanism problem doesn't disappear. But the fix isn't behaviour changes —
it's **visibility**, and that's a categorically easier ask.

`daisettler.h` exports six symbols. `cityresult_fill()`, `city_desirability()`,
`settler_map_iterate()`, `find_best_city_placement()` and the `tdc_plr_*` tile
cache are all `static`. `daicity.h` exports `dai_city_want()` but not
`base_want()`.

Making those non-static and declaring them in the header is:

- **zero behaviour change** — provably, by compiling and running the batch suite
- **zero risk** — no logic touched
- **a much easier upstream conversation** — "please expose these so another
  module can reuse them" versus "please change how the AI plays"

And it's the difference between `ai/deep` reimplementing 1,400 lines and
reimplementing forty. If upstream declines even that, `ai/deep` copies — which
per §1.3 is affordable.

## 3. The revised placement rule

Replaces PRD v3 §4.2.

> **Default: everything goes in `ai/deep`.**
>
> `ai/default` changes only when the change is (a) a pure visibility change with
> no behaviour effect, or (b) a fix so unambiguous that no reasonable person
> defends the current behaviour — **and even then, `ai/deep` must not depend on
> it landing.**
>
> `common/` and `server/advisors/` change only for pure-win, zero-behaviour-change
> infrastructure, or where the defect equally harms human players.

### Tier A — Submit upstream, depend on nothing

The ~25 Appendix A bug fixes. Offer them as standalone PRs because they're
genuinely bugs and it's good citizenship — and because it earns review bandwidth.
But **`ai/deep` implements correct behaviour on its own regardless.** If the
inverted `result_defense_bonus` is never fixed upstream, `ai/deep` simply doesn't
have that bug.

The critical change: these move from "Phase −1, a dependency" to "**submit in
parallel, depend on nothing.**"

### Tier B — Zero-behaviour-change, needed by `ai/deep`

Small, provable, easy to land:

| Change | Where | Risk |
|---|---|---|
| Export site-evaluation internals in the header | `ai/default` | None — visibility only |
| Export `base_want` and improvement-want internals | `ai/default` | None |
| **Delete the unused `is_req_active` mutex + list scan** | `common` | None — the value is never read; pure perf win for everyone |
| Optional government field on `req_context` | `common` | None — additive, defaults to current behaviour |
| `city_virtual_copy()` alongside `tile_virtual_new()` | `common` | None — new function |
| `city_waste()` owner parameter | `common` | None — mechanical |
| Per-player RNG (`fc_rand_state` already a value type) | `utility` | None |
| CM comparator context via `qsort_r` | `common/aicore` | None — reentrancy fix |
| Per-thread timing buckets | `server` | None |
| The four extension points (W30) | `advisors` | None — inert until a consumer registers |

Every row is defensible as "this is a refactor, here's the batch run showing
identical behaviour." That's a very different PR than "the AI now expands
differently."

**Note on `server/advisors/`:** it's shared with human auto-workers, so changing
it is arguably *more* invasive than changing `ai/default`. Two exceptions where
the defect harms humans equally and the fix is unambiguous: worker stacking
(W28 — human auto-workers also refuse to stack) and the transform-chain
precompute (W29 — additive). Everything else in `advisors` should be an
extension point that `ai/deep` registers into, not a logic change.

### Tier C — Everything else, in `ai/deep`

All tactical improvements return here: W2 site evaluation, W3 improvements and
co-investment, W5 settler routing, W6 exploration, W10 combat model, W11 defence
bookkeeping, W27's AI-side scaling fixes, W15's site-quality work.

`ai/deep` gets larger — roughly back to PRD v2's scope — but it is **self-
contained, abandonable, and unblocked.**

## 4. What this costs, stated honestly

Two real losses, and they're yours to accept:

**The tactical fixes don't reach existing players** until `ai/deep` becomes the
default, which may be never. A better default AI nobody has to opt into was worth
more than an excellent optional one — that argument was correct, it's just
outweighed by §1.1 and §1.2.

**More code.** `ai/deep` reimplements rather than inherits. If Tier B's visibility
changes land, that's forty lines instead of 1,400 per site — manageable. If they
don't, it's real duplication, mitigated by §1.3's observation that the originals
barely move.

**Mitigation for the first:** keep the graduation path as an *option*, not a
plan. Once a subsystem in `ai/deep` has proven itself over 100+ paired seeds, it
*may* be offered to `ai/default` as a separate contribution with the evidence
attached. But nothing downstream ever waits on that, and the offer can be
declined without consequence.

## 5. Revised phases

| Phase | Contents | Notes |
|---|---|---|
| **A (parallel, no dependents)** | Appendix A fixes as standalone upstream PRs | Good citizenship; earns review bandwidth; **nothing depends on it** |
| **0** | W0 scaffolding + harness, W25 tuning, Tier B visibility + purity changes | Tier B is the only upstream ask on the critical path, and every item is provably behaviour-neutral |
| **1** | W1 want unification, W2, W3, W5, W6, W10, W11, W27, W29 — **all in `ai/deep`** | Tactical correctness |
| **2** | W30 extension points (`advisors`), W28 worker stacking | The two justified `advisors` changes |
| **3** | W4, W7, W8, W9, W12, W13, W14, W17, W19 | Strategy layer |
| **4** | W15, W16, W18, W20, W21, W23 | |
| **5** | W22, W24 | Optional |

**`ai/default` is now frozen except for header visibility.** It remains the A/B
control for the life of the project, which is exactly what Part VIII needs.

## 6. The one thing to watch

With `ai/deep` owning the tactical layer, it inherits `ai/default`'s bugs
wherever it forwards rather than overrides. That's fine as long as it's
*deliberate* — but it will produce confusing results if a Phase 3 workstream sits
on top of a forwarded function that still has, say, the inverted defence bonus.

**Track it explicitly:** a table in the module of which hooks forward and which
are owned, and for each forwarded one, whether any Appendix A bug applies. When a
Phase 3 workstream depends on behaviour from a buggy forwarded function, that's
the trigger to take ownership of it — not before.

That keeps `ai/deep` growing on demand rather than pre-emptively, which is the
whole point of your objection.
