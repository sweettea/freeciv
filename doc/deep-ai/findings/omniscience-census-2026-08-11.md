# The omniscience census — every read in `ai/deep` a blind player could not make

Epic S5. Read at `74fec40528` (branch `epic/s5-omniscience-flag`, cut from
`epic/w03-horizon-content` @ `063a86bdf7`). **Every line number below was
verified by reading the code in this tree**, not carried forward. Where a
prior document's reference no longer matches, the mismatch is recorded
rather than the code being bent to it (§8).

Supersedes `findings/deep-dive-2026-08-04/omniscience-census.md`, which was
read at `36bac91163` when `deepdefence.c` was 3.8k lines and is now 7.2k.
Its **findings** survive almost intact; its **line numbers** do not.

Companion to `findings/honest-information.md` (the argument) and
`findings/non-omniscient-roadmap.md`. This is the artefact honest-information
§8 P0.0 asks for: *"the failure list IS the requirements document for the
knowledge layer."*

---

## 0. Headline

**1. Deep makes 148 distinct UNGATED reads of information a non-omniscient
player could not have, across 13 of its 19 source files.** Enumerated in §3
and countable there: a "read" is one code site, with genuinely repetitive
families (the nineteen consumers of one survival verdict, say) listed
individually and identical idioms (the ten settler pathfinder gates)
collapsed to one row.

**2. The `honest_info` flag moves none of those 148.** `ai/deep` contains
exactly **19 information-handicap gates** — 18 `H_MAP`, 1 `H_FOG`, 0
`H_TARGETS` (§1) — and those, plus the ~42 gates in the forwarded
`ai/default` code and 9 in `server/advisors/`, are the entire surface the
flag controls. They are a disjoint set from the 148: everything marked
UNGATED in §3 reads ground truth whether the handicap bit is set or not.
**70 gated sites against 148 ungated reads is why the flag is a measuring
instrument and not a fix.**

**3. Six visibility gates in deep already fire unconditionally** —
`deepdefence.c:930`, `deepdefence.c:5059`, `deepops.c:2612`,
`deepops.c:3475`, `deeptrade.c:947`, `deepexplore.c:144`. They are the
module's only honest reads today. Every one of them gates **existence**
(`map_is_known` = ever seen) and none gates **currency** (`map_is_known_and_seen`),
so a city remembered from turn 20 is scored at its turn-200 contents.

**4. The failure direction inverts relative to stock, and this is the
prediction the flag run must test.** Stock under `H_MAP` fails by paralysis:
`find_something_to_kill()` skips unknown cities and does nothing. Deep's
ungated reads fail *dangerously optimistic*, because absence of data enters
the arithmetic as a zero and deep's arithmetic reads zero as safety:

  - an unseen attack wave reads as `wave == 0` → survival 1.0 → fear
    satisfied → **the garrison melts** (`deepdefence.c:1856`, `:2198`);
  - an unseen garrison reads as an empty city → `P(take) → 1` →
    **assaults form everywhere** (`deepcombat.c:597`, `deepops.c:3197`);
  - an unknown catchment tile scores `sum = -1` and is dropped from the
    growth simulation, so **frontier sites deflate toward centre-only**
    (`deepsite.c:589-598`).

Only the third is the classic stall. The first two are self-harm.

**5. The single largest omniscience surface is not `dai_war_desire` — it is
deep's own survival/take-odds economy.** `deep_city_survival_core()`
(`deepcombat.c:349-394`) walks every at-war player's entire unit list with no
visibility test of any kind, and has **43 external call sites**. One value,
~14 subsystems, and it gates most of them on/off rather than scaling them.
honest-information §3's "read the fog-of-war model that has been sitting
there the whole time" undercounts by exactly this much: **the server's
fog-of-war model has no units in it.**

---

## 1. What the flag does, and what it cannot do

`honest_info != 0` (`ai/deep/deeptuning.h`, applied at
`ai/deep/deepai.c:554-589` from `dpai_data_phase_begin`) ORs `H_MAP`,
`H_FOG` and `H_TARGETS` onto a deep seat's own player every phase.

**Closed by the flag — the 19 gates in `ai/deep`:**

| File | Lines | What the gate does |
|---|---|---|
| `deepsite.c` | 526 (+590) | unknown catchment tile → `sum = -1`, dropped |
| `deepsite.c` | 989 | unknown tile is not a candidate site at all |
| `deepexplore.c` | 176 | `unknown = 0` unless handicapped — the discovery term |
| `deepsettler.c` | 145 | `H_FOG`: unseen adjacent tile → assume no danger |
| `deepsettler.c` | 263, 307, 359, 485, 705, 752, 831, 1189, 1238, 1407 | `parameter.omniscience` on ten pathfinders |
| `deeptrade.c` | 258, 1157 | `parameter.omniscience` on the caravan pathfinders |
| `deepops.c` | 1404, 1715 | `parameter.omniscience` on the two lift pathfinders |
| `deepdefence.c` | 476 | free-land share counts only known tiles |

