# W36 — Build commissioning (demand-side orders)

**New workstream, drafted 2026-08-04 from a human design call. Frame:
`findings/plans-commission.md`. Root cause: RC2 (no shared plan),
with RC4 in the arbitration.**

**SIZED 2026-08-04, AND MOSTLY RETIRED.** Step 0 ran on two cells, 20
seeds each; the numbers are in `findings/commission-sizing.md` and the
verdict is in "What the sizing decided" immediately below. **Read that
before the Design section, which is now a record of what was proposed
rather than a plan to execute.**

## What the sizing decided (2026-08-04)

Full numbers and caveats: `findings/commission-sizing.md`. Two cells,
20 paired seeds each, one pinned binary, 20/20 DONE, instrumentation
proven behaviour-neutral (the wait census is bit-identical across both
binaries built that session).

**RETIRED — do not build:**

- **The award by ETA (Design 2).** On production capability alone the
  ordering city is already the best or equal-best producer on its water
  65.4% (isolated-150) / 61.7% (lategame-80) of the time, and the
  median rival is a turn SLOWER. The 2-3 turn median advantage that
  appears once a rival is credited with its shield stock is that
  rival's SAVINGS, not a better shipyard — an accounting transfer whose
  cost (the wonder or settler those shields were for) the ETA does not
  show and the design does not price.
- **The fall-through cascade (Design 3), as motivated.** The failure it
  was justified by — a commission with no producer — is 12 waiting
  unit-turns in 20 games on the island cell and zero on lategame. The
  population is empty. (A commission REFUSED by the city it lands on is
  a different case and remains unmeasurable without building it.)
- **Cancellation (Design 4) as a large recovery.** An order lives a
  median of 2 turns (island) / 1 turn (lategame), and 85% of production
  runs already end without producing a hull. Build churn is de facto
  cancellation; there is almost no window for a demand to evaporate
  between order and delivery.

**SURVIVES:**

- **The passenger's ETA (Design 5), for the tail only.** The wait is 0
  at the median in both cells (mean 2.28 / 0.98, p90 7 / 3), so the
  hitch-versus-wait comparison does not bind in the median case at all.
  It binds in a fifth of spells, which hold three-fifths of all waiting
  turns. Anything built here is built for that tail — and the tail is
  where an ETA estimate is least reliable, so it must be measured, not
  assumed.
- **The two censuses.** `deep-hull-order` and `deep-wait` are cheap and
  behaviour-neutral, and they answer what the hull census cannot.

**The `lift-noboat` correction.** The population is an order of
magnitude larger than the brief carried (7.30 spells/game on the island
cell, 2.10 on lategame, against "7-10 per 20 games") and it matters
less, not more: those spells END WITHIN A TURN and never enrol. A
passenger with no boat anywhere stops wanting one rather than waiting
for a producer to appear.

## Problem

Production of anything a PLAN needs is decided by cities voting
independently. For hulls this is measurable and wrong in three ways at
once:

- **The demand signal does not arrive.** The berth-shortage test next
  to `colony_want` counts only MUSTER ops with a real `target_city`,
  and a lift sets `target_city = 0` (`deepops.c:763`), so
  `game_city_by_number(0)` is NULL and every lift contributes zero to
  measured hull demand. `colony_lift_weight` is fed by amphibious
  assaults alone.
- **The producer is not chosen.** Any coastal city may bid. Until
  `a81d509b4a` the only test was `is_terrain_class_near_tile(TC_OCEAN)`
  — "am I coastal" — and even now the test is a veto on useless water,
  not a choice among useful ones. Nothing asks which city on the right
  water finishes the hull SOONEST.
- **The order outlives its reason.** 29.2% of idle spells never end
  and 8.8% of hulls die having never carried anything. Whether the
  demand evaporated after the order was placed is untestable today,
  because nothing logs the turn production started.

