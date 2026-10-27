# Knob inventory: slice C, production and economy (46 knobs)

Read-only factual inventory, compiled-default behaviour. Sources: `ai/deep/deeptuning.h` (declaration + header comment), `benchmark/tuning-params.ini` (default/min/max/kind), `ai/deep/*.c` (call sites, line numbers at HEAD `b42d64d7a8`), `benchmark/tuning-blocks.ini` (tiers), `doc/deep-ai/findings/knob-screen-2026-08-30.md` and `screen-corpus-2026-08-30.md` (screen), `doc/deep-ai/STATUS.md` and `findings/*.md` (prior evidence). Compiled defaults in `deeptuning.c` equal the params.ini defaults for all 46 knobs (checked).

Conventions: "live at defaults" = the expression executes and can change a decision with every knob at its compiled default. "pv3.ini" etc. are archived campaign vectors (~80 knobs moved at once; the tune-postfix holdout rejected such a vector) and are cited only to show a value was campaign-moved, never as an A/B. STATUS.md line numbers are approximate to a few lines.

Key fixed constants used below: MORT = 24 (server/advisors/advtools.h:19); SHIELD_WEIGHTING = 17 (server/advisors/advbuilding.h:20); DAI_WANT_BELOW_MIL_EMERGENCY = 1000.0; DAI_WANT_DOMESTIC_MAX = 750.075 (ai/default/daitools.h:31-33).

## Summary table

| knob | unit | default | live at defaults? | tier | screen verdict | prior A/B | # call sites |
|---|---|---|---|---|---|---|---|
| caravan_discount | multiplier (per-turn retention) | 0.95 | yes | C | not screened | none | 5 |
| caravan_foreign_trade | enum/sentinel (0-3) | 3 | yes | C | not screened | none | 3 |
| wrace_margin_turns | turns | 10 | partial | C | not screened | none | 1 |
| wrace_pwin_gain | multiplier (blend 0-1) | 0.0 | no | C | not screened | none | 1 |
| wrace_eta_gain | multiplier (blend 0-1) | 0.0 | no | C | not screened | none | 1 |
| wrace_obsolete_gain | multiplier (blend 0-1) | 0.0 | no | C | not screened | none | 1 |
| wrace_urgency_gain | multiplier | 0.0 | no | C | not screened | none | 1 |
| expiry_building_gain | multiplier (blend 0-1) | 0.0 | no | C | inert | none | 3 |
| expiry_unit_gain | multiplier (deflator 0-1) | 0.0 | no | C | not screened | none | 2 |
| expiry_unit_gap | turns | 2 | no | C | not screened | none | 2 |
| sell_defend_gain | multiplier (gold/gold bar) | 0.0 | no | C | not screened | null | 2 |
| late_buy_gate | boolean gate | 0 | no | C | not screened | null | 1 |
| evacuate_gate | boolean gate | 0 | no | C | not screened | none | 1 |
| upkeep_gold_weight | want units per gold-turn | 0.125 | yes | A (AUCTION) | flips 93.2/game | null | 3 |
| upkeep_unit_weight | multiplier (per upkeep point) | 0.5 | yes | C | not screened | none | 2 |
| unit_upkeep_charge | multiplier | 0.0 | no | C | dead-path | null | 2 |
| rates_enabled | boolean gate | 0 | no | C | dead-path | negative | 1 |
| rates_science_gain | multiplier | 1.0 | no | C | dead-path | none | 2 |
| rates_luxury_gain | multiplier | 1.0 | no | C | dead-path | none | 2 |
| gov_valuation | boolean gate | 0 | no | C | dead-path | null | 1 |
| gov_anarchy_weight | multiplier | 1.0 | no | C | dead-path | none | 1 |
| gov_rate_ceiling_gain | multiplier (UNSURE calibration) | 0.0 | no | C | dead-path | none | 2 |
| trade_merit | boolean gate | 0 | no | C | dead-path (merit family; membership inferred) | null | 3 |
| trade_merit_gain | multiplier | 1.0 | no | C | dead-path (merit family; membership inferred) | null | 1 |
| trade_ceasefire_peace_odds | probability 0-1 | 0.0 | no | C | dead-path (merit family; membership inferred) | none | 2 |
| trade_route_risk_gain | multiplier | 0.0 | no | C | dead-path (merit family; membership inferred) | none | 1 |
| trade_route_love | count (love points/route/turn) | 5.0 | yes | C | not screened | null | 2 |
| trade_partner_target_discount | multiplier (dead in code) | 0.0 | no | C | not screened | none | 1 |
| wonder_merit | boolean gate | 0 | no | C | dead-path (merit family; membership inferred) | null | 1 |
| wonder_merit_gain | multiplier | 1.0 | no | C | dead-path (merit family; membership inferred) | null | 1 |
| pay_switch_penalty | boolean gate | 1 | yes | C | not screened | negative | 2 |
| switch_cost_price | boolean gate | 1 | yes | C | not screened | positive | 2 |
| switch_cost_gain | multiplier (x17 want/shield) | 1.0 | yes | A (AUCTION) | flips 274.7/game | none | 1 |
| scale_settler | multiplier on founder want (site-score units) | 1.0 | no | A (AUCTION) | flips 114.2/game | null | 2 |
| scale_worker | multiplier on worker want (terrain-improvement units) | 1.0 | no | A (AUCTION) | flips 51.6/game | null | 2 |
| scale_defender | multiplier on defender want (danger-percentage units) | 1.0 | no | A (DEFOPS) | flips 42.8/game | none | 2 |
| scale_attacker | multiplier on attacker want (kill-expectation units) | 1.0 | no | A (DEFOPS) | flips 112.9/game | none | 2 |
| scale_ferry | multiplier on ferry want (mixed units; UNSURE) | 1.0 | no | A (DEFOPS) | flips 16.2/game | none | 2 |
| emergency_value_cap | want units (absolute cap) | 0.0 | no | C pinned | flips 51.3/game but inert at merge | null | 2 |
| endgame_horizon | boolean gate | 0 | no | C | dead-path | null | 2 |
| endgame_window | turns | 0 | no | C | dead-path | null | 1 |
| endgame_unfinished_gain | multiplier (deflator 0-1) | 1.0 | no | C | dead-path | null | 2 |
| endgame_wonder_gain | multiplier (premium >= 1) | 1.0 | no | C | dead-path | null | 1 |
| buy_per_gold | boolean gate | 0 | no | C | not screened | null | 1 |
| prod_keep_gate | boolean gate | 0 | no | C | dead-path | null | 1 |
| prod_keep_gain | multiplier | 1.0 | no | C | dead-path | null | 3 |

Column notes: "live at defaults" `partial` = executes but only feeds a log/census at defaults. `# call sites` counts executable reads listed under "All call sites" (log-echo reads included, comment-only mentions excluded). Screen verdicts "dead-path" are those the screen doc names explicitly (knob-screen-2026-08-30.md:135-145); "(merit family; membership inferred)" means the doc says "the trade and wonder merit pair (6 knobs)" without listing them.

## The scale_* question (RC4): where the scaled bids meet

The build for one city is chosen in `deep_city_choose_build()` (deepdefence.c:8603). Three comparisons matter, in this order:

1. **Inside `domestic_advisor_choose_build()`** (ai/default/daidomestic.c:~541-600): the founder bid (`city_data->founder_want`, after /5 or /2 wonder-city, /100 cap, x expansionist trait / TRAIT_DEFAULT, food veto) is compared as a raw number against the best building want (`choice->want`, from `building_want[]`, a discounted output sum that deep has already post-adjusted with its horizon credits), and the worker bid (`worker_want * expand/100`) likewise. `scale_settler` and `scale_worker` multiply `founder_want` / `worker_want` in place BEFORE this call (deepdefence.c:8356, 8367 in `deep_scale_domestic_bids()`, called at :8709) and the unscaled values are restored after it returns (:8748-8749). The advisor clamps its winner to DAI_WANT_DOMESTIC_MAX = 750.075 (daidomestic.c:~631-634).
2. **The domestic-vs-military merge**: `deep_merge_domestic_wins()` (deepdefence.c:8507) calls `deep_want_cmp()` (deepwant.c) on `(tier, value)` pairs, TIER FIRST, then value. A domestic want is always NORMAL tier (<= 750.075 < 1000); a military want >= 1000 is EMERGENCY tier and wins regardless of value. The military scales (`scale_defender/attacker/ferry`) act on `city_data->choice` (the military choice) via `deep_want_scale_apply()` (deepwant.c:235, called at deepdefence.c:8792) immediately before this comparison is re-evaluated, NORMAL tier only. `emergency_value_cap` clamps the VALUE inside the emergency tier (deepwant.c:209-217) and never moves the tier.
3. **Absolute debits applied after the scale** (same function, order of calls): endgame unit deflate (:8799), `unit_upkeep_charge` (:8807), `switch_cost` (:8811), `prod_keep` credit (:8820), `mil_opportunity_price` charge (:8828; outside this slice) -- each acts on the already-scaled bids, and `switch_cost` / `unit_upkeep_charge` subtract absolute shield-equivalents (x SHIELD_WEIGHTING = 17), so a scale also changes how far a fixed debit goes.

Native units before scaling, as measured by the project: founder bid ~600-800 at T150 (findings/defaults-deficit-census-2026-08-25.md:32) and ~750 in the opening against a military bid of ~175-235 (fearseat-verdict-2026-08-30.md:253; fearsink-verdict-2026-08-30.md:298); the largest winning Settlers want in a probe game is exactly 750.1 (comment above `deep_founder_site_capped`, deepdefence.c); worker_want p90 69-72, max 148-150 (STATUS.md R3 entry); winning building anchor p50 ~192 (STATUS.md R2b, 191.8-192.0) and caravan want p50 ~88-197; emergency-tier honest wants: first defender p50 ~100, defense building up to ~398, defensive diplomat ~300, kill_something_with attacker p50 6485 up to 259976, attacker ferry ~2705 (deepwant.c comment in `deep_want_scale_apply`). No logged normal-tier magnitude for defender/attacker/ferry classes separately was found beyond the ~175-235 opening figure.

The three military keys have no unit conversion that the project derived: R3 concluded "scale_defender/attacker/ferry: NO FOLD, no derivation; ratios invert" (STATUS.md:8396-8398) and none was ever moved alone in a scored arm (R3 arms were S=scale_settler 0.20, W=scale_worker 3.0, CAP=emergency_value_cap 400, ALL=S+W+CAP).

## Per-knob entries

### caravan_discount
- **Declared:** deeptuning.h:497 (`double caravan_discount;`); params.ini: default=0.95 min=0.80 max=0.99 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the per-turn patience fed to caravan.c's NPV model; stock's 0.95 is kept as the placeholder". Stock switches it off for cross-continent routes; "Deep always counts transit and expresses patience as this one number."
- **Unit:** multiplier (dimensionless): per-turn retention factor d in caravan.c's d^turns NPV; the exponent is a turn-count ETA, so the knob itself carries no units. Range 0.80-0.99.
- **Formula at the primary call site:** deeptrade.c:247 deep_choose_trade_targets(): `parameter.discount = tune->caravan_discount;` (consumed by caravan_find_best_destination(), which discounts route value by discount^arrival_time). Merit-path form: deeptrade.c:1218 deep_trade_merit_want(): `double discount = tune->caravan_discount;` passed to deep_route_price().
- **All call sites:**
  - deeptrade.c:247 -- deep_choose_trade_targets -- sets the NPV patience for every live caravan's destination search -- live at compiled defaults? yes
  - deeptrade.c:296 -- deep_choose_trade_targets -- echoed on the deep-trade summary line (read only) -- live at compiled defaults? yes
  - deeptrade.c:1115 -- deep_trade_pair_value -- route face value for trunk-corridor pricing; only caller is deeptrunk.c:985 -- live at compiled defaults? no: only under trunk_trade_gain>0 (default 0.0)
  - deeptrade.c:1218 -- deep_trade_merit_want -- discount for the merit caravan bid -- live at compiled defaults? no: dead behind trade_merit=0
  - deeptrunk.c:903 -- deep_trunk_link_dividend -- trade dividend of a road link; called at deeptrunk.c:1253 only inside `if (tune->trunk_trade_gain > 0.0)` -- live at compiled defaults? no: dead behind trunk_trade_gain=0
- **Log line:** deep-trade (deeptrade.c:293: `chosen=%d reach=%d discount=%.3f`)
- **Interacts with:** caravan_foreign_trade, trade_merit, trade_merit_gain, trunk_trade_gain (outside this slice)
- **Tier/screen:** Tier C (in no tier A/B block; appears only in legacy G/GSITE/ECON/ALL blocks); screen: not screened individually (knobcensus MAP: S4 mag, direct tag deep-trade has:discount)
- **Prior evidence:**
  - findings/tuning-first-principles-2026-08-06.md:47 -- derivation, not an A/B: "1 - 1/MORT" = 0.9583; evolved campaign value 0.9634.
  - Campaign-moved only: a6-best.ini 0.9634, pv3.ini/decaytech-best.ini/derby-lean.ini 0.9458 (part of ~80-knob vectors; the tune-postfix holdout rejected the vector, findings/tune-postfix-verdict-2026-08-25.md). No isolated arm.