Plus, by forwarding, the ~42 information-handicap sites in `ai/default`
(33 `H_MAP`, 8 real `H_FOG`, 3 `H_TARGETS`; `daiair.c:87` and `:248` are
comments) and 9 in `server/advisors/` (`autoworkers.c:573/986/1256/1322`,
`advdata.c:426/446`, `advbuilding.c:94`, `autoexplorer.c:141`, plus
`server/unittools.c:536`).

**Not closed, and not closable by any flag:** the 129 ungated rows in §3.
Also unclosed, and explicitly out of scope per the brief and the standing
human scope guard: stock's Tier-3 strategic layer (§6).

---

## 2. Categories

Same scheme as the 2026-08-04 census, kept so the two are comparable.

- **(a)** already handicap-gated — the flag reaches it.
- **(b)** a direct remembered-state substitute exists. `map_get_player_tile()`
  gives remembered terrain, resource, owner, extras and a free
  `last_updated` stamp; `map_get_player_city()` gives remembered size,
  walls, occupancy, improvements bitvector and capital status. Mechanical
  substitutions.
- **(c)** needs real inference — no substitute exists. **Anything about
  units is (c) by construction: the server remembers cities and never
  remembers units.** So is anything about a rival's production, research,
  treasury, or private AI state.
- **(d)** legitimately knowable — own state, ruleset and lobby settings,
  adjacency, a broadcast, or a read guarded by an embassy we actually hold.

Category (d) rows are omitted from §3 except where a file is *entirely* (d).

---

## 3. The census

### 3.1 `deepcombat.c` — the survival / take-odds core (663 lines)

The most consequential file in the census, and the shortest.

| Lines | Function | Read | Gate | Cat |
|---|---|---|---|---|
| **349-394** | `deep_city_survival_core` | **every at-war player's entire unit list**: type, `adv_unit_att_rating`, `hp` (355), exact `unit_tile` (372, 378), `unit_transported`, `real_map_distance` (375, 388), `move_rate` (387) | UNGATED (at-war + `DS_NO_CONTACT` only) | **c** |
| **597** | `deep_take_odds_cost` | `deep_collect_defenders(acity, …)` — **the exact enemy garrison stack**, unit by unit, HP included (via 115-134) | UNGATED | **c** |
| **616-623** | `deep_take_odds_cost` | `can_city_build_unit_now(nmap, acity, ut, RPT_CERTAIN)` over `unit_type_iterate` on an **enemy** city — i.e. the rival's whole research state, buildings and government | UNGATED | **c** |
| 624-637 | `deep_take_odds_cost` | phantom defenders created on `city_tile(acity)` so the enemy's real walls and terrain apply | UNGATED | b |
| 170-174 | `deep_garrison_vs_stream_full` | exact current HP of enemy units on both sides | UNGATED | c |
| 179-182 | same | `get_total_defense_power` / `get_modified_firepower` — folds in enemy walls, terrain, veterancy | UNGATED | b+c |
| 96-108 | `deep_fresh_attack_power` | reads (and transiently writes) `att->moves_left` of an **enemy** unit on the defence path | UNGATED | c |
| 606, 613 | `deep_take_odds_cost` | `city_owner(acity)`, `is_barbarian(...)` | UNGATED | b |
| 338-348 | `deep_city_survival_core` | the player filter: at-war, contact made, barbarians exempt | partial | a (weak) |

**Fan-out: 43 external call sites** — `deepdefence.c` ×27, `deepops.c` ×11,
`deephorizon.c` ×3, `deepcm.c` ×2 — plus the per-city-per-phase memo at
274-306.

### 3.2 `deepdefence.c` (7204 lines)

