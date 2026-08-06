# TRADE: routes as a portfolio, and the annuity that was inverted

Session TRADE (`sessions/S-TRADE-routes.md`), 2026-08-06, branch
`epic/w03-horizon-content`. Pre-session tip `e18c3dd60d`.

The brief asked for a portfolio: slot opportunity cost priced
structurally, `city_trade_removable()` used for replacement, cease-fire
routes truncated rather than banned, transit risk discounted,
cross-water caravans behind a lift gate, and route value reaching
deep's own military and diplomatic decisions.

Two of its premises did not survive contact with the code, and one
defect underneath both of them turned out to be larger than anything
the brief named. Section 1 is that defect; sections 2-3 are the two
premises; the rest is what got built and what it measures.

---

## 1. `caravan.c`'s `annuity()` is inverted, and deep is the only caller that reaches it

    static double annuity(double payment, int term, double rate)
    {
      return perpetuity(payment, rate) * (1.0 - 1.0 / pow(rate, term));
    }

The closed form is `payment/(1-rate) * (1 - rate^term)`. The code writes
`1 - 1/rate^term`, which for `rate < 1` is **negative** and grows
without bound in `term`. At the values this project runs
(`rate = 0.95`, `horizon = MORT = 24`), an ongoing route worth one
trade per turn:

    arrival   annuity() as written   correct
        0            -48.496         +14.160
        4            -29.151         +10.450
        8            -16.878          +7.429
       12             -9.193          +4.967
       16             -4.466          +2.963

`perpetuity()` -- the infinite-horizon branch -- is correct at +20.000,
and **every in-tree caller uses `FC_INFINITY`**. That is why nothing
upstream has ever tripped over it. R2 set `horizon = MORT` for the
caravan build want, deliberately, to avoid perpetuity's 20x and to stop
the pathfinder walking the whole map, and thereby became the only
caller in the tree that reaches the defective branch.

The consequence is not a small mis-scaling. `get_discounted_reward()`
returns `trade + windfall`, so a destination where a route can
**actually be established** was priced tens of points BELOW one where
it cannot, and `caravan.c:569`'s `trade + windfall >= wonder` test then
discarded it. R2's price was systematically anti-correlated with the
thing it was pricing.

**Measured, not argued.** Deep now computes both numbers in the same
call -- its own NPV and what caravan.c's assembly says about the same
destination at the same arrival time (`npv=` and `cnpv=` on the
`deep-trade-merit` line). Two probe games of primary-120-classic at
`trade_merit_gain = 1.0`, 6 793 seam rows with a destination:

    quantity            p10      p50      p90
    deep's npv        32.10    52.60    72.10
    caravan.c cnpv   -13.60     6.20    29.60
    per-turn flow      2.00     2.00     3.00
    windfall          38.00    56.00    98.00

    an ongoing route is available on 94.8% of priced destinations
    caravan.c's NPV is NEGATIVE on 33.5% of them
    median ratio deep/caravan.c: 8.5x

**And the decomposition is proved rather than asserted:** on all 350
rows where the ongoing flow is zero -- the marketplace-only
destinations, where the annuity term vanishes -- deep's NPV equals
caravan.c's to the printed precision, 350 of 350. Deep's windfall
computation and discounting reproduce caravan.c exactly; the entire
difference is the annuity.

This also explains a number R2 could not decompose. R2 reported deep's
want at a median 0.114 of stock's bid and asked for a ~8x deflator.
With the annuity corrected the same ratio is **0.23** at the same gain
-- exactly double -- and deep's NPV median moves 27.9 -> 52.6. Half of
R2's "deflator" was this bug.

Not fixed in place: `ai/baseline` and `ai/default` both link
`common/aicore`, and both are measurement controls (CLAUDE.md rules 1
and 2). Filed as an upstream bug; deep assembles its own NPV from the
same primitives in `common/traderoutes.c`, which are correct and are
called rather than copied.

---

## 2. `city_trade_removable()` is not uncalled, and replacement is already priced

