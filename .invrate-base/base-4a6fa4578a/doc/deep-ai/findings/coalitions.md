# Part 7 — Coalitions: war leadership and tribute

Companion to Parts 1–6. Same tree: `freeciv/freeciv` @ `0896b34`.

---

## 1. The rules engine cannot express this, and that's fine

`common/diptreaty.h:24` — note the comment on the first line:

```c
/* Used in the network protocol */
#define SPECENUM_NAME clause_type
  CLAUSE_ADVANCE, CLAUSE_GOLD, CLAUSE_MAP, CLAUSE_SEAMAP, CLAUSE_CITY,
  CLAUSE_CEASEFIRE, CLAUSE_PEACE, CLAUSE_ALLIANCE, CLAUSE_VISION,
  CLAUSE_EMBASSY, CLAUSE_SHARED_TILES
```

Eleven clause types. Every one is either a **one-shot transfer** (gold, tech,
map, city) or a **diplomatic state change** (ceasefire, peace, alliance). And
treaties are executed at signing and then destroyed — `handle_diplomacy_accept_treaty_req`
iterates the clauses, applies them, and calls `treaty_remove()`.

There is no:

- recurring obligation ("10 gold per turn for 20 turns")
- conditional clause ("if you declare war on Persia, I give you Gunpowder")
- third-party clause ("declare war on X")
- escrow, collateral, or enforcement primitive
- hierarchy or command relationship

And because the enum is in the network protocol, **adding a clause type is a
wire-format break** — every client and server must match. That's a hard argument
against solving this in the rules layer.

---

## 2. The codebase already shows you how to route around it

`ally_patience` (`daidata.h:63`) is precisely a standing expectation held in AI
memory, invisible to the rules engine, persisted across saves
(`daiplayer.c:93, 136`), and enforced purely by AI behaviour.

Here's the whole mechanism (`daidiplomacy.c:1852–1895`). If an ally isn't at war
with your enemy:

1. `ally_patience == 0` → *"We call upon you to destroy our enemy, %s."*
2. `ally_patience == -1` → *"Why do I need to remind you of your promises?"*
3. below that → *"Dishonored one... our alliance is no more!"* and
   `handle_diplomacy_cancel_pact()`.

So there's already an obligation-with-escalation pattern in the tree. It's just
a bad one:

| Problem | Detail |
|---|---|
| **Purely rhetorical** | Three chat messages. No offer, no payment, no compensation for compliance. |
| **Binary compliance test** | `WAR(pplayer, eplayer) && !pplayers_at_war(aplayer, eplayer)` — it checks whether the ally *declared*, never whether they fought, moved a unit, or took a city. Declare war and sit still: fully compliant forever. |
| **Symmetric** | Every ally nags every other ally identically. No leader, no junior, no division of labour. |
| **One enforcement action** | Dissolve the alliance. Which mid-war often hurts the nagger more than the defector, so it's not credible. |

**The design conclusion:** build the coalition entirely as an AI-side convention
over existing one-shot clauses, repeated on a schedule. Tribute becomes "a
voluntary `CLAUSE_GOLD` gift every N turns." No protocol change, no ruleset
change — and it works against human players too, since a human can pay or not
pay and the AI just tracks it.

---

## 3. The structure

```c
struct coalition {
  struct player *leader;
  bv_player       members;
  struct player  *target;          /* the shared enemy */
  int             formed_turn;
  enum { COAL_FORMING, COAL_ACTIVE, COAL_DISSOLVING } phase;
};

/* Per member, in ai_dip_intel — it already persists via player_save_relations */
struct coalition_ledger {
  int tribute_owed_per_interval;   /* in gold-equivalent */
  int tribute_paid_total;
  int tribute_missed_intervals;
  int contribution_score;          /* actual war effort, see §5 */
  int leader_debt;                 /* what the LEADER owes this member */
  signed char patience;            /* graded, replacing ally_patience */
};
```

`dai_goldequiv_clause()` (`daidiplomacy.c:~140`) is already a common-currency
valuation across every clause type — that's literally what it's for. Use it as
the unit of account for tribute, leader obligations, and spoils alike, so all
three are comparable.

---

## 4. Who leads

Needs a computable claim, or you get two AIs each convinced they're in charge.

```c
leadership_claim(pplayer, target) =
    military_power(pplayer)                      /* ATTACK_POWER sum        */
  + total_shield_surplus(pplayer) * W_PROD
  - player_distance_to_player(pplayer, target) * W_DIST   /* proximity      */
  + (already_at_war(pplayer, target) ? W_COMMITTED : 0)
  + tech_lead(pplayer) * W_TECH
```

Highest claim leads. Two constraints:

- **Hysteresis**, same as the doctrine probe (Part 3 §2c): a challenger must beat
  the incumbent by ≥25% for two consecutive evaluations. Leadership flapping
  mid-war is worse than either leader.
- **Freeze during `CAMP_MOVING`/`CAMP_ASSAULT`** (Part 1 §3). You cannot change
  command while the stacks are walking.

---

## 5. Fixing the compliance test

This is where `ally_patience` fails hardest, and where your idea gets its teeth.
Contribution must be measured in *effort*, not *declaration*:

