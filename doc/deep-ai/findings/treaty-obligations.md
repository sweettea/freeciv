# Part 8 — Extending the treaty system: promises, escrow, reputation

Companion to Parts 1–7. Same tree: `freeciv/freeciv` @ `0896b34`.

**Revising Part 7.** I said the protocol was a hard argument against extending
the treaty system. That was too cautious — the codebase is *already built for
this*, in three separate places I hadn't checked. Here's the real cost.

---

## 1. Three pieces of infrastructure that already exist

### 1a. Clause types are ruleset-declared and requirement-gated

`common/diptreaty.h:56`:

```c
struct clause_info
{
  enum clause_type type;
  bool enabled;
  struct requirement_vector giver_reqs;
  struct requirement_vector receiver_reqs;
  struct requirement_vector either_reqs;
};
```

And `data/civ2civ3/game.ruleset`:

> *Clause types that are not listed here, are not enabled at all.*

with per-clause `giver_reqs` / `receiver_reqs` / `either_reqs`. So the clause
system is **already designed as an extension point**: new clause types can be
added, shipped disabled by default, and enabled per-ruleset. Nobody has to take
your promise mechanic to play civ2civ3.

Note also that `diptreaty.h` already `#include "requirements.h"`. That matters
in §3.

### 1b. The packet generator has first-class capability gating

`common/networking/packets.def:175`:

```
add-cap: only transfer this field if the given capability is
         TILE tile; add-cap(new_version)
```

and `generate_packets.py:462` implements it (`ADD_CAP_PATTERN`). So widening
`PACKET_DIPLOMACY_CREATE_CLAUSE` is a **supported, routine operation**, not a
wire-format break. Old clients simply never see the new fields.

The one real constraint: a new *enum value* on `CLAUSE` is still a value an old
client can receive and not understand. Handle that by refusing non-capable
connections in games where the ruleset enables promise clauses — which
`clause_info.enabled` already gives you as a natural gate.

### 1c. Savegames tolerate new sections

`player_diplstate` is written with named keys (`player%d.diplstate%d.current`
etc., `savegame3.c:4311+`), and missing keys fall back to defaults on load. New
obligation sections are the cheapest part of this whole project.

---

## 2. What actually costs something

### 2a. `struct Clause` is too narrow

```c
struct Clause {
  enum clause_type type;
  struct player *from;
  int value;
};
```

One `int`. A promise needs at minimum a **target** (declare war on *whom*), a
**deadline**, and a **condition**. Widening this touches the packet, every
`clause_list_iterate` site, the savegame, and all four client diplomacy dialogs.

### 2b. Treaty vs. obligation — get this split right or retrofit forever

`handle_diplomacy_accept_treaty_req` applies clauses and then calls
`treaty_remove()`. A treaty is a **negotiation artifact** and should stay
ephemeral. An obligation is **game state** that outlives the negotiation.

Make them different structures:

```c
/* Game state, hangs off the player or the diplstate pair. Saved. */
struct obligation {
  int              id;
  struct player   *obligor, *obligee;
  enum clause_type kind;             /* the promise type */
  struct player   *target;           /* third party, if relevant */
  int              deadline_turn;
  int              escrow_gold;      /* see §4 */
  int              escrow_tech;
  struct requirement_vector fulfil;  /* see §3 */
  enum { OB_PENDING, OB_KEPT, OB_BROKEN, OB_EXPIRED } state;
};
```

Clause execution *creates* an obligation; a per-turn sweep evaluates and
resolves it. Conflating the two is the mistake that will hurt later.

### 2c. Client UI is the underestimated cost

gtk3.22, gtk4, qt, and sdl2 all have diplomacy dialogs, and constructing a
conditional clause needs target and deadline pickers plus a way to display
pending obligations. **This is very likely more work than all the server-side
changes combined**, and it's the part that determines whether the feature is
usable by humans at all.

---

## 3. Reuse `requirement_vector` as the fulfilment condition

This is the design move I'd push hardest.

The temptation with conditional clauses is to invent a trigger language, and
then you own a scripting language forever. But `requirements.h` is already
included in `diptreaty.h`, already used for clause *gating*, already
ruleset-authorable, already savegame-serialisable, and already has a full
evaluation engine with ranges and negation.

Use it for the *fulfilment* condition too:

- "declare war on Persia" → a `DiplRel` requirement, `War`, range Player,
  with Persia as the context
- "don't attack me" → the negation of the same
- "reach Gunpowder" → an `Advance` requirement
- "hold city X" → a requirement on city ownership

You get a condition language for free, rulesets can author new promise types
without C changes, and it serialises to savegame with existing code. The cost is
that requirement vectors weren't designed with a *third-party* context, so
you'll likely need to extend `req_context` to carry an "other player" slot.
That's a contained change and much cheaper than a new DSL.

---

## 4. Escrow — the enforcement primitive you actually want

Your instinct is "a stronger downside if they actively break the treaty." I'd
argue reputation alone can't deliver that, and escrow can.

