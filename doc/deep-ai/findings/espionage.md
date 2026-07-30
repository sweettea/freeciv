# W35 — Espionage and intelligence

**Phase 3–4, `ai/deep`. Threads through W04 (knowledge), W14 (war ledger),
W08 (doctrine), W34 (victory plan).**
Root causes: RC4 (incommensurable wants — spy actions are on their own private
scale), RC6 (espionage is the *mechanism* for the information the AI currently
gets for free), RC1 (fixed constants, no situational value).

Baseline: `freeciv/freeciv` @ `0896b34`. `ai/default/daidiplomat.c` (881 lines),
`daiactions.c`. Stats/actions from civ2civ3.

---

## 1. What's there — a priority ladder disguised as a valuation

The espionage AI is not absent. `dai_choose_diplomat_offensive` builds spies,
`dai_diplomat_city` picks an action on arrival, `dai_diplomat_defend` counters
enemy spies, `dai_diplomat_bribe_nearby` bribes units. It functions. But the
*valuation* is the problem, and the code says so itself.

`dai_action_value_unit_vs_city()` (`daiactions.c:96`) — the function that decides
what a spy does — opens with:

> *FIXME: This is just a copy of how dai_diplomat_city() used to behave. The
> utility values are not proportional to utility values used for other tasks in
> the AI... Feel free to adjust the utility values.*

And the body is a **fixed ladder of magic constants**:

```c
Establish Embassy            utility += 10000
Steal Tech (untargeted)      utility +=  9000
Steal Tech (targeted)        utility +=  8000
Incite City                  utility +=  7000   (if affordable)
Sabotage City                utility +=  6000
Targeted Sabotage            utility +=  5000
Sabotage Production          utility +=  5010
Steal Gold                   utility +=  4000
Steal Maps                   utility +=  3000
Spread Plague                utility +=  2500
```

This is RC4 in miniature: the numbers encode a *ranking*, not a *value*. Stealing
tech is always worth 9000 whether the tech is a game-changer or a footnote.
Inciting a city is 7000 flat, independent of whether it's a size-2 backwater or
the enemy capital with three wonders — the incite *valuation* in the build
function even carries `FIXME: we should count wonders too`. Sabotage is 6000
whether the target is building a Palace or a Temple.

**So the AI performs espionage; it doesn't reason about it.** It runs down a
priority list and does the first affordable thing.

## 2. The deeper problem: espionage is disconnected from intelligence

The single most important spy action is **Establish Embassy** — correctly ranked
highest at 10000. But *why* it matters is invisible to the current AI, because
the current AI already knows everything (RC6). An embassy is scored high by a
constant, not because the AI understands it needs the information.

**Under W04 (honest information), this inverts into the central point:**

> An embassy is the *only* legitimate way to know an enemy's exact tech, treasury,
> and government. Espionage stops being a menu of nasty tricks and becomes the
> AI's **primary intelligence-gathering apparatus.**

Once the AI can't see enemy tech for free, the questions become real:
- *Who is ahead of me in tech?* → needs an embassy or a tech-steal.
- *Can I afford to incite that city?* → needs to know their incite defence,
  visible only with intel.
- *Is that stack real or a feint?* → needs a spy or recon.
- *Who is racing me for this wonder?* (W34) → needs to see their production.

Espionage and the knowledge layer are the same subsystem viewed from two sides:
W04 is *what the AI knows*; W35 is *how it comes to know it, and what it does with
the knowing.* Neither is complete without the other, which is why W04 is a hard
dependency.

## 3. Design — value spy actions in real terms

Replace the constant ladder with valuations in the unified want currency (W1),
each scaled by the actual stakes:

| Action | Value driver (not a constant) |
|---|---|
| **Establish Embassy** | Intelligence value = `intel_staleness(target) × how much a decision depends on that intel`. High when contemplating war/wonder-race with an unseen rival; low for a well-known neighbour. This is the W04 reconnaissance-want formula applied to a permanent intel source. |
| **Steal Tech** | *Value of the specific tech to me* — bulbs saved × how much my plan wants it (W34). A military tech before a war, or the tech that unlocks my victory path, is worth far more than the flat 9000. Targeted steal picks the tech my plan needs. |
| **Incite City** | `city_worth(target)` — the same catchment/improvement/wonder valuation W2 uses — minus incite cost, versus the cost of taking it militarily. Count wonders (the existing FIXME). A cheap incite of a wonder-city can be the single best move in the game. |
| **Sabotage Production** | *What are they building?* Sabotaging a wonder the turn before completion, or a key defensive unit mid-war, is worth a great deal; sabotaging a Temple is noise. Needs W04 to know the target's production — currently the AI sabotages blind. |
| **Steal Gold / Maps** | Gold: as fraction of their treasury vs my need (fund a rush, an incite). Maps: intel value under fog — high early, near-zero once explored. |
| **Spread Plague** | Situational and reputationally costly (W14 blowback); gate hard. |

The key change is that every value now reads **assessed enemy state (W04)** and
**my strategic need (W34)**, so the same action is worth different amounts in
different situations — which is what "reasoning about espionage" means.

## 4. Design — espionage as an operation (W33)

A spy mission is a multi-turn operation, not a per-turn impulse:

- **Intelligence operations:** establish and *maintain* embassies with every
  rival the AI has strategic exposure to. Embassies decay (spy expelled,
  diplomatic break); a standing intel doctrine keeps them refreshed. This is the
  W04 staleness clock made actionable.
- **Pre-war intelligence sweep:** before a W33 assault operation commits, a spy
  phase confirms the target's real garrison and production (turning W04's
  "assess" into "verify"). The paradrop-into-emptied-city plan (W32) especially
  needs this — a spy that confirms the garrison, or *sabotages* it, is part of
  the assault's PREP phase.
- **Incite campaigns:** inciting a city is an alternative to an AMPHIB/LAND
  assault operation — same objective (take the city), different method (gold not
  blood). The operations layer should evaluate incite and assault as *competing
  plans for the same objective* and pick the cheaper. The AI currently never
  compares them; incite is a diplomat's local choice, assault is the military's.
- **Counter-intelligence:** `dai_diplomat_defend` exists but is reactive. Under a
  doctrine facing an espionage-capable enemy, station spies in high-value cities
  (wonder cities, the capital) as standing defence, and value the ruleset's
  counter-espionage buildings.

## 5. Coupling to doctrine and victory (W08, W34)

Espionage intensity is a doctrine axis:

- **Behind in tech** → weight tech-theft embassies heavily; espionage is a
  catch-up engine.
- **Rich but small** → incite over conquer; buy cities the army can't take.
- **Going for a builder/space victory (W34)** → defensive counter-intel to
  protect the wonder-cities and the spaceship; offensive sabotage to slow a
  rival's spaceship (ties to the existing `daidiplomacy.c` spaceship-threat code,
  which currently only *threatens war* — sabotage is the subtler tool it lacks).
- **Total war (W14)** → the full offensive suite unlocked, blowback discounted.

## 6. Placement and dependencies

`ai/deep`. Hard dependencies:
- **W04** — the entire point is intelligence under uncertainty; on omniscience the
  embassy is pointless and the valuations can't be computed honestly.
- **W1** — spy-action values must be in the shared currency to compare against
  building/unit/military wants (kills the constant ladder, RC4).
- **W2** — incite/sabotage value reuses city-worth scoring.
- **W33** — spy missions are operations; incite competes with assault.
- **W34** — tech-steal and sabotage targeting read the victory plan's needs.

Must precede: nothing hard, but it makes W33's assault operations much cheaper
(intelligence + incite as alternatives to force).

## 7. Acceptance

- **Real valuation:** the value of stealing a tech varies with the tech's use to
  the AI's plan — assert two corpus positions with the same target city but
  different AI tech needs produce different steal priorities.
- **Embassy for intelligence:** under honest information, the AI establishes
  embassies with rivals it is contemplating war against *before* committing the
  assault (L1 corpus).
- **Incite vs assault:** given a wonder-city cheaper to incite than to storm, the
  AI incites; given the reverse, it storms (two corpus positions, opposite
  choices).
- **Targeted sabotage:** the AI sabotages a rival's near-complete wonder or key
  war production, not a random Temple (L1 corpus — assert target selection).
- **Counter-intel:** facing an espionage-active enemy, the AI defends its
  wonder-cities with counter-spies and buildings.
- **Overall:** in a tech-race game, an espionage-doctrine `deep` closes a tech gap
  measurably faster than baseline via theft, without triggering wars it can't win.

## 8. Commits (indicative)

```
ai/deep: value spy actions in the unified want currency
ai/deep: value stolen tech by its use to the current plan
ai/deep: value incite by city worth minus cost versus assault cost
ai/deep: value sabotage by the target's actual production and buildings
ai/deep: value an embassy by intelligence staleness and decision dependence
ai/deep: establish and maintain embassies with strategically exposed rivals
ai/deep: run a pre-assault intelligence phase confirming garrison and production
ai/deep: evaluate incite and assault as competing plans for the same city
ai/deep: station counter-spies in high-value cities under espionage threat
ai/deep: sabotage a rival spaceship instead of only threatening war
ai/deep: add an espionage-intensity axis to doctrine
test: corpus — tech-steal priority tracks the AI's own tech needs
test: corpus — embassy established before an assault is committed
test: corpus — incite chosen over assault when cheaper, and vice versa
test: corpus — sabotage targets a near-complete wonder not a temple
test: metric — tech gap closed by theft in a race game vs baseline
```

---

## 9. The one-paragraph version

The AI already runs spies, but it runs them off a **fixed priority ladder of magic
constants** (`daiactions.c:96`, whose own FIXME admits the values are arbitrary and
unscaled): steal-tech is always 9000, incite always 7000, regardless of whether
the tech matters or the city is a capital. It performs espionage without reasoning
about it. The fix is to **value every spy action in real terms** — a stolen tech
worth what that tech does for my plan, an incite worth the city's actual worth
minus its cost *versus* storming it, a sabotage worth what they're actually
building — all of which requires knowing the enemy's true state, which under
honest information (W04) is exactly what **an embassy is for.** That's the deep
point: espionage and the knowledge layer are one subsystem — W04 is what the AI
knows, W35 is how it finds out and what it does with knowing — so a spy that
establishes an embassy before a war, steals the tech my victory plan needs,
sabotages the wonder a rival is about to finish, or incites the city my army can't
afford to take, is the AI finally *playing* the intelligence game instead of
running down a menu.
