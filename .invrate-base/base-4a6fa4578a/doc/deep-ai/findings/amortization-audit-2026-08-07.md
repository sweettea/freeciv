# The amortization audit — what still isn't priced over time, and the unevaluated corners

2026-08-07, orchestrator (Fable), at the human's direction. Question 1:
what else earns or costs over the next N turns but is priced as a point
value? Question 2: which ruleset corners have never even been evaluated?
Verified against the code where stated; everything else is a claim for
its session to check.

## 1. The amortization gaps, ranked by expected leverage

**1a. TECH WANTS ARE UNDISCOUNTED BY RESEARCH TIME — the largest gap.**
Verified: `daitech.c` has zero `amortize()` calls; a tech 5 turns away
and one 40 turns away compete at face value, and every credit deep
routes into `tech_want[]` (H9 capacity, tech-wall, horizon_tech_weight)
inherits the distortion at the choose-research seam. The fix has the
codebase's own shape: tech want × amortize(bulbs_remaining / rate) —
the same own-research ETA read CONSEQ's expiry item already computes.
Consequences of the gap: distant wonder-techs and era-gates outbid
near-term unlocks; the research queue optimizes the far field. One
mechanism, one session slice ("TECHTIME"), global effect.

**1b. UNIT LIFETIME UPKEEP IS UNPRICED AT THE BUILD SEAM.** A building's
gold upkeep is charged against its horizon credit over MORT
(`upkeep_gold_weight`); a UNIT's shield/gold/food upkeep over its
lifetime is charged nowhere at build time — only the fear terms carry a
partial discount (`upkeep_unit_weight`). A Warriors kept 200 turns
costs ~20× its build price; derby-3's birth guards demonstrate the
population. Fix: charge upkeep × min(E[lifetime], MORT) at the unit
want seam, symmetric with buildings. E[lifetime] can start as MORT
(honest upper charge) and refine to obsolescence ETA (CONSEQ's signal)
— no new machinery. Home: the deterrence-seat session (M4b), which is
already re-pricing the defender want.

**1c. THE VETERAN STREAM.** A veteran level is +50% power for the
unit's whole life — a stream, priced by stock as a point WAG at the
Barracks. The deterrence seat's Barracks item (field hospital) should
price BOTH streams: HP throughput and veteran-margin × expected
combats. Same substrate outputs.

**1d. GOVERNMENT CHANGES.** A revolution is an upfront cost (anarchy
turns × forgone surplus — the CM counterfactual machinery can now
price those turns exactly) against a per-turn stream (the new
government's deltas) amortized over the remaining game. The gov-value
session (ladder 6a) should be written in exactly this form; noted here
so it inherits the grammar rather than reinventing a WAG.

**1e. Pollution/warming** — a slow stream stock prices by flat
POLLUTION_WEIGHTING; real only on long venues. Post-v4, longhaul-300
evidence first. Parked with a name.

Covered already (for the record): buildings (MORT credits), caravans
(NPV), wonders (completion-truncated), unit obsolescence (CONSEQ),
site travel, endgame clamps, routes' truncation, culture accrual.

## 2. Ruleset corners never evaluated

**2a. Disband-placement arbitrage.** Disband Unit Recover returns 50%
of shields INTO THE CURRENT BUILD of the host city — so WHERE a
surplus unit disbands is worth up to half its cost, and stock's own
comment says it disbands "even if all shields will be wasted"
(`daicity.c:511`, verified). Deep's rotation melts inherit the seam.
Cheap mechanism: route disbands to the nearest city whose build the
shields help (wonder cities first — HEDGE's caravan logic already
thinks this way). Pairs with 2b.

**2b. The gold-upgrade action.** Stock never spends gold to upgrade a
unit (verified: only `is_on_unit_upgrade_path` obsolescence checks) and
neither does deep — rotation melts-and-rebuilds instead, paying full
shields for what the upgrade formula sells for gold. Upgrade-vs-rebuild
is a pure arbitrage decision on prices both sides of which already
exist (buy-cost curve, rotation's own power test). Together 2a+2b are a
coherent "unit economy" slice.

**2c. The Defend_Bonus sweep.** The deterrence seat prices walls; the
same seat should sweep ALL Defend_Bonus/class-gated defense carriers —
Coastal Defense (vs ships), SAM (vs air, era-gated), SDI, Great Wall
analogues — each priced by the substrate against the CLASS of threat
its class-gate answers (the floor-class facts already classify threats
by sea/air/land). One credit family, not four WAGs.

**2d. Incite-resistance carriers.** EFT_INCITE_COST_PCT (Courthouse
family) is WAG-priced (`daieffects.c:218`); its honest price is
P(hostile incite attempt) × city worth — small until spies exist, and
M4a's spy-currency slice is building the attacker side RIGHT NOW, so
the defender side should reuse its numbers. Statecraft/deterrence
boundary item.

**2e. Enter-Marketplace vs establish-route.** The caravan's lump-sum
option: R2 prices routes on merit, but the one-shot gold dump
alternative at the same arrival is (probably — session to verify) never
compared. Cheap addendum to the trade family: max(lump NPV, route NPV)
at the destination decision.

**2f. Barbarian-leader ransom.** Killing a barbarian leader pays
ransom gold. Micro, but reclamation ops (EXPANSION #39) walk right past
these; a line in the raid/reclamation pricing, not a session.

**2g. Era-gated set** — paradrop, airlift/Airports (EFT_AIRLIFT noted
braindead upstream), nukes, space race: all wait on M4a's
era-reachability table. If no cell reaches flight, they park with
evidence, per the check's design.

## 3. Queue placement (no new epochs)

- TECHTIME (1a): its own small slice — post-freeze candidate or first
  post-v4; global lever, one seam.
- 1b, 1c, 2c, 2d → the M4b deterrence-seat session (already queued).
- 2a+2b → "unit economy" slice inside M4's umbrella, after M4b.
- 1d → the gov-value session's stated form. 2e → trade family rider.
- 2f → a pricing line inside EXPANSION's reclamation. 1e, 2g → parked
  with named evidence gates.
