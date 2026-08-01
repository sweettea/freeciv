# W31 — Naval doctrine

**Phase 3, ai/deep. Phase lists over W33.** Full design: `findings/naval-doctrine.md`.
Root causes: RC2, RC3-at-sea.

**Landed early (2026-07-31, opportunistic slices, see STATUS "operations
evening"):** convoy hold + escort recruit (slices 1+2); defence-at-arrival
go/no-go on loaded convoys (`deep_take_odds_at_arrival`, launch bar
`op_take_target`); overseas targets in assault wants and operations with
staging at own ports; lift wants from muster berth shortfalls and colony
demand. Still open here: sea-lane graph (§2 of the findings design), fleet
role templates, patrol/blockade wants, bombardment phases, coastal
defence-in-depth, and the W08 naval doctrine axis.

## The problem
`dai_choose_attacker(TC_OCEAN)` (`daimilitary.c:143/2033`) picks one best sea unit
per city by generic desirability. No fleet, no sea control (grep blockade/patrol/
escort = nothing), no combined arms. Anti-bombard defence is a self-described
"dummy support" (`daimilitary.c:606`, OSDN#41778). Transports cross alone.

## Unit roles (civ2civ3) — the synchrony comes from these differing sharply
- Destroyer a4/d4/mv6 — anti-sub screen (2× defence vs subs). Fast.
- Cruiser a6/d6, Aegis a8/d8 — escort; aegis adds air/missile umbrella.
- Battleship a12/d12 — capital ship; shells coasts (low_firepower_pearl_harbor
  doubles attacker fp, drops city defender to 1). Slow.
- Submarine a12/d5, Subsurface (stealth), Only_Native_Attack (ships only). Glass cannon.
- Carrier d9/cap8 — mobile air base; helpless alone. Transport a0/d3 — cargo.
Sub warfare is rock-paper-scissors: subs beat capital ships, destroyers beat subs,
battleships beat destroyers.

## Design (build order within W31)
1. **Escort** (do first — cheapest, highest payoff): transport/carrier never
   crosses uncontrolled water without a sized warship screen. A precondition on
   W18 ferry routes and W33 amphibious ops, sized from W04-assessed enemy naval.
2. **Sea-lane graph** (W12's water analogue): value lane control by dependent
   traffic → patrol (hold friendly lanes) + blockade (deny enemy lanes) wants.
3. **Fleet role templates** sized from assessed enemy composition — destroyer want
   spikes on sub sighting, near-zero on empty sea.
4. **Amphibious phase list** (over W33): muster → cross escorted lane → bombard
   beachhead → land → hold.
5. **Coastal defence in depth** — contest the sea approach, not the beach.
6. **Naval axis into doctrine** (W08): island/across-water start shifts toward
   harbours/offshore-platforms/fleet; landlocked declines a battle fleet.

## Acceptance
Unescorted crossings → ≈0; holds a lane between continents; destroyer want tracks
sub sightings; bombard precedes disembark; island vs landlocked → opposite naval
builds; **naval score gain on archipelago exceeds gain on Pangaea** (headline).

## Commits
`findings/naval-doctrine.md` §7.
