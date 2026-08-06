# Context — read this first, every session

Short by design. Everything else in `doc/deep-ai/` is reference; this is the part
you need in your head.

**Baseline commit: `0896b34`** (freeciv master, Apr 2026). Every line reference in
these documents was read from that tree. **If a reference doesn't match, the tree
has moved — say so rather than guessing.**

---

## 1. The problem in one paragraph

The Freeciv AI is a competent tactical engine bolted to no strategy at all, and
omniscience has hidden the absence. It doesn't decide where its empire should be
(site selection scores **two tiles**). It doesn't decide to build up rather than
out (improvement value is measured at today's city size, so an aqueduct scores
~0). It doesn't decide to attack a city (every unit picks its own target from its
own pathfinder map). It doesn't decide to connect its cities (workers only look
inside existing city work radii). It doesn't know whether a war is going well. And
it doesn't need to *look* at anything, because at every difficulty anyone plays
against, it sees the entire map.

The codebase is well-crafted. Its authors left honest comments wherever they knew
something was wrong — we found eleven. **A strategy layer was simply never
written.**

---

## 2. Six root causes

Every workstream attacks at least one. If a proposed change doesn't map to one of
these, it's probably the wrong change.

**RC1 — No time horizon.** Everything is evaluated at the present instant.
`base_want()` (`daicity.c:1530`) measures an improvement at current size with
currently-worked tiles, so growth infrastructure scores ~0.

> **RC1 corollary — greedy one-step evaluation.** The AI evaluates each
> improvement independently, *one step ahead, with one worker, against local tile
> yield*. Anything requiring a **sequence** of steps, a **set** of coordinated
> investments, **concurrent** effort, or an objective other than tile yield is
> invisible — not undervalued, but exactly zero or discounted to near-zero at
> every point along the path. (Farmland/Supermarket; transform chains; worker
> stacking.)

**RC2 — No shared plan.** Every actor decides independently, every turn, from
scratch. `find_something_to_kill()` builds a fresh pathfinder map *per unit*. The
ferry system's own docs call it *"statistical mechanics rather than logic."*

**RC3 — City-radius myopia.** The AI's world model is the union of its city work
radii. Roads, terraforming and (effectively) settling never look outside.

**RC4 — Incommensurable wants.** A dozen overlapping scales with magic caps.
**Three independent in-source admissions** of this, from three subsystems:
`README.AI`'s IDEA SPACE, `set_default_city_manager()` (`cityturn.c:324`), and
`want_tech_for_improvement_effect()` (`daicity.c:130`).

**RC5 — Planning and execution disagree.** Site *selection* prices travel as if
the map were empty and at peace, while *execution* applies risk costs. The AI
computes strategic priorities and then ignores them when assigning citizens
(`pcity->cm_parameter` is honoured and never set).

**RC6 — Omniscience substitutes for inference.** `H_MAP` and `H_FOG` are set at
exactly one difficulty level: `AWAY`, the "not playing" mode. And the information
handicaps only ever touched *tactics* — `dai_war_desire()` reads every enemy
city's exact size, surpluses, buildings, units, treasury and tech count at **every
difficulty**. Subsystems a strategic AI needs (memory, staleness, reconnaissance,
estimation) are missing *because omniscience made them unnecessary*.

**And RC6 makes the project unfalsifiable: you cannot evaluate a strategy layer
that is being fed the answers.**

---

## 3. The opportunity

An unusual amount of required infrastructure is **already built and abandoned in
place**. This is why the project is tractable.

| Infrastructure | State |
|---|---|
| `struct ai_type::funcs` vtable | Complete; `ai/tex` proves a second module works |
| `player_save` / `player_load` hooks | Exist; **commented out** in `classicai.c:628` |
| `struct player_tile` / `vision_site` | Full remembered map, maintained and saved, **never read by the AI** |
| `pcity->cm_parameter` | Honoured by `auto_arrange_workers`, **never set by the AI** |
| `is_pos_dangerous` PF callback | Documented, **assigned by nothing anywhere** |
| `adv->oceans[].size` | Computed every phase, **only `.threat` ever read** |
| `ai_invasion` counters | Built for enemy cities, **never for own** |
| `TIMING_LOG` + ~25 `AIT_*` buckets | Instrumented throughout, compiled out |
| `create_city_virtual()` | Used for site scoring, **never for strategy simulation** |
| `tile_virtual_new/destroy` | The correct purity pattern, used in one place |
| `struct req_context` | Threads through requirement eval; needs one added field |
| `fc_rand_state()` | RNG already a copyable value type |
| `assign_continent_numbers()` | Real component analysis, maintained on terrain change |
| `create <name> [ai type]` | **Mixed-module games already work and persist** |

Almost nothing here needs new infrastructure. It needs connecting infrastructure
that exists to decisions nobody wrote.

---

## 4. Architecture

**`ai/baseline`** — frozen copy of the stock AI at `0896b34`. Measurement control.
Never modified. Also lets you play against old-style and new-style AIs in the same
game, which works today via `create <name> <aitype>` and survives save/load.

**`ai/default`** — upstream's. Frozen except for `static` → non-static visibility
changes.

**`ai/deep`** — everything we build.

