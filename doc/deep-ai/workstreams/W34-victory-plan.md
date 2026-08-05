# W34 — Victory plan and the objective layer

**Drafted 2026-08-04.** Named by the military & intelligence bundle
(2026-07-30) as the highest-value undesigned gap; STATUS agreed and
added that it subsumes tech direction. It also **absorbs the human
objective-layer proposal of 2026-08-01** (`findings/objective-layer.md`),
which was accepted, never built, and has no other owner.

Root causes: **RC5** (planning and execution disagree — the AI computes
a "strategy" and never consults it), **RC4** in the gates, **RC6** for
everything in phase B.

**This file is staged, and the stages have different statuses.**

- **Phase A — the objective layer. BUILD THIS.** Measurable in the
  cells we already run, and it unblocks the tuning campaign.
- **Phase B — the victory plan proper. DESIGNED, NOT SCHEDULED.**
  Deferred until W04 (human call, 2026-08-04). Every input it needs is
  omniscient today; see §B.0 for why that is disqualifying rather than
  merely impure.

---

## Problem

**Nothing in either AI decides what it is trying to win**, and the one
place that looks like it does is vestigial.

`enum winning_strategy` (`daidata.h:31-36`) has four values. It is
assigned in three places and **read in exactly one**:

```
daidata.c:88          strategy = WIN_OPEN            (init)
daidiplomacy.c:880    strategy = WIN_CAPITAL         (target leads space race)
daidiplomacy.c:1125   strategy = WIN_SPACE           (we lead the space race)
daidiplomacy.c:1128   strategy = WIN_OPEN            (we no longer do)
daidiplomacy.c:1630   if (strategy != WIN_SPACE)     <-- the ONLY read
```

So `WIN_CAPITAL` — "we cannot win unless we take war_target's capital",
the exact conclusion the decapitation capstone is built to act on — is
computed and **read by nothing at all**. The PRD calls this enum
vestigial (W8); it is worse than vestigial, it is a correct inference
thrown away. This belongs in 00-context §3's table of infrastructure
built and abandoned in place.

The consequences run through three subsystems:

- **Tech has no direction.** `dai_select_tech()` sums per-city
  `tech_want` and picks a maximum. `daitech.c:371-372` says of the goal
  it sets: *"the value it sets (research->tech_goal) is practically
  never used"*. There is no tech a plan wants because there is no plan.
- **The spaceship is a reflex.** `dai_manage_spaceship()`
  (`daihand.c:98-110`) autoplaces parts and launches when the ship is
  structurally maximal. The AI never *decides* to go to space, never
  trades score-now for arrival-sooner, and never launches early to beat
  a rival — because nothing computes a rival's arrival.
- **A rival's victory draws a complaint, not a plan.** The whole
  response to an enemy spaceship is love-loss, a threat
  (`daidiplomacy.c:1495`), and alliance cancellation on launch
  (`:1635-1670`). Meanwhile capturing that rival's capital destroys the
  ship outright and unconditionally (`citytools.c:2025-2029`).

And there is a **measurement** consequence, which is why phase A is
scheduled first. `benchmark/tuning-params.ini` exposes 108 constants
against a diagonal ES at pop ~6, so they must be tuned in blocks — but
economy and military bid for **one shield budget**, so the blocks are
not separable and an economy optimum found under wrong fear knobs is an
optimum for spending a starved budget (`findings/objective-layer.md`
§1). An explicit, *forceable* objective **creates** the separability
instead of assuming it.

---

## Evidence — what the game actually decides on

Verified in this tree; `00-context.md`'s rule applies if a reference
has moved.

**Victory conditions are a settings bitvector.** `VC_SPACERACE`,
`VC_ALLIED`, `VC_CULTURE` (`fc_types.h:968-970`); the default is
spacerace + allied (`game.h:589`, `GAME_DEFAULT_VICTORY_CONDITIONS`);
`victory_enabled()` (`common/victory.c:26`) is the query. Conquest
needs no flag. **A plan that does not read this bitvector can plan for
a victory the server will not award.**

**Every deadline is computable, and none of it is estimated today.**

| Condition | The computable quantity | Where |
|---|---|---|
| Space | `spaceship_arrival()` returns the fractional arrival year; `rank_spaceship_arrival()` orders every launched ship | `spacerace.c:446`, `:460`, arrival = `launch_year + travel_time` at `:192` |
| Culture | best culture vs `game.info.culture_vic_points` **and** a lead of `culture_vic_lead` percent over second place | `srv_main.c:592-612` |
| Conquest | one player (or team) left alive | server end conditions |
| Capital capture | destroys a STARTED or LAUNCHED ship with no roll, and rolls civil war separately | `citytools.c:2025-2035` |

**Our primary metric is not any of them.** `get_civ_score()`
(`score.c:352-362`) is

```
citizens + 2*techs + 5*wonders + spaceship + units_built/10
        + units_killed/3 + culture/50
```

