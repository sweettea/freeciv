# Knob inventory F: diplomacy, civil messaging, honest-info (22 knobs)

Facts only. Sources read: `ai/deep/deeptuning.h`, `deeptuning.c`, `benchmark/tuning-params.ini`, `benchmark/tuning-blocks.ini`,
call sites in `ai/deep/*.c`, `doc/deep-ai/STATUS.md`, `findings/*.md`. Nothing built or run; the only analysis run was a `grep -c` of
log tags over the archived screen corpus (`benchmark/results/w25-screen-corpus/{econ,war}/holdout-report/cfg0`, compiled-default deep arm, 20 seeds each).

Conventions. "Compiled defaults" = `deeptuning.c` initialisers (they equal params.ini `default=` for all 22). The composed baseline
`benchmark/tuning/pv3.ini` (also pv4/pv3-lean/derby-lean) overrides several of these knobs; where it does, the section says so, because
"live at compiled defaults" and "live in the baseline arm" differ for the gift/proxy family and for `peace_seek_gain` (1.0 compiled, 0.801516 in pv3).
`tune`-side: none of the 22 knobs is in tier A (AUCTION, DEFOPS) or tier B (BFEAR, BASSAULT, BSITE, BCIVIL) in `tuning-blocks.ini`; all are therefore
"unranked / tier C (pinned)" by that file's definition. Block memberships found by parsing the file are listed per knob.
Tuning pointer: every site uses `deep_tuning_get()` (global) except the chat family in `deepchat.c`/`deeprequest.c`/`deeptrunk.c`, which uses
`deep_tuning_for(pplayer)` (per-player tuning).

## Summary table

| knob | unit | default | live at defaults? | tier | screen verdict | prior A/B | # call sites |
|---|---|---|---|---|---|---|---|
| embassy_science_value | want units (adv_want) | 8.0 | yes (civ2civ3 only; needs tech_leakage=Embassies and stock offensive-diplomat want>0) | unranked (G, GDIPLO, ECON, ALL) | not named in knob-screen doc; knobcensus: NO-INSTRUMENT for the want-side site | null (bundled in GDIPLO ablation only) | 3 |
| peace_seek_gain | multiplier (dimensionless fraction 0-1 of the love bar waived) | 1.0 | yes | unranked (G, GDIPLO, ECON, ALL) | dead-path in knob-screen Part 1 (see Unresolved: tag IS present in the compiled-default retake corpus) | positive (weak, t about 1.5, n=20) | 5 |
| diplo_deep_only_pacts | boolean gate | 0 | no: dead behind itself =0 | unranked (ALL) | not screened | none | 1 (+2 via helper) |
| embassy_price_gain | multiplier (dimensionless) on a gold figure; also a gate at 0 | 0.0 | no: dead behind itself =0 | unranked (G, GDIPLO, ECON, ALL) | not screened (dead-path family) | null (n=20, EMBASSY arm) | 6 |
| embassy_give_gain | multiplier (dimensionless) | 1.0 | no: dead behind embassy_price_gain=0 | unranked (G, GDIPLO, ECON, ALL) | not screened | none | 1 |
| gift_border_value | gold per danger unit (exchange rate); also a gate at 0 | 0.0 | no: dead behind itself =0 | unranked (G, GDIPLO, ECON, ALL) | dead-path (gift family, Part 1 prose) | positive (bundled with gift_love_gain, self-play only) | 4 |
| gift_love_gain | multiplier (dimensionless) on a gold-denominated relationship worth | 0.0 | no: dead behind itself =0 | unranked (G, GDIPLO, ECON, ALL) | dead-path (gift family) | positive self-play / transfer in mixed | 3 |
| gift_ally_gain | multiplier (dimensionless) | 1.0 | no: dead behind gift_love_gain=0 | unranked (ALL) | dead-path (gift family) | none | 1 |
| gift_instrument_gain | multiplier (dimensionless) | 1.0 | no: dead behind gift_love_gain=0 | unranked (ALL) | dead-path (gift family) | none | 1 |
| gift_rival_gain | multiplier (dimensionless) | 1.0 | no: dead behind gift_love_gain=0 | unranked (ALL) | dead-path (gift family) | null (pv3 fold, t +0.74) | 1 |
| proxy_arm_gain | multiplier (dimensionless) on a gold budget | 0.0 | no: dead behind itself =0 | unranked (G, GDIPLO, ECON, MIL, ALL) | not screened (dead-path family) | null (0 offers ever made) | 3 |
| honest_info | boolean gate | 0 | no: dead behind itself =0 | unranked (ALL) | not screened | mixed (n=84 games; not significant) | 1 |
| chatty_gate | boolean gate | 0 | no: dead behind itself =0 | unranked (ALL) | dead-path (chatty family) | none (output-only; liveness smoke) | 3 |
| chatty_max_per_turn | count (chat lines per turn) | 3 | no: dead behind chatty_gate=0 | unranked (ALL) | dead-path (chatty family) | none | 4 |
| chatty_inbound_gate | boolean gate | 0 | no: dead behind itself =0 | unranked (ALL) | dead-path (chatty family) | none | 6 |
| chatty_request_turns | turns | 10 | no: dead behind chatty_inbound_gate=0 | unranked (ALL) | dead-path (chatty family) | none | 2 |
| chatty_defend_fear | want/score units (danger-ledger currency added to city_data->danger) | 50.0 | no: dead behind chatty_inbound_gate=0 | unranked (ALL) | dead-path (chatty family) | none | 1 (+2 consumers) |
| chatty_attack_gain | multiplier (dimensionless; score x (1+gain)) | 0.5 | no: dead behind chatty_inbound_gate=0 | unranked (ALL) | dead-path (chatty family) | none | 1 (+2 consumers) |
| chatty_road_gain | multiplier (dimensionless; distance / (1+gain)) | 1.0 | no: dead behind chatty_inbound_gate=0 and trunk_ally_gate | unranked (ALL) | dead-path (chatty family) | none | 2 |
| diplo_price_gate | boolean gate | 0 | no: dead behind itself =0 | unranked (M, MIL, ALL) | not screened | none (parked, 10/96 games) | 1 |
| diplo_evidence_floor | count (foreign diplomat-class units) | 0 | no: dead behind diplo_price_gate=0 | unranked (M, MIL, ALL) | not screened | none | 1 |
| diplo_gap_floor | count (techs) | 0 | no: dead behind diplo_price_gate=0 | unranked (M, MIL, ALL) | not screened | none | 1 |

Counts: 22 knobs; 20 dead at compiled defaults; 2 live (embassy_science_value, peace_seek_gain); 0 UNSURE units.

## Per-knob sections