**Placement rule:** default is `ai/deep`. `ai/default` changes only for visibility.
`common/` and `server/advisors/` change only for zero-behaviour-change
infrastructure, or where a defect harms human players equally.

### The vtable constraint

Hooks are `phase_begin`, `first_activities`, `choose_building`, `settler_run`,
`unit_turn_end` — **empire-and-turn-level seams, not decision-level ones**.
`daisettler.h` exports six symbols; `cityresult_fill`, `city_desirability`,
`settler_map_iterate` and the tile cache are all `static`. `daicity.h` exports
`dai_city_want` but not `base_want`.

So changing site scoring means either (a) the visibility exception, or (b)
reimplementing ~1,400 lines. Prefer (a). Copying is affordable if refused — the
whole `ai/` + `advisors/` tree took only ~2,300 lines of churn in the past year,
so the originals barely move.

---

## 5. Why the hard rules exist

**Why `ai/baseline` is frozen.** The measurement plan rests on paired comparison.
If the control moves, every result becomes "better than a *different* old AI than
last month" — you lose attribution and the regression corpus needs regenerating
each time.

**Why `ai/default` isn't fixed in place.** Two reasons. Upstream review is a
bottleneck (one author writes 87% of commits; 2025 saw 591 commits total), so
making the strategy layer *depend* on upstream accepting tactical fixes would
block the project on someone else's decision. And behaviour changes there ripple
into stable branches, longturn communities, and CivRealm's published baselines —
freeciv-web builds the upstream C server, so `ai/default` changes propagate to the
research ecosystem automatically.

**Bug fixes are offered upstream in parallel, and nothing depends on them
landing.** If the inverted `result_defense_bonus` is never fixed upstream,
`ai/deep` doesn't have to carry the bug — but *not having it is work*, and this
document claimed the property before the work was done. **Correction
(2026-08-05, session D3):** `deep_city_desirability()` called stock's
`result_defense_bonus()` from the day the deep site scorer landed until D3
replaced it, so for months the example of a bug deep "simply doesn't have" was
a bug deep had. Reusing `ai/default` is the default and is usually right; what
it does *not* do is exempt the reused function from being read. The
[mechanism map](findings/mechanism-map.md) D-D found this by reading the call
site, which is the only way it can be found.

---

## 6. What "better" means

**Primary metric: score ratio in separate games on paired seeds.** Independent,
low variance under pairing, and not gameable by exploiting one specific opponent.

**Not head-to-head as the headline.** An AI tuned to beat `ai/default` specifically
can be worse in general — e.g. `ai/default` over-requisitions defenders, so an AI
that punishes exactly that looks brilliant against it and mediocre against
anything else. Guards: separate-game score as primary, **self-play in the suite**,
varied opponent mix.

**Always report held-out seeds.** Tuning and reporting on the same seeds is the
most common way game-AI claims turn out wrong.

**Sample sizes matter.** Unpaired at ~30% SD chasing a 5% effect needs ~550 games
per arm. Paired at ρ≈0.8 needs **~110**. Pair everything.

---

## 7. Phasing

| Phase | Contents | Why |
|---|---|---|
| **0** | Vendor `ai/baseline`; W00 harness + module skeleton; W25 tuning harness; visibility changes | You cannot measure without this. **Human-verified.** |
| **1** | W05, W11, W28, W06, W09-lite | **First demonstrable win** — none need the want refactor |
| **2** | W01 want unification → W02, W03, W29 | Economy: the biggest score mover |
| **3** | W27 scaling, W10 combat, W13 campaigns, W14 war ledger | Military effectiveness; unblocks large-game benchmarking |
| **4** | W04 knowledge, W08 doctrine, W07 geometry, W09 full, W12 empire graph | Strategy layer |
| **5** | W15–W24 | Breadth |
| **∥** | Upstream bug fixes | Parallel, decoupled, courtesy |

**Hard orderings** — violating these costs rework:

- W01 before W02/W03 (they manipulate want magnitudes)
- W05 before W07 (danger-blind routing + flattened distance discount = suicide missions)
- W06 before W04's flag flip (an honest AI that can't scout can't expand)
- W10 and W11 before W13 (`required_attack` inherits the combat model; drained cities fight campaigns)
- W26 before W20 and before memoization (evaluation must be pure)
- W03 before W17 (migration is gated on `city_can_grow_to`)
- W29 before W12.4 (bridges need chain-aware transform costing)
- W30 before plan-driven wants in W12, W13, W28
- W04 before W34 phase B (a victory plan is the most answer-fed component
  we could build; measuring it under omniscience measures the omniscience)

---

## 8. Expect ambiguity in early results

**Some fixes won't move score.** W05 cuts settler losses 60% — but if the AI was
replacing them anyway, score may barely move. Record the proxy metric *and* the
aggregate. Don't discard a correct fix because the headline was flat.

**Some fixes make things worse before better.** W06 builds explorers, which costs
shields that used to buy settlers. Expect a dip, measure it, and don't chase it
with a hasty counter-tune — it likely resolves once W08 can decide how much
exploration is worth.

This is why the *matrix* of configurations matters more than any single number. A
change that helps on large maps and hurts on small ones is information, not noise.