| Lines | Function | Read | Gate | Cat |
|---|---|---|---|---|
| 347-373 | `deep_overseas_danger` | whole rival unit lists; embarked cargo, exact positions | UNGATED | c |
| 441-451 | `deep_boat_shadow_fear` | does any at-war player *own* a hull, anywhere | UNGATED | c |
| 476, 517-541 | `deep_free_land_share` | **the flag's one gate here**; but `tile_owner()` behind it is live, not remembered, and the gate admits `TILE_KNOWN_UNSEEN` | GATED (partial) | a→b |
| 769-806 | `deep_density_fear` | every rival city position; rival transport capacity (790-800) | UNGATED | c |
| 849-878 | `deep_border_pressure_from` | the same lattice; feeds `deepstance.c:391`, `deepdiplomacy.c:939/1051` | UNGATED | c |
| 930-933 | `deep_barb_unrest_site` | `map_is_known` then `unit_list_size(ptile->units)` — known ≠ seen | GATED on existence | c |
| 1222-1232 | `deep_garrison_menace` | rival city continents for the reachability flag | UNGATED | b |
| **1235, 1243** | `deep_garrison_menace` | **`aplayer->score.units`** — the rival's private score field | UNGATED | **c** |
| 1482-1489 | `deep_floor_class_facts` | rival unit lists → is the sea class live | UNGATED | c |
| 1491-1504, 1651-1661 | `deep_floor_class_facts`, `deep_floor_weigh` | rival city positions → border and air classes | UNGATED | b |
| **1511** | `deep_floor_class_facts` | `can_player_build_unit_direct(aplayer, …)` — **the rival's known tech set** | UNGATED | **c** |
| 1792, 1856, 1968, 2198, 2461, 2550, 2801, 3165, 3228, 3277, 3317, 3327, 4015, 4150, 4157, 4162, 5506, 5631, 6821 | 14 subsystems | the **wave** from §3.1 | UNGATED (inherited) | c |
| 2394-2425 | `deep_kill_sorties` | `get_defender()` adjacent (mostly (d)) + full enemy stack walk for the killstack prize (2423-2425) | UNGATED | d/c |
| 2605-2612 | `deep_hold_tired_attackers` | `get_defender()` on a **remote** enemy tile; the comment at 2608 says "Nothing (visible) to duel" and no visibility test exists | UNGATED | c |
| 2669-2707 | `deep_flee_workers` | at-war units within `worker_flee_radius` / `worker_guard_radius` | UNGATED | c |
| 3419-3420, 3453-3467 | `deep_pillage_denial`, `deep_pillage_sorties` | rival tile output; `tile_owner`/`tile_has_extra` across `raid_radius` (the comment at 3392-3395 correctly admits "Omniscient today") | UNGATED | b |
| 4610-4620 | embassy want | rival city continents for diplomat reachability | UNGATED | b |
| **4840-4844** | colony lift | `def_ai_city_data(ot, ait)->worth` on an **enemy** city | UNGATED | **c** |
| 5055-5074 | `deep_assault_want` | rival city menu, **gated at 5059** by `map_is_known` (existence only) | GATED | b |
| **5067, 5099, 5100** | `deep_assault_want` | enemy `->worth`; `deep_conquest_worth()` → enemy `history` and unworked-tile value. The comment at 5090-5092 admits it | UNGATED | **c** |
| 5176-5297 | `deep_assault_want` | `deep_take_odds_cost` chains against the target's real garrison | UNGATED (inherited) | c |

### 3.3 `deepops.c` (4535 lines)

| Lines | Function | Read | Gate | Cat |
|---|---|---|---|---|
| **127-139** | `deep_conquest_worth` | enemy `def_ai_city_data(target)->worth` + `target->history`. Comment 117-121 admits the first | UNGATED | **c** |
| 470-534 | `deep_capture_census` | full enemy order of battle by exact position — **log-only**, gated at 447 on log level | UNGATED | c (metric) |
| 1320-1471 | lift geometry | `can_unit_exist_at_tile` / `tile_continent` on arbitrary radius tiles, two lines from the H_MAP-gated pathfinder at 1404 | UNGATED | b |
| 1404, 1715 | lift pathfinders | `parameter.omniscience` | GATED | a |
| 2607-2612 | `deep_op_pick_target` | the target menu, **gated at 2612** on `map_is_known` | GATED | b |
| **2620-2621** | same | enemy `->worth` / distance — the entire target ranking, behind a mere tile-known test | UNGATED | **c** |
| **2629-2632** | same | `deep_player_is_leader` → `aplayer->score.techs` / `score.mfg` (`deepdiplomacy.c:1240/1247`), no embassy test | UNGATED | **c** |
| **2781-2804** | `deep_raid_victim` | every at-war player's units, filtered to civilians, **by exact position, through fog** | UNGATED | **c** |
| 2834, 2912-2934 | raid enrollment and drive | `deep_raid_victim` re-called per raider per turn, twice | UNGATED | c |
| 3088-3097, 3318-3331, 4033-4048, 4346-4363, 4384-4437 | op bookkeeping | `game_city_by_number(target)` + `city_owner` + `pplayers_at_war` per op per turn — free instant notice that a distant city was razed or changed hands | UNGATED | b |
| **3197-3252** | `deep_op_form` | `deep_take_odds_at_arrival` sweeps sizing `required` against the target's real garrison, twice | UNGATED | **c** |
| 3467-3487 | `deep_op_retarget` | menu **gated at 3475** on `map_is_known`; coastality ungated | GATED/b | a/b |
| **3490-3495** | `deep_op_retarget` | `deep_take_odds_at_arrival` **for every at-war known city in one pass**, then rank by enemy `->worth` | UNGATED | **c** |
| 3614-3630 | `deep_op_poach_defender` | take-odds + `deep_conquest_worth` on the target | UNGATED | c |
| 3688-3711 | `deep_op_drive_consolidate` | survival on the captured city (inherited) | UNGATED | c |
| 3852, 3901-3934 | `deep_op_drive_strike` | `deep_take_odds` at the walls; `get_defender` re-read every round. Adjacency makes most of this (d); exact HP/veterancy/invisibility does not | UNGATED | d/c |
| **4084-4098** | `deep_op_drive` MUSTER gate | `deep_take_odds_cost` on a target possibly many tiles away — **the launch decision itself** | UNGATED | **c** |
| 4127-4130 | net-value gate | enemy `->worth` + `deep_conquest_worth` | UNGATED | c |
| **4258-4262** | `deep_op_drive` MARCH re-gate | `deep_take_odds_at_arrival` **once per turn for the whole march** — continuous exact surveillance of the target garrison during the approach | UNGATED | **c** |

