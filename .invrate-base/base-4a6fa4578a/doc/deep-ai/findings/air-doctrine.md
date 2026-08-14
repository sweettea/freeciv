# W32 — Air doctrine

**Phase 3–4, `ai/deep`. Threads through W08 (doctrine), W12 (empire graph),
W13 (campaigns), W04 (knowledge), W28/W29 (worker infrastructure).**
Root causes: RC2 (no shared plan), RC3 (radius myopia — airbases live outside city
radii), RC6 (air power is a vision/information weapon).

Baseline: `freeciv/freeciv` @ `0896b34`. Stats from `data/civ2civ3/units.ruleset`;
design is ruleset-general via W08.1's probe.

---

## 1. What's actually there — and why planes end up "dead in the air"

Unlike naval, the air AI is not absent. There is a whole `daiair.c` (656 lines),
a `daiparadrop.c`, and fuel-aware pathfinding. It is *almost* working, and it
fails in specific, structural ways rather than by omission.

**What works:** `find_nearest_airbase()` (`daiair.c:89`) does fuel-aware
pathfinding and — confirmed via `common/movement.c:537` — `tile_has_refuel_extra`
means it **already treats airbases and carriers as refuel points**, not just
cities. `find_something_to_bomb()` scores air targets. `dai_manage_paratrooper()`
exists. So the tactical primitives are present.

**Why planes still die:**

1. **The AI never *builds* airbases.** `grep -rn "airbase\|Airbase" autoworkers.c
   daidomestic.c daisettler.c` → **nothing**. Airbases are a worker `Airbase`
   extra, built outside city radii — and per RC3 the worker never looks there.
   So `find_nearest_airbase` almost always finds only *cities*, and air units are
   tethered to city range. A bomber with fuel 3 and move 8 can strike ~1.3 tiles
   past its own cities and must turn back — the AI's air reach is its city
   footprint, nothing more. **This is the single biggest gap and the root of
   "dead in the air."**

2. **Range management is per-unit and reactive.** Each plane individually flies
   out, maybe bombs, and scrambles for the nearest refuel (`daiair.c:528`). If it
   mistimes fuel it sits down wherever it can — often exposed — and
   `is_losing_hp` helicopters get an explicit *"we did our best, which was
   nothing"* (`daiunit.c` in `dai_manage_airunit`'s fall-through). There is no
   *forward staging* — no notion of extending reach deliberately.

3. **No air defence.** `grep -i "SDI\|air.def\|interceptor"` → nothing beyond a
   single `adv->threats.nuclear` flag (`daieffects.c:367`). The AI doesn't keep
   fighters on CAP, doesn't value SAM/SDI buildings against an air-armed enemy,
   doesn't intercept. It is defenceless against exactly the strategy you enjoy.

4. **Whole unit classes unused in practice.** Paratroopers have code but no
   staging to *reach* drop range. AWACS (vision_radius_sq **26**, move 16) — a
   flying radar that would feed W04 enormously — is never built for
   reconnaissance. Nuclear/cruise missiles: only a hunter-context nuke want
   (`daihunter.c:189`); no strategic use. Stealth units are chosen only if they
   happen to win the generic desirability compare.

So the fix is less "write an air AI" than "give the existing one **forward
infrastructure**, a **shared plan**, and a **defensive posture** — plus teach it
the specialist units."

## 2. The units, and what each is *for*

civ2civ3, class `Air` unless noted. Fuel is the leash; move rate is the reach:

| Unit | A | D | HP | MV | Fuel | Defining trait |
|---|--:|--:|--:|--:|--:|---|
| **Fighter** | 4 | 4 | 20 | 10 | 2 | `AirAttacker`. Air superiority — can hit other air. CAP/intercept. |
| **Bomber** | 6 | 2 | 30 | 8 | 3 | `Bombarder`, `OneAttack`. Ground attack only. The hammer. |
| **Stealth Fighter** | 8 | 8 | 20 | **14** | 2 | Fighter that mostly ignores interception. Reach 14. |
| **Stealth Bomber** | 9 | 5 | 30 | **12** | 3 | Bomber that penetrates air defence. The carpet-bomb unit. |
| **Helicopter** | 5 | 3 | 30 | 6 | 0 | `Helicopter` class, no fuel — but `is_losing_hp`, must rest in cities. Bombards; can take territory. |
| **AWACS** | 0 | 1 | 20 | **16** | 3 | vision **26**. Flying radar. Force multiplier for W04. |
| **Nuclear** | 99 | 0 | 10 | 0 | 1 | `Nuclear`, `Missile` class. Strategic. Provokes global consequences. |
| **Cruise Missile** | — | — | — | — | 1 | One-shot precision strike, expendable. |
| **Paratroopers** | 6 | 4 | 20 | 1 | — | `Paratroopers`. Land class. Drops from within range to seize undefended ground. |

