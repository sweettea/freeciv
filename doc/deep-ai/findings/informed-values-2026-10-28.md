# Informed starting values for the expansion causal chain (2026-10-28)

*Stage 2 of three. Stage 1 (`knob-inventory/`) inventoried 311 constants;
the theory-of-play memo derived what strong play is from the rules; the
T50/T100 panel located the gap in time. This memo proposes, for the knobs
on the expansion chain, a starting value derived from a measurement or a
rules fact — never from taste — as ONE stacked candidate arm
("informed-v1") to be adjudicated in games against compiled defaults +
the five S-STACKED gates. Nothing here becomes a default (CLAUDE.md rule
7). Stage 3 audits units and anchors without seeing this reasoning, so
every number carries its derivation.*

**Status: complete (2026-10-28). Part 1 holds the per-knob sections in
causal-chain order (Chains 1–7); Part 2 holds §A–E. Seven values change;
the rest are held at their compiled defaults with the reason stated.**

Companion file: `benchmark/tuning/informed-v1.ini` (only the knobs that
change; everything else falls back to compiled defaults).

## 0. Scope: the knob list

Starting list from the brief, with adds/drops and a one-line reason each.

| knob | in/out | reason |
|---|---|---|
| mil_opportunity_price | in | stack placeholder; the one measured calibration fact (750 vs 175–235) |
| founder_site_cap_slack | in | stack placeholder; founder count |
| founder_join_guard_mult | in | stack placeholder; horizon multiplier |
| frontier_transit_price | in | stack placeholder; turns |
| frontier_transit_stance_floor | in | stack placeholder; enum |
| boat_shadow_distance | in | stack placeholder; hull reaches |
| boat_shadow_count | in | stack placeholder; hull count |
| scale_settler / scale_worker / scale_defender / scale_attacker | in | the currency problem (§A) |
| emergency_value_cap | in (left) | out of the scales' reach; documented, not moved |
| founder_pool_per_site | in | supply vs site count |
| expansion_war_survival | in | expansion gate under war |
| settler_rush_weight | in | crowding × founder bid |
| settler_fearfulness_mult / settler_risk_tolerance | in | site acceptance under danger |
| site_enough_result | in | acceptance threshold |
| land_pressure_tiles | in | acceptance threshold |
| site_own_overlap | in | acceptance threshold (0 at default) |
| site_travel_discount | in | site scorer weight |
| site_potential_weight | in | site scorer weight |
| site_settler_cost_weight | in | site scorer weight |
| site_claim_bonus | in | site scorer weight |
| site_pressure_factor | in | site scorer weight |
| horizon_growth/content/science/culture/production_weight (AUCTION) | in | horizon credit vs military want |
| switch_cost_gain | in | production churn (deficit census: starts ≈ abandonments) |
| upkeep_gold_weight / upkeep_unit_weight | in | the 1-gold/unit bill |
| garrison_floor_danger | in | the floor's danger threshold |
| density_fear_value / density_fear_radius / density_prior_tiles | in | the fear seam; density_prior_tiles' double duty |
| survival_target / survival_fear_value | in | survival seam |
| cm_founder_food / cm_grow_food | in | B.1 break-even depends on parent regrowth |
| explore_discovery_value / explore_explorer_claim | in | Map_Known = 20 points |

Adds/drops (one line each): **dropped from the changed set** — `density_fear_value` (one empty-city switch must stay live when the floor goes off), `survival_target`/`survival_fear_value` (observed threats, not anticipatory; score direction is up), the whole Chain 5 scorer set (city ledger §5: empty searches score zero tiles, so the scorer never refuses them), the five AUCTION horizon weights (the gold bill is the defect, fixed at `upkeep_gold_weight`), `explore_*` (multiply a forced zero). **Added** — `founder_floor_gate` held at 0 explicitly (the one gate whose measured effect was a hoard; the control holds it at 0). `garrison_floor_turn` is named as the bisection alternative to `garrison_floor_danger = 0`, not changed.

## 1. Per-knob sections → Part 1 below (Chains 1–7).
## A–E → Part 2 below.

---

# Part 1 — per-knob sections (causal-chain order)

