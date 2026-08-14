# PRD v3 — A Tremendously Better Freeciv AI

**Supersedes PRD v1 and v2.** Consolidates all findings.
**Baseline:** `github.com/freeciv/freeciv` @ `0896b34` (master, Jul 2026;
commit dated 2026-07-23)
**License context:** Freeciv is GPLv2-or-later. All work must be GPLv2-compatible.

Every line reference in this document was read from the baseline tree. If a
reference doesn't match, the tree has moved — re-verify before acting.

> ## ⚠ Partially superseded — read this box before acting
>
> The **evidence** in this document (line references, in-source admissions,
> root-cause analysis) was re-verified against `0896b34` on 2026-07-24 and is
> accurate. The **placement and phasing directives** are not: they predate
> `findings/placement-revision.md`, which reverses §4.2, and they conflict with
> the operative working agreement (`CLAUDE.md`) and `00-context.md`.
>
> Where this document conflicts with those, **they win**. Concretely:
>
> 1. **`ai/default` is frozen** except for `static` → non-static visibility
>    changes (CLAUDE.md rule 2). Everything this document assigns to
>    `ai/default` in Phase 1 (W2, W3, W5, W6, W10, W11, W27) is built in
>    **`ai/deep`** instead. W28/W29/W30 remain legitimate `advisors` changes.
> 2. **Phase −1 / Appendix A is not a phase and not a dependency.** Upstream
>    bug-fix PRs are offered in parallel; nothing waits on them. `ai/deep`
>    implements correct behaviour regardless.
> 3. **§0.1's warning is inverted.** Implementing a tactical fix in the new
>    module is now the *correct* move, not the failure mode. Copying a function
>    into `ai/deep` is permitted as a last resort after the visibility
>    exception is considered (CLAUDE.md rule 3) — prefer making the `static`
>    original visible and calling it.
> 4. **Operative phasing is `00-context.md` §7** (Phase 1 = W05, W11, W28,
>    W06, W09-lite — first demonstrable win, none needing the want refactor),
>    tracked in `STATUS.md`. Part V's table and §0.3's phase annotations are
>    historical.
> 5. `ai/baseline` (a frozen vendored copy of the stock AI) is the measurement
>    control, letting `ai/default` stay usable for visibility changes without
>    moving the A/B control.

---

# Part 0 — How to use this document

## 0.1 If you are executing this

**Read Parts I–IV first.** They are short and they contain the reasoning that
makes the rest of the document interpretable. Do not start at a workstream.

**Then read only the workstream you are implementing**, plus its declared
dependencies (§0.3). Each workstream in Part VI is self-contained: problem,
evidence with line numbers, design, files touched, acceptance criteria, commits.

**Obey the placement rule — the revised one.** §4.2 below is superseded by
`findings/placement-revision.md` §3: **default placement is `ai/deep`**;
`ai/default` changes only for provably behaviour-neutral visibility; `advisors`
changes only for inert extension points or defects that harm human players
equally (W28, W29). Before copying a `static` function into `ai/deep`, check
whether the visibility exception lets you call it instead; copy only as a last
resort and say so in the commit message.

**One commit does one thing.** ≤~400 substantive lines, builds and passes tests
alone, describable in one line without "and". Title format:
`<area>: <imperative verb> <object>`.

**Verify before you claim.** Every workstream has mechanical acceptance criteria.
"It looks better" is not a result. Where a criterion requires batch games, run
them; where it requires a unit test, write it in the same commit stack.

## 0.2 What an agent will get wrong

Stated plainly, because these are the failure modes:

- **Tuning judgment.** Constants must be tuned against measured outcomes
  (Part VIII), not guessed. Do not invent numbers and declare success.
- **"Is this better?"** Freeciv is high-variance. A 5% score change over 10 games
  is noise. Use paired seeds and the stated sample sizes.
- **Scope creep across workstreams.** Each ships alone. Resist bundling, even
  when two changes touch the same file.
- **Assuming a ruleset.** Behaviour must adapt via the probe (W8.1), not
  hardcode civ2civ3.
- **Fixing symptoms.** If a change doesn't map to a root cause in Part II, it's
  probably the wrong change.

## 0.3 Dependency graph

```
PHASE -1  Appendix A bug fixes ─────────────── (no dependencies; do first)
              │
PHASE 0   W0 scaffolding+harness
          W26 purity ──────┐
          W1 want unification
          W25 tuning harness
              │
PHASE 1   W27 scaling ◄── W26         (ai/default: tactical correctness)
          W2 site eval ◄── W1
          W3 improvements + co-investment ◄── W1
          W5 settler routing
          W6 exploration
          W10 combat model
          W11 defence bookkeeping
          W28 worker stacking
          W29 transform chains
              │
PHASE 2   W30 extension points        (advisors: hooks for strategy)
              │
PHASE 3   W4 knowledge ◄── W6         (ai/deep: strategy layer)
          W8 doctrine ◄── W2,W3
          W7 geometry ◄── W5,W4,W8
          W9 CM params ◄── W1,W8
          W12 empire graph ◄── W30,W29
          W13 campaigns ◄── W10,W11,W30
          W14 war ledger ◄── W13
          W17 migration ◄── W3,W8
          W19 coalitions ◄── W13,W14,W15
              │
PHASE 4   W15 geography+allies, W16 diplomacy, W18 logistics,
          W20 threading ◄── W26,W27, W21 difficulty ◄── W4, W23 docs
              │
PHASE 5   W22 treaty obligations (optional, upstream-political)
          W24 conversational goals (optional)
```

**Hard orderings — violating these costs rework:**

| Must come first | Before | Why |
|---|---|---|
| W1 | everything with wants | RC4 blocks RC5 fixes |
| W26 | W20, W27 memoization | evaluation must be pure to cache or thread |
| W5 | W7 | danger-blind routing + flattened discount = suicide missions |
| W6 | W4's flag flip | an honest AI that can't scout can't expand |
| W10, W11 | W13 | `required_attack` inherits the combat model; drained cities fight campaigns |
| W3 | W17 | migration is gated on `city_can_grow_to` |
| W29 | W12.4 | bridges need chain-aware transform costing |
| W30 | W12, W13, W28 plan tiles | tagged wants need the hook |

---

# Part I — Thesis

## 1.1 The problem

The Freeciv AI is a competent tactical engine bolted to no strategy at all, and
it has been propped up by omniscience for so long that several subsystems a
strategic AI would need were never written.

It does not decide where its empire should be: site selection scores **two
tiles** with a distance discount so steep nothing beyond eight turns' walk is
reachable at any quality. It does not decide to build up rather than out:
improvement value is measured at today's city size, so an aqueduct scores
approximately zero. It does not decide to attack a city: every unit
independently picks its own target every turn from a fresh pathfinder map. It
does not decide to connect its cities: workers only look inside existing city
work radii. It does not decide who its friends are: diplomacy runs on an
affection counter that never learns whether a war is being won or lost. And it
does not need to *look* at anything, because at every difficulty anyone plays
against, it sees the whole map.

`doc/README.AI` says the codebase *"used to be in a bad shape but it has gotten a
lot better,"* which is true. It's readable, instrumented, has a clean module
vtable, and its authors left honest comments wherever they knew something was
wrong — we found eleven. The problem isn't craft. **A strategy layer was never
written, and omniscience hid the absence.**

## 1.2 What we are building

An AI that decides its empire shape and switches when the answer changes; builds
tall on purpose; stakes borders when land is contested and backfills when it
isn't; connects its cities deliberately; concentrates force and applies the whole
empire's production to one point of failure; knows when it is losing and sues,
pays, or evacuates; cooperates with allies and forms coalitions; **plays with the
same information a human has**, remembering and scouting and being deceivable;
and plays whatever ruleset it was given.

