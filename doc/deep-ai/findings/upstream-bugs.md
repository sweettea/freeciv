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