### embassy_science_value
- **Declared:** deeptuning.h:1300 (`double embassy_science_value;`); params.ini: default=8.0 min=0.0 max=50.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "extra diplomat build-want per met non-allied player we lack an embassy with, active only under Embassies tech leakage (each missing embassy is forgone research discount). 0 disables."
- **Unit:** want units (adv_want). It is added directly to `dip->want`, the stock offensive-diplomat build want, per missing embassy (deepdefence.c:6057). deepspy.c:186 reuses it as the "embassy" action price in the "want currency" that is compared to the other spy-action values (steal = bulbs x3 x spy_gold_value, etc.), so the same number is also on a want scale there.
- **Formula at the primary call site:** deepdefence.c:6057 `deep_military_choose_build`: `dip->want += missing * deep_tuning_get()->embassy_science_value;` (guarded at :6004-6006 by `dip->want > 0 && game.info.tech_leakage == TECH_LEAKAGE_EMBASSIES && embassy_science_value > 0.0`; `missing` counts alive, met, non-allied players without an embassy that are reachable by boat or shared continent, and not claimed by the treaty channel).
- **All call sites:**
  - deepdefence.c:6006 - deep_military_choose_build - gate on the whole diplomat-want bump (`> 0.0`) - yes at defaults, but only when game.info.tech_leakage == Embassies (civ2civ3) and stock's offensive diplomat want is already > 0
  - deepdefence.c:6057 - deep_military_choose_build - adds `missing * value` to the diplomat build want - same condition as above
  - deepspy.c:186 - deep_spy_action_value - prices the ACTRES_ESTABLISH_EMBASSY spy action (`value = tune->embassy_science_value`) - evaluated and counted in the `deep-spy` census at defaults; it only changes any unit's action when spy_enabled>0 (deepspy.c:458 `if (!on) continue;`; spy_enabled default 0) so: no at defaults for effect
- **Log line:** `deep-spy` (census, deepspy.c:348) for the action-price site; the build-side bump has none (knobcensus.py marks it NO-INSTRUMENT: "diplomat want bump ... no line"). The following `deep-damper pump=diplomat` line (deepdefence.c:~6086) prints the want after this bump only when diplomat_pump_damper>0.
- **Interacts with:** embassy_price_gain (via deep_embassy_by_treaty() at deepdefence.c:6028, which removes treaty-reachable players from `missing` when embassy_price_gain>0), diplomat_pump_damper, spy_enabled, spy_gold_value, spy_sabotage_gain (the competing spy action values).
- **Tier/screen:** unranked (not in tier A/B). Blocks: G, GDIPLO, ECON, ALL. Knob-screen 2026-08-30: not named; knob-coverage.md:170 `NO-INSTRUMENT`.
- **Prior evidence:**
  - STATUS "W35 E35.1 spy actions" (line ~10932) - smoke, 40 deep seats, spy_enabled=1 - "embassy 0 is a finding": steal always outbids the 8.0 embassy price, deep established no embassy; placeholders on uncalibrated scales - n=10 games, census only
  - STATUS "pv4 holdout confirm" (line ~16535-16690) - isolated-150 (econ) and tech50war-200 (war), held-out seeds 101-116 - pv4d (drops all 13 GDIPLO knobs incl. this one to compiled defaults) minus pv4: econ -1.1 (SE 10.4, t -0.11, exact tie on 5/16 seeds); war -34.8 (SE 25.2, t -1.38) - n=16; knob not isolated
  - pv3.ini tunes it to 4.596145; the readback note (STATUS ~16687) uses it as the one GDIPLO key that survives the logger cut

### peace_seek_gain
- **Declared:** deeptuning.h:2443 (`double peace_seek_gain;`); params.ini: default=1.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the fraction of stock's own req_love_for_peace bar deep is willing to WAIVE for a partner it is not committed to fighting: 0.0 asks for the full stock bar ... which is why 0.0 is the off switch AND the honest control." Also a pre-pass on exactly the cease-fire turns where `turns_left` <= 4. Default 1.0 is a recorded human call 2026-08-06 ("remove the love bar, trade want anchors love"), paired with trade_route_love.
- **Unit:** multiplier (dimensionless). It enters as `(1.0 - peace_seek_gain)` scaling `ai->diplomacy.req_love_for_peace` (stock AI "love" scale, integer compared with `pplayer->ai_common.love[...]`). The gold-vs-love question does not arise: this is love-vs-love.
- **Formula at the primary call site:** deepdiplomacy.c:291 `deep_peace_wanted`: `*bar = (int) (ai->diplomacy.req_love_for_peace * (1.0 - t->peace_seek_gain));` then deepdiplomacy.c:~320 `else if (*love < *bar) { *why = "love"; }`.
- **All call sites:**
  - deepdiplomacy.c:291 - deep_peace_wanted - love bar for signing/proposing a durable peace - yes
  - deepdiplomacy.c:1646 - deep_treaty_evaluate (fence branch) - enables the exact `would=` honesty field only when `> 0.0` - no: only under diplo_deep_only_pacts=1
  - deepdiplomacy.c:1697 - deep_treaty_evaluate - deep accepts a lone CLAUSE_PEACE treaty by its own rule only when `> 0.0` - yes
  - deepdiplomacy.c:1757 - deep_diplomacy_prepass - all-levers-off early return - yes
  - deepdiplomacy.c:1767 - deep_diplomacy_prepass - enables deep_peace_seek() per player pair - yes
- **Log line:** `deep-peaceseek` (deepdiplomacy.c:535, `bar=` is the waived bar) and `deep-peacetake` (deepdiplomacy.c:1704). Observed in the compiled-default screen corpus: deep-peaceseek 1005 lines (econ) / 1054 (war), deep-peacetake 570 / 929, in 40/40 games.
- **Interacts with:** trade_route_love (shipped as a pair), diplo_deep_only_pacts, trade_ceasefire_peace_odds. pv3 value is 0.801516 (bar = req_love * 0.198), not 1.0.
- **Tier/screen:** unranked. Blocks: G, GDIPLO, ECON, ALL. Knob-screen Part 1: named "dead-path" ("their log tag never appeared, so the mechanism is off"). That conflicts with the retake corpus (see Unresolved); knobscreen.py:795 assumes `_base_peace_seek_gain` default 0.0 (the pre-fold default).
- **Prior evidence:**
  - STATUS "PEACE: the cease-fire trap halves" (line 8720) / findings/peace-diplomacy-2026-08-06.md - contested, 20 paired seeds, all-deep, SEEK arm (peace_seek 0 -> 1) - score +3.60 (t +1.48), techs +1.23; ALL arm +4.64 (t +2.14, 15/20)
  - same - lategame-80 - SEEK +1.92 (t +1.47, 13/20); ALL +2.23 (t +1.76) - n=20
  - STATUS "TRADE ... the fold" (line ~9130-9160) - lategame-80, 20 paired seeds - peace_seek 0 -> 1 with trade_route_love 0 -> 5: +1.93, SE 1.31, t +1.47, 13/20; third arm (peace_seek=1, trade_route_love=0) byte-identical to fold-on on 20/20 scorelogs, so the whole move is the bar removal
  - STATUS 8845: the timing vs bar-waiver decomposition (small-gain arm) "was never run"
  - STATUS "pv4 holdout confirm" - pv4d minus pv4 (13 GDIPLO knobs together) - econ -1.1 t -0.11; war -34.8 t -1.38 - n=16

