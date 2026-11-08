# Upstream bug candidates

Found in `ai/default` / `common` / `advisors` at commit `0896b34`. **Do not fix
these in place** (CLAUDE.md rule 2) — implement correct behaviour in `ai/deep`,
and optionally submit fixes upstream as separate, decoupled PRs.

Agent: append newly-found bugs here rather than fixing them.

1. `result_defense_bonus()` inverted (`daisettler.c:596`) — integer division makes
   higher terrain defence score *lower*; contradicts its own docstring. At site
   total 500: grassland 39.9, forest 31.9, hills 23.9, mountains 20.0.
2. `ally_patience` nag ladder appears **unreachable** (`daidiplomacy.c:1858`) —
   `spam` is reset to ≥3 at line 1822 on every path reaching it. Verify with a log line.
3. Comment/code mismatch at `daidiplomacy.c:1823-1826` — "Bugger allies often" sits
   on the `!pplayers_allied` branch.
4. `WIN_WAR` declared and never set (`daidata.h:33`).
5. `is_pos_dangerous` never assigned by any AI code, despite being a documented
   `pf_parameter` callback.
6. No retraction of a call to arms — 42 `dai_diplo_notify` calls, none stands down.
7. `war_countdown()` demands war from allies allied with the target;
   `is_allied_with_enemy` detects exactly that case and is used only to double the
   love penalty.
8. Settler search omits the tile-behaviour callback that the worker search and the
   overseas settler search both set.
9. Supermarket never built. The README pointer names `farmland_food()` in
   `advdomestic.c` — **neither symbol nor file exists**.
10. `player_save`/`player_load` unwired in `classicai.c:628-629`, so `struct ai_plr`
    including `tech_want[]` doesn't persist.
11. `dai_war_desire()` performs a perfect census of enemy cities, buildings, units,
    gold and tech with **zero handicap checks**, at every difficulty including
    `AWAY` where `H_MAP` and `H_FOG` are set.
12. `daitech.c` and `daidata.c` contain no handicap checks at all.
13. `H_MAP` and `H_FOG` are only ever set at `AI_LEVEL_AWAY`.
14. The AI never reads `map_get_player_tile()` / `map_get_player_city()`.
15. `assess_defense_backend()` counts only units on the city tile; the equivalent
    accounting exists for enemy cities (`ai_invasion`).
16. Shore-defence want triggers on a one-tile lake; `adv->oceans[].size` is computed
    every phase and **never read**.
17. `kill_desire()` multiplies benefit by `victim_count` with no
    `game.info.killstack` check.
18. `total_danger` aggregates linearly while `assess_defense_quadratic` aggregates
    superlinearly — the AI under-fears enemy concentration.
19. Emergencies resolved per-city in list order with no arbitration or shared budget.
20. **`is_req_active()` takes a global mutex and scans a list on every call**
    (`requirements.c:6756-6771`) to produce a `trdata` pointer that is **never
    read**. Dead overhead on the hottest function in the game; total serialisation
    under any threading.
21. `cm.c:915-916` qsort comparator context in file-static globals — blocks
    per-city parallel city management.
22. `base_want()` temporarily writes `game.info.great_wonder_owners[]` — global game
    state — to price one building in one city.
23. `cityresult_fill()` temporarily writes `pplayer->government` and
    `tile_set_owner()` to score a candidate site.
24. Workers never stack — `autoworkers.c:490` skips any tile with a worker on it,
    though `total_activity()` sums across units.
25. **Doc:** `README.AI` lists `locally_zero_minimap` (identifier removed), calls
    `military_amortize` *"a complete mystery"* (it is now documented and
    straightforward), and points the supermarket bug at two dead symbols.
