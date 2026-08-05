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

- **Phase A — the stance layer. BUILD THIS.** Three axes with a weight
  each, per player, seeded from the traits the game already randomises.
  Measurable in the cells we already run, and it unblocks the tuning
  campaign.
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

### A.1 The object — AXES, not a menu

**Corrected 2026-08-04 after human review.** The first draft made this
a three-valued enum; that was wrong twice over. It contradicts
`findings/doctrine-architecture.md`, which had already established that
these are **not values of one variable** but simultaneous axes, and it
throws away the thing that makes AIs interesting against each other:
each player having its **own** preference rather than picking from a
menu of three.

```c
struct deep_stance {
  double econ;      /* growth, infrastructure, trade, expansion */
  double mil;       /* force: garrison, deterrence, campaigns */
  double cult;      /* culture and prestige — DECLARED, INERT: see A.1c */
};
```

A weight per axis, held in `struct deep_plr`, saved with the rest of
deep state (or recomputable — `deepai.h` requires one or the other).
The old enum survives only as **named presets** for logging and for
talking about behaviour, exactly as doctrine-architecture.md put it:
"economic" is the corner `(hi, lo, ·)`, "military" is `(lo, hi, ·)`,
"balanced" is identity. **The code branches on the axes, never on the
name.**

#### A.1a Where a player's own preference comes from: TRAITS

Do not invent a preference source. The game already gives every player
a randomised personality vector and the stock AI already respects it:
`TRAIT_EXPANSIONIST`, `TRAIT_TRADER`, `TRAIT_AGGRESSIVE`,
`TRAIT_BUILDER` (`common/traits.h:21-29`), read via
`ai_trait_get_value()` at `daidomestic.c:417`, `:558`, `:620`,
`daicity.c:2042`, `daimilitary.c:1778`, `:1914`, `daidiplomacy.c:1703`.

So the axes are **seeded from traits**, per player, per game:

    econ  <- EXPANSIONIST / BUILDER / TRADER
    mil   <- AGGRESSIVE
    cult  <- no trait exists; see A.1c

This gets the property you actually want — four deep seats in one game
behaving like four different players, deterministically, from a source
the ruleset and the nation already control — **without inventing a
personality system and without inventing constants**. It also means the
axes vary across our benchmark seats for free, which is a self-play
diversity win the enum could never give.

#### A.1b Two modes, and why both are needed

The per-player variation the design wants is in direct tension with the
reason phase A is scheduled first (tuning separability: pin the fleet,
stabilise the shield budget, tune one block). Resolve it as two modes
of the **same** forcing key, not as a compromise on either:

- **FREE** (shipping default): every player's axes come from its own
  traits. Players differ. This is the behaviour we actually want to
  ship and the one self-play should exercise.
- **PINNED** (`stance_forced`): every deep seat is clamped to the
  same named corner, which is what makes a tuning block separable.
  Tuning passes run pinned; acceptance and headline numbers run free.

A run must log which mode it was in. A tuning result taken free and
reported as if pinned — or the reverse — is a wasted campaign, and this
project has already lost one 40-game batch to a switch nobody verified.

#### A.1c The culture axis is declared and INERT, on purpose

You asked whether culture is one of the axes. It should be *declared*,
because leaving it out would mean retrofitting a third axis later
through every gate. It should not be *active* yet, and the reasons are
measurement facts rather than taste:

- **Culture victory is off in every cell we run.**
  `GAME_DEFAULT_VICTORY_CONDITIONS` is spacerace + allied
  (`game.h:589`); `VC_CULTURE` (`fc_types.h:970`) is checked at
  `srv_main.c:592-612` against `culture_vic_points` and a
  `culture_vic_lead` percentage over second place, and nothing in our
  configs enables it.
- **Culture is worth almost nothing to our metric.** `get_civ_score()`
  counts `culture / 50` (`score.c:361`). At our horizons that is noise
  against citizens and techs.
- **The lever already exists and is already priced.** The stock AI
  values `EFT_HISTORY` / `EFT_NATION_HISTORY` at `amount * 5` and
  performance effects at `amount / 10 + 1` (`daieffects.c:632-644`), so
  a culture axis today would only re-scale a term that is already
  there, chasing points our scoring barely counts.

