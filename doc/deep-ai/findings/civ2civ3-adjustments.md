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

## garden2 post-mortem + defence-G interpretation rule (2026-07-29)

garden2.conf halved land per player by construction (garden's 24x24
kept, players doubled: ~57 tiles each) — a knife-fight regime that
represents nothing. RETIRED in favour of garden2b.conf (34x34,
garden-parity ~115/player). It earned its keep by exposing the
naval-invasion hole; its data stays for comparability.

PRE-REGISTERED before defence-G finished (gen 5, best +31.5): G
trains on garden2+cramped, so its ECONOMY knob values (cm_*, site_*,
upkeep_*) get full weight — their signal comes from cramped, the
legitimate config (0.778 -> 1.129 under the gen-4 incumbent). G's
FEAR/FLOOR knobs (overseas_*, garrison_*, barbarian_fear) are
suspected inflated by garden2's catastrophic-loss dynamics and are
accepted only where defence-H (trained on properly-sized isolated) and
the garden2b/isolated validation sweep agree. Note the island-lottery
NOISE cancels in the mirrored objective; the concern is regime shape
only.

## Reachability-gated menace (2026-07-29, from the garden2b dig)

At defaults, deep fields 2x stock's military by T100 on garden2b,
loses ~60 cities of early expansion for it, and STILL loses 168
cities to T200+ invasions -- the early garrison is insurance that
taxes the growth years and cannot pay out at claim time (a T100
Phalanx is irrelevant to the Musketeer landing it nominally insures
against). Root cause: deep_garrison_menace() measures neighbour army
SIZE but not REACHABILITY, so island neighbours with no navy trigger
the full floor from first contact. Fix (third instance of the
"no fear before the threat class can exist" pattern, after the barb
onset gate and the embarked-only overseas term): weight each met
neighbour's units score by reachability -- same continent full,
overseas gated/scaled by their transport capability. Implement AFTER
defence-G/H land; H's island-trained floor values are the
confirmation signal.

## Horizon credits stop at the tech wall (2026-07-29, human insight)

deep_adjust_building_wants() gates on can_city_build_improvement_now
(RPT_CERTAIN), so a growth-capped city that LACKS the cap-lifter's
tech contributes zero want -- and therefore zero tech pressure --
toward exactly the research that would solve its problem; the cm_cap_*
interim posture is all it has. Fix: evaluate cap-lifting (and health)
credits for tech-gated improvements too (RPT_POSSIBLE) and route
those credits into the TECH want channel ("this city would pay X for
an Aqueduct it cannot build yet" = X pressure on Construction).
Related: cm_cap_trade is the interim-posture knob, and its right
value covaries with map trade-richness (the isolated -405 lesson) --
tile-quality-scaled CM factors remain the deeper fix there.