Key mechanics that shape doctrine:

- **Fuel is a hard leash.** A unit that can't reach a refuel point at end of turn
  is lost (`movement.c:548`). Reach = a function of move rate, fuel, *and where
  your refuel points are*. Airbases move the refuel points forward. That's the
  whole game.
- **Stealth ignores interception.** Stealth bomber's value isn't the +3 attack
  over a bomber — it's that it penetrates the air-defence net your normal bombers
  can't. Situational: worthless if the enemy has no interceptors, decisive if
  they do.
- **AWACS is an information weapon (RC6).** vision 26 vs 8 is a >3× radius. One
  AWACS orbiting the front feeds the whole knowledge layer and makes every other
  air strike better-targeted. Under honest information this is enormous.

## 3. Doctrine — layers, in build order

### 3.1 Forward airbases — build this first; it's the whole scenario

**The enjoyable strategy you describe is precisely this layer**, so it's the
worked example. A chain of airbases pushed through unclaimed territory turns a
short-legged air force into a continent-spanning strike arm.

Give W12's empire graph an **airbase-chain planner**:

- Identify a strike objective (enemy city cluster) beyond current air reach.
- Plan a chain of `Airbase` extras stepping toward it, each within fuel range of
  the last, ending within strike range of the target. This is a path-plan exactly
  like a road (W12), costed in worker-turns, valued by the strike reach it unlocks.
- Emit **tagged worker wants** (W30 hook) on the chain tiles — using the same
  plan-tile mechanism as roads/canals — so an engineer will build an airbase on a
  tile whose local yield is zero.
- **Escort with one guard unit** (your detail, and it's the correct doctrine):
  the engineers move under a single defender through unclaimed land, because the
  chain is deep in no-man's-land. This reuses W13's bodyguard assignment.
- Use `Stealth`-deployed engineers where the ruleset allows reduced visibility, so
  the chain isn't spotted before it's ready — turning it into a surprise.

When the chain completes, forward-based bombers reach the target that was
untouchable a dozen turns earlier: **carpet bombing out of nowhere.** That is the
"suddenly destroy the enemy" payoff, and it emerges from three plan objects
(airbase chain + escorted engineers + a waiting bomber wing) firing in sequence.

Airbases are also refuel points for *defence* and staging for paratroop/heli
operations, so this layer pays off even without the surprise-strike use.

### 3.2 Air wings — the shared plan (RC2)

Replace per-plane "fly out and maybe bomb" with an **air wing**: a group with a
base (city, carrier, or forward airbase), a mission radius derived from fuel and
that base, and a role. Wing roles:

| Role | Units | Mission |
|---|---|---|
| **Strike** | Bombers, stealth bombers | Hit ground targets softened for a campaign |
| **Superiority** | Fighters, stealth fighters | Clear enemy air; escort strike wings |
| **Recon** | AWACS, fast fighters | Feed W04; find targets and enemy air |
| **Intercept/CAP** | Fighters | Sit over friendly cities/fleets, kill incoming air |

A wing's strikes are **coordinated with campaigns (W13)**: air softens a stack the
turn *before* the land assault or the amphibious landing (ties directly to W31's
bombardment phase — naval guns and bombers do the same softening job, and the
doctrine should pick whichever is in range). Air power stops being random pot-shots
and becomes the preparation phase of a combined operation.

### 3.3 Air defence — stop being the victim

The counter to §3.1 done *to* you. Under W04, when an enemy is assessed to have
air power or forward airbases:

- Value SAM/SDI-type buildings and coastal/air defences in threatened cities
  (they're effects in the ruleset; the AI already tracks `adv->threats.nuclear`,
  generalise it to `threats.air`).
- Keep a **CAP** of fighters over high-value cities and fleets.
- **Detect and raid forward airbases.** An enemy airbase chain in your
  no-man's-land is a visible tell (if you're scouting — AWACS/recon). The
  doctrine should send bombers or a raiding party to break the chain before it's
  used. This is the direct answer to the strategy you enjoy — a competent
  opponent spots the airbase string and bombs it flat.

### 3.4 Specialist units

- **AWACS as standing recon** — build one or two under any air doctrine; park them
  where their vision-26 covers the contested front. Highest-leverage air unit for
  W04, and currently never built for this.
- **Paratroopers** — once §3.1 gives forward staging, paradrop becomes reachable:
  seize an undefended enemy city or a key chokepoint behind the front. Gate on
  W04 confirming the target is weakly held (a paradrop onto a defended tile is
  suicide).
- **Nuclear/cruise missiles** — strategic layer, heavily gated: only under a
  doctrine that has accepted total war (W14 war ledger), only against
  concentrated high-value targets, with explicit modelling of diplomatic
  blowback and the enemy's SDI. The AI should treat nukes as a deterrent and a
  last resort, not a hunter tactic. This is the one layer where "decline to build"
  is usually correct.

