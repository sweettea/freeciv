# Plans commission, cities arbitrate

**Human sketch, 2026-08-04.** A frame, not a workstream: it says what
shape the player turn should have, and which existing workstreams are
already pieces of it. Written down because the sketch is worth more
than the session that produced it.

## The shape

A player turn is a sequence of PLANS that price themselves, commission
builds at chosen cities, and are re-evaluated or abandoned as the
world moves:

    Assault plan (maybe)
      pick a rendezvous
      decide what navy / air / land force it needs
      evaluate its worth
      move land units to the rendezvous
      load, travel together
      beachhead if applicable
      attack
      pivot or give up

    Rates
      science / tax / luxury from the amortized value of techs

    Autoworkers

    City adjustments
      enemy nearby        -> consider killing it, or adjusting defence
      unprotected + enemy -> produce defence, buy it, or sell and buy
      site worth taking   -> commission a settler at a nearby city
                             and a hull for the crossing, if needed

The one rule underneath all of it: **plans commission, cities
arbitrate, everything amortized.** A plan decides what it needs and
what that is worth; it names a city; the city weighs that commission
against its own wants on one discounted scale and may refuse, in which
case the commission falls through to the next-best city.

Today the code is the exact inverse. Cities decide independently and
plans beg: `colony_want` is a per-city opinion, the berth-shortage
test that should carry lift demand to a shipyard counts only ops with
a real `target_city` while a lift sets `target_city = 0`
(`deepops.c:763`), and so **every lift contributes zero to measured
hull demand**. That is RC2 in miniature -- the same empire is
over-supplied with hulls in aggregate and short of one at the exact
place a settler is standing.

## What already exists

| Step | State |
|---|---|
| Assault plan spine | **BUILT** -- W33 operations layer: persistent phased ops that own their units, with commitment, phase sequencing, expected value and deliberate abort ("pivot or give up"). Four phases landed. |
| Naval / air force sizing | **SPECCED** -- W31, W32. "Decide what navy we need for it" is exactly their content. |
| Multi-op campaigns | **SPECCED** -- W13, gated on W10/W11/W30. |
| City emergency ordering | **BUILT** -- deep collects city emergencies first and resolves them by severity rather than city-list order. |
| Settler commissioning | **PARTIAL** -- deep chooses sites and wants founders, but the want is a city's own, not a commission from a plan. |
| Hull commissioning | **THE GAP** -- see the assignment design below. |
| One amortized scale | **HALF** -- W01 landed `want_t` + tiers at byte-identity; the conversions are still identity because founder deflation is gated on W03. |
| Rates (sci/tax/lux) | **MISSING, no workstream.** deep READS the tax rates when valuing tiles (`deephorizon.c:439`) and never sets them. |
| Trade routes on merit | **MISSING.** Stock prices a caravan on *initial* income, undiscounted -- there is no `amortize()` anywhere in `daidomestic.c` -- then clamps it to 0.1 when caravans pile up, and the fallback ladder builds one at want 1 precisely when the city wants nothing. |

The connective tissue already has a name and an ordering: **W30**, the
advisors extension point that lets a plan tag a want, which the PRD's
hard orderings put before plan-driven wants in W12, W13 and W28.

## Why this frame came up

The useful-water gate (2026-08-04) refused a ferry after it had won a
build slot, which meant the only route back to a sensible build was
reconstructing a runner-up the advisor had already freed. Filtering at
bid formation fixed that case. But the general question the fix could
not answer is *which city should build the hull at all* -- and that
question has no owner in the current architecture, because no object
represents the demand.

The generalisation the human drew: a hull order is not a city's
opinion, it is a demand with a water body, a deadline and a value,
shopping for a producer. And the same is true of a settler, a
defender, an escort. The ferry is just where it became visible.
