# W31 — Naval doctrine

**Phase 3–4, `ai/deep`. Threads through W08 (doctrine), W12 (empire graph),
W13 (campaigns), W15 (geography), W18 (logistics).**
Root causes: RC2 (no shared plan), RC1 (no horizon), RC3 (radius myopia — the sea
version).

Baseline: `freeciv/freeciv` @ `0896b34`. Stats below read from
`data/civ2civ3/units.ruleset` and `effects.ruleset`; the design is
ruleset-general and drives off W08.1's probe, but the worked numbers use civ2civ3.

---

## 1. The problem

The AI has no navy — it has *boats*, chosen one at a time by the same greedy
per-unit logic as everything else.

`dai_choose_attacker(TC_OCEAN)` (`daimilitary.c:143`, called at `:2033`) walks
every buildable sea unit, scores each with the **generic**
`dai_unit_attack_desirability`, and returns the single best. That's the entire
naval build story. Consequences, all confirmed by grep returning nothing:

- **No fleet.** No concept of a group of ships acting together.
- **No sea control.** `grep -rn "blockade\|patrol\|escort\|naval_superiority" ai/`
  finds only two *comments* about blockaded cities — no logic.
- **No combined arms.** A destroyer and a battleship are just two desirability
  scores; the AI never assembles the mix that makes each effective.
- **Anti-bombard defence is a self-described stub** — `daimilitary.c:606`:
  *"it's a dummy support for anti-bombard defense just to do something against
  approaching bombarders. Some better logic is needed, see OSDN#41778."*
- **Offensive bombardment is unused.** The `Bombard` action exists
  (`effects.ruleset:4535`); no AI code initiates it.
- **Transports cross contested water alone.** The ferry layer is
  "statistical mechanics" (W18), and nothing escorts it.

The result on water is what the land AI looked like before campaigns (RC2): units
trickle at targets individually and die. The fix is the same shape — a shared plan
— but the sea has its own physics, and the doctrine has to respect them.

## 2. The units, and what each is *for*

The synchrony this workstream is about comes from the fact that these ships have
sharply different roles. From civ2civ3, all class `Sea`, all `vision_radius_sq 8`,
all `BadCityDefender`:

| Unit | A | D | HP | MV | Cap | Defining trait |
|---|--:|--:|--:|--:|--:|---|
| **Destroyer** | 4 | 4 | 30 | **6** | 0 | Fast. **Anti-sub weapons double its defence vs Submarines.** The screen. |
| **Cruiser** | 6 | 6 | 30 | 5 | 0 | Generalist escort. |
| **Aegis Cruiser** | 8 | 8 | 30 | 5 | 0 | Air/missile defence umbrella (Defend_Bonus vs air). |
| **Battleship** | **12** | **12** | 30 | 4 | 0 | Capital ship. Firepower and staying power. Slow. |
| **Submarine** | **12** | 5 | 20 | 5 | 8 | `vision_layer=Subsurface` (stealthy). `Only_Native_Attack`. Glass cannon. |
| **Carrier** | 0 | 9 | **40** | 5 | 8 | Mobile air base. `Provoking`, `HasNoZOC`. Helpless alone. |
| **Transport** | 0 | 3 | 30 | 5 | 8 | The cargo. Cannot attack. `HasNoZOC`. |

Three mechanics turn this table into doctrine:

**Submarine stealth is a detection game.** Subs sit at the `Subsurface` vision
layer — invisible unless an enemy has subsurface vision (destroyers, coastal bases
with `vision_subs_sq`). So a sub is a scout and an ambush striker with A12, but
`Only_Native_Attack` means **it can only hit ships, never shore targets or cities**,
and D5/HP20 means anything that *finds* it kills it. The destroyer's doubled
anti-sub defence is the hard counter. Sub warfare is therefore rock-paper-scissors:
subs threaten capital ships, destroyers threaten subs, battleships threaten
destroyers.

**Shore attacks gut the defender.** `game.ruleset`: `low_firepower_pearl_harbor=1`
— when a sea unit attacks a target *on a city tile*, the attacker's firepower
doubles and the defender's drops to 1. Battleships shelling a coastal city are
devastating; the same battleship *defending* in a port is fragile. This dictates
that fleets fight in open water, not tucked in harbours.

**Bombardment softens without capturing.** The `Bombard` action damages a stack
without taking losses or the tile (`effect_attack_bombard_no_city_destruction`).
It's the pre-assault artillery of an amphibious operation and the AI never uses it.

## 3. Doctrine — five layers, built in order

### 3.1 Escort — build this first (cheapest, highest visible payoff)

