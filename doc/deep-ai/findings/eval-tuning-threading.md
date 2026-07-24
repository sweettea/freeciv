# Part 14 — Evaluation, tuning, and the threading question

PRD v2 addendum. Baseline: `freeciv/freeciv` @ `0896b34`.

---

# 1. How you actually tune with batches

## 1.1 Final score is a terrible gradient

It's terminal, aggregated over 250 turns of compounding, and dominated by
variance you don't control. If you tune against it directly you'll spend weeks
chasing noise.

**Use intermediate proxy metrics with a tight causal link to the change.** Each
workstream should tune against a metric that moves within 50–150 turns and has
lower variance than score:

| Workstream | Tuning objective | Why |
|---|---|---|
| W2 site eval | Total worked-tile output at T150 | Direct, low variance, no combat noise |
| W3 improvements | Sum of city sizes at T150 | Growth is the mechanism being fixed |
| W6 exploration | Techs-from-huts + explored fraction at T80 | Very low variance, fast |
| W7 geometry | Claimed tiles at T100, settlers lost | Two-sided — guards against suicide |
| W12 roads | Connected-city fraction, worker-turns per connection | Deterministic given a map |
| W13 campaigns | Units lost per city captured | The actual thing being improved |
| Everything | Final score | **Validation only, never the tuning target** |

Score stays as the acceptance gate. It should not be the objective function.

## 1.2 Paired comparison is the single biggest lever

Run A and B on **the same map seeds** and compare paired differences — common
random numbers, standard in simulation optimisation, and routinely skipped.

Rough power math. To detect effect size *d* at 80% power, α = 0.05, you need
about `16/d²` samples per arm.

- **Unpaired**, score SD/mean ≈ 30%, chasing a 5% improvement → d ≈ 0.17 →
  **~550 games per arm.**
- **Paired** on identical seeds, where A and B correlate at ρ ≈ 0.8, effective
  variance drops by (1−ρ) → **~110 games per arm.**

That's the difference between "not feasible" and "runs overnight." Every
comparison must be paired, and the seed list must be identical across arms.

Two further variance reducers: fix the map generator parameters (not just the
seed), and fix opponent AI types and count. Vary only the thing under test.

## 1.3 The eval pyramid

Map it onto the commit/epic/workstream decomposition from PRD §7:

| Level | What | Cost | Runs on |
|---|---|---|---|
| **L0** unit tests | Pure functions: `amortize`, `kill_desire`, migration-score estimator, MST, articulation points, want conversions | µs | every commit |
| **L1** position assertions | Load savegame, run one turn, assert a decision | seconds | every commit |
| **L2** short scripted games | 50–80 turns, fixed seed, assert proxy metrics | minutes | every epic |
| **L3** full batches | 250 turns × 100+ paired seeds | hours | every workstream |

**L0 and L1 carry most of the value**, because they're deterministic and fast
enough to gate every commit. L3 is expensive and only settles questions the
lower levels can't.

## 1.4 Unit tests worth writing

Freeciv has no AI test suite today, so this is greenfield.

**Pure-function tests.** `amortize()` monotonicity and boundary behaviour;
`kill_desire()` under killstack on/off; the migration-score estimator against
the server's `city_migration_score()` on fixture cities (they must agree
exactly — that's a strong test); MST correctness on hand-built graphs;
articulation points on known topologies.

**Property tests**, which catch more than example tests here:
- monotonicity — adding food to a tile never lowers a site's score
- invariance — want conversions round-trip within epsilon
- boundedness — no want is NaN, infinite, or negative where it shouldn't be
- symmetry — two identical map regions score identically
- ordering — a strict superset catchment scores ≥ its subset

**Golden want-vector snapshots.** For a fixture position, snapshot the full
want vector (every city's production choice, every unit's task). Any diff must
be intentional and explained in the commit message. This is how you catch "I
changed the road planner and it silently altered settler behaviour."

---

# 2. A rerunnable tuning harness

## 2.1 The one design decision that makes it work

**Constants must be data, not code.** If tuning requires recompilation you get
one trial per build, and the whole thing dies.

```c
struct tuning {
  /* W2 */ double catchment_tail_weight, specials_weight, cluster_bonus;
  /* W7 */ double perfection_base, pressure_flatten, claim_weight, denial_weight;
  /* W8 */ double doctrine_hysteresis, probe_horizon;
  /* W13 */ double force_margin, cluster_potential_weight;
  /* ... ~30 in total */
};
```

Loaded from a file named by a server setting, with compiled-in defaults. The
optimiser writes config files; the game reads them. No rebuild per trial.

Emit `tuning_hash` into the decision log so every result is traceable to the
exact parameter set that produced it.

## 2.2 Resumable by construction

```
results/
  <tuning_hash>/
    <scenario>/
      <seed>.json     # metrics, exit status, wall time
```

Content-addressed on `(tuning_hash, scenario, seed)`. A rerun skips completed
cells. Kill it, restart it, add seeds, add scenarios — nothing is recomputed.
This is what makes the framework survive contact with a laptop that sleeps.

## 2.3 The optimiser

**CMA-ES** is the right default: ~30 continuous parameters, noisy objective,
no gradients, moderately expensive evaluations. Bayesian optimisation is better
below ~20 parameters with very expensive evaluations; plain random search is a
surprisingly strong baseline and worth running first to calibrate.

Budget arithmetic: CMA-ES population ≈ `4 + 3·ln(30)` ≈ 14. At 100 generations
that's ~1,400 evaluations. Each evaluation is a paired batch — say 40 games at
30 s = 20 min. Single-threaded that's ~470 hours; across 16 cores, ~30 hours.
Overnight per workstream is realistic; a full global tune is not.

