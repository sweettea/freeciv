# W33 — The operations layer

**Phase 3, `ai/deep`. The spine W13/W31/W32 all attach to.**
Root cause: RC2, escalated — not "units don't coordinate this turn" but
**"the AI has no representation of a military operation that spans turns."**

Baseline: `freeciv/freeciv` @ `0896b34`.

---

## 1. The realization

Air and naval feel opportunistic because **the whole AI is opportunistic** — they
just make it *visible*. A land attack that re-decides every turn is cheap: the
units are adjacent, and retrying next turn costs almost nothing. An air strike or
an amphibious assault only pays off if a *sequence* completes — muster, escort,
cross, bombard, land, hold — and re-deciding each phase from scratch means the
sequence never coheres. Same bug; different blast radius.

### The evidence: there is no plan object

The closest things in the code, and why each is not a plan:

**`struct ai_invasion { int attack; int occupy; }`** (`daicity.h:29`). Looks like
invasion planning. It isn't — `daiunit.c:1302` zeroes it every turn:

```c
city_data->invasion.attack = 0;
city_data->invasion.occupy = 0;
/* ... then rebuilt by scanning which units already happen to point here */
```

It is a **per-turn census of emergent convergence**, not a decision. Nothing
*chose* to invade; the AI is *observing* that some units drifted toward a city and
tallying them. Remove the units' individual target-picking and the "invasion"
evaporates, because nothing holds it.

**`enum ai_unit_task`** (`daiunit.h:27`): `AIUNIT_ATTACK`, `AIUNIT_DEFEND_HOME`,
`AIUNIT_ESCORT`, `AIUNIT_HUNTER`... This is the only cross-turn intention a unit
carries — but it's a **role**, not a plan. A unit tagged `AIUNIT_ATTACK` still
re-runs `find_something_to_kill()` every turn and may pick a different target. The
persistence is one unit deep ("I am currently an attacker") and zero plans wide
("we are on turn 4 of 9 of taking Thebes").

So: **no object represents a future operation.** No muster order, no phase, no
commitment, no set of units bound to a shared goal that outlives the turn.
Everything is per-turn recomputation with stable-ish inputs.

W13 as previously specified (campaigns) is necessary but was still framed
per-target. This workstream is the missing *substrate* underneath it: the thing
that persists, sequences, and commits.

## 2. What a real operation is

An operation is a **stateful, multi-turn commitment** with:

```c
struct operation {
  enum op_type   type;        /* LAND_ASSAULT, AMPHIB_ASSAULT, AIR_STRIKE,
                                 AIRBASE_CHAIN, PARADROP_SEIZE, BLOCKADE,
                                 CARRIER_STRIKE, MISSILE_SALVO, EVACUATION */
  enum op_phase  phase;       /* MUSTER, STAGE, MOVE, PREP, EXECUTE, HOLD, ABORT */
  int            started_turn;
  struct tile   *objective;

  struct unit_list *committed;    /* units bound to THIS op — not re-tasked freely */
  struct force_requirement need;  /* what the op needs before it may advance a phase */

  struct tile   *staging;         /* muster point / forward base / beachhead */
  int            plan_horizon;    /* turns the op expects to take; for amortize */
  adv_want       expected_value;  /* objective value, discounted over horizon */
  adv_want       sunk;            /* committed cost so far — for abort math */

  int            stall_turns;     /* phases without progress; triggers abort */
};
```

The three things this buys that the current AI cannot express:

1. **Commitment.** Units in `committed` are not re-tasked by the per-turn unit
   loop. They belong to the operation until it releases them. This is what stops
   the bombardment fleet from wandering off to chase an opportunistic target the
   turn before the landing.

2. **Phase sequencing.** The op advances MUSTER → STAGE → … only when the phase's
   precondition is met. The bombers do not launch until STAGE says the forward
   airbase exists. The transports do not cross until PREP says the escort is
   present and the beachhead is bombarded. Each doctrine's phase list is where
   the "in synchrony" actually lives.

