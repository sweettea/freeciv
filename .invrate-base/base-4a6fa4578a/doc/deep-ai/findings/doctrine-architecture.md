# Part 3 — Doctrine architecture

Companion to Parts 1 and 2. Same tree: `freeciv/freeciv` @ `0896b34`.

---

## 0. There is already a strategy enum, and it's vestigial

Before adding one, know that `ai/default/daidata.h:31` has:

```c
enum winning_strategy {
  WIN_OPEN,     /* Still undetermined */
  WIN_WAR,      /* We have no other choice than to crush all opposition */
  WIN_SPACE,    /* We will race for space, peace very important */
  WIN_CAPITAL   /* We cannot win unless we take war_target's capital */
};
```

Grep the whole tree for uses. There are six, all in `daidiplomacy.c`, and it is
only ever assigned `WIN_SPACE`, `WIN_CAPITAL`, or `WIN_OPEN`.

**`WIN_WAR` is never set anywhere.** It's dead code — a declared intention
nobody wired up. And nothing outside the diplomacy module ever reads
`ai->diplomacy.strategy`; it has zero influence on production, settling, or unit
behaviour.

So the doctrine system should **replace** `winning_strategy`, not sit beside it.
There's already a vestigial one; two would be worse than one.

---

## 1. Why your six shouldn't be one enum

Landgrab, consolidate, enrich, arms-race, beg-for-peace, steamroller. Try to
hold them in a single `enum doctrine` and you immediately hit nonsense:

- What's the economic policy while in `STEAMROLLER`? Steamroller says where the
  army goes; it says nothing about whether you're still founding cities.
- Can you be `ARMS_RACE` and `CONSOLIDATE`? Obviously yes — building tall while
  building a deterrent is a coherent and common plan.
- `BEG_FOR_PEACE` is a stance toward one *specific* player. You might be
  begging Persia while steamrolling Egypt. A single global enum can't express it.

They're not six values of one variable. They're points on **three axes** that
run simultaneously, plus a per-player diplomatic stance:

```c
struct doctrine {
  /* Axis 1 — economy: what shape is the empire? */
  enum { ECON_LANDGRAB, ECON_CONSOLIDATE, ECON_ENRICH } econ;

  /* Axis 2 — military posture: what is force *for*? */
  enum { MIL_TURTLE, MIL_DETER, MIL_CAMPAIGN } mil;

  /* Axis 3 — per-player diplomatic stance, not global */
  enum { DIP_HOSTILE, DIP_NEUTRAL, DIP_COURT, DIP_SUE } stance[MAX_PLAYERS];

  /* Shield allocator — the axes bid, this splits */
  float budget_econ, budget_mil, budget_infra;
};
```

Your six names then become **named presets** — useful for logging and for
talking about behaviour, but not the thing the code branches on:

| Your name | = | Axis state |
|---|---|---|
| Landgrab | | `ECON_LANDGRAB` + `MIL_TURTLE` |
| Consolidate | | `ECON_CONSOLIDATE` + `MIL_DETER` |
| Enrich | | `ECON_ENRICH` + `MIL_DETER` |
| Arms-race | | any econ + `MIL_DETER`, no campaign target |
| Steamroller | | `ECON_CONSOLIDATE` + `MIL_CAMPAIGN` + one target |
| Beg-for-peace | | `DIP_SUE` toward one player; econ/mil unchanged |

The payoff is that the code stays a want-multiplier system (Part 2 §2c) rather
than six divergent behaviour trees, and illegal combinations become explicit
constraints you can assert on rather than states you forgot to handle.

**The one hard constraint:** `MIL_CAMPAIGN` and `DIP_SUE` toward the *same*
player is contradictory. Assert it. Everything else composes.

---

## 2. Enrich — the mechanic exists and the AI has never used it

This one turned out better than I expected. You guessed "tearing down extra
cities, post-settler," and Freeciv supports it two ways.

### 2a. Pop transfer via JOIN_CITY

`ACTION_JOIN_CITY` is real, gated at `server/unithand.c:1819–1836` by
`game.info.add_to_size_limit`. Build a pop-cost unit (Migrants in civ2civ3,
Settlers generally) in a marginal city, walk it to a core city, join.

The AI **never does this.** `grep -rn "ACTION_JOIN_CITY" ai/` finds it only in
`daicity.c:1292`, inside `action_target_neg_util()` — which is evaluating what
*enemies* might do. The AI has no code path that chooses to join a city.

Worse, it's a documented never-implemented TODO. `daisettler.c:884`:

```
Find nearest and best city placement or (TODO) a city to immigrate to.
```

and `daisettler.c:748–753` gets right up to the edge and turns around:

```c
/* If (x, y) is an existing city, consider immigration */
if (pcity && city_owner(pcity) == pplayer) {
  return NULL;
}
```