### 3.4 `deepsite.c` (1306 lines)

| Lines | Function | Read | Gate | Cat |
|---|---|---|---|---|
| 526, 589-598 | `deep_cityresult_fill` | **the flag's main site gate**; see §4 for exactly what an unknown tile becomes | GATED | a |
| 591 | same | `tile_worked(ptile)` — sits *beside* the handicap clause, not behind it, so a fogged rival's worked tiles leak even under the flag | UNGATED | c |
| 988-992 | `deep_city_desirability` | the second gate: an unknown tile is not a candidate at all | GATED | a |
| 137-143 | `deep_site_is_choke` | land/sea status of all 8 neighbours, possibly never seen | UNGATED | b |
| 274 | `deep_claim_build` | `tile_worked()` inside our radius — which *rival* city works it | UNGATED | c |
| 521-546 | `deep_cityresult_fill` | `tile_city` / `tile_owner` / `tile_claimer` of the candidate centre | UNGATED | b |
| 566-580, 728-756 | pressure and contest | every alive non-allied player's **whole city list** and continents | UNGATED | b |
| 607-680 | catchment loop | live `city_tile_output`, irrigation/mining increments (651-668), `tile_resource` (672), `tile_owner` (675) of tiles that are *ever-known* but possibly fogged | H_MAP-gated on existence only | b |
| 873-885, 964 | defence factor, naval bonus | fogged terrain defence bonus, extras, ocean adjacency | UNGATED | b |
| 1005-1009 | `deep_city_desirability` | `city_size_get()` of a foreign city on the target tile | UNGATED | b |
| 1084 | `deep_city_water_bridges` | `get_lake_surrounder()` — global ocean connectivity, i.e. "does this sea reach another continent" | UNGATED | c |
| 1118-1132 | `deep_settler_map_iterate` | `tile_continent(ptile)` for every pf tile including never-seen ones | UNGATED | c |
| 1134-1143 | same | `tile_owner` peace-theft filter over every candidate | UNGATED | b |

**Nothing in this file ever calls `map_get_player_tile()`.** Even with the
flag on, every yield, resource and owner it reads is the *live* value of a
possibly-fogged tile.

### 3.5 `deepsettler.c` (2223 lines)

| Lines | Function | Read | Gate | Cat |
|---|---|---|---|---|
| 144-148 | `deep_settler_overnight_veto` | **the only `H_FOG` gate in `ai/deep`**; fallback is `continue` — "Cannot see it: assume no danger" | GATED | a |
| 149-158 | same | every unit on the 8 adjacent tiles, `adv_unit_att_rating` | ungated unless H_FOG | c |
| 101-109 | `deep_settler_tile_behavior` | `tile_owner`, `is_non_allied_unit_tile`; **the pf `known` argument at 98 is discarded** | UNGATED | b/c |
| 128-142 | overnight veto | terrain and extras of the stopover tile | UNGATED | b |
| 263, 307, 359, 485, 705, 752, 831, 1189, 1238, 1407 | ten pathfinders | `parameter.omniscience = !has_handicap(…, H_MAP)` | GATED | a |
| 517 | `deep_settler_risk_route` | `omniscience = FALSE` deliberately, as a diagnostic rung | n/a | d |
| 617-625 | `deep_settler_tile_claim` | `tile_owner` (metric line) | UNGATED | b |
| **1907-1921** | `deep_fort_reference` | **every unit of every met non-allied player**, read for `attack_strength` — the strongest attacker anyone fields. The docstring at 1880-1881 already says "Omniscient (RC6)" | UNGATED | **c** |
| 2020, 2027-2044 | `deep_ground_weight` | choke ring; every met rival's city list and distances | UNGATED | b |
| **2201-2216** | `deep_worker_task_value` | **every at-war player's entire unit list** within `worker_guard_radius`, no visibility test | UNGATED | **c** |

