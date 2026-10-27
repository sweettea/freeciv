# Knob inventory D: fear, stance, garrison floor, survival (52 knobs)

Facts only. Source: HEAD b42d64d7a8 of /home/sweettea/freeciv, `ai/deep/*.c`, `benchmark/tuning-params.ini`, `benchmark/tuning-blocks.ini`, `doc/deep-ai/STATUS.md` and `doc/deep-ai/findings/`. Compiled defaults in `ai/deep/deeptuning.c` were checked equal to the params.ini defaults for all 52. `# call sites` = distinct code reads of the field in `ai/deep` (comment-only mentions excluded; survival_target and survival_horizon are counted by grep of `->name`).

| knob | unit | default | live at defaults? | tier | screen verdict | prior A/B | # call sites |
|---|---|---|---|---|---|---|---|
| diplomat_defence_value | want units (EMERGENCY-tier value) | 300 | yes (only where stock's diplomat_threat fires) | unranked | not screened (NO-INSTRUMENT) | none | 1 |
| garrison_floor | count (units on city tile) | 1 | yes | unranked (gate-like) | flips 113.0/g (0/1-style key) | none | 4 |
| garrison_floor_danger | danger units (zero/non-zero switch for empty cities) | 40 | yes | A (DEFOPS) | flips 113.1/g, 63.5% | positive | 1 |
| garrison_floor_turn | turns | 0 | no (no-op at 0) | A (DEFOPS) + B (BANTICIP) | not screened | none | 1 |
| garrison_menace_slope | multiplier (dimensionless) | 1.0 | yes | B (BFEAR) | fires 3306/g (partial) | none | 2 |
| menace_overseas_weight | multiplier 0-1 | 0.25 | yes | B (BFEAR) | fires 3306/g (partial) | positive | 3 |
| floor_barb_weight | multiplier | 1.0 | conditional (barb era only) | unranked | flips 0.6/g, 0.3% | none | 1 |
| floor_sea_weight | multiplier | 1.0 | yes | A (DEFOPS) | flips 63.4/g, 35.6% | none | 2 |
| floor_air_weight | multiplier | 1.0 | no in practice (flight era; 0 air=1 lines in 40 games) | unranked | inert (wair = 0.000) | none | 1 |
| floor_border_weight | multiplier (x R/(R+d)) | 1.0 | yes | A (DEFOPS) | flips 53.3/g, 30.0% | none | 1 |
| floor_interior_weight | multiplier 0-1 | 0.25 | yes | B (BFEAR) | not screened | none | 2 |
| floor_marines_mult | multiplier (divisor >= 1) | 1.5 | yes | B (BFEAR) | not screened | none | 1 |
| barbarian_fear | danger units x scale | 30 | conditional (barb era, rate>1) | unranked | not screened (NO-INSTRUMENT) | null | 2 |
| barb_anticipation_turns | turns | 10 | conditional (barb era) | unranked | not screened | null | 2 |
| barb_size_ref_cities | count (cities; normaliser) | 10 | conditional (barb era) | B (BFEAR) | fires 156/g (partial) | none | 1 |
| density_fear_value | danger units per neighbour city / (1+d) | 6 | yes | unranked | not screened (NO-INSTRUMENT) | none | 4 |
| density_fear_radius | tiles | 8 | yes | unranked | not screened (NO-INSTRUMENT) | none | 6 |
| density_fear_ramp_turns | turns | 0 | no (no-op at 0) | B (BANTICIP) | not screened | null (venue sign-flip) | 2 |
| fear_evidence_gate | boolean gate | 0 | no | unranked | not screened | null (venue sign-flip) | 2 |
| fear_evidence_floor | count (attack-capable units) | 1 | no (behind gate=0) | B (BANTICIP) | not screened | null | 2 |
| density_fear_capability | multiplier 0-1 | 0.25 | no (behind gate=0) | B (BANTICIP) | not screened | null | 1 |
| density_prior_value | danger units x crowding x ramp | 20 | yes (pre-contact, build seat only) | unranked | not screened (NO-INSTRUMENT) | none | 2 |
| density_prior_tiles | tiles (land per player ref) | 200 | yes (also scales settler bid + choke site bonus) | unranked | not screened (NO-INSTRUMENT) | none | 1 |
| wall_fear_weight | multiplier (danger -> building want) | 0.3 | yes | unranked | not screened (NO-INSTRUMENT) | none | 2 |
| birth_guard_gain | multiplier on p*worth (want units) | 0.0 | no | unranked | not screened | null | 2 |
| defensive_techwall_weight | multiplier (fear -> tech_want) | 0.0 | no | unranked | not screened | none | 2 |
| attacker_hold_damper | multiplier (damp = 1/(1+g*stuck)) | 0.0 | no | unranked | not screened | none | 2 |
| diplomat_pump_damper | multiplier per agent | 0.0 | no | unranked | not screened | none | 2 |
| base_fort_gain | want units (tile-output currency) | 40.0 | yes | B (BCIVIL) | fires 371/g (partial) | null | 2 |
| fortify_ground | boolean gate | 1 | yes | unranked | not screened | positive | 1 |
| pillage_denial_gain | multiplier on tile-output points (+ threshold literal 1.0) | 1.0 | yes (war only) | unranked | not screened | null | 2 |
| survival_target | probability 0-1 | 0.5 | yes | B (BFEAR) | fires 1086/g (partial) | none | 35 |
| survival_fear_value | danger units | 20.0 | yes | B (BFEAR) | fires 607/g (partial) | none | 3 |
| survival_horizon | turns (tiles at one site) | 3 | yes | B (BFEAR) | fires 1086/g (partial) | none | 47 |
| survival_pf_reach | boolean gate | 0 | no | unranked | not screened | positive (war venue only) | 1 |
| threat_pin_discount | probability 0-1 | 0.0 | no | unranked | not screened | null | 4 |
| attacker_garrison_gain | multiplier | 0.0 | no | unranked | not screened | none | 2 |
| satisfied_fear_discount | multiplier 0-1 | 0.0 | yes (0 = satisfied cities silent) | B (BFEAR) | fires 2427/g (partial) | none | 2 |
| barb_reinforce_rate | count per turn | 0.0 | yes as a read; value 0 = no credit | unranked | LIVE proxy, 1/11 corpora | none | 1 |
| stance_forced | enum/sentinel (-1 FREE,0,1,2) | -1 | no (FREE = identity) | mode (degenerate range) | MODE | null | 3 |
| stance_source | enum/sentinel (0/1/2) | 0 | no (identity) | mode (degenerate range) | MODE | null | 1 |
| stance_trait_span | multiplier (axis half-width) | 0.5 | no (unused at identity) | mode (degenerate range) | MODE | none | 1 |
| stance_fear_gain | multiplier (exponent of mil/econ) | 1.0 | no | unranked | live only when pinned | null | 1 |
| stance_op_gain | multiplier (exponent of econ/mil) | 1.0 | no | unranked | live only when pinned | none | 1 |
| stance_growth_gain | multiplier (exponent of econ/mil) | 1.0 | no | unranked | live only when pinned | null | 1 |
| stance_expand_gain | multiplier (exponent of econ/mil) | 1.0 | no | unranked | live only when pinned | null | 1 |
| stance_assault_gain | multiplier (exponent of mil/econ) | 1.0 | no | unranked | DEAD in coverage corpus; live pinned | null | 1 |
| stance_floor_gain | multiplier (exponent of mil/econ) | 1.0 | no | unranked | not screened | none | 1 |
| stance_pox_gain | multiplier (exponent on price) | 1.0 | no (also site_own_overlap=0) | unranked | not screened | null | 1 |
| stance_position_gain | multiplier (index units) | 0.5 | no (source 2 only) | unranked | not screened | none | 1 |
| stance_techlean_gain | multiplier (index units) | 0.5 | no (source 2 only) | unranked | not screened | none | 1 |
| stance_pressure_ref | danger units per city (half-saturation) | 4.0 | no (source 2 only) | unranked | not screened | none | 1 |

Rows whose 'live at defaults' begins with `no`: 26 of 52.

## How to read this slice: the fear chain (code-verified, deepdefence.c at HEAD b42d64d7a8)

All numbers below are line references at HEAD; the plan's line references were not used.

- **Danger field.** `city_data->danger` (`unsigned int`, "danger units") is first set by stock `assess_danger()`; deep then ADDS injected terms in two seats: the once-per-turn ledger `deep_credit_incoming_defence()` (deepdefence.c:2339, log `deep-fear site=ledger`) and the build seat inside `deep_military_choose_build()` (deepdefence.c:5444, block 5490-5560, log `deep-fear site=build`). Terms, each times `updisc * sat * fmult` (upkeep discount, satisfaction gate, stance fear mult): `over` (overseas), `shadow` (boat), `density` (density_fear_*), `prior` (density_prior_*, build seat only), `barb` (barbarian_fear). Then unscaled `surv` (survival deficit), minus the incoming/muster credit, then (gated) chatty/capital-stake terms, then `deep_apply_garrison_floor()` which only fires when danger is still exactly 0.
- **Danger -> defender want.** In `deep_military_choose_build()` (deepdefence.c:5718-5740): `qdanger = danger*danger`; `danger% = 100*qdanger/our_def` (our_def = `assess_defense_quadratic`), capped `MIN(200,..)+urgency`. When the city has NO defence (`our_def == 0`) and `urgency == 0` the result is a FLAT 100 whatever the magnitude (any danger >= 1 after uint truncation); with `urgency > 0` it is `200+urgency`. That flat value goes out at `DWT_EMERGENCY` tier (`first defender`, line ~5777-5786) unless `birth_guard_gain > 0` (then `DWT_NORMAL`, `p*worth*gain`). For a city that already has a defender the want is `danger%` (clamped to `MIN(25,danger)` for 1-defence units when urgency == 0) + martial + deterrence dividend (line ~5900). That `adv_choice` competes with the domestic advisor's founder/settler bid at the tier-first merge (see `deep_mil_opportunity_charge` deepdefence.c:8198 and fearseat-verdict section 0). Consequence worth knowing for UNIT purposes: for an EMPTY city every injected-fear knob acts as a **zero/non-zero switch**, not a magnitude (the want is 100 at the emergency tier either way); magnitudes only matter in cities that already have a defender.
- **Eligibility is a residual.** The garrison floor fires only if `danger == 0` after every fear term; so every fear term "owns" a city before the floor does.
- **Stance (W34).** `deep_stance_refresh()` sets `plr->stance.{econ,mil,expand}`. At compiled defaults (`stance_forced=-1`, `stance_source=0`, both with degenerate min=max in tuning-params.ini so the tuner cannot move them) the axes are identity (econ == mil, expand == 1), so every `stance_*_gain` multiplier is exactly 1.0 and ALL `stance_*` knobs are dead at defaults. They go live only when a run pins `stance_forced` (0/1/2) or sets `stance_source` 1 or 2.
- **Early-army seat (fearseat/fearsink).** In T1-25 the opening defenders are bought at the build seat, almost entirely by the density term (54.9% of evaluations) and the floor (73-74% of military city-turns sit at raw danger 0, E34.2/E34.2b). The fearseat verdict showed the knob that moves the opening army is `mil_opportunity_price` (not in this slice), not the fear magnitudes: `density_fear_ramp_turns` and `fear_evidence_*` moved the opening army by "nothing measurable" in `munits@25` (+0.01 +-0.03) and were venue sign-flipped on score.

# Knob entries

### diplomat_defence_value
- **Declared:** deeptuning.h:507 (`double diplomat_defence_value;`); params.ini: default=300 min=0 max=1000 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** Stock prices a defensive diplomat at a self-described "totally arbitrary" 16000 that outranks military emergencies; deep re-expresses it "as an emergency-tier value", default "just above the defender emergency range observed in the calibration corpus (Phalanx p90 native 1224 = tier value 224)".
- **Unit:** want/score units (the `.value` of a `deep_want` at `DWT_EMERGENCY`, same currency as the defender `danger%` want, whose own scale is 100 / 200+urgency). Why: it is written straight into `.value` of a `struct deep_want` and compared with `deep_want_cmp()` against the military choice.
- **Formula at the primary call site:** `deep_want_apply(dip, (struct deep_want) { .tier = DWT_EMERGENCY, .value = dt->diplomat_defence_value });` then `if (deep_want_cmp(deep_want_from_choice(dip), deep_want_from_choice(choice)) > 0) adv_choice_copy(choice, dip);` -- deepdefence.c:5679-5684, `deep_military_choose_build()`.
- **All call sites:** deepdefence.c:5680 - deep_military_choose_build - value of a defensive-diplomat build when stock's `dai_choose_diplomat_defensive` fired (`dip->want > 0`) and the S-DIPLOPRICE gate did not refuse - yes (`diplo_price_gate`=0 so stock's flag decides; fires only in cities where stock's `diplomat_threat` is set).
- **Log line:** `deep-dipthreat` (deepdefence.c:5660, carries `stockwant=`, `verdict=`); the chosen value itself is not printed.
- **Interacts with:** diplo_price_gate, diplo_evidence_floor, diplo_gap_floor (decide whether to bid at all).
- **Tier/screen:** unranked (not in tuning-blocks.ini); knob-coverage: NO-INSTRUMENT (the 300 is not logged); not screened.
- **Prior evidence:** none on the value (S-DIPLOPRICE adjudicated the trigger gate, findings/diploprice-verdict-2026-08-30.md, which states it "does not touch the WANT"). STATUS.md:459 and :607 only record the value as corpus-derived.

### garrison_floor
- **Declared:** deeptuning.h:520 (`int garrison_floor;`); params.ini: default=1 min=0 max=3 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Token garrison kept even at zero assessed danger: requisition a defender while the city tile holds fewer than garrison_floor units, by injecting garrison_floor_danger into the danger field".
- **Unit:** count (units standing on the city tile; `unit_list_size(city_tile->units)` -- ALL units, civilians included, in the floor test; non-civilians only in the melt and the veterancy gate).
- **Formula at the primary call site:** `if (city_data->danger != 0 || game.info.turn < tune->garrison_floor_turn || unit_list_size(city_tile(pcity)->units) >= tune->garrison_floor) return;` -- deepdefence.c:1979-1983, `deep_apply_garrison_floor()`.
- **All call sites:** deepdefence.c:1981 - deep_apply_garrison_floor - eligibility of the floor injection - yes; deepdefence.c:2085 - deep_fear_satisfaction - "garrison is SIZED" test; below the floor the satisfaction gate returns 1.0 (fears bid in full) - yes; deepdefence.c:2613 - deep_melt_surplus_defenders - melt only units above `count <= garrison_floor` - yes; deepdefence.c:6505 - deep_military_choose_build - veterancy swap `adjust_ai_unit_choice` allowed only when `floor_def >= MAX(garrison_floor,1)` and `grave_danger == 0` - yes.
- **Log line:** `deep-floor` (eligible city-turns), `deep-sat` (satisfaction), `deep-rotate`/`deep-meltblock` (melt).
- **Interacts with:** garrison_floor_danger, garrison_floor_turn, garrison_menace_slope, satisfied_fear_discount, survival_target (via satisfaction), floor_*_weight.
- **Tier/screen:** unranked as a knob (the screen flags it `*`, a 0/1-style key whose sigma step lands on the other setting); flips 113.0/game (63.5% of 178 floor decisions/game) -- but "their flip count is what the other mode would change, not a tuning gradient".
- **Prior evidence:** STATUS.md:7147 explains why the floor off-switch is `garrison_floor_danger`, not this (it is "also read by deep_fear_satisfaction() and by the melt, so zeroing it would move three mechanisms"). STATUS.md:13901 arena demo-202, `garrison_floor = 0` one-key file -- attribution proof on smoke, not an effect estimate. No A/B on this field alone.

### garrison_floor_danger
- **Declared:** deeptuning.h:521 (`int garrison_floor_danger;`); params.ini: default=40 min=0 max=200 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** the value "injected ... into the danger field" of a city with fewer than `garrison_floor` units (same fix family as overseas_fear).
- **Unit:** want/score units -- "danger units", the same unsigned-int field stock `assess_danger()` writes. It is multiplied by dimensionless factors (updisc, menace, class weight, fmult) and truncated to `unsigned int`. It then enters `qdanger = danger^2` against `our_def` (see chain note). For an empty city the result is a flat 100 at EMERGENCY tier whenever the injected value is >= 1, so within the live range it acts as an on/off switch, not a magnitude.
- **Formula at the primary call site:** `applied = (unsigned int) (tune->garrison_floor_danger * upkeep_discount * menace * w.best * fmult); city_data->danger = applied;` -- deepdefence.c:1995-1997, `deep_apply_garrison_floor()`.
- **All call sites:** deepdefence.c:1995 - deep_apply_garrison_floor - magnitude of the floor injection (called from the ledger at deepdefence.c:2414 and the build seat at :5557) - yes.
- **Log line:** `deep-floor` (`applied=`, `w=`, `menace=`, `fmult=`, `updisc=`), also `deep-fear` `post=`.
- **Interacts with:** garrison_floor, garrison_floor_turn, garrison_menace_slope (via `menace`), floor_barb/sea/air/border_weight (via `w.best`), floor_interior_weight, floor_marines_mult, menace_overseas_weight, stance_floor_gain (via `fmult`), upkeep discount.
- **Tier/screen:** tier A (DEFOPS); knob-screen: flips 113.1/game, flip rate 63.5%, 178 decisions/game, replay rule garrison_floor (99.9% self-check).
- **Prior evidence:** STATUS.md:7140ff floor sizing (floor-danger=0 vs default), 20 paired seeds, pinned `536df20ee44c`: isolated-150 floor ON-OFF score -6.12 +-3.18 (t -1.93), cities -1.09 (t -2.77); lategame-80 -2.52 +-1.71 (t -1.47), cities -0.66 (t -4.16); i.e. removing the floor was better on both, not significant on score; city losses did not fall (+0.80, +1.40). STATUS.md:7372 "garrison_floor_danger (40) and garrison_menace_slope (1.0) are what actually size the tax". STATUS.md:14438 derby-6 "lean" file hand-zeroed {floor_interior_weight, garrison_floor_danger, boat_shadow_fear, density_fear_value}: DEEP 1498 vs CLASSIC 1161 (+337) against derby-5 -809, one game each, four knobs differ -- anecdote, not an A/B on this knob. Never swept by the tuner against held-out seeds with a verdict.

### garrison_floor_turn
- **Declared:** deeptuning.h:529 (`int garrison_floor_turn;`); params.ini: default=0 min=0 max=100 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Turn before which the token garrison floor does not apply... 0 (the default) keeps the floor from turn one, which is the shipped behaviour. Adjudicated by nothing yet -- it owes the tuning pile both a value and an arm."
- **Unit:** turns (compared with `game.info.turn`).
- **Formula at the primary call site:** `|| game.info.turn < tune->garrison_floor_turn` in the early-return of `deep_apply_garrison_floor()` -- deepdefence.c:1980.
- **All call sites:** deepdefence.c:1980 - deep_apply_garrison_floor - holds the floor off before turn N - read at defaults but a no-op at 0 (`turn < 0` is never true).
- **Log line:** `deep-floor` (absence of lines before turn N is the only evidence); no dedicated tag.
- **Interacts with:** garrison_floor, garrison_floor_danger, density_fear_ramp_turns (same early window).
- **Tier/screen:** tier A (DEFOPS) AND BANTICIP (tier B); not individually in the knob-screen tables.
- **Prior evidence:** STATUS.md:23451 "inert, unadjudicated, no rung sets it"; fearseat verdict section 6: "garrison_floor_turn | not run | not run | 0 -- unadjudicated"; STATUS.md:23796 "still never run". Net: none.

### garrison_menace_slope
- **Declared:** deeptuning.h:536 (`double garrison_menace_slope;`); params.ini: default=1.0 min=0.0 max=8.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Slope on the met-neighbour militarisation ratio (their summed military rating / ours) that scales the garrison floor: 0 before contact, capped at 1. Zero disables the scaling entirely."
- **Unit:** multiplier (dimensionless): scales a ratio of `score.units` sums, then `MIN(1.0, ...)`.
- **Formula at the primary call site:** `return MIN(1.0, tune->garrison_menace_slope * theirs / MAX(pplayer->score.units, 1));` -- deepdefence.c:1487-1488, `deep_garrison_menace()`; `theirs += aplayer->score.units * (reachable ? 1.0 : tune->menace_overseas_weight)`.
- **All call sites:** deepdefence.c:1440 - deep_garrison_menace - `<= 0` returns 1.0 (floor unconditional) - yes (value 1.0 > 0); deepdefence.c:1487 - deep_garrison_menace - menace = MIN(1, slope*theirs/ours), the factor on the floor and also on the deterrence dividend and the attacker garrison credit (callers pass `deep_garrison_menace(pplayer)` at deepdefence.c:5557 (floor), :5775 and :5919 (deterrence dividend), :7012 (attacker garrison credit), :8106 and :145 (logs)) - yes.
- **Log line:** `deep-floor menace=`, `deep-def menace=` (deepdefence.c:159-161), `deep-deter`/`deep-agc menace=`.
- **Interacts with:** menace_overseas_weight, garrison_floor_danger, floor_*_weight, deterrence_gain, attacker_garrison_gain.
- **Tier/screen:** tier B (BFEAR); knob-screen: tier-B partial-instrument, fires 3306/game (not a flip rate).
- **Prior evidence:** STATUS.md:7381 "next move on this mechanism is a magnitude sweep of garrison_floor_danger and garrison_menace_slope" -- not run; STATUS.md:800 "No consensus: barbarian_fear (44/79/13), menace_slope" (earlier tuner runs disagreed). No A/B arm: none.

### menace_overseas_weight
- **Declared:** deeptuning.h:537 (`double menace_overseas_weight;`); params.ini: default=0.25 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** wholly-overseas armies are counted "at menace_overseas_weight (they must build and sail boats before they are a garrison problem)"; also the cross-water discount in the density lattice.
- **Unit:** multiplier (dimensionless 0-1), applied to a neighbour's `score.units` (menace) or to a per-city fear weight (density, border pressure).
- **Formula at the primary call site:** `theirs += aplayer->score.units * (reachable ? 1.0 : tune->menace_overseas_weight);` -- deepdefence.c:1480-1481, `deep_garrison_menace()`.
- **All call sites:** deepdefence.c:1028 - deep_density_fear_raw - cross-continent neighbour with no transports: `w *= menace_overseas_weight` - yes; deepdefence.c:1118 - deep_border_pressure_from - same discount, feeds stance source 2 and the gift/peace valuation - yes (the gift-family consumers in deepdiplomacy.c are dead-path per the screen); deepdefence.c:1481 - deep_garrison_menace - overseas army weight - yes. Also read indirectly by deeptrunk.c via `deep_density_fear_at`.
- **Log line:** `deep-density` (`cross=`, `discounted=`, `raw=`/`fear=`), `deep-floor menace=`.
- **Interacts with:** garrison_menace_slope, density_fear_value, density_fear_radius, density_fear_capability, floor_interior_weight (same value 0.25 by design).
- **Tier/screen:** tier B (BFEAR); fires 3306/game, not a flip rate.
- **Prior evidence:** STATUS.md:7337 rider "menace_overseas_weight 0.25 -> 1.0 on isolated-150, 20 paired seeds, same pin": score 197.70 vs 196.14 = +1.56 +-0.76 (t +2.05), city losses -0.05 +-0.45, floor firings -16.35 (t -3.18) -- positive direction; it moves BOTH the density and the menace discount (not a one-term test). Flagged "worth re-running at forty seeds" (STATUS.md:7392). Not adopted.

### floor_barb_weight
- **Declared:** deeptuning.h:552 (`double floor_barb_weight;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** weight of the barbarian class in the floor "while each threat class the danger assessment cannot see is LIVE"; floor takes the MAX over live classes.
- **Unit:** multiplier (dimensionless) on `barbarian scale` (itself normalised to 1.0 at barb_size_ref_cities cities under BARBS_NORMAL).
- **Formula at the primary call site:** `w->barb = tune->floor_barb_weight * deep_barb_city_scale(pcity);` -- deepdefence.c:1858, `deep_floor_weigh()`; then `w->best = MAX(MAX(w->barb, w->sea), MAX(w->air, w->border))` (1925).
- **All call sites:** deepdefence.c:1858 - deep_floor_weigh - barbarian class weight - conditional: live only after the anticipation ramp (onset - barb_anticipation_turns) with rate > BARBS_HUTS_ONLY and an unrest-eligible city (civ2civ3 onsetbarbs=120).
- **Log line:** `deep-floor wbarb=` (and `barb=` class flag).
- **Interacts with:** barbarian_fear, barb_size_ref_cities, barb_anticipation_turns, garrison_floor_danger.
- **Tier/screen:** unranked; knob-screen: flips 0.6/game, 0.3% -- "on the shortlist only nominally... its wbarb component is also usually dominated".
- **Prior evidence:** STATUS.md:10149 "must be tuned WITH barbarian_fear -- alone it just rescales them"; no arm moved it. none.

### floor_sea_weight
- **Declared:** deeptuning.h:553 (`double floor_sea_weight;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** sea-landing class weight; "the ceiling stays at floor_sea_weight so no live class can buy MORE floor than the ungated floor did".
- **Unit:** multiplier (dimensionless) -- scales the geometric class weight; exterior/interior and marines factors multiply it.
- **Formula at the primary call site:** `w->sea = tune->floor_sea_weight * (exterior ? 1.0 / esc : tune->floor_interior_weight);` (pre-marines branch; marines branch `(exterior ? 1.0 : floor_interior_weight / esc)`) -- deepdefence.c:1877-1881, `deep_floor_weigh()`.
- **All call sites:** deepdefence.c:1877 and :1880 - deep_floor_weigh - sea class weight, two branches - yes (class live once a met rival owns a landing hull; first seen T62-81 on isolated-150).
- **Log line:** `deep-floor wsea=` / `sea=` / `mar=`.
- **Interacts with:** floor_interior_weight, floor_marines_mult, floor_border_weight (invariant: border >= sea), garrison_floor_danger.
- **Tier/screen:** tier A (DEFOPS); flips 63.4/game, 35.6% (corpus retake; 55.2 on the s5 replicate).
- **Prior evidence:** STATUS.md:7381 "floor_sea_weight, which is the binding class on both cells" -- sweep recommended, not run. none (as an arm).

### floor_air_weight
- **Declared:** deeptuning.h:554 (`double floor_air_weight;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** weight of the airborne class (within air range of a rival city; carriers extend to open water).
- **Unit:** multiplier (dimensionless) on the air class flag (0/1 geometry).
- **Formula at the primary call site:** `w->air = tune->floor_air_weight;` -- deepdefence.c:1916, `deep_floor_weigh()`, guarded by `fc->air && (carrier ? deep_open_water_within(...) : near_any <= fc->air_range)`.
- **All call sites:** deepdefence.c:1916 - deep_floor_weigh - air class weight - no: only when a met rival's KNOWN tech can build an airborne unit (flight era); 0 `air=1` lines in 40 games on both standing cells.
- **Log line:** `deep-floor wair=`.
- **Interacts with:** garrison_floor_danger, floor_*_weight.
- **Tier/screen:** unranked; knob-screen "inert under replay": `wair` is 0.000 on every logged floor line.
- **Prior evidence:** STATUS.md:7255 ("0 air=1 lines in all 40 games"): class code "proven inert, not proven correct". none.

### floor_border_weight
- **Declared:** deeptuning.h:555 (`double floor_border_weight;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** land-neighbour class weight; "floor_border_weight should not fall below floor_sea_weight, or an overseas-only rival would outweigh a land neighbour for a border city".
- **Unit:** multiplier (dimensionless) on a distance shape R/(R+d) in (0,1], with R = density_fear_radius and d = tiles to the nearest met non-allied city on this city's landmass.
- **Formula at the primary call site:** `w->border = tune->floor_border_weight * (double) tune->density_fear_radius / (tune->density_fear_radius + near_same);` -- deepdefence.c:1907-1909, `deep_floor_weigh()`.
- **All call sites:** deepdefence.c:1907 - deep_floor_weigh - border class weight - yes (94% of eligible lategame city-turns have a land neighbour on the same landmass).
- **Log line:** `deep-floor wborder=` / `border=`.
- **Interacts with:** density_fear_radius, floor_sea_weight, garrison_floor_danger.
- **Tier/screen:** tier A (DEFOPS); flips 53.3/game, 30.0% (62.2 on the s5 replicate).
- **Prior evidence:** tune-postfix-verdict-2026-08-25.md:49 lists `floor_border_weight=0` among constants the tuner drove to a bound -- that campaign was rejected by the holdout, so no usable effect. none.

### floor_interior_weight
- **Declared:** deeptuning.h:569 (`double floor_interior_weight;`); params.ini: default=0.25 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "A city whose water leads nowhere (a closed lake...) or that has no water at all is INTERIOR and carries the class at floor_interior_weight".
- **Unit:** multiplier (dimensionless 0-1) on the sea class weight, interior cities.
- **Formula at the primary call site:** `w->sea = tune->floor_sea_weight * (exterior ? 1.0 / esc : tune->floor_interior_weight);` -- deepdefence.c:1880-1881 (pre-marines); the marines branch divides it by `esc` (1877-1878).
- **All call sites:** deepdefence.c:1878 and :1881 - deep_floor_weigh - interior sea weight - yes.
- **Log line:** `deep-floor wsea=` ("takes exactly the four values the geometry defines").
- **Interacts with:** floor_sea_weight, floor_marines_mult, menace_overseas_weight (same 0.25 by design).
- **Tier/screen:** tier B (BFEAR); not individually in the knob-screen tables.
- **Prior evidence:** STATUS.md:14438 derby-6 "lean" file hand-zeroed it with three other knobs (see garrison_floor_danger; single-game anecdote); STATUS.md:14734 notes two files set it twice (ini hygiene, not an effect). none as an isolated arm.

### floor_marines_mult
- **Declared:** deeptuning.h:570 (`double floor_marines_mult;`); params.ini: default=1.5 min=1.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "moves both weights DOWNWARD from full and never upward past it: before attack-from-hull tech exists anywhere the exterior weight is 1/mult... once it exists the exterior weight reaches full while the interior weight is divided by mult."
- **Unit:** multiplier (dimensionless, >= 1; divisor of the weights).
- **Formula at the primary call site:** `double esc = MAX(tune->floor_marines_mult, 1.0);` then `1.0 / esc` (exterior, pre-marines) or `floor_interior_weight / esc` (interior, marines) -- deepdefence.c:1864, 1877-1881, `deep_floor_weigh()`.
- **All call sites:** deepdefence.c:1864 - deep_floor_weigh - escalation divisor for the sea class - yes.
- **Log line:** `deep-floor mar=` (class flag), `wsea=`.
- **Interacts with:** floor_sea_weight, floor_interior_weight.
- **Tier/screen:** tier B (BFEAR); not individually screened.
- **Prior evidence:** none.

### barbarian_fear
- **Declared:** deeptuning.h:579 (`int barbarian_fear;`); params.ini: default=30 min=0 max=200 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Base danger for the barbarian era, multiplied by the SHARED barbarian scaling... that mirrors the server's own uprising formula: the pre-onset ramp, the (barbarianrate - 1) frequency scale, the willingness probability, the government's civil-war chance and the horde ladder."
- **Unit:** want/score units (danger units, same field as the other injected fears), times a dimensionless scale (1.0 at barb_size_ref_cities cities, BARBS_NORMAL).
- **Formula at the primary call site:** `return (unsigned int) (tune->barbarian_fear * deep_barb_city_scale(pcity));` -- deepdefence.c:1359, `deep_barbarian_fear()`; added as `barb * updisc * sat * fmult` into danger (deepdefence.c:2387 ledger, :5530 build).
- **All call sites:** deepdefence.c:1359 - deep_barbarian_fear - fear term injected into city danger (both seats) - conditional: zero before onset - lead and zero for barbarian rate <= HUTS_ONLY; deepdefence.c:1397 - deep_barb_census - log only.
- **Log line:** `deep-barb` (`scale=`, `fear=`, `pre=`), `deep-fear barb=`.
- **Interacts with:** barb_anticipation_turns, barb_size_ref_cities, floor_barb_weight, stance_fear_gain.
- **Tier/screen:** unranked; knob-coverage NO-INSTRUMENT; not screened.
- **Prior evidence:** STATUS.md:10062 barbarian re-shape A/B (the SHAPE, not this magnitude): raging-120, 20 paired seeds, TIP vs STEP: score +0.17 +-1.36 (t +0.13), city losses +0.25 +-2.48 -- wash; on contested "the re-shaped barbarian term truncates to ZERO". STATUS.md:800 "No consensus: barbarian_fear (44/79/13)" across tuner runs. none on magnitude.

### barb_anticipation_turns
- **Declared:** deeptuning.h:585 (`int barb_anticipation_turns;`); params.ini: default=10 min=0 max=40 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "How many turns BEFORE game.server.onsetbarbarian the terror starts ramping in, linearly, from zero to full... The knob is the LEAD, not the shape... 0 restores the step function."
- **Unit:** turns.
- **Formula at the primary call site:** `return (double) (game.info.turn - (game.server.onsetbarbarian - lead)) / lead;` -- deepdefence.c:1252-1253, `deep_barb_ramp()` (`int lead = tune->barb_anticipation_turns;` at 1243).
- **All call sites:** deepdefence.c:1243 - deep_barb_ramp - ramp length - conditional (barb era); deepdefence.c:1394 - deep_barb_census - log only.
- **Log line:** `deep-barb lead=` and `pre=`.
- **Interacts with:** barbarian_fear, floor_barb_weight, barb_size_ref_cities; the same ramp feeds `deep_barb_walkin_odds` (birth guard).
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** STATUS.md:10145 listed as owed to the tuning pile; the barbarian-reshape A/B above changed it together with the shape (wash). null.

### barb_size_ref_cities
- **Declared:** deeptuning.h:592 (`int barb_size_ref_cities;`); params.ini: default=10 min=1 max=40 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Empire size (in cities) at which the shared barbarian scaling is exactly 1.0, under BARBS_NORMAL. Pure normalisation... Raising it makes the whole class milder, lowering it makes it fiercer."
- **Unit:** count (cities); a normaliser of a dimensionless scale.
- **Formula at the primary call site:** `ref_n = MAX(1, tune->barb_size_ref_cities); ref_will = MIN(1.0, ref_n * (BARBS_NORMAL - 1) / 30.0); ref_horde = 1.0 + deep_barb_uprise(ref_n) * BARBS_NORMAL;` returning `ramp * (rate-1) * cwc * (will*horde) / (ref_will*ref_horde)` -- deepdefence.c:1282-1292, `deep_barb_empire_scale()`.
- **All call sites:** deepdefence.c:1282 - deep_barb_empire_scale - normaliser - conditional (barb era); feeds barbarian_fear and the floor's barb class (NOT the walk-in odds `deep_barb_walkin_odds`).
- **Log line:** `deep-barb scale=`.
- **Interacts with:** barbarian_fear, floor_barb_weight, barb_anticipation_turns.
- **Tier/screen:** tier B (BFEAR); knob-screen fires 156/game (partial instrument).
- **Prior evidence:** none as an arm; STATUS.md:10146 "must be tuned WITH barbarian_fear and floor_barb_weight".

### density_fear_value
- **Declared:** deeptuning.h:621 (`int density_fear_value;`); params.ini: default=6 min=0 max=40 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Border-pressure fear: each met non-allied city within the radius adds value/(1+dist) standing danger. 0 disables."
- **Unit:** want/score units (danger units, per neighbouring city, divided by (1 + distance in tiles)). Summed into `city_data->danger` (uint) after `* ramp * updisc * sat * fmult`.
- **Formula at the primary call site:** `w = tune->density_fear_value / (1.0 + d); ... fear += w;` (per neighbour city within radius; reachability and capability discounts multiply `w`) -- deepdefence.c:989, `deep_density_fear_raw()`; then `return (unsigned int) (fear * deep_anticipation_ramp());` in `deep_density_fear()` (deepdefence.c:~924-948).
- **All call sites:** deepdefence.c:968 - deep_density_fear_raw - `<= 0` disables - yes; deepdefence.c:989 - deep_density_fear_raw - per-city weight (used by the ledger density term at 2350, the build-seat density term at 5495, and the corridor tile risk `deep_trunk_tile_risk`, deeptrunk.c:629, only if the trunk machinery is on) - yes; deepdefence.c:1091 and :1103 - deep_border_pressure_from - the same lattice per player pair, consumed by stance source 2 (deepstance.c:422) and the gift/peace valuation (deepdiplomacy.c:444, 793, 1211, 1324; gift family is dead-path at defaults per the screen) - yes as code, consumers mostly dead.
- **Log line:** `deep-density` (unconditional; `near= cross= discounted= nocap= raw= gated= ramp= fear=`), `deep-fear density=`.
- **Interacts with:** density_fear_radius, density_fear_ramp_turns, fear_evidence_gate/floor, density_fear_capability, menace_overseas_weight, stance_fear_gain, satisfied_fear_discount, upkeep discount.
- **Tier/screen:** unranked; knob-coverage NO-INSTRUMENT (before the deep-density census was made unconditional); the census shows the term on 54.9% (T1-25) and 72.1% (T26-50) of build-seat evaluations. Not screened.
- **Prior evidence:** fearseat-verdict-2026-08-30.md sections 2-3 measure the term's seat share, but the arms moved the RAMP and the EVIDENCE gate, not this value. STATUS.md:18431 derby-7 ini zeroes `density_fear_value` (and `boat_shadow_fear`) -- derby-6 "lean winner" (single game, four fear/floor knobs together, see garrison_floor_danger). none as an isolated arm.

### density_fear_radius
- **Declared:** deeptuning.h:622 (`int density_fear_radius;`); params.ini: default=8 min=2 max=16 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** the radius within which met non-allied cities contribute; reused as R of the floor's border class `R/(R+d)` and as the unit-count radius of the evidence gate.
- **Unit:** tiles (compared with `real_map_distance`).
- **Formula at the primary call site:** `if (d > tune->density_fear_radius) { continue; }` -- deepdefence.c:986, `deep_density_fear_raw()`.
- **All call sites:** deepdefence.c:896 - deep_hostile_capability_near - radius of the observed-capability count - yes (evid= is computed even with the gate off); :986 - deep_density_fear_raw - neighbour cutoff - yes; :1100 - deep_border_pressure_from - cutoff - yes; :1908-1909 - deep_floor_weigh - `R/(R+near_same)` in the floor's border class - yes; deepsettler.c:3395 - deep_ground_weight - same R in the base-fortress ground weight (`R/(R+d)`) - yes (feeds base_fort_gain).
- **Log line:** `deep-density near=`, `deep-floor wborder=`, `deep-fort` (indirect).
- **Interacts with:** density_fear_value, floor_border_weight, fear_evidence_floor, base_fort_gain.
- **Tier/screen:** unranked; knob-coverage NO-INSTRUMENT. Not screened.
- **Prior evidence:** STATUS.md:15430 notes "density_fear_radius = 8 puts an enemy city within reach of nearly every tile" on a 32x32 map (ground weight saturates near 1.0). No arm moved it. none.

### density_fear_ramp_turns
- **Declared:** deeptuning.h:623 (`int density_fear_ramp_turns;`); params.ini: default=0 min=0 max=200 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** (comment at deepdefence.c:777) "scales from 0 toward 1 over the first density_fear_ramp_turns turns of the game. A neighbour's mere existence is not a capability... Ramp 0 disables (current behaviour)." Evidence-based terms are NOT ramped.
- **Unit:** turns (denominator of a linear 0..1 ramp).
- **Formula at the primary call site:** `return MIN(1.0, (double) game.info.turn / tune->density_fear_ramp_turns);` -- deepdefence.c:793, `deep_anticipation_ramp()`.
- **All call sites:** deepdefence.c:789 and :793 - deep_anticipation_ramp - ramp factor for the density term (deep_density_fear, deep_density_fear_at) and the prior (deep_density_prior at 833) - read at defaults but returns exactly 1.0 (a no-op) when 0 -> inert at compiled default.
- **Log line:** `deep-density ramp=` / `gated=`.
- **Interacts with:** density_fear_value, density_prior_value, fear_evidence_gate.
- **Tier/screen:** tier B (BANTICIP); not individually screened.
- **Prior evidence:** S-FEARSEAT (fearseat-verdict-2026-08-30.md; STATUS.md:23489), TRAINING seeds 1-20, ramp=62 vs off, paired: contested score -2.48 +-1.63 (3/20 wins), cities -0.10 +-0.06, `munits@25` +0.01 +-0.03 (did not shrink the opening army); lategame-80 score +1.93 +-1.27 (15/20). Opposite signs by venue; contested T76-125 shows the deferred fear arrives larger (mean post 10.83 -> 14.03, 11.55 -> 20.36). Verdict: ships 0. n=20 per venue. S-FEARSINK did not include it.

### fear_evidence_gate
- **Declared:** deeptuning.h:650 (`int fear_evidence_gate;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "EVIDENCE, off by default... the anticipatory terms count CITIES and never units... fear_evidence_floor is the falsifiable form of that sentence". Gate 0 "keeps the terms as they ship".
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (tune->fear_evidence_gate <= 0 || tune->fear_evidence_floor <= 0) { return TRUE; }` -- deepdefence.c:916-918, `deep_fear_evidence_met()`; and `if (tune->fear_evidence_gate > 0) {... w *= tune->density_fear_capability ...}` -- deepdefence.c:998.
- **All call sites:** deepdefence.c:916 - deep_fear_evidence_met - gate on conversion of density (ledger 2370, build 5516) and prior (build 5516) into danger - no: returns TRUE (no gating) at default 0; deepdefence.c:998 - deep_density_fear_raw - per-neighbour capability discount - no: dead behind gate=0.
- **Log line:** `deep-fear evid=` (measured whether or not the gate acts), `deep-density nocap=`.
- **Interacts with:** fear_evidence_floor, density_fear_capability, density_fear_radius, density_fear_value, density_prior_value.
- **Tier/screen:** unranked (BANTICIP holds floor and capability, not the gate); not screened.
- **Prior evidence:** S-FEARSEAT rung `evid` (STATUS.md:23490): contested score -2.40 +-1.32 (1/20 wins), econ +3.84 +-1.42 (13/20); `munits@25` +0.01; density share T1-25 54.0% -> 45.4%. Ships 0 (venue sign-flip, n=20). Rung `all` (ramp+evid+price): contested +0.98 +-2.08, econ +3.22 +-1.54.

### fear_evidence_floor
- **Declared:** deeptuning.h:651 (`int fear_evidence_floor;`); params.ini: default=1 min=0 max=8 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "the least number of non-allied ATTACK-CAPABLE units that must stand within density_fear_radius of a city before density and prior convert into danger there at all."
- **Unit:** count (units with `attack_strength > 0`, non-civilian, owned by met non-allied players).
- **Formula at the primary call site:** `return (deep_hostile_capability_near(pplayer, ptile, NULL) >= tune->fear_evidence_floor);` -- deepdefence.c:920-921, `deep_fear_evidence_met()`.
- **All call sites:** deepdefence.c:916 and :921 - deep_fear_evidence_met - threshold - no: only consulted past the `fear_evidence_gate > 0` early return.
- **Log line:** `deep-fear evid=`.
- **Interacts with:** fear_evidence_gate, density_fear_radius.
- **Tier/screen:** tier B (BANTICIP); not screened.
- **Prior evidence:** only at floor 1 inside the S-FEARSEAT `evid`/`all` rungs: floor 1 zeroes the anticipatory terms on 38.0% (T1-25) / 32.5% (T26-50) of evaluations (`evid=0`); effects as under fear_evidence_gate. The floor value was never swept.

### density_fear_capability
- **Declared:** deeptuning.h:652 (`double density_fear_capability;`); params.ini: default=0.25 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "discounts one NEIGHBOUR rather than the whole sum: a met non-allied player showing no attack-capable unit in that radius has its cities weighted by this factor, exactly as a neighbour whose army cannot cross the water is already weighted by menace_overseas_weight."
- **Unit:** multiplier (dimensionless 0-1) on a neighbour's per-city density weight.
- **Formula at the primary call site:** `if (armed == 0) { w *= tune->density_fear_capability; n_cap++; }` -- deepdefence.c:1003-1005, `deep_density_fear_raw()`.
- **All call sites:** deepdefence.c:1004 - deep_density_fear_raw - per-neighbour capability discount - no: inside `if (tune->fear_evidence_gate > 0)`.
- **Log line:** `deep-density nocap=`.
- **Interacts with:** fear_evidence_gate, density_fear_value, menace_overseas_weight (same law, same default).
- **Tier/screen:** tier B (BANTICIP); not screened.
- **Prior evidence:** S-FEARSEAT `evid`/`all` rungs ran at 0.25 only (see fear_evidence_gate); value never swept.

### density_prior_value
- **Declared:** deeptuning.h:678 (`int density_prior_value;`); params.ini: default=20 min=0 max=100 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Pre-contact prior from lobby settings: shared-continent startpos on thin land predicts early contact before anyone is met. Value scales by min(1, prior_tiles / land-per-player); off at first contact. 0 disables."
- **Unit:** want/score units (danger units), times dimensionless crowding (0-1) and ramp (0-1).
- **Formula at the primary call site:** `return (unsigned int) (tune->density_prior_value * deep_map_crowding() * deep_anticipation_ramp());` -- deepdefence.c:831-833, `deep_density_prior()`.
- **All call sites:** deepdefence.c:801 - deep_density_prior - `<= 0` disables - yes; :831 - deep_density_prior - the prior magnitude, BUILD SEAT ONLY (`prior` at deepdefence.c:5496; the ledger logs `prior=0`) - yes (pre-contact only; returns 0 if startpos resolves to SINGLE/own continent, or after any non-allied contact).
- **Log line:** `deep-fear prior=` (build seat); the prior has no `deep-density` line.
- **Interacts with:** density_prior_tiles, density_fear_ramp_turns, fear_evidence_gate (zeroes it when gated), stance_fear_gain, upkeep discount and sat.
- **Tier/screen:** unranked; knob-coverage NO-INSTRUMENT; not screened.
- **Prior evidence:** none (STATUS.md has no hit; tuning-first-principles-2026-08-06.md lists it as a constant).

### density_prior_tiles
- **Declared:** deeptuning.h:679 (`int density_prior_tiles;`); params.ini: default=200 min=50 max=1000 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** reference land-per-player for the crowding prior. The `land_pressure_tiles` comment (deeptuning.h:284) says that knob is "Deliberately its own knob rather than reusing density_prior_tiles, so the tuner cannot move site spacing while it is really adjusting a fear prior" -- yet `deep_map_crowding()` has non-fear consumers (below).
- **Unit:** tiles (land tiles per player).
- **Formula at the primary call site:** `return MIN(1.0, (double) tune->density_prior_tiles / MAX(land_per_player, 1));` -- deepdefence.c:736-737, `deep_map_crowding()`.
- **All call sites:** deepdefence.c:736 - deep_map_crowding - crowding in [0,1]. Consumers of that crowding: (1) the density PRIOR (deepdefence.c:831) -- yes pre-contact; (2) deepdefence.c:9262-9268 `deep_manage_cities` -- `founder_want *= (1.0 + settler_rush_weight * deep_map_crowding())` (fresh wants only; `settler_rush_weight` default 1.0, so at maximum crowding the settler bid DOUBLES) -- yes, live; (3) deepsite.c:1322 `deep_cityresult_fill` -- `wspecials += site_choke_bonus * deep_map_crowding()` for choke sites (`site_choke_bonus` default 12.0) -- yes, live; (4) deepsite.c:2423 `deep_settler_map_iterate` -- `relief = 1 - site_travel_crowding * deep_map_crowding()` -- no: `site_travel_crowding` default 0.0 makes it a no-op. So this "fear prior" knob also moves the settler bid and site scoring.
- **Log line:** none for crowding itself (`deep-fear prior=` reflects it; founder/site logs reflect (2)/(3) indirectly).
- **Interacts with:** density_prior_value, settler_rush_weight, site_choke_bonus, site_travel_crowding, density_fear_ramp_turns.
- **Tier/screen:** unranked; knob-coverage NO-INSTRUMENT; not screened.
- **Prior evidence:** none.

### wall_fear_weight
- **Declared:** deeptuning.h:682 (`double wall_fear_weight;`); params.ini: default=0.3 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Injected fears also fund WALLS (the upkeep-free defence): building_want[walls] += weight * danger. 0 disables."
- **Unit:** multiplier (dimensionless) converting danger units into a building want (want units): `building_want[wall] += weight * danger`.
- **Formula at the primary call site:** `pcity->server.adv->building_want[wall_id] += deep_tuning_get()->wall_fear_weight * (deep_tuning_get()->mil_infra_gate > 0 ? fear_pre : city_data->danger);` -- deepdefence.c:5838-5841, `deep_military_choose_build()`.
- **All call sites:** deepdefence.c:5828 (guard `> 0.0 && danger > 0 && wall_id != B_LAST`) and :5839 (the add) - deep_military_choose_build - walls bid, reached only when `build_walls` is still TRUE (not the first-defender branch) - yes (`mil_infra_gate`=0 so the basis is the INJECTED danger).
- **Log line:** none dedicated; the building choice is only in CITY_LOG(LOG_DEBUG) "defense building".
- **Interacts with:** every injected fear term (it sums them), mil_infra_gate, mil_infra_threat_floor, TRAIT_BUILDER scaling (`0.5 + builder/2`).
- **Tier/screen:** unranked; knob-coverage NO-INSTRUMENT; not screened.
- **Prior evidence:** none as an arm. tune-postfix-verdict-2026-08-25.md:49 lists `wall_fear_weight=0` as a bound the rejected tuner hit; the mil-infra gate (S-DIPLOPRICE, `mil_infra_gate`, default 0) would change its basis to stock's measured danger but is a separate knob (STATUS.md:10781 records the Defend_Bonus sweep item).

### birth_guard_gain
- **Declared:** deeptuning.h:693 (`double birth_guard_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The FIRST defender in an empty city, priced instead of pinned: want = P(walk-in) x ai_city.worth x gain, at NORMAL tier and through the stance fear gate... 0 restores the inherited rule exactly -- a flat danger of 100 at DWT_EMERGENCY whenever urgency is 0".
- **Unit:** multiplier (dimensionless) on `p (probability) * worth (want units)`; result in want units at `DWT_NORMAL` tier (compare the inherited flat 100 at `DWT_EMERGENCY`).
- **Formula at the primary call site:** `*want_out = p * worth * tune->birth_guard_gain * deep_stance_fear_mult(pplayer);` -- deepdefence.c:5051-5052, `deep_birth_guard_want()`; applied at ~5777-5781 as `.tier = DWT_NORMAL, .value = priced + div`.
- **All call sites:** deepdefence.c:5040 - deep_birth_guard_want - `<= 0` returns FALSE (inherited flat-100 emergency rule) - no: dead behind gain=0; :5051 - the pricing - no: dead at 0.
- **Log line:** `deep-birth` (per priced decision), `deep-birthguard` (per-turn counters `cands/priced/urgent`; `cands` counts even at 0).
- **Interacts with:** survival_horizon (wave), barb_anticipation_turns / barbarian ramp (via `deep_barb_walkin_odds`), stance_fear_gain, deterrence dividend `div`.
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** STATUS.md:10435 (CONSEQ/WRACE session), contested, 20 paired seeds, pin 724e885dc2ab61, gain 1.0: score -0.11 +-2.49 (t -0.05), cities +0.16 +-0.12, city losses +0.20 +-0.47 -- wash; 88.6% of 14 955 first-defender decisions priced at P(walk-in) == 0, mean priced want 13.11 vs the inherited flat 100, 96.0% below it. The tier change (EMERGENCY -> NORMAL) is part of the lever. n=20, one venue.

### defensive_techwall_weight
- **Declared:** deeptuning.h:704 (`double defensive_techwall_weight;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "a city whose survival is below target and whose best BUILDABLE land defender is beaten by one the ruleset offers at reachable tech routes survival_fear_value x deficit x weight into tech_want[] for the gating advance... A pure ratio -- how many turns of the fear a turn of research is worth."
- **Unit:** multiplier (dimensionless ratio) turning the survival fear (danger units) into tech_want units.
- **Formula at the primary call site:** `credit = tune->survival_fear_value * deficit * tune->defensive_techwall_weight;` -- deepdefence.c:2512-2513, `deep_defensive_techwall()`; added to `def_ai_player_data(...)->tech_want[advance]` (~2531).
- **All call sites:** deepdefence.c:2477 - deep_defensive_techwall - `<= 0.0` returns - no: dead behind weight=0; :2513 - credit - no: dead at 0. Called once per player per turn from deepdefence.c:9184.
- **Log line:** `deep-defwall` (per routed credit).
- **Interacts with:** survival_target, survival_fear_value, survival_horizon.
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** none (findings/conseq-wrace-2026-08-07.md mentions the design only).

### attacker_hold_damper
- **Declared:** deeptuning.h:723 (`double attacker_hold_damper;`); params.ini: default=0.0 min=0.0 max=8.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "never-launches damper (CONSEQ item 5, derby N3)... saturation is a RATE -- assault operations formed that never reached MARCH, over all formed"; 1/(1 + gain x saturation); exempt for a target that threatens us.
- **Unit:** multiplier (dimensionless): `damp = 1/(1 + gain * stuck)`, `stuck` in [0,1].
- **Formula at the primary call site:** `double damp = 1.0 / (1.0 + tune->attacker_hold_damper * stuck); ... want = (adv_want) (want * damp);` -- deepdefence.c:6994-7003, `deep_assault_want()` (the offensive build want).
- **All call sites:** deepdefence.c:6989 - deep_assault_want - scales the assault build want by the stalled-operation rate - no: dead behind gain=0 (`> 0.0` guard).
- **Log line:** `deep-damper pump=attacker` (formed/launched/stuck/damp); `deep-opodds` carries running formed/launched totals.
- **Interacts with:** stance_assault_gain (applied just before), attacker_garrison_gain (applied after, deliberately not damped), op_take_target.
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** none (STATUS.md:12771 only records where it lives).

### diplomat_pump_damper
- **Declared:** deeptuning.h:724 (`double diplomat_pump_damper;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "saturation is a COUNT, of diplomat-class units walking plus queued... the identical (prize)/(1 + holders + queued) shape deepexplore uses"; 1.0 exactly at 0.
- **Unit:** multiplier (dimensionless) per diplomat agent: `damp = 1/(1 + gain * agents)`, `agents` a count.
- **Formula at the primary call site:** `double damp = 1.0 / (1.0 + deep_tuning_get()->diplomat_pump_damper * agents); ... dip->want = (adv_want) (dip->want * damp);` -- deepdefence.c:6089-6097, `deep_military_choose_build()` (the embassy/diplomat build want).
- **All call sites:** deepdefence.c:6072 - deep_military_choose_build - divides the embassy/diplomat build want by (1 + gain*agents) - no: dead behind gain=0.
- **Log line:** `deep-damper pump=diplomat`.
- **Interacts with:** embassy_science_value, diplomat build want.
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** none.

### base_fort_gain
- **Declared:** deeptuning.h:828 (`double base_fort_gain;`); params.ini: default=40.0 min=0.0 max=200.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "What a defensive BASE is worth to a worker, in the same currency the advisor values a tile's output in... this gain is the entire want. Multiplied by the survival a defender standing on the tile gains from the base (0..1...) and by how much the ground matters (0..1...)". 0.0 "restores byte identity".
- **Unit:** want/score units (the advisor's tile-output currency): gain x (delta survival 0-1) x (ground weight 0-1).
- **Formula at the primary call site:** `want = tune->base_fort_gain * (p_with - p_now) * ground;` -- deepsettler.c:3488, `deep_base_want()`; added to the worker task value: `addon += deep_base_want(pplayer, target, ptile);` -- deepsettler.c:3901, `deep_worker_task_value()` (after the danger discount, exempt from it).
- **All call sites:** deepsettler.c:3455 - deep_base_want - `<= 0` disables - yes; :3488 - deep_base_want - base want for ACTIVITY_BASE candidates the advisor offers (a hook in server/advisors/autoworkers via `aw_task_value_fn`) - yes (positive base wants run ~0 to 13.58, median 6.50 on contested).
- **Log line:** `deep-fort` (offers/evals), `deep-fortwant` (want distribution), `deep-fortsat` (saturation).
- **Interacts with:** fortify_ground (consumer), worker_danger_discount, survival model (`deep_fort_survival`), density_fear_radius (via ground weight), chokepoints (ground = 1.0).
- **Tier/screen:** tier B (BCIVIL); knob-screen: fires 371/game (partial instrument, not a flip rate).
- **Prior evidence:** STATUS.md:9964 ground round (S-GROUND), file-vs-file `ground-off.ini` (base_fort_gain=0, fortify_ground=0, pillage=0) vs defaults, 20 paired seeds, pinned `dfde58a6691b`: contested (civ2civ3) score +4.03 +-1.98 (t +2.03, 13/20), city losses +0.55 +-0.42; contested-classic score -1.52 +-1.14 (t -1.33), city losses -0.90. Isolation (STATUS.md:10015): base want + pillage with NO fortify hold: -0.17 +-0.89 (wash); adding the hold: +4.20 +-2.14 -> the producer is a wash, the consumer is the prize; the 2x2 base_fort_gain x fortify_ground is "owed". STATUS.md:15386: derby-7 ini ran 15.6 vs 40.0 -- want distribution fell ~4x but "behaviour does not move at all" (the carpet is not price-sensitive).

### fortify_ground
- **Declared:** deeptuning.h:834 (`int fortify_ground;`); params.ini: default=1 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Fortify a military unit already standing on ground the base want values -- a defensive base, or a chokepoint -- instead of letting the manage loop wander it off. Runs after every claiming pass... 0 disables and restores byte identity."
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (tune->fortify_ground <= 0) { return; }` -- deepdefence.c:4322, `deep_fortify_ground()`; units on a base native to their class or `deep_site_is_choke()` are fortified and phase-held.
- **All call sites:** deepdefence.c:4322 - deep_fortify_ground - gate on the whole pass (called from `deep_do_first_activities`, deepdefence.c:4394) - yes.
- **Log line:** `deep-ground` (eligible/held).
- **Interacts with:** base_fort_gain (what makes ground "worth holding" is bases).
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** same S-GROUND A/B as base_fort_gain (bundle); isolation arm `fortify_ground=0` with the two wants on: -0.17 +-0.89 vs +4.20 +-2.14 with the hold on (contested civ2civ3, n=20) -- positive, but the hold also fires on chokepoints so it is not attributable to the fortresses (STATUS.md:10031).

### pillage_denial_gain
- **Declared:** deeptuning.h:840 (`double pillage_denial_gain;`); params.ini: default=1.0 min=0.0 max=10.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Value of denying an enemy one point of tile output by pillaging the extra that produces it. A unit already standing on an at-war-owned tile pillages the best extra when gain x denied output reaches 1. 0 disables the whole pass."
- **Unit:** multiplier (dimensionless) on tile-output points (food + shield + trade lost, `deep_pillage_denial`); the trigger threshold is a hard-coded literal 1.0 in the source (so the gain also acts as a threshold).
- **Formula at the primary call site:** `want = tune->pillage_denial_gain * deep_pillage_denial(ptile, pextra); ... if (best != NULL && best_want >= 1.0)` -- deepdefence.c:4265-4268 and :4275, `deep_pillage_sorties()`.
- **All call sites:** deepdefence.c:4211 - deep_pillage_sorties - `<= 0.0` disables the pass - yes; :4265 - deep_pillage_sorties - pillage want per extra - yes (war only: unit already on an at-war-owned tile; ~8-10 pillages started per game on contested).
- **Log line:** `deep-pillage` (standing/started/nearby).
- **Interacts with:** raid_radius (counts the nearby population but does not move units).
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** same S-GROUND bundle A/B as base_fort_gain (pillages started 7.8 / 9.9 per game). Isolation arm "base want + pillage, no hold" = -0.17 +-0.89 (wash). The threshold literal 1.0 is why gain and threshold are inseparable (STATUS.md:10142).

### survival_target
- **Declared:** deeptuning.h:1406 (`double survival_target;`); params.ini: default=0.5 min=0.0 max=0.95 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "a city's garrison must survive the observed attacker wave with probability survival_target, computed by chained duels (deepodds.c) over at-war units reachable within survival_horizon turns... target <= 0 disables every survival consumer."
- **Unit:** probability 0-1 (compared with `surv`, a probability from the sequential-attrition model).
- **Formula at the primary call site:** `fear = (unsigned int) (tune->survival_fear_value * (tune->survival_target - surv) / tune->survival_target * updisc * away_mult);` -- deepdefence.c:2046-2050, `deep_survival_deficit_fear()`; the same threshold test everywhere: `surv < tune->survival_target` with `wave > 0`.
- **All call sites:** (a) deepdefence.c:2034/2042/2048 - deep_survival_deficit_fear - the deficit becomes injected danger (UNSCALED by stance) - yes; (b) :2099/2105 - deep_fear_satisfaction - an under-target city is un-satisfied (fears bid in full) - yes; (c) :2478/2489/2511 - deep_defensive_techwall - no (weight 0); (d) :2716/2728/2734 - deep_melt_surplus_defenders - melt a defender only if the city still clears the target without it - yes; (e) :3022 - deep_kill_sorties, :3063/3121 deep_cover_sorties, :3376 deep_city_sparable_blade - a lender city must clear the target after losing the blade - yes; (f) :3907 - deep_evacuate_civilians; :3961/3976/4026/4061/4067/4088 - deep_defend_counter_claims (reinforce until `relieved >= target`) - yes; (g) :7172 deep_sell_to_defend and :7297 deep_late_defence_buy - no (sell_defend_gain=0, late_buy_gate=0); (h) deepcm.c:252/257 - deep_cm_survival_ok - grow-hard veto when under target - yes; (i) deephorizon.c:2109/2118/2123 - deep_adjust_building_wants - "threatened" flag for the building ladder (replaces stock urgency/grave_danger) - yes; (j) deepops.c:4053/4192 - deep_op_form (draft/home-survival check), :4911/4914 deep_op_drive_consolidate, :5342/5358 deep_muster_abandon (op_muster_home=1) - conditional on an op/muster existing.
- **Log line:** `deep-surv` (`p=`, `wave=`, `fear=`), `deep-meltblock`, `deep-sat`, `deep-wave surv=`.
- **Interacts with:** survival_horizon, survival_fear_value, survival_pf_reach, threat_pin_discount, satisfied_fear_discount, expansion_war_survival, cm_war_survival (a separate threshold on purpose, STATUS.md:1895), op_muster_home, worker_guard_radius.
- **Tier/screen:** tier B (BFEAR); fires 1086/game.
- **Prior evidence:** STATUS.md:13540 "at survival_target = 0.85 a frontier staging city on a war venue essentially never spares its best blade" (a 0.85 setting seen in a tuned ini; A/B not isolated); STATUS.md:14472 lists it with survival_pf_reach for a joint design; STATUS.md:5162 notes it prices "the same probability differently on purpose" (cm_war_survival). No isolated A/B of the value: none.

### survival_fear_value
- **Declared:** deeptuning.h:1407 (`double survival_fear_value;`); params.ini: default=20.0 min=0.0 max=200.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "When the odds fall short, survival_fear_value scales the deficit into the danger field."
- **Unit:** want/score units (danger units): value x normalised deficit in [0,1] x updisc x away_mult -> `unsigned int` danger.
- **Formula at the primary call site:** `fear = (unsigned int) (tune->survival_fear_value * (tune->survival_target - surv) / tune->survival_target * updisc * away_mult);` -- deepdefence.c:2046-2050, `deep_survival_deficit_fear()`.
- **All call sites:** deepdefence.c:2034 - deep_survival_deficit_fear - `<= 0` disables - yes; :2047 - same - deficit-to-danger conversion (added to danger in both seats, deepdefence.c:2394 and :5533, NOT scaled by stance) - yes; :2512 - deep_defensive_techwall - `credit = value * deficit * defensive_techwall_weight` - no (weight 0).
- **Log line:** `deep-surv fear=`, `deep-fear surv=`.
- **Interacts with:** survival_target, survival_horizon, committed_fear_mult (`away_mult`), defensive_techwall_weight, deterrence_gain / defence_worth_mult ("three knobs funding the same body from three sides", STATUS.md:10800).
- **Tier/screen:** tier B (BFEAR); fires 607/game.
- **Prior evidence:** none as an isolated arm; STATUS.md:10800 (M4b): "MIL tuning is DEFERRED... must sweep deterrence_gain against defence_worth_mult and survival_fear_value". Because an EMPTY city's defender want is a flat 100 whenever danger >= 1, this value can only change the want in cities that already hold a defender.

### survival_horizon
- **Declared:** deeptuning.h:1408 (`int survival_horizon;`); params.ini: default=3 min=1 max=8 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "over at-war units reachable within survival_horizon turns" -- the arrival window for the attacker wave; reused as the lookahead of the employ/poach claims and the chat threat window (derived, not a separate knob).
- **Unit:** turns (compared to an eta in turns: `turns = (dist*SINGLE_MOVE + rate - 1)/rate; if (turns > horizon) continue`). Exception: `deep_relay_chain` (deepdefence.c:3394, 3435) documents it as "tiles" in `d > tune->survival_horizon`. Mixed; see Unresolved.
- **Formula at the primary call site:** `if (turns > horizon) { continue; }` (wave admission) -- deepcombat.c:1123-1125, `deep_city_survival_core()`; the horizon is passed down from every `deep_city_*survival*(.., tune->survival_horizon, ..)` caller.
- **All call sites:** ~47 reads in ai/deep, the same functions as survival_target plus: deepdefence.c deep_survival_deficit_fear 2037, deep_fear_satisfaction 2102, deep_melt_surplus_defenders 2718, deep_kill_sorties 3020, deep_cover_sorties 3119, deep_city_sparable_blade 3374, deep_relay_chain 3435, deep_cover_removal_cost 3555-3596, deep_evacuate_civilians 3906, deep_defend_counter_claims 3974-4095, deep_expansion_blocked 4941, deep_birth_guard_want 5044, deep_marginal_attrition 5196-5208, deep_sell_to_defend 7170, deep_late_defence_buy 7296, deep_manage_cities 9114 (emergency ranking), deep_committed_away 331; deepops.c deep_op_form 4162-4206, deep_op_poach_defender 4774/4819, deep_op_drive_consolidate 4913, deep_muster_abandon 5356; deephorizon.c 2115-2133; deepcm.c 256/679; deeprequest.c:228 and deepchat.c:467/475 (chat/request seams, dead: chatty_gate=0); deepemploy.c:423 (poach-claim due turn) - yes except as noted.
- **Log line:** `deep-wave horizon=`, `deep-surv`.
- **Interacts with:** survival_target, survival_pf_reach, survival_fear_value, expansion_war_survival.
- **Tier/screen:** tier B (BFEAR); fires 1086/game.
- **Prior evidence:** STATUS.md:21071 "window is survival_horizon = 3 and it is derived, not chosen" (employ claim); STATUS.md:12534 "pegged at 18 = survival_horizon 3 x a destroyer's 6" (a clamp observation). No arm moved it: none.

### survival_pf_reach
- **Declared:** deeptuning.h:1432 (`int survival_pf_reach;`); params.ini: default=0 min=0 max=1 kind=int
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "HONEST LAND REACH (ERA 3c)... 1 requires the pathfinder to confirm every land-arm admission... A mechanism switch, not a magnitude... 0 = the straight-line test alone, exactly as before."
- **Unit:** boolean gate.
- **Formula at the primary call site:** `if (tune->survival_pf_reach > 0 && !flies_in && !unit_transported(aunit) && tile_continent(unit_tile(aunit)) == home) { int pf_turns = deep_reach_pf_turns(nmap, aunit, ctile); if (pf_turns < 0 || pf_turns > horizon) continue; }` -- deepcombat.c:1135-1142, `deep_city_survival_core()`.
- **All call sites:** deepcombat.c:1135 - deep_city_survival_core - drops land attackers the pathfinder cannot confirm inside the horizon from the survival wave (affects every survival consumer) - no: dead at default 0.
- **Log line:** `deep-wave pf_turns=` (measured whether or not on).
- **Interacts with:** survival_horizon, threat_pin_discount (explicitly non-overlapping populations), survival_target.
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** STATUS.md:12441 ERA-3c 2x2 over op_draft_marginal x survival_pf_reach, tech50war-200, 16 seeds/cell (era-a-pv1 base): reach main effect +71.9 +-28 score/seed (t +2.58, 13/16 seeds) -- "first statistically solid mechanism win against the collapse" on the war venue. STATUS.md:13441: "+71.9/seed on pv1 does not transfer to the tuned ECON baseline (G4/G5: +2.8/seed, t +0.04)". Positive on the war venue, null on tuned econ. Compiled default stays 0.

### threat_pin_discount
- **Declared:** deeptuning.h:1461 (`double threat_pin_discount;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "an enemy unit the pin tracker currently classifies as home-pinned (live tag, never yet seen off its home tile) contributes (1 - threat_pin_discount) of its normal weight to the wave, as a presence probability in the duel substrate". Measured first cut 0.687.
- **Unit:** probability 0-1 (the complement is a presence probability: weight = 1 - discount).
- **Formula at the primary call site:** `if (t != NULL && t->live && !t->left) { w = 1.0 - pin_disc; pin_hit = TRUE; ...}` then `deep_combatant_add(att, &n_att, DEEP_WAVE_MAX, aunit, rating, w);` -- deepcombat.c:1233-1244, `deep_city_survival_core()`; `pin_disc = (pplayer->ai == deep_ai_get_self() && tune->threat_pin_discount > 0.0) ? MIN(tune->threat_pin_discount, 1.0) : 0.0;` at :1056-1057.
- **All call sites:** deepcombat.c:869 - deep_pin_track_active - arms the pin tracker when > 0; :1054/1056 - deep_city_survival_core - arms the discount; :1233-1244 - applies the weight - all no: dead at default 0 (w stays 1.0). (The tracker itself also runs whenever the metric log level is on.)
- **Log line:** `deep-pindisc`, `deep-pin`.
- **Interacts with:** survival_pf_reach (non-overlapping), survival_target, survival_horizon.
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** STATUS.md:14615-14680 A/B at 0.687 vs 0 (pv3 base), tech50war-200 mixed, 16 mirrored seeds: score -1.05 +-3.40 (t -0.31, 8/16), cities -0.05, unitslost +2.16 +-1.64 (t +1.32), pindisc share of wave mass 0.357 -- WASH. Byte-identity at 0 proven 20/20.

### attacker_garrison_gain
- **Declared:** deeptuning.h:1616 (`double attacker_garrison_gain;`); params.ini: default=0.0 min=0.0 max=4.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** price a home-based attacker's defensive worth "as exactly the dividend a defender of the same type would earn in this city... WEIGHTED by the fraction of unit-turns this player's attack-rated units have actually spent standing in own cities -- an OBSERVED cadence". 0 = off.
- **Unit:** multiplier (dimensionless) on `marginal (shield-denominated attrition) * menace (0-1) * home_share (0-1)`; result is added to the assault build want (want units).
- **Formula at the primary call site:** `credit = (adv_want) (marginal * menace * share * tune->attacker_garrison_gain);` -- deepdefence.c:5426-5427, `deep_attacker_garrison_credit()`; added at deepdefence.c:7011 `want += deep_attacker_garrison_credit(...)` in `deep_assault_want()`.
- **All call sites:** deepdefence.c:5414 - deep_attacker_garrison_credit - `<= 0` returns 0 - no: dead at default 0.
- **Log line:** `deep-agc`, `deep-agc-sum` (zero lines at 0).
- **Interacts with:** garrison_menace_slope (via `menace`), deterrence_gain, attacker_hold_damper (deliberately not applied to it), stance_assault_gain.
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** STATUS.md:11929 (NETTHREAT item 3): measured cadence mean share 0.891 (0.821 at T100) over 3 409 player-turns, 10 smoke seeds; no score A/B listed for it. none.

### satisfied_fear_discount
- **Declared:** deeptuning.h:1839 (`double satisfied_fear_discount;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** once the garrison is sized and "survival against the observed wave clears the target, the injected fear terms scale by this factor so the domestic ladder can win the build auction. 0 = satisfied cities are quiet (default...); >= 1 disables the gate."
- **Unit:** multiplier (dimensionless 0-1) on all injected anticipatory fears (`sat`).
- **Formula at the primary call site:** `return MAX(tune->satisfied_fear_discount, 0.0);` (the `sat` multiplier; the function returns 1.0 when the garrison is below the floor or an observed wave beats the garrison) -- deepdefence.c:2109, `deep_fear_satisfaction()`.
- **All call sites:** deepdefence.c:2082 and :2109 - deep_fear_satisfaction - sat factor (consumed at the ledger deepdefence.c:2347 and the build seat :5488 as `over/shadow/density/prior/barb * updisc * sat * fmult`) - yes (default 0 means a satisfied city's injected fears are ZERO); :2725 - deep_melt_surplus_defenders (comment only).
- **Log line:** `deep-sat` (printed when `sat < 1.0`), `deep-fear sat=`.
- **Interacts with:** garrison_floor, survival_target, survival_horizon, stance_fear_gain.
- **Tier/screen:** tier B (BFEAR); fires 2427/game (partial instrument).
- **Prior evidence:** introduced as task #19 from T153 forensics (STATUS.md:1147); no isolated A/B. none.

### barb_reinforce_rate
- **Declared:** deeptuning.h:1948 (`double barb_reinforce_rate;`); params.ini: default=0.0 min=0.0 max=1.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Barbarian targets reinforce at this rate instead of invasion_reinforce_rate (T160: barbarians field armies and lean against garrisoning their takes -- 47/52 cities empty). 0 prices a barb city exactly as it stands today."
- **Unit:** count per turn (defenders added per turn of eta; the odds code computes `reinforce = (int) (eta * rate + 0.5)` virtual defenders).
- **Formula at the primary call site:** `if (is_barbarian(city_owner(acity))) { return tune->barb_reinforce_rate; }` -- deepcombat.c:1514-1516, `deep_reinforce_rate()`; consumed as `reinforce = (int) (eta * deep_reinforce_rate(nmap, acity) + 0.5);` -- deepcombat.c:1600, `deep_take_odds_cost()`.
- **All call sites:** deepcombat.c:1515 - deep_reinforce_rate - reinforcement rate for barbarian-held assault targets - read live whenever a barbarian-held city is priced; the value 0 means no reinforcement credit (a no-op value, not a dead branch).
- **Log line:** `deep-kill rrate=` (and `barb=1`).
- **Interacts with:** invasion_reinforce_rate, invasion_reinforce_slope (civilised targets only), op_take_target.
- **Tier/screen:** unranked; knob-coverage: LIVE proxy, 1/11 corpora (contested, 209 lines); not screened.
- **Prior evidence:** none (design from T160 forensics, STATUS.md:1263).

### stance_forced
- **Declared:** deeptuning.h:1966 (`int stance_forced;`); params.ini: default=-1 min=-1 max=-1 kind=int (degenerate range on purpose: the tuner cannot move it)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Which corner every deep seat is clamped to, or -1 for FREE. 0 = balanced (identity), 1 = economic, 2 = military... FREE is what ships... PINNED is what makes a tuning block separable... NOT a magnitude".
- **Unit:** enum/sentinel (-1 FREE; 0 balanced; 1 economic; 2 military, `enum deep_stance_preset`).
- **Formula at the primary call site:** economic: `plr_data->stance.econ = 1.0 + span; plr_data->stance.mil = 1.0 - span;` military: the mirror -- deepstance.c:551-566, `deep_stance_refresh()`; `deep_stance_pinned()` is `stance_forced >= 0` (deepstance.c:98).
- **All call sites:** deepstance.c:98 - deep_stance_pinned - mode test (also gates the source ladder) - yes (returns FALSE); :551 - deep_stance_refresh - corner selection - dead at -1 (switch falls to the not-pinned path); :624 - deep_stance_log - log.
- **Log line:** `deep-stance` (`mode=PINNED|FREE preset=`).
- **Interacts with:** stance_trait_span (corner depth), every stance_*_gain, stance_source (a pin overrides the source).
- **Tier/screen:** mode switch (excluded from tuner by degenerate range; knob-coverage "MODE"); not screened.
- **Prior evidence:** the pinned-economic corner is the E34.1/E34.2/E34.2b measurement regime. STATUS.md:4640 gate 1 alone pinned-econ lategame-80, 10 paired seeds: score -5.60 +-16.94 (t -0.33); STATUS.md:5370 civilian-share split measured, "A.6 regime claim as written is false (measured twice, at four gates and at five)"; STATUS.md:5382 gates 3+4 re-measured pinned vs FREE, 20 paired seeds: score -0.38 (t -0.07). Net: null on score, structure-only. n=10-20.

### stance_source
- **Declared:** deeptuning.h:1973 (`int stance_source;`); params.ini: default=0 min=0 max=0 kind=int (degenerate range)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Where a FREE player's axes come from: 0 identity, 1 traits, 2 traits+position (W34 A.4's ladder)."
- **Unit:** enum/sentinel (0 identity, 1 traits, 2 traits+position; anything else falls back to identity with a log_error).
- **Formula at the primary call site:** `int want = deep_tuning_get()->stance_source; if (want >= 0 && want <= 2) return want;` -- deepstance.c:146-149, `deep_stance_source_effective()`; dispatched in `deep_stance_refresh()` (`case 1: deep_stance_seed_from_traits ... case 2: deep_stance_seed_from_position`).
- **All call sites:** deepstance.c:146 - deep_stance_source_effective - selects axis seed - yes as a read; value 0 -> identity axes (so the whole stance layer is inert).
- **Log line:** `deep-stance src=`.
- **Interacts with:** stance_forced (overrides), stance_trait_span, stance_position_gain, stance_techlean_gain, stance_pressure_ref; and indirectly every gate gain.
- **Tier/screen:** mode switch (degenerate range); knob-coverage "MODE".
- **Prior evidence:** E34.3 (STATUS.md:5455-5560): `stance_source = 1` vs 0, 20 paired seeds, isolated-150-traits (traitdistribution EVEN): score +5.74 +-4.67 (t +1.23, 13/20), offence share -0.040 (t -4.95), improvement share +0.037 (t +2.67); guard cell -0.45 +-1.33; with traits FIXED (all shipping cells) byte-identical 20/20 -- "Nothing here is significant at n=20". E34.4 source 2 smoke only (STATUS.md:10860): differentiates 4 stances/game on FIXED traits; no score A/B listed. Compiled default stays 0.

### stance_trait_span
- **Declared:** deeptuning.h:1981 (`double stance_trait_span;`); params.ini: default=0.5 min=0.5 max=0.5 kind=float (degenerate range)
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "How far off identity an axis may be moved -- by a pinned corner today, by a trait in E34.3. Shared on purpose: a pinned corner is then exactly the extreme a trait-driven player can reach... the one number that decides how DIFFERENT players are allowed to be."
- **Unit:** multiplier (dimensionless half-width of the axis range: axes = 1 +- span), clamped `MIN(MAX(span, 0), 0.9)`.
- **Formula at the primary call site:** `span = MIN(MAX(tune->stance_trait_span, 0.0), 0.9);` -- deepstance.c:531, `deep_stance_refresh()`; economic corner `econ = 1.0 + span`, `mil = 1.0 - span`; trait seed `1.0 + span * (trait index)`.
- **All call sites:** deepstance.c:531 - deep_stance_refresh - sets span for every non-identity path - read live but affects nothing at defaults (identity returns before it is used).
- **Log line:** `deep-stance econ= mil=` (the realised axes).
- **Interacts with:** stance_forced, stance_source, all stance gains (through the ratio mil/econ).
- **Tier/screen:** mode/degenerate range; knob-coverage "MODE".
- **Prior evidence:** STATUS.md:5683: "over 80 seats the realised axes span econ 0.656..1.356, mil 0.500..1.483, ratio 0.47..2.03 -- traits reach about a quarter of the pin"; not swept (rule 7). none as an A/B.

### stance_fear_gain
- **Declared:** deeptuning.h:1989 (`double stance_fear_gain;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Gate 1: sensitivity of the ANTICIPATORY fear terms to the mil:econ ratio, as (mil/econ)^gain. Identity axes give identity behaviour at any gain; 0.0 disables the gate outright. Does not touch the survival deficit or the garrison floor."
- **Unit:** multiplier (dimensionless exponent): `pow(mil/econ, gain)` multiplies the injected fears.
- **Formula at the primary call site:** `return pow(st->mil / st->econ, gain);` -- deepstance.c:683 in `deep_stance_mil_mult()` called by `deep_stance_fear_mult()` (deepstance.c:703-706); applied as `over/shadow/density/prior/barb * updisc * sat * fmult` (deepdefence.c:2347-2388 ledger, 5492-5531 build) and `p * worth * gain * deep_stance_fear_mult` (birth guard, deepdefence.c:5052).
- **All call sites:** deepstance.c:706 - deep_stance_fear_mult - gate 1 multiplier; consumed at deepdefence.c:2342, :5492, :5052 - dead at defaults: returns exactly 1.0 when `deep_stance_is_identity` (econ == mil) whatever the gain.
- **Log line:** `deep-stance fear_gate=` (cumulative applications; 0 on every line is the identity proof), `deep-fear fmult=`.
- **Interacts with:** stance_forced/source/trait_span (set the ratio), density_fear_value, density_prior_value, barbarian_fear, birth_guard_gain, boat_shadow_fear.
- **Tier/screen:** knob-coverage "LIVE" only in the pinned isolated-150 corpus (fear_gate 3858/game under pinned-econ); at compiled defaults dead.
- **Prior evidence:** STATUS.md:4640 gate 1 alone, pinned-economic vs FREE, lategame-80, 10 paired seeds: score -5.60 +-16.94 (t -0.33), `munits` +0.70 -- "moves nothing measurable"; STATUS.md:5382 (gates 3+4 re-measured with fear/op/assault gain set to 0). null.

### stance_op_gain
- **Declared:** deeptuning.h:1998 (`double stance_op_gain;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Gate 2: sensitivity of ASSAULT FORMATION to the econ:mil ratio... scales that [op_take_target] bar by (econ/mil)^gain, capped at 1.0... Targets that THREATEN us keep the unscaled bar."
- **Unit:** multiplier (dimensionless exponent) on the op formation odds bar (a probability, capped at 1.0).
- **Formula at the primary call site:** `bar = MIN(1.0, bar * omult);` with `omult = deep_stance_op_mult(pplayer)` = `pow(econ/mil, gain)` -- deepops.c:3659-3664, `deep_op_assault_bar()`; the multiplier at deepstance.c:851.
- **All call sites:** deepstance.c:851 - deep_stance_op_mult - gate 2 multiplier; deepops.c:3659 - deep_op_assault_bar (called from deep_op_form at 3978 and from the bar census) - dead at defaults (identity -> 1.0).
- **Log line:** `deep-stance op_gate=`.
- **Interacts with:** op_take_target (the base bar), stance_forced/source/trait_span.
- **Tier/screen:** knob-coverage "LIVE" only pinned (op_gate 3952/game); dead at defaults.
- **Prior evidence:** E34.2/E34.2b pinned arms (STATUS.md:4731ff, 5254ff): effect not separable from the other gates; set to 0 in the gate 3+4 re-measure. none isolated.

### stance_growth_gain
- **Declared:** deeptuning.h:2005 (`double stance_growth_gain;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Gate 3: sensitivity of the horizon growth credits (the growth ladder that bids against barracks and walls) to the econ:mil ratio, as (econ/mil)^gain. The credits are additive on top of stock's base want and never go below zero."
- **Unit:** multiplier (dimensionless exponent) on the horizon growth credit (want units).
- **Formula at the primary call site:** `adv_want scaled = add * gmult;` with `gmult = deep_stance_growth_mult(pplayer)` = `pow(econ/mil, gain)` -- deephorizon.c:2042 and :2352-2353, `deep_adjust_building_wants()`; multiplier at deepstance.c:865.
- **All call sites:** deepstance.c:865 - deep_stance_growth_mult; deephorizon.c:2042/2352 - deep_adjust_building_wants - scales deep's growth credit on building wants - dead at defaults (1.0).
- **Log line:** `deep-stance growth_gate=` (21 956/game pinned-econ); `deep-decay-sum gmult=`.
- **Interacts with:** stance_expand_gain ("should be tuned together", STATUS.md:5401), horizon_growth_weight, stance_forced/source.
- **Tier/screen:** knob-coverage "LIVE" only pinned; dead at defaults.
- **Prior evidence:** STATUS.md:5382-5401 gates 3+4 (growth+expand) re-measured pinned-econ vs FREE, 20 paired seeds: score -0.38 (t -0.07), founder want/city-turn +650 (t +12.4), expansion city-turns -27.8 (t -0.46). Not isolated; null on score.

### stance_expand_gain
- **Declared:** deeptuning.h:2012 (`double stance_expand_gain;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Gate 4: sensitivity of EXPANSION -- the settler want and the colony lift it needs to cross water -- to the econ:mil ratio, as (econ/mil)^gain. Whether land remains is already asked by the site machinery."
- **Unit:** multiplier (dimensionless exponent) on `founder_want` and on the colony-lift demand.
- **Formula at the primary call site:** `city_data->founder_want = (int) (city_data->founder_want * emult);` with `emult = deep_stance_expand_mult(pplayer)` -- deepdefence.c:9279-9284, `deep_manage_cities()`; also `lift_demand = (int) (lift_demand * emult)` (deepdefence.c:6257, `deep_military_choose_build`) and `cw = cw * emult` (:6415).
- **All call sites:** deepstance.c:879 - deep_stance_expand_mult; deepdefence.c:6257, :6415, :9279 - scales colony lift, hull commission, founder want - dead at defaults (1.0).
- **Log line:** `deep-stance expand_gate=`.
- **Interacts with:** stance_growth_gain (pair), settler_rush_weight (applied first at deepdefence.c:9262), colony_lift_weight.
- **Tier/screen:** knob-coverage "LIVE" only pinned (23 440/game); dead at defaults.
- **Prior evidence:** as stance_growth_gain (the pair): pinned-econ founder want triples; score null.

### stance_assault_gain
- **Declared:** deeptuning.h:2023 (`double stance_assault_gain;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Gate 5: sensitivity of the OFFENSIVE BUILD -- deep_assault_want's marginal-take-odds attacker demand -- to the mil:econ ratio, as (mil/econ)^gain... Targets that THREATEN us keep the unscaled want."
- **Unit:** multiplier (dimensionless exponent) on the assault build want (want units).
- **Formula at the primary call site:** `want = (adv_want) (want * amult);` with `amult = deep_stance_assault_mult(pplayer)` = `pow(mil/econ, gain)`, skipped when `deep_op_target_threatens` -- deepdefence.c:6961-6966, `deep_assault_want()`.
- **All call sites:** deepstance.c:732 - deep_stance_assault_mult; deepdefence.c:6961 - deep_assault_want - dead at defaults (1.0).
- **Log line:** `deep-stance assault_gate=`.
- **Interacts with:** attacker_hold_damper (applied after), attacker_garrison_gain, op_take_target.
- **Tier/screen:** knob-coverage "DEAD" in the corpus it examined (0/6 runs) but live pinned (STATUS: 964.8/game under pinned-econ isolated-150); dead at compiled defaults.
- **Prior evidence:** STATUS.md:5406 "Its measured effect at 1.0 under a pinned corner is -0.007 offence share (t -1.87) for +1.34 score (t +0.97)" -- cheap, intended direction; null on score. n=20.

### stance_floor_gain
- **Declared:** deeptuning.h:2028 (`double stance_floor_gain;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Gate 6: exponent on (mil/econ) scaling the class-gated garrison floor... a pinned or trait-seeded stance turns it into an early-war / early-peace personality."
- **Unit:** multiplier (dimensionless exponent) on the floor injection.
- **Formula at the primary call site:** `fmult = deep_stance_floor_mult(city_owner(pcity));` ... `applied = (unsigned int)(tune->garrison_floor_danger * upkeep_discount * menace * w.best * fmult);` -- deepdefence.c:1991 and :1995-1997, `deep_apply_garrison_floor()`; multiplier at deepstance.c:764.
- **All call sites:** deepstance.c:764 - deep_stance_floor_mult; deepdefence.c:1991 - deep_apply_garrison_floor - dead at defaults (1.0).
- **Log line:** `deep-floor fmult=`; `deep-stance floor_gate=`.
- **Interacts with:** garrison_floor_danger and the floor class weights, stance_forced/source.
- **Tier/screen:** unranked; not in knob-coverage S2 list; not screened.
- **Prior evidence:** STATUS.md:7365 registered (gate 6, "identity at equal axes"); no stance_floor_gain arm. none.

### stance_pox_gain
- **Declared:** deeptuning.h:2038 (`double stance_pox_gain;`); params.ini: default=1.0 min=0.0 max=3.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "Gate 7 (S-POX): exponent on the SHAPE axis scaling the own-empire crowding price, as expand ^ -gain... 0.0 disables the coupling while leaving the price flat."
- **Unit:** multiplier (dimensionless exponent) on a PRICE (the own-overlap charge), `pow(expand, -gain)`.
- **Formula at the primary call site:** `double charge = nshare * deep_tuning_get()->site_own_overlap * mult;` with `mult = deep_stance_pox_mult(pplayer)` -- deepsite.c:1308-1313, `deep_cityresult_fill()`; multiplier at deepstance.c:796-805.
- **All call sites:** deepstance.c:796 - deep_stance_pox_mult; deepsite.c:1308 - deep_cityresult_fill - dead at defaults twice over: axes identity (`expand == 1`) AND `site_own_overlap = 0.0` (price flat zero).
- **Log line:** `deep-stance pox_gate=`; `deep-sitespace`.
- **Interacts with:** site_own_overlap, stance_trait_span (expand axis `1 + span * shape_index`), TRAIT_EXPANSIONIST/TRAIT_BUILDER.
- **Tier/screen:** unranked; not screened (counter exists: pox_gate).
- **Prior evidence:** STATUS.md:20456-20560 (S-POX): 1.0 vs "soft" 0.317 vs flat vs off arms on one venue; the acceptance criterion (opposite seats -> different spacing) NOT met; at 1.0 the builder end hits the MAX(0, ...) clamp (7.3% of candidates non-positive vs 0.0% off); 0.317 derived from one venue. Stays at the 1.0 placeholder (0.317 was derived from one venue and not adopted); acceptance criterion unmet.

### stance_position_gain
- **Declared:** deeptuning.h:2049 (`double stance_position_gain;`); params.ini: default=0.5 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "SOURCE 2: how far the POSITION ... may move an axis off the trait prior, in index units... weights (free land, war, border pressure)". Zero on both position and tech-lean gains "is exactly source 1".
- **Unit:** multiplier (dimensionless weight in trait-index units, index range [-1,+1]).
- **Formula at the primary call site:** `e_idx = e_trait + pgain * e_pos + tgain * (-rd->lean); m_idx = m_trait + pgain * m_pos + tgain * rd->lean;` then `econ = 1.0 + span * MIN(MAX(e_idx, -1.0), 1.0)` -- deepstance.c:462 (read) and :479-483 (use), `deep_stance_seed_from_position()`.
- **All call sites:** deepstance.c:462 - deep_stance_seed_from_position - dead at defaults: only runs for `stance_source == 2` while not pinned.
- **Log line:** `deep-stance land= war= threat= press= pos_moved=` (zeros at sources 0/1).
- **Interacts with:** stance_techlean_gain, stance_pressure_ref, stance_trait_span, density_fear_value (via border pressure).
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** STATUS.md:10985 registered with placeholders; smoke measurement only (10 games, 4 distinct stances per game under source 2), no score A/B. none.

### stance_techlean_gain
- **Declared:** deeptuning.h:2050 (`double stance_techlean_gain;`); params.ini: default=0.5 min=0.0 max=2.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "weights the military-vs-economic mix of the advances this player's bulbs have actually bought."
- **Unit:** multiplier (dimensionless weight in trait-index units) on `lean` in [-1,+1].
- **Formula at the primary call site:** `e_idx = e_trait + pgain * e_pos + tgain * (-rd->lean);` -- deepstance.c:479-480, `deep_stance_seed_from_position()` (same site as stance_position_gain).
- **All call sites:** deepstance.c:463 - deep_stance_seed_from_position - dead at defaults (source 2 only).
- **Log line:** `deep-stance lean= tech_mil= tech_econ=`.
- **Interacts with:** stance_position_gain, stance_trait_span.
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** STATUS.md:10986; smoke only: the tech lean "does discriminate (range 0.32 across seats) but is uniformly economic (-0.39..-0.07)". none (no A/B).

### stance_pressure_ref
- **Declared:** deeptuning.h:2058 (`double stance_pressure_ref;`); params.ini: default=4.0 min=0.5 max=40.0 kind=float
- **Intent (from the header comment, 1-3 lines, quote key phrases):** "The border pressure PER CITY WE HOLD at which source 2 reads the threat term as half its maximum: threat = p / (p + ref)... In density-lattice units (density_fear_value / (1 + d) summed over city pairs)."
- **Unit:** want/score units (density-lattice danger units per city held); it is the half-saturation of a dimensionless `threat` in [0,1).
- **Formula at the primary call site:** `rd->threat = (ref > 0.0 && rd->pressure > 0.0) ? rd->pressure / (rd->pressure + ref) : 0.0;` -- deepstance.c:426-427, `deep_stance_read_position()` (`ref = MAX(tune->stance_pressure_ref, 0.0)` at :408).
- **All call sites:** deepstance.c:408 - deep_stance_read_position - dead at defaults (source 2 only).
- **Log line:** `deep-stance threat= press=` (press is the raw unsaturated pressure).
- **Interacts with:** density_fear_value, density_fear_radius, menace_overseas_weight (all via `deep_border_pressure_from`), stance_position_gain.
- **Tier/screen:** unranked; not screened.
- **Prior evidence:** STATUS.md:10885 smoke: "stance_pressure_ref = 4.0 is too high for the observed distribution (mean raw pressure 1.93)"; flagged to the pile, not adjusted. none (no A/B).

## Unresolved

- **Screen verdicts for knobs marked "not screened".** `benchmark/knobscreen.py` was not re-run (read-only, no heavy compute); verdicts are taken only from `findings/knob-screen-2026-08-30.md`, `findings/screen-corpus-2026-08-30.md` and `findings/knob-coverage.md`. A knob absent from those tables is marked "not screened"; knobscreen.py's own full 277-knob output may classify some of them (dead-path / not-logged) which these documents do not print.
- **survival_horizon unit is mixed.** Turns at nearly every site (wave admission `turns > horizon`, claim `due = turn + horizon`), but `deep_relay_chain` (deepdefence.c:3394, 3435) treats it as tiles (`d > tune->survival_horizon`). Both readings are in the code; I did not determine whether the relay chain's distance is converted.
- **survival_target / survival_horizon: liveness of deep_op_form (draft branch), deep_op_drive_consolidate and deep_muster_abandon** at compiled defaults was not individually traced (op_muster_home=1, op_draft_marginal not read; whether an op muster exists at default `op_muster` was not checked). The other sites (deficit fear, satisfaction, melt, sorties, counter-claims, cm veto, horizon "threatened") are live by construction.
- **Consumers of deep_border_pressure_from() (density_fear_value, density_fear_radius, menace_overseas_weight on the pair path):** deepdiplomacy.c:444/793/1211/1324 and stance source 2. The screen lists the gift family as dead-path at defaults; I did not verify each diplomacy consumer's gate. The corridor consumer (deeptrunk.c:629 `deep_trunk_tile_risk`) needs the trunk family on (dead-path per the screen).
- **"Danger units" versus "want units".** The injected fears are in the stock `city_data->danger` field; I labelled the unit "want/score units (danger units)" and traced the conversion (danger^2 / our_def -> percent -> defender want; flat 100 for an empty city at urgency 0). I did not check what `assess_defense_quadratic` returns for a tile holding only non-military units, so "empty city" in the zero/non-zero-switch claim means `our_def == 0`.
- **Log line for diplomat_defence_value and wall_fear_weight:** no tag prints the chosen value / the added building_want; only the surrounding decision is logged.
- **Fear-seat shares (54.9%, 72.1%, 73-74%)** are quoted from fearseat-verdict-2026-08-30.md and STATUS (E34.2b) and were not recomputed from archived logs.
- **Prior-evidence word for knobs that only appeared inside multi-knob arms** (derby-6 "lean" file, S-FEARSEAT `all` rung, S-GROUND bundle) is the arm's result; the arm cannot attribute to the single knob.
- **Pinned-only liveness of stance gates** comes from findings/knob-coverage.md (one pinned corpus); at compiled defaults they are dead by code (identity axes return exactly 1.0 before the pow()).