And which is fun to play against: legible, explicable, and beatable at chosen
difficulty by scaling *competence*, not *information*.

## 1.3 Non-goals

- Not a rewrite of `ai/default`. It stays, it stays working, and roughly half
  this document improves it directly.
- Not machine learning at runtime. Every decision explainable in a log line.
  (Offline constant tuning is in scope — Part VIII.)
- Not a ruleset. Behaviour adapts to rulesets; it doesn't require new ones.
- Not necessarily a protocol change. W22 is optional and separable.

---

# Part II — Root causes

Six defects. Every workstream attacks at least one. Fixing symptoms individually
will not compose; fixing these will.

## RC1 — No time horizon

The AI evaluates the present instant and nothing else.

- `base_want()` (`daicity.c:1530`) adds an improvement, measures the city **at
  current size with currently worked tiles**, subtracts. An aqueduct's entire
  value is future growth → measures ~0. Same for granary, harbour, sewer,
  supermarket.
- `cityresult_fill()` (`daisettler.c:275`) scores at current tech, terrain,
  government — no projection of irrigation, mines, rivers, or rail.
- `assess_danger()` (`daimilitary.c:709`) counts units near a city *now*, never
  "that neighbour has 40 shields/turn and is six tiles away."
- `city_migration_score`'s culture term compounds forever; the AI's culture
  valuation is three hardcoded constants (`daieffects.c:631–644`).

### RC1 corollary — greedy one-step evaluation

The AI evaluates each improvement independently, **one step ahead, with one
worker, against local tile yield.** Anything requiring a *sequence* of steps, a
*set* of coordinated investments, *concurrent* effort, or an objective other than
tile yield is invisible — not undervalued, but **exactly zero or discounted to
near-zero at every point along the path.**

Three confirmed instances:

- **Farmland/Supermarket.** The civ2civ3 effect requires `Extra Farmland Tile`
  **and** `Building Supermarket City`. `adv_calc_extra` measures farmland's delta
  as *exactly 0* without the building; `base_want` measures the building's delta
  as *exactly 0* without farmland. A mutual-zero deadlock. **W3's horizon fix
  does not solve this** — projecting zero over time is still zero.
- **Transform chains.** `adv_calc_transform` reads `old_terrain->transform_result`
  — one step. A canal through mountains is `Mountains→Hills→Plains→Lake`, 96
  worker-turns, and the *final* step destroys a good tile.
- **Worker stacking.** `total_activity()` sums across all units on a tile, so N
  workers is N× progress — but `autoworkers.c:490` `continue`s past any tile with
  a worker on it. One worker on a 96-turn job delivers **1.7% of nominal value**
  after `amortize`. Six deliver **50.6%**. The single-worker rule is a large part
  of why terraforming is inert.

## RC2 — No shared plan

Every actor decides independently, every turn, from scratch.

- `find_something_to_kill()` (`daiunit.c:1214`) builds a fresh PF map **per
  unit**. `README.AI`: *"Teach the AI to coordinate the units in an attack."*
- The ferry system, per its own docs: *"better described by statistical mechanics
  than by logic... Each turn both boats and passengers forget all about prior
  arrangements."*
- `adv_workers_road_bonus()` (`autoworkers.c:132`) is a 12-tile pattern matcher
  returning 0–8 with `/* FIXME: if you can understand the algorithm below please
  rewrite this explanation! */`. No concept of "city A", "city B", or "path".
- `assess_defense_backend()` counts only units **on** the city tile; defenders en
  route are invisible, so cities over-requisition at emergency priority. The
  equivalent accounting **exists for enemy cities** (`ai_invasion`).
- Emergencies resolved per-city in **list order** with no arbitration
  (`daicity.c:875`).
- `enum winning_strategy` exists (`daidata.h:31`); **`WIN_WAR` is never set.**

## RC3 — City-radius myopia

The AI's world model is the union of its city work radii.

- Worker search: `city_list_iterate` → `city_tile_iterate_index`
  (`autoworkers.c:471`). `README.AI`: *"AI won't build cross-country roads
  outside of city radii."*
- Terraforming is evaluated but only inside radii — so ocean→grassland bridging,
  fully supported (`[terrain_ocean] transform_result = "Grassland"`), is
  structurally unreachable.
- No chokepoint analysis. Two grep hits across `ai/` and `advisors/`, one being
  `daisettler.c:809` discarding the idea: *"We have an accidental land bridge.
  Ignore it."*
- No territory-claim term in site scoring.

## RC4 — Incommensurable wants

`FOOD_WEIGHTING 30`, `SHIELD_WEIGHTING 17`, `TRADE_WEIGHTING 18`,
`INFRA_WEIGHTING 13.5`, `PERFECTION 3`, `GROWTH_POTENTIAL_DEEMPHASIS 8`,
`DEFENSE_EMPHASIS 20`, `RESULT_IS_ENOUGH 250`, `DAI_WANT_DOMESTIC_MAX 750`,
`DAI_WANT_MILITARY_EMERGENCY 1000.1`, plus `adv->*_priority` on a *different*
scale, plus trait multipliers at `daidomestic.c:558` and `:620`, plus README's
conventions (`-199` = "get a boat"; `>200` capped) which it admits *"technology
and diplomats, in particular, seem to violate."*

**Three independent in-source admissions:**

1. `README.AI` IDEA SPACE — *"struct choice should have a priority indicator...
   remove the necessity to have want capped, thus reducing confusion."*
2. `set_default_city_manager()` (`cityturn.c:324`) — *"those values are on a
   different scale"*, and AI priorities were consequently **severed** from
   citizen arrangement.
3. `want_tech_for_improvement_effect()` (`daicity.c:130`) — *"the 'want' scales
   are unclear and kludged."*

Three authors, three subsystems, years apart, each routing around it locally.
**This is why W1 is Phase 0 and not cleanup.**

## RC5 — Planning and execution disagree

RC4's consequence.

- **Settler routing.** Site *selection* (`daisettler.c:954`) uses bare
  `pft_fill_unit_parameter` — no `get_TB`, no `get_EC`, no `adv_avoid_risks`. So
  travel is priced as if the map were empty and at peace, and that number feeds
  the discount that dominates the decision. *Execution* (`daitools.c:456`) does
  apply risk — but disables ZoC and fight avoidance for `long_path` cityfounders,
  i.e. exactly the dangerous trips. Workers get `no_fights` unconditionally; the
  overseas settler search sets `no_fights_or_unknown`. **Only the land settler
  search is bare.**
- **Citizen arrangement.** `set_default_city_manager()` sets factors for every AI
  city: food 10–20, shield 5, **trade 0**, gold 2, **luxury 0**, science 2,
  **happy_factor 0** — identical at war and peace. `auto_arrange_workers()`
  honours `pcity->cm_parameter`; **the AI never sets it.**
- **Combat model.** `kill_desire()` multiplies benefit by `victim_count` with **no
  `game.info.killstack` check**, and saturates where combat compounds. Defence
  aggregates superlinearly (`assess_defense_quadratic`), threat aggregates as a
  plain sum — so the AI correctly values its own concentration and systematically
  under-fears the enemy's.

## RC6 — Omniscience substitutes for inference

`ai/difficulty.c:52–133`:

| Level | Information handicaps |
|---|---|
| `AWAY` | `H_MAP`, `H_FOG`, `H_TARGETS`, `H_HUTS` |
| `NOVICE`/`RESTRICTED`/`EASY`/`NORMAL` | `H_TARGETS`, `H_HUTS` |
| `HARD`/`CHEATING`/`EXPERIMENTAL` | **none** (only `H_RATES`) |