and `get_spaceship_score()` (`score.c:367-377`) pays **only** at
`SSHIP_ARRIVED`. Our cells truncate at T80/T150/T300, so in the entire
measurement history of this project **no victory has ever fired and no
spaceship has ever scored a point**. Under truncation, "maximise score"
is almost exactly "pursue an economic objective" — which is why phase A
is aligned with the metric we have and phase B is not.

---

## Phase A — the objective layer (BUILD THIS)

### A.1 The object

```c
enum deep_objective {
  DEEP_OBJ_BALANCED = 0,        /* identity: every gate is 1.0 */
  DEEP_OBJ_ECONOMIC,
  DEEP_OBJ_MILITARY
};
```

Lives in `struct deep_plr`, saved with the rest of deep state (or
recomputable — `deepai.h` requires one or the other), and **forceable
from the tuning file**, which is the property the whole tuning plan
rests on. Build the forcing first.

`DEEP_OBJ_BALANCED` with every gate weight at its identity value must
be **byte-identical** to today's binary. That is the A/B switch and the
acceptance gate; W01 set the precedent (byte-identity 20/20).

### A.2 The gates

Four, taken from `findings/objective-layer.md` §4, in the order the
code already offers seams. **Each gate is a multiplier on an existing
want, never a new scale** — RC4 says do not add another incommensurable
currency, and this workstream is not permitted to invent one.

1. **Fear → want** (`deep_credit_incoming_defence` and the injected
   fear terms in `deepdefence.c`). ECONOMIC scales anticipatory fear
   toward — **never below** — the existing survival floor
   (`survival_fear_value`). MILITARY leaves it.
2. **Operation formation** (`deep_op_form*` in `deepops.c`). ECONOMIC
   forms assaults only against opportunistic or threatening targets;
   MILITARY forms freely.
3. **Build wants** (`deepwant`, the `horizon_*` credits). The growth
   ladder against barracks/walls.
4. **Expansion** (`settler_rush_weight`, `colony_lift_weight`).
   ECONOMIC expands while land remains; MILITARY converts land pressure
   into targets.

**"Economic" means minimise military spend subject to survival, not "no
army".** A pinned-ECONOMIC player on a contested map still gets
attacked; if it refuses to build defenders it dies and the economy
constants get tuned in a world that never occurs.

### A.3 Tech direction

The one place phase A touches victory at all, because it is where the
objective has somewhere obvious to go and no dependency on knowing
anything about rivals: bias `plr_data->tech_want` toward the objective
before `dai_select_tech()` maximises over it. ECONOMIC weights
trade/growth/science unlocks; MILITARY weights the unlocks its doctrine
consumes.

This is a **want multiplier on an existing scale**, not a goal-seeking
search. Real goal-seeking (a path to a named tech with a deadline) is
phase B, because a deadline needs a rival.

### A.4 Choosing, when not forced

Until an objective is chosen from position, the default is
`DEEP_OBJ_BALANCED` — identity — so an unforced game is today's game.
Choice from position (land available, neighbour militarisation, which
victory conditions the ruleset even enables) lands last in the phase
and is the only part that changes an unpinned game's behaviour.

### A.5 Constants (rule 7 — all PLACEHOLDERS, none invented here)

Add to `struct tuning` with placeholder values, register in all four
places `tune-audit.py` checks, and flag in STATUS for a tuning pass:

```
objective_forced        -1 = AI chooses, 0/1/2 = pinned   (switch)
obj_econ_fear_scale     gate 1 under ECONOMIC
obj_mil_fear_scale      gate 1 under MILITARY
obj_econ_op_scale       gate 2 under ECONOMIC
obj_mil_op_scale        gate 2 under MILITARY
obj_econ_growth_scale   gate 3 under ECONOMIC
obj_mil_growth_scale    gate 3 under MILITARY
obj_econ_expand_scale   gate 4 under ECONOMIC
obj_mil_expand_scale    gate 4 under MILITARY
obj_tech_bias           A.3
```

Identity for every scale is 1.0, and BALANCED uses identity
unconditionally. No new survival floor: gate 1 reuses
`survival_fear_value`.

### A.6 Acceptance

- **Byte-identity** at `DEEP_OBJ_BALANCED` with identity weights, 20
  paired seeds across two cells. Not "no significant difference" —
  identity.
- **The forcing is proven by its execution counter, not assumed.** One
  `deep-objective` line per player per turn naming the objective and
  whether it was forced; a pinned arm shows zero lines of the other
  objective. Four changes in this project have measured as "no effect"
  while never executing.
- **The regime must be real.** Pin every deep player ECONOMIC on
  `isolated`: military spend must fall toward the survival floor and
  the army ratio toward baseline's. **If it does not, a gate is
  missing** — this is a falsifiable claim about the gates being
  complete, and it is the point of the phase.
- **Separability, the thing this is for:** re-run one economy-block
  tuning pass pinned ECONOMIC and show the per-candidate objective
  variance falls against the same pass unpinned. If it does not fall,
  the premise in `objective-layer.md` §2 is wrong and this workstream
  should say so rather than proceed to phases 3-5 of that plan.
- **Score: report it, do not promise it.** Phase A is a control
  surface, not a mechanism; the honest prior for the unpinned default
  (BALANCED, identity) is *exactly zero* by construction.