### 3.6 `deepexplore.c` (340 lines)

| Lines | Read | Gate | Cat |
|---|---|---|---|
| 176-178 | `if (!has_handicap(pplayer, H_MAP)) { unknown = 0; }` — **the discovery term, deliberately dormant while omniscient**, documented at 163-175 to "return in full the moment W04 makes deep honest". The one mechanism in the module written *for* the honest world | GATED | a |
| 144 | `map_is_known(ptile, pplayer)` — the unknown count itself is honest, unconditionally | GATED | a |
| 128-143 | but the reachability filter around it reads `tile_continent(ptile)` for **every tile on the map**, so the count is "unknown tiles on a continent I stand on" — which requires knowing the continent of an unknown tile | UNGATED | c |
| 146-160 | `hut_on_tile()` on known-but-fogged tiles: live extras, not remembered ones | UNGATED | b |

### 3.7 `deeptrade.c` (1755 lines)

**The H_MAP flags at 258 and 1157 gate PATHFINDING ONLY.** Both reach
`pfparam.omniscience` in `common/aicore/caravan.c:227` and deep's own
`parameter.omniscience` at 883, and are spent nowhere else. The candidate
*set* (892) and the entire *valuation* are ungated. The one honest gate in
the file — `map_is_known` at **947** — guards `deep_route_offshore`, whose
own header at 918 says "A census, and nothing reads it."

| Lines | Read | Gate | Cat |
|---|---|---|---|
| 683-701 | on the **foreign** destination: `city_num_trade_routes`, `max_trade_routes`, `city_trade_removable`, then walks the evicted partners' route lists | UNGATED | c |
| 732-737 | `can_establish_trade_route` / `get_caravan_enter_city_trade_bonus` — dest slot fill and trade internals | UNGATED | c |
| 750 | `trade_base_between_cities` → `city_size_get(pc2)` or `pc2->citizen_base[O_TRADE]` | UNGATED | b/c |
| 271-286, 729, 738-744, 886-904, 1164 | foreign city identity, owner, continent, existence in the pf halo, remembered-improvement-dependent slot count | UNGATED | b |
| 1170-1176 | `deep_route_danger_fraction` → `deep_settler_overnight_veto` per path tile | UNGATED (inherited) | c |

### 3.8 `deepdiplomacy.c` (1473 lines)

| Lines | Read | Gate | Cat |
|---|---|---|---|
| 209-229 | `deep_embassy_leak_worth`: per-advance comparison of the rival's research against ours. **The embassy test at 216 is inverted for honesty purposes** — the function returns 0 when we *hold* an embassy, so the full research read runs exactly when we do not | UNGATED | **c** |
| 505-514, 557, 569, 601-608 | rival and **third-party** research state, `research_goal_bulbs_required`, and whether two *other* players hold embassies | UNGATED | c |
| 658-660, 808-835, 1081 | **the rival AI's private state**: `aplayer->ai_common.love[us]`, `req_love_for_peace`, `req_love_for_alliance`, `love_incr` | UNGATED | **c** |
| 874, 903-905, 1094 | `can_player_build_unit_now/later(aplayer, …)` — the rival's buildable set, i.e. their tech | UNGATED | c |
| 964-972 | `deep_is_distant`: every rival city's continent | UNGATED | b |
| **974-980** | same: the rival's **entire unit list**, scanned for any sea transport anywhere on the map | UNGATED | **c** |
| 1051-1052 | `deep_proxy_arm`: the diplstate *between two third parties* and their mutual border geometry | UNGATED | c |
| **1240-1251** | `deep_leaders_refresh`: `aplayer->score.techs` and `aplayer->score.mfg` for **every live player, with no embassy test** | UNGATED | **c** |
| 472, 824 | `city_list_size(aplayer->cities)` — exact rival city count | UNGATED | b |
| 939 | `deep_top_menacer` → `deep_border_pressure_from` | UNGATED (inherited) | c |

### 3.9 `deepwonder.c` (579 lines)

| Lines | Read | Gate | Cat |
|---|---|---|---|
| 198-220 | `deep_wonder_rival_eta`: every rival city (202), **what it is building right now** (205-206), and via `deep_wonder_own_eta` its `surplus[O_SHIELD]` (156) and shield stock (159) | UNGATED | **c** |
| 247, 267, 482-499, 570 | `p_win`, the race gate and the caravan urgency, all consumers of the above | UNGATED (inherited) | c |