Meanwhile the passenger waits without knowing how long, so
`deep_lift_enroll()` decides hitching on FEASIBILITY ("can I reach the
rendezvous") and never on COMPARISON ("what would I get by waiting for
my own boat"), which is the open call in NEXT-SESSION §1d.

## Design

**A demand object owns the order.**

```c
struct deep_commission {
  Continent_id water;      /* the body that must touch BOTH ends */
  struct tile *rendezvous, *destination;
  int needed_by_turn;
  want_t value;            /* the delivery's worth, one scale */
  int owner_op;            /* the op that raised it */
  int awarded_city, eta;   /* filled by the award step */
};
```

1. **Raise**: a lift op (or a settler wanting a crossing) raises a
   commission instead of hoping a coastal city feels like it.
2. **Award**: candidate cities are those on `water` that can build a
   ferry, ranked by completion ETA (shield stock + surplus), not by
   coastal-ness. Best ETA wins the award.
3. **Arbitrate**: the awarded city's ferry bid enters its own choice
   at `amortize(value, eta)` — one scale, same as every other bid. If
   a better local build wins, the commission **falls through to the
   next producer on `water`**. That cascade is the award loop, not a
   special case.
4. **Cancel**: the commission owns the order. Settler founds
   elsewhere, is re-tasked, or dies -> the order is cancelled. This
   makes "the order outlived its reason" structurally impossible
   rather than merely measurable.
5. **Meanwhile**: the award gives the passenger an ETA, so hitching
   becomes `hitch_cost < remaining_wait`, and a passenger with a long
   wait keeps working instead of being frozen by the op hold (the
   defect that cost ~570 held-idle passenger-turns per 20 games before
   `b699ddefa1`).

The useful-water gate retires into step 2: a city on a closed sea is
never a candidate producer, so there is nothing to veto.

## Step 0 — the sizing pass, DONE 2026-08-04

**Answered. See "What the sizing decided" above and
`findings/commission-sizing.md`.** Kept below as the question that was
asked, because the third bullet's phrasing is what the instrumentation
was built to satisfy.

The census already records the water each hull touches and the demand
on it. Add the order-time shield stock and answer:

- when a hull was ordered, was there a city on the SAME water with a
  materially shorter ETA? If most orders would land at the same city
  anyway, the award machinery is ceremony and only cancellation (4)
  and the ETA (5) are worth building.
- how often does a raised commission find no producer at all? That is
  the `lift-noboat` population, 7–10 per 20 games on lategame-80 —
  small, and the island cell is where to measure it.
- the wait distribution: turns from "first wanted a boat"
  (`deep_lift_wants_boat` true) to enrolled as a lift's core. §1d has
  asked for this number twice; step 5 cannot be priced without it.

## The capability the gate forecloses (human, 2026-08-04)

**Transports beat walking, and not only between continents.** The
useful-water gate refuses same-continent coastal hops along with the
dead lakes, and that cost is currently zero *by construction*: deep's
lift model is a continent inequality --
`tile_continent(unit) != tile_continent(goto_tile)` at
`deepops.c:478`, `:770` and `:909` -- so a same-continent passenger is
rejected at enrolment, at candidate selection and at drop routing. The
census agrees it is not costing anything today: of the closed-water
hulls the gate kills, 13 of 15 never carried a passenger, and the ~2
that did were stock's emergent pickups, about 1% of hulls.

It is free because nothing tries. If intra-continent lifts are ever
wanted, three things change together:

1. **The gate generalises without becoming demand-driven.** "Does this
   water reach another continent" becomes "does this water SHORTCUT
   anything" -- is there a pair of points whose sea path plus
   embark/disembark beats the land path. Still pure geometry, still
   computable per water body from the map, so it keeps the property
   that made the gate work: it does not move with demand, and cannot
   suppress the hull that serves the next burst. A lake that shortcuts
   nothing stays refused at any demand.
2. **The lift's continent test becomes a route comparison** at those
   three sites: is the sea route materially better than walking.
3. **The premise is terrain-dependent and must be measured, not
   assumed.** A boat outruns a settler on unimproved terrain; roads
   give 3 tiles/turn and rails are effectively free, and a hop pays
   rendezvous plus load/unload on top. Early game favours the boat, a
   developed continent favours walking -- exactly the shape that flips
   between cells, so measure it in both.

## What this is NOT

**Not a reason to build more transport.** Section 2's verdict stands:
five delivery mechanisms have worked and the aggregate never followed;
`lift_op_slots` bought +26% formations and −13.50 score. The channel
that did move on the water gate is shields refused at the wrong city
buying other things (unitsbuilt +5.82 t=2.91, cities +0.76 t=2.26 over
40 seeds). Fewer, better-placed, cancellable hulls — not more.

**Not a ranking framework.** The decision stays a single max over
bids. What changes is that filters and awards happen BEFORE the
comparison, so no runner-up ever needs reconstructing.

## Acceptance

- Every ferry order traceable to a commission: op id, water, ETA,
  award city, and either a delivery or a cancellation. Grep-able.
- Backstop counter (`deep-watergate`) stays at zero — a nonzero
  reading means a producer bids outside the commission path.
- Hulls that die having never carried anything fall from 8.8%; hulls
  ordered for a demand that no longer exists reach zero by
  construction.
- Score: report it, do not promise it. The honest prior from five
  predecessors is flat.

## Commits

Landed 2026-08-04 (step 0):

```
ai/deep: log the turn a hull order is placed and its shield stock
ai/deep: log each turn a passenger waits for a lift
benchmark: size W36 step 0 from the order and wait censuses
ai/deep: measure the hull order where its build choice commits
doc: size the commissioning gap from the order and wait censuses
```

Sketched and now RETIRED by that sizing — raise, award, fall-through
and cancel all target populations the measurement found empty or
already handled:

```
ai/deep: raise a commission when a lift needs a hull
ai/deep: award a commission to the best-ETA city on its water
ai/deep: let a refused commission fall through to the next producer
ai/deep: cancel a hull order when its demand is gone
```

The one survivor, and only if someone takes the tail seriously:

```
ai/deep: price hitching against the waiting passenger's own ETA
```