**`H_MAP` and `H_FOG` are set at exactly one level — `AWAY`**, the "not playing"
mode. Omniscience is the default; blindness is a handicap applied to weaken it.

And the ~40 `omniscience` gates only touched **tactics**. Handicap-check counts:
`daidiplomacy.c` — 8, all `H_AWAY`/`H_DIPLOMACY`/`H_CEASEFIRE`, **no information
handicaps**; `daitech.c` — **0**; `daidata.c` — **0**. `dai_war_desire()` reads
every enemy city's exact size, surpluses and building list, every unit's type and
location, their treasury and exact tech count, **at every difficulty including
`AWAY`.**

The AI also **never** calls `map_get_player_tile()` / `map_get_player_city()`.

**Why this is a root cause:** subsystems a strategic AI needs are missing
*because omniscience made them unnecessary* — no memory of last-seen state, no
staleness reasoning, no reconnaissance want, no estimation under uncertainty. It
is why there is no explorer production want at all.

**And it makes the project unfalsifiable: you cannot evaluate a strategy layer
that is being fed the answers.**

---

# Part III — The opportunity

An unusual amount of required infrastructure is **already built and abandoned in
place.**

| Infrastructure | State | Used by |
|---|---|---|
| `struct ai_type::funcs` module vtable | Complete; `ai/tex` proves reuse | W0 |
| `player_save` / `player_load` hooks | Exist; **commented out** `classicai.c:628` | W0 |
| `struct player_tile` / `vision_site` remembered map | Fully maintained, saved, **never read by AI** | W4 |
| `pcity->cm_parameter` override | Honoured by `auto_arrange_workers`, **never set** | W9 |
| `is_pos_dangerous` PF callback | Documented, **assigned by nothing** | W5 |
| `adv->oceans[].size` | Computed every phase, **only `.threat` read** | W15 |
| `ai_invasion` reinforcement counters | Built for enemy cities, **never own** | W11 |
| `TIMING_LOG` + ~25 `AIT_*` buckets | Instrumented, compiled out | W0 |
| `create_city_virtual()` | Used for sites, **never for strategy simulation** | W8 |
| `tile_virtual_new/destroy` | The correct purity pattern, **used in one place** | W26 |
| `struct req_context` | Threads through requirement eval; needs one field | W26 |
| `fc_rand_state()` / `fc_rand_set_state()` | RNG already a copyable value type | W26 |
| `assign_continent_numbers()` | Real component analysis, maintained on terrain change | W4, W27 |
| `clause_info` + requirement vectors | Ruleset-declared, extensible by design | W22 |
| `packets.def` `add-cap()` | Documented and implemented in the generator | W22 |
| `enum winning_strategy` | `WIN_WAR` **never set** | W8 (replaces) |

Almost nothing here requires new infrastructure. It requires connecting
infrastructure that exists to decisions nobody wrote.

---

# Part IV — Architecture

## 4.1 Module, not fork

`struct ai_type::funcs` (`classicai.c:618–698`) is a real vtable; `ai/tex` proves
a second module can reuse `ai/default` internals. Registration is three lines in
`meson.build:285–303`.

But note the granularity: the hooks are `phase_begin`, `first_activities`,
`choose_building`, `settler_run`, `unit_turn_end` — **empire-and-turn-level
seams, not decision-level ones.** `daisettler.h` exports six symbols;
`cityresult_fill`, `city_desirability`, `settler_map_iterate` and the tile cache
are all `static`. `daicity.h` exports `dai_city_want` but not `base_want`.

**So changing how a city site is scored means reimplementing
`dai_auto_settler_run` and everything under it — ~1,400 lines, to change forty.**

That constraint dictates the split.

## 4.2 The placement rule — SUPERSEDED

> **This section is superseded by `findings/placement-revision.md` §3.** The
> revised rule: *default everything to `ai/deep`; `ai/default` only for pure
> visibility changes; `advisors`/`common` only for zero-behaviour-change
> infrastructure or defects harming humans equally — and `ai/deep` must never
> depend on an upstream change landing.* The original text is kept below for
> the reasoning it contains, which informed the reversal.

> **Does this change require state that only the new module has?**
>
> - **No** → fix it in `ai/default`, or lift the machinery into
>   `server/advisors/` or `common/`. Everyone benefits, and `ai/deep` inherits it
>   free instead of copying it.
> - **Yes** → `ai/deep`.

Consequence: **`ai/default` becomes a correct tactical engine; `ai/deep` becomes
the empire-level strategy layer on top of it.** Roughly 45% of what earlier
drafts assigned to `ai/deep` belongs in `ai/default`.

**Never copy a function into `ai/deep` in order to modify it.** That is forking
by increments.

### Where strategy must influence tactics: extension points

Four narrow hooks in shared code, replacing what would otherwise be duplication
(W30):

```c
/* server/advisors/autoworkers.h */
typedef adv_want (*adv_extra_tile_want_cb)(const struct tile *, void *ctx);
void adv_workers_set_plan_cb(adv_extra_tile_want_cb cb, void *ctx);
```

- **Plan-tile want callback** — lets the road planner, terraform plans,
  co-investment bundles and worker stacking all inject wants that survive local
  valuation saying "this tile is worthless." **One hook, four consumers.**
- **`campaign_hint` on `unit_ai`** — strategy suppresses rampage and pins units
  without owning unit movement.
- **`adv_choice` priority tier** — strategy injects empire-level production
  demand without owning `choose_building`.
- **Site-score callback** — contest pressure adjusts site value without owning
  `cityresult_fill`.

### Behaviour-change risk, and the graduation path

Changing `ai/default` changes the game for everyone: stable branches, longturn
communities, and CivRealm/freeciv-bot baselines calibrated against current
behaviour. (Note: freeciv-web builds the **upstream C server** with protocol
patches — so `ai/default` changes propagate there automatically, and a new
module appears once they bump their pinned commit.)

| Change class | Where | How |
|---|---|---|
| Bug fix — nobody defends current behaviour | `ai/default` | Immediately, standalone PRs |
| Shared machinery / extension points | `advisors`, `common` | Immediately, no behaviour change |
| Behaviour-changing tactical improvement | `ai/default` | **Proven in `ai/deep` first, then migrated** as a separate reviewed event |
| Empire-level strategy | `ai/deep` | Stays; may become default at a major version |

## 4.3 Want unification (W1)

Define one unit — *discounted shield-equivalent output over horizon H* — and
express every want in it:

```c
typedef double want_t;   /* shield-equivalents, discounted. No caps, no sentinels. */
```