```c
contribution_score(member, coalition) =
    shields_of_target_units_destroyed  * W_KILLS
  + target_cities_captured             * W_CITIES
  + units_committed_within_N_of_front  * W_PRESENCE
  + tribute_paid_gold_equiv            * W_TRIBUTE
  + techs_gifted_gold_equiv            * W_TECH
```

Note tribute and fighting are **substitutes** in the same currency. That's the
heart of what you're proposing: a junior partner who can't fight effectively
pays instead, and the ledger treats both as contribution. A member is compliant
when `contribution_score >= expected_share`, where the expected share is
proportional to their capacity, not equal across members — a size-4 rump state
owes less than a peer power.

The war ledger from Part 3 §4 (`DIP_SUE`) gives you most of these counters.
Build it once, use it three times.

---

## 6. The hard part: why would anyone stay a junior?

Tribute without reciprocity is extortion, and a rational junior defects. So the
leader must **owe** things, tracked in `leader_debt`:

1. **Take the front line.** The leader's campaign targets the enemy cities
   nearest the *junior's* border, not just the juiciest ones. Costly, legible,
   and the main thing a junior is buying.
2. **Gift defensive tech.** Currently impossible — `dai_share()` gates the tech
   donation loop on `players_on_same_team` (Part 5 §6c), so a non-team ally gets
   nothing. Must be fixed for this to work at all.
3. **Defend junior cities.** Currently the AI *actively refuses* —
   `dai_military_defend()` drops any charge whose city isn't its own
   (Part 5 §6a), and `assess_danger` never even looks at allied cities. Also must
   be fixed.
4. **Spoils.** `CLAUSE_CITY` is never proposed by the AI today (Part 3 §2c). A
   post-war transfer of a captured border city is the natural payoff to a junior
   who paid in gold rather than blood.

So this proposal is what makes the Part 5 alliance gaps worth closing. On their
own they're niceties; as coalition obligations they're load-bearing. I'd flag
that as the strongest argument for doing §6a and §6c at all.

**Caveat on spoils** — cross-reference Part 2 §3c: a city transferred from
leader to junior is full of the *leader's* nationals, who migrate back toward
the leader's cities at 2× attractiveness. A gifted city is a leaky payment. If
migration is active, price it accordingly, or prefer gold and tech as the
settlement currency.

---

## 7. Free-riding, and a graded ladder

The failure mode is a junior who pays nothing, declares war nominally, and
banks the security for free. `ally_patience`'s single response — dissolve the
alliance — isn't credible mid-war. Grade it:

| Shortfall | Response |
|---|---|
| 1 interval | Private reminder. Nothing changes. |
| 2 intervals | Suspend leader obligations: stop tech gifts, stop defending their cities. **Cheap, immediate, reversible.** |
| 3 intervals | Withdraw shared vision; exclude from spoils allocation. |
| 4+ | Downgrade alliance to peace. Optionally: mark as a future `dai_war_desire` target with a `has_reason_to_cancel` bump. |

Suspension at step 2 is the important one — it's the punishment that costs the
leader *nothing* (they stop spending) while costing the defector real security.
That's what makes the threat credible, which is what makes the arrangement
stable.

Symmetrically, a **leader** who takes tribute and doesn't deliver should lose
the coalition: if `leader_debt` stays unpaid for several intervals, members
re-run leader selection and the incumbent's claim takes a large penalty.

---

## 8. Exploitability

An AI that pays tribute is an AI a human will farm. Two guards:

- **Tribute is priced against received benefit**, never against demand. Compute
  what the leader has actually delivered (`leader_debt` paid down) over the last
  N intervals and cap tribute at some fraction of it. An AI that has received
  nothing pays nothing, regardless of how loudly the leader asks.
- **Never let a human be leader by default.** A human's `leadership_claim`
  should be evaluated on the same terms, but with the delivery history weighted
  much more heavily, since a human will happily promise and not deliver.

Also worth gating the whole subsystem behind the `diplomacy` server setting the
same way existing diplomacy is, and behind `H_DIPLOMACY` for novice AIs.

---

## 9. Positioning against teams

Freeciv already has the rigid version of this: **teams**. Shared research, fixed
at game start, no defection possible, `players_on_same_team` checked in a dozen
places. What you're describing is the *dynamic, negotiated, defectable* version
— formed in response to a threat, with a ledger, and dissolvable.

That's the pitch, and it's a good one: teams are a lobby setting, coalitions are
gameplay. It's also the single most visible AI behaviour in any of these seven
documents — a human player watching three AIs elect a war leader, pool tribute,
and hold a defector to account will notice immediately, in a way they'd never
notice a better settler evaluator.

---

## 10. Where it sits in the phasing

Late. It depends on:

- Part 3 §4's war ledger (compliance counters)
- Part 5 §6a (allied danger assessment — you can't defend an ally you can't see
  is in trouble)
- Part 5 §6c (tech gifting to non-team allies)
- Part 1 §3's campaign object (the leader's shield demand is what juniors help
  satisfy, and what tribute converts into)

But it's worth writing the `coalition_ledger` struct early, because the war
ledger and the coalition ledger are nearly the same object, and building them
together is much cheaper than retrofitting.

One implementation note: coalition role fits Part 3's doctrine axes as a
modifier on the per-player `stance[]` array plus a military-posture override.
Being a junior means your campaign target is *the leader's* target and part of
your `budget_mil` services their deficit rather than your own. That falls out of
the existing allocator without new machinery.
