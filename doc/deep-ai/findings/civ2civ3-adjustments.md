# civ2civ3 adjustments: remaining queue (2026-07-28)

Follow-up to the head-to-head grid and the two landed defect fixes
(naval blindness, upkeep blindness — see STATUS.md 2026-07-28
section). Source evidence: the mixed-grid seat-paired table
(benchmark/results/mixed-grid-2026-07-28.tgz), the ruleset deviation
survey of data/civ2civ3/README.civ2civ3 done that session, and the
per-dimension collapse analysis (gold ratio 0.13-0.55 head-to-head vs
1.5 same-world).

Items are split by whether they close the gap TO stock (asymmetric:
deep's own machinery misprices the ruleset) or build an edge OVER
stock (symmetric weaknesses both AIs share).

## Gap-closers

1. **Government-aware upkeep** (the recurring theme). Gold upkeep is
   1x/2x by government, free units per city are 1-3 by government,
   empire-size unhappiness divisor is per government: government
   choice, army size and city count are one coupled decision in
   civ2civ3. Cheap first step: deep_defender_upkeep_discount()
   should use government-adjusted utype_upkeep_cost() instead of raw
   ruleset tables. Full government-coupled planning is its own
   workstream, later.
2. **Carrying-cost-aware site evaluation** (extends the queued
   tech-upkeep item). civ2civ3 doubles distance waste until
   Trade/Corporation; Courthouse/Granary halve it; tech upkeep is
   charged per city (3 free). W02 prices a site's yields but none of
   its carrying costs, so remote sites are overvalued exactly in the
   era deep founds them. One merged change: site value charges
   expected waste-at-distance + marginal tech upkeep, ruleset-read.
3. **Health/plague credits in the horizon (W03)**. Illness starts
   above size 4; Aqueduct/Sewer cut it 30% each via Health_Pct,
   which no horizon credit sees — deep builds Aqueduct at the size
   gate, later than optimal. Small bundle-credit extension when
   illness is enabled.
4. **Size-aware garrison floor?** Food upkeep + pop-capped free
   support means a floor defender in a size-2 city eats a real share
   of its food. HOLD until defence-D reports: if the tuner crushes
   garrison_floor_danger, that is the data saying the flat floor
   overcharges small cities, and city size is the missing covariate
   (per the rule: a knob that wants regime-dependent values is a
   formula missing a term).

## Edge-builders (symmetric; do after the gap closes)

5. **Tired-attack discipline**. civ2civ3 scales attack by remaining
   MP; stock routinely attacks at 2/3 strength after moving. Rule:
   never initiate below a tunable MP fraction unless target flees or
   urgency demands. First unit-micro item on the board; measure via
   loss ratios in contested/cramped head-to-head.
6. **Embassy science (W06 extension)**. Tech leakage via embassies;
   Diplomats/Explorers can establish from boats. Add a tunable
   embassy term to explorer/diplomat wants.
7. **Caravan policy**. National trade routes are worth ZERO in
   civ2civ3; caravans are wonder-fuel (allies' wonders too) and
   capturable loot. Verify stock wastes caravans domestically there;
   if yes, suppress in deep.
8. **Cheap-Walls valuation**. Walls cost 30 for +100%, cities have
   inherent bonuses. Stock's wallvalue is effect-driven and may
   already track it — verify before touching.

Checked, no action needed: terrain yields/specialists (CM and site
eval read real outputs), citymindist/mountain bans (engine),
barbarian onset (gate reads the server variable), Big Land pathing
(engine moveclass), animals (generic danger maps).

## Rotation of obsolete surplus defenders (design, human-requested)

Goal: garrisons modernize and surplus melts, instead of Phalanx
stacks paying civ2civ3 upkeep into the industrial era. Three moves,
cheapest first, all gated on urgency==0 and never below
garrison_floor, max one rotation action per city per turn:

1. **Upgrade in place**: keep stock's dai_upgrade_units(), but cap
   its gold limit with deep_balance_treasury()'s projection (never
   upgrade into deficit). civ2civ3 charges a vet level per upgrade;
   for garrisons the type gap dominates.
2. **Replace-then-disband** (core): when the best present defender
   is obsolete (best buildable defense / present defense >=
   rotation_power_ratio, tunable), inject a small rotation want
   (same channel as the floor, passes the upkeep discount); when
   count exceeds need, disband the WORST obsolete unit, preferring
   disband-in-city-recover so ~half its shields fund the successor.
   New arrives before old leaves: zero garrison gap.
   (Verify civ2civ3 enables the shield-recovering disband action.)
3. **Surplus melt**: count > need and unit has ruleset upkeep ->
   disband worst, only at urgency==0 && danger==0 (hysteresis
   against fear-fluctuation thrash).

Tunables: rotation_power_ratio (~2.0 PLACEHOLDER), rotation_want.
Logging: deep-rotate line per action. Acceptance: cramped+long
head-to-head (tech span creates obsolescence), garrison-modernity
delta at T300, score + city losses, same-world --quick guard.
Commit split: melt; replace-then-disband; treasury-gated upgrade.
Implement after defence-D lands (tuned fear/floor values define
"need").