**Rule:** a transport or carrier never crosses water the AI doesn't control
without a warship screen. Implement as a precondition on any W18 ferry route and
any W13 amphibious campaign: the operation does not *launch* until an escort of
sufficient strength is assigned and co-located.

Escort strength is sized against **assessed** enemy naval presence on or near the
route (W04 knowledge — remembered sightings, not ground truth). Screen composition:
at least one destroyer if subs are possible (the anti-sub doubler), plus a cruiser
or battleship sized to the heaviest surface threat seen.

This alone ends the single most visible naval failure — the lone transport
wandering into a battleship — and it reuses the campaign-assignment machinery W13
already builds. **Escort is a precondition, not a new subsystem.** That's why it's
first.

### 3.2 Sea control — the water analogue of W12

Build a **sea-lane graph** over the water: nodes are water regions (from the
per-player known-tile components, W04/W27), edges are the crossings your
transports, trade routes, and campaigns actually use. Value controlling a lane by
the traffic that depends on it, exactly as W12 values a road.

A lane is *controlled* when the AI has naval presence that can intercept hostiles
on it and no un-countered enemy presence sits astride it. Controlling the lanes
between your continents and toward your enemy is naval superiority, expressed as a
graph property rather than a vibe.

This produces **patrol** and **blockade** wants:
- **Patrol** — station a fleet on a friendly lane to keep it controlled (protects
  the escort routes and trade).
- **Blockade** — station a fleet on an enemy's lane to deny it (their
  transports can't cross, their island cities starve of trade). Note the two
  in-source comments about blockaded cities losing trade — the *mechanic* the AI
  suffers from but never *inflicts*.

Sea control is where subs and destroyers earn their keep: subs to contest a lane
you can't hold openly (stealth denial), destroyers to sweep subs off a lane you
need.

### 3.3 Fleet composition — combined arms as a build target

Replace "pick the single best sea unit" with a **fleet template** sized to the
doctrine's current naval task. A fleet is a set of roles, and the AI builds toward
filling them:

| Role | Filled by | Why |
|---|---|---|
| Core firepower | Battleship (fallback Cruiser) | Kills surface ships; shells coastal cities |
| Anti-sub screen | Destroyer | The doubled anti-sub defence; also fast recon |
| Air/missile umbrella | Aegis Cruiser | Only if the enemy has air/missiles seen |
| Stealth strike / scout | Submarine | Ambush capital ships; scout under fog |
| Mobile air | Carrier + air | Only under an air-heavy doctrine, and only escorted |

Composition is *situational*, driven by W04's assessment of what the enemy floats:
all-surface enemy → battleships + cruisers; sub threat → weight destroyers;
air threat → an aegis in the screen. The template is the shared plan (RC2) that
makes the ships act in synchrony instead of arriving one at a time.

Value naval units by their **role contribution to a needed fleet**, not by generic
attack desirability — a destroyer's want should spike when a sub threat is assessed
and be near zero when the sea is empty, which the current generic score can't
express.

### 3.4 Offensive bombardment — the amphibious opener

Fold naval fire support into the W13 amphibious campaign as an explicit phase.
Sequence of a proper landing:

1. **Recon** the beachhead (sub or destroyer; W04 requires it before committing).
2. **Bombard** the defending stack with battleships to soften it — using the
   `Bombard` action, and exploiting `low_firepower_pearl_harbor` when the target
   sits on the coastal city tile.
3. **Land** the assault troops from escorted transports onto the beachhead.
4. **Screen** the beachhead until the land campaign takes the city.

This is the concrete meaning of "subs/destroyers/battleships in synchrony": the
sub found the target and screens against enemy subs, the destroyers hold the lane
and hunt counter-attacking subs, the battleships bombard then guard the transports,
the transports land the army. One operation, four ship types, each doing the thing
only it can do — expressed as an ordered campaign in W13, not four independent unit
decisions.

### 3.5 Coastal defence in depth — contest the crossing, not the beach

Replace the reactive, partly-broken shore defence (W15: fires on one-tile lakes,
`adv->oceans[].size` computed and never read) with defence keyed on the sea-lane
graph:

- Detect the lanes by which an enemy could reach your coast (their controlled
  lanes adjacent to your territory).
- Contest those lanes with patrols **before** a landing, rather than massing
  land defenders in every coastal city after.
- Keep a mobile reserve fleet that can converge on a threatened approach — the
  naval version of W11's incoming-defence accounting.

A competent naval defender fights the invasion at sea, where transports are D3 and
helpless, not on the beach after the army has landed.

## 4. Coupling to doctrine (W08)

The missing signal today is that **water changes what kind of empire you are.**
Add a naval axis to the W08 doctrine probe/state:

- **Island or archipelago start**, or a primary enemy across water, shifts the
  economy axis toward coastal cities, harbours, offshore platforms, and shipyards,
  and the military axis toward a fleet — the way a land border shifts toward
  defenders. Offshore Platform (`effects.ruleset:2287`) turns ocean tiles into
  shield producers, which is what funds a navy; the AI should value it under a
  sea doctrine and ignore it inland.
- **Continental start with no reachable enemy** keeps naval minimal — a few
  escorts for exploration and trade, no battle fleet. The doctrine should *decline*
  to build a navy it doesn't need, which is as important as building one it does.

This is the "you are a sea power now" switch the AI currently lacks. It's what
makes naval investment proportionate instead of the flat, tiny, per-city dribble
that `dai_choose_attacker(TC_OCEAN)` produces today.

## 5. Placement and dependencies

Per the placement rule, all of this is `ai/deep` — it needs the knowledge layer,
doctrine, campaigns, and the empire graph, none of which exist in `ai/default`.

Hard orderings:
- **W04 before all of it** — every threat assessment is "what have I *seen*", and
  sub warfare is *entirely* an information game. Naval on ground-truth is both a
  cheat and, ironically, easier than the honest version; build it honest.
- **W12 sea-lane graph before 3.2 sea control** — control is a graph property.
- **W13 before 3.1 escort and 3.4 bombardment** — both are campaign phases.
- **W18 before 3.1** — escort attaches to ferry routes.
- **3.1 escort before everything else naval** — cheapest, highest payoff, and it
  makes every later naval operation survivable.

Suggested build order within W31: **3.1 → 3.3 → 3.2 → 3.5 → 3.4**, doctrine
coupling (§4) folded into W08 whenever W08 lands.

## 6. Acceptance

- **Escort:** transports/carriers crossing contested water without a screen drop
  to ≈0 (L1: assert a ferry route does not launch unescorted when enemy naval
  presence is assessed on the lane).
- **Sea control:** on a two-continent map, the AI establishes and holds at least
  one lane between its landmasses; measure lane-control turns.
- **Combined arms:** destroyer build want rises measurably when an enemy submarine
  is sighted and falls when none is (L1 on a corpus position).
- **Bombardment:** in an amphibious campaign, coastal defenders are bombarded
  before troops land (L1: assert bombard precedes disembark in the campaign log).
- **Doctrine:** on an island start the AI builds harbours + offshore platforms +
  a fleet; on a landlocked start it builds essentially no warships. Two corpus
  positions, opposite outcomes.
- **Overall:** on archipelago maps, `deep` vs `baseline` score ratio improves by
  more than it does on Pangaea maps — i.e. the naval work shows up specifically
  where water matters. This is the headline test and it guards against naval
  effort that doesn't actually help.

## 7. Commits (indicative; expand per sub-layer when scheduled)

```
ai/deep: build a sea-lane graph over known water regions
ai/deep: value sea-lane control by dependent transport and trade traffic
ai/deep: require a sized warship escort before launching a ferry route
ai/deep: require an escort before launching an amphibious campaign
ai/deep: define fleet role templates and build toward filling them
ai/deep: size fleet composition from assessed enemy naval presence
ai/deep: raise anti-sub destroyer want when submarines are sighted
ai/deep: emit patrol wants to hold friendly sea lanes
ai/deep: emit blockade wants to deny enemy sea lanes
ai/deep: add a bombardment phase to amphibious campaigns
ai/deep: bombard coastal defenders before disembarking troops
ai/deep: contest invasion sea lanes with patrols before landings
ai/deep: keep a mobile reserve fleet for threatened approaches
ai/deep: add a naval axis to doctrine from start geography
ai/deep: value harbours and offshore platforms under a sea doctrine
ai/deep: decline a battle fleet when no enemy is reachable by sea
test: corpus — unescorted ferry declines to launch into naval threat
test: corpus — destroyer want responds to submarine sightings
test: corpus — bombard precedes disembark in an amphibious assault
test: corpus — island vs landlocked start produce opposite naval builds
test: metric — naval score gain on archipelago exceeds gain on Pangaea
```

---

## 8. The one-paragraph version

Give the AI a **fleet** instead of boats: a shared plan (RC2) sized to what it has
actually *seen* of the enemy (RC6/W04), moving over a **sea-lane graph** it values
like roads (W12), where each ship does the one thing only it can — battleships
shell coasts and anchor the line, destroyers screen the subs, subs scout and ambush
capital ships from stealth, transports never sail unescorted — assembled into
**amphibious operations** that recon, bombard, land, and screen as one campaign
(W13), and turned on or off by a **doctrine** that knows whether water is central
to this game (W08). Build escort first; it's cheap and it stops the bleeding.
