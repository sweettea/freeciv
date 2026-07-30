# W32 addendum — return-first planning, paradrop exploitation, missiles, carriers

Four refinements from playtesting intuition. The first is structural — it inverts
the air control flow — so it leads.

Baseline: `freeciv/freeciv` @ `0896b34`. Stats from civ2civ3.

---

## A. Return-first: plan the recovery, *then* choose the target

**This is the correct fix for "planes die in the open," and it reverses the
current order of operations.**

Today, `dai_manage_airunit` (`daiair.c:492`) leaves a refuel point at full fuel
and calls `find_something_to_bomb()` — target first. Only *after* committing to
the target does anything look for a base (`daiair.c:528`), and by then the fuel
may be spent. The unit's own header admits the weakness:
`TODO: support for fuel > 2` — i.e. the return logic is least reliable for exactly
the long-range units (bombers, stealth bombers, AWACS at fuel 3) that stray
furthest.

**Invert it.** A mission is only valid if the *round trip* closes:

```
for each candidate target:
    strike_tile   = best tile to attack the target from
    recovery_tile = nearest refuel point reachable FROM strike_tile
                    with the fuel remaining AFTER the strike
                    (include allied cities/bases — is_refuel_tile already allows them)
    if recovery_tile exists within fuel:
        mission is viable; score it
    else:
        mission is a SUICIDE mission; only allow if explicitly sanctioned
```

So the sequence becomes **base → reachable target with a guaranteed recovery →
strike → recover**, not "fly at a target and hope." This mirrors W31's rule that a
transport doesn't launch without a guaranteed escort, and W32 §3.2's wing
discipline: *commit only if the return leg is already solved.*

**Suicide missions are a valid but explicit choice.** Some strikes are worth a
one-way trip — a cruise missile (single-use anyway), a bomber trading itself to
break a critical stack, a kamikaze against a game-deciding target. The point is
that the AI should **decide** this, tagging the mission `AIUNIT_SUICIDE`, not back
into it by fuel-mismanagement. A sanctioned suicide strike and an accidental
stranding look identical in the current code; they must not.

This changes W32 §3.2's wing model: a wing's mission radius is defined by
**round-trip** fuel from its base set (own + allied + carrier + forward airbases),
not one-way reach. Halve the naive radius for round trips, unless a forward base
or carrier extends the far end.

Acceptance: planes-lost-to-fuel-starvation drops to ≈0 *except* units explicitly
tagged suicide; and a corpus position with a juicy out-of-return-range target
shows the AI declining it (or committing a missile/expendable instead of a
bomber).

---

## B. Paradrop into a bomber-emptied city — the combined-arms payoff

You identified the ideal use, and it's the air analogue of W31's
bombard-then-land: **soften, then seize.**

Sequence, as one campaign (W13):

1. **Recon** confirms the target city is capturable — weakly enough held that
   bombing can clear the defenders (W04; honest information required so this isn't
   a cheat).
2. **Strike wing** bombs the garrison down. Bombers have `CityBuster`-adjacent
   behaviour and the shore/city firepower rules from W31 §2 apply to air too — a
   city tile is a soft target once its stack thins.
3. **Paradrop** onto the emptied city the *same* turn the last defender falls —
   paratroopers are move-1 Land units that seize the tile, and taking an empty
   enemy city captures it.
4. **Hold** — the paras are A6/D4/HP20 and now exposed; a follow-up (heli-borne or
   the next wave) reinforces or the operation accepts the trade.

The precondition chain matters: paradrop range is short, so this only works once
§3.1's **forward airbases** (or an allied city, per point D) put the paratroopers'
transport/staging within drop range. That's why forward basing gates paradrop in
the dependency list.

The gate is strict, because a paradrop onto a *still-defended* tile is dead
paratroopers: require the strike wing to have **cleared the tile this turn**, or
W04 to assess the garrison at zero, before the drop commits. Bomb-then-drop is one
atomic campaign step, not two hopeful ones.

Acceptance: corpus position — defended-but-weak enemy city; assert the AI bombs it
to empty and paradrops the same turn; assert it does *not* drop when the garrison
survives the bombing.

---

## C. Cruise missiles — the expendable precision strike

Cruise missile (civ2civ3): **A18**, move 16, fuel 1, HP10, `Missile` class,
`OneAttack`, `CityBuster`, `AirAttacker`. The `Missile` unit class carries the
`Missile` flag — it is **consumed on attack**. That single fact defines its
doctrine and resolves your "idk how to use them well":