The comment at 183-186 states the seam is deliberate — *"Under the honesty
flag this becomes a known_city read with its own staleness … That is why the
read is one function and not four inline loops"* — and 170-171 says plainly
it is "the same omniscient reads the W36 order census took". **The seam
exists; the substitution has not been made.** Note that a remembered
`vision_site` carries no production and no shield stock, so this is (c), not
(b): the seam alone will not be enough.

### 3.10 `deepspy.c` (485 lines)

The target *set* is genuinely adjacent (`square_iterate(…, 1, …)` at 415), so
existence is fair. The *prices* are not. **One read is properly
embassy-guarded: line 183.**

| Lines | Read | Gate | Cat |
|---|---|---|---|
| 183 | `player_has_embassy(pplayer, tplayer)` — do *we* hold it | guarded | d |
| 201 | `diplomats_unignored_tech_stealings(punit, target)` — the target city's private theft counter | UNGATED | c |
| 210 | `city_incite_cost(pplayer, target)` — derives from the target's size, buildings, units, **its owner's gold** and capital distance | UNGATED | c |
| 226 | `target->shield_stock` | UNGATED | c |
| 233-236 | `city_has_building(target, …)` | UNGATED | b |
| 218, 295-309 | `deep_conquest_worth` and stock's sub-target chooser | UNGATED (inherited) | c |

### 3.11 Inherited-only consumers

No fresh cross-player reads; they consume §3.1's ground truth.

| Site | Call |
|---|---|
| `deephorizon.c:1800-1802, 1814-1815, 1819-1821` | `deep_city_survival_verdict` ×2, `deep_city_defence_read` |
| `deepcm.c:255-256, 677-679` | `deep_city_survival_verdict` |
| `deepstance.c:391` | `deep_border_pressure_from` (`deepdefence.c:849`) |

`deepstance.c:382` (`deep_free_land_share`) is the one stance input that is
**already honest**, gated at source by `deepdefence.c:476`.
`deepstance.c:383-390` (rivals, at-war count) is own diplstate — (d).

### 3.12 Own-state only

`deepai.c` (the flag lives here), `deepwant.c`, `deepodds.c`, `deeprates.c`.
`deepodds.c` deserves a note: it touches no game state at all, pure
arithmetic over caller-supplied arrays. It is the *sink* of inherited
omniscience and never a source — which is why the fix for §3.1 is a change
of census, not of model.

---

## 4. The three predicted failures, checked against the code

honest-information §4's table and the 2026-08-04 census §0.3 made specific
predictions. Verified statically here; the runtime confirmation is §9.

**"Settling stops" — PARTIALLY CONFIRMED, and the real mechanism is worse
than a stop.** `deepsite.c:589-598`:

```c
    if (reserved < 0
        || (handicap && !map_is_known(ptile, pplayer))
        || tile_worked(ptile) != NULL) {
      /* Tile is reserved or we can't see it */
      ...
      ptdc->sum = -1;
```

The tile is not rejected; it is **zeroed and dropped**. `sum = -1` fails the
`> 0` tests at 634 and 650, so it never enters `sums/foods/potentials` and
never increments `ncatch`. `deep_catchment_stream()` then runs on a shorter
list, so `deep_growth_arrivals()` truncates the site's **target size as well
as its yield**, and `result->total` (786) collapses toward the centre tile
alone. After the settler-cost subtraction at 1193, `cr->result > 0` at 1197
typically fails. Meanwhile `deepsite.c:988-992` removes never-seen tiles
from candidacy outright.

So it is not a stop; it is a **monotone deflation that is strongest exactly
at the frontier**, and because `deep_contemplate_new_city` feeds the same
number to the build side, low frontier want also means fewer settlers built.
The one counterweight switches on in the same instant —
`deepexplore.c:176-178`'s discovery term — but the coupling is **one-way**:
exploration gets bought, and a partially-explored site still never scores as
if the unknown part might be good. honest-information §5.4's priors are the
missing half, and nothing in `deepsite.c` has a place to put them yet.

**"Danger amnesia" — CONFIRMED, in both directions.**
`deepsettler.c:144-148` is a verbatim port of `ai/default/daiunit.c:3250-3254`:
`continue; /* Cannot see it: assume no danger. */`. With H_FOG unset (every
level played) the loop reads *all* adjacent stacks including invisible ones;
with H_FOG set it reads nothing **and keeps no memory**, so the settler
walks back into the tile it fled last turn. Bodyguard requests (319-324) and
`deep_civilian_pos_dangerous` (191) degrade with it.