So: `cult` exists in the struct, is carried through every gate, and is
**pinned at identity until a victory-enabled cell exists** — which is
phase B's condition anyway. Un-defer it exactly when culture victory is
enabled in a cell, not before. Anything else is building a mechanism we
have no way to price.

### A.2 The gates

Four, taken from `findings/objective-layer.md` §4, in the order the
code already offers seams. **Each gate is a multiplier on an existing
want, never a new scale** — RC4 says do not add another incommensurable
currency, and this workstream is not permitted to invent one.

Each gate reads the axes as a ratio, not as a name. The shape is
`want *= f(stance)`, where `f` is identity when the axes are equal —
so a player with balanced traits plays today's game exactly.

1. **Fear → want** (`deep_credit_incoming_defence` and the injected
   fear terms in `deepdefence.c`). A high `econ`-to-`mil` ratio scales
   anticipatory fear toward — **never below** — the existing survival
   floor (`survival_fear_value`).
2. **Operation formation** (`deep_op_form*` in `deepops.c`). Low `mil`
   forms assaults only against opportunistic or threatening targets;
   high `mil` forms freely.
3. **Build wants** (`deepwant`, the `horizon_*` credits). The growth
   ladder against barracks/walls.
4. **Expansion** (`settler_rush_weight`, `colony_lift_weight`). High
   `econ` expands while land remains; high `mil` converts land pressure
   into targets.

**A high `econ` weight means minimise military spend subject to
survival, not "no army".** A trader-natured player on a contested map
still gets attacked; if it refuses to build defenders it dies, and the
economy constants get tuned in a world that never occurs.

**Identity is the acceptance gate.** With every axis equal and every
scale at 1.0, the binary must be **byte-identical** to today's. W01 set
that precedent (byte-identity 20/20), and it is the only way to prove a
control surface this wide is inert when it should be.

### A.3 Tech direction

The one place phase A touches victory at all, because it is where the
axes have somewhere obvious to go and no dependency on knowing anything
about rivals: bias `plr_data->tech_want` along the axes before
`dai_select_tech()` maximises over it. High `econ` weights
trade/growth/science unlocks; high `mil` weights the unlocks its
doctrine consumes.

Note this is where traits and the axes will interact with a scale
problem already measured: W01's calibration corpus found settler wants
are *already* trait-rigged (median 311) against building wants near the
anchor (10-48). Biasing tech along an axis is safe because tech_want is
one scale; biasing builds is what W01's founder deflation is for. **Do
not let the axis compensate for a scale gap** — that is how the two
mechanisms would fight.

This is a **want multiplier on an existing scale**, not a goal-seeking
search. Real goal-seeking (a path to a named tech with a deadline) is
phase B, because a deadline needs a rival.

### A.4 Where the axes come from, in landing order

Three sources, each strictly later than the last, and the ordering is
what keeps the phase measurable:

1. **Identity** — every axis equal. Today's game, byte-identical. This
   is what ships until a later step is proven.
2. **Traits** (A.1a) — each player's own preference, from the vector
   the game already randomises and the stock AI already reads. This is
   the first step that makes deep seats differ from each other, and it
   is measured as such: seat-level variance in military share should
   RISE while the fleet mean moves little.
3. **Position** — land available, neighbour militarisation, and which
   victory conditions the ruleset actually enables (`victory_enabled()`,
   `common/victory.c:26`), modulating the trait-given prior rather than
   replacing it. A trait prior says who this player *is*; position says
   what the map is offering. Lands last.

Under PINNED mode all three are overridden, which is the point.

### A.5 Constants (rule 7 — all PLACEHOLDERS, none invented here)

Add to `struct tuning` with placeholder values, register in all four
places `tune-audit.py` checks, and flag in STATUS for a tuning pass:

```
stance_forced        -1 = free (traits/position), 0..n = pinned corner
stance_source         0 identity, 1 traits, 2 traits+position (A.4 ladder)
stance_trait_span     how far a trait may move an axis off identity
stance_fear_gain      gate 1: sensitivity of fear to the econ:mil ratio
stance_op_gain        gate 2
stance_growth_gain    gate 3
stance_expand_gain    gate 4
stance_tech_gain      A.3
```

**Eight, not ten, and none of them per-named-objective.** That is the
axis form paying for itself: a gate is one gain against a ratio, so
adding an axis or a corner does not add constants. Identity for every
gain is 0.0 (no sensitivity) or 1.0 (no scaling) as the gate's shape
requires, and identity axes give identity behaviour at any gain — which
is what makes the byte-identity gate provable rather than argued.