### diplo_deep_only_pacts
- **Declared:** deeptuning.h:2489 (`int diplo_deep_only_pacts;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "When 1, a treaty partner who is an AI and is NOT running ai/deep gets nothing at peace or above: deep neither proposes nor accepts a treaty carrying CLAUSE_PEACE or CLAUSE_ALLIANCE" (cease-fire left open; humans exempt). "STRUCTURAL POLICY FLAG, not an optimiser knob."
- **Unit:** boolean gate (tested `> 0`).
- **Formula at the primary call site:** deepdiplomacy.c:346 `deep_fenced_partner`: `return deep_tuning_get()->diplo_deep_only_pacts > 0 && is_ai(aplayer) && aplayer->ai != pplayer->ai;`
- **All call sites:**
  - deepdiplomacy.c:346 - deep_fenced_partner - the single definition of the fence predicate - no: dead behind =0
  - (indirect) deepdiplomacy.c:515 - deep_peace_seek - drops a proposal deep decided to make - no: only under =1
  - (indirect) deepdiplomacy.c:1622 - deep_treaty_evaluate - vetoes (returns TRUE without accepting) any treaty with CLAUSE_PEACE/CLAUSE_ALLIANCE - no: only under =1
- **Log line:** `deep-fence` (deepdiplomacy.c:399; fields side, level, would, live, refused, wouldhave).
- **Interacts with:** peace_seek_gain (honesty field `would=` computed only if >0), gift_ally_gain (session note says fence narrows gift flow automatically).
- **Tier/screen:** unranked. Block: ALL only. Not screened (no row in knob-screen doc).
- **Prior evidence:** none (no entry in STATUS.md or findings/*.md; only the brief `doc/deep-ai/sessions/S-FENCE-deep-only-pacts.md` and `benchmark/tuning/fence-on.ini` = pv3 + `diplo_deep_only_pacts = 1`).

### embassy_price_gain
- **Declared:** deeptuning.h:2663 (`double embassy_price_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "a GATE and a residue in one number. At 0.0 deep does not touch embassy clauses and does not offer swaps ... Above 0.0 deep prices an embassy by what the leakage rule says it is worth" (per tech the other player knows and we do not, tech_leak_pct/players off that tech's price, valued at the current step's bulb cost, converted to gold at x3). The residue is for the undiscounted timing of the techs.
- **Unit:** multiplier (dimensionless) applied to a gold-equivalent figure (`DEEP_BULBS_TO_GOLD`=3.0 x bulbs x ahead x tech_leak_pct/(100 x players)). The resulting worth is gold-equivalent but is compared only to 0 (treaty balance of embassy clauses summed in the same treaty), i.e. it is a receive-vs-give balance, not a treasury, want or love comparison.
- **Formula at the primary call site:** deepdiplomacy.c:263 `deep_embassy_clause_worth`: `return t->embassy_price_gain * deep_embassy_leak_worth(pplayer, aplayer);` (giving side :259 `-t->embassy_price_gain * t->embassy_give_gain * deep_embassy_leak_worth(aplayer, pplayer)`); acceptance at deepdiplomacy.c:~1735 `if (balance >= 0.0)`.
- **All call sites:**
  - deepdiplomacy.c:259 - deep_embassy_clause_worth - giver-side clause worth - no: dead behind =0
  - deepdiplomacy.c:263 - deep_embassy_clause_worth - receiver-side clause worth - no
  - deepdiplomacy.c:1581 - deep_embassy_by_treaty - `<= 0.0` returns FALSE, so the diplomat channel keeps every player - the function is called at defaults (deepdefence.c:6028) and returns FALSE, i.e. inert
  - deepdiplomacy.c:1716 - deep_treaty_evaluate - deep rules on pure-embassy treaties - no
  - deepdiplomacy.c:1757 - deep_diplomacy_prepass - all-levers-off early return - yes (part of the guard)
  - deepdiplomacy.c:1778 - deep_diplomacy_prepass - enables deep_embassy_offer() - no
- **Log line:** `deep-embassy` (deepdiplomacy.c:1724; `worth=`, `accept=` is the running balance's sign).
- **Interacts with:** embassy_give_gain, embassy_science_value (channel split), peace_seek_gain (embassy treaties only at DS_PEACE/DS_ARMISTICE).
- **Tier/screen:** unranked. Blocks: G, GDIPLO, ECON, ALL. Not screened (dead-path family, no row).
- **Prior evidence:**
  - STATUS "PEACE" (line 8720) - contested, 20 paired seeds, EMBASSY arm (embassy_price_gain=1.0, give 1.0, peace_seek 0) - score +0.07 (t +0.04); embassy treaties 48.7 judged / 26.9 cleared per game - null
  - same - lategame-80 - EMBASSY +1.44 (t +2.18); 21.3 judged / 10.9 cleared - n=20
  - same - ALL arm (seek+embassy) contested +4.64 (t +2.14), lategame-80 +2.23 (t +1.76)
  - pv3.ini ships it at 0.0 (GDIPLO ablation pv4d-pv4 above is not informative for this knob since it was already 0)

### embassy_give_gain
- **Declared:** deeptuning.h:2664 (`double embassy_give_gain;`); params.ini: default=1.0 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the weight deep puts on the RIVAL's side of the same computation when giving. 1.0 says their discount hurts exactly as much as ours helps, so a swap clears only for whoever is behind on tech; below 1.0 deep is more willing to trade."
- **Unit:** multiplier (dimensionless), on the giver-side gold-equivalent embassy worth; same balance-vs-0 comparison as embassy_price_gain.
- **Formula at the primary call site:** deepdiplomacy.c:259-260 `deep_embassy_clause_worth`: `return -t->embassy_price_gain * t->embassy_give_gain * deep_embassy_leak_worth(aplayer, pplayer);`
- **All call sites:**
  - deepdiplomacy.c:259 - deep_embassy_clause_worth - weight on the clause deep gives away - no: dead behind embassy_price_gain=0 (product is 0)
- **Log line:** `deep-embassy` with `give=1` (deepdiplomacy.c:1724).
- **Interacts with:** embassy_price_gain.
- **Tier/screen:** unranked. Blocks: G, GDIPLO, ECON, ALL. Not screened.
- **Prior evidence:** none isolated. Moved only together with embassy_price_gain in the PEACE EMBASSY/ALL arms (both =1.0, which equals the compiled default); pv3.ini writes it as 0.0.

### gift_border_value
- **Declared:** deeptuning.h:2508 (`double gift_border_value;`); params.ini: default=0.0 min=0.0 max=500.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the exchange rate between the density lattice's units and gold, and the switch that turns the gift census on." Border pressure is in danger units; a treaty is in gold; "There is no read anywhere in this tree for the rate between them". 0.0 removes the border half of the peace valuation and stops the `deep-gift` census.
- **Unit:** gold per danger unit (exchange rate; multiplies `deep_border_pressure_from()`, whose "units are danger"). The product is a gold-equivalent relationship worth that is compared (after x gift_love_gain) to a tech's price in gold; it is not compared to the treasury or to a love scalar. (The treasury figure `deep_gift_budget` is only printed in `deep-gift`, deepdiplomacy.c:~886.)
- **Formula at the primary call site:** deepdiplomacy.c:444-445 `deep_peace_worth`: `double border = deep_border_pressure_from(pplayer, aplayer) * t->gift_border_value;` and return `trade + border` (trade = `deep_route_value_with() * MORT`).
- **All call sites:**
  - deepdiplomacy.c:445 - deep_peace_worth - border half of what peace with a player is worth (used by deep_gift_probe and deep_love_gift) - no: dead behind =0 (those callers are gated)
  - deepdiplomacy.c:1299 - deep_proxy_arm - `budget = pressure * gift_border_value * proxy_arm_gain` - no: dead behind proxy_arm_gain=0
  - deepdiplomacy.c:1757 - deep_diplomacy_prepass - all-levers-off guard - yes (guard only)
  - deepdiplomacy.c:1770 - deep_diplomacy_prepass - enables deep_gift_probe() (census) for non-allied AI pairs - no
- **Log line:** `deep-gift` (deepdiplomacy.c:889; `worth`, `trade`, `border`, `budget`, `price`), and `deep-proxy` `budget=`.
- **Interacts with:** gift_love_gain, proxy_arm_gain, gift_ally_gain / gift_instrument_gain / gift_rival_gain (via deep_love_gift); trade_route_love/trade_merit indirectly (trade term).
- **Tier/screen:** unranked. Blocks: G, GDIPLO, ECON, ALL. Knob-screen Part 1: "gift family (5)" listed dead-path (per-knob row not in the committed doc).
- **Prior evidence:**
  - STATUS "BALANCE" (line 9772) / findings/balance-statecraft-2026-08-06.md - contested + lategame-80, 20 paired seeds, gift_border_value=100 and gift_love_gain=100 together (chosen to make the willingness bound non-binding) - score +8.28 (t +3.65, 15/20) and +8.92 (t +4.57, 17/20); knob not separable from gift_love_gain
  - same note: "must be swept as one" with gift_love_gain and with proxy_arm_gain; "trade at stake was exactly ZERO on every row of both civ2civ3 cells" so on those cells the border term is the entire peace worth
  - pv3.ini value 73.533238 (tuned in the pv2 campaign); GDIPLO ablation (above) - n=16 - null/negative, bundled

### gift_love_gain
- **Declared:** deeptuning.h:2540 (`double gift_love_gain;`); params.ini: default=0.0 min=0.0 max=200.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the willingness bound: deep hands over an advance when what the receiver banks for it is no more than this multiple of what the relationship is worth to us (routes at stake plus border pressure, both in gold). It is therefore a ratio between two computed quantities and not a magnitude." 0.0 = no gift ever offered.
- **Unit:** multiplier (dimensionless): `bound = gift_love_gain * worth` where worth is a gold-equivalent relationship value (trade annuity + border pressure x gift_border_value), compared to `tech_worth` (gold: stock's price of the tech to the receiver, via deep_gift_tech_pick). Not compared to the treasury (the treasury `deep_gift_budget` is logged only) and not compared to love directly (love only enters via the pre-checks `love >= bar`, `love >= cap` and the `expect=` prediction).
- **Formula at the primary call site:** deepdiplomacy.c:1092-1096 `deep_love_gift`: `bound = t->gift_love_gain * worth; ... relbound = bound * relgain; if (tech_worth > relbound) { if (tech_worth > bound) return; refused = TRUE; ...}`
- **All call sites:**
  - deepdiplomacy.c:1092 - deep_love_gift - willingness bound on the tech gift - no: dead behind =0 at compiled defaults; LIVE in the pv2/pv3 baseline (46.586943)
  - deepdiplomacy.c:1758 - deep_diplomacy_prepass - all-levers-off guard - yes (guard)
  - deepdiplomacy.c:1774 - deep_diplomacy_prepass - enables deep_love_gift() per non-allied AI pair - no at compiled defaults
- **Log line:** `deep-lovegift` (deepdiplomacy.c:1114; `worth`, `bound` (= relbound), `relgain`, `refused`, `took`, `expect`, `capped`).
- **Interacts with:** gift_border_value, gift_ally_gain, gift_instrument_gain, gift_rival_gain, trade_route_love (the alliance clamp the gift lacks, per STATUS 9914), proxy_arm_gain (shares gift_border_value).
- **Tier/screen:** unranked. Blocks: G, GDIPLO, ECON, ALL. Knob-screen Part 1: gift family dead-path (at the compiled-default corpus the tag cannot appear; my grep of deep-gift/deep-lovegift in the retake corpus: 0 lines).
- **Prior evidence:**
  - STATUS "BALANCE" (line 9772) - contested, 20 paired seeds, all-deep - +8.28 (t +3.65, 15/20); techs +2.80; lategame-80 +8.92 (t +4.57, 17/20), techs +2.98 (t +11.26, 20/20)
  - same - mixed_dc contested (deep odd seats, ai/default even) - deep +1.93 (t +0.72, 13/20), counterparty +5.26 (t +2.15), margin -3.34 (t -0.86) - a transfer
  - STATUS "gift refold" (line ~13100-13224) - identity proofs and liveness only; pv2 ships gift_love_gain=46.59, so the BALANCE mechanism is live in the adopted baseline
  - STATUS "pv3 minted" (line 13785) - see gift_rival_gain
  - STATUS "pv4 holdout confirm" - GDIPLO drop (incl. this) - econ -1.1 t -0.11, war -34.8 t -1.38, n=16

### gift_ally_gain
- **Declared:** deeptuning.h:2579 (`double gift_ally_gain;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the alliance and its candidates: an existing ally, a shared enemy, a common ally. This is where section 11.4 wants the surplus." Multiplies gift_love_gain's willingness bound; "1.0 on all three is exactly today's bound for every class".
- **Unit:** multiplier (dimensionless) on the willingness bound (gold-denominated, see gift_love_gain).
- **Formula at the primary call site:** deepdiplomacy.c:813 `deep_gift_rel_gain`: `return t->gift_ally_gain;` then deepdiplomacy.c:1094-1095 `relgain = deep_gift_rel_gain(t, rel); relbound = bound * relgain;`
- **All call sites:**
  - deepdiplomacy.c:813 - deep_gift_rel_gain - class multiplier for DEEP_GIFT_ALLY - no at compiled defaults (deep_love_gift never runs); yes under pv3 (value 1.0, so numerically neutral)
- **Log line:** `deep-lovegift rel=ally relgain=` (deepdiplomacy.c:1114).
- **Interacts with:** gift_love_gain, gift_border_value, gift_instrument_gain, gift_rival_gain.
- **Tier/screen:** unranked. Block: ALL only (deliberately not in ECON, per STATUS 13130). Gift family dead-path at defaults.
- **Prior evidence:** STATUS gift refold (line ~13199) - contested, 2 seeds, liveness only - `gift_ally_gain` 30 firings (ally rows: 772 census, 12 priced, 7 sent under pv2). No scored arm moved it off 1.0.

### gift_instrument_gain
- **Declared:** deeptuning.h:2580 (`double gift_instrument_gain;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "no friendship, but we are already at war elsewhere and this player presses our border: the second front deep cannot afford, priced rather than assumed." Own multiplier so the fold "can keep it, kill it, or read it separately".
- **Unit:** multiplier (dimensionless) on the same gold-denominated willingness bound.
- **Formula at the primary call site:** deepdiplomacy.c:815 `deep_gift_rel_gain`: `return t->gift_instrument_gain;` (then `relbound = bound * relgain`, :1095).
- **All call sites:**
  - deepdiplomacy.c:815 - deep_gift_rel_gain - class multiplier for DEEP_GIFT_INSTRUMENT - no at compiled defaults; numerically neutral (1.0) under pv3
- **Log line:** `deep-lovegift rel=instr relgain=`.
- **Interacts with:** gift_love_gain, gift_border_value (an INSTRUMENT class needs `deep_border_pressure_from() > 0`), gift_ally_gain, gift_rival_gain.
- **Tier/screen:** unranked. Block: ALL only. Gift family dead-path at defaults.
- **Prior evidence:** STATUS gift refold (~13199) - liveness only: 72 firings, instr rows 368 census / 30 priced / 30 sent under pv2. No scored arm.

### gift_rival_gain
- **Declared:** deeptuning.h:2581 (`double gift_rival_gain;`); params.ini: default=1.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "everyone else -- the population that banked the +5.26. 0.0 refuses them outright."
- **Unit:** multiplier (dimensionless) on the same gold-denominated willingness bound; 0.0 makes `relbound` zero so any positive-price tech is "refused".
- **Formula at the primary call site:** deepdiplomacy.c:820 `deep_gift_rel_gain`: `return t->gift_rival_gain;` (fallthrough for DEEP_GIFT_RIVAL); `relbound = bound * relgain` at :1095; `tech_worth > relbound` -> `refused=TRUE` at :1096-1100.
- **All call sites:**
  - deepdiplomacy.c:820 - deep_gift_rel_gain - class multiplier for the RIVAL class - no at compiled defaults; LIVE with value 0.0 in pv3/pv4 baselines (pv3.ini:225)
- **Log line:** `deep-lovegift rel=rival relgain=` (`refused=1 took=0` at 0.0).
- **Interacts with:** gift_love_gain, gift_border_value.
- **Tier/screen:** unranked. Block: ALL only. Gift family dead-path at defaults.
- **Prior evidence:**
  - STATUS gift refold (~13187-13200) - contested, 2 seeds/arm, pv2 vs gift-refold.ini (rival=0.0) - liveness only: rival gold given 2020 -> 0, 133 gifts refused by class price; 168 firings
  - STATUS "pv3 minted" (line 13785) - mixed head-to-head holdout - deep margin over ai/default +62.8 (pv3-style rival=0.0) vs pv2 +36.9; paired +25.9, t +0.74 - "clears wash-or-better" but not significant; n not stated in the entry

### proxy_arm_gain
- **Declared:** deeptuning.h:2620 (`double proxy_arm_gain;`); params.ini: default=0.0 min=0.0 max=200.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the willingness bound ... denominated exactly like the love gift's: deep will hand over an advance whose price to the receiver is at most this multiple of what the menacer's pressure is costing us, at gift_border_value's rate." Hands a military tech to a distant power already in the menacer's way. "ETHICS OF PLAY ... arms your rivals to keep you busy."
- **Unit:** multiplier (dimensionless) on a gold budget = border pressure (danger units) x gift_border_value (gold/danger). Compared to gold figures: the tech's price to the receiver (`worth`, deep_tech_worth_to) and the cease-fire price `need` (greed(love)^2/10, a gold-equivalent treaty balance). Not the treasury, not a want.
- **Formula at the primary call site:** deepdiplomacy.c:1299 `deep_proxy_arm`: `double budget = pressure * t->gift_border_value * t->proxy_arm_gain;` used as `if (need > budget) continue;` (:1356) and `if (worth <= 0 || worth > budget) continue;` (:1371).
- **All call sites:**
  - deepdiplomacy.c:1299 - deep_proxy_arm - budget for arming a proxy - no: dead behind =0
  - deepdiplomacy.c:1759 - deep_diplomacy_prepass - all-levers-off guard - yes (guard)
  - deepdiplomacy.c:1786 - deep_diplomacy_prepass - enables the per-player proxy lever - no
- **Log line:** `deep-proxy` (deepdiplomacy.c:1413; `budget`, `need`, `reach`, `took`).
- **Interacts with:** gift_border_value (multiplied in the same expression); not with gift_love_gain.
- **Tier/screen:** unranked. Blocks: G, GDIPLO, ECON, MIL, ALL. Not screened (no per-knob row; deep-proxy 0 lines in the compiled-default corpus).
- **Prior evidence:**
  - STATUS "BALANCE" section 3 (line ~9860) - contested, 2 games, budget made non-binding - 97 structurally eligible candidate-turns, zero offers (cease-fire priced at median 9486 gold; a legal one-step gift worth ~45 gold) - "cannot be revived by tuning"; lategame-80 has 0 candidates
  - pv3.ini value 11.047219 was tuned anyway (pv2 campaign); GDIPLO ablation (above), n=16, bundled

### honest_info
- **Declared:** deeptuning.h:2914 (`int honest_info;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Whether a deep seat declines omniscience (W04 / RC6, epic S5)": `!= 0` ORs H_MAP, H_FOG and H_TARGETS onto deep's own player in deep's own phase hook. "1 is therefore expected to make deep WORSE ... STRUCTURAL EXPERIMENT FLAG, not a tuning target."
- **Unit:** boolean gate (`<= 0` returns).
- **Formula at the primary call site:** deepai.c:666 `deep_impose_honest_info`: `if (deep_tuning_get()->honest_info <= 0 || is_human(pplayer) || pplayer->ai_common.handicaps == NULL) return;` then `BV_SET(*handicaps, H_MAP/H_FOG/H_TARGETS)` (:675-686).
- **All call sites:**
  - deepai.c:666 - deep_impose_honest_info - sets three information handicaps on the deep seat each phase (called from deepai.c:712 before dai_data_phase_begin) - no: dead behind =0
  - downstream (not direct reads of the knob): ~19 H_MAP/H_FOG gates in ai/deep, ~42 forwarded ai/default gates, 9 in server/advisors become effective once the bits are set (omniscience-census-2026-08-11.md)
- **Log line:** `deep-honest` (deepai.c:689), once per seat on the first phase that imposes bits. 0 lines in the compiled-default corpus.
- **Interacts with:** hut_risk_honest (separate knob, M block, not in this slice); pay_switch_penalty (same hook pattern).
- **Tier/screen:** unranked. Block: ALL only ("in the ALL block only because tune-audit.py requires it", STATUS ~11265). Not screened. The "moves 1 read in 8" figure in the task brief was not found verbatim; the census says the flag "moves none of the 148 ungated reads" and affects 70 gates (see Unresolved).
- **Prior evidence:**
  - findings/omniscience-flag-breakage.md section 0 (S5 phase 2) - contested (8p, T150, all deep), 20 seeds - score 83.17 -> 81.02, -2.15 (SE 2.65, t -0.81)
  - same - isolated-150, 10 seeds - 176.70 -> 190.10, +13.40 (SE 7.93, t +1.69)
  - same - contested mixed vs baseline, 6x2 mirrors - gap +11.19 -> +1.83, -9.35 (SE 6.15, t -1.52); deep lead falls about 80%; 252 deep-honest lines, 84 games, no crashes
  - STATUS S5 entry (line ~11204) - identity at honest_info=0: 10/10 smoke scorelogs byte-identical

### chatty_gate
- **Declared:** deeptuning.h:2960 (`int chatty_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "deep says nothing, deep_chat_open() fails on its first comparison ... != 0: the four message classes are live ... FLAVOUR FLAG, not a tuning target: every line is output only and no line can move a want, a unit or the RNG."
- **Unit:** boolean gate (`<= 0` closes).
- **Formula at the primary call site:** deepchat.c:87 `deep_chat_open`: `if (tune->chatty_gate <= 0 || tune->chatty_max_per_turn <= 0) { return FALSE; }`
- **All call sites:**
  - deepchat.c:87 - deep_chat_open - first test before any outbound ally line - no: dead behind =0
  - deepchat.c:234 - deep_chat_reply - gate on a reply line to a requester - no
  - deepchat.c:409 - deep_chat_is_warner - warner election among deep allies (mirrors the sender's gate) - no
- **Log line:** `deep-chat` (deepchat.c:161 and :260).
- **Interacts with:** chatty_max_per_turn.
- **Tier/screen:** unranked. Block: ALL only. Chatty family dead-path (Part 1 prose). knobcensus.py class MODE.
- **Prior evidence:** STATUS CHATTY slice 1 (line ~18848-18930) - smoke/liveness only: identity proofs, `chatty_gate` MODE 472 firings; derby seat config turns it on. No scored arm (output only; byte-identity between gate-on and gate-off claimed).

### chatty_max_per_turn
- **Declared:** deeptuning.h:2972 (`int chatty_max_per_turn;`); params.ini: default=3 min=0 max=12 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "lines this player may say per turn, counted per LINE and not per recipient ... <= 0 is silence even with the gate open." PLACEHOLDER; "can never BE tuned by benchmark/tune.py" (quantity traded off is a human ally's patience).
- **Unit:** count (chat lines per player per turn; `dp->chat_said >= chatty_max_per_turn`). Reused as a per-sender request-hearing cap in deeprequest.c:358.
- **Formula at the primary call site:** deepchat.c:95 `deep_chat_open`: `if (dp->chat_turn == game.info.turn && dp->chat_said >= tune->chatty_max_per_turn) return FALSE;`
- **All call sites:**
  - deepchat.c:87 / :95 - deep_chat_open - zero-cap closes the channel; cap ends the turn's lines - no: dead behind chatty_gate=0
  - deepchat.c:234-236 - deep_chat_reply - same checks on reply lines - no
  - deepchat.c:409 - deep_chat_is_warner - candidate must have a nonzero cap - no
  - deeprequest.c:358 - deep_request_hear - per-ally heard-request cap `chat_req_heard[sender] >= cap` -> verdict rate-capped - no: dead behind chatty_inbound_gate=0
  - (log prints only: deepchat.c:165, :263)
- **Log line:** `deep-chat` (`said=%d/%d`), `deep-request verdict=rate-capped`.
- **Interacts with:** chatty_gate, chatty_inbound_gate.
- **Tier/screen:** unranked. Block: ALL only. Chatty family dead-path. knobcensus MODE.
- **Prior evidence:** STATUS CHATTY slices 1 and 2 (lines 18848-18861, 19814-19818) - liveness only: MODE 472 / 320 firings; "cap breaches: 0". chatty2-on.ini sets 12. No scored arm.

### chatty_inbound_gate
- **Declared:** deeptuning.h:3083 (`int chatty_inbound_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "deep_request_hear() returns on its first comparison, no chat line is ever parsed ... != 0: four exact-match commands from an ALLIED player -- defend <city>, attack <city|player>, road to <city>, status -- are read and answered. Unlike chatty_gate this one is NOT output-only: an accepted request moves a want."
- **Unit:** boolean gate (`<= 0` returns).
- **Formula at the primary call site:** deeprequest.c:298 `deep_request_hear`: `if (tune->chatty_inbound_gate <= 0 || text == NULL) { return; }`
- **All call sites:**
  - deeprequest.c:298 - deep_request_hear - parse/ignore inbound ally chat - no: dead behind =0
  - deepdefence.c:2404 - deep_credit_incoming_defence (ledger seat) - adds `deep_request_defend_fear` to danger - no
  - deepdefence.c:5547 - deep_military_choose_build (build seat) - same term at the build seat - no
  - deepops.c:3202 - deep_op_pick_target - multiplies operation target score by `deep_request_attack_mult` - no
  - deepops.c:4592 - deep_op_retarget - same multiply at retarget - no
  - deeptrunk.c:817 - deep_trunk_ally_nodes - `asked` flag for the corridor endpoint - no
- **Log line:** `deep-request` (deeprequest.c:263, one line per line heard) and `deep-requests` (deeprequest.c:514).
- **Interacts with:** chatty_request_turns, chatty_defend_fear, chatty_attack_gain, chatty_road_gain, chatty_max_per_turn, trunk_ally_gate.
- **Tier/screen:** unranked. Block: ALL only. Chatty family dead-path. knobcensus MODE.
- **Prior evidence:** STATUS CHATTY slice 2 (lines 19700-19960) - smoke and scripted-alliance games, liveness only: identity 20/20 byte-identical at 0; gate-open-no-traffic vs deaf identical; MODE 20 firings; no scored arm.

### chatty_request_turns
- **Declared:** deeptuning.h:3090 (`int chatty_request_turns;`); params.ini: default=10 min=1 max=50 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "How many turns an accepted request keeps counting for. Every effect below decays to nothing when it passes." PLACEHOLDER: trades an ally's patience against deep's own judgement.
- **Unit:** turns (`until = game.info.turn + chatty_request_turns`, compared with `game.info.turn > req->until`).
- **Formula at the primary call site:** deeprequest.c:96 `deep_request_store`: `int until = game.info.turn + tune->chatty_request_turns;` (also :372 in deep_request_hear).
- **All call sites:**
  - deeprequest.c:96 - deep_request_store - expiry turn stored on a request - no: dead behind chatty_inbound_gate=0
  - deeprequest.c:372 - deep_request_hear - expiry turn logged/returned for the accepted request - no
- **Log line:** `deep-request until=` and `deep-requests live= retired=`.
- **Interacts with:** chatty_inbound_gate (and, through it, the three price knobs).
- **Tier/screen:** unranked. Block: ALL only. Chatty family dead-path. knobcensus MODE.
- **Prior evidence:** STATUS CHATTY slice 2 (line ~19949) - scripted alliance - requests taken at T41 stop counting at T52 = 10 turns exactly; liveness only.

### chatty_defend_fear
- **Declared:** deeptuning.h:3098 (`double chatty_defend_fear;`); params.ini: default=50.0 min=0.0 max=400.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "fear added to that city's danger figure while the request lives, in the same currency as every other injected fear in deepdefence.c. A TERM and not an override -- a city under real siege still outbids one that was merely asked about."
- **Unit:** want/score units, specifically the danger-ledger currency: the value is cast to `unsigned int` and added to `city_data->danger` (deepdefence.c:2405, :5548), which the build path "then squares and converts like any other danger" (deeprequest.c header comment). It is not itself a build want.
- **Formula at the primary call site:** deeprequest.c:547 `deep_request_defend_fear`: `return (unsigned int) tune->chatty_defend_fear;` consumed at deepdefence.c:5548 `city_data->danger += deep_request_defend_fear(pplayer, pcity);` (deep_military_choose_build).
- **All call sites:**
  - deeprequest.c:538 / :547 - deep_request_defend_fear - zero returns 0; otherwise the live-request fear - no: dead behind chatty_inbound_gate=0 (callers check the gate first)
  - consumers: deepdefence.c:2405 (ledger), deepdefence.c:5548 (build seat) - no
- **Log line:** none specific; effect appears in `deep-fear ... site=build|ledger post=` (deepdefence.c:5560, :2418), and the request itself in `deep-request verdict=defend`. knobcensus.py marks it "proxy".
- **Interacts with:** chatty_request_turns, chatty_inbound_gate; every other injected fear term in the same danger accumulation (garrison floor, capital stake).
- **Tier/screen:** unranked. Block: ALL only. Chatty family dead-path. knobcensus: LIVE 4 firings in chatty2 smoke.
- **Prior evidence:** STATUS CHATTY slice 2 (line ~19817) - liveness only (4 firings); no scored arm. chatty2-on.ini sets 50.0.

### chatty_attack_gain
- **Declared:** deeptuning.h:3105 (`double chatty_attack_gain;`); params.ini: default=0.5 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "multiplier 1 + gain on the operation target score, which is a pure ranking quantity (worth per tile), so this re-orders candidates and can never clear an odds gate. 0.0 = off and the multiply is skipped."
- **Unit:** multiplier (dimensionless): `score *= (1 + gain)`.
- **Formula at the primary call site:** deeprequest.c:580 `deep_request_attack_mult`: `return 1.0 + tune->chatty_attack_gain;` applied at deepops.c:3203 `score *= deep_request_attack_mult(pplayer, acity);` (deep_op_pick_target; score = worth per distance).
- **All call sites:**
  - deeprequest.c:567 / :580 - deep_request_attack_mult - returns 1.0 at gain<=0, else 1+gain for a live attack request naming the city or its owner - no: dead behind chatty_inbound_gate=0
  - consumers: deepops.c:3203 (deep_op_pick_target), deepops.c:4593 (deep_op_retarget) - no
- **Log line:** none specific (`deep-request verdict=attack-city|attack-player`; effect read on `deep-op` target). knobcensus "proxy".
- **Interacts with:** chatty_inbound_gate, chatty_request_turns; other score multipliers on the same target score (weak_prey_gain, leader_check_gain, trade_partner_target_discount).
- **Tier/screen:** unranked. Block: ALL only. Chatty family dead-path. knobcensus LIVE 4 firings.
- **Prior evidence:** STATUS CHATTY slice 2 - liveness only; no scored arm.

### chatty_road_gain
- **Declared:** deeptuning.h:3113 (`double chatty_road_gain;`); params.ini: default=1.0 min=0.0 max=8.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "how much closer a requested ally city is PRETENDED to be when the trunk planner picks which of that ally's cities the corridor runs to -- the distance is divided by 1 + gain ... with trunk_ally_gate shut there is nothing to prefer and this is inert."
- **Unit:** multiplier (dimensionless; divisor `1 + gain` on a tile distance, so the affected quantity is tiles).
- **Formula at the primary call site:** deeptrunk.c:829 `deep_trunk_ally_nodes`: `d = (int) (d / (1.0 + deep_tuning_for(pplayer)->chatty_road_gain));` (only when `asked`).
- **All call sites:**
  - deeprequest.c:601 - deep_request_road_wanted - returns FALSE at gain<=0 - no: dead behind chatty_inbound_gate=0
  - deeptrunk.c:829 - deep_trunk_ally_nodes - distance discount for the requested ally city - no: also needs trunk_ally_gate
- **Log line:** none specific (`deep-request verdict=road`; effect on `deep-trunklink ally=1`). knobcensus "proxy".
- **Interacts with:** chatty_inbound_gate, trunk_ally_gate, chatty_request_turns.
- **Tier/screen:** unranked. Block: ALL only. Chatty family dead-path. knobcensus LIVE 2 firings.
- **Prior evidence:** STATUS CHATTY slice 2 - liveness only; no scored arm.

### diplo_price_gate
- **Declared:** deeptuning.h:3611 (`int diplo_price_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Substitute deep's own trigger for stock's on the DEFENSIVE diplomat" (stock writes a "totally arbitrary" 16000 whenever `diplomat_threat` is set; the flag is set for any attack/pillage/bombard/conquer-capable unit, upstream bug 50). "0 = stock's flag decides, byte for byte."
- **Unit:** boolean gate (`> 0`).
- **Formula at the primary call site:** deepdefence.c:5652-5654 `deep_military_choose_build`: `if (dt->diplo_price_gate > 0 && (ev.dip < dt->diplo_evidence_floor || ev.gap < dt->diplo_gap_floor)) { verdict = "refuse"; dip->want = 0; }` (inside `if (dip->want > 0)`).
- **All call sites:**
  - deepdefence.c:5652 - deep_military_choose_build - refuses the stock defensive-diplomat choice when evidence is below floors - no: dead behind =0
- **Log line:** `deep-dipthreat` (deepdefence.c:5669; `verdict=allow|refuse|off`, `dip`, `gap`). 0 lines in the compiled-default screen corpus (corpus predates the tag).
- **Interacts with:** diplo_evidence_floor, diplo_gap_floor, diplomat_defence_value (the want paid when allowed; not changed by the gate).
- **Tier/screen:** unranked. Blocks: M, MIL, ALL. Not screened.
- **Prior evidence:** STATUS S-DIPLOPRICE (line ~23590) / findings/diploprice-verdict-2026-08-30.md section 3 - contested + lategame-80 2x2 with mil_infra gate - PARKED at 10/96 games; control cell only; no effect estimate. Liveness at cell 11 (one game): gate refuses 331 of 569 warranted diplomat choices (58.2%). Byte-neutral at defaults (seeds 101-103 reproduce archived scorelogs).

### diplo_evidence_floor
- **Declared:** deeptuning.h:3617 (`int diplo_evidence_floor;`); params.ini: default=0 min=0 max=4 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Minimum number of foreign diplomat-class units observed inside stock's own `2 >= move_time` window for a defensive diplomat to be warranted. PLACEHOLDER. The tested value is 1, which is not a magnitude but the literal reading of the flag's own comment. Inert while the gate is 0."
- **Unit:** count (foreign UTYF_DIPLOMAT units within eta<=2, `ev.dip`).
- **Formula at the primary call site:** deepdefence.c:5653 `deep_military_choose_build`: `ev.dip < dt->diplo_evidence_floor` (ev.dip from `deep_diplomat_evidence()`, deepdefence.c:5296).
- **All call sites:**
  - deepdefence.c:5653 - deep_military_choose_build - refusal predicate - no: dead behind diplo_price_gate=0
- **Log line:** `deep-dipthreat dip=`.
- **Interacts with:** diplo_price_gate, diplo_gap_floor.
- **Tier/screen:** unranked. Blocks: M, MIL, ALL. Not screened.
- **Prior evidence:** S-DIPLOPRICE parked (above); the 2x2 cells use `diplo_evidence_floor=1` (diploprice-10.ini) - no result.

### diplo_gap_floor
- **Declared:** deeptuning.h:3625 (`int diplo_gap_floor;`); params.ini: default=0 min=0 max=8 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Additionally require that a hostile neighbour in contact knows at least this many techs we do not ... shipping UNTESTED at 0 (requires nothing): the census gives no threshold and rule 7 forbids inventing one. Inert while the gate is 0."
- **Unit:** count (techs the best hostile-capable neighbour knows and we do not, `ev.gap`).
- **Formula at the primary call site:** deepdefence.c:5654 `deep_military_choose_build`: `|| ev.gap < dt->diplo_gap_floor`.
- **All call sites:**
  - deepdefence.c:5654 - deep_military_choose_build - refusal predicate - no: dead behind diplo_price_gate=0 (and `ev.gap < 0` is never true at the default 0)
- **Log line:** `deep-dipthreat gap=`.
- **Interacts with:** diplo_price_gate, diplo_evidence_floor.
- **Tier/screen:** unranked. Blocks: M, MIL, ALL. Not screened.
- **Prior evidence:** S-DIPLOPRICE parked (above); "deliberately UNTESTED" (STATUS ~23637); cell inis set it to 0.

## Unresolved

- **peace_seek_gain screen verdict vs corpus.** knob-screen-2026-08-30.md Part 1 (lines 146-155) lists it dead-path ("log tag never appeared"). In the compiled-default retake corpus (`benchmark/results/w25-screen-corpus/{econ,war}/holdout-report/cfg0`, 40 games) `deep-peaceseek` appears in 40/40 games (econ 1005 lines, war 1054) and `deep-peacetake` in 40/40 (570, 929). The screen's replay rule (benchmark/knobscreen.py:631-646, 795) assumes `_base_peace_seek_gain` from the base tuning with default 0.0, which matches the pre-fold default, not the 1.0 compiled now. I did not re-run knobscreen.py (it errored with "bad predicate: 'slack'" when I tried it on the corpus dirs), so the current screen verdict for this knob is not established.
- **Per-knob screen rows.** The knob-screen doc only names families (chatty 7, gift 5, spy, etc.) as dead-path in prose and lists top-ranked tier-B knobs; none of the 22 is in the table I could read. The "dead-path" entries above for gift/chatty are inferred from the family statement; embassy_price_gain, embassy_give_gain, proxy_arm_gain, honest_info, diplo_* and diplo_deep_only_pacts are marked "not screened" because no row was found.
- **"honest_info moves 1 read in 8".** Not found verbatim in 00-context.md or findings/omniscience-*.md. What is documented: 148 ungated omniscient reads (the flag moves none), about 70 gates (19 in ai/deep, ~42 forwarded stock, 9 in server/advisors), and 6 deep visibility gates that fire unconditionally.
- **Which of the pv-lineage files is the benchmark default arm.** pv3.ini (and pv4, pv3-lean, derby-lean) set peace_seek_gain=0.801516, gift_border_value=73.533238, gift_love_gain=46.586943, gift_rival_gain=0.0, proxy_arm_gain=11.047219, embassy_science_value=4.596145, embassy_price/give=0 (pv2 identical except it omits the three gift_*_gain keys). I did not determine which file `run.sh` loads by default, so "live" statements are for compiled defaults, with pv3 values noted.
- **gift_rival_gain pv3 evidence n.** STATUS 13785 gives holdout +62.8 vs +36.9 (paired +25.9, t +0.74) but not the seed count or venue detail; the underlying findings file was not located.
- **diplo_deep_only_pacts.** No STATUS/findings result exists; only the brief and fence-on.ini. The `fence` A/B may not have been run.
- **embassy_science_value call-site liveness.** "Yes (civ2civ3)" assumes the ruleset's `tech_leakage` is Embassies as the header and deeptuning.h:2625 state; I did not read the ruleset file.
