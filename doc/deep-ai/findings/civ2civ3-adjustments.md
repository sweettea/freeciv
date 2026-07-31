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

## No grow-hard CM state (2026-07-29, human observation)

Deep's mean city size trajectory is IDENTICAL to stock's (3.5->5.4
over T300 on garden2b) while running fewer cities -- per-city
compensation flows only through science (techs 0.90 on 0.59x
cities), not size. deepcm.c has states war/capped/wonder/default but
no growth state: a size-3 city with headroom works stock-ish weights.
Add cm_grow_food/shield/trade factors for uncapped cities below a
size threshold so quality can compensate quantity. Note civ2civ3 has
NO rapture growth, so CM food weighting and W03 unblocking are the
only growth levers that exist.

## Naval convoy design (2026-07-30, human-requested; NEXT EPIC)

Stock invades desultorily by construction: daiferry.c is a one-boat-
one-passenger state machine (a boat sails the moment it has A
passenger), ai_invasion mass is bookkeeping no decision consumes, and
sea escorts are an unfinished stub (the "Consider making a sea
bodyguard" comment). Landings arrive as unescorted singles that
coastal defenders eat one by one -- observed from the defender's side
all week. Three deep-side slices, each head-to-head benchmarkable on
garden2b/isolated via mechanism metrics (landing-success rate,
cities-taken-per-unit-lost) that clear the n>=50 noise bar:

1. FULL-LOAD GATE: a deep transport in home port sails only when
   loaded to capacity or after convoy_max_wait turns (the wait-cap is
   the anti-deadlock device; ferry state machines deadlock silently:
   boat-waits-for-passenger-waits-for-boat). convoy_min_load=1
   default = current behaviour, ships off.
2. ESCORT PAIRING: loaded transport requests a warship bodyguard;
   warship build-want gains an escort-demand term; pair moves
   together. Biggest lift (cross-class guard assignment).
3. MASSED LANDINGS: hold the convoy until the assembled force clears
   a tunable threshold vs the target's DEFENCE AT ARRIVAL, not its
   defence today (human refinement 2026-07-30): current garrison plus
   eta x a reinforcement-rate estimate (tunable slope on the
   defender's public production/military signals). Travel time is
   the defender's budget -- a 12-turn sail must clear a much higher
   bar than a 2-turn hop, which is exactly why long-range desultory
   attacks fail while looking winnable on paper. Mirror of the W11
   en-route credit, aimed outward. ai_invasion becomes a go/no-go
   gate instead of statistics.

Deadlock analysis required for slice 1: every wait condition needs a
turn-capped escape; watch for interaction with dai_spend_gold
disbanding "idle" boats.

## Sequential-attrition odds (2026-07-30, from the grow-release debate)

All stock threat arithmetic sums linear ratings, but attackers strike
sequentially: a defender rated equal to N attackers must win N
straight fights (p^N survival, not parity). The cm_grow_safety_margin
knob papers over this at one boundary; a proper survival-probability
model (defence vs ordered attacker stream) would upgrade every
consumer of danger/defence comparisons -- future combat workstream,
pairs with tired-attack discipline.

## Investigations closed: walls and caravans (2026-07-30 reads)

WALLS (edge-builder #8): verdict CRUDE BUT NOT BROKEN, no action.
EFT_DEFEND_BONUS want (daieffects.c:501) is already threat-aware
(ocean/continent threat flags gate the bonus terms), and build cost
enters the want subtractively (daicity.c:1676, its own FIXME admits
the crudeness) -- so civ2civ3's cheap 30-shield walls DO score
better than classic's dearer ones. Residual imprecision: the effect
amount ignores civ2civ3's inherent city bonuses, so walls' MARGINAL
value (150%->250% vs 100%->200%) is somewhat overvalued -- but the
error direction favours building walls, which the garden2b data says
deep needs anyway. Revisit only if tuning fights it.

CARAVANS (edge-builder #7): verdict MOSTLY RULESET-AWARE, low
priority. dai_choose_trade_route counts ONLY the one-time entry
bonus (its comment says so), scales it by the ruleset's
Trade_Revenue_Bonus effect (civ2civ3's reductions flow through), and
prefers destinations per trade settings. The waste risk is bounded
to the hard-coded income fantasy ("75% of our trade, 10 squares
away") overestimating the bonus; no evidence of the feared
domestic-route spam. Deep-side suppression NOT needed; magnitude is
tuning-era at most.

## Density-driven expansion race (2026-07-30, human idea; NEXT-CYCLE EPIC)

The fear lattice has a designed-in tension: density fear raises
danger, danger wins build choices for DEFENDERS -- but crowding is
exactly when settling FASTER matters most (land runs out; cramped
and garden2b were lost in the first fifty turns). The coupling that
resolves it, two halves:

1. SETTLER RUSH: scale founder/settler want by the density signal
   (prior + observed), tunable settler_rush_weight -- on crowded
   maps the race and the garrison are BOTH urgent, and the build
   choice must price the race, not just the wall. Interception
   point: founder_want flow (daisettler/daicity) -- needs a read to
   find the deep-side hook; possibly a second advisors extension
   point if the flow is advisor-internal.
2. CHOKEPOINT CLAIMING: with 3+ settlers alive, route one toward the
   nearest land chokepoint (narrow passage / articulation point of
   the home continent's land graph) -- a city in a gate is worth a
   garrison army. Needs a cheap approximate articulation analysis
   (sampled BFS cut detection) over the known-land graph; W02's site
   scoring could take a chokepoint bonus term instead of hard
   routing, which is simpler and composes (site_choke_bonus knob).

Guard for half 1: the rush multiplier must apply to settler WANTS,
not bypass the grow-gate -- a city under real (paid-for) threat
still defends first; the race rides on cities whose fear is covered.

## Seed-205 save forensics (2026-07-31, sonnet dive, Z2-gen2 tuning)

Full save-series diff of deep's worst confirm seed (garden2b, ratio
~0.2), both mirror arms, vs stock on identical starts. Two-stage
loss: compounding expansion gap from T20, discrete conquest rout
T180-300. Per-city growth REFUTED as a cause (deep's mean city size
BEATS stock's at most checkpoints -- grow-hard works). The deficit
is city COUNT: deep flatlines at 24-27 cities ~T100 while stock
reaches 44-66; live settlers T90-140: deep 2-10 vs stock 15-27,
with land still available. Meanwhile deep carries 2-4x stock's
units AND hoards 2000-2479 unspent gold (stock: 92-349), and
game.log shows dozens of deep-rotate disband events per city
T32-193 -- build-then-melt churn burning shields.

Code suspects, in leverage order:
1. SETTLER SUPPRESSION: danger-injected defender wants (density
   fear value 17.6 radius 11.2 in this tuning) may permanently
   outbid settlers in choose_build at ~24 cities; find what caps
   founder output midgame when stock's identical machinery reaches
   44+ cities.
2. ROTATION CHURN: melt-then-rebuild loop (deep-rotate dozens/city)
   suggests the power-ratio guard (1.2 here) thrashes as tech
   advances; needs hysteresis or a cooldown.
3. IDLE TREASURY: 2000+ gold unspent during the stall -- check
   whether deep_balance_treasury / rotation reserve guard is
   starving dai_spend_gold buys.
Also: a civil-war splinter player spawned with ai_type="deep" in
one arm (Sumerian, T236) -- mid-game player creation inherits the
server default aitype, polluting arm purity; run-game.sh should pin
splinter aitype or it randomizes which brain barbarian/civil-war
players get per arm.

## Seed-205 expansion verdict: BUG, not anti-smallpox design (2026-07-31)

Same-island control (each island played by each brain across the
mirror arms): deep packs at EXACTLY stock's density -- median
nearest-neighbour spacing 3 tiles (the citymindist floor) for both
brains on both islands. Deep is not choosing wider spacing; it
places identically and STOPS at 24-27 cities while stock reaches
44-66 on the same terrain. At T140, 42-50% of the tiles stock had
already citied were >2 tiles from any deep city. At deep's own
observed spacing it left 19-20 cities unfounded by T140 (~90-95
pop foregone vs a quality premium worth ~23-27 pop -- the quality
bet loses 4:1 as played).

Settler diagnosis: ASSIGNMENT-LAYER stall, not production and not
site-eval. Live settlers hold at 2-10 continuously T60-T300 (never
zero), but tracked positions show settlers parked UNMOVED on own
city tiles for 50+ turns (e.g. two at (17,66) from T150-T200),
never founding, never dying. Suspects to check, in order: (1)
settlers assigned overseas targets waiting forever for a ferry
(interaction with the passive navy + convoy hold gating), (2)
AIUNIT task assignment dropping founder tasks after
dai_auto_settler_reset in deep_manage_cities, (3) the settler map
ranking no site above the keep-idle threshold despite compliant
land. The (17,66) parked pair is the reproducer: instrument WHY
those two units receive no goto/found order per turn.

## Bug-round resolutions (2026-07-31, overnight)

- Rotation churn: FIXED (ac3aaf890a). Melts now require ruleset
  obsolescence (can_upgrade_unittype != NULL), a strictly-better
  buildable regardless of ratio arithmetic, and non-defender current
  production. Seed-205 rerun verification pending.
- Civilian protection: FIXED (482ffd17df). Treasury sacrifice order
  buildings -> cheapest gold-paying military (danger-free cities
  preferred) -> civilians never by our hand.
- Explorer pump: FIXED (fbc555e79e). Explore want counts only
  reachable-continent unknown/hut tiles; per-pass claims join the
  diminishing divisor.
- Dead buy loop: FIXED (41c122309b). Sentinel restored after the
  spend pass; deep can now buy production for the first time ever.
  Upstream defect recorded in upstream-bugs.md.
- Splinter aitype: NOT A BUG. split_player creates civil-war rebels
  with the PARENT's ai type (plrhand.c:2779) -- symmetric across
  arms by construction. Earlier suspicion withdrawn.
- STILL OPEN: settler assignment stall (the top-line defect --
  settlers parked unmoved on own city tiles 50+ turns; reproducer:
  the (17,66) pair, seed 205 mixed_db T150-T200). Next epic.

## Density fear needs the reachability gate too (2026-07-31, live Europe game)

Observed live (Europe scenario, classic, T28): Hannibal's moated
North-African start builds 4+ Phalanx garrisons with ZERO sea
transports in existence anywhere -- boat_shadow correctly silent,
but deep_density_fear counts hostile cities within radius across
WATER (Iberia over Gibraltar) as if they were land neighbours.
Proximity is priced, reachability is not: the same
unreachable-threat-is-no-threat principle applied to explore/
embassy/menace this round. Fix: cross-continent contributions to
density fear (and the prior) should scale by hostile sea-transport
existence (boat_shadow's gate) or menace_overseas_weight, not count
at full value. Until then the garden2b-tuned dfv=16/dfr=14 taxes
moated starts for invasions that cannot come.

## Posture timing and coherence (2026-07-31, human critique of the Europe game)

Two defects in the hedgehog behaviour, observed live at T42-T111:
1. TIMING: anticipatory fear (density, prior) fires on neighbour
   EXISTENCE from first contact, decades before any neighbour can
   project force -- deep paid T5 insurance for a T60 storm while
   classic raced naked and panic-armed at T60 nearly fast enough.
   Anticipatory terms should ramp with a capability clock (era,
   tech, or the menace signal itself) and DECAY as evidence-based
   menace takes over -- the travelers-report machinery already
   measures actual militarization; density fear keeps paying long
   after better signals exist.
2. COHERENCE: the strategy halves contradict. Hedgehogging pairs
   naturally with TALL+SPREAD (full catchments so growth continues
   under garrison, walls amortized over deep cities); racing pairs
   with DENSE+LIGHT. Deep plays racer spacing (citymindist-floor
   packing, shared tiles) with hedgehog military budgets -- the
   growth ceiling of a turtle with the insurance bill of a fort.
   site_overlap_weight and the fear terms need COUPLING by posture,
   not independent global values: this is the W08 doctrine
   architecture's case in one sentence. Until W08, a tunable
   time-ramp on density fear is the cheap palliative.