The brief's job list says `city_trade_removable()` is a function "no AI
has ever called". It is called, on every candidate destination, by
`caravan.c`'s own `one_city_trade_benefit()` (`caravan.c:294`), which
`trade_benefit()` runs for **both** endpoints; and
`account_for_broken_routes` defaults to TRUE, so the far side of a
broken domestic route is already counted. `can_establish_trade_route()`
calls it a second time as a legality test: a route that does not beat
the worst incumbent cannot be made at all.

So the "one-ledger rule" the brief asks for is the behaviour the game
already has, and deep inherited it. What deep was doing wrong was
throwing it away: R2 set `consider_trade = FALSE` once every free slot
was claimed by a caravan in the field, which on **classic** deletes the
candidate outright rather than repricing it -- because
`enter_marketplace_blocked_by = "Establish Trade Route"`
(`data/classic/actions.ruleset:39`) makes the windfall it expected to
fall back on illegal at exactly the destinations where a route is
legal.

The structural replacement is section 5.

---

## 3. Trade caravans are already lift-eligible; the block is the destination search

Design call 3 asked for trade caravans to become lift-eligible behind
`lift_carry_caravans`. They already are.
`deep_lift_wants_boat()` (`deepops.c:1405`) tests three things -- not
transported, no ferry assigned, has a goto tile -- and has **no
unit-class filter of any kind**. `deep_op_form_lift()` adds only "the
goto is on another continent" and `could_unit_load()`. A caravan
holding a cross-continent goto would be picked up by the next lift
formation exactly like a settler, today, with no gate to open.

The binding constraint is one step upstream: **the destination search
is a land pathfinder**, in deep's enumeration and in `caravan.c`'s
`caravan_search_from()` alike, so an overseas city is never a candidate
and the goto that would make the caravan lift-eligible is never set.
Stock reaches those cities only by setting `ignore_transit_time = TRUE`
(`daiunit.c:2466`) -- i.e. by pricing the crossing as free, which is
the exact defect R2 removed and must not be reintroduced.

So the gate the brief names is not where the work is. Building
cross-water trade means giving deep an **overseas candidate
enumeration with a modelled ETA** (walk to a port + expected lift wait
+ crossing + walk from the beachhead), and that is a real piece of work
with an unmeasured population. Per this project's own method note --
size the machinery against the population it would act on -- the
population is now counted for free: `offshore=` on the
`deep-trade-merit` line reports the legal trade partners on another
continent that a caravan built here cannot walk to, and `offpct=` what
the ruleset would pay for them.

    cell                     offshore partners per seam line (p10/p50/p90)
    primary-120-classic                 0 / 0 / 0

The classic tuning cell has **no overseas legal partners at all**, so
nothing built for design call 3 could have been measured there. The
island cell's number is in section 8.

The drop-pick watch item is therefore moot for this session: no new
passenger class reached the lift protocol, because none could.

---

## 4. The switch tree, and each level proved

    trade_merit = 0            R2's master switch, the compiled default.
      (COMPILED DEFAULT)       Skips the probe, the enumeration, the
                               pricing and the census entirely.
      |
      +-- trade_merit = 1      deep's own NPV assembly replaces
                               caravan.c's, with:
          |
          +-- the corrected annuity          always on inside (a bug
          |                                  fix, not a knob)
          +-- rank-priced slots              always on inside (a
          |                                  comparison, not a knob)
          +-- trade_ceasefire_peace_odds     0.0 = pure truncation
          +-- trade_route_risk_gain          0.0 = survival factor 1.0

    trade_partner_target_discount = 0.0      own off-switch, outside
                                             trade_merit
    trade_route_love = 0.0                   own off-switch, outside
                                             trade_merit

Everything with its own knob is skipped rather than multiplied by an
identity, so the off arm keeps the previous ordering byte-identically.
The two things without their own knob sit under `trade_merit`, which is
what the acceptance criterion permits.