It even checks `add_to_size_limit` correctly at line 740, then discards the
whole idea. The plumbing is half-built.

### 2b. Outright disband

`disband_city()` — `server/cityturn.c:3976`, called from `cityturn.c:3059` when
a size-1 city completes a pop-cost unit. The city vanishes, and
`transfer_city_units()` moves all its supported units to the nearest city. So
"shrink a marginal city to 1, then build a settler in it" is a complete,
supported teardown, and the settler is your pop transfer.

### 2c. When is enrich actually correct?

Worth being precise, because "fewer cities" is not automatically better.

**Gains from moving a citizen from marginal city M to core city C:**
- the citizen works a better tile (C's catchment is better by construction)
- it's multiplied by C's building stack — this is the big one, and it's exactly
  why enrich is the *late* game: `EFT_OUTPUT_BONUS` multipliers only exist once
  you've built them
- M's corruption/waste disappears entirely (`city_waste()`)
- M's building gold upkeep disappears (`city_improvement_upkeep`)
- empire-size unhappiness relaxes for every remaining city
- **C's migration score rises and M's leak risk vanishes** (Part 2 §3)

**Losses:**
- M's tile coverage and border claim
- M's free content citizens / flat per-city effects
- the shields sunk in M's buildings, gone
- one fewer build queue

So enrich is right when multipliers are strong, empire-size penalties are
biting, and M's tiles are poor or heavily overlapped with C's. That is precisely
the CONSOLIDATE crossover from Part 2 §2b pushed one step further — which is
why it belongs on the same axis rather than as a separate doctrine.

**Implementation:** reuse the doctrine probe. For each city, compute
`enrich_delta = value_of_citizens_relocated + waste_saved + upkeep_saved
+ migration_gain − tiles_lost − flat_effects_lost`. Where it's positive and the
city is a genuine runt, set `founder_want` negative there and queue a pop-cost
unit with a JOIN_CITY task on the nearest core city. That last part is new code
— there's no `AIUNIT_JOIN_CITY` task today; you'd add one alongside
`AIUNIT_BUILD_CITY` in `daiunit.h`.

Do not enrich border cities regardless of the arithmetic. Losing the border
claim is worth more than the math says, and the probe won't see it.

---

## 3. Arms-race — deterrence is exactly computable

The trap: building an army with no target is how the current AI already wastes
production. Arms-race is only correct as **deterrence** or as **campaign prep
with a deadline**. Otherwise it's just shields on fire.

But deterrence here is unusually tractable, because you can compute the enemy's
decision function directly. `dai_war_desire()` — `daidiplomacy.c:797`:

```c
unit_list_iterate(target->units, punit) {
  fear += ATTACK_POWER(ptype);        /* their units frighten us */
}
unit_list_iterate(pplayer->units, punit) {
  fear -= ATTACK_POWER(ptype) / 2;    /* ours reassure us */
}
...
return (want - fear);
```

An enemy AI attacks you when `want − fear > 0`, and **every point of visible
attack power you own adds one point to their fear.** So the deterrence
requirement is a number you can literally compute:

```c
/* run dai_war_desire from THEIR perspective toward US */
int their_desire = estimate_war_desire(neighbour, self);
/* attack power needed to push it negative, plus margin */
int deter_target = MAX(0, their_desire) + margin;
```

That gives `MIL_DETER` a concrete, satisfiable objective instead of a vibe, and
— crucially — a **stopping point**. Once you're over `deter_target`, stop
building units and put the shields back into the economy. The current AI has no
notion of "enough army."

Two other terms in that function are worth exploiting:

```c
fear += MAX(research_get(target)->techs_researched
            - research_get(pplayer)->techs_researched, 0) * 100;
```

A tech lead is worth **100 fear per tech** — often cheaper deterrence than
units. And:

```c
/* Our own expansionism reduces want for war */
if (unit_is_cityfounder(punit)) { want -= 200; settlers++; }
...
want -= abs(want) / MAX(cities - settlers, 1);
```

A neighbour busy expanding won't attack you. Which means `ECON_LANDGRAB` is
*itself* partially deterrent — another argument for axes over a flat enum, since
your economic choice feeds the military one.

---

## 4. Beg-for-peace — the trigger is wired to the wrong variable

This is the most clearly broken of the six, and not in the way you'd expect.
It's not that it's unimplemented. It's that it's implemented against the wrong
input.

`daidiplomacy.c:1938–1946`, the ceasefire request:

```c
case DS_WAR:
  clause.type = CLAUSE_CEASEFIRE;
  if ((is_human(aplayer) && adip->asked_about_ceasefire > 0)
      || dai_goldequiv_clause(ait, pplayer, aplayer, &clause,
                              FALSE, DS_CEASEFIRE) < 0) {
    break; /* Fight until the end! */
  }
```

`dai_goldequiv_clause` for a ceasefire resolves to `greed(love)` —
`daidiplomacy.c:125`. So:

> **The AI sues for peace when it *likes* you, not when it's *losing*.**

Search the AI tree for any war-outcome tracking — cities lost, units lost, score
trend, territory ceded — and there is none. `dai_war_desire`'s `fear` term is a
static snapshot of unit counts and tech lead, computed identically whether
you're twenty turns into a triumph or a rout. Nothing in the AI notices that a
war is going badly.

**What `DIP_SUE` needs:**

1. **A war ledger.** Per enemy, per war: cities gained/lost, shields of units
   lost vs destroyed, score delta since war start, turns elapsed. Store it in
   `ai_dip_intel` (`daidata.h:52`), which already persists per-player-pair and
   is already save/loaded via `player_save_relations`.
2. **Feed it into `dai_war_desire`.** Add an attrition term so `fear` reflects
   what's actually happened, not just what's visible on the map. A war where
   you've lost 4000 shields of units and 2 cities should read as terrifying even
   if your remaining stack looks healthy.
3. **Willingness to pay.** This is the real gap. `dai_goldequiv_clause` handles
   `CLAUSE_GOLD`, `CLAUSE_ADVANCE`, `CLAUSE_CITY` — the AI *can* construct an
   offer. It just never offers anything to end a war it's losing. A losing AI
   should be willing to cede gold, tech, and in extremis a city, and the
   valuation should scale with how badly the ledger reads. Ceding a border city
   to end a hopeless war is a legitimate move no Freeciv AI has ever made.
4. **Respect the spam limiter.** `asked_about_ceasefire` is set to 9 turns for
   humans (`daidiplomacy.c:1948`). Don't bypass it; a begging AI that messages
   every turn is worse than one that doesn't beg.

Note the interaction with Part 1's campaign object: `DIP_SUE` toward a player
must dissolve any active campaign against them, and campaign dissolution should
be one of the triggers that *lets* `DIP_SUE` engage. Otherwise you get an AI
suing for peace while its stacks are still walking toward the enemy capital.

---

## 5. Steamroller

Mostly built in Part 1 §3 — it's the campaign object. Three additions now that
it's a posture rather than a permanent mode:

1. **It's a state transition, not a setting.** `MIL_CAMPAIGN` engages when a
   target's `value / defence`, including the cluster-cascade term, exceeds
   threshold *and* `deter_target` from §3 is already satisfied. You should not
   steamroller while undefended at home.
2. **It must move the economic axis with it.** Entering `MIL_CAMPAIGN` sets
   `budget_mil` high and suppresses `founder_want` — this is where "apply the
   might of the empire to one city" actually happens. A campaign that doesn't
   change what cities *build* is just a stack walking somewhere.
3. **It needs an exit.** On target captured, or campaign dissolved by the abort
   conditions, drop back to `MIL_DETER` and re-run the doctrine probe. Without
   an explicit exit the AI stays on a war footing forever, which is the current
   behaviour and part of why it never builds improvements.

---

## 6. The seventh you didn't list

**Tech/space.** `WIN_SPACE` already exists and is the one branch of the
vestigial enum that's actually wired (`daidiplomacy.c:1125`). It belongs on the
economy axis as a terminal state past `ECON_ENRICH` — maximum multipliers,
minimum city count, science rate pinned, and a diplomatic posture of
`DIP_COURT` toward everyone because you need to not be invaded for 80 turns.

It's also the doctrine most likely to expose bugs in everything else, because it
requires the AI to hold a peaceful course for a long time — which the current
love-driven diplomacy cannot do.

---

## 7. Where the axes contend

All three bid for the same shields, so you need an allocator, not just three
independent multiplier sets. Suggested shape:

```c
/* Each axis proposes a demand in shields/turn; allocator normalizes. */
demand_econ  = sum of settler + building wants above threshold
demand_mil   = MAX(deter_target - current_power, campaign_deficit)
demand_infra = unbuilt road-plan length * INFRA_WEIGHTING_STRATEGIC

/* Hard priorities first */
if (any city in grave_danger)      → mil gets what it needs, unconditionally
if (magnet city growth-capped)     → infra/econ for aqueduct (Part 2 §3b.1)
/* Then proportional split of the remainder */
```

The existing ceiling structure supports this:
`DAI_WANT_MILITARY_EMERGENCY = 1000.1` and `DAI_WANT_DOMESTIC_MAX = 750`
(`daitools.h:31–33`). Campaign and deterrence wants slot between them.

**Log the doctrine vector every turn.** `(econ, mil, budget split, active
campaign target, stance per player)` in one line. When the AI does something
baffling forty turns in, that log is the only thing that will tell you why —
and with three axes plus per-player stances, "baffling" is now a much larger
space than it was with a flat enum.