**Reputation is soft.** A human who breaks a promise in the turn that wins them
the war does not care what the AIs think afterwards. Any purely reputational
penalty is dodgeable by timing.

**Escrow is hard.** Transfer the consideration at *signing* into a locked state:

```c
/* At signing: gold leaves the obligor's treasury immediately. */
obligor->economic.gold -= escrow_gold;

/* At resolution: */
OB_KEPT   → escrow returns to obligor (or transfers to obligee as payment)
OB_BROKEN → escrow forfeits to obligee
OB_EXPIRED→ per-ruleset: return, split, or forfeit
```

This makes promises **self-enforcing with zero trust**. It also gives you the
"stronger downside" directly and proportionally: bigger promise, bigger bond,
bigger loss. And it's mechanically trivial compared to everything else here.

Escrow can't cover everything — you can't escrow "declare war on Persia" — so
the natural split is:

| Promise kind | Enforcement |
|---|---|
| Pay gold/tech later | **Escrow** (fully self-enforcing) |
| Declare war on X by turn N | Escrowed *bond*, forfeited on failure |
| Don't attack me for N turns | Escrowed bond + reputation |
| Cede city on victory | Reputation + casus belli |

So reputation handles only the residue, which is a much smaller job.

---

## 5. Reputation must be game state, not AI memory

If reputation lives in `ai_dip_intel` (as `ally_patience` does), then it's
per-observer, invisible to humans, and a newly-met AI has no priors. It can't be
the basis of pricing.

Worth knowing the history: **Freeciv had reputation and removed it.** The only
survivor is a comment at `server/diplomats.c:1005` — *"It should give a loss of
reputation to steal from a player you are at peace with."* I don't know the
removal rationale, but the standard failure mode for a single opaque scalar is
that players can't tell what moved it or why, so it feels arbitrary.

Design around that: make it a **public, itemised ledger**, not a number.

```c
/* Per player, public, saved */
struct reputation_record {
  int obligations_kept;
  int obligations_broken;
  int obligations_expired;
  /* plus a bounded ring of recent entries, for display */
};
```

Anyone with an embassy can see "kept 7, broke 2, and here's which." A specific
history is legible and defensible in a way a scalar never is.

---

## 6. The risk multiplier, done properly

Your "risk multiplier on the cost depending on whether they've followed through"
is exactly a Bayesian posterior, and it's worth writing it as one:

```c
P(comply) = (kept + ALPHA) / (kept + broken + ALPHA + BETA);
promise_worth = dai_goldequiv_clause(nominal) * P(comply);
```

with a mild prior (`ALPHA = 2, BETA = 1`, say) so unknowns start slightly
trusted rather than at zero. Three properties make this good *gameplay*, not
just good arithmetic:

1. **New players' promises are cheap** — both to buy and to sell. Early
   promise-trading is low-stakes, so players learn the mechanic without ruin.
2. **A clean record becomes a real asset.** After a dozen kept promises, your
   word trades near face value, and that's a genuinely valuable accumulated
   position — you can borrow against it. That's the thing that makes players
   *want* to keep promises rather than merely fear breaking them.
3. **It self-corrects.** One breach after twenty kept promises barely moves the
   posterior; three in a row destroys you. Proportionate without any tuning.

For the AI side, that expression drops straight into `dai_goldequiv_clause()`
with no restructuring — it's already the common-currency valuation function
(Part 7 §3). And it's what lets an AI rationally *accept* a promise instead of
demanding cash: a proven partner's promise is nearly as good as gold, an
unproven one's is worth a fraction.

Add an asymmetry worth having: weight `broken` more heavily than `kept` in the
posterior, or decay `kept` over time while `broken` persists. Trust should be
slow to build and fast to lose, and that also happens to make the mechanic
harder to farm.

---

## 7. Honest assessment

This is the most **upstream-political** item across all eight documents, and
that's a different kind of cost from the others.

Everything else here is an AI change — it affects how `ai/default` plays and
nothing else, so it's judged on whether the AI plays better. This changes the
*game*: it adds capability to human players, changes the diplomacy dialog, and
touches the network protocol and savegame format. That's a much wider review
surface, and it will attract opinions about balance, UI, and scope that a
settler-evaluator fix never would.

Two things make it tractable anyway, and they're both already in the tree:
`clause_info.enabled` means you ship it off by default and let rulesets opt in,
and `add-cap()` means old clients aren't broken. Lead with both when you propose
it.

**Suggested slice for a first version**, deliberately small:

- One new clause type: `CLAUSE_PROMISE_GOLD` (pay N gold on turn T), fully
  escrowed. No requirement vectors, no third-party targets.
- The `struct obligation` list and its per-turn sweep.
- The public reputation record, updated by that sweep.
- AI valuation via the posterior in §6.

That's a complete, self-enforcing, useful mechanic with no trigger language, no
third-party context, and minimal UI — one row in the dialog with a turn spinner.
If it lands, `CLAUSE_PROMISE_WAR` and the requirement-vector conditions from §3
are the natural second step, and the coalition tribute from Part 7 becomes a
first-class treaty object instead of an AI-side convention.