**A cruise missile is one guaranteed high-firepower hit that you never have to
bring home.** Everything awkward about aircraft — the return leg (A), fuel
management, survival — doesn't apply, because it dies on use by design. So it's the
*correct* tool for exactly the missions return-first (A) rejects:

| Use it for | Because |
|---|---|
| **The out-of-return-range target** | No recovery needed — A's suicide case, but *designed* for it, so no unit is wasted |
| **Softening a hard stack before assault** | A18 × CityBuster guarantees damage; pairs with W31 bombardment / W13 assault prep |
| **Picking off a high-value exposed unit** | AWACS, a lone battleship, a stack leader — one-shot removal |
| **First strike through air defence** | Cheaper to trade a missile to the SAM than a stealth bomber |

**Doctrine:** treat cruise missiles as *ammunition*, not aircraft — build them for
a specific queued strike, launch within a turn or two, don't stockpile idle.
Value = expected damage on the designated target minus build cost, with **no
survival term** (there's no survival). This makes them cleanly separable from the
wing model: a missile is a scored *shot*, not a unit with a life to manage.

They can also be **carried** (subs/ships with missile-carrier capability in some
rulesets) for forward launch — fold into W31 if the probe finds a missile carrier;
otherwise land/air-launched only.

Acceptance: the AI builds cruise missiles against a specific target and expends
them within N turns; it does *not* accumulate idle missiles; it prefers a missile
over a bomber for a no-return-leg strike.

---

## D. Carriers — naval airbase *or* roving assault support, by doctrine

A carrier is the mobile refuel/staging point that unifies W31 and W32. Two modes,
selected by the doctrine axis:

**Mode 1 — mobile naval airbase (sea-control / strike doctrine).** The carrier
*is* a forward airbase that floats (`is_refuel_tile` treats it as refuel via
`unit_could_load_at`). It extends the round-trip radius (A) into blue water: a
strike wing bases on the carrier, the carrier sits in a controlled sea lane (W31
§3.2), and the wing strikes coastal targets no land base could reach. This is
§3.1's airbase-chain idea, made mobile — and it's why carrier and forward-airbase
logic should share the "base set" abstraction from A.

**Mode 2 — roving amphibious support (invasion doctrine).** The carrier moves
*with* the W31 amphibious task force. Its air wing provides the bombardment phase
(W31 §3.4 / W32 §B) from offshore: soften the beachhead and the target city, then
the transports land and — ideal case — paratroopers from the carrier-staged wing
seize the bombed-out city (B + D together). The carrier is the air component of a
combined naval-air-land operation.

Both modes make the carrier a **shared base-set member**, so A's round-trip
planner automatically uses it: a plane on a carrier computes recovery to the
carrier, and if the carrier will move, to its *projected* position. (That
projection is the one genuinely new bit — a carrier is a refuel point with a
velocity, so the recovery check must use where it *will* be, not where it is.)

Carrier is helpless alone (A0/D9, `BadCityDefender`) — so per W31 §3.1 it never
sails without an escort screen. Carrier doctrine therefore *requires* the naval
escort layer, which is why W31 precedes W32's carrier use.

Acceptance: on a sea map, the AI bases air on a carrier and strikes targets
unreachable from land; in an amphibious campaign, the carrier's wing runs the
bombardment phase; the round-trip planner correctly recovers aircraft to the
carrier's projected position after it moves.

---

## Dependency note

These sharpen the W32 orderings:

- **A (return-first) is foundational** — implement it before B, C's bomber
  comparison, or D, because all of them reason about recovery.
- **B (paradrop) needs §3.1 forward basing or D (carrier) or point-D allied
  cities** to reach drop range, plus W04 for the garrison assessment.
- **C (missiles) is nearly standalone** — it deliberately sidesteps A's recovery
  logic, so it can land early as the "expendable strike" primitive that A's
  suicide branch points at.
- **D (carriers) needs W31's escort layer and the shared base-set abstraction
  from A.**

Suggested insertion into W32 build order:
**3.1 → A → C → 3.2 → D → B → 3.3 → 3.4**, with AWACS (§3.4) explicitly deferred
until after W04 flips honest information — vision is worthless while the AI is
omniscient.