Conventions used below. "tile-sum" = the site scorer's currency
(weighted tile yields amortized by citizen-arrival turn, `deepsite.c`);
"want" = `adv_want`, the build auction's currency, with the tier encoded
as magnitude: `want >= 1000` (`DAI_WANT_BELOW_MIL_EMERGENCY`) is the
EMERGENCY tier and `deep_want_from_choice()` (`ai/deep/deepwant.c:48-58`)
decodes `(tier, value)` from it. Stock's priorities (`server/advisors/
advdata.c:509-518`): `food_priority = FOOD_WEIGHTING = 30`,
`shield_priority = SHIELD_WEIGHTING = 17`, `gold_priority =
TRADE_WEIGHTING = 18` (`advbuilding.h:19-21`). `MORT = 24`. Venue
geometry: contested 32×32, 8 players; lategame-80 40×40, 6 players
(`benchmark/configs/*.conf`); with the server default `landpercent 30`
that is **38 and 80 land tiles per player**, so `deep_map_crowding()
= MIN(1, 200/land_per_player)` (`deepdefence.c:736`) is **exactly 1.0 on
both venues**.

Score exchange rates (theory memo §A.2, price list §1 of the anchor
pack): a city founded T1–25 = 17.8 ± 1.0 score at T151, T26–50 = 13.0 ±
0.6, T51–75 = 6.0 ± 0.6 (stacked cell, n = 791 seats); 1 citizen = 1
point at the horizon; 1 tech = 2; 1 kill = 1/3; 10 units built = 1; 50
culture = 1; a civ2civ3 achievement = 20. A lost city = −11.2 to −14.2.

## Chain 1 — the stack's seven placeholders

### mil_opportunity_price
- **Unit (from inventory, re-verified at deepdefence.c:8430-8432):** dimensionless multiplier on the founder bid `dom->want` (want units); `charge = price × dom->want; mil->want = MAX(0, mil->want − charge)`. The charge is NOT tier-guarded (there is no `tier == DWT_EMERGENCY` early return in `deep_mil_opportunity_charge`, deepdefence.c:8416-8427), and tier is decoded from magnitude afterwards (`deepwant.c:52`), so a charge can demote an EMERGENCY military bid to NORMAL.
- **Current:** compiled 1.0; placeholder? y; live at defaults? behind `mil_opportunity_gate` (on in this arm and in the control).
- **Anchor:** (d) observed magnitudes at the call site: founder bid ~750 (pinned at `DAI_WANT_DOMESTIC_MAX = 750.075`, daidomestic.c:631; reached because `settler_rush_weight` doubles every fresh bid at crowding 1.0, see §B.3) against a first-defender bid of `1000 + 100 + div` (the flat 100 for an empty city, deepdefence.c:5733-5735, applied at EMERGENCY, :5783-5784) and NORMAL military bids of ~175–235 (fearseat-verdict §7). (e) S-FEARSEAT at 1.0: munits@25 −0.61 ± 0.06, T1–25 military shields 15.8 → 8.0, unitslost@50 +0.07 ± 0.09 (flat). (a) price list: any city founded by T75 is worth ≥ 6.0 score; a first defender is worth 0.1 (units-built/10) plus a loss-avoidance term the fearseat cell measured as not distinguishable from zero.
- **Derivation:** After the charge the first defender's want is `1100 + div − p·d` (want). It falls below the founder `d` iff `d > (1100 + div)/(1 + p)`. At p = 1.0 the threshold is 550 want (+div/2); at 2.0 it is 367 (+div/3). Founder bids are pinned near 750 whenever a site exists, so 1.0 already lets the pinned bid win; what 2.0 adds is the band of bids in [367, 550) — distant or poor sites, and bids below the cap in cities whose rush multiplier has already been consumed. In score, every one of those bids is a city worth 6–13 points against a defender whose measured marginal protection was zero at halved army size, so the exchange rate says the threshold should sit as low as the range allows. The range (`tuning-params.ini:107`) tops out at 2.0.
- **Proposed value:** 2.0 (up ×2; the range maximum).
- **Expected effect:** cities/seat +0.00…+0.05 at T50 (the band of affected bids is narrow once the pinned bid already wins); munits@25 down a further ~0.1; unitslost flat; score +0…+0.5. Panel: units/city at T50 down slightly from 1.00; contested is the venue to read it on (econ has fewer first-defender contests).
- **Falsification:** `deep-milopp` lines with `founder <550` whose city then loses a defender-less city within 25 turns, i.e. cities-lost per seat up by more than its SE in the war venue; or unitslost@50 up by > 1 SE.
- **Interactions / ordering:** acts after `scale_defender` (deepdefence.c:8792 then :8828), so halving the defender want (below) also widens the band this price wins; it is disarmed by `founder_site_cap_gate` withdrawing the founder bid (no bid → no charge). Does not touch cities without a founder bid, which is where the garrison floor (Chain 3) acts.

### founder_site_cap_slack
- **Unit (from inventory, re-verified at deepdefence.c:8218-8219):** count of founders (double) added to `deep_remaining_sites()`; `cap = sites + slack; capped = committed >= cap` where `committed` = founders alive + under construction elsewhere.
- **Current:** compiled 1.0; placeholder? y; live at defaults? behind `founder_site_cap_gate` (on in this arm and control).
- **Anchor:** (d) `deep_remaining_sites()` = reachable free land ÷ (own land tiles per own city) (deepdefence.c:674-690). City ledger §5 (`siteplan/war/c00`, 60 games): the `sites` column reads **3.24 / 3.60 / 3.26** in T51–75 / T76–100 / T126–150 while the fraction of searches coming home empty is 71.6% / 74.0% / **97.2%**. The estimator therefore carries a positive bias of ≳ 3 sites exactly when no site can be found. S-STACKED §4: both venues end with ~1.7 more live founders a seat in the on arm, idle.
- **Derivation:** The slack exists to admit "the founder walking toward a site that opens while it walks" (siteplan verdict §5). The estimator's own late-game bias already admits three such founders before the slack adds a fourth; adding 1.0 double-counts the allowance. At slack 0 the cap is `committed >= sites`, which at the ledger's late values (3.3) still permits three live founders — above baseline's late stock of 2.4 and above the on arm's 1.86. So 0 removes the double count and binds only where `sites` has actually fallen below the live stock.
- **Proposed value:** 0.0 (down by one founder; the range minimum).
- **Expected effect:** live founders at T150 −0.1…−0.4 (from 1.86), pop +0.2…+0.8 (two citizens per settler not built), cities/seat ±0.02 (no site was there to found). Score +0.2…+0.8 (citizens). Both venues; the econ venue (1.7 idle founders at T81) shows it sooner.
- **Falsification:** cities/seat at T100 down by more than its SE (0.06) relative to the stacked control — the cap refusing a founder that would have found a site.
- **Interactions / ordering:** paired with `founder_join_guard_gate` by design (the cap keeps the guard from hoarding); the §B.2 release mechanism is the proper fix for the idle stock and this knob is the only lever available without code.

### founder_join_guard_mult
- **Unit (from inventory, re-verified at deepsettler.c:1846-1847):** dimensionless multiplier on `rebuild` = turns to rebuild a settler (`ceil(cost / best shield surplus)`, deepsettler.c:1789); `window = mult × rebuild` in turns.
- **Current:** compiled 1.0; placeholder? y; live at defaults? behind `founder_join_guard_gate` (on).
- **Anchor:** (d) siteplan verdict §3: at 1.0 the `blocked` signal fires on **99.6%** of 5412 join attempts, so the guard as built is "never join" and the window barely binds. Mean rebuild 11.3 turns.
- **Derivation:** No value of the multiplier changes the 99.6% verdict in a measurable way while the blocked signal is `found-or-cancel-or-blocked`; the verdict says "fix the signal first". The break-even reasoning behind 1.0 (hold a founder for as long as it takes to build another) is the only derivation on record and nothing measured contradicts it. No anchor for a different number → unchanged.
- **Proposed value:** 1.0 (unchanged).
- **Expected effect:** none (unchanged). The idle-stock problem it is blamed for is addressed in §B.2 by a signal selector and a release, not by this multiplier.
- **Falsification:** n/a.
- **Interactions / ordering:** `founder_run_hold` × cap/guard cost ~16 points in the war venue (siteplan §4 item 4) — `founder_run_hold` stays 0 in this arm as in the control.

### frontier_transit_price
- **Unit (from inventory, re-verified at deepsettler.c:180-181):** turns per foreign tile entered; `cost += price × PF_TURN_FACTOR` extra pf cost on a non-allied owner's tile when `frontier_transit_gate` is on. Path cost feeds `turns` and hence the site's `amortize(total, site_travel_discount × turns)` (deepsite.c:2424-2426).
- **Current:** compiled 1.0; placeholder? y; live at defaults? behind `frontier_transit_gate` (on).
- **Anchor:** (c) ruleset/server fact: the transit is only ever opened for DS_PEACE/DS_ARMISTICE owners at stance floor 0 (deeptrunk.c:574); a settler is NonMil so `players_non_invade()` does not stop it (frontier verdict §2, `common/movement.c:775`); a peace treaty that is cancelled goes to war only after `TURNS_LEFT = 16` turns (`server/diplhand.c:57`, `server/plrhand.c:973`). (d) frontier verdict §3: the mean walk to the land behind a peace border is `wturns` 7.0 (T1–25) and 13.6 (T26–50) turns, both shorter than the 16-turn lag.
- **Derivation:** The price's stated job is risk: "a tile of someone else's ground costs a turn". Under peace the combat exposure of a settler on that tile is zero for the whole of a 13.6-turn walk (no hostile unit may be there, and no war can start inside the 16-turn cancellation lag). The walk itself is already charged by `site_travel_discount` at the real terrain cost. The only remaining cost a foreign tile carries is therefore zero, and any positive price double-charges the walk — at 1.0 it roughly doubles the per-tile cost of a 1-move settler, which de-ranks the behind-the-border site by (23/24)^(extra tiles) against a nearer, poorer one.
- **Proposed value:** 0.0 (down from 1.0; the range minimum).
- **Expected effect:** cities/seat +0.02…+0.06 at T75 (the transit's measured city effect at 1.0 was +0.147 ± 0.059 at T75 war; removing the surcharge should recover part of the T26–50 `wfree` 10.9 tiles the census found behind borders); unitslost flat (peace tiles); score +0.3…+0.8. Panel: founders at T50 unchanged, cities at T75. War venue (transit fired 89/arm there vs 361 econ — read both).
- **Falsification:** `unitslost@75` up by > 1 SE in the war venue with `deep-transit` crossings up — settlers dying on transited ground, which the rules argument says cannot happen under peace.
- **Interactions / ordering:** `frontier_transit_stance_floor` must stay 0 for the rules argument to hold (cease-fire owners revert to war in 16 turns; war owners kill settlers). `settler_fearfulness_mult` still prices any actual danger on the path through `adv_avoid_risks()`, chained before this callback.

### frontier_transit_stance_floor
- **Unit (from inventory, re-verified at deeptrunk.c:575-582):** enum 0/1/2 — 0 admits PEACE and ARMISTICE owners only, 1 adds CEASEFIRE, 2 adds WAR and NO_CONTACT.
- **Current:** compiled 0; placeholder? y; live at defaults? behind `frontier_transit_gate` (on).
- **Anchor:** (d) city ledger §5, T51–75 empty searches: `off` = 45.1 tiles seen of which `offpeace` = 43.3; non-peace foreign tiles are 1.8 of 110.5 (1.6%). (c) cease-fire → war after 16 turns (`diplhand.c:57`); a settler inside a war owner's border is a kill.
- **Derivation:** Floor 1 or 2 opens at most 1.6% of the tiles a failed search sees, and every one of them carries the settler loss the `frontier_transit_price` argument above relies on not existing. Nothing to buy, a real cost.
- **Proposed value:** 0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** required at 0 by the `frontier_transit_price = 0` derivation.

### boat_shadow_distance
- **Unit (from inventory, re-verified at deepdefence.c:521-522):** hull reaches; `reach = (int)(distance × (move_rate/SINGLE_MOVE + 1))` tiles; a hostile transport within `reach` counts.
- **Current:** compiled 1.0; placeholder? y; live at defaults? behind `boat_shadow_gate` (on).
- **Anchor:** (d) fearseat §7: at 1.0 only 18.8% of the hulls the flat term feared are within reach; (e) the gate at 1.0 was score-null in both venues with unitslost down in both (fearseat §6, S-MARGINAL re-read); S-FEARSEAT: no rung in either venue raised unitslost above its SE.
- **Derivation:** A larger distance re-approaches the flat term (which fired on 42.1% of T51–75 coastal evaluations and whose removal cost no city). A smaller one (min 0.25) would fear only hulls already adjacent, i.e. after the landing is unavoidable — and the measured unitslost already fell at 1.0, so no observation asks for less warning. One reach = the hull can be adjacent next turn, the minimum horizon a defender build can act on.
- **Proposed value:** 1.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a (would need a coastal city loss ledger showing landings the 1-reach shadow missed; S-FEARSEAT's unitslost column says none appeared).
- **Interactions / ordering:** `boat_shadow_count` below.

### boat_shadow_count
- **Unit (from inventory, re-verified at deepdefence.c:472-473):** count of hulls; `scale = MIN(1, hulls/need)`.
- **Current:** compiled 1; placeholder? y; live at defaults? behind `boat_shadow_gate` (on).
- **Anchor:** (c) theory memo §E: 8 of deep's 13 takes were walk-ins (undefended or size-1 cities); a walk-in needs one unit, and a civ2civ3 Trireme carries 2 — one hull. A defended town needs ~6.5 Catapults (3–4 hulls), and the §E arithmetic says that landing is not attempted against a walled town anyway.
- **Derivation:** The threat the shadow guards against in these venues is the one-hull walk-in; `need = 1` saturates the fear at exactly that threat. Raising it to 3–4 would discount the only landing that happens.
- **Proposed value:** 1 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** none beyond `boat_shadow_distance`.

## Chain 2 — the bid scales and the emergency cap (where the currencies meet)

### scale_settler
- **Unit (from inventory, re-verified at deepdefence.c:8347, 8356):** dimensionless multiplier on `city_data->founder_want` (want; native = site-scorer tile-sum output, deepsite.c:2830), applied in `deep_scale_domestic_bids()` just before `domestic_advisor_choose_build()` (called at :8709) and restored after; skipped at exactly 1.0.
- **Current:** compiled 1.0; placeholder? n (R3 "one scale per class", measured); live at defaults? n (multiply skipped at 1.0).
- **Anchor:** (a)+(d): a T26–50 city = 13.0 score ↔ a founder bid pinned at ~750 want (cap) or 358 want mean `best_net` for found sites (ledger §5) → 27–58 want per score point. A Temple (building anchor p50 ~192 want, C inventory) = the size-4 gate (+1…+2 citizens held at the horizon) + 1 History/turn (≈ 2 points by T150 if built at T50) ≈ 3–4 points → 48–64 want per point. (e) R3 arm S = 0.20: cities −13% at isolated-150 n=40, score −0.05 (null); contested +1.67 ± 2.1 (null).
- **Derivation:** The founder and the building anchor already convert to score at the same rate within the uncertainty of both (27–58 vs 48–64 want/point); W01's "one currency" is empirically satisfied at 1.0 for these two classes. The only measured move (0.20) cost cities without buying score. Note that on both venues `settler_rush_weight` (Chain 4) already multiplies every fresh founder bid by exactly 2.0 before the 750 cap, so the effective settler scale is 2.0-then-clamped; a value above 1.0 here can only pin more bids at the cap, a value below it was measured to cost cities.
- **Proposed value:** 1.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `settler_rush_weight` (applied earlier, deepdefence.c:9265-9268), `mil_opportunity_price` (charges `price × dom->want` after the advisor's clamp), `founder_site_cap_gate` (zeroes the bid before the scale is read).

### scale_worker
- **Unit (from inventory, re-verified at deepdefence.c:8348, 8367):** dimensionless multiplier on `city_data->worker_want` (want; native = ai/default's terrain-improvement estimate, p90 69–72, max 148–150).
- **Current:** compiled 1.0; placeholder? n; live at defaults? n (skipped at 1.0).
- **Anchor:** (e) R3 arm W = 3.0: worker picks 75 → 196 (t +12.1), score −1.52 (t −1.17) contested n=20; "the poison in the bundle" (ALL arm −24.30, t −6.28 at isolated-150). (a) rough score worth of one worker: +1 food on one worked tile for ~100 turns = 100 food ≈ 5–10 growth steps at civ2civ3's 10–20 food per step ≈ 2–5 citizens ≈ 2–5 points ↔ 70 want → 14–35 want/point, i.e. the worker is if anything *under*-bid against the Temple's 48–64 — yet the only measured move up was harmful.
- **Derivation:** The score arithmetic and the measurement disagree in sign; under rule 6 the measurement wins and under rule 7 I have no derivation that survives it (the worker's marginal tile is not the best unworked tile the arithmetic assumed). No anchor for a new value → unchanged.
- **Proposed value:** 1.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** same advisor auction as `scale_settler`; R3: "two rescales that are each survivable are not jointly survivable".

### scale_defender
- **Unit (from inventory, re-verified at deepwant.c:143, 235 and deepdefence.c:8792):** dimensionless multiplier on `choice->want` of a `DWC_DEFENDER` military choice, NORMAL tier only (`deep_want_scale_apply` returns early on EMERGENCY); native = stock's `danger%` as deep forms it: for a city that already holds a defender, `100 × danger² / our_def` capped at 100 when `urgency == 0` (deepdefence.c:5728-5741), plus martial law and the deterrence dividend.
- **Current:** compiled 1.0; placeholder? n (R3: "NO FOLD, no derivation"); live at defaults? n (skipped at 1.0).
- **Anchor:** (b) baseline-matching on the panel: units/city at T50 deep 1.00 (stack on) vs baseline 0.33; T100 2.60 vs 1.79; T150 3.42 vs 2.57 (panel §2/§1). Subtracting founders+workers (deep 1.86/4.76 = 0.39 per city at T150, baseline 2.40/5.28 = 0.45) leaves military ≈ 3.0 vs 2.1 per city. (a) score worth of the marginal (second-and-later) defender: deep loses 0.131 cities/seat/game vs baseline 0.394 (ledger §1), a difference of 0.26 cities × 12.7 points (mean of lost coefficients) ≈ 3.4 points per seat, bought by ≈ 0.9 extra units × 4.8 cities ≈ 4.3 units → **≈ 0.8 points of protection per marginal unit**; its cost is 20 shields (Phalanx, `units.ruleset`) + 1 gold/turn above the 2–3 free per city (`effects.ruleset:922-944`, `Shield2Gold_Pct 100`) for its lifetime ≈ 75–100 gold ≈ 37–50 shields at the 2-gold-per-shield buy rate (`improvement.c:319`) ≈ 1.2–2.3 Settlers' worth of production ≈ one marginal city's shields. Net ≤ 0 score at the margin.
- **Derivation:** Two derivations. (i) Score exchange: a want of 100–235 at 48–64 want/point prices the marginal defender at 2–4 points; its measured net worth is ≤ 0.8 − (cost) ≈ 0 → the scale that puts it at its score price is ~0.1–0.2. (ii) Baseline-matching: deep carries ≈ 0.9 excess military per city, about 40% of its ~2.1 non-first defenders; a 40–50% cut of the NORMAL-tier defender want, compared linearly against the building anchor p50 192, is the direct reading. (ii) is the conservative one and is the one the panel can check at T50/T100 without a new instrument; (i) is the second bisection rung if (ii) clears.
- **Proposed value:** 0.5 (down ×2).
- **Expected effect:** units/city at T100 −0.3…−0.5 (from 2.60); gold at T100 up by 30–60 (1 gold/unit/turn saved); cities/seat +0.05…+0.15 (the shields become settlers only where a site exists — the stack's cap means they otherwise become buildings); unitslost down (fewer units) — flat per unit; score +0.5…+2. Panel: units/city T100, gold band. Both venues.
- **Falsification:** cities lost per seat up by more than its SE in the war venue (deep 0.131/seat/game today), or `deep-surv` deficit fear rising into the EMERGENCY tier more often (the scale never touches that tier, so a rise there means the cut defenders were the ones that kept `surv` above target).
- **Interactions / ordering:** `garrison_floor_danger` (Chain 3) is "one lever applied twice" with this knob (`tuning-blocks.ini:104-107`): the floor creates the want and the scale multiplies it — but the floor's want is EMERGENCY (empty city) and is out of this scale's reach, so the two are complementary, not redundant. Acts before `switch_cost`, `unit_upkeep_charge` and `mil_opportunity_price` (deepdefence.c:8792 → :8828), so it widens the band the opportunity price wins.

### scale_attacker
- **Unit (from inventory, re-verified at deepwant.c:145, 235):** dimensionless multiplier on a `DWC_ATTACKER` (CT_ATTACKER, transport_capacity 0) want, NORMAL tier only; native = stock kill-expectation (`kill_something_with`), of which the EMERGENCY-tier population is p50 6485.
- **Current:** compiled 1.0; placeholder? n; live at defaults? n (skipped at 1.0).
- **Anchor:** (d) S-OPXRATE: launch-gate forecast P(take) 0.89 vs realised 4.4% — a 20× calibration error on city assaults; 354 member deaths per 2 kills at the walls. (a) 1 kill = 1/3 point ≈ 17 want at 50 want/point.
- **Derivation:** The calibration fact would put a city-assault attacker's want at 1/20 of its forecast, i.e. the range minimum 0.05. But the NORMAL-tier attacker population is a mix of assault members and field/barbarian killers (which realise their kills: baseline books 9.2 kills/seat), and the inventory records no per-class NORMAL-tier distribution (C inventory Unresolved 8). Without the mix, the 0.05 figure applies to an unknown fraction of the bids and the rest would be mis-scaled by up to 20× the other way. No anchor for the mixture → unchanged; the measurement that supplies it is named in §D.
- **Proposed value:** 1.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `attack_value_weight`, `conquest_worth_mult` form the want; `mil_opportunity_price` charges it too (class DWC_ATTACKER is admitted at deepdefence.c:8424).

### emergency_value_cap
- **Unit (from inventory, re-verified at deepwant.c:171, 209-217):** want units — absolute ceiling on `w.value = want − 1000` inside the EMERGENCY tier; the tier is never moved.
- **Current:** compiled 0.0 (= no cap); placeholder? n (measured inert); live at defaults? n.
- **Anchor:** (e) R3 arm CAP = 400: 22.1 clamps/game, 19/20 games byte-identical, score −0.21 (t −1.00); knob-screen: in 143 k logged merges no ±1σ move changes one, because the merge is tier-first and a domestic want is ≤ 750.075 and always NORMAL.
- **Derivation:** The cap can only reorder military candidates against each other inside the emergency tier (a 259 976 attacker vs a 100 first defender). That ordering is not logged and nothing on the expansion chain reads it. The only emergency-tier lever that reaches the domestic/military merge is a charge that crosses the 1000 boundary — which is exactly what `mil_opportunity_price` does and this cap cannot.
- **Proposed value:** 0.0 (unchanged). Documented here because §A must say plainly that the emergency tier is out of the scales' reach.
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** none on this chain.

## Chain 3 — the fear seam: what makes a city buy a unit before a settler

The chain (D inventory, code-verified): injected fears add to
`city_data->danger`; `deep_apply_garrison_floor()` fires only when danger
is still exactly 0 and the city tile holds fewer than `garrison_floor`
units; then `deep_military_choose_build()` turns danger into a want —
for an EMPTY city (`our_def == 0`) any `danger >= 1` gives a FLAT 100 at
the EMERGENCY tier (deepdefence.c:5733-5735, :5783-5784); for a city that
holds a defender the want is `100 × danger²/our_def` capped at 100 when
`urgency == 0`, at NORMAL tier. So for the first defender every fear knob
is an on/off switch, and only the floor and the opportunity charge
(Chain 1) can move it; the fear MAGNITUDES move the second defender.

### garrison_floor_danger
- **Unit (from inventory, re-verified at deepdefence.c:1995-1997):** danger units (the stock `assess_danger()` field, unsigned int); `applied = (uint)(garrison_floor_danger × updisc × menace × w.best × fmult)`, written into `city_data->danger` for a city with `danger == 0` and fewer than `garrison_floor` (1) units on its tile. For such a city the result is the EMERGENCY flat 100 for any value ≥ 1 (after truncation), so within its range it is a switch.
- **Current:** compiled 40; placeholder? n (but "never swept against held-out seeds with a verdict"); live at defaults? y — tier A (DEFOPS), flips 113.1/game, 63.5% of floor decisions.
- **Anchor:** (e) STATUS ~7140 floor sizing, 20 paired seeds, pinned `536df20ee44c`, floor-danger 0 vs 40: isolated-150 **ON − OFF score −6.12 ± 3.18 (t −1.93), cities −1.09 (t −2.77)**; lategame-80 **−2.52 ± 1.71 (t −1.47), cities −0.66 (t −4.16)**; city losses did not fall with the floor on (+0.80, +1.40). (b) baseline-matching: baseline's units/city at T50 is 0.33 with 3.24 cities — it does not garrison every city; deep's is 1.00 and 73–74% of deep's military city-turns in T1–25 sit at raw danger 0 (fearseat E34.2), i.e. the floor is the empty-city switch most of the time. (e) Derby-6: zeroing the floor family flipped the Europe game from −809 to +337 (n = 1, four knobs). (c) theory §E/§F: the doctrine wants a *fortification* (Walls, 30 shields, 0 upkeep) per border city before `onsetbarbs` (T120 civ2civ3), not a unit per city from T1 at 1 gold/turn.
- **Derivation:** The one prior arm that moved this knob alone is the strongest city-endpoint result in the inventory (t −2.77 and −4.16 for the floor being ON), on two venues, and it is the direction baseline's own garrison count points. A token unit costs 10–20 shields now and ~1 gold/turn for its life (`Shield2Gold_Pct 100` above 2 free under Despotism, `effects.ruleset:931-935`); in the T1–50 window those shields are 1/3–2/3 of a Settlers (30 shields, 13–17.8 points) and the gold is the treasury gap the panel shows (47 vs 177 at T50). The protection it buys was measured at zero (losses did not fall). The informed value is therefore the switch's OFF position; the magnitude between 1 and 200 is irrelevant for the empty city it exists for.
- **Proposed value:** 0 (off; down from 40).
- **Expected effect:** the largest lever in this arm. Cities/seat **+0.2…+0.6 at T150** (the prior arm's +1.09/+0.66 were against gates-off deep on a different code era; with the stack's cap on, the freed shields become settlers only while sites remain, so I register the lower range); units/city at T50 **1.00 → 0.5–0.7**; gold at T50 +40…+80; unitslost down (fewer units standing); score +2…+5. Both venues; contested first.
- **Falsification:** cities lost per seat per game rising above baseline's 0.394 (deep today 0.131) in the war venue, or unitslost@75 up by > 1 SE while munits is down — undefended cities being walked into. Also the barbarian window: if cities lost concentrate after T120 (`onsetbarbs`), the right fix is `garrison_floor_turn = 120` (the bisection alternative), not restoring 40.
- **Interactions / ordering:** `garrison_floor` (count) stays 1 — it is also read by `deep_fear_satisfaction()` and the melt, so the inventory's instruction is to zero the *danger*, not the count. With the floor off, `density_fear_value` (below) becomes the dominant first-defender switch after contact (54.9% of T1–25 build-seat evaluations carried a density term with the floor on) — so this arm does NOT also zero density, to keep one switch live. `mil_opportunity_price` (Chain 1) is what lets a founder beat the first defender when density does fire.

### density_fear_value
- **Unit (from inventory, re-verified at deepdefence.c:989):** danger units per met non-allied city within `density_fear_radius`, divided by (1 + distance); summed, × ramp (1.0 at `density_fear_ramp_turns` 0) × updisc × sat × fmult, truncated to unsigned int.
- **Current:** compiled 6; placeholder? n; live at defaults? y (54.9% / 72.1% of T1–25 / T26–50 build-seat evaluations).
- **Anchor:** (e) S-FEARSEAT: the two arms that reduced anticipatory fear (`density_fear_ramp_turns = 62`, `fear_evidence_gate`) moved the opening army by nothing measurable (munits@25 +0.01 ± 0.03) because the floor took over at danger 0, and scored **−2.48 ± 1.63 and −2.40 ± 1.32 on contested** (positive on econ) — the deferred anticipation bought a larger late army. (b) baseline has no anticipatory term at all (stock `assess_danger()` counts units within reach).
- **Derivation:** With the floor off, this term is the remaining empty-city switch; zeroing it too would make deep baseline-like in the opening (no first defender until a unit is seen) but the only measured reductions of this family were negative on the war venue. Two levers on one seam cannot be read in one arm; the floor has the measurement and this does not. Unchanged, and named as the second rung if the floor clears.
- **Proposed value:** 6 (unchanged).
- **Expected effect:** none directly; it becomes the switch that the opportunity price and the (unchanged) 6/(1+d) arithmetic decide.
- **Falsification:** n/a.
- **Interactions / ordering:** with the floor off, uint truncation matters: one neighbour at distance ≥ 6 gives 6/7 → 0 danger → no first defender; two at distance 4 give 2. Those are the cities whose first defender the stack would now skip.

### density_fear_radius
- **Unit (from inventory, re-verified at deepdefence.c:986):** tiles (`real_map_distance`), also reused as R in the floor's border class and the fortress ground weight.
- **Current:** compiled 8; placeholder? n; live at defaults? y.
- **Anchor:** (d) STATUS ~15430: at 8 "an enemy city is within reach of nearly every tile" on a 32×32 map. No arm.
- **Derivation:** No measurement of the term's magnitude as a function of radius exists, and the knob has three consumers (fear, floor border weight, fortress weight) that would move together. No anchor → unchanged.
- **Proposed value:** 8 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `floor_border_weight` (dead once the floor is off), `base_fort_gain`.

### density_prior_tiles
- **Unit (from inventory, re-verified at deepdefence.c:736-737):** land tiles per player (reference); `crowding = MIN(1, 200 / land_per_player)`, where `land_per_player = MAP_INDEX_SIZE × landpercent / 100 / players`.
- **Current:** compiled 200; placeholder? n; live at defaults? y as a read — but see derivation.
- **Anchor:** (c) venue arithmetic: contested 32×32×30%/8 = **38** land tiles per player, lategame-80 40×40×30%/6 = **80**; both below 200, so `deep_map_crowding()` returns exactly **1.0** on every benchmark game. Its three live consumers are therefore constants: the pre-contact prior is `density_prior_value × ramp`; the settler bid is `founder_want × (1 + settler_rush_weight)` = ×2; the choke bonus is `site_choke_bonus` × 1.
- **Derivation:** Any value ≥ 80 is identical on both venues; a value below 38 would start to scale three unrelated mechanisms at once (the double duty the D inventory flags). There is no anchor for a "right" crowding reference and the knob is inert in its current range on the venues that adjudicate. Unchanged; the double duty goes to §B.3 as a mechanism.
- **Proposed value:** 200 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `settler_rush_weight` (Chain 4) — on these venues it is a constant ×2 on the founder bid, which is why the "~750" calibration fact is really the 750.075 cap.

### survival_target
- **Unit (from inventory, re-verified at deepdefence.c:2046-2050 and the 35 threshold sites):** probability 0–1 compared with `surv`, the chained-duel survival probability over `survival_horizon` turns.
- **Current:** compiled 0.5; placeholder? n; live at defaults? y (1086 fires/game).
- **Anchor:** none isolated; (d) STATUS 13540 notes 0.85 makes a staging city never spare a blade. The survival deficit is built from OBSERVED at-war units within reach — the one fear term that is baseline-like in kind.
- **Derivation:** The threshold has 35 consumers (melt, sorties, counter-claims, expansion gate, CM veto, building ladder "threatened"); moving it moves them all and no measurement separates them. A city that survives an observed wave with probability 0.5 is a coin flip on a −11…−14 point asset; the direction a score argument gives is UP, not down, which is the opposite of this arm's purpose and would need its own cell. Unchanged.
- **Proposed value:** 0.5 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `expansion_war_survival` (Chain 4) is a separate threshold on the same `surv` by design.

### survival_fear_value
- **Unit (from inventory, re-verified at deepdefence.c:2046-2050):** danger units × normalised deficit `(target − surv)/target` × updisc × away_mult, truncated; NOT scaled by stance.
- **Current:** compiled 20.0; placeholder? n; live at defaults? y (607 fires/game).
- **Anchor:** none isolated (M4b: "must sweep deterrence_gain against defence_worth_mult and survival_fear_value", never run).
- **Derivation:** For an empty city it is a switch like every other fear; for a city with a defender under an observed wave it is the one term that answers a real threat, and a full deficit (surv = 0) injects 20 danger → `qdanger` 400 against `our_def` of a Phalanx (~9) → want capped 100 (NORMAL) unless urgency is set. A real wave deserves to reach the emergency tier; nothing here says 20 is wrong in either direction and cutting it in the same arm that cuts the floor and the defender scale would remove the last real-threat response. Unchanged.
- **Proposed value:** 20.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a — but this is the knob the falsification of `scale_defender`/`garrison_floor_danger` would point at if cities fall to observed waves.
- **Interactions / ordering:** `survival_target`, `survival_horizon`, `committed_fear_mult` (identity at 1.0).

### upkeep_unit_weight
- **Unit (from inventory, re-verified at deepdefence.c:439):** 1/(upkeep point); `updisc = 1/(1 + weight × cheapest)` where `cheapest` = min buildable defender's gold+food upkeep per turn (utype-level, before the city's free allowance); multiplies every injected fear term.
- **Current:** compiled 0.5; placeholder? n; live at defaults? y.
- **Anchor:** (c) civ2civ3: a Warriors/Phalanx costs 1 shield of upkeep converted to 1 gold (`Shield2Gold_Pct 100`, `effects.ruleset:745-751`) plus 1 food above the free allowance; at the type level `cheapest` = 1–2, so `updisc` = 0.67–0.50 today. The discount is on a danger field whose consumer, for the empty city, is a switch (any ≥ 1 after truncation).
- **Derivation:** Because the first-defender want is a switch, `updisc` only changes the magnitude for the second defender, where `scale_defender` (Chain 2) already acts as a direct multiplier on the want itself. Two multipliers on one quantity in one arm cannot be attributed; the scale has the baseline-matching anchor and this does not. Unchanged.
- **Proposed value:** 0.5 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** multiplies `overseas_fear`, `boat_shadow_fear`, `density_fear_value`, `barbarian_fear`, `survival_fear_value`, `rotation_want`, `garrison_floor_danger` (the last now 0).

## Chain 4 — founder supply: how many settlers exist and when they are built

### founder_pool_per_site
- **Unit (from inventory, re-verified at deepsettler.c:1929-1930):** founders per remaining site; `founders <= (int)(pool × deep_remaining_sites())` refuses the join.
- **Current:** compiled 0.06; placeholder? y ("calibrated on primary-120", stale by ~13× after the reachability filter); live at defaults? y (tier A, 9.9 flips/game).
- **Anchor:** (d) ledger §5: `sites` = 9.3 / 5.5 / 3.2 by band → target = 0.56 / 0.33 / 0.19 founders → after the `(int)` cast the pool protects **zero** founders at every band on contested. (b) baseline's live founder stock: 1.42 / 2.93 / 2.40 at T50/100/150, which the theory memo calls wasteful ("what wins is the founding rate the stock serves").
- **Derivation:** With `founder_join_guard_gate` on, the join is refused on 99.6% of attempts before this gate is consulted (the guard is placed last, deepsettler.c:1951, but either refusal suffices), so the pool is a dead second lock in this arm. Baseline-matching would put it at 0.26–0.8 per site, but the guard already restores the stock to baseline's level (panel §2: 1.73 vs 1.42 at T50). Nothing to buy from moving it while the guard holds → unchanged.
- **Proposed value:** 0.06 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** superseded by the guard in this arm; becomes live again only if §B.2's release lets the guard fall.

### expansion_war_survival
- **Unit (from inventory, re-verified at deepdefence.c:4941-4942):** probability 0–1; on war footing, with a hostile wave, `surv < threshold` zeroes `worker_want` and `founder_want` in that city.
- **Current:** compiled 0.9; placeholder? n; live at defaults? y, war-footing city-turns only (tier A, 15.3 flips/game).
- **Anchor:** (e) ERA stage 2 (tech50war-200, n = 8): 0.9 vs 0.0 (stock's gate) — stock's gate blocks 3.6× more, founds 19 fewer cities, score −148.9/seed (t −1.83). Directional, n = 8, a different venue.
- **Derivation:** The only evidence is directional for 0.9 over stock. A score argument: a city building a settler under a real wave with surv < 0.9 risks a 30-shield unit against a −11…−14 asset; 0.9 says "stop expanding when there is a 10% chance of losing the city", which is already permissive. No anchor for another number.
- **Proposed value:** 0.9 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `survival_target` (0.5) is the garrison's own bar; this one is deliberately stricter.

### settler_rush_weight
- **Unit (from inventory, re-verified at deepdefence.c:9265-9268):** dimensionless multiplier on `deep_map_crowding()` (0..1); `founder_want ×= 1 + weight × crowding` on fresh founder wants.
- **Current:** compiled 1.0; placeholder? n; live at defaults? y.
- **Anchor:** (c) crowding ≡ 1.0 on both venues (Chain 3, `density_prior_tiles`), so this is a constant **×2** on every fresh founder bid, applied before `ai/default`'s clamp to 750.075. (a) the score value of a city does not depend on crowding — the price list is per founding band, the same per turn remaining across venues (anchor pack §1: "the two venues do not disagree about what a city is worth per turn").
- **Derivation:** As implemented on these venues the knob is `scale_settler = 2.0` under another name, then clamped. Removing it (0) would halve every founder bid and un-pin them from the cap — that is R3's arm S = 0.20 in kind, which cost 13% of cities. Keeping it at 1.0 keeps the measured behaviour; there is no crowding gradient on these maps for any other value to act on. Unchanged, flagged in §B.3.
- **Proposed value:** 1.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `scale_settler` (the two are one lever here), `mil_opportunity_price` (whose threshold arithmetic assumes the pinned 750).

### settler_fearfulness_mult
- **Unit (from inventory, re-verified at deepsettler.c:403-405):** dimensionless multiple of `NORMAL_STACKING_FEARFULNESS` (= PF_TURN_FACTOR/36); pf extra cost per tile = settler build shields × p_killed × fearfulness, in 1/PF_TURN_FACTOR turns.
- **Current:** compiled 2.0; placeholder? n (W05: "kept above 1"); live at defaults? y (tier B, ~892 fires/game).
- **Anchor:** (e) 7-arm probe (n = 5, no SE, arm value unstated): the only site weight that moved city count (133 → 150). (d) ledger §5: empty searches have `scored = 0.00` — every refusal is pre-valuation (`off`/`noterr`/`nomin`/`mindist`), and danger-priced *paths* do not appear in that ladder (pf prunes before `seen`).
- **Derivation:** The multiplier prices risk along the walk; its city effect, if any, is through tiles the pf never offers the scorer, which no instrument has counted (the `deep-sitewall` counterfactual was run for ownership, not for danger). A value of 1.0 (stock) is the baseline-matching reading, but W05's reason for 2.0 (a dead settler also loses its walked turns) is a correct accounting and the n = 5 probe's direction is unknown. No anchor that survives → unchanged; the measurement is named in §D.
- **Proposed value:** 2.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `settler_risk_tolerance` (veto vs price on the same pf parameter), `frontier_transit_price` (chained after it).

### settler_risk_tolerance
- **Unit (from inventory, re-verified at deepsettler.c:352, 368):** dimensionless multiplier on the settler's squared defence in the overnight veto `a²×10 >= d × tolerance`.
- **Current:** compiled 1.0 (= stock's bar); placeholder? n; live at defaults? y.
- **Anchor:** (b) 1.0 is exactly the stock bar, so baseline-matching is satisfied by construction.
- **Derivation:** Baseline finds the sites with this bar; nothing says deep's refusals come from it (ledger: they are ownership/terrain/citymindist). Unchanged.
- **Proposed value:** 1.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `settler_guard_tolerance` (escort request, same bar).

### cm_founder_food
- **Unit (from inventory, re-verified at deepcm.c:754-777):** dimensionless multiplier on the founder unit's food upkeep (1 food/turn for civ2civ3 Settlers); `food_floor = ceil(k × upkeep)` written to `cmp.minimal_surplus[O_FOOD]` for a NO_POSTURE city with `founder_want != 0`; the city also takes the GROW factors.
- **Current:** compiled 0.0; placeholder? y; live at defaults? n (gate `> 0`).
- **Anchor:** (c) theory §B/§B.1: civ2civ3 granary 20 food to the first growth, then **10 food per step** to size 4 (`Growth_Food 50` below size 4, `granary_food_ini 20…`), and the break-even arithmetic assumes the parent regrows the 2 citizens a Settlers costs "in 10–15 turns (30 food at +2…+3)". (d) liveness: 69 and 76 `deep-cmfound` firings per lategame-80 game at 1.0 (STATUS 12974-13038); no A/B.
- **Derivation:** A city that is about to build a Settlers (founder_want != 0) must be size ≥ 3 and will drop 2 pop; the memo's break-even requires a food surplus of +2/turn to regrow one citizen per 10 turns. The founder's own upkeep is 1 food, so the floor that implements the +2 assumption is `k = 2` (floor = ceil(2 × 1) = 2 food/turn). k = 1 would only cover the settler's own upkeep; k = 3 would demand +3, which the §B tile arithmetic (grassland 2/1/2 after the despotism penalty, +2 food worked) does not generally allow at size 1–3 without starving shields.
- **Proposed value:** 2.0 (on, from 0.0).
- **Expected effect:** founders built per seat +0.1…+0.3 by T100 (parents regrow sooner); pop/city at T50 −0.1…−0.3 (food tiles instead of shield tiles slow the Settlers build itself — the two effects oppose); cities/seat +0.0…+0.1; score ±1. Econ venue (8 starting settlers, migration) shows the food side; war venue the settler side.
- **Falsification:** settler production time per Settlers up by more than the regrowth gain (read `deep-prodswitch`/`unitsbuilt` of Settlers per seat falling while `deep-cmfound` fires), or pop at T50 down by > 1 SE with no founder gain.
- **Interactions / ordering:** reuses `cm_grow_food/shield/trade` (16/4/4); `cm_war_survival` vetoes it under threat. Pairs with `founder_site_cap_slack = 0` (more founders built sooner meets a tighter cap late).

### cm_grow_food
- **Unit (from inventory, re-verified at deepcm.c:431):** integer CM weight → `cm_parameter.factor[O_FOOD]` in the GROW posture (size < `cm_grow_below` = 6, not at war/capped/wonder); siblings shield 4, trade 4.
- **Current:** compiled 16; placeholder? n; live at defaults? y.
- **Anchor:** (e) pv3's GCM block (17 cm keys incl. this at 18) vs pv3: −24.2 ± 13.4 (t −1.80, econ, n = 16) — block-level, negative. (a) score exchange while a legal site exists: 1 food ≈ 0.1 citizen-step (10 food/step) ≈ 0.1 point; 1 shield ≈ 1/30 Settlers ≈ 13/30 = 0.43 point if the settler founds (T26–50 price), ≈ 0.17 if it becomes a Temple; 1 trade ≈ 0.5 bulb ≈ 1/300 tech ≈ 0.007 point + gold. So shields ≥ food ≫ trade while sites remain, the reverse of 16:4:4.
- **Derivation:** The exchange rate says the GROW posture over-weights food by ~2–4× relative to shields *while a site exists*, and is right once sites are gone (a citizen then earns its own point and the shields have nowhere to go). The posture has no "site exists" input; the knob cannot express the switch; and the one block-level move (up) was negative. Changing the ratio in this arm would confound the supply-side knobs above. Unchanged; the state-dependent weighting is a mechanism (§B.5).
- **Proposed value:** 16 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `cm_founder_food` reuses these factors; `cm_grow_below`.

## Chain 5 — site acceptance: what the scorer refuses

**A finding that reorders this chain.** The brief's working hypothesis
("after the stack the binding constraint is site acceptance … a deep-side
refusal: the border wall, `site_enough_result`, `land_pressure_tiles`,
`site_own_overlap`, crowding prices, risk tolerance") is only half
supported by the ledger it cites. City ledger §5 (60 games): in every
empty search `scored = 0.00` — **not one tile reaches the scorer**. The
refusals that make a search empty are, in T51–75, `off` 45.1 (of which
`offpeace` 43.3), `noterr` 30.4, `nomin` 30.5, `mindist` 4.1 of 110.5
tiles seen — ownership, terrain and `citymindist`, all pre-valuation. The
scorer's thresholds and weights (`site_enough_result`, the cost weights,
`site_own_overlap`, the rival/claim terms) never see an empty search's
tiles and therefore cannot be what refuses them. The one refusal that
*was* deep-only is the TB_IGNORE wall, and the stack's `frontier_transit_
gate` removes it for peace owners (frontier verdict §3: `wextra` = 0.00 in
every band with the gate on). What is left after the stack is the
ruleset's `citymindist 3`, terrain, and foreign ground that founding is
not permitted on (frontier verdict §1: "no benchmarked ruleset permits
founding on a foreign tile") — none of which a deep knob can move. The
scorer knobs below are therefore held at their defaults with that
measurement as the reason, and §D names the two instruments that would
locate the residual T51–75 gap (≈ 0.4 foundings/seat after the stack's
+0.132): the `neg`/`starve` buckets of *successful* searches, and a
`deep-sitewall`-style counterfactual for the danger veto.

### site_enough_result
- **Unit (from inventory, re-verified at deepsettler.c:1158 and deepsite.c:2531):** tile-sum want units, compared with `cr->result` (post-travel-amortize, post-cost); above it phase 2 (overseas) is skipped and the pf scan may break early.
- **Current:** compiled 250.0 (= stock's `RESULT_IS_ENOUGH`); placeholder? y ("reproduces stock exactly"); live at defaults? y.
- **Anchor:** (b) 250 is stock's compile-time constant → baseline-identical by construction. (d) ledger §5: mean `best_net` of successful searches 357.8 (T51–75) / 387.9 (T1–25), so the skip fires on the typical find; empty searches score nothing, so the bar never refuses a site.
- **Derivation:** A lower bar would skip phase 2 more often (fewer overseas candidates), a higher one would run the overseas search after a good land find (cost: time, and `site_boat_cost_weight` makes overseas rarely win). Neither moves a land founding; baseline-matching holds at 250.
- **Proposed value:** 250.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** every term that moves `cr->result`.

### land_pressure_tiles
- **Unit (from inventory, re-verified at deepdefence.c:771-772):** land tiles per player (reference); `pressure = T/(T + land_per_player)`; sole consumers are the two `site_rival_crowding` expressions (deepsite.c:1271, 1283).
- **Current:** compiled 200; placeholder? n; live at defaults? y.
- **Anchor:** (c) venue arithmetic: land_per_player 38 (contested) / 80 (lategame-80) → pressure 0.84 / 0.71 → the rival penalty is multiplied by `1 − 0.9 × pressure` = **0.24 / 0.36**. The rival penalty itself is a scorer term and (ledger §5) the scorer is never reached by an empty search.
- **Derivation:** The knob already forgives 64–76% of the rival penalty on these venues; it decides ranking among scored sites, not whether a search finds one. No anchor for a different forgiveness curve → unchanged.
- **Proposed value:** 200 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `site_rival_pressure` (10), `site_rival_crowding` (0.9).

### site_own_overlap
- **Unit (from inventory, re-verified at deepsite.c:1305-1309):** tile-sum want per catchment tile already inside an own city's work radius, × `deep_stance_pox_mult` (1.0 at identity); subtracted from `wspecials`.
- **Current:** compiled 0.0 (off); placeholder? y (derived on-value 33.2/tile from a T110 Europe probe); live at defaults? n.
- **Anchor:** (c) theory §B.2: at `citymindist 3` with `init_city_radius_sq 5` the pattern that maximises worked tiles at size ≤ 6 is spacing 3–4; civ2civ3's anti-ICS terms are second-order at T150 ("Verdict: no"), and baseline's 5.3 cities of mean size 5.7 sit on the Temple gate, not the land. (a) any city founded by T75 is +6…+17.8 points regardless of overlap; overlap costs only the tiles both cities could work, which at size ≤ 6 is none.
- **Derivation:** Turning the charge on can only *reduce* acceptance (it is a subtraction on candidates that already reached the scorer), which is the wrong sign for the primary meter, and the score function does not penalise packing at this horizon. The Europe derivation (33.2/tile) was for a 20–30-city empire where `Empire_Size` unhappiness fires; it does not fire in either venue (§B.2). Stays off.
- **Proposed value:** 0.0 (unchanged, off).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `stance_pox_gain` (dead at identity).

### site_travel_discount
- **Unit (from inventory, re-verified at deepsite.c:2424-2426):** dimensionless multiplier on travel turns before `amortize()`; retained value `(23/24)^(d × turns)`.
- **Current:** compiled 0.9; placeholder? n (human call 2026-08-02: "a little below 1"); live at defaults? y.
- **Anchor:** (a)+(c): a city founded `t` turns later is worth less by the price list's slope — between the T26–50 and T51–75 bands the value falls from 13.0 to 6.0 over 25 turns, i.e. ≈ 3.1% per turn of founding delay; `amortize` at MORT 24 discounts (23/24) = 4.2% per turn, so d = 0.9 gives 3.8%/turn and d = 0.75 would give 3.1%/turn. Between T1–25 and T26–50 the slope is (17.8−13.0)/25 = 1.5%/turn.
- **Derivation:** The score-implied discount per turn of delay is 1.5–3.1%/turn depending on band; the shipped 0.9 (3.8%/turn) is at or slightly above the steep end. A move to 0.75 (3.1%/turn, the range floor) matches the T26–75 slope exactly and is the direction the human call named. But the discount only reorders candidates that reach the scorer and decides sign when `result − cost` is near zero; the ledger says the fork's empty searches never get that far. Gain bounded to the `neg` population of successful searches, which no corpus has sized. I register it as the first knob of a follow-up, not in this arm — the arm already moves the transit price on the same walk, and two walk prices in one arm are unreadable.
- **Proposed value:** 0.9 (unchanged in this arm; 0.75 is the score-anchored candidate for the next).
- **Expected effect:** none in this arm.
- **Falsification:** n/a.
- **Interactions / ordering:** `frontier_transit_price` (adds turns to the same walk), `site_settler_cost_weight` (subtracted after).

### site_potential_weight
- **Unit (from inventory, re-verified at deepsite.c:1104):** dimensionless multiplier on `MAX(irrigation food incr × food_priority, mine shield incr × shield_priority)` per catchment tile; then × `site_potential_discount` (0.5) × pressure factor.
- **Current:** compiled 1.5 ("stock's effective PERFECTION/2"); placeholder? y; live at defaults? y.
- **Anchor:** (b) stock-equivalent by the header's own derivation. No arm.
- **Derivation:** A ranking weight among scored tiles; no acceptance role (ledger). No anchor for a change → unchanged.
- **Proposed value:** 1.5 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `site_potential_discount`, `site_pressure_factor`, `site_improvable_gate` (0).

### site_settler_cost_weight
- **Unit (from inventory, re-verified at deepsite.c:2475-2476):** dimensionless multiplier on the settler's build cost in SHIELDS (30), subtracted from `cr->result` in tile-sum want units ("dimensionally awkward" by the header's own admission); a candidate with `result <= 0` is dropped (`neg`).
- **Current:** compiled 1.0 (stock's implicit 1); placeholder? y; live at defaults? y.
- **Anchor:** (a)+(d): the settler's real cost is 2 citizens for the parent's regrowth time (B.1: 10–15 turns → ~25 citizen-turns ≈ a 0.5–1 citizen deficit at the horizon ≈ 0.5–1 point) plus 30 shields (≈ one Temple ≈ 3–4 points, Chain 2) ≈ **4–5 points**; at 27.5 tile-sum per point (358 want ↔ 13 points) that is **110–140 tile-sum units**, against the 30 charged today.
- **Derivation:** The dimensionally honest weight is 110–140/30 ≈ **4–5**. Its effect is to drop sites whose net value is below ~4–5 points — i.e. it REDUCES acceptance, which is the wrong direction for the primary meter and the opposite of what this arm is for; and since (ledger) a found site averages 358 net, the drop would hit only the marginal 100–150 band whose size nobody has measured. Correct in principle, premature in fact: unchanged, and the `neg` census is named in §D as the measurement that decides it.
- **Proposed value:** 1.0 (unchanged; the derivation says 4–5 and the arm deliberately does not take it).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `site_boat_cost_weight` (0.333, same subtraction), `site_travel_discount`.

### site_claim_bonus
- **Unit (from inventory, re-verified at deepsite.c:1198):** tile-sum want per catchment tile owned by a non-allied player.
- **Current:** compiled 8.0; placeholder? n; live at defaults? y (only with foreign catchment tiles).
- **Anchor:** (c) frontier verdict §1: founding on a foreign tile is not permitted; a catchment tile owned by a rival is one the new city may never work until borders move. No measurement of `nclaim` exists.
- **Derivation:** The term pays for denying ground the city cannot use; the score function has no land term. Direction is down, magnitude unmeasured; a ranking term, not an acceptance one. Unchanged.
- **Proposed value:** 8.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `site_rival_pressure` (counterweight), `site_pressure_factor`.

### site_pressure_factor
- **Unit (from inventory, re-verified at deepsite.c:1105-1106):** dimensionless 0–1 multiplier on a tile's improvement potential when a non-allied city sits within `4 × city_radius_sq` of the site.
- **Current:** compiled 0.3; placeholder? n; live at defaults? y (pressured sites only).
- **Anchor:** none; the `pressured` predicate is not logged.
- **Derivation:** No anchor → unchanged.
- **Proposed value:** 0.3 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `site_potential_weight`, `site_potential_discount`.

## Chain 6 — the building auction and the gold bill

### upkeep_gold_weight
- **Unit (from inventory, re-verified at deephorizon.c:2394-2398 and daicity.c:1490, 1508-1511):** want per gold-turn; deep's horizon credit is debited `city_improvement_upkeep × dk_bdur × weight` where `dk_bdur = deep_decay_duration(deep_horizon_turns() = MORT = 24, …)` = 24 at defaults (decay gate off). STOCK prices the same gold inside `base_want()`: `prod[O_GOLD] −= city_improvement_upkeep(acity, upkept)` (daicity.c:1490) and `want += prod[O_GOLD] × adv->gold_priority` (:1508/:1511) with `adv->gold_priority = TRADE_WEIGHTING = 18` (advdata.c:518, advbuilding.h:21) — i.e. **18 want per gold per turn**, on the same per-turn footing as the output it is netted against.
- **Current:** compiled 0.125; placeholder? y ("owes a tuning pass"; default and range divided by MORT when the one-turn-bill bug was fixed); live at defaults? y — tier A (AUCTION), flips 93.2/game.
- **Anchor:** (c)+(d) internal consistency with stock's own conversion of the same quantity: stock charges a gold of upkeep at 18 want; deep's credit charges it at 0.125 × 24 = **3 want** — a 6× asymmetry between the gold a building earns (credited at 18 in `base_want`) and the gold it costs (debited at 3 against the horizon credit). (d) content-military-gold §: "3 × 24 × 0.125 = 9 want against a mean credit of 161" for an Amphitheater; deficit census: Colosseum/Amphitheater = 23% of all picks at T100–125 with riots ≈ 0.1/game. (c) theory §G: an Amphitheater's 3 gold over 100 turns = 300 gold = 150 shields at the 2-gold buy rate (`improvement.c:319`) = five Settlers. Panel: deep's treasury 39 vs baseline 275 at T100 and never in the 150–500 band.
- **Derivation:** The horizon credits are per-turn quantities (per_citizen × steps is the per-turn yield of the citizens that arrive within the horizon, `deephorizon.c:579`), the same footing as stock's `base_want`; so the gold that offsets them should be priced at the same 18 want per gold-turn stock uses. The debit multiplies by `dk_bdur = 24`, so the weight that yields 18 per gold-turn is **18 / 24 = 0.75**. Check against the C inventory's own example: a 3-gold building then loses 3 × 24 × 0.75 = 54 of a 161 credit (a third), a 1-gold Temple loses 18. Range `[0, 1.25]` admits it.
- **Proposed value:** 0.75 (up ×6).
- **Expected effect:** Amphitheater/Colosseum picks at T100–125 down from 23% toward baseline's share; gold at T100 up by 50–150 (into the panel's 150–500 band in a fraction of seats); pop/city at T150 ±0.2 (a Temple is still credited; the 3-gold content building is what loses); culture −0…−20 (Amphitheater History 2); score **+1…+3 at T150** on contested from the shields and gold redirected; econ venue (T81) smaller. Panel: gold band at T100, achievements unchanged.
- **Falsification:** riots or disorder turns per city rising at T100+ (deficit census: 0.1 riots/game today) with pop/city falling by > 1 SE — the content building was needed after all; or gold rising with no change in picks (the credit was not what bought them).
- **Interactions / ordering:** acts on the T100+ building auction, which fearseat deliberately left alone "to keep the military result readable"; this arm accepts that confound because its bisection halves (§C) separate the defence seam from the economy seam. `horizon_content_weight` (below) is the credit this bill is subtracted from.

### horizon_capacity_weight / horizon_growth_weight / horizon_bundle_weight / horizon_content_weight / horizon_multiplier_weight (the AUCTION five)
- **Unit (from inventory, re-verified at deephorizon.c:579, 772, 1014, 704, 935):** dimensionless multipliers on horizon credits ADDED to `building_want[]` (`deephorizon.c:2475-2476`), each in want units: capacity/growth/content = `per_citizen × (citizens unlocked/freed)` amortized by ttg, where `per_citizen = deep_unworked_tile_value(best 3 unworked tiles)` = food×30 + trade×(science/gold priority) + shield×17 per turn; multiplier = pct × arriving × raw yields × priorities amortized; bundle = ruleset effect value × viable tiles (a lump).
- **Current:** compiled 1.0 / 0.5 / 1.0 / 1.0 / 1.0; placeholder? y (the W03 "model as designed", growth 0.5 after the Granary inversion fix); live at defaults? y — tier A, flips 11 / 79 / 84 / 753 / 145 per game.
- **Anchor:** (c) how the credit meets the military want after the merge (the brief's open question): the merge is tier-first (`deep_merge_domestic_wins` → `deep_want_cmp`), so a credit of any size loses to an EMERGENCY military want and competes only with NORMAL ones (~100 for the second defender, 175–235 opening). A `per_citizen` on a 2/1/2 grassland = 2×30 + 2×18 + 1×17 = 113 want per citizen-turn; a Temple freeing one citizen at the size-4 gate therefore carries ~113 before amortize — the same order as the second defender's 100, and above it once `scale_defender` is 0.5. (a) per unit of score the credit is priced consistently with the founder and the Temple (Chain 2: 48–64 want/point). (e) GHORIZON block ablation vs pv3: −6.3 ± 21.9 (null); the only single-knob moves (growth 0→0.5, content 0→1.0) were mechanism-landed, score-null at n ≤ 6.
- **Derivation:** The weights are already in the currency the founder and the Temple share, and the merge arithmetic says they cannot buy a building over an emergency unit whatever their value. Their known defect (23% Amphitheaters) is the gold bill, which `upkeep_gold_weight` above fixes at the source rather than by discounting all content credits. No anchor to move any of the five → unchanged; `horizon_content_weight` is the second rung if the upkeep fix alone leaves the Amphitheater share high.
- **Proposed value:** 1.0 / 0.5 / 1.0 / 1.0 / 1.0 (all unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `upkeep_gold_weight` is subtracted from their sum; `horizon_culture_weight` stays 0 (measured −19.43 at 1.0 on isolated-150; see §B.4 for why the achievement lumps are a different mechanism).

### switch_cost_gain
- **Unit (from inventory, re-verified at deepdefence.c:7919-7921):** dimensionless multiplier on `forfeit shields × SHIELD_WEIGHTING (17)`, debited from every NORMAL-tier candidate that would change production class.
- **Current:** compiled 1.0; placeholder? y ("the hottest untuned knob in the tree"); live at defaults? y — tier A, flips 274.7/game.
- **Anchor:** (c) 17 want per shield is stock's own conversion (`advbuilding.h:20`), so 1.0 prices the forfeited shields at exactly the rate the auction prices shields everywhere else — consistency, the same anchor as `upkeep_gold_weight`. (d) HEDGE: at 1.0 the debit zeroes 86.8% of the candidates it touches (debit p50 136 vs candidate want p50 41); deficit census: "starts ≈ abandonments" was the settler-completion defect, and `switch_cost_price` on raised wonders 0.09 → 0.28/seat and cut class switches 629 → 378.
- **Derivation:** The debit is dimensionally honest at 1.0 and its effect (switch rarely) is the one the settler-completion finding wanted. A lower gain would re-admit the switches that abandon Settlers runs; a higher one is already saturated (87% zeroed). Unchanged.
- **Proposed value:** 1.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** applied after the scales (so `scale_defender = 0.5` makes a defender candidate more often fully zeroed by its switch debit — intended), before `mil_opportunity_price`.

## Chain 7 — exploration and the achievement lumps

### explore_discovery_value
- **Unit (from inventory, re-verified at deepexplore.c:243, 255):** want units per unknown reachable tile; BUT `unknown` is forced to 0 unless the player has `H_MAP` (deepexplore.c:243), which SKILL=hard does not set, so the term is structurally zero in every benchmark.
- **Current:** compiled 0.05; placeholder? y; live at defaults? n (multiplies a forced zero).
- **Anchor:** (a) `Map_Known` = 100% of tiles known = +1000 culture = **20 score** (theory §A.2, `data/civ2civ3/game.ruleset [achievement_*]`), credited to `pplayer->history`; panel §1: baseline books 0.17 achievements/seat by T100 vs deep 0.02, ≈ +3 points of the 6.85 gap. (c) the achievement reads the player's *known* map, not the AI's omniscient view, so even an omniscient AI must walk the tiles.
- **Derivation:** No value of this knob can make deep finish the map: the multiplicand is zero by code. The informed value, were the forced zero lifted (§B.4), is the lump divided by the tiles: a 32×32 map has 1024 tiles; if the explorer and the cities reveal ~half, ~500 unknown tiles stand between deep and 20 points → 0.04 point/tile ≈ 1.1–2.4 want/tile at 27.5–58 want/point — 20–50× the shipped 0.05. That number belongs to the mechanism, not to this arm.
- **Proposed value:** 0.05 (unchanged — inert; see §B.4).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** `explore_hut_value`, `explore_explorer_claim` (the claim subtraction applies to the whole prize).

### explore_explorer_claim
- **Unit (from inventory, re-verified at deepexplore.c:325-329):** huts' worth of prize per fielded/queued explorer (× `explore_hut_value` → want); `want = (prize − claimed) / (1 + explorers + queued)`.
- **Current:** compiled 1.0; placeholder? n; live at defaults? y iff `huts > 0` (server default 15).
- **Anchor:** (e) item-4 arm (claim shape + salvage + floor, bundled): +0.01 ± 0.34, 18/20 seeds bit-identical; peak explorers 12 → 1. (c) a hut yields 1/14 barbarians, 3/14 tech (= 2 points), 5/14 gold, 1/14 a city (`default.lua:146-167`) — expected ≈ 0.5–1 point per hut ≈ 15–60 want, consistent with `explore_hut_value = 15` at the low end.
- **Derivation:** The claim decides how many explorers chase the visible huts; it has no path to `Map_Known` while discovery is forced zero. Lowering it would field more explorers for the same huts (measured: the 12-explorer tail was the bug the claim fixed). Unchanged.
- **Proposed value:** 1.0 (unchanged).
- **Expected effect:** none.
- **Falsification:** n/a.
- **Interactions / ordering:** §B.4.

---

# Part 2 — the memo

## A. The currency problem, made concrete

**The comparison the build choice actually makes** (`deep_city_choose_build`,
deepdefence.c:8603–8840; C inventory "where the scaled bids meet"):

1. `deep_scale_domestic_bids()` multiplies `founder_want` and `worker_want`
   in place (skipped at 1.0), then `domestic_advisor_choose_build()` runs
   ai/default's auction: founder vs worker vs best `building_want[]`
   (which deep has already enlarged with the horizon credits), clamps the
   winner to `DAI_WANT_DOMESTIC_MAX = 750.075`, and the unscaled bids are
   restored.
2. The military choice (`city_data->choice`) is scaled by class
   (`deep_want_scale_apply`, NORMAL tier only), then debited by endgame,
   unit upkeep (off), switch cost, prod-keep (off) and finally the
   opportunity charge (`price × founder bid`, NOT tier-guarded).
3. `deep_merge_domestic_wins()` → `deep_want_cmp()` on `(tier, value)`:
   **tier first, then value**. A domestic want is ≤ 750.075 and always
   NORMAL. A military want ≥ 1000 is EMERGENCY and wins at any value.

**Each bid's native unit, with the magnitudes the project has logged:**

| bid | native quantity | typical magnitude (want) | where it is formed |
|---|---|---:|---|
| founder | site scorer tile-sum (weighted yields amortized by arrival turn), × (1 + rush) = ×2 on both venues, clamped 750.075 | 358 mean `best_net` for a found site; **~750 pinned** when a site exists | deepsite.c:2830 → deepdefence.c:9265 → daidomestic.c:546-634 |
| worker | ai/default terrain-improvement estimate | p90 69–72, max 150 | daicity.c:800-849 |
| building | stock `base_want` (Σ output deltas × priority, per turn) + deep horizon credits (per_citizen 113 on a 2/1/2 tile × citizens) − gold bill | p50 ≈ 192 (winning anchor) | daicity.c:1477-1511, deephorizon.c:2037-2476 |
| defender | `danger%` = 100 × danger²/our_def, cap 100 at urgency 0; EMERGENCY flat 100 for an empty city | NORMAL 100–235; EMERGENCY 1100+div | deepdefence.c:5718-5790 |
| attacker | kill-expectation | NORMAL < 1000 (unlogged per class); EMERGENCY p50 6485, max 259 976 | stock `kill_something_with` via deep |

**The exchange rate the score function implies** (price list, theory §A.2):

- a city founded T26–50 = **13.0 ± 0.6** score; at the pinned bid that is
  **58 want/point**, at the mean found site **27.5 want/point**.
- a Temple at T50: +1–2 citizens held at the size-4 gate (+1–2) + 1
  History/turn ≈ 2 culture-points by T150 → **3–4 points ↔ 192 want =
  48–64 want/point**. The founder and the building anchor are in one
  currency already, within the spread of both estimates — that is W01
  done empirically, and it is why `scale_settler` stays at 1.0.
- a defender: `unitsbuilt/10` = **0.1 point**; the loss it prevents, from
  the deep-vs-baseline cities-lost difference spread over the excess
  army, ≈ **0.8 point per marginal unit** (Chain 2); its cost is 20
  shields + ~1 gold/turn for life (civ2civ3 `Shield2Gold_Pct 100` above
  2–3 free, `effects.ruleset:745-751, 922-944`) ≈ 75–100 gold ≈ 37–50
  shields at the 2-gold buy rate ≈ 1.2–2.3 Settlers. **Net ≤ 0 at the
  margin.** Its NORMAL want of 100–235 prices it at 2–4 points — 3–10×
  its worth. The scale that puts the second defender in the founder's
  currency is **~0.1–0.2 by exchange, 0.5 by baseline-matching** the
  panel's units/city; this arm takes 0.5 and names 0.15 as the next
  rung.
- an attacker: a kill = **1/3 point ≈ 17 want**; S-OPXRATE's realised
  4.4% vs forecast 0.89 says city-assault bids are overstated ~20× — but
  the NORMAL-tier class mix is unlogged, so `scale_attacker` is left at
  1.0 with the measurement named in §D.
- a worker: +1 food on a worked tile for 100 turns ≈ 2–5 citizens ≈ 2–5
  points ↔ 70 want = 14–35 want/point — *under*-bid by the arithmetic, yet
  the only measured move up (×3) was harmful. Left alone (rule 6 beats
  the arithmetic).

**The emergency tier is out of the scales' reach.** `deep_want_scale_apply`
returns early on EMERGENCY; `emergency_value_cap` clamps the value inside
the tier and never the tier; a domestic want cannot reach 1000. The only
knob on this chain that crosses the boundary is `mil_opportunity_price`,
because its charge subtracts from the raw want and the tier is re-decoded
from the result: the first defender (1100 + div) loses to a founder bid `d`
iff `d > (1100 + div)/(1 + price)`. At the compiled 1.0 the pinned 750
already wins; at 2.0 bids down to 367 win. And the mechanism that creates
the EMERGENCY first-defender want in a city nobody threatens is the
garrison floor (danger 0 → 40 → empty-city flat 100): that is why the
arm's largest lever is `garrison_floor_danger = 0`, not a scale.

**Gold.** Stock prices a gold of building upkeep at `gold_priority = 18`
want per turn inside `base_want` (daicity.c:1490, 1508); deep's horizon
credit debits the same gold at 0.125 × 24 = 3. The consistent weight is
18/24 = **0.75** (`upkeep_gold_weight`). Gold itself scores nothing, but
it buys shields at 2 gold each and it is the 1-gold-per-unit bill that
pins deep's treasury at 39 against baseline's 275 at T100.

## B. Mechanisms, not knobs

Each item: what the code does today, the placeholder that would make it
adjudicable (name, unit, default = current behaviour), the informed first
value and its derivation, and the one-line test. None of these is in the
ini; they are the next epic's placeholders.

### B.1 The empty-city switch
**Today:** a city with `our_def == 0` and `urgency == 0` gets a FLAT 100
at the EMERGENCY tier for any `danger >= 1` (deepdefence.c:5733-5735,
:5783-5784), so the first defender beats any settler regardless of the
scales — the fear magnitudes are on/off switches there. The existing
`birth_guard_gain` (D inventory; 0.0, null prior) already replaces that
flat 100 with `p × worth × gain` at NORMAL tier (deepdefence.c:5777-5781).
**Placeholder:** `birth_guard_gain` is the knob; the mechanism gap is
that at 0 it is off and at any positive value the price is `P(loss) ×
city worth`, which has never been checked against the price list.
**Informed first value:** 1.0 — `worth` is `ai_city.worth` (stock's city
worth in want) and `p` is a probability, so 1.0 is the identity of a price
that is already in the auction's currency; the derivation that matters is
whether `p × worth` for a fresh size-1 city lands near the founder bid it
competes with (a size-1 city is worth 6–17 points ≈ 165–990 want; p for an
unthreatened city should be ≪ 1, so the priced first defender should
usually lose to a 750 founder — the behaviour baseline shows at 0.33
units/city).
**Test:** units/city at T50 (panel) and `deep-birthguard` want vs the
founder bid in the same city-turn; falsified if cities lost per seat
rises above baseline's 0.394.

### B.2 The settler inventory the cap and the guard hold from T50 on
**Today:** `deep_founder_join_guarded()` (deepsettler.c:1839-1866) declines
the join when any of three signals is inside `mult × rebuild` turns;
`s_blocked` fires on 99.6% of attempts, so the guard is "never join", and
both venues end with ~1.7 idle founders a seat (S-STACKED §4) — 3.4
citizens standing in the field. The cap (`deep_founder_site_capped`) does
not bind because `deep_remaining_sites()` reads ≈ 3.3 when 97% of
searches are empty (ledger §5).
**Placeholders:** (i) `founder_join_guard_signals` — bitmask {found=1,
cancel=2, blocked=4}, default 7 (= today); (ii) `founder_hold_release_
searches` — count of consecutive empty fresh searches by this player after
which the guard releases regardless of signals, default 0 (= never, today).
**Informed first values:** (i) 3 (found|cancel): the siteplan census
measured that the narrower rule declines 44.8% of attempts instead of
99.6% — a measured, not chosen, population; (ii) `rebuild` turns ≈ 11
(the guard's own window, mean 11.3 measured) — a founder that has searched
every turn for one settler-build-time and found nothing has outlived the
guard's own argument ("a site may open inside the time it would take to
build another one").
**Test:** live founders at T150 (panel) toward baseline's 2.4 → below it;
pop +2 per released founder; falsified if foundings in T101–150 fall
(today 0.275/seat) — the released founder was about to find a site.

### B.3 `density_prior_tiles`' double duty
**Today:** `deep_map_crowding() = MIN(1, 200/land_per_player)` feeds the
pre-contact fear prior, the founder bid (`× (1 + settler_rush_weight ×
crowding)`, deepdefence.c:9265-9268) and the choke site bonus
(deepsite.c:1322). On both venues `land_per_player` is 38 or 80, so
crowding ≡ 1.0 and `settler_rush_weight` is an unconditional ×2 on every
fresh founder bid — a hidden `scale_settler = 2.0` applied before the
750.075 clamp. The "~750 founder bid" calibration fact is the clamp.
**Placeholder:** `settler_rush_tiles` — land tiles per player at which the
race premium is half (its own reference, so a fear-prior tune cannot move
the settler bid), default 200 (= today's shared constant).
**Informed first value:** 200 (identity with today) — the mechanism change
is the separation; the value is then tunable on a venue where crowding is
not saturated (Europe, 100+ land tiles per player).
**Test:** log the post-rush `founder_want` (today unlogged, A1
inventory) and confirm the ×2 is constant on both venues; nothing to
adjudicate until a non-saturated venue is in the suite.

### B.4 The achievement lumps no mechanism knows about
**Today:** `Map_Known` (100% of tiles known) is +1000 culture = 20 score,
`Multicultural` (one conquered size ≥ 2 city) +20, `Literate` +20,
`Land_Ahoy` +2 (theory §A.2); baseline books 0.17 achievements/seat by
T100 vs deep 0.02 (panel §1, ≈ 3 points of the 6.85 gap). Deep's only
exploration want is huts: `explore_discovery_value` multiplies an
`unknown` count that deepexplore.c:243 forces to 0 without `H_MAP` (so an
omniscient AI does not scout for information it already has — correct for
information, wrong for the lump, which reads the player's *known* map).
**Placeholder:** `explore_mapknown_gate` — 0 (= today: `unknown` forced
0 without H_MAP), 1 = count unknown tiles regardless of the handicap and
value them at `explore_discovery_value`.
**Informed first value for `explore_discovery_value` under the gate:**
20 points ÷ unknown tiles; a 32×32 map with ~500 tiles still unknown at
T50 gives 0.04 point/tile ≈ **1.1–2.4 want/tile** (27.5–58 want/point) —
20–50× the shipped 0.05, and it rises as the count falls (the last tile
is worth the whole lump). A Trireme (40 shields) plus an Explorer (30)
is ~70 shields for 20 points, against 30 shields per 6–13-point city.
**Test:** achievements at T100 (panel) and the `culture` tag's 1000-step;
falsified if explorer shields rise with no `Map_Known` by T150 (the
coastline needs hulls the gate does not buy).
**Multicultural** is one conquest; `conquest_culture_gain` (E inventory,
0.0, null) exists and is off the expansion chain — noted, not proposed.

### B.5 A state-dependent grow posture
**Today:** `cm_grow_food/shield/trade` = 16/4/4 for every city below size
6 in peace, whether or not a legal site exists. The score arithmetic
(Chain 4) says shields ≥ food ≫ trade while a settler has somewhere to
go, and food ≫ shields once it does not.
**Placeholder:** `cm_grow_site_shield` — the shield weight the GROW
posture uses while `deep_remaining_sites() >= 1` and the city is size ≥ 3
(can build a Settlers), default 4 (= today).
**Informed first value:** 16 (equal to food) — the exchange rate puts a
shield at 0.17–0.43 point and a food at ~0.1 while a site exists, so
parity is the conservative end of the arithmetic's 2–4×.
**Test:** Settlers build time in `deep-prodswitch` and foundings in T26–75
(ledger); falsified if pop at T50 falls by more than the founding gain
buys (13 points per city vs 1 per citizen).

## C. The stack: informed-v1

One candidate arm. Control = compiled defaults + the five S-STACKED gates
(`benchmark/tuning/stacked-on.ini`). Every row below is a hypothesis; the
ini lists only the rows whose value changes, plus the five gates.

| knob | compiled | proposed | anchor | expected sign on cities |
|---|---:|---:|---|---|
| **Chain 1 — stack placeholders** | | | | |
| `mil_opportunity_price` | 1.0 | **2.0** | threshold `d > (1100+div)/(1+p)` at the call site; price list ≥ 6 pts/city vs defender ≈ 0; fearseat unitslost flat | + (small, T50) |
| `founder_site_cap_slack` | 1.0 | **0.0** | `sites` ≈ 3.3 when 97% of searches empty (ledger §5) → the estimator already carries the allowance | 0 (pop +) |
| `founder_join_guard_mult` | 1.0 | 1.0 | no anchor (signal fires 99.6%; §B.2) | 0 |
| `frontier_transit_price` | 1.0 | **0.0** | peace: NonMil may walk, war only after 16 turns (`diplhand.c:57`) vs 13.6-turn walk → zero exposure; walk already priced | + (small, T75) |
| `frontier_transit_stance_floor` | 0 | 0 | non-peace `off` tiles 1.6% of seen; cease-fire → war in 16 turns | 0 |
| `boat_shadow_distance` | 1.0 | 1.0 | fearseat: unitslost down at 1.0, nothing asks for more warning | 0 |
| `boat_shadow_count` | 1 | 1 | theory §E: the landing that happens is the one-hull walk-in | 0 |
| **Chain 2 — bid scales** | | | | |
| `scale_settler` | 1.0 | 1.0 | founder 27–58 want/pt vs Temple 48–64: one currency already; R3 0.2 cost cities | 0 |
| `scale_worker` | 1.0 | 1.0 | R3 ×3 harmful; arithmetic disagrees in sign → measurement wins | 0 |
| `scale_defender` | 1.0 | **0.5** | baseline-matching units/city (3.0 vs 2.1 military per city at T150); exchange says 0.1–0.2 (next rung) | + |
| `scale_attacker` | 1.0 | 1.0 | 20× assault miscalibration but NORMAL-tier class mix unlogged | 0 |
| `emergency_value_cap` | 0 | 0 | inert at the merge (tier-first); documented | 0 |
| **Chain 3 — fear seam** | | | | |
| `garrison_floor_danger` | 40 | **0** | prior arm: floor ON −1.09 cities (t −2.77) isolated-150, −0.66 (t −4.16) lategame-80, n=20 each; baseline 0.33 units/city at T50 | **++** |
| `density_fear_value` | 6 | 6 | fear reductions measured −2.4 on contested; one switch kept live | 0 |
| `density_fear_radius` | 8 | 8 | no anchor; three consumers | 0 |
| `density_prior_tiles` | 200 | 200 | crowding ≡ 1 on both venues; inert in range (§B.3) | 0 |
| `survival_target` | 0.5 | 0.5 | 35 consumers; score direction is UP | 0 |
| `survival_fear_value` | 20 | 20 | the one real-threat term; kept while two other defence levers move | 0 |
| `upkeep_unit_weight` | 0.5 | 0.5 | second multiplier on the same want as `scale_defender` | 0 |
| **Chain 4 — supply** | | | | |
| `founder_pool_per_site` | 0.06 | 0.06 | protects 0 founders after the int cast; superseded by the guard | 0 |
| `expansion_war_survival` | 0.9 | 0.9 | ERA n=8 directional for 0.9 over stock | 0 |
| `settler_rush_weight` | 1.0 | 1.0 | constant ×2 on both venues (= scale_settler 2.0 pre-clamp) | 0 |
| `settler_fearfulness_mult` | 2.0 | 2.0 | danger-pruned tiles never counted; n=5 probe direction unknown | 0 |
| `settler_risk_tolerance` | 1.0 | 1.0 | = stock bar | 0 |
| `cm_founder_food` | 0.0 | **2.0** | B.1: +2 food/turn regrows a citizen per 10 food-step; founder upkeep 1 → k=2 | + (small) |
| `cm_grow_food` | 16 | 16 | exchange says shields ≥ food while sites exist, but block move was negative (§B.5) | 0 |
| **Chain 5 — site acceptance** | | | | |
| `site_enough_result` | 250 | 250 | = stock; empty searches score 0 tiles | 0 |
| `land_pressure_tiles` | 200 | 200 | pressure 0.84/0.71 → rival penalty ×0.24/0.36; ranking only | 0 |
| `site_own_overlap` | 0.0 | 0.0 | subtraction = fewer cities; §B.2 says packing is free at T150 | 0 |
| `site_travel_discount` | 0.9 | 0.9 | score slope 1.5–3.1%/turn vs 3.8% at 0.9 → 0.75 is the next-arm candidate | 0 |
| `site_potential_weight` | 1.5 | 1.5 | stock-equivalent | 0 |
| `site_settler_cost_weight` | 1.0 | 1.0 | honest value 4–5 (110–140 tile-sum) REDUCES acceptance; `neg` census first | 0 |
| `site_claim_bonus` | 8.0 | 8.0 | no measurement of `nclaim` | 0 |
| `site_pressure_factor` | 0.3 | 0.3 | predicate unlogged | 0 |
| **Chain 6 — auction & gold** | | | | |
| `upkeep_gold_weight` | 0.125 | **0.75** | stock prices the same gold at `gold_priority` 18/turn (daicity.c:1490,1508); debit × dk_bdur 24 → 18/24 | + (via shields/gold) |
| `horizon_*_weight` (5) | 1/0.5/1/1/1 | unchanged | in the founder/Temple currency already; tier-first merge | 0 |
| `switch_cost_gain` | 1.0 | 1.0 | 17 want/shield is stock's conversion; 87% zeroed already | 0 |
| **Chain 7 — explore** | | | | |
| `explore_discovery_value` | 0.05 | 0.05 | multiplies a forced zero (§B.4) | 0 |
| `explore_explorer_claim` | 1.0 | 1.0 | huts only; no path to Map_Known | 0 |

**Seven values change.** Members that could disarm each other:
- `garrison_floor_danger = 0` removes the EMERGENCY first-defender want
  in unthreatened cities; `mil_opportunity_price = 2.0` is then live only
  where `density_fear_value` fires (after contact, neighbour within 8
  tiles). The two are complementary (floor → no want; price → founder
  beats the want); neither disarms the other, but the price's marginal
  effect shrinks once the floor is off.
- `scale_defender = 0.5` and `upkeep_gold_weight = 0.75` both free shields
  at T50–150 and both raise gold; their city effects add only where a
  site exists (the cap holds otherwise) — expect the surplus to show as
  gold and buildings, not cities, after T100.
- `founder_site_cap_slack = 0` and `cm_founder_food = 2.0` push opposite
  ways on the founder count (fewer carried late, more built early);
  the ledger's `sites` column decides which binds.
- `frontier_transit_price = 0` and `settler_fearfulness_mult` (unchanged)
  price the same path; the fearfulness term still charges real danger.

**Pre-registration (contested, n = 100 training seeds, vs the gates-on
compiled control; panel §2 values are the control's):**
- cities/seat at T150: **+0.25 … +0.70** (control 4.76 → 5.0–5.45; the
  remaining gap to baseline on holdout is −0.47). A delta inside ±0.10
  at T150 with units/city unmoved means the floor did not fire the way
  the 2026-08 arm said it did; inside ±0.10 with units/city down means
  the shields went to buildings (the cap held) — read `deep-sitecap`.
- cities/seat at T50: +0.05 … +0.25 (control 3.29).
- unitslost@75: **flat or down** (fewer units standing). Falsification:
  up by > 1 SE in the war venue, or cities lost per seat per game above
  baseline's 0.394 (deep 0.131 today) — the corner is void and the
  bisection says which half.
- score/seat at T150: +2 … +6 (control 85.0).
- panel: units/city at T50 **1.00 → 0.5–0.7**; founders at T100 **2.21
  → 1.8–2.2**; gold at T100 **39 → 100–200** (into the 150–500 band in
  ≥ 20% of seats, from 0–2%); pop/city at T50 2.54 → 2.4–2.6 (the
  `cm_founder_food` risk); achievements unchanged (no mechanism in this
  arm).
- lategame-80 (T81): cities +0.15 … +0.40 (control's stack gain was
  +0.60); score +1 … +3.

**Bisection halves to run first** (each vs the same control):
- **H-DEF (the defence seam):** `garrison_floor_danger = 0`,
  `scale_defender = 0.5`, `mil_opportunity_price = 2.0`. Reads on
  units/city at T50 and cities at T50/T100. If the corner fails on
  unitslost or cities-lost, this is the half to suspect; its own next
  rung is `garrison_floor_turn = 120` (floor off until `onsetbarbs`)
  in place of `= 0`.
- **H-ECON (supply and bill):** `upkeep_gold_weight = 0.75`,
  `founder_site_cap_slack = 0`, `frontier_transit_price = 0`,
  `cm_founder_food = 2.0`. Reads on gold at T100, founders at T100/T150,
  cities at T75. If the corner clears, this half's next rungs are
  `site_travel_discount = 0.75` and `scale_defender = 0.15`.

## D. What was NOT changed, and why — with the measurement that would supply an anchor

| knob(s) | why unchanged | the measurement that would anchor it |
|---|---|---|
| `scale_attacker` | the 20× assault miscalibration (S-OPXRATE) applies to an unknown fraction of NORMAL-tier attacker bids | per-class NORMAL-tier distribution from `deep-bid milclass=2` split by target (city vs unit) on the s6 corpus — zero games |
| `scale_settler`, `scale_worker` | founder and Temple already convert at one rate; the worker's only move was harmful | the worker's realised yield gain per unit built (irrigation/mine tiles × turns worked), to replace the "best unworked tile" assumption |
| `founder_join_guard_mult` | the blocked signal fires 99.6%; the window does not bind | re-run the siteplan census with the stack on (does `s_blocked` still fire with peace borders transited?) — zero games, `deep-joinguard` is already logged |
| `site_settler_cost_weight`, `site_travel_discount`, the rest of the scorer set | empty searches score zero tiles (ledger §5); these knobs only rank what is already found | the `neg` and `starve` buckets of SUCCESSFUL searches by band (how many candidates did the cost subtraction or the travel amortize push below zero) — `deep-siteladder neg=`, zero games |
| `settler_fearfulness_mult`, `settler_risk_tolerance` | danger-pruned tiles never reach `seen`; no count exists | a `deep-sitewall`-style counterfactual pf with the veto/risk off on empty searches (the ownership version exists; the danger version does not) — code, then zero-game census on a 3-seed probe |
| `density_fear_value`, `density_fear_radius` | fear reductions measured −2.4 on contested; one empty-city switch kept live while the floor goes off | the floor-off arm itself: with the floor at 0, `deep-density` becomes the only first-defender trigger and its share is readable from `deep-fear site=build` |
| `survival_target`, `survival_fear_value` | the one observed-threat term; score direction is up, not down | cities lost by cause with `deep-surv p=` at the turn of loss — `cityledger.py --why` plus the survival log, zero games |
| `upkeep_unit_weight` | second multiplier on the same want as `scale_defender` | none needed — fold into the `scale_defender` rung |
| `horizon_*_weight` (5) | one currency with the founder already; tier-first merge | Amphitheater share at T100–125 after `upkeep_gold_weight = 0.75` — if still ≫ baseline's, `horizon_content_weight` is next |
| `switch_cost_gain` | 17 want/shield is stock's conversion; 87% of touched candidates already zeroed | Settlers-run completion rate (deficit census 24.8%) in this arm |
| `founder_pool_per_site`, `expansion_war_survival`, `settler_rush_weight`, `density_prior_tiles`, `land_pressure_tiles`, `site_own_overlap`, `site_claim_bonus`, `site_pressure_factor`, `site_potential_weight`, `site_enough_result`, `boat_shadow_*`, `frontier_transit_stance_floor`, `emergency_value_cap`, `cm_grow_food` | derivation in the per-knob section: either baseline-identical by construction, inert on these venues, or the anchor points the wrong way for the primary meter | named per knob above; `cm_grow_food` wants the §B.5 mechanism, not a value |
| `explore_discovery_value`, `explore_explorer_claim` | multiply a forced zero / huts only | §B.4's gate; no knob reaches `Map_Known` |
| `garrison_floor_turn` | alternative to `garrison_floor_danger = 0` (floor off until `onsetbarbs` 120) | the H-DEF bisection: if cities-lost concentrate after T120 with the floor off, run `= 120` |

Knobs the brief listed that I dropped from the "changed" set with a
one-line reason: `density_fear_value` (one switch must stay live when the
floor goes off), `survival_*` (observed threats, not anticipatory), the
whole Chain 5 scorer set (ledger: `scored = 0.00` in empty searches), the
AUCTION horizon weights (the gold bill is the defect, not the credit),
`explore_*` (structurally zero). Knobs added to the list: `founder_floor_
gate` (explicitly held at 0 in the ini because the control holds it at 0
and it is the one gate whose measured effect was a hoard).

## E. Known weaknesses

1. **Observational anchors.** The `scale_defender` derivation's "0.8 points
   of protection per marginal unit" divides a *cross-arm* difference
   (deep vs baseline cities lost) by a *cross-arm* unit difference; both
   AIs differ in a hundred other ways. It gives a direction and an order
   of magnitude, not a coefficient. The baseline-matching value (0.5) is
   what the arm carries for that reason.
2. **The floor's prior arm is old.** The −1.09/−0.66 cities result is from
   2026-08 (pinned `536df20ee44c`), against a gates-off deep on a tree
   that has since gained the stack, the opportunity charge and the fear
   census. The direction is anchored by baseline's 0.33 units/city as
   well, but the magnitude in this arm is a range for that reason.
3. **Inferred units.** `upkeep_gold_weight`'s 18/24 assumes the horizon
   credits are per-turn quantities on the same footing as stock's
   `base_want`; `deephorizon.c:579` (`per_citizen × steps`, amortized by
   ttg) reads that way, but `steps = MORT/ttg` is a count of citizens
   arriving, and if a reader shows the credit is a *sum over turns* the
   consistent weight is 18 (above the range) and 0.75 is a 24× under-
   charge rather than exact. Stage 3 should check daicity.c:1508 and
   deephorizon.c:579 side by side.
4. **The opportunity-price threshold** uses `div` (the deterrence
   dividend) as a small additive term; if `div` is routinely ≫ 100 the
   band 2.0 opens is narrower than stated. `deep-milopp mil=` on the
   fearseat corpus would say.
5. **`cm_founder_food` has no prior A/B at all** (liveness only). It is in
   the arm because its anchor is pure ruleset arithmetic, but it is the
   value most likely to cost pop/city at T50, and the bisection half it
   sits in (H-ECON) will show that.
6. **civ2civ3 vs classic.** Every value in the ini is civ2civ3 (all
   benchmark venues). Where classic changes the number: `cm_founder_food`
   — classic Settlers cost 1 pop, 1 shield + 1 food, granary 20+10·n with
   no free step, so the +2 floor regrows a citizen in 10–20 turns, not 10;
   the B.1 arithmetic there gives k ≈ 1.5–2 and the value stands.
   `upkeep_gold_weight` — classic has no `Shield2Gold`, unit upkeep is
   shields, but *building* gold upkeep is the same mechanism and the
   `gold_priority` anchor is ruleset-invariant: 0.75 stands. `scale_
   defender`/`garrison_floor_danger` — classic's unwalled city is soft
   (.94 to a Catapult, theory §E) and barbarians arrive at T60 not T120:
   the floor-off risk is real there and `garrison_floor_turn = 60` is the
   classic analogue, untested. `frontier_transit_price` — the 16-turn
   pact lag is server code, not ruleset: stands. `mil_opportunity_price`
   — the 750 clamp and the 1100 first-defender are ai/default constants:
   stands. `founder_site_cap_slack` — `citymindist 2` in classic makes
   `deep_remaining_sites()` larger and the slack less relevant: stands.
7. **The site-acceptance reorder (Chain 5) rests on one corpus's ladder**
   (`siteplan/war/c00`, 60 games, with s6-nogates-war reproducing it
   within 2 points). If a stack-on corpus shows `scored > 0` in empty
   searches, the scorer knobs come back into play and `site_settler_
   cost_weight`'s honest 4–5 becomes the first thing to test — in the
   direction of fewer, better cities, which the price list says is wrong
   at T150 but which the `neg` census would have to adjudicate.
8. **Nothing here has been run.** Every expected-effect line is a
   pre-registration, and the numbers in §C are ranges chosen to be
   falsifiable at n = 100 on the city endpoint (SE ≈ 0.04–0.06), not
   predictions with error bars of their own.

## F. Classic variant (`benchmark/tuning/informed-v1-classic.ini`)

For the standing human-vs-AI game (`play-europe.sh`: `--ruleset classic`,
6 deep vs 6 stock classic + 1 human, techlevel 20, 300 turns). **A
legibility game, not a measurement**: no venue runs classic, nothing
below has an A/B, nothing becomes a default. Same seven knobs; one line
per knob that changes against the civ2civ3 vector, with the §B′/C′ fact
that changes it. Knobs whose anchor is ruleset-invariant keep their value
and say so in the ini comment (`mil_opportunity_price` — ai/default
constants; `upkeep_gold_weight` — advisor `gold_priority`;
`frontier_transit_price` — server pact lag and movement code).

- **`garrison_floor_danger` 0 → stays 40, and `garrison_floor_turn` 0 → 60.**
  Classic `onsetbarbs` is T60 (civ2civ3 T120) and an unwalled classic city
  is .94 to a Catapult (theory §E; Walls are 60 shields, +200%). The floor's
  measured cost is the opening shields; the threat the ruleset schedules
  arrives at T60, so the classic analogue of "off" is "off until the
  barbarians". Caveat: techlevel 20 fields Catapults from T1, so the
  stock classic seats can punish an empty city before T60 — the human
  will see whether they do.
- **`cm_founder_food` 2.0 → 1.5.** Classic Settlers cost 1 pop (not 2),
  the granary is 20 + 10·(size−1) with no free step (30–40 food to regrow
  the citizen), and the founder's food upkeep is 1 under Despotism/
  Monarchy but 2 under Republic/Democracy (`Upkeep_Pct`). k = 1.5 floors
  the surplus at ceil(1.5) = +2 under Despotism (B.1's rate, 15–20 turns
  to regrow one citizen) and ceil(3.0) = +3 under Republic; k = 2.0 would
  demand +4 there and fight rapture, which grows a celebrating Republic
  city on +1 food.
- **`scale_defender` 0.5 → kept.** The anchor (baseline-matching
  units/city) is a civ2civ3 panel reading with no classic counterpart;
  the exchange-rate sign (marginal defender net ≤ 0) is invariant, and
  classic's shield upkeep with 3 free makes the cost side smaller, so the
  classic value is if anything higher. Kept for comparability; flagged.
- **`founder_site_cap_slack` 0.0 → kept.** The estimator-bias anchor is a
  civ2civ3 ledger reading; `citymindist 2` makes `deep_remaining_sites()`
  larger in classic, so the cap binds less and the slack matters less —
  same direction.
- **No classic counterpart at all:** anything anchored to culture or the
  achievement lumps (`Map_Known`, `Multicultural`, `horizon_culture_
  weight`, §B.4) — classic has no `History` effects and no achievements,
  so its culture term is identically zero. None of those was in the ini.
- **Not re-derived, but worth watching in the game:** `cm_grow_*` has no
  rapture branch (theory §G: "correct on civ2civ3, silently wrong on
  classic"), and Despotism's 37% + 4%/tile trade corruption makes distant
  classic cities worth less per turn than the civ2civ3 price list says —
  the `site_travel_discount` 0.9 (3.8%/turn) is closer to right there.


## G. Post-audit resolution (2026-10-28)

The blind audit (`informed-values-audit-2026-10-28.md`) returned four
CONFIRMED (`garrison_floor_danger` 0, `mil_opportunity_price` 2.0,
`upkeep_gold_weight` 0.75, `founder_site_cap_slack` 0.0), one
UNVERIFIABLE kept as a labelled guess (`scale_defender` 0.5), one
CORRECTED (`cm_founder_food` 2.0 -> **1.0**: the knob is released the
turn surplus meets upkeep, so it cannot hold a regrowth surplus; k = 1
clears the veto at deepdefence.c:8298), and one WRONG and dropped
(`frontier_transit_price` 0.0: cancelling peace is immediate war,
common/player.c:79; the 16-turn figure is the ceasefire countdown; and
the price is a pathfinding extra cost that never reaches `turns` in the
site valuation, deepsite.c:2414). **The arm is six knobs.** Both ini
files are amended in place with `AUDIT` comments; the per-knob sections
above are left as written, as the record of the hypothesis.

Two audit findings that change how the arm should be read: (1) ~80% of
the pre-registered city range is the floor term, whose anchor arm ran
before the opportunity charge existed and never on contested, so a
contested result of +0.10..+0.25 cities is not a falsification;
(2) `upkeep_gold_weight` 0.75 stacks on stock's own 18-want charge for
the buildings stock fully prices (Library, Marketplace, Granary,
Courthouse, Harbour), giving 36 per gold-turn there and 18 for the
buildings stock only estimates (Temple, Amphitheater) -- the falsifier
should watch Library/Marketplace picks, not only Amphitheaters.
Bisection halves: H-DEF unchanged; H-ECON = {upkeep 0.75, slack 0.0,
cm_founder_food 1.0}.
