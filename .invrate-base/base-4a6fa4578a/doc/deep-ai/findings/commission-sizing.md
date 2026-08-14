# W36 step 0: sizing the commission before building it

**Measured 2026-08-04. Two cells, 20 paired seeds each, one pinned
binary, `--arms deep`, 20/20 DONE both, 0 pairing violations.**
Instrumentation only: `benchmark/commission.py` over the new
`deep-hull-order` and `deep-wait` census lines.

The pass was mandated by W36's own step 0, which said it "may retire
half this file". It retires more than half.

## Behaviour-neutrality, for free

The wait census is bit-identical between the two binaries built during
this session (3291 wait lines, 1031 spells, every quantile equal), and
the second binary added the choice-site order log. The instrumentation
does not move the games it measures.

## Definitions used throughout

- A **hull order** is one consecutive run of turns in which a city's
  committed build choice is a ferry-role hull (`when=choice`). Runs are
  reconstructed offline, so no state is stored and nothing goes stale
  across a save/load.
- A **waiting spell** is one consecutive run of turns in which a unit
  wants a boat for a cross-continent target. It ends at enrolment as a
  lift's core, or when the unit stops wanting one.
- **ETA** is the crudest estimate available at order time: shield stock
  plus surplus, no growth, no waste, no buy. Deliberately what an award
  step would itself have.

## 1. The award: it buys banked shields, not a better shipyard

The first cut of this number was wrong and its own output said so. It
credited a rival city with its whole shield stock, so a city holding
200 shields toward a wonder looked like a 0-turn producer of a
20-shield hull. Rivals are now charged
`city_change_production_penalty()`, and a second figure --
`bestfresh`, cost over surplus with **no** stock -- is logged as the
floor: what an award buys while borrowing nothing from what the rival
was already building.

    rival ETA advantage at order time     isolated-150   lategame-80
    orders measured                          1563 (78/game) 1074 (54/game)
    after switch penalty: median                 2              3
      ... rival faster by >= 2 turns          56.0%          60.4%
      ... rival faster by >= 5 turns          30.4%          35.1%
    from a standing start: median               -1             -1
      ... rival faster by >= 2 turns          27.3%          31.1%
      ... rival faster by >= 5 turns          13.6%          17.9%
    ordering city best-or-tied on capability   65.4%          61.7%

**Read the two rows together.** Ranked on production capability alone,
the city that placed the order is already the best or equal-best
producer on its water about two-thirds of the time, and the median
rival is a turn *slower*. The apparent 2-3 turn win comes entirely
from the stock a rival has already banked -- which is to say the award
does not find a better shipyard, it spends another city's savings. That
is a real option, but it is an accounting transfer with a cost the ETA
does not show (the wonder or settler those shields were accumulating
for), and W36's design does not price it.

The gap that remains is genuinely small: on the fresh comparison a
rival is >= 5 turns faster on 13.6% / 17.9% of orders.

## 2. The fallback: the case it handles barely happens

    waiting unit-turns                     3291 (165/game)  313 (16/game)
    ... no loadable hull exists anywhere     4.6%           15.3%
    ... no producer CITY at all              0.4%            0.0%
    ... no producer on its own water         9.8%            3.5%
        (of those, with a producer elsewhere)  96%            100%

A commission with no candidate city to award to -- the case that
cannot fall through to a next-best producer -- is 12 unit-turns in 20
games on the island cell and zero in 20 games on lategame. **The
no-producer failure mode does not exist in either cell.**

Note this is NOT the same as the award being *refused* by the city it
lands on, which is what W36's cascade is actually for. That one cannot
be measured without building it; what can be said is that the
population it would serve is not the no-producer population, because
that population is empty.

`lift-noboat`, the demand with no vehicle, reads 7.30 spells/game on
the island cell and 2.10/game on lategame -- an order of magnitude
above the "7-10 per 20 games" the brief carried. But the finding is
what those spells DO:

    spells that never saw a loadable hull      146           42
    ... of which ever enrolled                   0            0
    ... median spell length                      0 turns      0 turns

**They end within a turn.** A passenger with no boat anywhere does not
stand and wait; it stops wanting a boat almost immediately (the settler
takes a land site, the unit is retasked). So the population that a
producer-side fallback would serve is not stuck -- it has already gone.

## 3. The wait: short in the middle, and all of it in the tail

Turns from first wanting a boat to enrolment as a lift's core. This is
the number section 1d asked for three times.

                                   isolated-150      lategame-80
    waiting spells                  1031 (52/game)   166 (8/game)
    enrolled as a lift core            42.6%            35.5%
    censored (never enrolled)          57.4%            64.5%
      ... still waiting at game end       36               19
    wait to enrolment: median            0                0
                       mean             2.28             0.98
                       p75 / p90       3 / 7            2 / 3
                       max               41               30

**Half of all passengers are enrolled the same turn they first want a
boat**, and the censored majority mostly stops wanting one rather than
waiting forever -- only 36 of 592 (island) and 19 of 107 (lategame)
were still waiting when the game ended.

But the pain is concentrated, and that is the part worth keeping:

    spells >= 5 turns:  19.8% of spells, 61.4% of waiting turns (island)
                         5.4% of spells, 29.4% of waiting turns (lategame)
    spells >= 10 turns:  6.3% of spells, 34.1% of waiting turns (island)

So hitching priced against an awarded ETA -- W36 step 5 -- has almost
no room in the median case and all of its room in a fifth of spells.
Any version of it that pays has to be built for the tail, and the tail
is where an ETA estimate is least reliable.

## 4. What this retires

- **The award step (W36 design 2) as specified.** Ranking candidates by
  ETA is not choosing a better producer; two-thirds of the time the
  ordering city already is one. What ranking finds is other cities'
  banked shields, at an unpriced cost.
- **The fall-through cascade (3) as motivated.** Its stated
  justification -- commissions that find no producer -- is a population
  of essentially zero in both cells.
- **Cancellation (4) as a large recovery.** An order lives a median of
  2 turns (island) and 1 turn (lategame), and 85% of production runs
  already end without producing a hull: build churn is de facto
  cancellation today. There is very little window in which a demand can
  evaporate between order and delivery.

## 5. What survives

- **The passenger's ETA (5), for the tail only.** A fifth of spells
  hold three-fifths of the waiting, and those are the ones where "wait
  for my own boat" is a real cost worth comparing a hitch against.
- **The order log itself.** `deep-hull-order`/`deep-wait` are cheap,
  behaviour-neutral, and answer questions the hull census cannot.

## 6. Caveats a reader should hold

- The rival ETA ignores the *value* of what the rival was building.
  That is exactly why the fresh comparison is reported beside it; the
  truth for any real award lies between the two rows, and closer to the
  fresh one the more the rival's own build mattered.
- Candidates are scored on the SAME hull type the ordering city chose.
  A candidate that can only build a lesser hull is not the same offer,
  and is excluded rather than credited.
- "Shares a water" is tile adjacency to a common ocean continent id,
  the same test the useful-water gate uses. It says both cities touch
  one body of water, not that a hull can sail between them; a strait a
  hull cannot enter would be counted as shared.
- Both cells run four deep seats; all figures are per game across those
  seats unless stated.