**Level 1 (the whole session off).** Smoke cell, 10 paired seeds, both
arms loading `benchmark/tuning/trade-off.ini`; the only difference is
the binary. Pre-session `e18c3dd60d` (pinned `923fc3512696`) against
the session binary:

    seed  1 59aeddeb566f   seed  6 5e4c2bbd2be9
    seed  2 82fb0c66c073   seed  7 f954506e5f87
    seed  3 e363f4dedec9   seed  8 a8b2d7a81a5a
    seed  4 327b1c3a007d   seed  9 e7daec9dddb7
    seed  5 83e497e71bb7   seed 10 6e372be6425c
    => 10/10 byte-identical

Those are the same ten hashes M1 published on 2026-08-06 (§5b), which
makes this the third independent check that the no-op path has not
moved. Execution counters on the same ten seeds: **0
`deep-trade-merit` lines, 0 `deep-trade-love` lines, and
`spared=0 loved=0` on every `deep-leaders` line.** Non-execution and
byte-identity are different claims and both are made.

---

## 5. What the price actually does now

`deep_trade_merit_want()` enumerates every city a caravan of this type
standing in this city could reach inside MORT turns, prices each one
itself, sorts them, and sells the marginal caravan the **rank-th
best** destination, where rank is the number of caravans this city
already has committed to the trade errand.

That is the slot opportunity cost expressed as a comparison rather than
a coefficient: the second caravan is worth the second-best partner, not
the best one twice. It replaces a boolean that fell off a cliff, and it
degrades to exactly the boolean's answer -- zero -- at the point where
the boolean was right, namely when every remaining partner is spoken
for.

Replacement pricing needs no separate mechanism. When a city's slots
are full, `deep_route_endpoint_gain()` asks the game's own
`city_trade_removable()` what the incumbent is worth and subtracts it,
on both endpoints, with the far side of a broken domestic route counted
once. A new partner is worth building for exactly when it beats the
worst incumbent by more than the caravan costs -- and the caravan's
cost is charged by `amortize()` over the turns to build it.

Cease-fire truncation reads `player_diplstate_get()->turns_left`, the
same counter PEACE's pre-pass watches. Both rulesets in use set the
`Enemy` trade pct to 0 with cancelling `Cancel`, and
`cities_trade_route_type()` maps `DS_CEASEFIRE` to `TRT_IN` (100% on
civ2civ3) and `DS_WAR` to `TRT_ENEMY` (0%), so a cease-fire route is
legal now and dies on a date we can read. It is priced as the annuity
up to that date and nothing after it. `trade_ceasefire_peace_odds` buys
the tail back at whatever probability the tuner thinks peace follows;
at its default 0.0 no optimism is expressed.

---

## 6. A/B: primary-120-classic, 20 paired seeds

(TABLES)

---

## 7. A/B: lategame-80 (civ2civ3, peace pinned), 20 paired seeds

(TABLES)

---

## 8. The island cell, and what cross-water would be worth

(TABLES)

---

## 9. Owed to the tuning pile

    trade_ceasefire_peace_odds  0.0  PLACEHOLDER. 0 = pure
                                     known-mechanics truncation. The
                                     tail it buys is real income if
                                     peace follows a cease-fire; PEACE
                                     measured how often that happens
                                     under its own pre-pass and the
                                     number belongs in the same pass.
    trade_route_risk_gain       0.0  PLACEHOLDER. 0 = no discount. The
                                     shape is fixed (survival =
                                     1 - gain * dangerous fraction);
                                     the gain is how much of it to
                                     charge.
    trade_route_love            0.0  PLACEHOLDER, love per active
                                     foreign route per turn. The scale
                                     it must be read against is the
                                     -50/turn best_target bleed and the
                                     125 peace bar. Clamped
                                     structurally at
                                     req_love_for_alliance.
    trade_partner_target_discount
                                0.0  PLACEHOLDER, and the STANCE knob:
                                     0 = pure opportunist, 1 = refuse a
                                     partner outright.
    trade_merit                 0    unchanged from R2, and the master
                                     switch for everything above.
    trade_merit_gain            1.0  unchanged. R2's measured useful
                                     range (5-10) was measured on the
                                     INVERTED price and does not carry
                                     over; section 6 has the new
                                     calibration.
