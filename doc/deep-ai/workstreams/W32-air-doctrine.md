# W32 — Air doctrine

**Phase 3, ai/deep. Phase lists over W33.**
Full design: `findings/air-doctrine.md` + `findings/air-doctrine-addendum.md`.
Root causes: RC2, RC3-in-air, RC6.

## The problem — planes "dead in the air"
Air is NOT absent (there's `daiair.c`, `daiparadrop.c`, fuel-aware PF; refuel
already honours airbases via `movement.c:537`). It's **tethered**:
- The AI never BUILDS airbases (`grep airbase autoworkers.c daidomestic.c` = nothing).
  Per RC3 the worker never looks outside city radii, so air reach = city footprint.
- Mission planning is target-first: leaves base, flies at target, THEN looks for
  fuel (`daiair.c:528`). `TODO: support for fuel > 2` — worst for long-range units.
- No air defence (one `adv->threats.nuclear` flag, nothing else).

## Build order (addendum A is foundational)
1. **Return-first planning** (addendum A): a mission is valid only if the ROUND
   TRIP closes — strike tile → refuel reachable AFTER the strike (allied bases
   count) → within fuel. Suicide missions become an explicit tagged choice, not a
   fuel-mismanagement accident. Wing radius = round-trip fuel.
2. **Cruise missiles** (addendum C): `Missile` class = consumed on attack, so no
   return leg applies. Treat as AMMUNITION — build for a queued strike, value =
   expected damage − cost, NO survival term. The right tool for no-return targets.
3. **Forward airbase chains** (the enjoyable scenario): W12 path-plan of Airbase
   extras through no-man's-land, escorted engineers (W28 stacking to finish fast,
   W30 tagged wants), stealth-deployed. Unlocks carpet bombing a formerly
   unreachable target.
4. **Air wings** with base+role+round-trip radius; strikes sync with W33 campaigns.
5. **Carriers** (addendum D): mobile naval airbase OR roving amphibious support;
   shared "base set" member so return-first recovers to the carrier's PROJECTED
   position. Needs W31 escort (helpless alone).
6. **Paradrop-seize** (addendum B): depends on a strike op clearing the tile THIS
   turn; PARADROP_CONQUER captures. Ideal target: a bomber-emptied city.
7. **Air defence**: CAP over high-value assets, air-defence buildings vs assessed
   enemy air, detect+raid enemy forward airbases (the counter to your own scenario).
8. **AWACS + specialists — DEFER until W04.** Vision is worthless while the AI is
   omniscient; AWACS (vision 26) only matters under honest information.

Order: 3.1 → A → C → 3.2 → D → B → 3.3 → 3.4.

## Acceptance
Airbase chain built toward an out-of-reach target; full escorted-chain-then-strike
sequence; bombers strike turn-before-assault; AI raids a sighted enemy airbase;
landlocked-peaceful builds ~no aircraft; **planes lost to fuel starvation per 100
turns drops sharply** (the "dead in the air" metric).

## Commits
`findings/air-doctrine.md` §7 + addendum sections A–D.