No new survival floor: gate 1 reuses `survival_fear_value`.
`stance_trait_span` is the one that decides how *different* players are
allowed to be, and it is exactly the kind of number rule 7 forbids
guessing: placeholder, then tuned.

### A.6 Acceptance

- **Byte-identity** at identity axes, 20 paired seeds across two cells.
  Not "no significant difference" — identity.
- **The mode and the axes are proven by an execution counter, not
  assumed.** One `deep-stance` line per player per turn carrying the
  three axis weights, the source (identity / traits / position) and the
  mode (free / pinned); a pinned arm shows one distinct stance across
  all seats, a free arm shows four. Four changes in this project have
  measured as "no effect" while never executing, and one 40-game batch
  was lost to a switch nobody verified.
- **The regime must be real.** Pin every deep seat to the econ corner
  on `isolated`: military spend must fall toward the survival floor and
  the army ratio toward baseline's. **If it does not, a gate is
  missing** — this is a falsifiable claim about the gates being
  complete, and it is the point of the phase.
- **Traits must actually differentiate.** In FREE mode, seat-level
  variance in military share must RISE against the identity arm while
  the fleet mean moves little. If the seats still behave alike, either
  `stance_trait_span` is too small or the gates are not reading the
  axes — and the difference between those two is why the stance line
  carries the weights themselves.
- **Separability, the thing this is for:** re-run one economy-block
  tuning pass pinned ECONOMIC and show the per-candidate objective
  variance falls against the same pass unpinned. If it does not fall,
  the premise in `objective-layer.md` §2 is wrong and this workstream
  should say so rather than proceed to phases 3-5 of that plan.
- **Score: report it, do not promise it.** Phase A is a control
  surface, not a mechanism; at identity its prior is *exactly zero* by
  construction, and in FREE mode there is no prior at all — four
  differentiated players may beat four identical ones or may not, and
  that is a genuine open question rather than a hoped-for win.

### A.7 Epics

Roughly three, in this order. Do not bundle.

**E34.1 — the surface (5-7 commits).** `struct deep_stance` in
`deep_plr`, saved; the pinning key and its corners; the `deep-stance`
log line; gate 1. Axes stay at IDENTITY throughout this epic — no
trait reading yet. Acceptance: byte-identity, pinning proven by the
counter, gate 1 executing under a pinned corner.

**E34.2 — the remaining gates (6-8 commits).** Gates 2, 3, 4, each with
its own counter and its own A/B, plus the regime-reality check. Still
identity in FREE mode: everything so far is only reachable by pinning.

**E34.3 — the players become different (4-6 commits).** A.1a: seed the
axes from traits, `stance_source = 1`. **This is the first commit in
the workstream that changes an unpinned game**, and it lands only after
every gate is proven by pinning, so a behaviour change here can be
attributed to the trait mapping rather than to a gate nobody exercised.
Acceptance is the differentiation criterion above.

**E34.4 — tech direction and position (5-7 commits).** A.3, then A.4
step 3. Position modulates the trait prior; it does not replace it.

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
- **Not W08.** Doctrine is *how* we fight — and specifically, per
  `findings/doctrine-architecture.md`, the per-RIVAL diplomatic stance
  (`DIP_HOSTILE`/`NEUTRAL`/`COURT`/`SUE`) which cannot be a global
  value because you may be begging Persia while steamrolling Egypt.
  W34 owns the *global* axis weights; W08 owns the per-rival stance and
  the named presets on top. W08 consumes this.
- **Not a personality system.** The per-player variation is read from
  `common/traits.h`, which the ruleset and the nation already control.
  Inventing a second source of personality beside it would be two
  systems disagreeing about who a player is.

---

## Commits (sketch, phase A only)

```
ai/deep: give a player a stance on three axes and a way to pin it
ai/deep: log a player's stance, its source and its mode
ai/deep: scale anticipatory fear by the stance
ai/deep: gate operation formation on the stance
ai/deep: weight the growth ladder by the stance
ai/deep: weight expansion by the stance
ai/deep: seed a player's stance from its traits
ai/deep: bias tech wants along the stance
ai/deep: modulate the stance by what the position offers
```

Phase B's commits are deliberately not sketched: they depend on W04's
shape, and a sketch would go stale before anyone reads it.
