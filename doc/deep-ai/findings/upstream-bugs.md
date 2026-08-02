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