26. **`long_path` in `dai_fill_unit_param()` is always FALSE** —
    `daitools.c:473` computes `map_distance(unit_tile(punit),
    unit_tile(punit))` (the unit's distance to itself, 0) instead of the
    distance to `ptile`. The whole long-path branch is dead code: the
    ZoC-nulling never fires and long-path cityfounders accidentally keep
    the short-path `no_fights` protection. W05's plan assumed this
    branch was live; upstream "fixing" the distance call would silently
    strip settler protection, so any fix must delete the branch, not
    repair the condition. ai/deep makes the safe behaviour explicit
    (deepsettler.c).
27. **`cityresult_fill()` leaks its government override** — the
    "all tiles worked" early return (`result->total = 0`) skips both
    the `pplayer->government = curr_govt` restore and the virtual-city
    teardown, leaving the player's government set to the goal
    government for the rest of the evaluation pass.
28. **The AI never cultivates or plants — cache key mismatch.**
    `initialize_infrastructure_cache()` stores the cultivate/plant
    valuations under the legacy ACTIVITY_IRRIGATE / ACTIVITY_MINE
    keys, while the consumer (`autoworkers.c` via
    `action_id_get_activity(ACTION_CULTIVATE/PLANT)`) reads
    ACTIVITY_CULTIVATE / ACTIVITY_PLANT — which are preset to -1 and
    never written, so the `>= 0` gate fails on every tile in every
    ruleset, for AI players and human auto-workers alike. Terrain
    changes measured across hundreds of benchmark games: exactly zero.
    (Transform is additionally gated behind Fusion Power in civ2civ3's
    actions.ruleset — endgame by design, not a bug.)

## dai_spend_gold's buy loop is dead code (since 2017, c84ce356f5)

dai_manage_cities (daicity.c:962-968) resets every city's
choice.want to -1 ("Initialize for next turn" -- actually a
military-advisor-already-ran sentinel added by c84ce356f5,
2017-09-13) BEFORE calling dai_spend_gold. dai_spend_gold's buy
list is a scan for the city with the highest choice.want
(daicity.c:~560); with every city at -1 the scan never beats the
zero-initialised bestchoice, pcity stays NULL, and the do-loop
breaks on its first iteration. The entire crash-buy machinery --
including the grave-danger emergency buy and the
sell-something-to-afford-a-defender path -- has been unreachable
for nine years. Only dai_upgrade_units (danger + civilian passes)
still spends AI gold. The buy loop has been actively maintained
(settler special cases etc.) throughout, so this is clearly
unintended. Symptom in games: AI treasuries idle at 4-25x reserve
while cities under siege never emergency-buy defenders.

Found 2026-07-31 via deep's inherited copy (deep_spend_gold) during
the seed-205 idle-treasury investigation: deep_manage_cities copied
the same reset-before-spend order, so deep never buys either.
Deep-side fix: run the spend pass on live choices (spend_gold's own
internal want=-1 marking already prevents double-processing);
restore the sentinel only after the spend pass. Do NOT fix in
ai/default (rule 2).

## adjust_ai_unit_choice swaps in a Barracks with no garrison check

`daimilitary.c` `adjust_ai_unit_choice()` converts any non-veteran
military unit choice into the EFT_VETERAN_BUILD building when one is
available and unbuilt -- with no check that the city has any
defenders, no urgency check, nothing. The walls path in
`military_advisor_choose_build()` explicitly refuses walls before the
first defender ("Walls will help nothing if there's nobody behind
them"); the veterancy path fires in an empty city, replacing the
"build first defender" choice with an academy for an army that does
not exist, at gold upkeep. Observed live (Europe playtest T153,
2026-07-31): early Barracks queues across brains. Deep gates the call
on the garrison floor and grave danger (deepdefence.c); fix left out
of ai/default per the freeze rule.

## caravan.c's annuity() is inverted, so a FINITE horizon prices an
## ongoing trade route NEGATIVE

`common/aicore/caravan.c:446`:

    static double annuity(double payment, int term, double rate)
    {
      return perpetuity(payment, rate) * (1.0 - 1.0 / pow(rate, term));
    }

The closed form of an annuity at discount `rate < 1` is
`payment/(1-rate) * (1 - rate^term)`. The code writes `1 - 1/rate^term`.
For `rate < 1`, `rate^term < 1`, so `1/rate^term > 1` and the bracket is
**negative** — and it grows without bound in `term`. At the values this
project runs (`rate = 0.95`, `horizon = MORT = 24`):

    arrival   annuity() as written   correct value   (per 1 trade/turn)
        0            -48.496              +14.160
        4            -29.151              +10.450
        8            -16.878               +7.429
       12             -9.193               +4.967
       16             -4.466               +2.963

`perpetuity()` (the infinite-horizon branch) is correct at +20.000, which
is why nothing upstream has ever tripped over this: **every in-tree
caller uses `horizon = FC_INFINITY`**, which takes the `perpetuity()`
branch at `caravan.c:545`. `caravan_parameter_init_default()` sets
`FC_INFINITY`, and `ai/default` never overrides it. The buggy branch is
reachable only by a caller that sets a finite horizon.

`ai/deep` is that caller. R2 (`deeptrade.c`, 2026-08-05) set
`parameter.horizon = MORT` for the caravan BUILD want, deliberately, to
avoid perpetuity's 20x and to stop the pathfinder walking the whole map
— and thereby landed on the only defective branch in the file. The
consequence is specific and severe: in `get_discounted_reward()` the
value is `trade + windfall`, so a destination where a route can
**actually be established** (`trade_benefit > 0`) is priced tens of
points BELOW one where it cannot, and `caravan.c:569`'s
`trade + windfall >= wonder` test then discards it. Deep's own
destination CHOOSER (`deep_choose_trade_targets`) keeps
`FC_INFINITY` and is unaffected.

Not fixed in place: `ai/baseline` and `ai/default` both link this file
and both are measurement controls, so a fix here would move every
benchmark number the project has ever taken. `ai/deep` computes the
annuity itself instead (`deeptrade.c`, `deep_route_annuity()`), from the
same primitives in `common/traderoutes.c`, and its census reconstructs
`caravan.c`'s own number beside it so the two can be compared rather
than assumed.

Second, smaller defect in the same neighbourhood
(`caravan.c:333`, `one_city_trade_benefit()`): the broken-route ledger
does `city_owner(losercity)` on the result of
`game_city_by_number(plost->partner)` **without a null check**, while
every other partner lookup in `traderoutes.c` guards it. A trade route
whose partner city has been destroyed but whose route record has not yet
been reaped would dereference null. Not observed; noted because deep's
re-derivation guards it.

## pf danger maps read past an empty segment when the start is dangerous

`common/aicore/path_finding.c`: `pf_danger_map_create_segment()`'s
back-walk excludes the origin tile whenever it is the map's start
(`direction8_is_valid(node->dir_to_here)` is false there), so a path
whose START tile satisfies `is_pos_dangerous` gets a zero-length
danger segment. `pf_danger_map_construct_path()` then hits the start
node with `is_dangerous` set and a non-NULL (empty) segment pointer
and fills `positions[0]` from `danger_seg->cost` -- a read past a
malloc(0) allocation. Symptom under PF_DEBUG: the
`pos->moves_left == param->moves_left_initially` assertion in
`pf_map_path()` (path_finding.c:3253), garbage `positions[0]`
otherwise. Latent upstream: nothing in-tree ever sets
`is_pos_dangerous` (the callback is documented and unassigned), so
only ai/deep's settler risk mix triggered it -- ~100 of ~1300 deep-arm
benchmark logs, zero baseline. Deep-side fix: the civilian danger
callback returns FALSE for `param->start_tile` (you cannot refuse to
be where you already are), which is semantically right regardless of
the pf bug. Upstream fix would be: treat a dangerous start like a
safe origin in segment construction (include it in the segment, or
have construct_path fall back to `node->cost` when the segment is
exhausted). Not fixed in place per rule 2's spirit for common/.

30. **`dai_sanity_check()` asserts a diplomacy invariant that the AI
    itself violates** — `ai/default/daisanity.c:51`,
    `adip->countdown >= -1 || at_war || war_target`. Fires in ordinary
    benchmark play: 6 logs per ~240-game generation, with the frozen
    control's twin `bl_dai_sanity_check()` (`ai/baseline/default/`)
    firing at the SAME count on the same generation. Module-symmetric,
    therefore not a deep regression and not a measurement hazard — it
    costs both arms equally. Recorded rather than fixed per rule 2;
    noted here so a future session does not re-diagnose it after the
    two deep-side assertions around it were cleared (2026-08-01).

31. **No AI ever performs `Join City`** — a capability gap rather than a
    defect, recorded here because the code makes it look handled.
    `ACTRES_JOIN_CITY` appears three times across the AI tree
    (`ai/default/daicity.c:1283`, `ai/default/daidiplomacy.c:2080`,
    `server/advisors/advdata.c:926`) and every one is a **classification**
    switch — what the action is worth when somebody else does it, how to
    bucket it. Nothing anywhere is an actor: no AI unit ever joins a city.

    It is legal and cheap. civ2civ3 Settlers carry the `AddToCity` flag
    (`units.ruleset`) with `pop_cost = 2`, and the `Join City` enabler
    (`actions.ruleset:757`) asks only for that flag, a domestic target and
    one move fragment. So a settler is not a sunk cost — it is two
    population that can be handed back to any own city.

    This matters most in exactly the state deep gets into on tight land.
    When site scoring returns nothing above zero — the `lategame-80`
    failure, and the failure mode `site_rival_pressure` and
    `site_overlap_weight` produce when their values are extreme — the
    settler has no site to walk to, and today it idles, wanders or dies
    (deep's `unitslost` runs high). Joining recovers the whole
    investment. It is also the natural endgame for a largepox empire:
    once the map is full, converting settlers into citizens in the big
    cities is the intended play, not a consolation.

    Deep-side implementation would sit beside the founder want: when the
    best site's worth is below the join value of the nearest own city
    that can still grow, join instead of settle. Needs `city_can_grow_to`
    (W03) so it does not push citizens into a city that will starve or
    riot, and a tunable for the exchange rate. Not fixed in `ai/default`
    per rule 2. Human observation, 2026-08-02.

32. **`goto_is_sane()` decides reachability without the caller's
    knowledge level** — `ai/default/daitools.c:429`. It builds its pf
    parameter with `pft_fill_unit_attack_param()` and never touches
    `omniscience`, which `pft_fill_unit_default_parameter()` leaves
    `FALSE`. Unknown tiles therefore count as passable, so for a player
    with no `H_MAP` handicap — that is, every difficulty anyone plays —
    the answer is *more* optimistic than the same player's own
    pathfinding elsewhere.

    Callers use it as a boat-versus-walk switch. `dai_gothere()`
    (`daiunit.c`) has the same shape as deep's copy did: "if the goto
    is not sane, call `aiferry_gobyboat()`". A target across water the
    player has not explored reports sane, the ferry branch is skipped,
    the omniscient mover finds no land path, and the unit stands still
    holding a valid mission. It is self-clearing for a unit that
    explores, and not self-clearing for a settler, which does not.

    Measured in `ai/deep` before the deep-side fix
    (`findings/settler-missions.md` §7): on `lategame-80`, 5656 of 6323
    stalled settler mission-turns over 10 games were overseas missions
    being walked to. Deep now asks the question at the player's real
    handicap level (`deep_settler_can_walk_there()`, `0f126c33b7`);
    `ai/default` keeps the behaviour per rule 2. An upstream fix would
    be one line — set `parameter.omniscience` from the caller's
    handicap, as every other AI pf site already does — but it changes
    unit behaviour, so it is offered separately and nothing here depends
    on it landing. Measured 2026-08-02.

33. **A ferry that also carries the `Workers` flag never reaches the
    ferry code** — `ai/default/daiunit.c:2742`, `dai_manage_unit()`.
    Its dispatch tests `ptype->adv.worker || unit_is_cityfounder(punit)`
    **before** it tests `is_ferry`, and the worker branch is
    `dai_manage_settler()`, which sets `SSA_AUTOWORKER` and `done` and
    returns. So such a boat is never offered to
    `dai_manage_ferryboat()`, never calls `aiferry_findcargo()`, and
    never collects a waiting passenger — for its whole life.

    civ2civ3's **Transport** is exactly that unit: `roles =
    "Ferryboat", "FerryStartUnit"`, `transport_cap = 8` — the
    colonisation boat — and `flags = "Workers", ...`. Galleons
    (capacity 4, no `Workers` flag) take the ferry branch normally; the
    bigger boat never does.

    **Ruleset-dependent.** A sweep of the shipped rulesets finds this
    unit only in civ2civ3; `classic` has no ferry carrying the
    `Workers` flag, so the bug is latent there. That does not make it a
    civ2civ3 problem — the dispatch order is wrong for any ruleset that
    ever ships such a unit, and rulesets are user-authored — but it
    does mean the `-classic` benchmark cells cannot reproduce it, and
    any measurement of it has to say which ruleset it was taken on.

    Measured in `ai/deep` on `lategame-80`, 10 games
    (`findings/settler-missions.md` §8): of the idle boat-turns where a
    settler was flagged `FERRY_WANTED` *and* the boat's own cargo-search
    map could reach it, **every one was a Transport**. Reproducing
    `aiferry_findcargo()`'s scan exactly — same parameter, same
    `no_fights`, same iteration, same match condition — found the
    waiting settler every time, which is how the branch was identified:
    the scan is fine, it is never run.

    An upstream fix is a dispatch reorder (test `is_ferry` before the
    worker branch, or exclude units whose role is `L_FERRYBOAT` from
    it). **Deep does not route around it**: doing so was measured and
    made things worse (§8), because the stock ferry manager parks an
    empty boat rather than fetching anyone. Recorded rather than fixed
    per rule 2. Measured 2026-08-02.

34. **`actres_possible()` dereferences a null `goods_type` for a
    speculative trade route** — `common/actres.c:927`:

    ```c
    struct goods_type *pgood = unit_current_goods(actor->unit, homecity);

    if (!can_establish_trade_route(homecity, target->city,
                                   pgood->replace_priority)) {
    ```

    `unit_current_goods()` (`common/traderoutes.c:779`) returns
    `punit->carrying` unexamined whenever `game.info.goods_selection`
    is not `GSM_ARRIVAL` — civ2civ3 sets `"Leaving"`
    (`data/civ2civ3/game.ruleset:634`) — and under `GSM_ARRIVAL` it
    returns `goods_from_city_to_unit()`, which is documented to return
    `nullptr` when the city can provide nothing. Neither result is
    checked before `pgood->replace_priority`.

    In a normal game `carrying` is set when the server creates the
    unit, so the null never appears. It appears the moment anyone asks
    `action_speculate_unit_on_city(ACTION_TRADE_ROUTE, ...)` about a
    **virtual** unit — which is the documented way to price a unit
    before building it, and is what `dai_wonder_city_distance()`
    already does for the wonder-caravan distance map. **Measured**:
    `ai/deep`'s trade-merit build want, isolated-150 seed 1, turn 127,
    SIGSEGV at `actres_possible+0x259`, via `caravan.c:514`.

    A one-line upstream fix is `if (pgood == nullptr) { return TRI_NO; }`
    — a unit carrying nothing cannot establish a route, which is what
    the surrounding code already means. Per rule 2 deep does not fix it
    in place: `deeptrade.c` sets the virtual caravan's `carrying` from
    the city before the speculation and refuses the valuation when the
    city can provide no goods at all. Measured 2026-08-05.

35. **Bombard is priced as a kill** — `dai_rampage_want` values every attack
    via `stack_cost` × win-chance (shield value of killing the stack), but
    bombard does not kill, and civ2civ3's `attack_blocked_by` forces
    Bombarders to bombard forever, each pass priced as a kill.
    (2026-08-05 action sweep; era-unreachable in current cells.)
36. **Base extras are structurally unvaluable** — `adv_calc_extra()` →
    `city_tile_value()` sums food/shield/trade only; a Fortress/Airbase
    changes no output, so its want is exactly 0 by construction. Empirically
    zero fortresses/airbases/buoys across a 175-city T162 game.
37. **`EFT_AIRLIFT`'s want makes Airports unbuildable** — the valuation is a
    self-described "totally braindead" constant (`daieffects.c:314-318`);
    0 Airports in 175 cities.
38. **The civ2civ3 trade-route dead branch** — `dai_choose_trade_route`'s
    only foreign-destination scan is gated on `prefer_different_cont =
    (NATIONAL_IC > NATIONAL)` (`daidomestic.c:283`), a comparison between
    two NATIONAL percentages; with both 0 (civ2civ3) the function returns
    without producing any want in 100% of games, though foreign routes pay
    100/200%.
39. **`daieffects.c:176-201`'s "already evaluated in base_want()" list is
    wrong for 8 of the 25 effects it names** (GOV_CENTER, OUTPUT_WASTE,
    OUTPUT_WASTE_BY_DISTANCE, SPECIALIST_OUTPUT, TECH_UPKEEP_FREE,
    IRRIGATION_PCT, MINING_PCT, OUTPUT_TILE_PUNISH_PCT) — absent from the
    `impr_calc` trigger switch (`advdata.c:89-138`), so e.g. the
    Ecclesiastical Palace's gov-center value is dark.
40. **`allow_specialists` is read by nothing**, and every AI pins the CM's
    gold and science factors equal (`cityturn.c:342-348`), so taxman vs
    scientist is decided by lattice order.
41. **AI build wants are docked for a production-switch penalty the AI never
    pays** — `daicity.c:1909` subtracts `shield_stock × SHIELD_WEIGHTING/15`
    while `citytools.c:3221` exempts AI players without `H_PRODCHGPEN`
    (unset at every played difficulty).
42. **War-target clobber** — `daidiplomacy.c:1839` reassigns the
    function-scope `target` inside the DS_ALLIANCE branch, so the
    `aplayer == target /* no mercy */` guard compares an unrelated player
    for the rest of the loop.
43. **A hard AI can never open with a cease-fire** — without `H_CEASEFIRE`
    the first-contact branch requires `worth > 0`, and the ceasefire
    valuation returns exactly 0 while `turns_since_contact <
    TURNS_BEFORE_TARGET` — always true at first contact.
44. **An alliance, once formed, is never dissolved for non-participation** —
    the dissolution path (`daidiplomacy.c:1890`) is inside the unreachable
    `ally_patience` ladder (bug #2's consequence; matches the measured 22.6%
    end-state alliance rate).
45. **`tech_leader`/`production_leader` can name a dead player** —
    `advdata.c:488-498` uses `players_iterate` (not `_alive`) with strict
    `<`, keeps the last max, and the results are read by nothing anyway.
46. **The mutual embassy swap is arithmetically impossible from turn 2** —
    receive is valued `tech_leak_pct/10` = 10 gold flat; give at peace is
    `-5 × turn`, unbounded (`daidiplomacy.c:566-584`) — in a ruleset where
    embassies ARE the tech leakage.
47. **civ2civ3's Trireme defeats every `UMT_SEA` transport test** — the
    class is river-native (`UMT_BOTH`→reads as land move type), so
    move-type-based hull detection misses the ancient era's primary
    transport; terrain-nativity ("can it exist on ocean") is the correct
    test (deep fixed 988f284760).
48. **`revolentype_length()` divides by zero at `revolen = 1`** —
    `server/plrhand.c:530` computes `change_speed = (player_count() / 2)
    / (game.server.revolution_length - 1)` for the QUICKENING and
    RANDQUICK revolution types, and `revolen`'s own minimum
    (`GAME_MIN_REVOLUTION_LENGTH`, `common/game.h:745`) is 1. Not an AI
    bug and not reachable in any benchmark cell (the default `revolen`
    is 5 and the default type is RANDOM), so it is a server-side
    courtesy report rather than a project dependency. Deep's
    `deep_revolen_expected()` (`deepgov.c`, the gov epic) guards it with
    `MAX(1, len - 1)` and does not reproduce it. Found 2026-08-12.
49. **`dai_select_tech()`'s immediate choice can select a tech the
    switch then silently refuses** — the newtech argmax admits any
    advance passing `research_invention_gettable(presearch, i, TRUE)`
    (`daitech.c:172`), which with `allow_holes = TRUE` tests only
    `root_reqs_known` (`common/research.c:704`), but `choose_tech()`
    returns without effect unless the tech is `TECH_PREREQS_KNOWN`
    (`server/techtools.c:1021`). When the argmax lands on a
    holes-gettable-but-not-researchable advance, `dai_manage_tech()`
    "changes research" into a no-op every turn: the penalty guard is
    evaluated, the TECH_LOG line claims a new research, and nothing
    happens. Measured on tech50war-200 (TECHMIL slice 2 liveness, gate
    mirroring stock at power 1.0): the attempted-but-refused switch
    fires on 33% of deep player-turns. Harmless per attempt (the
    refusal has no side effects), but it means the AI's realised
    research direction follows `research_goal_step()` — the goal path
    — far more often than `dai_manage_tech()`'s own logs suggest.
    Deep's `deep_reselect_research()` (deeptech.c) reproduces the
    attempt for shape fidelity at power 1.0 and reports honestly
    (`switched=` is the post-call state change, not the attempt).
    Found 2026-08-14.
50. **`city_data->diplomat_threat` is set by any hostile-capable unit,
    not by a diplomat** — the field's own comment reads "Enemy diplomat
    or spy is near the city" (`daicity.h:58`) and it is the sole
    trigger for `dai_choose_diplomat_defensive()`'s "totally arbitrary"
    16000 want (`daidiplomat.c:126`). It is set at
    `daimilitary.c:905` by `utype_acts_hostile(unit_type_get(punit))
    && 2 >= move_time`, and `utype_acts_hostile()` is
    `utype_can_do_action(putype, ACTION_HOSTILE)`
    (`common/unittype.c:470`), which `common/actres.c` marks TRUE for
    ACTRES_ATTACK, ACTRES_PILLAGE, ACTRES_BOMBARD, ACTRES_CONQUER_CITY
    and ACTRES_CAPTURE_UNITS as well as for the six spy families. An
    approaching Warriors therefore buys a Diplomat, at a want that
    outranks every military emergency. Measured on the end state of 40
    contested/lategame games (`benchmark/diploprice.py` [D6]): of the
    cities with an enemy hostile-capable unit within two tiles, only
    28.6% (contested) / 21.2% (lategame) had an enemy diplomat within
    the same radius. Deep prices the defensive diplomat off observed
    diplomat-class presence instead (`deep_diplomat_defence_price()`,
    deepdefence.c, behind `diplo_price_gate`). Found 2026-08-30. Fix
    offered upstream 2026-11-07 from branch
    `upstream/daimilitary-diplomat-threat` (92b27f2c95 on main
    761bdd5a2f): the threat is raised only when the unit's type can
    perform a hostile diplomat action against a city AND some enabler
    of it has actor and target requirements met for this unit and this
    city (range left out, the unit is two moves away), so ruleset,
    government and buildings all count (the user's refinement).
51. **`cityresult_fill()` early returns skip its own cleanup**
    (`ai/default/daisettler.c`). The function sets
    `pplayer->government = adv->goal.govt.gov` and, for a virtual city,
    `tile_set_owner(result->tile, pplayer, ...)`, and the restore lives
    only at the end. Three returns skip it: the
    `result->city_center.tdc == NULL` assert path (leaks the result and
    the virtual city, leaves the government and the tile owner changed),
    the "All is worked" real-city return (government left changed), and
    the two trailing `fc_assert_ret_val` (leak the result). The first two
    are unreachable in practice: the centre is always in the iteration,
    and `best_other` takes the first non-centre tile whatever its value.
    So no live effect; a latent bug on assert paths. Deep's copy
    (`deep_cityresult_fill()`, deepsite.c) already restored on its early
    "all worked" returns and now cleans up on the assert paths too. Fix
    offered upstream from branch `upstream/daisettler-cleanup`
    (`ai/default: Clean up on every early return from cityresult_fill()`,
    RM #TBD). Found 2026-11-02 while porting the shared reverse-map
    searches.
52. **`dai_unit_attack()` dereferences a stale bodyguard pointer after
    the attack** (`ai/default/daitools.c:1081`, identical on upstream
    main 4fd223356b). The function reads `bodyguard =
    aiguard_guard_of(ait, punit)` before `unit_do_action()`, checks only
    that the ATTACKER survived (`game_unit_by_number(sanity)`), and then
    reads `def_ai_unit_data(bodyguard, ait)->charge`. Anything the
    action does to the bodyguard in between is a use after free.
    Observed once in ~1,500 benchmark games: scoreboard europe-late,
    seed 128, mixed_db, T224 (2026-11-04 19:45, core 67008): a baseline
    (stock) Cavalry rampage captured a deep player's capital, the
    capital's loss started a civil war (`Player 'Ona' now has AI skill
    level 'Hard'` is the last log line), and the server segfaulted in
    `bl_dai_unit_attack` via `adv_unit_execute_path` <-
    `bl_dai_military_rampage` <- `dai_military_attack`. The bodyguard
    pointer was 0x413092e0 with the attacker alive; the civil war's
    transfers (`transfer_city_units`, `resolve_unit_stacks` between the
    new player and everyone it is not allied with) are what can wipe a
    unit standing beside the attacker. The game is deterministic from
    its start save, so the rerun fails at the same turn. Fix: keep the
    bodyguard's id and re-fetch it with `game_unit_by_number()` after
    the action, as the function already does for the attacker. Deep
    calls `dai_unit_attack()` itself (deepops.c) and is exposed to the
    same crash; its own operation code must re-fetch likewise or carry
    a corrected copy (rule 3, stated when done). Fix offered upstream
    from branch `upstream/daitools-attack-bodyguard` (93f40a034a,
    builds on upstream main). Reproduction note: replaying the game's
    own T223 autosave on the crashing binary does NOT reproduce it (ran
    to T228 cleanly, no civil war): the stock AI's guard assignments are
    not in the save, so the loaded game diverges. The replay from the
    T150 start save IS deterministic: the scoreboard's rerun of the same
    game (launched 2026-11-07 13:40) segfaulted again at T224 with the
    last autosave at T223, exactly as the first run. Found 2026-11-05.