**"Targeting starvation" — FALSIFIED for deep, in the direction the
2026-08-04 census predicted.** Deep's target menus are gated on *existence*
(`deepops.c:2612`, `:3475`, `deepdefence.c:5059`) and those gates fire
already. What is not gated is everything scored behind them: the garrison
(`deepcombat.c:597`), the enemy's own valuation of the city
(`deepops.c:2620`), and the per-turn march re-read (`deepops.c:4258`). So
deep does not starve for targets; it keeps a menu of remembered cities and
prices each one with the defender's books open. Under a knowledge layer the
menu barely changes and every price does.

**"Active self-harm rather than stalls" — CONFIRMED statically.** The wave
is a count. An unseen wave is `wave == 0`, and 14 subsystems read 0 as
safety: `deep_fear_satisfaction` (1856) discounts all injected fear,
`deep_melt_surplus_defenders` (2198) disbands on `p_without == 1.0`,
`deep_defend_counter_claims` (3228) `continue`s so the entire relief system
never runs, `deep_deterrence_dividend` (4150) early-returns 0. Symmetrically
an empty-looking garrison gives `P(take) → 1` at `deepcombat.c:597`. The
2026-08-04 prediction stands and now has line numbers.

---

## 5. Comments that claim an honesty the code does not have

These matter more than ordinary bugs: a future session reading them will
believe a read is already gated and skip it.

| Site | Claim | Reality |
|---|---|---|
| `deepcombat.c:564-565` | "military garrison only, **observed units only**" — in `deep_take_odds_cost`'s docstring | Nothing in the function implements observation. Directly contradicted by the same file's own 213-218 |
| `deepdefence.c:1774-1777` | "the shortfall funds extra defence — **from observed units only**" | The wave comes from `deepcombat.c:349`, which has no visibility test |
| `deepdefence.c:1901-1902` | "NOT scaled: the survival deficit prices an **OBSERVED** wave" — used as the *justification* for skipping a stance multiplier | Same false premise, now load-bearing on a design decision |
| `deepdefence.c:4111` | "run the chained-duel model against the **observed wave** twice" | Same |
| `deepdefence.c:4119-4120` | "Times the **observed attack pressure** — `deep_garrison_menace()`" | `deep_garrison_menace()` is `aplayer->score.units` (1235), the single least observable input in the file |
| `deepdefence.c:2608` | "Nothing **(visible)** to duel" | `get_defender()` at 2605 has no visibility filter |

**Honest self-reports, for contrast, and they should be the model:**
`deepcombat.c:210-218` states the wave is "Real units only … But NOT
observed"; `deepops.c:117-121` and 434-436 admit the `worth` read and the
capture census; `deepdefence.c:3392-3395` says "Omniscient today";
`deepsettler.c:1880-1881` says "Omniscient (RC6)"; `deepdefence.c:5090-5092`
cites the prior census by finding number.

**One prior finding has been fixed and should be struck from the list.** The
2026-08-04 census §5.6 flagged `deepdefence.c`'s claim that `score.units` is
public via the historian. `deepdefence.c:1164-1173` now says the opposite,
correctly, citing `server/report.c`. The *read* at 1235/1243 is unchanged;
only the comment was corrected — which is the right order of operations, and
worth noting because it is the only place in the module where an overclaim
was retired rather than inherited.

---

## 6. Stock paths deep still delegates — Tier 3, out of scope, unchanged

Per the brief and the standing scope guard, these are recorded and not
touched. They are already upstream-bug candidates 11-14 in
honest-information §9.

| Path | Delegation | Fog status at this HEAD |
|---|---|---|
| War / peace | `dai_diplomacy_actions` → `dai_war_desire` (`daidiplomacy.c:797`) | Perfect census of every enemy city's size, surpluses, buildings, every unit, treasury and exact tech count. **Zero information-handicap checks, at every difficulty including AWAY.** honest-information §5.5 remains entirely unbuilt |
| Tech | `dai_manage_tech` | `daitech.c` still has **0** handicap checks (verified at this HEAD) |
| Player data | `dai_data_phase_begin` | `daidata.c` still has **0** handicap checks |
| City danger | `dai_assess_danger_player` | Iterates all rivals' units; only the *pathing* honours H_MAP (`daimilitary.c:824`) |
| Unclaimed-unit targeting | `find_something_to_kill` | Real lists plus skip-gates: it knows the city exists and declines to target it |
| Ferries / hunters / diplomats / air / paradrop | stock | Tier-1 gated; the flag reaches these |

**The flag makes the Tier-1/Tier-3 split visible for the first time.** With
`honest_info = 1`, deep's pathfinding and stock's ~42 gates go honest while
`dai_war_desire` keeps its perfect census. That is not a bug in the flag; it
is the shape of the problem, and it means any measured breakage under the
flag is a **lower bound** on what a genuinely honest AI would suffer.

---

## 7. What the knowledge layer must provide, in order

Unchanged in substance from the 2026-08-04 census §4; re-stated with this
census's evidence and re-ordered by what §3 says is load-bearing.

