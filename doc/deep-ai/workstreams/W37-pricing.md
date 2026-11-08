# W37 — Pricing: one currency instead of 311 knobs

Status: workstream opened 2026-11-07. Epic 1 (S-PRICING-1) in flight.

## The claim

`benchmark/tuning-params.ini` now carries **311 tunables**. Almost every
one is a multiplier or a threshold on a want that stock computes in its
own units: survival under 0.90 blocks a settler, take odds under 0.70
refuse an operation, the first defender is worth 1,100 by fiat, a pillage
is worth denied yield times a gain, a stance gate is a ratio raised to a
tuned power. Each is a human guessing an exchange rate between things the
code never prices against each other, and the optimiser cannot repair
that (objective-layer.md §1: coupled blocks, a (mu,lambda) ES at pop 6).

The Europe late venue (STATUS S-EUROPE) and the interactive Europe game of
2026-11-04..07 showed what the guesses cost on a big map: deep loses
head-to-head (AUC 0.41) and in self-play (−47 score, gold 77 vs 738),
and every mechanism found was a threshold or multiplier deciding wrongly
where an expected value would have decided correctly (STATUS entries
"Defect seen in play", "Design item from the Europe WILD game",
"Barbarian-held cities", "late-game passivity", "threatened nation stops
expanding", "why Alpine Troops").

**End state:** every decision deep makes is a comparison of expected
values in ONE currency (gold-equivalent, the unit `ai_city.worth`,
`city_gold_worth()` and unit build cost already share), computed from
things the game defines: score, yield, combat odds, the wave model. The
tunables that survive are of three kinds and there are a handful of each:
exchange rates the ruleset fixes (computed, not tuned: shields↔gold from
the buy formula, food↔shields from the city's own conversion), one
discount rate / horizon, and risk attitude (the stance axes). Everything
else in the registry is deleted as its consumer migrates.

## Why not all at once

Three reasons, all measured on this project:

- **Identity is the control.** Every scoreboard number rests on byte-
  identity of the gates-off binary with the pin. A big-bang rewrite has
  no gates-off arm and would orphan every comparison. So each consumer
  migrates behind a gate, proves identity with the gate off, and is read
  with the gate on; the old knob dies when the gate is promoted to the
  compiled default.
- **Formulas are predictions and must be calibrated one at a time**
  (the "how do we know the formula is right" protocol): dimensional unit
  tests, calibration of each probability against outcomes, worth against
  realised loss, decision replay from saves, then the paired benchmark.
  Five formulas at once cannot be attributed.
- **The thresholds encode real knowledge in a bad form.** The 0.70 bar
  exists because operations that lose are expensive; the expansion gate
  because settlers die in waves. The EV form must reproduce what the
  threshold protected before it may replace it, which is a per-consumer
  check.

## The consumers, in migration order

Each row is one epic (5–10 commits). Each lands behind a gate, proves
identity with the gate off, is read with the gate on (Europe venues +
quick), and names the knobs it retires.

| # | consumer | today | priced as | knobs retired when promoted |
|---|---|---|---|---|
| 1 | capture / retake / pillage scope, EV op formation, opportunity term, defender per shield | thresholds, class exclusions, max-by-score | S-PRICING-1 (in flight) | `op_take_target` (floor only), `pillage_denial_gain`, part of `stance_assault_gain` |
| 2 | **founding as position**: site value = yield stream NPV + Σ targets Δodds × worth − holding cost; escorted founding as one joint bid; gate asks whether the SITE holds | expgate 0.90, founder_* floors/caps/slack (11 keys), site_* (23 keys) | epic 2 | most `founder_*`, `expansion_gate_enabled`, `site_defence_weight`, `settler_*` |
| 3 | **defence as insurance**: garrison want = P(attack within horizon) × worth(city) − cost, from the wave model; floors become EV floors; the defensive Diplomat likewise: Σ over hostile diplomat actions the approaching unit can perform and that the enablers allow against THIS city (ruleset, government, buildings) of P(success) × damage, instead of a 16,000 fiat (upstream fix branch daimilitary-diplomat-threat has the enabler test) | garrison_floor_* (4), scale_defender, floor_* (6), density_fear (6), boat_shadow (7), survival_* (4), the 1,100 first-defender clamp | epic 3 | ~28 keys |
| 4 | **operations as investments**: form/abort/retarget by EV with loss model; invasion reinforcement from observed rates | op_* (21), lift_* (10), invasion_* | epic 4 | ~25 keys |
| 5 | **forward infrastructure**: roads, rails, forts, airbases priced by ETA removed × target worth; trunks/corridors as the same good | trunk_* (10), frontier_*, worker_* | epic 5 | ~15 keys |
| 6 | **build auction**: every build want (horizon_*, cm_*, wonder, trade, culture) in the currency; credits and ladders replaced by NPV of the yield stream | horizon_* (15), cm_* (21), trade_* (6), wonder_*, culture weights | epic 6 | ~45 keys |
| 7 | **stance = risk attitude only**: axes shape the discount/risk, no longer multiply wants; gains retire | stance_* (13) | epic 7 | ~10 keys |
| 8 | **calibration layer**: predicted-vs-observed tables for P(win), P(hold), P(take), wave size, worth-vs-realised-loss, kept per game and used to correct the inputs; `deep-calib` log line | — | epic 8 | makes the remaining rates self-correcting |
| 9 | **registry cull**: delete retired keys, their struct fields and loaders; registry ≤ 40 keys of the three kinds | 311 keys | epic 9 | the rest |

Diplomacy (love, gifts, chatty) and espionage keep their own scales for
now; they already price in gold where they price at all (deepdiplomacy.c
gift bound), and they are not what the Europe venue indicted.

## Acceptance for the workstream

- Each epic: identity gates-off on ≥ 4 contested + 2 Europe seeds; L0
  dominance tests for its formulas; directional read gates-on on
  europe-late seeds 121–125; STATUS entry with numbers.
- Promotion of a gate to the compiled default: the scoreboard (all venues,
  `--europe`) on the new pin against the previous pin's report, training
  seeds adjudicate, Europe reports; the retired knobs deleted in the same
  epic as the promotion.
- Workstream done when: registry ≤ 40 keys; Europe late head-to-head AUC
  ≥ 0.5 and self-play score Δ ≥ 0 on held-out seeds; no venue regressed
  against the 2026-11-03 scoreboard beyond its SE.

## Decision replay, the tool this needs

`benchmark/run-game.sh` can now resume any save (`START=`), reseed per
seed (`RESEED=1`, fcrand.py) and pin seats (`FIXED_SEATS=`). A decision
replay = load the save at the decision turn, force the alternative (a
tuning gate on/off, or a seat file), play N turns under K seeds, compare
score/worth. Build `benchmark/replay.py` around that in epic 1 or 2 and
use it on every formula before the paired benchmark. The saved positions
from the Europe game (`play-1104-2100/saves/`, `play-1107-*/saves/`) are
the first corpus.

## Taste: the knobs that are supposed to survive (user, 2026-11-07)

The cull removes exchange-rate guesses. It must NOT remove preferences:
a personality is a statement about what a nation wants, and the pricing
machinery is how it gets it. So the registry keeps a fourth kind,
**taste**, and taste enters the arithmetic in one place only: as a weight
on the VALUE of a class of outcome, never on a probability or a cost.
That is what keeps the EV honest while nations differ. Tastes are set per
seat (arena files), seeded from nation traits by default (the stance
layer's existing trait path), and are not optimised for "best": they
define the objective, the benchmark's default profile is just one of them.
The ten the user asked for, and where each plugs in:

| taste | what it weights | today's nearest knob |
|---|---|---|
| `taste_nuke` | the reputation and retaliation cost charged against a nuclear strike's EV (0 = never, 1 = priced like any attack) | none (stock never nukes AI-to-AI well) |
| `taste_peace` | the worth of conquest and the cost of a war declaration; a pacifist discounts captured cities and prices broken treaties high | `peace_seek_gain`, `conquest_culture_gain` |
| `taste_shape` | the crowding price: largepox vs smallpox city spacing | the stance `expand` axis, `stance_pox_gain`, `density_*` |
| `taste_sneak` | the value of covert outcomes (spy, incite, sabotage, surprise war) relative to open force; the penalty for reputation loss | `spy_*`, `diplo_*` |
| `taste_mil` | the econ/mil split: aggression vs polishing own cities | the stance `econ`/`mil` axes |
| `taste_purity` | willingness to pay for aesthetics: raze captured cities, refuse foreign-culture cities, keep borders tidy, at a known score cost | none |
| `taste_far` | the distance discount on everything priced over a path: targets, trade partners, sites; a low value is a long-distance mind | `caravan_discount`, `site_wait_discount`, `trade_route_risk_gain` |
| `taste_frontier` | settle-far-away-ness when near sites are gone or contested: the value of a remote colony relative to a near one | `founder_*`, `lift_*` |
| `taste_sea` | naval vs overland: the weight on sea paths, hulls, coastal sites and naval targets | `boat_*`, `lift_*`, `site_naval_gate` |
| `taste_ocean` | fill-in-the-ocean-ness: the value of ocean tiles, island and one-tile sites, Harbours and offshore platforms | `site_naval_*`, `boat_water_gate` |

Rules for a taste: it is a float in [0, 2] with 1.0 = the benchmark
default (identity), it multiplies a VALUE term, it has one line in the
registry marked `taste`, and the optimiser never moves it. Three of them
already exist as the stance axes (`taste_mil` = econ/mil, `taste_shape` =
expand, culture = cult); epic 7 renames them into this scheme rather than
retiring them. The arena manifest and the Europe seat files are where a
player's tastes live; `deep-stance` logs them today and `deep-taste` will.

## Decapitation: the offensive mirror of the capital stake (user, 2026-11-07)

Desired, occasionally, against a runaway like the Russia of the Europe
game (205 cities): a massive landing to win a beachhead as close to the
enemy capital as the coast allows, then a long-range leapfrog toward the
capital (paratroopers, bombers and fighters, airports and airbases to
pull reinforcements forward, nukes where taste allows) to take it and
set off the civil war that splits the empire. Nothing in either AI does
this, and in this workstream it is not a special case: it is what the
pricing produces when three things exist.

1. **The prize is priced.** Deep already prices the civil-war exposure
   of its OWN capital (`deep_capital_stake_mult`, deepdefence.c: the
   server's `civil_war_triggered` conditions, `EFT_CIVIL_WAR_CHANCE` by
   government, the split of MAX(j/2, 1) of the j eligible cities, each
   at `ai_city.worth`). The mirror is the worth of an ENEMY capital to
   the attacker: its own worth plus P(civil war) × Σ worth of the cities
   that would defect, counted as the enemy's loss and as a weak new
   player we are not at war with. Against a 200-city empire that term is
   two orders of magnitude above any other target on the map, which is
   what makes a campaign worth a hundred units. Epic 4 (operations as
   investments) owns it.
2. **Position is priced.** A beachhead city or fort near the capital
   removes ETA to the prize; an airport there removes it for everything
   that can fly or drop; each is a forward-infrastructure purchase in the
   same currency (epic 5). A campaign is then a chain of such purchases,
   each justified by the prize at the end of the chain, not a scripted
   plan.
3. **Reach is priced.** Paratroopers, bombers, fighters, carriers and
   nukes differ from land units only in the ETA they achieve and the
   losses they take, so the take-odds and loss models must read them
   (W32 air doctrine has the movement half). Nukes carry `taste_nuke` on
   their reputation cost.

What is new in form, not in price: a **campaign** that persists across
turns, owns the chain (landing → airport → leapfrog → capital), re-prices
each step as the front moves and aborts when the prize stops paying.
The operation layer (W33) has the per-target object; the campaign is the
object above it. It belongs after epics 4 and 5 and before 6; it is the
first thing in this workstream that is a mechanism rather than a price,
and the Europe game with a 205-city Russia is its test position
(play-1107-1548/saves/freeciv-T0355-Y01929-interrupted.sav.zst).

Parked (user, 2026-11-07), a server rule rather than AI: civil war splits
are untidy because `civil_war()` (server/plrhand.c) takes each non-capital
city with a coin flip (`fc_rand(2)`) until half are gone, then picks the
rebel capital at random, one rebel only. Tidier splits (contiguous
regions, far-from-capital first, one rebel per N cities for huge empires)
would be a server setting beside `civilwarsize` and an upstream feature,
not deep work; it would also raise the decapitation prize above. Out of
scope for W37; noted so it is not lost.