Delete `DAI_WANT_DOMESTIC_MAX` and the `-199 == get_a_boat` sentinel (replace
with an explicit `adv_choice` field, per README's own suggestion). Replace the
`*_WEIGHTING` constants with conversion through actual production. Keep trait
multipliers as *preference*, not scale correction. Emergency becomes a
**priority tier**, not a want above 1000.

The payoff isn't tidiness — it's that RC5 becomes fixable. Once wants are in real
units, `set_default_city_manager()`'s comment stops being true and the AI can
drive its own citizen arrangement (W9).

## 4.4 State layout

```c
struct plr_ai_state {                 /* extends ai_plr */
  struct ruleset_probe   probe;       /* W8  — game start */
  struct knowledge       known;       /* W4  — recomputable from player_tile */
  struct doctrine        doctrine;    /* W8  */
  struct road_plan       roads;       /* W12 — recomputable */
  struct chokepoint_map  chokes;      /* W12 — recomputable */
  struct terraform_plan *terraform;   /* W12 — MUST persist (progress) */
  struct campaign       *campaigns;   /* W13 — MUST persist */
  struct war_ledger     *wars;        /* W14 — MUST persist */
  struct migration_intel migration;   /* W17 — recomputable */
  struct coalition      *coalition;   /* W19 — MUST persist */
};
```

**Persistence rule:** save what cannot be recomputed. Recompute the rest. Halves
the save surface.

---

# Part V — Phases and gates

> **Superseded.** Operative phasing is `00-context.md` §7, tracked in
> `STATUS.md`: Phase 0 = vendoring + harness + skeleton + tuning + Tier B
> visibility/purity; Phase 1 = W05, W11, W28, W06, W09-lite; W01 want
> unification moves to Phase 2. The table below is the pre-revision plan, kept
> because the *gates* remain the right mechanical checks for their workstreams.

Each phase has a **gate**: a mechanical condition that must hold before the next
phase begins.

| Phase | Contents | Gate |
|---|---|---|
| **−1** | Appendix A bug fixes (~25 standalone PRs to `ai/default`, `common`, `doc`) | All merged or submitted upstream |
| **0** | W0 scaffolding+harness, W26 purity, W1 want unification, W25 tuning | 50-seed paired batch shows `deep` ≡ `classic` within ±2%; state checksum assertion passes; tuning harness reproduces a run |
| **1** | W27 scaling, W2, W3, W5, W6, W10, W11, W28, W29 — all `ai/default` | Each workstream's acceptance met on held-out seeds; turn time on the 20-player case reduced ≥50% |
| **2** | W30 extension points (`advisors`) | No behaviour change; all four hooks exercised by a test |
| **3** | W4, W7, W8, W9, W12, W13, W14, W17, W19 — `ai/deep` strategy layer | Each acceptance met; no campaign deadlocks in 50 games |
| **4** | W15, W16, W18, W20, W21, W23 | Difficulty tiers measurably distinct in win rate |
| **5** | W22, W24 — optional | — |

**Ship Phase 1 alone and the AI is dramatically better, with no new module and no
opt-in required.** That is the minimum viable outcome and it should be treated as
the primary goal.

---

# Part VI — Workstreams

Each: **problem → evidence → design → files → acceptance → commits.**

## W0 — Scaffolding and harness

**Deliverable.** `ai/deep` forwarding everything to `dai_*`; persistence wired;
batch harness; profiler enabled.

**Evidence.** `ai->funcs.player_save` / `player_load` are **commented out** at
`classicai.c:628–629`, so `struct ai_plr` including `tech_want[]` doesn't persist
today.

**Files.** `ai/deep/*` (new), `meson.build`, `server/srv_log.c`, `tests/*`.

**Acceptance.** `set aitype deep` produces score distributions statistically
indistinguishable from `classic` over 50 paired seeds. All state round-trips
through save/load. `AIT_*` timings dump on demand.

```
build: add 'deep' to the aimodules option
ai/deep: add module skeleton with capability string and self pointer
ai/deep: forward player, city and unit lifecycle hooks to default AI
ai/deep: forward build advisor and settler hooks to default AI
ai/deep: forward diplomacy and turn-phase hooks to default AI
ai/deep: add per-player state struct and allocation hooks
ai/deep: wire player_save and player_load hooks
ai/deep: persist per-player state with defaulted missing keys
server: allow enabling AI timing buckets via a server setting
test: add headless batch runner with fixed map and RNG seeds
test: add paired batch mode using identical seed lists across arms
test: extract per-player score and city metrics to CSV
test: add A/B mode running deep against classic in one game
test: add saved-position corpus loader and assertion framework
test: add corpus positions for contested border, losing war, hut-rich start
ai/deep: log a per-turn decision vector line per player
test: assert AI state round-trips through save and reload
doc: document the deep AI module and how to select it
```

## W26 — Planning purity  *(Phase 0 — promoted; gates W20 and W27)*

**Problem.** The codebase's idiom for counterfactual evaluation is **"mutate
shared state, evaluate, restore."** This blocks memoization *and* threading, and
is a latent correctness bug today.

**Evidence — complete inventory (10 AI-side sites, 8 server-side):**

| Site | Mutates |
|---|---|
| `daisettler.c:292, 455` | `pplayer->government` |
| `daisettler.c:301, 458` | `tile_set_owner()` — **map-global** |
| `daicity.c:1556, 1566` | city improvements |
| `daicity.c:1553, 1573` | `pplayer->wonders[]` |
| `daicity.c` same block | `game.info.great_wonder_owners[]` — **game-global** |
| `daitech.c:243` | `game.info.global_advance_count` — **game-global** |
| `daidata.c:546`, `advdata.c:1030/1067`, `daidiplomacy.c:1410–1421` | `pplayer->government` |
| `citymap.c:53` | `static int *citymap` — **process-global** |
| **`requirements.c:6756–6771`** | **global mutex + list scan per `is_req_active()`, result never used** |
| `cm.c:915–916` | `compare_key`, `compare_key_trade_bonus` — qsort comparator globals |
| `rand.c:85` | `static RANDOM_STATE rand_state` |
| `srv_log.c:44–45` | `aitimer[][]`, `recursion[]` — timing buckets |
| `citytools.c:98,101`, `cityturn.c:88,94`, `maphand.c:63` | deferred-work queues + `send_*_suppressed` flags |

**The `is_req_active` block is the priority.** It takes a global mutex and scans
a list on **every requirement evaluation** to produce a `trdata` pointer that is
**never read**. It is dead weight single-threaded and total serialisation
threaded. **Delete it before profiling anything.**

**Design.** Government → optional `const struct government *` on `req_context`;
`is_gov_req_active` is a single chokepoint, plus one direct read at
`common/city.c:2319`. Improvements → `city_virtual_copy()`, mirroring
`tile_virtual_new()`. Tile ownership → owner parameter on `city_waste()`. Globals
→ parameters. `citymap`, RNG, timers → per-player/per-thread.

**Verified safe, do not touch:** `city_map_index`/`city_map_xy`/
`city_map_numtiles` (`city.c:60–67`), `dir_cardinality`/`dir_validity`
(`map.c:88–89`), `improvement_types` (`improvement.c:41`) — all init-once,
read-only after ruleset load.

**Acceptance.** Debug-build **game-state checksum** around the planning phase
asserts unchanged. This catches unknown unknowns and is the real deliverable.

```
common: remove the unused per-thread requirement data lookup
common: add an optional government override to the requirement context
common: honour the government override in the government requirement evaluator
common: honour the government override in the revolution check
ai/default: pass the goal government by context instead of assigning to the player
advisors: pass the goal government by context instead of assigning to the player
common: add city_virtual_copy mirroring tile_virtual_new
ai/default: evaluate improvement want against a city copy
ai/default: drop the wonder-ownership writes now made unnecessary
common: give city_waste an explicit owner parameter
ai/default: pass virtual tile ownership to waste instead of setting tile owner
ai/default: pass hypothetical advance count instead of writing game state
common: pass comparator context explicitly in city management sorting
common: move the citymap reservation array into per-player AI state
utility: give each player an independent random state
server: make AI timing buckets per-thread
server: make city refresh and worker arrange queues thread-local
server: audit lazily initialised caches in common for init-once safety
common: add a game-state checksum for debug assertions
test: assert AI planning leaves game state unchanged
```

## W1 — Want unification  *(Phase 0)*

See §4.3. **Acceptance:** no behavioural regression vs W0 baseline (±2% over 50
paired seeds); no want site depends on a magic constant; `adv_choice` carries an
explicit priority tier.

```
advisors: introduce want_t and document shield-equivalent semantics
advisors: add output-to-shield-equivalent conversion helpers
advisors: add a priority tier field to adv_choice
ai/default: express settler want in shield-equivalents
ai/default: express worker want in shield-equivalents
ai/default: express building want in shield-equivalents
ai/default: express military want in shield-equivalents
ai/default: express caravan and trade-route want in shield-equivalents
ai/default: express tech want in shield-equivalents
ai/default: replace the get-a-boat want sentinel with an explicit choice field
ai/default: replace the domestic want cap with priority tiers
ai/default: route emergency production through priority tiers, not want magnitude
ai/default: apply personality traits as preference rather than scale correction
test: assert no want site depends on a magic constant
```

## W25 — Tuning harness  *(Phase 0)*

**Design.** Constants as **data, not code** — a `struct tuning` loaded from a
file named by a server setting, with compiled-in defaults. The optimiser writes
config files; the game reads them. No rebuild per trial. Content-addressed result
cache keyed `(tuning_hash, scenario, seed)` so reruns skip completed cells.
CMA-ES over ~30 continuous parameters, **tuned six at a time per workstream**,
never globally. **Hold out seeds** — tune on 1–100, report only 101–200.

**Acceptance.** A tuning run is resumable after kill; held-out metrics reported
separately; `tuning_hash` appears in the decision log.

```
ai/default: extract tunable constants into a tuning struct with defaults
ai/default: load tuning parameters from a file named by a server setting
ai/default: emit the tuning hash in the decision log
test: add a content-addressed result cache keyed on tuning hash
test: add CMA-ES driver over the tuning struct
test: split seeds into tuning and held-out validation sets
test: report held-out metrics only
doc: document the tuning workflow and per-workstream parameter groups
```

## W27 — Scaling  *(revised placement: `ai/deep`; do early — addresses multi-minute turns)*

**Problem.** At ~20 AIs / ~2000 cities, turns take minutes. Two quadratic terms.

**Evidence.**
- `assess_danger()` (`daimilitary.c:827`): per city, `players_iterate` →
  `unit_list_iterate` over every unit in the game. **O(own_cities × all_units),
  every turn, no cadence.** ~10M unit-vs-city evaluations per turn at this scale.
- `base_want()` (`daicity.c:1559`): `city_range_iterate` with **player range**
  walks *every city the player owns*, and `base_want` is called per improvement
  per city. **O(own_cities² × player-range improvements).** ~250,000
  `dai_city_want` calls per player per recalc at 100 cities.

**Design.**
- **The 100× fix:** for a player-range improvement the empire-wide effect is
  largely the same regardless of *which* city builds it. Evaluate the empire
  delta **once**, then add per-city local terms. **C² → C.** Also a better model
  — it separates "is this worth building" from "where should we build it."
- **Threat index:** build a per-turn spatial index of threatening units, query
  per city. `own_cities × all_units` → `all_units + own_cities × nearby_units`.
- **Memoization:** possible only after W26. Cache `dai_city_want` per (city,
  turn); mirror the server's own `mgr_score_calc_turn` pattern.
- **Reachability early-out:** `assign_continent_numbers()` already exists and is
  maintained on terrain change — but continent numbers are *global truth*, so
  under W4 the AI must use a per-player known-tile component index instead (W4).

**Acceptance.** Turn time on the 20-player/2000-city case down ≥50%. A scaling
benchmark in CI asserting sub-quadratic growth in city count.

```
ai/default: evaluate player-range improvement want once per empire, not per city
ai/default: separate empire-wide improvement value from city-local placement value
ai/default: build a per-turn spatial index of threatening units
ai/default: query the threat index per city instead of scanning all units
ai/default: memoize city want per city per turn
ai/default: reject unreachable targets before building a pathfinder map
common: add a turn-scoped pathfinder map cache keyed on parameter identity
ai/default: reuse cached pathfinder maps for units sharing start and parameters
common: flush pathfinder caches at turn change
test: add a 20-player 2000-city scaling benchmark
test: assert per-turn AI time scales sub-quadratically in city count
```

## W2 — Site evaluation  *(revised placement: `ai/deep`)*

**Evidence.** `cityresult_fill()` scores `city_center.sum + best_other.sum` —
**two tiles** — plus everything else divided by `GROWTH_POTENTIAL_DEEMPHASIS = 8`.
Line 407: *"Baseline is best extra tile only. This is why making new cities is so
darn good."* `ptdc->sum *= 2` if `food >= 2` makes it a grassland-seeking missile.

**Design.** Score all workable tiles weighted by probability the city works them
(`w[i] = i < target_size ? 1.0 : 0.15`, `target_size` from cumulative food).
Explicit specials term under `adv->goal.govt.gov`, with cluster bonus. Potential
term via speculative `adv_calc_extra()`. Retire `GROWTH_POTENTIAL_DEEMPHASIS`.
Marginal-city diminishing returns against available land and frontier waste.

**Acceptance.** Mean city size at T150 up ≥40%; city count down; the AI stops
founding when the marginal city's discounted output falls below the marginal
improvement in its best city.

```
ai/default: score all workable tiles instead of centre plus best other
ai/default: weight catchment tiles by probability the city works them
ai/default: derive target city size from cumulative food surplus
ai/default: retire the growth-potential deemphasis divisor
ai/default: add an explicit resource-special term to site scoring
ai/default: reward special clusters within a single catchment
ai/default: value post-improvement tile yield discounted by build time
ai/default: score specials under the government the player is heading toward
ai/default: compute a per-empire marginal settler value
ai/default: decay founder want as city count rises against available land
test: add metric — mean city size and city count at turn 150
```

## W3 — Horizon-aware improvements and co-investment  *(revised placement: `ai/deep`)*

**Two problems.** (a) Improvement value measured at today's size → aqueducts,
granaries, harbours, sewers score ~0. (b) **Co-investment deadlock:** the
civ2civ3 farmland effect requires `Extra Farmland Tile` **and** `Building
Supermarket City`; each side measures the other's absence as **exactly zero**.
Horizon projection does not fix (b) — zero times a horizon is zero.

**Design (a).** Measure at `min(size + growth_over_H, cap_with_this_building)`
and `amortize()`. Capacity-gate bonus: a building lifting a hard size cap gets
credit for the whole blocked growth curve.

**Design (b).** Detect bundles at probe time by scanning effect requirement
vectors for one containing **both** a `Building` at `City` range **and** an
`Extra` at `Tile` range — mechanical and ruleset-general. Value the **bundle**
jointly: `(output with both − output with neither) / (building cost + min_tiles ×
extra turns)`. Emit *both* wants together, with the worker want **tagged** (W30)
so it survives local zero valuation. Schedule the overlap so they complete
together — start tile work at ~60% building completion.

**Do not special-case farmland.** Any ruleset can create more of these.

**Acceptance.** Aqueducts/harbours/granaries/sewers appear at appropriate times.
**Supermarket gets built at all** — closes a README bug. Farmland appears *before*
supermarket completion.

```
ai/default: project city size forward when valuing improvements
ai/default: credit capacity-gating buildings for unlocked growth
ai/default: amortize projected improvement value over the planning horizon
ai/default: detect co-investment bundles from ruleset effect requirements
ai/default: value building and tile-extra bundles jointly
ai/default: emit paired building and worker wants for a chosen bundle
ai/default: start bundle tile work once the building is mostly complete
test: add metric — turn of first aqueduct, harbour, granary, sewer
test: assert farmland is built ahead of supermarket completion
test: assert supermarket is built in a farmland-viable city
doc: correct the stale supermarket entry in README.AI
```

## W5 — Settler routing  *(revised placement: `ai/deep`; MUST precede W7)*

**Evidence.** Land search parameter is `pft_fill_unit_parameter` + omniscience —
no `get_TB`, no `get_EC`, no `adv_avoid_risks`. Meanwhile the *worker* search
sets `get_TB = autoworker_tile_behavior` (`TB_IGNORE` for non-allied territory)
and the *overseas settler* search sets `no_fights_or_unknown`. Execution inverts
protection: with `LONG_TIME 4`, trips over 4 tiles get `get_zoc = nullptr` and
cityfounders fall into a branch whose body is a comment saying *"ignore the
presence of enemy units."* `is_pos_dangerous` is assigned by **nothing anywhere**.
`dai_gothere_bodyguard()` counts hostiles **on the destination tile only**.

**Acceptance.** Settler loss rate to hostiles down ≥60%.

```
ai/default: add a settler tile-behaviour callback mirroring the worker one
ai/default: apply risk avoidance to the settler site search parameter
ai/default: raise settler fearfulness above the default stacking value
ai/default: implement is_pos_dangerous for civilian pathfinding
ai/default: give long-path city founders intermediate fight avoidance
ai/default: stop disabling zone of control for civilian long paths
ai/default: sample path danger when requesting a settler bodyguard
test: add metric — settlers lost to hostiles per 100 turns
```

## W6 — Exploration and huts  *(revised placement: `ai/deep`; MUST precede W4's flag flip)*

**Evidence.** There is **no explorer production want anywhere** in
`ai/default/`. `daicity.c:542` **actively disbands** explorers that come home,
under `FIXME: This is a hack`. `HUT_SCORE = BEST_NORMAL_TILE + 2` — a tiebreaker
— applied only to **already-known** tiles.

**Acceptance.** In a hut-heavy config, techs-from-huts by T50 up ≥3×; explored
fraction at T80 up ≥50%; **no explorer disbanded while `explore_want > 0`.**

```
ai/default: add an explorer production want
ai/default: scale explorer want by hut density and unexplored area
ai/default: stop disbanding explorers while exploration is wanted
ai/default: make the hut exploration bonus proportional to expected value
ai/default: value expected huts in unexplored territory
test: add metric — techs from huts and explored fraction by turn 80
```

## W10 — Combat model  *(revised placement: `ai/deep`; MUST precede W13)*

**Evidence.** `kill_desire()` (`daiunit.c:341`) multiplies benefit by
`victim_count` with **no `game.info.killstack` check** and saturates where
sequential combat compounds. `total_danger += vulnerability` (`daimilitary.c:939`)
aggregates linearly while `assess_defense_quadratic` aggregates superlinearly.

**Acceptance.** With `killstack` off, attacks on multi-unit stacks down ≥50%;
units lost per kill down; AI reacts to massed enemy stacks earlier.

```
ai/default: gate multi-victim benefit on the killstack setting
ai/default: model sequential combat as compounding rather than saturating
advisors: extract the superlinear force aggregation used for defence
ai/default: aggregate threat superlinearly to match defence
test: add metric — attacks on multi-unit stacks with killstack disabled
test: add corpus position — massed enemy stack near an underdefended city
```

## W11 — Defence bookkeeping  *(revised placement: `ai/deep`; MUST precede W13)*

**Evidence.** `assess_defense_backend()` iterates only `pcity->tile->units` — en
route defenders invisible, so cities over-requisition at
`DAI_WANT_MILITARY_EMERGENCY`. Equivalent accounting **exists for enemy cities**
(`ai_invasion`). `dai_manage_cities()` (`daicity.c:875`) resolves emergencies in
**list order** with no arbitration, competing for the same gold.

**Acceptance.** Defender over-build down ≥40%; no production oscillation over 50
games.

```
ai/default: add an incoming defence counter per owned city
ai/default: credit defenders en route against assessed city danger
ai/default: weight incoming defence by arrival time and survival odds
ai/default: collect city emergencies before resolving any of them
ai/default: rank emergencies by urgency, worth and strategic role
ai/default: resolve emergencies against a shared gold budget
test: add metric — defenders produced beyond assessed need
```

## W28 — Worker stacking  *(Phase 1, `advisors`)*

**Evidence.** `total_activity()` sums `activity_count` across **all units on the
tile** — N workers is N× progress, and late arrivals contribute from arrival.
But `autoworkers.c:490` **`continue`s** past any tile with one of your workers on
it, and `struct workermap { int enroute; int eta; }` can express only one
assignment.

**Why it's a value bug:** `amortize` discounts `(23/24)` per turn. A 96-worker-turn
chain: 1 worker → **0.017** of nominal; 6 workers → **0.506**. The single-worker
rule is a large part of why terraforming is inert.

**Design.** Widen `workermap` to hold multiple assignments and a
`rate_committed`. Value the Nth worker by completion time saved:
`amortize(V, ceil(rem/(rate+r))) − amortize(V, ceil(rem/rate))`. **Snap N to a
divisor** of remaining work — waste is `N × ceil(T/N) − T`, zero when `N | T`.
Weight by `get_activity_rate()` (scales with `move_rate` and veteran level, so
Engineers count double). Cap by danger (`adv_danger_at`). **Stack on plan tiles;
spread on opportunistic improvement.**

Shared with human auto-workers, who also refuse to stack — this is a defect for
them too.

```
advisors: let the worker map hold multiple assignments per tile
advisors: track committed activity rate per tile instead of a single unit
advisors: allow workers to consider tiles other workers are already working
advisors: value an additional worker by the completion time it saves
advisors: prefer worker counts that divide the remaining activity evenly
advisors: weight workers by activity rate rather than counting units
advisors: cap worker stacking near danger
advisors: prefer stacking on plan tiles over opportunistic improvements
test: assert three workers complete a transform in a third of the turns
test: assert stacking is declined when the marginal turn saving is partial
```

## W29 — Transform chains  *(Phase 1, `advisors`; MUST precede W12.4)*

**Evidence.** `adv_calc_transform()` (`infracache.c:142`) reads
`old_terrain->transform_result` — **one step** — and scores with
`city_tile_value` — **tile yield only** — and takes a `struct city *`, so it's
city-radius bound.

Chains to water: Grassland/Plains 1 step 36wt; Hills 2 steps 60wt; Mountains 3
steps 96wt; Jungle 4 steps 108wt. Ocean→Grassland 1 step 36wt; Deep Ocean `"no"`.

**Design.** Precompute all-pairs shortest paths over the transform graph at
ruleset load — a dozen terrains, trivial, static. Expose "cheapest path to
terrain X and its cost." **This fixes an ordinary bug today:** the AI won't
transform Mountains toward Plains even when Plains is what it wants, because step
one looks unappealing alone.

```
advisors: precompute all-pairs terrain transform distances at ruleset load
advisors: expose cheapest transform path and cost between two terrains
ai/default: value terraforming against the best reachable terrain, not the next one
test: assert mountains are transformed toward plains when plains is wanted
```

## W30 — Extension points  *(Phase 2, `advisors`)*

**No behaviour change.** Four hooks (§4.2) enabling strategy to steer tactics
without duplication.

```
advisors: add a plan-driven tile want callback to the worker search
advisors: let plan-driven tile wants survive local zero valuation
advisors: add a campaign hint field to unit AI data
advisors: honour adv_choice priority tiers in production selection
advisors: add a site-score adjustment callback to settler evaluation
test: exercise each extension point with a stub consumer
```

## W4 — Knowledge layer / honest information  *(Phase 3, `ai/deep`)*

**Design.** Read `map_get_player_tile()` / `map_get_player_city()` instead of
ground truth. Build `known_city` / `known_force` per enemy each phase. **Staleness
projection** off `last_updated`: project size forward; project defence via
`dai_choose_defender_versus()` extended over elapsed turns; decay unit-position
confidence by age × move rate. **Reconnaissance want** =
`intel_staleness × planned_action_value / distance`. **Priors over unknown** from
generator settings, hut density, claim value. **Honest `dai_war_desire`** with an
uncertainty penalty — discount uncertain gains, inflate uncertain threats; tech
and treasury knowable *with an embassy*.

**Per-player known-tile component index** (per unit class), incrementally unioned
as tiles are revealed. `assign_continent_numbers()` exists but is global truth —
reading it for unseen tiles is a leak.

**Pathfinding constraint:** **share the primitives, never the products.** A shared
move-cost table is safe (gated access); shared distances, paths or reachability
are not — the number itself encodes unseen terrain. Two invalidation clocks: the
shared table on *actual* change, per-AI caches on *observed* change.

**Acceptance.** With `H_MAP`/`H_FOG`/`H_TARGETS` at all levels, `deep` scores
within 10% of its omniscient baseline. Declines to attack unscouted neighbours.
A human feint measurably changes behaviour. **Test: assert an AI cannot path
through terrain it has not observed.**

```
ai/deep: add a per-enemy knowledge struct for cities and forces
ai/deep: populate known cities and forces from the remembered map
ai/deep: record intel age from tile last-updated stamps
ai/deep: project remembered city size and defence forward from intel age
ai/deep: decay remembered unit position confidence by age and move rate
common: add a per-player known-tile connectivity index per unit class
common: maintain the connectivity index incrementally as tiles are revealed
ai/deep: source target selection and danger from remembered state
ai/deep: rebuild war desire from remembered state with an uncertainty penalty
ai/deep: read tech and treasury from embassies rather than ground truth
ai/deep: want reconnaissance proportional to intel staleness and planned action
ai/deep: value unexplored territory using generator and hut density priors
ai/deep: add an honest-information setting defaulting to off
ai/deep: apply map, fog and target handicaps under honest information
test: assert an AI cannot path through terrain it has not observed
test: add corpus position — hidden stack behind a feint
ai/deep: enable honest information by default
```

## W7, W8, W9, W12, W13, W14, W15, W16, W17, W18, W19, W20, W21, W22, W23, W24

*(Unchanged in substance from PRD v2 Part IV; see that document for full design
text. Summarised here with placement and dependency corrections.)*

- **W7 Expansion geometry** (`ai/deep`, needs W5+W4+W8). Contest pressure; claim
  and denial terms; `effective_perfection = PERFECTION × (1 − 0.6 × pressure)`
  moving the 10-turn requirement from 3.59× to 1.63×; gate the early break.
- **W8 Doctrine + ruleset probe** (`ai/deep`). Three orthogonal axes; probe via
  `create_city_virtual()`; hysteresis ≥15% over two probes; replaces the
  vestigial `winning_strategy`. Enrich is half-built — `ACTION_JOIN_CITY` works,
  `disband_city()` works, the AI has never used either.
- **W9 CM parameters** (`ai/deep`). **Highest leverage-to-effort item.**
  `pcity->cm_parameter` is honoured and never set. Note `happy_factor = 0`
  currently fights the migration score.
- **W12 Empire graph** (`ai/deep`, needs W30+W29). Roads/rail/chokepoints/land
  bridges/canals as one planner. **Chokepoint analysis is the most genuinely
  novel item here.** Terraform plans are *ordered sequences* —
  `terrain_surroundings_allow_change` means you build outward from shore, never
  from the middle.
- **W13 Campaigns** (`ai/deep`, needs W10+W11+W30). Muster/move/assault with a
  recon precondition; production feedback; abort conditions including "no viable
  path for N turns"; **largest CPU win** (O(units) → O(1) target ranking).
- **W14 War ledger** (`ai/deep`). The AI currently sues for peace when it *likes*
  you, not when it's losing — the trigger resolves to `greed(love)`.
- **W15 Geography and allies** (split: `ai/default` for the bug fixes,
  `ai/deep` for doctrine-coupled parts). Includes the inverted
  `result_defense_bonus`, the shore-defence pond fix using the never-read
  `adv->oceans[].size`, allied danger assessment, allied bodyguards, tech gifting
  to non-team allies. **Correction: healing in allied cities already works.**
- **W16 Diplomatic legibility** (`ai/default`). The missing call-to-arms
  retraction (42 notify calls, none stands down); the two-line guard against
  demanding war from an ally allied with your target; the likely-dead
  `ally_patience` ladder — **verify with a log line**.
- **W17 Migration** (`ai/deep`, needs W3). Poaching needs **3.3×–9×** the
  neighbour's score, so it *is* the largepox strategy. **Culture is the only
  unbounded term.** Three silent gates. An AI that correctly declines to chase
  migration on a default server passes.
- **W18 Logistics** (`ai/deep`). Ferries; boat bridges — **prototype the PF
  routing first**, since if PF won't route over a transport chain nothing else
  matters.
- **W19 Coalitions Tier 1** (`ai/deep`). Contribution in gold-equivalent effort so
  tribute and fighting are substitutes; suspend leader obligations at two missed
  intervals. Heed README's warning that diplomacy usually gains humans, not AIs.
- **W20 Threading** (needs W26+W27). Fan-out per-city/per-unit evaluation → serial
  empire decision → serial execution in fixed order. `invasion` counters are a
  **reduction** — per-thread partials merged serially, not a lock.
- **W21 Difficulty** (needs W4). Scale competence, not information: horizon, force
  margin, coordination fidelity, staleness tolerance. Keep `CHEATING` as an
  honestly-named omniscient level.
- **W22 Treaty obligations** (optional). `clause_info.enabled` + `add-cap()` make
  it feasible; **escrow beats reputation**; reuse `requirement_vector` as the
  fulfilment condition; **client UI across four toolkits likely exceeds all
  server-side work.**
- **W23 Documentation.** Three stale README entries confirmed:
  `locally_zero_minimap` (identifier removed), `military_amortize` (*"complete
  mystery"* — it's now documented and straightforward), and the supermarket entry
  pointing at `farmland_food()` in `advdomestic.c` — **both symbols and the file
  no longer exist**.
- **W24 Conversational goals** (optional, last). The AI is currently **deaf** —
  `handle_chat_msg_req` has zero `is_ai` checks. Build the grammar and pipeline
  with a **regex baseline first**; the model is a swap-in for one component.
  Closed 12-kind grammar; **the model can produce a wrong request, never a wrong
  decision.** For parsing, a fine-tuned encoder classifier (~20–100MB) likely
  beats a generative model. For generation, OLMo 2 1B or SmolLM3-3B — both
  Apache 2.0, both OSAID-aligned.

---

# Part VII — Cross-cutting

**Ruleset adaptivity (W8.1, before W2).** Probe at game start: settler pop cost;
flat-per-city vs percentage-per-citizen effects; corruption shape; growth caps;
`culture_migration_pml`; `mgr_*`; hut density; `killstack`; whether ocean
transforms; `airliftingstyle`; enabled clause types; co-investment bundles;
transform graph distances.

**Persistence.** Wire `player_save`/`player_load` day one. Save what can't be
recomputed.

**Determinism.** Seed `fc_rand` and record it. Keep a single-threaded mode. Log
the decision vector every turn — with this many interacting subsystems it's the
only way to answer "why did it do that on turn 140."

---

# Part VIII — Measurement

**Score is a terrible tuning target** — terminal, aggregated, high variance. Each
workstream tunes against a proxy metric that moves by T100–150. Score is the
acceptance gate, never the objective.

**Pair every comparison on identical seeds.** Unpaired at 30% SD chasing a 5%
effect needs ~550 games per arm; paired at ρ≈0.8 needs **~110**. That's the
difference between infeasible and overnight.

**The eval pyramid:**

| Level | What | Cost | Runs on |
|---|---|---|---|
| L0 | Unit tests on pure functions | µs | every commit |
| L1 | Position assertions (load save, one turn, assert a decision) | s | every commit |
| L2 | Short scripted games, 50–80 turns | min | every epic |
| L3 | Full paired batches, 250 turns × 100+ seeds | hrs | every workstream |

**L0/L1 carry most of the value.** Property tests worth writing: monotonicity
(more food never lowers a site score), round-tripping want conversions,
boundedness, superset catchment ≥ subset. **Golden want-vector snapshots** for
fixture positions — any diff must be intentional.

One free strong test: the W17 migration-score estimator must agree **exactly**
with the server's `city_migration_score()` on fixtures.

---

# Part IX — Risks

| Risk | Mitigation |
|---|---|
| Want refactor destabilises everything | Phase 0. Classic stays as A/B control. Each conversion commit independently revertible. |
| Honest information tanks strength | W6 first; flag flips only within 10% of omniscient baseline; it's a setting, reversible in the field. |
| Scaling fixes insufficient | Profile before and after each of W27's three changes; the `is_req_active` mutex deletion is free and may be large. |
| AI becomes unbeatable or unfun | W21 is a workstream. W16's narration makes behaviour legible. |
| Ruleset over-fitting | W8.1 probe lands before W2. |
| Boat bridges don't work | Prototype PF routing first — cheapest disproof. |
| Treaty extension rejected upstream | W19 delivers most coalition value with zero protocol change. |
| Threading nondeterminism | Single-threaded test mode; W20 is Phase 4. |
| Bus factor (87% one author) | Phase −1 exists to earn review bandwidth. |
| Scope | Every workstream ships alone. Resist bundling. |

---

# Appendix A — Upstream bug candidates *(parallel track — nothing depends on these)*

Standalone, mostly small, no PRD buy-in required. Offered upstream as courtesy
PRs and to earn review bandwidth; `ai/deep` implements the correct behaviour
regardless of whether any of them land (`placement-revision.md` §Tier A). Do
**not** fix these in `ai/default` within this tree — record them in
`findings/upstream-bugs.md` per CLAUDE.md rule 2.

1. `result_defense_bonus()` inverted (`daisettler.c:596`) — integer division makes
   higher terrain defence score *lower*; contradicts its own docstring. Grassland
   39.9 vs mountains 20.0.
2. `ally_patience` nag ladder appears **unreachable** (`daidiplomacy.c:1858`) —
   `spam` reset to ≥3 at line 1822 on every path. Verify with a log line.
3. Comment/code mismatch at `daidiplomacy.c:1823–1826`.
4. `WIN_WAR` declared and never set (`daidata.h:33`).
5. `is_pos_dangerous` never assigned by any AI code.
6. No retraction of a call to arms — 42 notify calls, none stands down.
7. `war_countdown()` demands war from allies allied with the target;
   `is_allied_with_enemy` detects exactly that and only increases resentment.
8. Settler search omits the tile-behaviour callback that workers and the overseas
   settler search both set.
9. Supermarket never built — and the README pointer names two symbols and a file
   that no longer exist.
10. `player_save`/`player_load` unwired in `classicai.c`.
11. `dai_war_desire()` performs a perfect census with zero handicap checks at
    every difficulty including `AWAY`.
12. `daitech.c` and `daidata.c` contain no handicap checks at all.
13. `H_MAP` and `H_FOG` only ever set at `AI_LEVEL_AWAY`.
14. The AI never reads `map_get_player_tile()` / `map_get_player_city()`.
15. `assess_defense_backend()` counts only units on the city tile.
16. Shore-defence want triggers on a one-tile lake; `adv->oceans[].size` computed
    every phase and never read.
17. `kill_desire()` multiplies by `victim_count` with no `killstack` check.
18. `total_danger` aggregates linearly while defence aggregates superlinearly.
19. Emergencies resolved per-city in list order with no arbitration.
20. **`is_req_active()` takes a global mutex and scans a list on every call to
    produce a value that is never used** (`requirements.c:6756–6771`).
21. `cm.c` qsort comparator context in file-static globals (`cm.c:915–916`).
22. `base_want()` temporarily writes `game.info.great_wonder_owners[]` — global
    game state — to price one building in one city.
23. `cityresult_fill()` temporarily writes `pplayer->government` and
    `tile_set_owner()` to score a candidate site.
24. Workers never stack — `autoworkers.c:490` skips any tile with a worker on it,
    though `total_activity()` sums across units.
25. **Doc:** README lists `locally_zero_minimap` (removed), calls
    `military_amortize` a *"complete mystery"* (it isn't), and points the
    supermarket bug at `farmland_food()` in `advdomestic.c` (neither exists).

---

# Appendix B — File map

| File | LoC | Contents |
|---|---:|---|
| `ai/default/daisettler.c` | 1402 | Site scoring, settler + worker dispatch |
| `ai/default/daicity.c` | 2261 | Building want, `base_want`, emergencies |
| `ai/default/daidomestic.c` | 696 | Settler/worker/caravan want, traits |
| `ai/default/daiunit.c` | 3589 | `find_something_to_kill`, rampage, airlift |
| `ai/default/daimilitary.c` | 2068 | Danger, attacker/defender selection |
| `ai/default/daidiplomacy.c` | 2247 | War desire, treaties, call to arms |
| `ai/default/daitools.c` | 1468 | `kill_desire`, `military_amortize`, goto params |
| `ai/default/daieffects.c` | 993 | Effect valuation incl. culture and migration |
| `ai/default/daitech.c` | 622 | Tech want aggregation |
| `ai/default/daiferry.c` | 1332 | Boats |
| `ai/difficulty.c` | 348 | Handicap assignment per level |
| `server/advisors/autoworkers.c` | 1365 | Worker search, `road_bonus` — **shared with humans** |
| `server/advisors/infracache.c` | 512 | Tile activity value; `adv_calc_*` |
| `server/advisors/advdata.c` | 1171 | Continent/ocean stats, government goal |
| `server/advisors/advgoto.c` | 662 | `adv_danger_at`, `adv_avoid_risks` |
| `common/requirements.c` | — | `is_req_active`, `req_context` |
| `common/aicore/cm.c` | — | City management; comparator globals |
| `common/aicore/citymap.c` | — | Settler reservation; global scratch |
| `server/cityturn.c` | — | `set_default_city_manager`, migration, disband |
| `server/maphand.h` | — | `struct player_tile` — the remembered map |
| `common/diptreaty.h` | — | Clause types, `clause_info` |
| `ai/classic/classicai.c` | 719 | Module template |
| `ai/tex/` | — | Threaded module template |
| `doc/README.AI` | 342 | **Read in full — the TODO list is a roadmap** |

---

*The most encouraging thing about this codebase is how much of the work is
already half-done and abandoned in place: the module vtable, the CM parameter
override, the remembered map, `is_pos_dangerous`, `adv->oceans[].size`, the
invasion counters, `tile_virtual_new`, `req_context`, `fc_rand_state`,
`create_city_virtual`, `clause_info`'s requirement vectors, `add-cap()`. Almost
nothing here requires new infrastructure. It requires connecting infrastructure
that already exists to decisions nobody ever wrote — and then, finally, letting
the AI look at the world through the same window as everyone else.*