1. **`known_force` — sightings of units — is the single largest
   prerequisite, and it is new persisted state.** The server remembers cities
   and never units, so §3.1's 43-consumer wave, `deepdefence.c:347/441/1482/2669`,
   `deepsettler.c:1907/2201`, `deepops.c:2781`, and `deepdiplomacy.c:974` all
   need last-seen sightings with confidence decaying as age × move rate.
   **This contradicts honest-information §7's "save nothing and rebuild on
   load"** — a carve-out is required. Deep has a save namespace and the
   ops-persistence precedent.
2. **`known_city` from `map_get_player_city()` per phase** — cheap, and it
   unblocks the (b) column wholesale: density fear, border pressure, site
   pressure and contest, the floor's border/air classes, embassy reachability,
   the diplomacy city counts, and the trade candidate set.
3. **§5.4 priors over the unknown must land before or with any flag flip in
   site eval.** §4 shows why: `deepsite.c:589-598` has no place to put an
   expected value today, so honest expansion deflates from the frontier
   inward.
4. **An own-side valuation to replace enemy `->worth`.** Six sites
   (`deepops.c:127/2620/3495/4127`, `deepdefence.c:4844/5067`) reduce target
   selection, retarget selection, poach arbitration, the net-value launch
   veto and the colony-lift bid to the *victim's* private bookkeeping. A
   remembered `vision_site` carries size, walls and an improvements bitvector
   — enough to price a city without asking its owner.
5. **Staleness projection before staleness-tolerant re-gating.** Extend the
   eta-reinforcement window at `deepcombat.c:614-637` from `eta` to
   `eta + intel_age`; only then can `deepops.c:4258`'s march re-gate run on a
   remembered garrison without thrashing. The existing hysteresis
   (`op_abort_floor` below the formation bar) already gives estimation-noise
   tolerance for free.
6. **Recon as a want, with the op formation bar as its natural seat**, and
   `deep_raid_victim` (`deepops.c:2781`) as its natural executor — an honest
   raid is a patrol.
7. **Mechanical, independent, any time:** `map_get_player_tile()`
   substitution for terrain/resource/owner in `deepsite.c` (which never calls
   it); rank-bucket estimation for `score.units`; observed-transport bits for
   the boat shadow; seen-gates plus threat memory on worker flee and worker
   task value.

---

## 8. Where the older documents are now stale

Recorded per CLAUDE.md's rule rather than silently corrected.

- **`findings/deep-dive-2026-08-04/omniscience-census.md`: every line
  number.** Read at `36bac91163`. Its §1.2 cites `deepdefence.c:2937-3149`
  for assault want (now 5055-5297) and `:758-814` for garrison menace (now
  1164-1246). Its §6 tally of 18 gated sites is now 19 (`deepsite.c:989` is a
  second `H_MAP` gate; `deepdefence.c:3396`'s `H_AWAY` was never an
  information handicap and should not have been counted). Its findings
  #1-#5 all still hold.
- **`findings/honest-information.md` §4 "Settling" row** is stale in the way
  the 2026-08-04 census already noted — deep has an explorer want and a
  pre-wired discovery term — and now also imprecise about the mechanism: see
  §4 above.
- **`findings/honest-information.md` §7 "save nothing"** needs the
  `known_force` carve-out.
- **`ai/deep/deepexplore.c:169`** cites `deepsettler.c:258,305,412,456,535`
  for the omniscience sites. The real ones are 263, 307, 359, 485, 705, 752,
  831, 1189, 1238, 1407. In-code drift, harmless, noted.
- **`ai/deep/deepsite.c:1213`** cites `deepsettler.c:711` for the shared
  `site_enough_result` threshold; the test is at 712.

---

## 9. What this session did NOT do

Stated plainly so the next session does not assume it.

- **No games were run.** A tuning batch owned the machine; the human's
  standing instruction for this session was build and unit tests only.
- **Byte-identity of `honest_info = 0` is PENDING PROOF.** The exact command
  is in `STATUS.md`.
- **The flag has never been executed with `honest_info = 1`.** The
  `deep-honest` log line exists precisely so that the first run can prove
  the bits reached the player rather than assuming it — the 40-game batch
  lost to an unverified switch is the reason.
- **The runtime breakage census — brief item 3 and the
  `findings/omniscience-flag-breakage.md` deliverable — is QUEUED.** §4's
  predictions are static: they are what the code says will happen, not what
  was observed. The run should instrument melt events, assault formations
  versus actual garrisons, foundings, explore fraction and losses, in at
  least isolated-150 and one land cell.
- **Nothing was fixed.** By instruction. Not one of the 148 reads was
  changed, and no inference, knowledge struct, recon want or staleness
  projection was built.