3. **Cross-turn value and abort.** `expected_value` is the objective discounted
   over `plan_horizon` (RC1's amortize, applied to a *plan* not a tile). `sunk`
   grows as units commit. Abort when `stall_turns` exceeds a bound, or when
   `expected_value - remaining_cost` goes negative, or when W04 reveals the
   objective changed. **A committed operation that stalls is abandoned
   deliberately, banking survivors** — not left to rot because no one owns it.

## 3. Phase lists per operation type

The doctrines already drafted become **phase lists over this substrate**:

**LAND_ASSAULT:** MUSTER (gather N attackers within range) → MOVE (advance as a
group under W10 combat model) → EXECUTE (killstack-aware assault) → HOLD (garrison).

**AMPHIB_ASSAULT (W31):** MUSTER (troops + transports + escort at a port) → MOVE
(cross an escort-controlled lane) → PREP (bombard the beachhead — naval guns and/or
air) → EXECUTE (land onto the cleared beachhead) → HOLD (screen until the city
falls).

**AIR_STRIKE / CARRIER_STRIKE (W32):** STAGE (ensure a base with a closing
round-trip — W32 addendum A) → EXECUTE (strike with recovery guaranteed) → recover.

**AIRBASE_CHAIN (W32 §3.1):** MUSTER (engineers + one escort) → MOVE (march through
no-man's-land) → STAGE (build the chain, phase-gated tile by tile) → hand off to an
AIR_STRIKE op that the chain made reachable.

**PARADROP_SEIZE (W32 addendum B):** requires a paired AIR_STRIKE op to reach
EXECUTE and clear the target *this turn* → then EXECUTE (drop) → HOLD. The
dependency between two operations is itself state the substrate holds.

**BLOCKADE (W31):** MUSTER (fleet) → MOVE (to the enemy lane) → HOLD (maintain
presence, refresh as ships attrit).

**EVACUATION (W14 losing-war):** MUSTER (transports at a doomed city) → EXECUTE
(load the valuable units) → MOVE (withdraw to safety). The AI currently cannot
plan a retreat *at all*; this is the same machinery pointed backward.

## 4. How it changes the per-turn loop

Today: iterate units, each decides for itself.

With the substrate: **operations decide, then units execute their assigned part.**

```
each turn:
  1. review operations: advance phase if precondition met; abort if stalled/negative
  2. for each active op, issue its committed units' orders for this phase
  3. THEN run the ordinary per-unit loop for UNCOMMITTED units only
     (opportunistic behavior still exists — it's just no longer the ONLY behavior)
```

Opportunism isn't deleted — it's demoted to the default for units not currently
part of a plan. A lone frigate with nothing better to do still raids. But the
six ships tagged to the amphibious operation hold formation.

This is also the natural home for W20 threading's serial/parallel split: operation
review and unit assignment are serial (they touch shared commitment state); the
per-unit evaluation of uncommitted units is the parallel fan-out.

## 5. Relationship to the existing plan

This **reframes W13 rather than replacing it.** W13's "campaign" was the
land-assault case; W33 is the general substrate, and W13/W31/W32 become phase-list
definitions over it. Concretely:

- W13 → LAND_ASSAULT and the muster/abort machinery, generalized into `operation`.
- W31 naval → AMPHIB_ASSAULT, BLOCKADE, CARRIER_STRIKE phase lists + the escort
  precondition as a phase gate.
- W32 air → AIR_STRIKE, AIRBASE_CHAIN, PARADROP_SEIZE phase lists + round-trip
  recovery as a phase gate.
- W14 → EVACUATION, and the abort/value math feeding the war ledger.

Build W33's substrate **first**, then the doctrines are phase lists — a few dozen
lines each — instead of standalone systems. Without it, each doctrine reinvents
commitment and sequencing and they don't interoperate (paradrop can't depend on a
strike; carrier can't serve an amphibious op).

## 6. Placement and dependencies

`ai/deep`. Depends on: W04 (operations reason about *seen* enemy state and abort on
new information), W10/W11 (the combat model the assault phases use), W30 (committed
units need the `campaign_hint` pin so the per-unit loop leaves them alone).

Must precede: the *coordinated* parts of W13, W31, W32. The opportunistic fallbacks
in each can exist without it; the multi-turn operations cannot.

## 7. Acceptance

- **Commitment holds:** units assigned to an operation are not re-tasked by the
  per-turn loop — assert a committed bombardment fleet stays with its op when an
  opportunistic target appears (L1 corpus).
- **Phases gate:** an amphibious op does not reach EXECUTE until PREP's bombardment
  cleared the beachhead — assert order in the op log.
- **Cross-turn persistence:** an operation started on turn T is still coherent on
  T+5 with the same objective and accumulated units (L2, short game).
- **Deliberate abort:** an operation whose objective is reinforced past viability
  aborts and withdraws survivors rather than feeding them in (L1 corpus — the thing
  the current AI most conspicuously fails).
- **Opportunism survives:** uncommitted units still raid/defend opportunistically —
  the substrate adds planned operations, it doesn't suppress everything else.
- **Overall:** multi-turn operations (amphibious, airbase-chain-strike) complete
  end-to-end in full games at measurable rates; baseline completes ≈0 because it
  cannot represent them.

## 8. Commits (indicative)

```
ai/deep: add the operation struct and its lifecycle state
ai/deep: bind committed units to an operation and exempt them from the unit loop
ai/deep: advance operation phases only when preconditions are met
ai/deep: value an operation by objective discounted over its horizon
ai/deep: abort operations that stall or turn value-negative and bank survivors
ai/deep: define the land-assault phase list over the operation substrate
ai/deep: define the amphibious-assault phase list with a bombardment gate
ai/deep: define the air-strike phase list with a round-trip recovery gate
ai/deep: define the airbase-chain phase list handing off to an air strike
ai/deep: define the paradrop-seize phase list depending on a strike op
ai/deep: define the blockade and evacuation phase lists
ai/deep: run operations before the opportunistic per-unit loop each turn
test: corpus — committed fleet ignores an opportunistic target
test: corpus — amphibious op gates execute on bombardment
test: corpus — stalled operation aborts and withdraws survivors
test: metric — multi-turn operations completed per game vs baseline zero
```

---

## 9. The one-paragraph version

The reason air and navy look opportunistic is that **the entire AI is** — it has
no object that represents a military operation lasting more than one turn. The
`invasion` counters are a per-turn census of units that happened to drift toward a
city (`daicity.h:29`, zeroed at `daiunit.c:1302`), and `AIUNIT_ATTACK` is a role a
unit re-decides every turn, not a plan it belongs to. Add an **operation
substrate**: a persistent, phased, committed multi-turn plan that owns its units,
advances only when each phase's precondition is met, and aborts deliberately when
it stalls. Then every doctrine already drafted — land assault, amphibious landing,
airbase-chain carpet bombing, bomb-then-paradrop, blockade, even evacuation —
becomes a *phase list* over that substrate, and the units in an operation finally
hold formation instead of re-deciding their lives every turn. Opportunism doesn't
die; it becomes what uncommitted units do in their spare time.