### caravan_foreign_trade
- **Declared:** deeptuning.h:498 (`int caravan_foreign_trade;`); params.ini: default=3 min=0 max=3 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "how far afield to look, as a foreign_trade_limit: 0 national, 1 allied, 2 peaceful, 3+ non-war. Stock re-rolls fc_rand(2) between allied and non-war per unit per call".
- **Unit:** enum/sentinel: 0 FTL_NATIONAL_ONLY, 1 FTL_ALLIED, 2 FTL_PEACEFUL, 3+ FTL_NONWAR (switch in deep_caravan_reach()); not a magnitude. knobcensus MAP: "an ENUM selecting foreign_trade_limit... four different candidate sets".
- **Formula at the primary call site:** deeptrade.c:123 deep_caravan_reach(): `switch (deep_tuning_get()->caravan_foreign_trade) { case 0: return FTL_NATIONAL_ONLY; case 1: FTL_ALLIED; case 2: FTL_PEACEFUL; default: FTL_NONWAR }`; result assigned at :249 `parameter.allow_foreign_trade = deep_caravan_reach();`
- **All call sites:**
  - deeptrade.c:123 -- deep_caravan_reach -- selects the candidate-destination class (only direct read of the knob) -- live at compiled defaults? yes
  - deeptrade.c:249 -- deep_choose_trade_targets -- caravan.c foreign-trade limit for the live dispatch search -- live at compiled defaults? yes
  - deeptrade.c:1028 -- deep_route_reach_allows -- reach filter for merit candidates (:1257), offshore census (:949) and pair value (:1120) -- live at compiled defaults? no: all three callers sit in the merit path (trade_merit=0) or trunk pricing (trunk_trade_gain=0)
- **Log line:** deep-trade (reach=%d echoed at deeptrade.c:293)
- **Interacts with:** caravan_discount, trade_merit
- **Tier/screen:** Tier C (legacy G/GSITE blocks only); screen: not screened (knobcensus: S4 "mode", pin per pass, do not hand to optimiser as continuous)
- **Prior evidence:**
  - none as an isolated arm. pv3.ini/decaytech-best.ini/derby-lean.ini carry 2 (campaign-evolved vs default 3).

### wrace_margin_turns
- **Declared:** deeptuning.h:749 (`int wrace_margin_turns;`); params.ini: default=10 min=2 max=40 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the SCALE of the P(win) ramp, not a threshold: P = 0.5 + 0.5 x (rival ETA - our ETA) / margin, clipped... an int in turns because both inputs are turn counts."
- **Unit:** turns (int): divides a difference of two turn-count ETAs (`(theirs - ours) / scale`), giving a dimensionless p in [0,1].
- **Formula at the primary call site:** deepwonder.c:249,264 deep_wonder_p_win(): `double p, scale = MAX(1, tune->wrace_margin_turns);` ... `p = 0.5 + 0.5 * (theirs - ours) / scale; return CLIP(0.0, p, 1.0);`
- **All call sites:**
  - deepwonder.c:249 -- deep_wonder_p_win -- P(win) ramp scale; consumed by deep_wonder_race() (:596) and deep_wonder_race_urgency() (:725) -- live at compiled defaults? yes, but only as census at defaults: p feeds `decline`/log and factor terms whose gains (wrace_pwin_gain, wrace_urgency_gain) are 0, so no build decision changes
- **Log line:** deep-wrace (deepwonder.c:634: p=%.3f ... decline=%d), deep-wrace-sum, deep-wrace-end
- **Interacts with:** wrace_pwin_gain, wrace_urgency_gain (the two consumers of p); wrace_eta_gain, wrace_obsolete_gain (same pass, do not read p)
- **Tier/screen:** Tier C (screen-corpus-2026-08-30.md:228: "the first name outside [the tier-B blocks] is wrace_margin_turns at 69.7" instrument firings/game); screen: not screened for flips (firing rate 69.7/game only; knobcensus MAP: S4 mag, proxy tag deep-wrace theirs>=0)
- **Prior evidence:**
  - WRACE (STATUS.md:10578-10650, 2026-08-07) -- contested probe 10 seeds, census only: 4902 evaluations, estimated P(win) median 1.000 vs observed win rate 0.371 over 35 resolved races; no scored arm.
  - WRACE race-pass check (STATUS.md:17600-17800, 2026-08-22) -- 4 seeds/cell probes under pv3 (margin 20): "decline has been dead code as an action". No A/B.
  - Campaign-moved: pv3.ini 20 vs default 10.