## 4. Coupling to doctrine (W08)

Add an **air axis** to the W08 doctrine, parallel to W31's naval axis:

- Enemy with strong air / forward airbases → invest in air defence (§3.3) and air
  superiority before strike.
- Front line stalemated on land → an airbase chain (§3.1) to break it from the
  flank becomes high-value.
- Under honest information with poor map knowledge → AWACS and recon fighters
  first, because you can't strike what you can't see.
- Peaceful / no reachable enemy → minimal air, like naval. Decline the air force
  you don't need.

## 5. Placement and dependencies

All `ai/deep` — needs the empire graph, campaigns, knowledge, and doctrine.

Hard orderings:
- **W12 before §3.1** — the airbase chain is a path-plan in the empire graph.
- **W28/W29 before §3.1** — engineers build the chain; worker stacking (W28) makes
  a multi-airbase chain complete in reasonable time, and the tagged-want mechanism
  (W30) carries the plan tiles.
- **W13 before §3.1 escort and §3.2 wings** — the escorted engineer march and the
  air-softens-then-army-assaults sequence are campaigns.
- **W04 before §3.3 and §3.4** — air defence reacts to *sighted* enemy air; AWACS
  and paradrop targeting are information problems.
- **W14 before §3.4 nukes.**

Build order within W32: **3.1 → 3.2 → 3.3 → 3.4**, air axis folded into W08.

## 6. Acceptance

- **Forward airbases:** the AI builds an airbase outside any city radius when a
  strike target lies beyond current air reach (L1 corpus: target just out of
  range → airbase chain planned toward it).
- **The scenario, end to end:** on a corpus map with an enemy across unclaimed
  land, the AI builds an escorted engineer chain of airbases and then strikes the
  previously-unreachable enemy with forward-based bombers. Assert the sequence in
  the campaign log.
- **Air wings:** bombers strike a stack the turn before a land assault, not at
  random (L1: bomb action precedes assault in campaign log).
- **Air defence:** vs an air-armed enemy, the AI keeps fighters on CAP and builds
  air-defence structures; enemy air losses over friendly cities rise measurably.
- **Airbase raiding:** when the enemy builds a forward airbase in sight, the AI
  targets it (L1 corpus).
- **AWACS:** under an air doctrine the AI fields recon aircraft; measure explored/
  known fraction of the front with vs without.
- **Decline:** landlocked peaceful start builds essentially no combat aircraft.
- **Overall:** planes-lost-to-fuel-starvation per 100 turns drops sharply from
  baseline — the direct metric for "dead in the air."

## 7. Commits (indicative)

```
ai/deep: plan airbase chains toward strike targets beyond air reach
ai/deep: emit tagged worker wants for airbase-chain tiles
ai/deep: march chain engineers under a single escort through no-man's-land
ai/deep: prefer reduced-visibility engineers when deploying forward airbases
ai/deep: model air reach from all refuel points including forward airbases
ai/deep: group air units into wings with a base, radius and role
ai/deep: coordinate strike wings with land and amphibious assaults
ai/deep: assign fighters to combat air patrol over high-value assets
ai/deep: value air-defence structures when enemy air is assessed
ai/deep: detect and raid enemy forward airbases
ai/deep: field AWACS for standing reconnaissance of the front
ai/deep: paradrop onto weakly-held targets once staging is in range
ai/deep: gate strategic missile use behind total-war doctrine and blowback
ai/deep: add an air axis to doctrine from enemy air and front state
ai/deep: decline an air force when no enemy is reachable
test: corpus — airbase chain planned toward an out-of-reach target
test: corpus — escorted engineer chain then forward bomber strike
test: corpus — bombers strike the turn before assault
test: corpus — AI raids a sighted enemy forward airbase
test: metric — planes lost to fuel starvation per 100 turns
```

---

## 8. The one-paragraph version

The air AI isn't missing — it's *tethered*, because it never builds the
**forward airbases** that would let planes reach past their own cities, so they
fly out, mistime fuel, and sit down to die. Give W12 an **airbase-chain planner**
that pushes refuel points through no-man's-land under a single escort (your
scenario, and the cheapest high-impact fix), group planes into **wings** with a
base and a role that strike **in sync with campaigns** (W13) the way naval guns do
(W31), add the **air defence** that makes the AI stop being a victim of exactly
that airbase-chain strike, and teach it the specialists — **AWACS** as a vision-26
force multiplier for W04, **paratroopers** once staging puts them in range,
**stealth** to beat interception, and **nukes** as a heavily-gated last resort.
Turn it on or off with a **doctrine** that knows whether air matters this game.