**So tune per workstream, not globally.** Six parameters at a time, with
everything else frozen, is tractable, interpretable, and debuggable. Thirty
parameters jointly is a research project.

## 2.4 The mistake to avoid

**Hold out seeds.** Tune on seeds 1–100, validate on 101–200, and report only
the held-out numbers. Otherwise you overfit to map layouts and ship constants
that are excellent on exactly the maps you tested.

This is the standard ML lesson and it's routinely skipped in game AI tuning,
where "we tested on 50 games" usually means "we tuned and evaluated on the same
50 games."

Also: **re-validate old workstreams after tuning new ones.** Parameters
interact. A regression suite of L2 scenario tests across all completed
workstreams should run after every tuning round.

---

# 3. Can each AI run in its own thread?

Your intuition — that AI-to-AI interaction is mostly read-only — is right about
the *game semantics* and wrong about *this implementation*. Three concrete
blockers, all found in code that looks read-only.

## 3.1 The most read-only-looking function in the AI mutates global state

`cityresult_fill()` — scoring a candidate city site — `daisettler.c:292–301`:

```c
pplayer->government = adv->goal.govt.gov;          /* ← writes the PLAYER */
...
saved_owner   = tile_owner(result->tile);
saved_claimer = tile_claimer(result->tile);
tile_set_owner(result->tile, pplayer, result->tile); /* ← writes the MAP */
```

and restores both at lines 455–458.

So the function that merely *scores a site* temporarily rewrites the player's
government and the ownership of a shared map tile. Under per-player threading,
another thread reading either during that window sees corrupt state — and it's a
torn read, not a stale one.

## 3.2 A single global scratch array shared by all AI players

`common/aicore/citymap.c:53`:

```c
static int *citymap = nullptr;
...
citymap = fc_realloc(citymap, MAP_INDEX_SIZE * sizeof(*citymap));
```

One `MAP_INDEX_SIZE` array, file-static, used for settler placement reservation
by every AI player. Two threads planning settlers simultaneously is a
straightforward data race — and a `fc_realloc` on top of it.

## 3.3 Global RNG

18 `fc_rand()` call sites across `ai/` and `server/advisors/`, drawing from
global state. Concurrent access is both a race and a determinism-killer — which
directly conflicts with §1's requirement that batches be reproducible.

## 3.4 This is presumably why `ai/tex` exists in the shape it does

`ai/tex` runs **one** thread for all AI players with a shadow copy of the world
fed by a message queue. That's a strange design choice unless you've tried
per-player threading and hit exactly these three walls. The shadow world isn't
about parallelism — it's about isolation from mutation.

## 3.5 The path, if you want it

Each blocker has a fix, and two of them you want anyway:

1. **Pass government as a parameter** instead of mutating `pplayer->government`.
   You need this regardless — PRD §6.3 already requires the evaluated government
   in the tile-cache key, and this is the same change.
2. **Pass virtual ownership into the waste calculation** instead of
   `tile_set_owner`. Contained: `city_waste()` needs an owner argument.
3. **Per-player `citymap`**, hanging off the AI player state rather than
   file-static.
4. **Per-player RNG streams.** Also fixes reproducibility under any threading,
   which §1 needs anyway.

Then the classic **plan/apply split**: a parallel read-only planning phase where
each player's thread computes plans against an immutable snapshot, followed by a
serial apply phase in fixed player order. Deterministic, because ordering of
*writes* never varies.

## 3.6 But measure before you build it

Two reasons to be sceptical this pays off:

**W13 removes most of the cost anyway.** `find_something_to_kill()` currently
builds a PF map per unit per turn. The campaign object makes target ranking once
per player per turn — an O(units) → O(1) reduction on the single most expensive
function. After that, per-player planning may simply not dominate.

**Amdahl.** Unit movement is inherently serial — each move changes the state the
next decision depends on — and it's a large share of AI time. If planning is 40%
of AI time, perfect parallelism across 6 players caps you at ~1.6× overall.

**And the granularity is wrong.** "One thread per AI" scales with player count,
which varies, and per-player work is uneven — a 30-city empire and a 2-city rump
are not equal tasks. A **thread pool with per-player planning jobs** is strictly
better: bounded threads, work-stealing, and it degrades gracefully at any player
count.

So: turn on `TIMING_LOG`, measure `AIT_*` after W13 lands, and decide with data.
My guess is the answer is "the four fixes in §3.5 are worth doing on their own
merits, and then a task pool over planning jobs — but only if the profile still
justifies it."

---

# 4. Additions to the PRD

**W0 gains:**
```
test: add unit test harness for AI pure functions
test: add property-based tests for want monotonicity and bounds
test: add golden want-vector snapshot for a fixture position
test: add paired batch runner with shared seed lists
test: add content-addressed result cache keyed on tuning hash
```

**New workstream W25 — Tuning harness**, after W1, before W2:
```
ai/deep: extract tunable constants into a tuning struct with defaults
ai/deep: load tuning parameters from a file named by a server setting
ai/deep: emit the tuning hash in the decision log
test: add CMA-ES driver over the tuning struct
test: split seeds into tuning and held-out validation sets
test: report held-out metrics only
doc: document the tuning workflow and per-workstream parameter groups
```

**New workstream W26 — Planning purity** (prerequisite for any threading):
```
ai/deep: pass evaluated government as a parameter rather than mutating the player
ai/deep: pass virtual tile ownership into waste calculation
common: make the citymap reservation array per-player
common: give each player an independent random number stream
test: assert AI planning performs no writes to shared game state
```

That last test is the valuable one — a debug-build assertion that the planning
phase touches no shared mutable state. It makes purity a maintained property
rather than a one-time cleanup, and it's the precondition for every threading
option including the one `ai/tex` already implements.