### A.7 Epics

Roughly three, in this order. Do not bundle.

**E34.1 — the surface (5-7 commits).** Enum, `deep_plr` field, save,
tuning key, `deep-objective` log line, gate 1. Acceptance: byte-identity
at BALANCED, forcing proven by counter, gate 1's execution counter
nonzero under ECONOMIC.

**E34.2 — the remaining gates (6-8 commits).** Gates 2, 3, 4, each with
its own counter and its own A/B, plus the regime-reality check.

**E34.3 — tech direction and self-selection (5-7 commits).** A.3, then
A.4. A.4 is the first commit in the whole workstream that changes an
unforced game, and it lands last on purpose.

---

## Phase B — the victory plan (DESIGNED, NOT SCHEDULED)

### B.0 Why it is deferred, and what would un-defer it

**Human call, 2026-08-04: defer until W04.** The reason is not
squeamishness about purity. Every input a victory plan needs is
information the AI has not earned:

- a rival's `spaceship.state`, `structurals`, `components`, `modules`
  and therefore `spaceship_arrival()`
- a rival's exact culture against `culture_vic_points`
- the city counts and governments that make `civil_war_triggered()`
  computable before committing to a strike

`00-context.md` §2 states the problem exactly: **RC6 makes the project
unfalsifiable, because you cannot evaluate a strategy layer that is
being fed the answers.** A victory planner is the *most* answer-fed
component we could build — its entire content is inference about other
players — so building it before W04 would produce a planner whose
measured value is an artefact of omniscience and which would have to be
rebuilt when the information got honest.

Second, independent reason: **it cannot be measured.** No victory has
ever fired in any cell in this project's history, and
`get_spaceship_score()` pays only at `SSHIP_ARRIVED`. Phase B needs a
victory-enabled cell (T300+, spacerace on, win-rate and turn-of-victory
reported beside score) that does not exist. Build that cell **with**
phase B, not before — and build both after W04, so the cell measures an
AI that had to look.

**Un-defer when:** W04 lands a knowledge layer with staleness and
confidence, *and* someone is willing to pay for a victory-enabled cell
in the matrix.

### B.1 The object, when it is built

```c
struct deep_victory_plan {
  enum victory_condition_type condition;  /* or CONQUEST / SCORE */
  int eta_turns;            /* our own, from committed production */
  int confidence;           /* W04: how stale is what this rests on */
  int denial_target;        /* player number, or -1 */
  int denial_deadline;      /* turn by which denial must land */
};
```

One for ourselves, one per rival worth modelling. Every field comes
from a computable quantity in the Evidence table — none of it is
estimated by feel.

### B.2 What it would gate

- **The objective's own choice** (phase A.4 becomes plan-driven rather
  than position-driven).
- **Tech goal-seeking**: a named tech with a deadline, which is what
  `research->tech_goal` was for and what `daitech.c:371` says nobody
  uses.
- **The spaceship decision**: build parts *because* the plan says
  space, launch when arrival beats the rival's, not when the ship is
  structurally maximal.
- **The decapitation trigger** (`findings/decapitation-strike.md`): the
  deadline is `rival_arrival − operation_eta`, and the payoff is
  `P(civil war) × fracture value + spaceship denial − cost`. Note that
  the AI already computes the premise and throws it away — `WIN_CAPITAL`
  at `daidiplomacy.c:880`.
- **Diplomacy**, replacing the one surviving read of
  `winning_strategy`.

### B.3 The honest measurement plan, for whenever it happens

- A victory-enabled cell; report **win rate** and **turn of victory**,
  not score, as primaries.
- Corpus positions as L1: a loaded save where a rival is N turns from
  launch, asserting the plan detects it and forms the denial op.
- The RC6 discipline: measure with W04's handicaps ON. A planner that
  only works while reading the answers has not been shown to work.

---

## What this is NOT

- **Not a new want scale.** Gates are multipliers on existing wants
  (RC4). If a gate seems to need its own currency, that is a finding to
  report, not a licence.
- **Not difficulty.** Hardness (handicaps, `H_MAP`/`H_FOG`) is an
  orthogonal axis and belongs to W21 —
  `findings/objective-layer.md` §5.
- **Not a score promise.** At its default (BALANCED, identity) phase A
  changes nothing by construction; its value is that it makes the
  tuning campaign separable and gives every later "it depends what
  we're trying to win" question somewhere to be answered.
- **Not W08.** Doctrine is *how* we fight; the objective is *what we
  are playing for*. W08 consumes this.

---

## Commits (sketch, phase A only)

```
ai/deep: give a player an objective and a way to force it
ai/deep: log the objective and whether it was forced
ai/deep: scale anticipatory fear by the objective
ai/deep: gate operation formation on the objective
ai/deep: weight the growth ladder by the objective
ai/deep: weight expansion by the objective
ai/deep: bias tech wants toward the objective
ai/deep: choose an objective from position when none is forced
```

Phase B's commits are deliberately not sketched: they depend on W04's
shape, and a sketch would go stale before anyone reads it.