### wrace_pwin_gain
- **Declared:** deeptuning.h:750 (`double wrace_pwin_gain;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the race term: a contested wonder we would lose ranks below an uncontested lesser one." Composition gains move the want "from unchanged to fully the factor: want x (1 - gain x (1 - factor))". At 0 "the factor is computed, logged and not applied".
- **Unit:** multiplier (dimensionless): blends a [0,1] factor into a building-want multiplier; applied to `*want` in building_want[] (want units) but the knob itself is a 0-1 blend weight.
- **Formula at the primary call site:** deepwonder.c:606-608 deep_wonder_race(): `if (tune->wrace_pwin_gain > 0.0) { pwin_f = 1.0 - tune->wrace_pwin_gain * (1.0 - p); factor *= pwin_f; }` then (:667) `if (factor != 1.0) *want = (adv_want) (*want * factor);` on wonder-city building_want[] of great wonders.
- **All call sites:**
  - deepwonder.c:606 -- deep_wonder_race -- p-win discount on the wonder city's great-wonder building_want -- live at compiled defaults? no: dead behind wrace_pwin_gain=0 (the `> 0.0` test)
- **Log line:** deep-wrace (pwin_f=%.3f, factor=%.3f) -- computed and logged even at 0; the multiply is skipped
- **Interacts with:** wrace_margin_turns (sets p), wrace_eta_gain, wrace_obsolete_gain (multiplied into the same `factor`)
- **Tier/screen:** Tier C; screen: not screened (knobcensus MAP: "-" no per-term instrument originally; per-term pwin_f= later added to deep-wrace)
- **Prior evidence:**
  - WRACE conseq-wrace-probe.ini (all gains 1.0 together with expiry/wonder_merit; STATUS.md:10578-10650) -- census only: face-value composition factor mean 0.292 median 0.256 (cuts a wonder want ~70%); no scored arm.
  - WRACE race-pass check (STATUS.md:17617-17640, 17779-17793): at pv3 value 0.03697 it is "a 3.7% want shave"; "the whole pass... averages factor 0.90-0.92... a rounding error"; "decline has been dead code as an action".

### wrace_eta_gain
- **Declared:** deeptuning.h:751 (`double wrace_eta_gain;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the completion-ETA discount: a 40-turn build must not be paid from today's budget at face value." (same composition shape as wrace_pwin_gain)
- **Unit:** multiplier (dimensionless 0-1 blend weight); operand `disc` = amortize(1000, ours)/1000 is dimensionless.
- **Formula at the primary call site:** deepwonder.c:610-614 deep_wonder_race(): `if (tune->wrace_eta_gain > 0.0 && ours > 0) { double disc = (double) amortize((adv_want) 1000, ours) / 1000.0; eta_f = 1.0 - tune->wrace_eta_gain * (1.0 - disc); factor *= eta_f; }`
- **All call sites:**
  - deepwonder.c:610 -- deep_wonder_race -- ETA discount on great-wonder building_want -- live at compiled defaults? no: dead behind wrace_eta_gain=0
- **Log line:** deep-wrace (eta_f=%.3f)
- **Interacts with:** wrace_pwin_gain, wrace_obsolete_gain (same `factor`); MORT via amortize()
- **Tier/screen:** Tier C; screen: not screened (no per-term instrument in the original MAP; eta_f= now logged)
- **Prior evidence:**
  - Same as wrace_pwin_gain (WRACE, STATUS.md:10578-10650: census only; factor mean 0.292 at all-gains-1.0). pv3.ini 0.1188 (campaign-evolved).

### wrace_obsolete_gain
- **Declared:** deeptuning.h:752 (`double wrace_obsolete_gain;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the obsolescence truncation: a wonder five turns from the advance that voids it is currently priced as eternal."
- **Unit:** multiplier (dimensionless 0-1 blend weight); operand `share` = min(1, life/MORT) is dimensionless (turn counts over MORT=24).
- **Formula at the primary call site:** deepwonder.c:616-624 deep_wonder_race(): `if (tune->wrace_obsolete_gain > 0.0 && ob_eta >= 0) { double life = MAX(0, ob_eta - MAX(ours, 0)); double share = MIN(1.0, life / (double) MORT); obs_f = 1.0 - tune->wrace_obsolete_gain * (1.0 - share); factor *= obs_f; }`
- **All call sites:**
  - deepwonder.c:616 -- deep_wonder_race -- obsolescence truncation on great-wonder building_want -- live at compiled defaults? no: dead behind wrace_obsolete_gain=0
- **Log line:** deep-wrace (obs_f=%.3f, obsolete=%d)
- **Interacts with:** wrace_pwin_gain, wrace_eta_gain (same `factor`)
- **Tier/screen:** Tier C; screen: not screened
- **Prior evidence:**
  - Same as wrace_pwin_gain (WRACE, STATUS.md:10578-10650). pv3.ini 0.07175 (campaign-evolved).

### wrace_urgency_gain
- **Declared:** deeptuning.h:753 (`double wrace_urgency_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "multiplies the help-wonder caravan want as a race we LEAD gets close (R2b is the delivery arm). 1.0 + gain at a dead heat, 1.0 once we lead by the whole margin or trail."
- **Unit:** multiplier (dimensionless): returns a factor 1.0+gain*(...) applied to the wonder-caravan want (want units); knob range 0-4.
- **Formula at the primary call site:** deepwonder.c:733 deep_wonder_race_urgency(): `return 1.0 + tune->wrace_urgency_gain * (1.0 - (p - 0.5) * 2.0);` (early return 1.0 at :721 if gain<=0); applied at deeptrade.c:1717-1727 in deep_wonder_merit_want(): `want = (adv_want)(want * urg);`
- **All call sites:**
  - deepwonder.c:721,733 -- deep_wonder_race_urgency -- race-urgency multiplier on the help-wonder caravan want -- live at compiled defaults? no: dead behind wrace_urgency_gain=0 and the wonder_merit=0 gate on its only caller (deeptrade.c:1717)
- **Log line:** deep-wrace-feed (deeptrade.c:1721; emitted only when urg != 1.0)
- **Interacts with:** wrace_margin_turns (p), wonder_merit, wonder_merit_gain
- **Tier/screen:** Tier C; screen: not screened (knobcensus MAP: S4 direct tag deep-wrace-feed)
- **Prior evidence:**
  - WRACE (STATUS.md:10600-10620): conseq-wrace-probe.ini with wonder_merit=1: 659 caravan evaluations/10 games, "deep-wrace-feed is 0.0 a game" -- population empty on contested. No scored arm.

### expiry_building_gain
- **Declared:** deeptuning.h:774 (`double expiry_building_gain;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "truncates a building credit's horizon at the expected arrival of the advance that voids it, when OUR OWN queue is the one bringing it... want x (1 - gain x (1 - surviving share))."
- **Unit:** multiplier (dimensionless 0-1); acts on `add` in building-want units; share = ob/horizon (turn counts) is dimensionless.
- **Formula at the primary call site:** deephorizon.c:2423-2424,2435 deep_adjust_building_wants(): `double keep = 1.0 - deep_tuning_get()->expiry_building_gain * (1.0 - share); ... add = (adv_want) (add * MAX(keep, 0.0));` with share = MIN(1.0, ob / horizon).
- **All call sites:**
  - deephorizon.c:2400 -- deep_adjust_building_wants -- gate for the truncation `if (add > 0 && gain > 0.0)` -- live at compiled defaults? no: dead behind expiry_building_gain=0
  - deephorizon.c:2423 -- deep_adjust_building_wants -- keep factor applied to the building credit -- live at compiled defaults? no: same gate
  - deephorizon.c:2438,2450 -- deep_adjust_building_wants -- census-only branch recording the factor (no behaviour) -- live at compiled defaults? no: same gate (> 0.0)
- **Log line:** deep-expiry (deephorizon.c:2429, building form: impr= eta= share= credit= keep=); also keep= on deep-decay-sum
- **Interacts with:** upkeep_gold_weight (same `add` just before), horizon_* weights, MORT
- **Tier/screen:** Tier C (screen: provably inert); screen: inert (knob-screen-2026-08-30.md:118: over 93k credited improvements no +-1 sigma move changes the building anchor's argmax; STATUS.md:23056). Note the replay perturbs a 0.0 default; at defaults the path is gated off.
- **Prior evidence:**
  - CONSEQ item 4 (STATUS.md:10490-10500, conseq-wrace-probe.ini gain 1.0): execution probe only -- deep-expiry 1545.8/game (15398 building : 60 unit) on contested 10 seeds; "Item 4's building truncation is the largest new population in the pair (15 398 a game) and has never been scored."
  - W25 screen (knob-screen-2026-08-30.md:118, screen-corpus-2026-08-30.md:198-206): inert under replay; one of the two flip-nothing members.
  - Campaign-moved: pv3.ini 0.1111.

### expiry_unit_gain
- **Declared:** deeptuning.h:775 (`double expiry_unit_gain;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "defers a unit build whose SUCCESSOR arrives inside its own build time... Never applied to an emergency-tier want or a city carrying danger or urgency."
- **Unit:** multiplier (dimensionless 0-1): `w.value *= 1.0 - gain` on a unit want value (want units).
- **Formula at the primary call site:** deepdefence.c:6571 deep_military_choose_build(): `w.value *= 1.0 - deep_tuning_get()->expiry_unit_gain; deep_want_apply(choice, w);`
- **All call sites:**
  - deepdefence.c:6545 -- deep_military_choose_build -- gate `expiry_unit_gain > 0.0` for the successor-arrives-first deferral -- live at compiled defaults? no: dead behind expiry_unit_gain=0
  - deepdefence.c:6571 -- deep_military_choose_build -- deflates the NORMAL-tier unit want -- live at compiled defaults? no: same gate
- **Log line:** deep-expiry (deepdefence.c:6565, unit form: unit= next= eta= build= gap= want=)
- **Interacts with:** expiry_unit_gap, the successor ETA from deeptech
- **Tier/screen:** Tier C; screen: not screened (knobcensus MAP: S2 direct tag deep-expiry has:unit; gated off at defaults)
- **Prior evidence:**
  - CONSEQ (STATUS.md:10490-10500): probe at gain 0.5, 60 unit deferrals vs 15398 building truncations per game; no scored arm.
  - Campaign-moved: pv3.ini 0.2392.

### expiry_unit_gap
- **Declared:** deeptuning.h:776 (`int expiry_unit_gap;`); params.ini: default=2 min=0 max=20 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the hysteresis in turns -- the successor must beat the build by this margin before the queue churns... a gap and not a ratio because both sides are turn counts."
- **Unit:** turns (int): compared as `eta + gap <= build`, both turn counts.
- **Formula at the primary call site:** deepdefence.c:6561 deep_military_choose_build(): `if (build > 0 && build < FC_INFINITY && eta + deep_tuning_get()->expiry_unit_gap <= build)`
- **All call sites:**
  - deepdefence.c:6561 -- deep_military_choose_build -- hysteresis margin for deferring a unit build -- live at compiled defaults? no: only under expiry_unit_gain>0 (enclosing gate at :6545)
  - deepdefence.c:6570 -- deep_military_choose_build -- echoed on deep-expiry (read only) -- live at compiled defaults? no: same gate
- **Log line:** deep-expiry (gap=%d)
- **Interacts with:** expiry_unit_gain
- **Tier/screen:** Tier C; screen: not screened (knobcensus: its branch never runs at expiry_unit_gain=0)
- **Prior evidence:**
  - none beyond the CONSEQ probe above (no scored arm). Campaign-moved: pv3.ini 8.

### sell_defend_gain
- **Declared:** deeptuning.h:796 (`double sell_defend_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "rung 1, SELL TO DEFEND: when a city wants a defender bought and the treasury cannot cover the rush, sell the highest-value sellable building... The gain is the BAR and not a scale: sell only while the building's sale price is at most gain x the rush cost."
- **Unit:** multiplier (dimensionless): a ratio of two gold amounts (building sale gold vs defender buy cost), used as a bar `best_gold > gain * buycost`; both sides are gold.
- **Formula at the primary call site:** deepdefence.c:7197 deep_sell_to_defend(): `|| best_gold > tune->sell_defend_gain * buycost) { ...log deep-selldefend...; return FALSE; }` (entry gate at :7160 `tune->sell_defend_gain <= 0.0` returns FALSE).
- **All call sites:**
  - deepdefence.c:7160 -- deep_sell_to_defend -- entry gate; returns FALSE before reading anything -- live at compiled defaults? no: dead behind sell_defend_gain=0
  - deepdefence.c:7197 -- deep_sell_to_defend -- the sale-price bar -- live at compiled defaults? no: same gate
- **Log line:** deep-selldefend (deepdefence.c:7199, 7219)
- **Interacts with:** survival_horizon, survival_target (gate on the verdict wave/surv in the same function)
- **Tier/screen:** Tier C; screen: not screened (knobcensus MAP: S2 direct tag deep-selldefend; dead at defaults)
- **Prior evidence:**
  - CONSEQ (STATUS.md:10440-10470) -- conseq-lastditch.ini (sell_defend_gain=1.0 AND late_buy_gate=1 together), contested, 20 paired seeds, pin bdd23bb77f8b04: score 85.10 -> 84.46, -0.64 (SE 0.53, t -1.22, 4/20); mean_city_size t -2.00; CITY LOSSES -0.05; 2.65 buildings sold/game (44 Barracks of 50), every sale completed its buy. STATUS.md:10539: "sell_defend_gain is the first tuning entry the session earned".
  - Campaign-moved: pv3.ini 0.4187.

### late_buy_gate
- **Declared:** deeptuning.h:797 (`int late_buy_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "rung 3, THE FINAL DEFENCE BUY: at the end of deep's phase, spend gold the turn ADDED since the main buy pass, restricted to the triage population."
- **Unit:** boolean gate (0/1): `late_buy_gate <= 0` returns before reading a city.
- **Formula at the primary call site:** deepdefence.c:7276 deep_late_defence_buy(): `if (tune->late_buy_gate <= 0 || deep_late_mark < 0) { deep_late_mark = -1; return; }`
- **All call sites:**
  - deepdefence.c:7276 -- deep_late_defence_buy -- whole-pass off-switch -- live at compiled defaults? no: dead behind late_buy_gate=0
- **Log line:** deep-latebuy (deepdefence.c:7316), deep-latebuy-sum
- **Interacts with:** survival_horizon, survival_target, deep_late_mark budget (gold added after the main buy pass)
- **Tier/screen:** Tier C; screen: not screened (dead at defaults; knobcensus tag deep-latebuy)
- **Prior evidence:**
  - CONSEQ (STATUS.md:10440-10470): in the conseq-lastditch arm (with sell_defend_gain) "Rung 3 has ZERO population: not one deep-latebuy line... the budget was never positive"; late buys 0.00/game.

### evacuate_gate
- **Declared:** deeptuning.h:808 (`int evacuate_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Rung 2a, CIVILIAN EVACUATION... 0/1 off-switch, PLACEHOLDER"; a civilian "contributes exactly zero to the defence and dies with the city, so being wrong costs a few unit-turns of walking".
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepdefence.c:3881 deep_evacuate_civilians(): `if (tune->evacuate_gate <= 0 || p >= 0.01 || relieved > p + 0.001) { return; }`
- **All call sites:**
  - deepdefence.c:3881 -- deep_evacuate_civilians -- whole-pass off-switch (plus the loose compound gate on survival p) -- live at compiled defaults? no: dead behind evacuate_gate=0
- **Log line:** deep-evac (deepdefence.c:3926)
- **Interacts with:** survival verdict p/relieved passed by the caller (survival_horizon, survival_target)
- **Tier/screen:** Tier C; screen: not screened (dead at defaults; knobcensus tag deep-evac)
- **Prior evidence:**
  - CONSEQ (STATUS.md:10440-10490): "Rung 2a (civilian evacuation) is built with its arm archived and was not A/B'd" (conseq-evac.ini exists; the host restart took the pins).

### upkeep_gold_weight
- **Declared:** deeptuning.h:1282 (`double upkeep_gold_weight;`); params.ini: default=0.125 min=0.0 max=1.25 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "charges a building's gold upkeep against its horizon credit, over the MORT horizon the credit itself is summed over (charging ONE turn of gold against a MORT-horizon value made the ruleset's bill ~24x too cheap; the default and the registered range were divided by MORT when that was corrected... the knob owes a tuning pass)".
- **Unit:** want/score units per gold (a conversion weight): the expression is gold-per-turn x turns x weight subtracted from `add`, which is a building-want sum (adv_want). content-military-gold-2026-08-30.md:131 reads it: "3 x 24 x 0.125 = 9 want-units against a mean credit of 161". Second use: unit_upkeep_charge charges (gold+food upkeep) x dur x this weight in want units.
- **Formula at the primary call site:** deephorizon.c:2394-2398 deep_adjust_building_wants(): `dk_bdur = deep_decay_duration(deep_horizon_turns(), 0, 1, -1); add -= city_improvement_upkeep(pcity, pimprove) * dk_bdur * deep_tuning_get()->upkeep_gold_weight; add = MAX(add, 0);` (the :2387 product `dk_upk` is the same figure, for the log).
- **All call sites:**
  - deephorizon.c:2387 -- deep_adjust_building_wants -- dk_upk recorded for deep-decay-sum (read only) -- live at compiled defaults? yes
  - deephorizon.c:2397 -- deep_adjust_building_wants -- subtracts the ruleset gold upkeep bill from every credited improvement's want -- live at compiled defaults? yes
  - deepdefence.c:7854 -- deep_unit_upkeep_charge -- prices a unit's gold+food lifetime upkeep -- live at compiled defaults? no: dead behind unit_upkeep_charge=0
- **Log line:** deep-decay-sum (upk=), deep-credit (deephorizon.c:2466, 2523); deep-upcharge for the unit side
- **Interacts with:** unit_upkeep_charge, horizon_decay_* gate (via deep_decay_duration), endgame_horizon (via deep_horizon_turns), horizon_capacity/growth/bundle/content/multiplier weights (the credit it is subtracted from), SHIELD_WEIGHTING (17, fixed)
- **Tier/screen:** Tier A (AUCTION); screen: flips 93.2 decisions/game (4.0% of 2329.3 building anchors/game; sigma 0.208; replay rule building_anchor, 100.0% self-check)
- **Prior evidence:**
  - Head-to-head tuning campaign (STATUS.md:735-800, 2026-07-28..30) -- knob's old scale (pre-MORT division): all three tuned vectors raised it ("AGREED across all three: raise upkeep_gold_weight (4.7/6.0/5.2 vs 3.0)"), but at n=60 per-seed pairing G-final vs defaults: cramped -0.13+-0.15 null, garden2b -0.18+-0.20 null, isolated +0.085+-0.056 significant (39/60); H-final all null. Not an isolated arm; scale changed afterwards.
  - fearseat/content-military-gold (findings 2026-08-30): deliberately NOT swept with the military arms because it "moves the T100+ building auction and would make the military result unreadable" (fearseat-verdict-2026-08-30.md:258).
  - Campaign-moved: a6-best.ini 1.5 (old scale), pv3.ini 0.2385.

### upkeep_unit_weight
- **Declared:** deeptuning.h:1283 (`double upkeep_unit_weight;`); params.ini: default=0.5 min=0.0 max=5.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "discounts the injected fear terms by the cheapest buildable defender's gold+food upkeep: discount = 1 / (1 + weight * upkeep)."
- **Unit:** multiplier (dimensionless) per upkeep point: `weight * cheapest` where `cheapest` is an integer gold+food upkeep/turn, so the knob has units 1/(upkeep point); the discount itself is dimensionless in (0,1].
- **Formula at the primary call site:** deepdefence.c:439 deep_defender_upkeep_discount(): `return 1.0 / (1.0 + tune->upkeep_unit_weight * cheapest);` (cheapest = min over buildable defenders of utype_upkeep_cost(O_GOLD)+utype_upkeep_cost(O_FOOD)); applied at :2384-2387 and :5526-5530 as `city_data->danger += (unsigned int)(over * updisc * sat * fmult)` and :5599 `rotation_want * updisc`.
- **All call sites:**
  - deepdefence.c:418 -- deep_defender_upkeep_discount -- <=0 returns 1.0 (identity) -- live at compiled defaults? yes
  - deepdefence.c:439 -- deep_defender_upkeep_discount -- the discount formula; feeds updisc at :2346 (deep_credit_incoming_defence) and :5487 (deep_military_choose_build) -- live at compiled defaults? yes
- **Log line:** deep-def / deep-credit style lines carry `updisc=%.3f` (deepdefence.c:2001, 2422, 5563)
- **Interacts with:** survival_fear_value, overseas_fear, density_fear_value, barbarian_fear, boat_shadow_fear, rotation_want, satisfied_fear_discount (all multiplied together with updisc)
- **Tier/screen:** Tier C (legacy S/ECON/MIL/ALL blocks only); screen: not screened (knobcensus MAP: S2 "-" no instrument originally; updisc= now logged)
- **Prior evidence:**
  - Head-to-head tuning campaign (STATUS.md:790-791): "AGREED across all three: ... raise upkeep_unit_weight" -- campaign-moved, not an isolated arm; a6-best.ini 1.0.

### unit_upkeep_charge
- **Declared:** deeptuning.h:1660 (`double unit_upkeep_charge;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "UNIT LIFETIME UPKEEP AT THE BUILD SEAM... Charged symmetrically with buildings: upkeep per turn x min(E[lifetime], horizon)... 0 = off. Moves every unit want in the auction, so it is A/B'd alone."
- **Unit:** multiplier (dimensionless) on a charge already in want units: charge = up_shield*dur*SHIELD_WEIGHTING + (up_gold+up_food)*dur*upkeep_gold_weight; `debit = charge * unit_upkeep_charge` (adv_want).
- **Formula at the primary call site:** deepdefence.c:7852-7855 deep_unit_upkeep_charge(): `charge = (double) up_shield * dur * SHIELD_WEIGHTING + (double)(up_gold + up_food) * dur * tune->upkeep_gold_weight; debit = (adv_want)(charge * tune->unit_upkeep_charge);`
- **All call sites:**
  - deepdefence.c:7809 -- deep_unit_upkeep_charge -- entry gate (<=0 returns 0) -- live at compiled defaults? no: dead behind unit_upkeep_charge=0
  - deepdefence.c:7855 -- deep_unit_upkeep_charge -- debit subtracted from the choice's want, both sides of the merge (called at :8807-8809 for "mil" and "dom") -- live at compiled defaults? no: same gate
- **Log line:** deep-upcharge (deepdefence.c:7863)
- **Interacts with:** upkeep_gold_weight, endgame_horizon (life = deep_horizon_turns()), horizon_decay_* (deep_decay_duration), switch_cost_*, scale_* (applied after scale, before switch debit)
- **Tier/screen:** Tier C; screen: dead-path (knob-screen-2026-08-30.md:143-153: tag never appeared in 40 games; "Turn the gate on and the screen prices them the same afternoon")
- **Prior evidence:**
  - M4b (STATUS.md:10670-10700, findings/m4b-deterrence-2026-08-07.md:482) -- m4b-upcharge.ini unit_upkeep_charge=1.0 alone, contested, 20 paired seeds, pin cff0da39e9dd3d: score 85.10 -> 83.29, -1.81 (SE 2.79, t -0.65, 6/20); pop -1.91 (t -2.51); 1551 upcharges/game. Null on score.
  - STATUS.md:14840-14845 (DECAY slice 2): dormant under pv3; liveness probe at 1.0 showed deep-upcharge 2323/2323 lines with rate>0.

### rates_enabled
- **Declared:** deeptuning.h:2101 (`int rates_enabled;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "rates_enabled != 0 lets deep re-choose the split by comparing the amortized value of what each point BUYS... 0 = off = stock's ladder untouched, which is the compiled default."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deeprates.c:476 deep_manage_taxes(): `dai_manage_taxes(ait, pplayer); if (tune->rates_enabled == 0) { return; }`
- **All call sites:**
  - deeprates.c:476 -- deep_manage_taxes -- whole-feature off-switch after stock's dai_manage_taxes() -- live at compiled defaults? yes (as the stock path; the deep re-choice is dead behind rates_enabled=0)
- **Log line:** deep-rates (deeprates.c:490, 502, 539; modes celebrate/unpriced/set/agree)
- **Interacts with:** rates_science_gain, rates_luxury_gain, gov_rate_ceiling_gain (gov code reuses the rates model), adv->science_priority, adv->gold_priority
- **Tier/screen:** Tier C; screen: dead-path (knob-screen-2026-08-30.md:135-137 "rates (3)")
- **Prior evidence:**
  - R1 (STATUS.md:5870-6090, 2026-08-05) -- identity.ini vs identity.ini+rates_enabled=1 (both gains 1.0), isolated-150, 20 paired seeds, pin 638e167d2d0a: score 194.32 -> 144.51, -49.81 (SE 5.94, t -8.39, 1/20); techs -15.54 (t -21.56). Negative. Verdict: "rates_enabled stays 0 and must not be turned on until the two gains have been through a tuning pass."

### rates_science_gain
- **Declared:** deeptuning.h:2102 (`double rates_science_gain;`); params.ini: default=1.0 min=0.0 max=5.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Both PLACEHOLDER 1.0 and both owed a tuning pass; a degenerate all-science or all-tax split is what a wrong gain looks like." (honest residue for the bulb valuation)
- **Unit:** multiplier (dimensionless) on the science term v_sci = amortize(bulbs * sci_priority, wait) * MORT * gain (want/score value of the split, same sum as v_gold, v_lux). R1 measured vlux/vsci averaging 4.33 at gains 1.0.
- **Formula at the primary call site:** deeprates.c:161 deep_rate_value(): `*v_sci = amortize(bulbs * m->sci_priority, wait) * MORT * m->sci_gain;` (m->sci_gain set at :423 from the knob)
- **All call sites:**
  - deeprates.c:423 -- deep_rates_model -- copies the knob into the model -- live at compiled defaults? no: model is only consumed when rates_enabled>0 (deep_manage_taxes) or gov_rate_ceiling_gain>0 (deepgov.c:284)
  - deeprates.c:161 -- deep_rate_value -- scales the science term of every legal triple -- live at compiled defaults? no: same
- **Log line:** deep-rates (vsci=)
- **Interacts with:** rates_enabled, rates_luxury_gain, adv->science_priority, MORT
- **Tier/screen:** Tier C; screen: dead-path (rates (3), knob-screen-2026-08-30.md:135)
- **Prior evidence:**
  - R1 (STATUS.md:6075-6077): handoff, not a result -- "rates_science_gain around 4-13" read out of logged terms; never itself moved. pv3.ini 2.267 (campaign-evolved).

### rates_luxury_gain
- **Declared:** deeptuning.h:2103 (`double rates_luxury_gain;`); params.ini: default=1.0 min=0.0 max=5.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** Same pair as rates_science_gain (header: "Both PLACEHOLDER 1.0"); scales the luxury valuation.
- **Unit:** multiplier (dimensionless) on v_lux = lux_by_rate[lux/10] * MORT * gain (the freed-citizen tile value).
- **Formula at the primary call site:** deeprates.c:172 deep_rate_value(): `*v_lux = m->lux_by_rate[lux / 10] * MORT * m->lux_gain;` (m->lux_gain set at :431)
- **All call sites:**
  - deeprates.c:431 -- deep_rates_model -- copies the knob into the model -- live at compiled defaults? no: dead behind rates_enabled=0 (and gov_rate_ceiling_gain=0)
  - deeprates.c:172 -- deep_rate_value -- scales the luxury term -- live at compiled defaults? no: same
- **Log line:** deep-rates (vlux=)
- **Interacts with:** rates_enabled, rates_science_gain, horizon_content_weight (via lux_by_rate)
- **Tier/screen:** Tier C; screen: dead-path (rates (3))
- **Prior evidence:**
  - R1 (STATUS.md:6070-6077): diagnosis "luxury term roughly half an order of magnitude too hot"; suggested range 0.1-0.25; not moved in an arm. pv3.ini value 0.0 (campaign-evolved).

### gov_valuation
- **Declared:** deeptuning.h:2120 (`int gov_valuation;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "gov_valuation != 0 replaces [stock's TODO-horizon dai_gov_value] with the grammar every other deep credit is written in: the per-turn output DELTA against the incumbent, counted over deep_horizon_turns(), against the one-off cost of the revolution. 0 = off = the hook forwarded to stock unchanged."
- **Unit:** boolean gate (0/1) -- "Structural off-switch, not an optimiser knob."
- **Formula at the primary call site:** deepgov.c:500 deep_gov_value(): `if (tune->gov_valuation == 0) { dai_gov_value(ait, pplayer, gov, val, override); return; }`
- **All call sites:**
  - deepgov.c:500 -- deep_gov_value -- forwards to stock when 0 -- live at compiled defaults? yes (the stock forward); deep valuation dead behind 0
- **Log line:** deep-gov (deepgov.c:562)
- **Interacts with:** gov_anarchy_weight, gov_rate_ceiling_gain, endgame_horizon, horizon_decay_*
- **Tier/screen:** Tier C; screen: dead-path (gov (3), knob-screen-2026-08-30.md:136)
- **Prior evidence:**
  - GOV / G3 gov guard (STATUS.md:13229, 2026-08-12) -- pv2 vs pv2-gov.ini (gov_valuation=1), lategame-80 holdout, 20 seeds, cross-binary: -36.5 vs -10.1, paired -26.4, t -2.92, 16/20 worse ("significantly harmful as shipped").
  - STATUS.md:13748 (same-binary re-run, anarchy floor in): gov_valuation=1 vs H0 control, paired -5.5, t -0.55 -- NULL. Liveness only (2 games) in the GOV entry STATUS.md:12170-12250 ("The valuation is therefore unmeasured for score").

### gov_anarchy_weight
- **Declared:** deeptuning.h:2134 (`double gov_anarchy_weight;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "What the anarchy a revolution passes through is charged at... 1.0 therefore means 'charge the measured loss at face value' and is an identity rather than a taste... Inert at gov_valuation 0. NOT inert above EASY."
- **Unit:** multiplier (dimensionless): charge = anarchy_turns * MAX(0, base - anarchy_out) * weight, where base/anarchy_out are per-turn empire outputs (want/score per turn) and anarchy_turns is a turn count, so the charge is in the same output x turns currency as the gain.
- **Formula at the primary call site:** deepgov.c:527-529 deep_gov_value(): `charge = anarchy_turns * MAX(0.0, base - anarchy_out) * tune->gov_anarchy_weight;` then `value = (cand - base) * dur - charge + ceiling;`
- **All call sites:**
  - deepgov.c:529 -- deep_gov_value -- revolution charge -- live at compiled defaults? no: dead behind gov_valuation=0; also zero when the server charges an AI no anarchy (benchmark cells run `hard`: STATUS.md:12170 "anturns=0.0 on 527/527")
- **Log line:** deep-gov (ancost=)
- **Interacts with:** gov_valuation, gov_rate_ceiling_gain, endgame_horizon (horizon in dur), horizon_decay_*
- **Tier/screen:** Tier C; screen: dead-path (gov (3))
- **Prior evidence:**
  - STATUS.md:13751: "knob to the pile for the finale ECON sweep alongside gov_anarchy_weight" -- never moved in an arm; decaytech-best.ini carries a value (campaign-evolved).

### gov_rate_ceiling_gain
- **Declared:** deeptuning.h:2151 (`double gov_rate_ceiling_gain;`); params.ini: default=0.0 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "What a government's RATE CEILING is worth (darkfx item 5)... PLACEHOLDER, 0.0 = off... the term is the one part of this valuation whose magnitude has no honest source, because it converts a rates-enumeration total into a government want and nothing has measured that ratio."
- **Unit:** UNSURE (calibration): nominally a multiplier (dimensionless) on deep_gov_ceiling_gain(), a difference of two deep_rates_pick(...).value totals (each already x MORT, in the rates model's v_sci+v_gold+v_lux currency), added directly to `value` (output-delta x turns). The header itself says the conversion ratio between those currencies is unmeasured.
- **Formula at the primary call site:** deepgov.c:549-551 deep_gov_value(): `if (tune->gov_rate_ceiling_gain > 0.0) { ceiling = deep_gov_ceiling_gain(pplayer) * tune->gov_rate_ceiling_gain; ...}` then `value = (cand - base) * dur - charge + ceiling;`
- **All call sites:**
  - deepgov.c:284 -- deep_gov_baseline -- builds the rates model only when gain>0 (`gov_rate_model_ok`) -- live at compiled defaults? no: dead behind gov_rate_ceiling_gain=0 and gov_valuation=0
  - deepgov.c:549 -- deep_gov_value -- ceiling credit -- live at compiled defaults? no: same
- **Log line:** deep-gov (ceil=)
- **Interacts with:** gov_valuation, rates_enabled (shares deeprates.c model), rates_science_gain, rates_luxury_gain
- **Tier/screen:** Tier C; screen: dead-path (gov (3))
- **Prior evidence:**
  - GOV (STATUS.md:12184-12190): liveness only -- at 1.0 ceil non-zero on 339 of 528 lines, ordered Monarchy +18.0, Communism +82.5, Federation +127.2 at T10; shipped 0.0 -> ceil=0.0 on 518/518. gov-ceiling.ini exists; no scored arm.

### trade_merit
- **Declared:** deeptuning.h:2184 (`int trade_merit;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "trade_merit != 0 replaces [stock's dai_choose_trade_route bid: hardcoded distance of 10, only the SOURCE city's trade, no destination] with the discounted stream the caravan would actually collect: caravan.c's own NPV model run against the REAL destinations... annuitized back to a per-turn output value and amortized over the turns until the caravan is finished. 0 = off = stock's bid untouched."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deeptrade.c:1450 deep_trade_merit_apply(): `if (deep_tuning_get()->trade_merit <= 0) { return; }`; also deepdefence.c:9000/9007 in deep_city_choose_build(): fallback-ladder caravan rung is skipped when `trade_merit > 0`, taken (stock behaviour) when `<= 0`.
- **All call sites:**
  - deeptrade.c:1450 -- deep_trade_merit_apply -- whole merit-bid pass (probe, replace/promote/keep) is skipped when 0 -- live at compiled defaults? no: deep path dead behind trade_merit=0
  - deepdefence.c:9000 -- deep_city_choose_build -- logs deep-trade-fallback skipped=1 when merit is on -- live at compiled defaults? no: only under trade_merit>0
  - deepdefence.c:9007 -- deep_city_choose_build -- fallback ladder may enqueue a want-1 caravan when merit is off -- live at compiled defaults? yes (this is the stock-ladder branch that runs at defaults)
- **Log line:** deep-trade-merit (deeptrade.c:1489), deep-trade-fallback (deepdefence.c:~9003)
- **Interacts with:** trade_merit_gain, trade_ceasefire_peace_odds, trade_route_risk_gain, caravan_discount, caravan_foreign_trade
- **Tier/screen:** Tier C; screen: dead-path (knob-screen-2026-08-30.md:135 "the trade and wonder merit pair (6 knobs)")
- **Prior evidence:**
  - R2 (STATUS.md:6255-6570, 2026-08-05; note STATUS.md:9181 "R2's entire calibration table was taken on the inverted price") -- primary-120-classic, 20 paired seeds, pin ac74b9367d3a, all arms load tuning files (control identity.ini): trade_merit=1 at gain 1: score 138.40 -> 137.78, -0.63 (SE 1.31, t -0.48, 9/20); at gain 7: -1.06 (SE 1.10, t -0.96, 10/20). Route endpoints 10.0 -> 3.5 (g=1) / 19.6 (g=7). Null on score. civ2civ3 smoke guard (10 seeds): -0.03 (t -0.43), seam fires and finds nothing.
  - Verdict STATUS.md:6555: "trade_merit ships 0" -- civ2civ3 makes domestic routes illegal (National pct 0); TRADE (STATUS.md:9094-9100): lategame-80 holds 0.30 route endpoints/game.

### trade_merit_gain
- **Declared:** deeptuning.h:2185 (`double trade_merit_gain;`); params.ini: default=1.0 min=0.1 max=5.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the honest residue and the thing a tuning pass must find: the NPV is in raw output points priced by the advisor's own trade priority, and whether that lands on the same scale as a Library's want is a CALIBRATION question... 1.0 is the untuned corner, not a claim."
- **Unit:** multiplier (dimensionless) on a want in adv_want units: `want = amortize(npv * science_priority / MORT, build_turns) * gain`, clamped to DAI_WANT_DOMESTIC_MAX (750.075). The NPV operand is "raw output points" (header), converted to want by adv->science_priority.
- **Formula at the primary call site:** deeptrade.c:1319-1323 deep_trade_merit_want(): `want = amortize((adv_want)(pick->npv * adv->science_priority / MORT), MAX(tc->build, 0)) * tune->trade_merit_gain; return MIN(want, DAI_WANT_DOMESTIC_MAX);`
- **All call sites:**
  - deeptrade.c:1321 -- deep_trade_merit_want -- scales the caravan merit want -- live at compiled defaults? no: dead behind trade_merit=0 (caller deep_trade_merit_apply returns first)
- **Log line:** deep-trade-merit (merit=)
- **Interacts with:** trade_merit, caravan_discount, adv->science_priority, MORT, DAI_WANT_DOMESTIC_MAX
- **Tier/screen:** Tier C; screen: dead-path (merit pair)
- **Prior evidence:**
  - R2 (STATUS.md:6377, 6564): "gain's useful range is 5-10, read off two measured arms": g=1 -0.63 (t -0.48), g=7 -1.06 (t -0.96), 20 paired seeds primary-120-classic (see trade_merit). TRADE STATUS.md:9187: "The corrected price has never been run at a gain where it acts. ~4.8 puts it on stock's median."
  - Campaign-moved: pv3.ini 0.1 (the range floor).

### trade_ceasefire_peace_odds
- **Declared:** deeptuning.h:2212 (`double trade_ceasefire_peace_odds;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the probability that peace follows the cease-fire, which buys back the post-lapse tail. At 0.0 -- the compiled default -- the price is pure known mechanics and no fragile route is farmed on optimism."
- **Unit:** probability 0-1: multiplies the annuity tail beyond the cease-fire lapse (`odds * deep_route_annuity_tail(...)`).
- **Formula at the primary call site:** deeptrade.c:824-827 deep_route_price(): `ongoing = deep_route_annuity(c->per_turn, c->life, discount) + tune->trade_ceasefire_peace_odds * deep_route_annuity_tail(c->per_turn, c->life, full, discount);` and :833 `c->npv = tune->trade_ceasefire_peace_odds * deep_route_pv(...)` for routes arriving after lapse.
- **All call sites:**
  - deeptrade.c:826 -- deep_route_price -- tail recovery on a cease-fire-truncated route -- live at compiled defaults? no: dead behind trade_merit=0 (deep_route_price only called from deep_trade_merit_want :1268); also exactly 0 at default
  - deeptrade.c:833 -- deep_route_price -- after-lapse optimism branch -- live at compiled defaults? no: same
- **Log line:** deep-trade-merit (trunc=)
- **Interacts with:** trade_merit, caravan_discount
- **Tier/screen:** Tier C; screen: dead-path (merit pair; knobcensus: "exactly zero at the compiled odds=0.0 default regardless of trunc")
- **Prior evidence:**
  - TRADE (STATUS.md:9114): level-1 off proof only (10/10 byte-identical); no scored arm. Campaign-moved: pv3.ini 0.1189.

### trade_route_risk_gain
- **Declared:** deeptuning.h:2213 (`double trade_route_risk_gain;`); params.ini: default=0.0 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The route's NPV is multiplied by 1 - gain * (dangerous tiles / path length), clamped to [0,1], so 0.0 is exactly no discount and 1.0 prices a wholly dangerous road as worthless."
- **Unit:** multiplier (dimensionless): scales a fraction (dangerous tiles / path tiles) into a survival probability.
- **Formula at the primary call site:** deeptrade.c:1260-1266 deep_trade_merit_want(): `survival = CLIP(0.0, 1.0 - tune->trade_route_risk_gain * f, 1.0);` then `c->npv *= survival` in deep_route_price().
- **All call sites:**
  - deeptrade.c:1260 -- deep_trade_merit_want -- per-candidate road-danger discount -- live at compiled defaults? no: dead behind trade_merit=0 (and own value 0)
- **Log line:** deep-trade-merit (risk=)
- **Interacts with:** trade_merit, settler_guard_tolerance (danger test it reuses)
- **Tier/screen:** Tier C; screen: dead-path (merit pair)
- **Prior evidence:**
  - TRADE (STATUS.md:9115): switch-tree proof only; no scored arm. Campaign-moved: trade-g1.ini 0.0 pin; pv-files carry values.

### trade_route_love
- **Declared:** deeptuning.h:2258 (`double trade_route_love;`); params.ini: default=5.0 min=0.0 max=20.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "love per ACTIVE foreign trade route per turn, added from deep's own phase wrapper... counterweight to a bleed that already exists... trade love is added only while love stays below the player's own req_love_for_alliance... So the compiled default is 5.0, not 0.0 (human call 2026-08-06)."
- **Unit:** count (love points added to pplayer->ai_common.love[partner] per active route per turn; an int in stock's love scale, capped at req_love_for_alliance - 1).
- **Formula at the primary call site:** deeptrade.c:438 deep_trade_route_love(): `add = (int)(tune->trade_route_love * routes[slot]); ... add = MIN(add, cap - *love); *love += add;`
- **All call sites:**
  - deeptrade.c:414 -- deep_trade_route_love -- entry gate (<=0 returns) -- live at compiled defaults? yes (default 5.0)
  - deeptrade.c:438 -- deep_trade_route_love -- love increment per partner -- live at compiled defaults? yes in code; fires only when deep_routes_with_players() > 0 -- 0 firings on civ2civ3 cells (STATUS.md:9150)
- **Log line:** deep-trade-love (deeptrade.c:451)
- **Interacts with:** peace_seek_gain (ships ON together by human call), gift_love_gain, req_love_for_alliance (stock)
- **Tier/screen:** Tier C; screen: not named in the screen docs; instrument expected silent on civ2civ3 (STATUS.md:9150: 0 deep-trade-love lines in 60 games)
- **Prior evidence:**
  - TRADE fold (STATUS.md:9120-9160, 2026-08-06) -- lategame-80, 20 paired seeds: peace_seek_gain 0->1 AND trade_route_love 0->5 together: score 168.05 -> 169.97, +1.93 (SE 1.31, t +1.47, 13/20). Third arm (peace_seek_gain=1, trade_route_love=0) was byte-identical on 20/20 scorelogs with 0 deep-trade-love lines in all three arms: "the whole +1.93 is the bar removal", the anchor is inert on civ2civ3. On classic: 64 firings over 6 games.

### trade_partner_target_discount
- **Declared:** deeptuning.h:2259 (`double trade_partner_target_discount;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "multiplies [deep_op_pick_target's score] by (1 - discount) when we hold live routes with its owner, so among comparable targets deep burns a stranger before a partner... 0.0 is the PURE OPPORTUNIST posture and is the compiled default; 1.0 refuses a partner outright."
- **Unit:** multiplier (dimensionless 0-1) intended on a target score `worth / (1 + distance)`; BUT see formula: no multiplication by the knob exists in the current source.
- **Formula at the primary call site:** deepops.c:3129 deep_op_pick_target(): `bool spare = deep_tuning_get()->trade_partner_target_discount > 0.0;` -> `partner = spare && ... routes[player_index(aplayer)] > 0;` (:3155). The block that did `score *= 1.0 - trade_partner_target_discount; trade_partner_spared++` was removed by commit 2a80901ceb ("ai/deep: price an ally's attack request into the operation target score"); `partner` is now set and never read, and trade_partner_spared is never incremented.
- **All call sites:**
  - deepops.c:3129 -- deep_op_pick_target -- only read of the knob: sets `spare`, which only gates a deep_routes_with_players() census call and the unused `partner` flag -- live at compiled defaults? no effect even when >0: the discount multiply is absent (see Unresolved)
- **Log line:** deep-leaders (deepdiplomacy.c:1545 prints trade_partner_spared, which is never incremented -> always 0)
- **Interacts with:** leader_check_gain, chatty_inbound_gate (sibling multipliers on the same score), trade_route_love
- **Tier/screen:** Tier C; screen: not screened (knobcensus MAP expects spared>0 from deepops.c:2644, which no longer exists)
- **Prior evidence:**
  - TRADE (STATUS.md:9181-9186, 2026-08-06) -- execution probe only, before the multiply was removed: 44.67 applications/game at 0.5 on primary-120-classic; "it has no scored arm". none since.

### wonder_merit
- **Declared:** deeptuning.h:2296 (`int wonder_merit;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "wonder_merit != 0 re-denominates [dai_choose_help_wonder's caravan want] per shield delivered: the wonder city's own want times the share of the wonder these shields pay for... amortized over the turns to build the caravan PLUS the turns to walk it there. 0 = off = stock's bid untouched."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deeptrade.c:1795 deep_wonder_merit_apply(): `if (deep_tuning_get()->wonder_merit <= 0) { return; }`
- **All call sites:**
  - deeptrade.c:1795 -- deep_wonder_merit_apply -- whole wonder-caravan merit pass -- live at compiled defaults? no: dead behind wonder_merit=0
- **Log line:** deep-wonder-merit (deeptrade.c:1839)
- **Interacts with:** wonder_merit_gain, wrace_urgency_gain, endgame_* (city_turns_to_build does not model caravans)
- **Tier/screen:** Tier C; screen: dead-path (merit pair)
- **Prior evidence:**
  - R2b (STATUS.md:7647-7930, 2026-08-05) -- classic and civ2civ3 cells, 20 paired seeds, gain 1 and gain 5: "Score does not move on either cell at either gain (|t| <= 0.7)"; at the gain that matches stock, 7% fewer caravans, wonder dispatches 7.80 -> 10.85 (t +2.64), wonders completed 2.75 -> 3.10; at gain 1.0 "it abolishes the wonder caravan on civ2civ3 and costs wonders there". Null on score.

### wonder_merit_gain
- **Declared:** deeptuning.h:2297 (`double wonder_merit_gain;`); params.ini: default=1.0 min=0.1 max=5.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the residue, and it is DELIBERATELY not trade_merit_gain: the two wants have different denominators (an NPV in output points annuitized over MORT there, a building want scaled by a shield share here)... 1.0 is the untuned corner, not a claim."
- **Unit:** multiplier (dimensionless) on a want (adv_want): `want = amortize(wonder_want * usable / cost, walk + build) * gain`; the wonder_want operand is the wonder city's own building_want (already in want units).
- **Formula at the primary call site:** deeptrade.c:1704-1707 deep_wonder_merit_want(): `want = amortize((adv_want)(wc->wonder_want * wc->usable / (double) wc->cost), MAX(wc->walk, 0) + MAX(wc->build, 0)) * tune->wonder_merit_gain;`
- **All call sites:**
  - deeptrade.c:1707 -- deep_wonder_merit_want -- scales the help-wonder caravan want -- live at compiled defaults? no: dead behind wonder_merit=0
- **Log line:** deep-wonder-merit (merit=)
- **Interacts with:** wonder_merit, wrace_urgency_gain (multiplied after), MORT
- **Tier/screen:** Tier C; screen: dead-path (merit pair)
- **Prior evidence:**
  - R2b (STATUS.md:7670, 7922): measured gain ~5; "1.0 is measurably too cold". See wonder_merit for the two arms (null on score, |t| <= 0.7, n=20). pv3.ini 1.91.

### pay_switch_penalty
- **Declared:** deeptuning.h:2330 (`int pay_switch_penalty;`); params.ini: default=1 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "makes deep ISSUE ITSELF the [change-production] handicap in its own phase hook... the seat simply stops being exempt, so the dock becomes true pricing and the shields a switch strands are really stranded. Compiled default 1 by human call, 2026-08-06: honest but expensive."
- **Unit:** boolean gate (0/1): sets BV H_PRODCHGPEN on the seat's handicap vector; the cost itself is the server's city_change_production_penalty().
- **Formula at the primary call site:** deepai.c:616-624 deep_impose_switch_penalty(): `if (deep_tuning_get()->pay_switch_penalty <= 0 || is_human(pplayer) || ... || has_handicap(pplayer, H_PRODCHGPEN)) return; BV_SET(*handicaps, H_PRODCHGPEN);`
- **All call sites:**
  - deepai.c:616 -- deep_impose_switch_penalty -- imposes the handicap on the deep seat each phase -- live at compiled defaults? yes (default 1)
  - deepai.c:1042 -- dpai_build_adv_adjust -- with switch_cost_price>0, hides H_PRODCHGPEN from dai_build_adv_adjust() so stock's dock stays /15 not /4 -- live at compiled defaults? yes (both default 1)
- **Log line:** deep-payswitch (deepai.c:629; once per seat per game), deep-prodswitch pen=1
- **Interacts with:** switch_cost_price, switch_cost_gain
- **Tier/screen:** Tier C; screen: not screened (mode/gate; knobcensus S4 "mode", tag deep-payswitch)
- **Prior evidence:**
  - HEDGE (STATUS.md:8137-8200, 2026-08-06) -- contested, 20 paired seeds, pin 52e293e7aba5: arm B (paid, unpriced) vs A (exempt): score 79.47 -> 76.79, -2.69 (t -2.26); unitsbuilt -11.58 (t -8.84); forfeits 3470 shields/game (22% of output). Negative; shipped ON by human call ("honest but expensive").

### switch_cost_price
- **Declared:** deeptuning.h:2360 (`int switch_cost_price;`); params.ini: default=1 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Whether the build auction PRICES the switch it is about to make... debits every non-emergency candidate that would change production CLASS by the shields city_change_production_penalty() says the change really costs, converted at SHIELD_WEIGHTING... a candidate in the incumbent's own class is charged nothing."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepdefence.c:7906 deep_switch_cost_apply(): `if (deep_tuning_get()->switch_cost_price <= 0 || choice->want <= 0 || tier == DWT_EMERGENCY || !deep_choice_target(...) || are_universals_equal(&target, &pcity->changed_from)) return 0;`
- **All call sites:**
  - deepdefence.c:7906 -- deep_switch_cost_apply -- whole debit pass on both sides of the merge (called :8811-8812) -- live at compiled defaults? yes (default 1)
  - deepai.c:1041 -- dpai_build_adv_adjust -- hides H_PRODCHGPEN from stock's dock (needs pay_switch_penalty>0 too) -- live at compiled defaults? yes
- **Log line:** deep-switchcost (deepdefence.c:7924), deep-switchflip, deep-switchhold
- **Interacts with:** pay_switch_penalty, switch_cost_gain, scale_* (applied after scale), prod_keep_gate (applied after)
- **Tier/screen:** Tier C; screen: not screened as a gate (switch_cost_gain is the screened continuous knob: 1358 switch prices/game)
- **Prior evidence:**
  - HEDGE (STATUS.md:8137-8200) -- contested, 20 paired seeds: arm C (paid and priced) vs B (paid, unpriced): score 76.79 -> 79.12, +2.33 (t +1.87); vs A (exempt) -0.36 (t -0.24); class-changing switches 629.0 -> 377.6; wonders 0.09 -> 0.28 per seat (t +2.78 vs A). Positive-leaning, not significant on score; shipped ON.

### switch_cost_gain
- **Declared:** deeptuning.h:2361 (`double switch_cost_gain;`); params.ini: default=1.0 min=0.1 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "switch_cost_gain is the residue on the conversion, 1.0 = the shield's own weight, untuned. PLACEHOLDER." STATUS.md:8210: "the hottest untuned knob in the tree".
- **Unit:** multiplier (dimensionless) on shields converted to want at SHIELD_WEIGHTING (17 want per shield): `debit = forfeit(shields) * SHIELD_WEIGHTING * gain` (adv_want), subtracted from the candidate's want.
- **Formula at the primary call site:** deepdefence.c:7919-7921 deep_switch_cost_apply(): `debit = (adv_want)(forfeit * SHIELD_WEIGHTING * deep_tuning_get()->switch_cost_gain);` then `choice->want = MAX(choice->want - debit, 0);` (:7931)
- **All call sites:**
  - deepdefence.c:7921 -- deep_switch_cost_apply -- size of the forfeit debit on each non-emergency candidate that changes class -- live at compiled defaults? yes
- **Log line:** deep-switchcost (forfeit= stock= debit= want= left=), deep-switchflip, deep-switchhold
- **Interacts with:** switch_cost_price, pay_switch_penalty, scale_settler, scale_worker, scale_defender, scale_attacker, scale_ferry, unit_upkeep_charge, prod_keep_gain (all act on the same want, in the order scale -> endgame deflate -> unit upkeep -> switch cost -> prod keep -> mil_opportunity)
- **Tier/screen:** Tier A (AUCTION); screen: flips 274.7 decisions/game (20.2% of 1358 switch prices/game; sigma 0.483; replay rule switch_cost, 100.0% self-check)
- **Prior evidence:**
  - HEDGE (STATUS.md:8207-8215): at 1.0 the debit "zeroes 86.8% of the candidates it touches (debit p50 136 against a candidate want p50 41)"; 1788.1 debits/game; flagged for a tuning pass. No isolated arm moving the gain.
  - Campaign-moved: pv3.ini 0.9628.

### scale_settler
- **Declared:** deeptuning.h:2386 (`double scale_settler;`); params.ini: default=1.0 min=0.02 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "R3 -- one scale for what a city can do. The build auction compares bids that are not in the same unit: a weighted-output sum for a building, a percentage of danger for a defender, a trait-multiplied settler count for a founder, a kill-expectation for an attacker. Each key multiplies ONE producer class before the comparison. The BUILDING is the anchor and has no key... 1.0 everywhere is today's behaviour, byte-identically -- the scale is skipped, not multiplied."
- **Unit:** multiplier (dimensionless) on city_data->founder_want, an int in "want" units. NATIVE UNIT BEFORE SCALING: deep's site scorer output -- deepsite.c:2830 `city_data->founder_want = (result->virt_boat ? -result->result : result->result)` where result->result is the cityresult score from deep_find_best_city_placement() (negative = "and I need a boat first"). ai/default then divides it (/5 or /2 wonder city, /100 when capped, daidomestic.c:546-554), multiplies by trait/TRAIT_DEFAULT (:558), and vetoes on food; the scale is applied BEFORE those transforms and restored after the advisor returns (deepdefence.c:8748). Same "want" currency as building_want[] (a discounted output sum) in the sense that daidomestic.c:565 compares `founder_want > choice->want` directly; but the two are not calibrated to each other (that is RC4).
- **Formula at the primary call site:** deepdefence.c:8356 deep_scale_domestic_bids(): `city_data->founder_want = (int) (was * fs);` (fs = scale_settler; skipped when fs == 1.0 or founder_want == 0; int truncation). Called at deepdefence.c:8709 inside deep_city_choose_build(), just before `domestic_advisor_choose_build()` (daidomestic.c:~560-600 auction: founder_want vs building/worker want; result clamped to DAI_WANT_DOMESTIC_MAX = 750.075 at daidomestic.c:631-634).
- **All call sites:**
  - deepdefence.c:8347 -- deep_scale_domestic_bids -- reads scale_settler -- live at compiled defaults? yes as a read; the multiply is skipped at 1.0
  - deepdefence.c:8356 -- deep_scale_domestic_bids -- rescales the founder bid inside the domestic auction (settler vs building/worker) and, via the winner, the domestic-vs-military merge -- live at compiled defaults? no at compiled 1.0 (skipped, 0 deep-scale lines/game); yes at any other value
- **Log line:** deep-scale (deepdefence.c:8358: pick="founder" class=5 scale= want= to=); also deep-bid founder=, deep-founderbid, deep-founder
- **Interacts with:** switch_cost_gain, switch_cost_price, mil_opportunity_price (charged as price x the founder bid after the scale), founder_pool_per_site, founder_floor_want, founder_tier_gate, scale_worker (same advisor auction), expansion_war_survival
- **Tier/screen:** Tier A (AUCTION); screen: flips 114.2 decisions/game (1.6% of 7141 auctions/game; sigma 0.330; replay rule auction_domestic + merge, 100.0% self-check). Typical magnitudes: founder bids ~600-800 at T150 (defaults-deficit-census-2026-08-25.md:32), ~750 in the opening vs military ~175-235 (fearseat-verdict-2026-08-30.md:253); largest winning Settlers want in a probe game exactly 750.1 (deepdefence.c comment above deep_founder_site_capped).
- **Prior evidence:**
  - R3 (STATUS.md:8318-8430, 2026-08-06) -- arm S = scale_settler 0.20, pin 1044a6e97ff4, both arms pin pay_switch_penalty=1/switch_cost_price=1: contested 20 paired seeds score 79.12 -> +1.67 (t +0.80), settler picks -85.3 (t -5.27), cities -0.12 (t -1.28); isolated-150 at 40 seeds score -0.05 (t -0.02), cities -1.94 (t -4.34), mean_city_size +0.44 (t +3.50). "NO FOLD: safe (gate open) but score-neutral at 40 seeds; costs 13% of cities." Null.
  - Campaign-moved: pv3.ini 0.9245.

### scale_worker
- **Declared:** deeptuning.h:2387 (`double scale_worker;`); params.ini: default=1.0 min=0.05 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** Same R3 header: scale_settler and scale_worker "act where ai/default FORMS the bid (city_data->founder_want / worker_want, before domestic_advisor_choose_build() runs its own auction), because a factor on that advisor's winner cannot reorder the classes inside it."
- **Unit:** multiplier (dimensionless) on city_data->worker_want, an int in "want" units. NATIVE UNIT BEFORE SCALING: a terrain-improvement estimate formed in ai/default daicity.c:~800-849 (`want = (want - unit_food_upkeep*FOOD_WEIGHTING/2)*100/(40+foodbox_cost)` then reduced by existing workers; stored `city_data->worker_want = want`), later x `expand`/100 (daidomestic.c:507) and /5 or /2 for the wonder city. R3 measured it as bounded and small: worker_want p90 69-72, max 148-150 (STATUS.md:8365).
- **Formula at the primary call site:** deepdefence.c:8367 deep_scale_domestic_bids(): `city_data->worker_want = (int) (was * ws);` (ws = scale_worker; skipped at 1.0 or worker_want == 0); compared in the domestic advisor against the building want and founder_want.
- **All call sites:**
  - deepdefence.c:8348 -- deep_scale_domestic_bids -- reads scale_worker -- live at compiled defaults? yes as a read; multiply skipped at 1.0
  - deepdefence.c:8367 -- deep_scale_domestic_bids -- rescales the worker bid -- live at compiled defaults? no at compiled 1.0; yes at any other value
- **Log line:** deep-scale (deepdefence.c:8369: pick="worker" class=6 scale= want= to=); deep-bid worker=
- **Interacts with:** scale_settler (same auction; STATUS.md:8372 "two rescales that are each survivable are not jointly survivable"), switch_cost_gain
- **Tier/screen:** Tier A (AUCTION); screen: flips 51.6 decisions/game (0.72% of 7141 auctions/game; sigma 0.492; auction_domestic + merge)
- **Prior evidence:**
  - R3 (STATUS.md:8318-8430) -- arm W = scale_worker 3.0, contested 20 paired seeds: score -1.52 (t -1.17), worker picks 75 -> 196 (t +12.11). "the median-match premise is falsified for the worker"; NO FOLD, "measured harmful... and the poison in the bundle" (ALL = S+W+CAP: contested -3.79, t -1.52; isolated-150 20 seeds -24.30, t -6.28).
  - Campaign-moved: pv3.ini 1.946.

### scale_defender
- **Declared:** deeptuning.h:2388 (`double scale_defender;`); params.ini: default=1.0 min=0.05 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "scale_defender / scale_attacker / scale_ferry act on deep's own military choice at the merge seam. The EMERGENCY tier is exempt everywhere: it is the ruleset's must-act signal, and a deflator that leaked across it could lose a city."
- **Unit:** multiplier (dimensionless) on choice->want of a CT_DEFENDER military choice. NATIVE UNIT BEFORE SCALING: the military advisor's defender want as deep forms it (deep_military_choose_build, deepdefence.c:5444; fear-injected `danger` plus the garrison floor) -- header: "a percentage of danger for a defender"; honest first defender p50 ~100, defense building up to ~398 (deepwant.c comment in deep_want_scale_apply). Same adv_want type as the building anchor but a different quantity (RC4). Applies only in the NORMAL tier.
- **Formula at the primary call site:** deepwant.c:143 deep_want_class_scale(): `case DWC_DEFENDER: return t->scale_defender;` applied at deepwant.c:235 deep_want_scale_apply(): `choice->want = choice->want * scale;` (early return if tier == EMERGENCY, scale == 1.0 or want <= 0). Called at deepdefence.c:8792 in deep_city_choose_build() on `&(city_data->choice)` ("mil"), BEFORE endgame deflate (:8799), unit_upkeep_charge (:8807), switch_cost (:8811), prod_keep (:8820) and mil_opportunity (:8828).
- **All call sites:**
  - deepwant.c:143 -- deep_want_class_scale -- selects the key for DWC_DEFENDER -- live at compiled defaults? yes as a read; skipped at 1.0
  - deepwant.c:235 -- deep_want_scale_apply -- rescales the military defender bid before the domestic-vs-military comparison -- live at compiled defaults? no at compiled 1.0 (0 deep-scale lines/game)
- **Log line:** deep-scale (deepwant.c:228: class=1 scale=%.4f want= to=); deep-bid milclass=1; deep-want (final)
- **Interacts with:** garrison_floor_danger (it multiplies exactly the want that knob produced: tuning-blocks.ini:104-107 "one lever applied twice"), switch_cost_gain, unit_upkeep_charge, mil_opportunity_price, emergency_value_cap, scale_attacker, scale_ferry
- **Tier/screen:** Tier A (DEFOPS); screen: flips 42.75 decisions/game (1.2% of 3570.7 merges/game; sigma 0.492; replay rule auction_merge on deep-bid milclass). Was dead-path before the 2026-08-30 corpus retake (screen-corpus-2026-08-30.md:150). Compared at deep_merge_domestic_wins() (deepdefence.c:8507) against a domestic winner clamped to <= 750.075; typical military bids ~175-235 early (fearseat-verdict-2026-08-30.md:253).
- **Prior evidence:**
  - R3 (STATUS.md:8396): "scale_defender NO FOLD: no derivation; ratios invert" -- no arm ever moved it alone (R3 arms were S, W, CAP, ALL=S+W+CAP). Screen corpus 3-game smoke 83.0 -> 40-game 42.75 flips/game.
  - Campaign-moved: pv3.ini 1.503 (whole-vector campaign rejected on holdout, tune-postfix-verdict-2026-08-25.md).

### scale_attacker
- **Declared:** deeptuning.h:2389 (`double scale_attacker;`); params.ini: default=1.0 min=0.05 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** Same R3 header as scale_defender: "a kill-expectation for an attacker"; acts "on deep's own military choice at the merge seam"; emergency tier exempt.
- **Unit:** multiplier (dimensionless) on choice->want of a CT_ATTACKER choice whose unit type has transport_capacity == 0. NATIVE UNIT BEFORE SCALING: the stock kill_something_with / military advisor attacker want ("a kill-expectation", header) as filed by dai_choose_attacker; in the NORMAL tier these are far below the emergency spikes (emergency attacker p50 6485, max 259976, deepwant.c comment). Different quantity from the building anchor (RC4).
- **Formula at the primary call site:** deepwant.c:145 deep_want_class_scale(): `case DWC_ATTACKER: return t->scale_attacker;` applied at deepwant.c:235 `choice->want = choice->want * scale;` via deepdefence.c:8792 (same pipeline as scale_defender).
- **All call sites:**
  - deepwant.c:145 -- deep_want_class_scale -- selects the key for DWC_ATTACKER (CT_ATTACKER, transport_capacity == 0) -- live at compiled defaults? yes as a read; skipped at 1.0
  - deepwant.c:235 -- deep_want_scale_apply -- rescales the attacker bid -- live at compiled defaults? no at compiled 1.0
- **Log line:** deep-scale (class=2); deep-bid milclass=2
- **Interacts with:** attack_value_weight, conquest_worth_mult (what produces the want), switch_cost_gain, mil_opportunity_price, scale_defender, scale_ferry, emergency_value_cap
- **Tier/screen:** Tier A (DEFOPS); screen: flips 112.90 decisions/game (3.2% of 3570.7 merges/game; sigma 0.492; auction_merge). Dead-path before the corpus retake.
- **Prior evidence:**
  - R3 (STATUS.md:8397): "scale_attacker NO FOLD: no derivation; ratios invert" -- never moved alone in an arm. pv3.ini 1.512 (campaign-evolved).

### scale_ferry
- **Declared:** deeptuning.h:2390 (`double scale_ferry;`); params.ini: default=1.0 min=0.05 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** Same R3 header; the ferry class is a transport-capable hull -- "the attacker ferry and the colony lift, which file as CT_ATTACKER" (knobcensus MAP, deepwant.c:~85-99).
- **Unit:** multiplier (dimensionless) on choice->want of a CT_ATTACKER choice whose unit type has transport_capacity > 0. NATIVE UNIT BEFORE SCALING IS MIXED (UNSURE which dominates): (a) the stock attacker-ferry want (kill-expectation family); (b) deep's colony-lift bid `amortize(colony_lift_weight * lift_demand, build_turns)` (deepdefence.c:6463) where lift_demand = dp->colony_want / (1 + boat_idle_damping*idle), i.e. founder-site (settler-scorer) units; (c) the amphibious-muster berth shortfall `lift_demand = MAX(lift_demand, worth)` (city worth). The three sources are three different currencies filed under one class.
- **Formula at the primary call site:** deepwant.c:147 deep_want_class_scale(): `case DWC_FERRY: return t->scale_ferry;` applied at deepwant.c:235 `choice->want = choice->want * scale;` via deepdefence.c:8792.
- **All call sites:**
  - deepwant.c:147 -- deep_want_class_scale -- selects the key for DWC_FERRY -- live at compiled defaults? yes as a read; skipped at 1.0
  - deepwant.c:235 -- deep_want_scale_apply -- rescales the ferry/lift bid -- live at compiled defaults? no at compiled 1.0
- **Log line:** deep-scale (class=3); deep-bid milclass=3; deep-colony (the lift producer)
- **Interacts with:** colony_lift_weight, boat_idle_damping, lift_op_slots, scale_attacker, switch_cost_gain, mil_opportunity_price
- **Tier/screen:** Tier A (DEFOPS); screen: flips 16.15 decisions/game (0.45% of 3570.7 merges/game; sigma 0.492; auction_merge). Dead-path before the retake.
- **Prior evidence:**
  - R3 (STATUS.md:8398): "scale_ferry NO FOLD: no derivation; ratios invert" -- never moved alone. pv3.ini 0.8949 (campaign-evolved).

### emergency_value_cap
- **Declared:** deeptuning.h:2400 (`double emergency_value_cap;`); params.ini: default=0.0 min=0.0 max=2000.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The same disease one tier up. Inside the EMERGENCY tier priority is still a magnitude, so a kill_something_with attacker at 259976 outranks a grave-danger defender at 100 by three orders. This caps the emergency VALUE -- the tier itself is never touched... 0 = no cap = today's behaviour."
- **Unit:** want/score units (absolute ceiling): compared against w.value = choice->want - 1000 (DAI_WANT_BELOW_MIL_EMERGENCY) inside the emergency tier. Range 0-2000; 0 = off.
- **Formula at the primary call site:** deepwant.c:209-217 deep_want_scale_apply(): `if (cap > 0 && w.value > cap) { ... w.value = cap; deep_want_apply(choice, w); }` (cap read at :171).
- **All call sites:**
  - deepwant.c:171 -- deep_want_scale_apply -- reads the cap -- live at compiled defaults? yes as a read
  - deepwant.c:209 -- deep_want_scale_apply -- clamps emergency-tier value; the tier is unchanged -- live at compiled defaults? no at compiled 0 (no CAP lines); census line deep-milcap is emitted regardless
- **Log line:** deep-milcap (deepwant.c:201; every emergency pick: pre= cap= bind=), deep-scale scale=CAP (:211; only when it binds)
- **Interacts with:** scale_defender, scale_attacker, scale_ferry, DAI_WANT_BELOW_MIL_EMERGENCY (1000.0, fixed)
- **Tier/screen:** Tier C pinned (tuning-blocks.ini RESCREEN note: flips 51.3/game but "tier C DESPITE flipping"); screen: flips 51.27 decisions/game (1.3% of 4008.9; sigma 333.3; rule emergency_cap, 100.0% on 17,530 checks) BUT inert at the merge: the merge is tier-first and a domestic want is clamped to 750.075 and always NORMAL (knob-screen-2026-08-30.md:117); the flips are "an input to a decision and not a decision"
- **Prior evidence:**
  - R3 (STATUS.md:8384-8399) -- arm CAP = emergency_value_cap 400, contested 20 paired seeds: 22.1 clamps/game, 19/20 games identical, score -0.21 (t -1.00), cities +0.00. "executes, provably inert at 400". Null.
  - W25 (STATUS.md:23056): "cannot move the domestic/military merge at all". pv3.ini 56.31 (campaign-evolved).

### endgame_horizon
- **Declared:** deeptuning.h:2707 (`int endgame_horizon;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "endgame_horizon = 1 makes deep's growth ladder plan over MIN(MORT, end_turn - turn) instead of MORT, on BOTH the benefit and the upkeep side, and refuse a credit whose first instalment lands after the last turn. It is a correctness clamp, not a strategy: arithmetically inert on every turn more than MORT from the end... 0 = off = byte-identical."
- **Unit:** boolean gate (0/1) selecting whether deep_horizon_turns() returns MIN(MORT, turns left) or MORT.
- **Formula at the primary call site:** deephorizon.c:182-195 deep_horizon_turns(): `if (deep_tuning_get()->endgame_horizon <= 0) return MORT; left = deep_turns_left(); if (left >= MORT) return MORT; deep_horizon_clamped++; return left;` and :209 deep_arrives_in_time(): `if (...endgame_horizon <= 0) return TRUE;`
- **All call sites:**
  - deephorizon.c:182 -- deep_horizon_turns -- horizon clamp for ~20 consumers (growth ladder steps, content/multiplier credits, building upkeep bill at deephorizon.c:2394, gov dur deepgov.c:536, unit lifetime deepdefence.c:7825, ...) -- live at compiled defaults? no: identity (returns MORT) at 0
  - deephorizon.c:209 -- deep_arrives_in_time -- refuses credits that first pay after the last turn -- live at compiled defaults? no: returns TRUE at 0
- **Log line:** deep-endgame (deephorizon.c:2598: clamps= missed=)
- **Interacts with:** endgame_window, upkeep_gold_weight, gov_valuation, unit_upkeep_charge, horizon_* weights, horizon_decay_*, MORT
- **Tier/screen:** Tier C; screen: dead-path (endgame_* (4), knob-screen-2026-08-30.md:135)
- **Prior evidence:**
  - ENDGAME (STATUS.md:9420-9600, 2026-08-06) -- CLAMP arm (endgame_horizon=1 alone), 20 paired seeds, pin cc1fab22cea8: lategame-80 score 169.47 -> +0.68 (t +1.37), pop +0.24 (t +2.40); isolated-150 184.06 -> +0.25 (t +0.19). "fold CANDIDATE: it never measured negative on either cell"; not folded (20 seeds = direction; fairness: stock does not know the game ends). Null/positive-leaning.
  - Campaign-moved: pv3.ini 1.

### endgame_window
- **Declared:** deeptuning.h:2735 (`int endgame_window;`); params.ini: default=0 min=0 max=40 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "how many of the final turns deep plays for the scoreboard rather than for the empire: inside it, a build that cannot be FINISHED before the bell earns nothing it can spend, and a great wonder that can be finished is worth its five score. 0 = off = byte-identical; the window is a PLACEHOLDER and its size is measured, not argued."
- **Unit:** turns (int): `window > 0 && deep_turns_left() < window`.
- **Formula at the primary call site:** deephorizon.c:352-354 deep_in_endgame_window(): `int window = deep_tuning_get()->endgame_window; return window > 0 && deep_turns_left() < window;`
- **All call sites:**
  - deephorizon.c:352 -- deep_in_endgame_window -- gate for deep_endgame_bias_buildings() (:1975) and deep_endgame_unit_deflate() (deepdefence.c:7721) -- live at compiled defaults? no: false at 0
- **Log line:** deep-endgame (refused= paid=), deep-endgame-unit (deepdefence.c:7733), deep-endgame-impr
- **Interacts with:** endgame_unfinished_gain, endgame_wonder_gain, endgame_horizon
- **Tier/screen:** Tier C; screen: dead-path (endgame_* (4))
- **Prior evidence:**
  - ENDGAME (STATUS.md:9455-9530) -- MODE arm (eg-mode.ini: endgame_horizon 1 + window 15 / unfinished 0.0 / wonder 3.0, so it includes the clamp), 20 paired seeds: lategame-80 score +0.45 (t +0.86); isolated-150 +1.55 (t +0.90), wonders +0.14 (t +2.60), unitsbuilt +3.94 (t +5.47). Coupled triple; "fold nothing yet". Null on score.
  - Campaign-moved: pv3.ini 7.

### endgame_unfinished_gain
- **Declared:** deeptuning.h:2736 (`double endgame_unfinished_gain;`); params.ini: default=1.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "multiplies a candidate whose city_turns_to_build() exceeds the turns that remain. 1.0 is off (today's behaviour); 0.0 refuses it outright. It is a DEFLATOR and the emergency tier is exempt."
- **Unit:** multiplier (dimensionless 0-1) on a want (adv_want).
- **Formula at the primary call site:** deephorizon.c:1999,2013 deep_endgame_bias_buildings(): `mult = tune->endgame_unfinished_gain; ... add = want * (mult - 1.0); building_want[id] += add;` and deepdefence.c:7716,7731 deep_endgame_unit_deflate(): `choice->want = choice->want * mult;`
- **All call sites:**
  - deephorizon.c:1999 -- deep_endgame_bias_buildings -- deflates an unfinishable building -- live at compiled defaults? no: dead behind endgame_window=0 (and mult==1.0 is skipped)
  - deepdefence.c:7716 -- deep_endgame_unit_deflate -- deflates an unfinishable unit choice on both sides of the merge (called :8799-8801) -- live at compiled defaults? no: same gates (mult == 1.0 || !window)
- **Log line:** deep-endgame (refused=), deep-endgame-unit (deepdefence.c:7733), deep-endgame-impr
- **Interacts with:** endgame_window, endgame_wonder_gain, scale_* (applied after scale), switch_cost_gain
- **Tier/screen:** Tier C; screen: dead-path (endgame_* (4)); knob-screen-2026-08-30.md:152 "log tag never appeared, so the mechanism is off, not insensitive"
- **Prior evidence:**
  - ENDGAME (STATUS.md:9541-9575): DEFLATE arm (eg-deflate.ini: endgame_horizon 1 + window 15 / unfinished 0.0 / wonder 1.0), lategame-80 20 seeds: score +0.67 (t +1.61), wonders -0.04 (t -2.03); "The deflator has never been run at a gentler setting than 0.0" (zeroes a 750 founder as readily as a Trireme). pv3.ini 0.9844.

### endgame_wonder_gain
- **Declared:** deeptuning.h:2737 (`double endgame_wonder_gain;`); params.ini: default=1.0 min=1.0 max=8.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "multiplies a great wonder that CAN still be finished. get_civ_score() pays 5 per great wonder against 1 per citizen and 2 per tech... 1.0 is off."
- **Unit:** multiplier (dimensionless >= 1.0, range 1-8) on a wonder building want.
- **Formula at the primary call site:** deephorizon.c:2001,2013 deep_endgame_bias_buildings(): `mult = tune->endgame_wonder_gain; add = want * (mult - 1.0); pcity->server.adv->building_want[id] += add;`
- **All call sites:**
  - deephorizon.c:2001 -- deep_endgame_bias_buildings -- premium on a finishable great wonder -- live at compiled defaults? no: dead behind endgame_window=0 and a no-op at 1.0
- **Log line:** deep-endgame (paid=), deep-endgame-impr
- **Interacts with:** endgame_window, endgame_unfinished_gain, wonder_merit (city_turns_to_build does not model caravans: STATUS.md:9565)
- **Tier/screen:** Tier C; screen: dead-path (endgame_* (4))
- **Prior evidence:**
  - ENDGAME (STATUS.md:9455-9530): MODE arm premium 3.0 on isolated-150: wonders +0.14 (t +2.60), in-window completions +28%; on lategame-80 the premium bought back "exactly what the deflator took, 0.17 -> 0.20". pv3.ini 4.361.

### buy_per_gold
- **Declared:** deeptuning.h:2876 (`int buy_per_gold;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "buy_per_gold = 1 ranks the loop by want per gold instead [of raw want]. The triage is deliberately untouched... 0 = off = byte-identical."
- **Unit:** boolean gate (0/1): switches the buy-list ranking key from want to want / buycost.
- **Formula at the primary call site:** deepdefence.c:7435 deep_spend_gold(): `if (deep_tuning_get()->buy_per_gold > 0) { /* DARKFX: rank by want per GOLD ... */ double bestkey = 0.0; int besttier = -1; ...`
- **All call sites:**
  - deepdefence.c:7435 -- deep_spend_gold -- buy-list ranking key -- live at compiled defaults? no: dead behind buy_per_gold=0 (default branch ranks by want)
- **Log line:** deep-buy (deepdefence.c:7575, 7601; emitted ungated; no field says which comparator picked, per knobcensus)
- **Interacts with:** sell_defend_gain, late_buy_gate (other gold spenders), cached_limit reserve
- **Tier/screen:** Tier C; screen: not screened (knobcensus MAP: "-" no rank marker on deep-buy)
- **Prior evidence:**
  - DARKFX (STATUS.md:9655-9660, findings/darkfx-2026-08-06.md:79) -- dark-buy.ini, 20 paired seeds: isolated-150 184.06 -> +5.58 (t +1.64, 13/20); contested 82.44 -> -0.12 (t -0.08, 10/20). Null.
  - pv3.ini keeps buy_per_gold = 0.

### prod_keep_gate
- **Declared:** deeptuning.h:3316 (`int prod_keep_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "!= 0 credits the candidate that IS what the city is already building, in proportion to how far that build has got: want *= 1 + prod_keep_gain * min(stock, cost) / cost... The EMERGENCY tier is exempt... 0 (default) = every build re-bid from scratch every turn."
- **Unit:** boolean gate (0/1).
- **Formula at the primary call site:** deepdefence.c:7996 deep_prod_keep_credit(): `if (tune->prod_keep_gate <= 0 || tune->prod_keep_gain <= 0.0 || choice->want <= 0 || pcity->shield_stock <= 0 || tier == DWT_EMERGENCY || ... || !are_universals_equal(&target, &pcity->production)) return 0;`
- **All call sites:**
  - deepdefence.c:7996 -- deep_prod_keep_credit -- completion-bias entry gate (called :8820-8822 on both sides) -- live at compiled defaults? no: dead behind prod_keep_gate=0
- **Log line:** deep-prodkeep (deepdefence.c:8024), keep= on deep-prodswitch (deepdefence.c:8088)
- **Interacts with:** prod_keep_gain, switch_cost_price/gain, scale_*, mil_opportunity_price
- **Tier/screen:** Tier C; screen: dead-path (knob-screen-2026-08-30.md:145 "prod_keep_gain/prod_keep_gate")
- **Prior evidence:**
  - S-PRODKEEP (STATUS.md:22555-22610, 2026-08-25; findings/prodkeep-verdict-2026-08-25.md) -- contested, 20 TRAINING seeds, postfix-start.ini base: on-off = -3.8 (SE 9.1, t -0.42, 8/20); "A wash"; gate stays 0 (82% of abandoned settlers were not candidates in that auction). Rejected.

### prod_keep_gain
- **Declared:** deeptuning.h:3317 (`double prod_keep_gain;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "prod_keep_gain is the residue on that shape -- 1.0 = one full bid-share of extra weight at a finished box. PLACEHOLDER, untuned." Credited want is clamped below DAI_WANT_BELOW_MIL_EMERGENCY.
- **Unit:** multiplier (dimensionless): `credit = want * gain * progress`, progress = min(stock, cost)/cost in [0,1]; credit in want units, clamped to 999.
- **Formula at the primary call site:** deepdefence.c:8015 deep_prod_keep_credit(): `credit = (adv_want) (choice->want * tune->prod_keep_gain * progress);` then `choice->want = choice->want + credit;` (:8033), clamped so want + credit <= DAI_WANT_BELOW_MIL_EMERGENCY - 1.
- **All call sites:**
  - deepdefence.c:7997 -- deep_prod_keep_credit -- gate part (gain <= 0 returns) -- live at compiled defaults? no: dead behind prod_keep_gate=0
  - deepdefence.c:8015 -- deep_prod_keep_credit -- credit size -- live at compiled defaults? no: same
  - deepdefence.c:8031 -- deep_prod_keep_credit -- echoed on deep-prodkeep (read only) -- live at compiled defaults? no: same
- **Log line:** deep-prodkeep (credit= gain=)
- **Interacts with:** prod_keep_gate, switch_cost_gain (applied before), scale_* (applied before)
- **Tier/screen:** Tier C; screen: dead-path (knob-screen-2026-08-30.md:145,152)
- **Prior evidence:**
  - S-PRODKEEP (STATUS.md:22600-22605): "FLAGGED FOR TUNING (rule 7): prod_keep_gain = 1.0 is a PLACEHOLDER... Do NOT tune it against this result -- what failed is the mechanism's reach". Only moved together with the gate (on-off -3.8, t -0.42, n=20).

## Unresolved

Facts found while compiling that do not fit a single entry, and items I could not settle. No recommendations.

1. **`trade_partner_target_discount` has no effect in the current source.** The only read is deepops.c:3129 (`spare = ... > 0.0`), which gates a `deep_routes_with_players()` call and sets a local `partner` flag (:3155) that is never read afterwards. The multiply `score *= 1.0 - trade_partner_target_discount; trade_partner_spared++` was removed by commit 2a80901ceb ("ai/deep: price an ally's attack request into the operation target score"); the comment block describing it (deepops.c:~3187-3195) remains, `dp->trade_partner_spared` (deepai.h:287) is printed on `deep-leaders` (deepdiplomacy.c:1545) but never incremented, and `benchmark/knobcensus.py` still cites "deepops.c:2644-2645". The TRADE entry's 44.67 applications/game probe (STATUS.md:9181) predates the removal. Per CLAUDE.md "stop and ask": the plan/doc evidence and the code disagree.
2. **`wrace_*` decline is not an action.** `decline` in `deep_wonder_race()` is only counted and logged; the want changes only through `factor`, which is 1.0 unless one of `wrace_pwin_gain` / `wrace_eta_gain` / `wrace_obsolete_gain` is >0 (STATUS.md:17779-17793 states the same). At compiled defaults `wrace_margin_turns` therefore feeds only census output (`deep-wrace`, `deep-wrace-sum`, `deep-wrace-end`).
3. **Per-knob screen classes for the knobs not named in the screen docs are inferred, not read.** `knob-screen-2026-08-30.md` names the dead-path families only by group ("trade and wonder merit pair (6 knobs)", "rates (3)", "gov (3)", "`endgame_*` (4)", `unit_upkeep_charge`, `prod_keep_gain`/`prod_keep_gate`) and gives numbers only for the shortlist. For caravan_*, wrace_*, expiry_unit_*, sell_defend_gain, late_buy_gate, evacuate_gate, upkeep_unit_weight, trade_route_love, trade_partner_target_discount, pay_switch_penalty, switch_cost_price, buy_per_gold the table says "not screened"; their dead-at-defaults status comes from reading the code gates, not from a screen output. I ran `benchmark/knobscreen.py` (read-only, `--md --jobs 4`) on `benchmark/results/w25-screen-corpus/{econ,war}` under a 500 s timeout; it wrote nothing to stdout (stderr held only repeated `bad predicate: 'slack'` lines) and I did not investigate, so no new per-knob table was obtained. Exact membership of the "merit pair (6 knobs)" is my inference (trade_merit, trade_merit_gain, trade_ceasefire_peace_odds, trade_route_risk_gain, wonder_merit, wonder_merit_gain).
4. **Screen "flips" for the scale_* keys are replay flips at the default 1.0**, where the production code skips the multiply entirely (0 `deep-scale` lines/game at defaults, STATUS.md:8420). The 40-game corpus (`tuning=defaults.ini`) therefore measures what moving the key by +-1 sigma WOULD flip on logged inputs, not behaviour that executes at defaults.
5. **`scale_ferry` native unit is mixed.** The class holds three producers in three currencies (stock attacker-ferry kill-expectation; deep's colony lift `amortize(colony_lift_weight * lift_demand, bt)` with `lift_demand` from `dp->colony_want`, a founder-site quantity; the amphibious berth shortfall `MAX(lift_demand, worth)`, a city worth). I did not determine which dominates the 16.15 flips/game; `deep-bid milclass=3` plus `deep-colony` lines would separate them.
6. **`gov_rate_ceiling_gain` unit is UNSURE.** Its operand is a difference of two `deep_rates_pick().value` totals (v_sci+v_gold+v_lux, each already x MORT) added straight into the government `value` (output-delta x turns); the header says "nothing has measured that ratio".
7. **`upkeep_gold_weight` has two readings of its unit in the docs.** The header calls it "gold per turn against the horizon" (want per gold-turn, 0.125); content-military-gold-2026-08-30.md:132 reads it as "a gold is currently worth one eighth of a shield-equivalent" (SHIELD_WEIGHTING = 17 want per shield, so 1 gold = 0.125 want, not 0.125 x 17). The code (deephorizon.c:2397) multiplies gold x turns by the weight with no SHIELD_WEIGHTING factor, so the first reading matches the arithmetic; the "shield-equivalent" phrasing is the doc's.
8. **Normal-tier magnitudes of defender / attacker / ferry bids** are not stated per class in any comment or log reading I found; only the ~175-235 opening military figure and the emergency-tier populations (deepwant.c comment) are. A per-class distribution would come from `deep-bid mil= milclass=` lines in a corpus, which I did not open.
9. **All archived-vector citations (pv3.ini, decaytech-best.ini, derby-lean.ini, a6-best.ini)** show campaign-moved values only. The 2026-08-25 holdout rejected the tune-postfix vector (findings/tune-postfix-verdict-2026-08-25.md) and the 2026-07-30 head-to-head campaign shipped the compiled defaults (STATUS.md:760-770), so none of them is an attributable single-knob A/B.
10. **STATUS.md entry line numbers** were taken from `grep -n` at HEAD `b42d64d7a8` and are exact for the cited line but entry spans are approximate; entry names are the nearest preceding `## ` heading.

