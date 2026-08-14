# Expansion & logistics in `ai/deep` — mechanism audit (reader report, verbatim)

Scope: `deepsite.c`, `deepsettler.c`, `deepexplore.c`, `deepops.c` (+ headers), plus the consumers of their outputs in `deepdefence.c` (build side) since the pipeline is unreadable without them. Everything below is **verified by reading the code at HEAD** unless marked *(inferred)*.

Out of scope, one line each: threading, honest-information/W04 inference, and ally conversation (`deep_brief_allies`, `deepops.c:2136`) are not covered.

---

## 0. Turn seams (the frame everything else hangs on)

Verified ordering, one player-phase:

| # | Seam | Call site | What runs |
|---|---|---|---|
| 1 | `first_activities` | `server/srv_main.c:1146` → `deepdefence.c:1921` | `deep_run_operations()` (`deepdefence.c:1935`) forms/drives lifts and assaults; then `deep_manage_units()` (`:1941`) |
| 2 | `auto_workers_player` | `server/srv_main.c:1524` → `server/advisors/autoworkers.c:1402` | `settler_run` hook per worker/founder → `deep_auto_settler_run()` (`deepsettler.c:1465`): site search, mission, `aiferry` request |
| 3 | `last_activities` | `srv_main.c:1526` → `deepdefence.c:3909` | `deep_manage_cities()` (`:3563`): explorer want, `deep_contemplate_new_city`, colony-lift boat want, build choice |

Consequence, verified: **a lift can only form on demand raised in a previous turn's step 2**, because `deep_lift_wants_boat()` (`deepops.c:1234`) requires `unit_ai.ferryboat < 0` (FERRY_WANTED), which is set inside `aiferry_gobyboat()` from `deep_settler_gothere()` (`deepsettler.c:397`). The passenger must first try the stock protocol and fail. `colony_want` written at step 2 (`deepsettler.c:761`) is consumed at step 3 (`deepdefence.c:2741`, guarded `colony_turn >= turn-1`).

`auto_workers_player`'s gate is `ssa_controller == SSA_AUTOWORKER || is_ai(pplayer)` (`autoworkers.c:1372`) — the `|| is_ai` is why op holds needed an explicit early return at `deepsettler.c:1501`.

---

## 1. Mechanism inventory

### 1.1 Site evaluation (`deepsite.c`)

| Mechanism | Location | Decides | Inputs | Output | Gating tunables |
|---|---|---|---|---|---|
| Growth simulation | `deep_growth_arrivals()` `deepsite.c:209` | *when* each ranked catchment tile gets a citizen; when growth stops for good | ranked food array, centre food, founding size, food stock, `city_granary_size()`, `EFT_SIZE_ADJ/UNLIMIT` | `arrive[]` (turn per tile, −1 = never) | none (deliberate: no horizon constant) |
| Size-gate liftability | `deep_size_gate_is_liftable()` `:166` | whether a ruleset size cap is final | improvement list, `can_city_build_improvement_later` | bool, lazily evaluated | none |
| Neighbour claim | `deep_claim_build()` `:258`, `deep_city_claim()` `:324` (per-turn cache), `deep_neighbour_claim_turn()` `:355` | earliest turn one of *our* existing cities works a tile | own cities' catchments, real size + food stock, `adv->*_priority` | claim turn or −1 | none |
| Catchment stream | `deep_catchment_stream()` `:435` | the site's value as Σ `amortize(sum + w·potential, arrive)` | sums/foods/potentials/claimed arrays | `adv_want` | `site_potential_discount` |
| Site fill | `deep_cityresult_fill()` `:516` | the full site total | tile yields (tdc cache), `citymap_read` reservations, resources, foreign tile ownership, rival city proximity, choke geometry | `cityresult` | `site_potential_weight`, `site_pressure_factor`, `site_special_bonus`, `site_special_cluster`, `site_claim_bonus`, `site_rival_pressure`, `site_rival_crowding`, `site_choke_bonus`, `site_stream_weight` |
| Contest context | `:566-580` | whether a non-allied city sits within `4·radius_sq` | all alive players' cities | `pressured` bool → compresses potential | (bound is hardcoded `4 *`) |
| Choke test | `deep_site_is_choke()` `:127` | is this tile a land gate (≥4 land/sea transitions on the 8-ring, 2..5 land) | 8 neighbours | bool → `site_choke_bonus · deep_map_crowding()` | `site_choke_bonus` |
| Foundability gate | `deep_city_desirability()` `:857` | reject site | `city_can_be_built_here`, citymindist reservation scan (`:876`), `adv_danger_at`, `citymap_is_reserved`, add-to-size limit | `cityresult` or NULL | none |
| Distance discount | inside `deep_settler_map_iterate()` `:998-1011` | how hard to punish travel | pathfinder `move_cost / move_rate`, `deep_map_crowding()` | `amortize(total, d·turns)` | `site_travel_discount`, `site_travel_crowding` |
| Candidate search | `deep_settler_map_iterate()` `:936` | best site over a pf map | pf map, boat_cost | best `cityresult` | `site_settler_cost_weight`, `site_boat_cost_weight`; early break at **compile-time `RESULT_IS_ENOUGH`** (`:1033`) |
| Build-side want | `deep_contemplate_new_city()` `:1051` | `city_data->founder_want`, `founder_boat` | virtual founder unit, coastality | `founder_want` (negative ⇒ "need a boat", consumed by `daidomestic.c:572`) | via the search |

### 1.2 Settler routing & mission (`deepsettler.c`)

| Mechanism | Location | Decides | Gating |
|---|---|---|---|
| Risk-aware pf parameter | `deep_settler_apply_risk()` `:200` (+ `deep_settler_tile_behavior()` `:96`, `deep_civilian_pos_dangerous()` `:172`) | which tiles a settler may route through / end a turn on | `settler_fearfulness_mult`, `settler_risk_tolerance` |
| Overnight-danger bar | `deep_settler_overnight_veto()` `:122` | copy of `dai_consider_tile_dangerous()` with a tolerance multiplier (rule-3 copy, reason stated) | tolerance argument |
| Walkability oracle | `deep_settler_can_walk_there()` `:251` | boat-needed vs walkable, at the player's real vision | none |
| Bodyguard request | `deep_settler_bodyguard_needed()` `:277` | samples the *planned route*, not just the destination | `settler_guard_tolerance` |
| Ferry-enabled goto | `deep_settler_gothere()` `:375` | walk / call `aiferry_gobyboat()` / release boat on landing | — |
| Placement search (2 phases) | `deep_find_best_city_placement()` `:684` | on-continent vs overseas | `site_enough_result` (`:711`), **1:1 ferry cap** `passengers >= boats` (`:721`), `ferry_wait` amortization (`:838-845`) |
| Colony-demand probe | `:738-772` | when the cap refuses, still *score* the best overseas site with a virtual boat and publish `dp->colony_want` / `colony_turn` | inside the cap branch; requires coastal city tile |
| Idle-work fallback | `deep_idle_fallback_work()` `:1214` + `deep_idle_scan()` `:1019` | soonest-completed road/irrigation/mine when the normal want is zero | ordering only (no threshold) |
| Cross-water worker | `deep_idle_cross_water()` `:1138` | may a worker take a spare hull to an overseas colony | 1:1 cap re-checked (`:1165`), real boat only (`:1175`), `worker_cross_continent_discount` |
| Join City | `deep_settler_join_city()` `:1325` | dissolve a surplus founder into a city | `founder_pool_per_site · deep_remaining_sites()` (`:1383`), `city_can_grow_to` (`:1408`), homeless-first bias unless `killunhomed` |
| Mission driver | `deep_auto_settler_run()` `:1465` | op hold (`:1501`) → BUILD_CITY (`:1508`) → validity re-check (`:1525`) → move → found; boat-less drop + same-turn land-only re-search (`:1612-1641`) | `land_only` latch |
| Worker task discount | `deep_worker_task_value()` `:1829` | de-rate exposed tiles on the advisors' task hook | `worker_danger_discount`, `worker_guard_radius` |
| Diagnostics (no decision reads them) | `deep_settler_risk_route()` `:465`, `deep_settler_stall_reason()` `:639`, `deep_settler_lift_state()` `:545`, `deep_settler_nearest_boats()` `:566`, `deep_settler_boat_distance()` `:594`, `deep_settler_tile_claim()` `:613`, `deep_idle_work_census()` `:907` | — | metric log level |

### 1.3 Exploration (`deepexplore.c`)

`deep_explore_want(pplayer, queued)` `:54` — one global want, consumed once per city in the build pass (`deepdefence.c:3669-3684`) with `queued` incremented so one want orders one explorer, and again by `deep_spend_gold(…, explore_want)` (`deepdefence.c:3788`).

- Hard zero when `wld.map.server.huts <= 0` (`:64`).
- Reachability filter: only continents where we have a city or unit (`:87-110`) — the seed-205 "explorer pump" fix.
- Hut risk: huts within `hut_risk_radius` of an own city weigh `hut_risk_discount`, but only while `barbarianrate != BARBS_DISABLED && turn >= onsetbarbarian` (`:77`, `:133`).
- Supply count includes explorers **under construction** (`:159-164`).
- **Discovery term is zeroed whenever `!has_handicap(H_MAP)`** (`:179-181`) — i.e. at every difficulty anyone plays, exploration is worth *huts only*.
- Want = `(hut_weight·explore_hut_value + unknown·explore_discovery_value) / (1 + explorers + queued)`.

### 1.4 The lift / convoy pipeline, end to end

Traced flow with its seams:

**(a) Want formation.** Site search picks an overseas target → `adv_unit_new_task(AUT_BUILD_CITY)` + `punit->goto_tile` (`deepsettler.c:1710-1716`). Next mission turn, `deep_settler_gothere()` (`:375`) sees `!can_walk` and calls `aiferry_gobyboat()`, which raises FERRY_WANTED. **Seam 1:** the lift never sees demand until stock's protocol has already been asked and failed; `deep_lift_wants_boat()` reads only the *sign* of `ferryboat`.

**(b) Gate.** Two independent admission rules, and they disagree:
- Site search: `plr_data->stats.passengers >= plr_data->stats.boats` refuses overseas evaluation entirely (`deepsettler.c:721`).
- Cross-water worker: same cap re-checked (`:1165`).
- **The lift itself checks no cap at all** (`deep_op_form_lift()` `deepops.c:1252-1292` — it filters on unclaimed, cross-continent, `could_unit_load`, `lift_form_radius`). **Seam 2.**

**(c) Boat assignment.** `deep_op_form_lift()` picks the nearest (passenger, unclaimed boat) pair inside `lift_form_radius`. Claim is enforced by `deep_op_claiming()` (`deepops.c:72`), which treats a lift's `ferry` as firmly claimed as its manifest (`:86-89`).

**(d) Rendezvous.** `deep_lift_best_meeting()` (`:985`) minimises `max(boat turns, passenger turns)` by **real pathfinding on both sides**, tie-broken on the sum; the tile is fixed at formation into `op->rendezvous` (`:1353`). Beachhead = the core passenger's own `goto_tile` (`:1352`), and is refused if the hull cannot *path* to a floatable tile beside it (`:1324-1336`).

**(e) Muster (`DEEP_OP_MUSTER`).** `deep_op_drive_lift()` `:1453`: suspends SSA on members (`:1485-1491`), enrolls hitchers (`deep_lift_enroll()` `:1099`, capped by `lift_manifest_max` ∧ hull capacity), boat sails to the meeting tile (`:1606`), passengers walk to the shore beside it (`:1639`) and board — including **same-turn board-and-sail** (`:1697-1721`). Patience for stragglers: `deep_lift_worth_waiting()` (`:1178`), `lift_hitch_wait_ratio`/`lift_hitch_wait_min`, never spending the window's last turn. Timeout `lift_gather_turns`.

**(f) Crossing (`DEEP_OP_MARCH`).** Per-member trace (`:1738`), `lift_cross_turns` budget (`:1814`), arrival when the hull is within 1 of the beachhead (`:1822`) including same-turn re-check after moving (`:1854`).

**(g) Drop.** `deep_lift_serve_stop()` (`:1382`): release everyone within 1 of their own target; re-aim at the nearest remaining target if within `lift_drop_radius`; else dissolve and let the rest walk. Dissolution reasons are the metric: `lift-arrived`, `lift-delivered`, `lift-strayed`, `lift-gather-timeout`, `lift-cross-timeout`, `lift-noboat`, `lift-nobeach`, `lift-unreachable`, `lift-nobody`.

**(h) Slot allocation.** `deep_run_operations()` `:3505`: validate → form ≤1 assault and ≤1 raid into the army's 4 slots (`:3626-3666`) → **then** ≤1 lift, preferring its own range then army leftovers (`:3668-3697`, `lift_op_slots`, default 0) → drive all (`:3699`). **Seam 3:** the army's claim always precedes the lift's, and one lift forms per player per turn regardless of how many passengers wait.

**(i) Holds.** `deep_op_holds()` (`:107`) holds lift members through MUSTER **and** MARCH (unlike an assault, which releases transported members). Honoured in `deep_manage_units()` (`:1903`) and, since `b699ddefa1`, in the settler_run hook (`deepsettler.c:1501`). **Seam 4 (a real hole, see §3.5):** that early return sits *above* the mission-validity re-check at `:1525`, so a held passenger's target is never re-validated during a crossing.

**(j) Supply side.** `colony_want` → `deepdefence.c:2731-2882`: damped by per-water *surplus* (`deep_idle_ferries_on − deep_ferry_demand_on`, `:2788`), multiplied by `deep_stance_expand_mult` (`:2801`), amortized by build time (`:2866`), gated by `deep_city_water_bridges()` (`:2856`). Plus a berth-shortfall term for amphibious **MUSTER ops with a real `target_city`** (`:2815-2843`) — which **excludes every lift**, since a lift sets `target_city = 0` (`deepops.c:1344`). Three producers of a ferry bid now withdraw at formation: domestic founder's boat (`deepdefence.c:3424-3440`), deep's colony want (`:2851-2856`), stock's attacker ferry (`:2715-2723`); `deep_boat_choice_refused()` (`:3368`) is the backstop.

Also on the supply side: `deep_manage_units()` diverts a `Workers`-flagged ferry to `dai_manage_ferryboat()` when it has cargo or the player has waiting passengers (`deepops.c:1942-1949`) — upstream bug #33.

---

## 2. Unconsumed signals

**Computed and only logged (no decision reads them).** This is the largest class, and it is deliberate instrumentation — but several items are one line away from being a decision input:

1. `deep_settler_risk_route()`'s **blocker ladder** (`deepsettler.c:465-527`) computes *exactly why* a mission cannot route — `danger` / `behaviour` / `vision` / `attack` / `none` — and only prints it. The mission-drop logic (`:1612`) instead re-derives a coarser "can't walk and no boat".
2. `deep_lift_best_meeting()`'s `meet_turns` (`deepops.c:1311`) — the honest cost of the rendezvous — is logged at `:1362` and never compared against anything (not against walking, not against waiting for a nearer hull, not against forming at all).
3. `deep_wait_census()` (`deepops.c:785`) computes, per waiting passenger per turn: boats that could load it, distance to nearest, producer cities on its own water, producers anywhere. **This is precisely the tail statistic W36's sizing says survives**, and nothing consumes it.
4. `deep_hull_order_log()` / `deep_build_eta()` (`deepops.c:389`, `:441`) compute the best rival producer's ETA under three stock modes. Logging only (W36 retired the award; the computation stayed).
5. `deep_hitch_census()` (`:637`), `deep_lift_census()` (`:168`), `deep_hull_census()` (`:281`), `deep_order_census()` (`:571`) — all `log_do_output_for_level`-gated.
6. `deep_idle_work_census()` (`deepsettler.c:907`) splits open work by continent (`xopen`/`xtiles`/`xcities`) — the exact quantity `deep_idle_cross_water()` acts on — and the fallback never reads the census.
7. `deep_ferry_demand_near()`'s `nearest` distance (`deepdefence.c:2026`) is used only by the hull census; the colony want uses the *count* (`deep_ferry_demand_on`).
8. `huts_visible` in `deep_explore_want()` (`deepexplore.c:142`) — the weighted `hut_weight` drives the want; the raw count is log-only.

**Computed and discarded inside a decision:**

9. `claimed[i]` in `deep_catchment_stream()` (`deepsite.c:474-485`) is a *turn*, collapsed to a boolean keep/drop. How much sooner the neighbour arrives — the contest margin — is computed and thrown away.
10. `crd->tile` in the colony-demand probe (`deepsettler.c:766-769`) is logged; only the scalar `colony_want` is stored. **Nothing remembers where the colony demand pointed**, so the shipyard cannot know which water, and the next turn's probe may point somewhere else entirely.
11. `cr->remaining` is structurally always 0 (`deepsite.c:694`, added at `:925`) — dead field kept for stock shape.
12. Lift ops set `required = 1`, `march_eta = 0`, `staging_city = 0` (`deepops.c:1344-1349`) and never read them; the berth-shortfall want then filters on `target_city != 0` and misses lifts entirely (`deepdefence.c:2822`).

**Stock-side infrastructure:**

13. **`adv->oceans[]` is still never read by `ai/deep`** — the only occurrence is a comment (`deepdefence.c:2177`). `deep_water_bridges()` (`:2098`) reads `get_lake_surrounder()` instead, which answers *connectivity* but not `oceans[].size` (how big a sea is, hence how long a crossing on it can be) nor `.threat`. Ocean size is available and unpriced anywhere in the lift.
14. **Continent numbers are read**, and well: `deep_water_bridges` (`:2098`), the cross-water census (`deepsettler.c:914-921`), the explorer's own-continent filter (`deepexplore.c:87-110`), `deep_lift_can_make_meeting`'s walkability guard (`deepops.c:1058`). This is the one abandoned-infrastructure row §3 lists that expansion genuinely consumes.
15. **`ai_invasion` counters: still never built for own cities**, and nothing in expansion reads them. Deep computes its own boat-shadow fear instead (`deepdefence.c:440`).
16. `citymap` reservation is used only in two narrow places: the citymindist scan (`deepsite.c:876`) and a single `citymap_reserve_tile(best_other)` (`deepsettler.c:1714`). No site itself is ever reserved.
17. `struct player_tile` / `vision_site` (remembered map) — unread here, as §3 predicts; every search sets `omniscience = !has_handicap(H_MAP)` (`deepsettler.c:262, 306, 358, 484, 704, 751, 830`; `deepops.c:997, 1308`).

---

## 3. Missing couplings and emergent interactions

W36's verdicts are respected below: the **award, fall-through cascade and cancellation are retired** by `findings/commission-sizing.md` §4 and are not re-proposed. The surviving lead — **the passenger's own ETA for the tail of the wait distribution** (a fifth of spells hold three-fifths of the waiting) — is where §3.1 and §3.6 point.

### 3.1 Site scoring does not know the ferry-wait lesson it taught

`ferry_wait` is a **single post-hoc discount on the winner** of the overseas search (`deepsettler.c:838-845`): `deep_settler_map_iterate()` ranks every overseas candidate with no delivery term at all, then the champion is amortized once by the turns to the nearest available boat. Two consequences:

- **Ranking among overseas candidates is delivery-blind.** A beachhead the hull can path to and one it cannot are ranked identically — even though `deep_op_form_lift()` (`deepops.c:1324-1336`) refuses the second, with a comment recording that 21% of traced crossings (59/275) never closed distance at all.
- **The probe path prices nothing.** When the 1:1 cap fires, `colony_want` is the raw site score (`deepsettler.c:761`), with no wait term whatever.

*Emergent behaviour:* a settler picks a site that the lift will decline to form for, then joins the waiting population and eventually stops wanting a boat. STATUS measures 57.4% of waiting spells censored (never enrolled) on `isolated-150`. **Size of prize:** the wait census already computes the per-passenger spell (`deepops.c:785`); wiring the *tail* estimate into the overseas candidate loop is the one W36 lead that survived sizing, and it acts on ~20% of spells holding ~60% of all waiting.

**Proposed coupling (mechanism, no values):** make the overseas leg of `deep_settler_map_iterate()` discount each candidate by (i) hull-reachability of its beachhead — the test `deep_lift_meeting_tile` + `pf_map_position` already performs — and (ii) an expected-wait term drawn from the same water's demand/supply, rather than discounting one winner by one boat's ETA.

### 3.2 Site scoring pays a naval bonus for water the build side refuses to sail

`deep_city_desirability()` adds `naval_bonus(cr)` (`deepsite.c:921` → `daisettler.c:557`), a flat `NAVAL_EMPHASIS%` of total for adjacency to **any** ocean tile — including a sea that closes on its own continent. The build side then refuses to build hulls at exactly such a city (`boat_water_gate`, `deepdefence.c:3368`), and `deep_water_bridges()` (`:2098`) is sitting right there, free, already `extern`.

*Emergent behaviour:* sites are bid up for a maritime option that is geometrically unavailable, and the empire discovers this only at the shipyard. **Size of prize:** the gate's own sizing found closed-water hulls were 6.9% of hulls but 11.2% of idle time; the site-side mirror of the same geometry has never been measured, and it is a one-call change to score.

### 3.3 Exploration demand and the settler pipeline are mutually blind

`deep_explore_want()` (`deepexplore.c:54`) is a function of huts and (under `H_MAP` only) unknown tiles. It never reads `deep_remaining_sites()` (`deepdefence.c:518`), never reads `founder_want` anywhere, and never asks where the settler pipeline intends to go. Symmetrically, the site search never requests reconnaissance.

Under the shipped configuration the discovery term is **identically zero** (`:179-181`), so the entire exploration want is a hut-collection want. That is correct given RC6 and is documented as such — but it means **the AI has no mechanism by which "we are about to run out of sites" produces scouting**, and W04 will land the discovery term back into a want with no expansion coupling to give it meaning.

*Proposed coupling:* an explorer want term proportional to unexplored land **on continents the settler pipeline can reach** weighted by `deep_remaining_sites()` — inert today (discovery zeroed), live and correctly shaped the day W04 flips the flag. This is the W06→W04 ordering in `00-context` §7 made concrete.

### 3.4 Ops do not know what expansion wants, and expansion does not know what ops are about to take

Two directions, both open:

- `deep_op_pick_target()` (`deepops.c:2026`) ranks by `city_data->worth / (1+distance)`. Nothing weights a target by whether taking it opens settling room, and `deep_op_target_threatens()` (`:2390`) is the only geographic reasoning in target choice.
- `deep_cityresult_fill()`'s rival-pressure penalty (`deepsite.c:711-768`) counts every non-allied city within `4·radius_sq` as a permanent tax — **including a city an assault is three turns from capturing**. `deep_op_targeted()` (`deepops.c:2005`) can answer "is this city claimed by one of our ops" in O(8).

*Emergent behaviour:* deep prices down exactly the ground it is about to conquer, and re-prices it up only after the capture, by which time settlers have been sent elsewhere. **Size of prize:** unmeasured, and cheap to instrument — count candidate sites whose `contest` sum includes a city under one of our own live ops.

### 3.5 A held lift passenger's mission is never re-validated

`deep_auto_settler_run()` returns at `:1501` for a held unit, **before** the `city_can_be_built_here()` check at `:1525` that would abandon a dead mission. The lift's beachhead is fixed at formation (`deepops.c:1352`) and re-aimed only by `deep_lift_serve_stop()` (`:1382`), which reads `m->goto_tile` — a field nothing updates during the hold.

*Emergent behaviour (inferred, but the code path is unambiguous):* if the target tile is claimed, founded on, or falls inside `citymindist` of a new city while the manifest is at sea, the lift completes the crossing anyway and drops a settler on an unfoundable tile; the mission then dies on the first unheld turn and the settler re-searches from a beach. Nothing in the metric distinguishes this from a successful `lift-arrived`. **Proposed mechanism:** a per-turn validity re-check inside `deep_op_drive_lift()`'s MARCH block (the same predicate the settler path uses), aborting or re-aiming rather than delivering.

### 3.6 The shipyard cannot see throughput, queue length, or lifts

Verified: boat want = `colony_want` (site quality) ÷ per-water surplus, plus a berth term that excludes lifts by construction (`target_city != 0`, `deepdefence.c:2822`). STATUS's own conclusion: "nothing in the build decision can see fleet productivity", and five count-based dampings all moved idle share not at all (75–80% invariant across five interventions).

`deep_ferry_demand_near()` already returns both a count and a distance; the wait census already reconstructs spells. The unpriced quantity is **the wait a passenger faces without a marginal hull**, which is what `hull-idleness.md` names as the remaining timing question. *Proposed coupling:* a lift-aware shortfall term (count MUSTER lifts short of hulls the same way amphibious musters are counted) plus a demand-side ETA term, replacing the idle-count damping that measured negative twice.

### 3.7 Three admission rules for one hull

The settler search enforces `passengers >= boats` (`deepsettler.c:721`), the cross-water worker re-enforces it (`:1165`), and the lift enforces nothing (`deepops.c:1252`). The lift can therefore claim a hull the settler search has already decided is unavailable — which is arguably *correct* (the lift is the thing that makes the trip happen) but means the cap now governs **site evaluation only**, not lift admission. Nothing states that intent anywhere; `findings/idle-workers.md` §10 still records the cap as the governing rule with a human "keep 1:1" call.

### 3.8 The founder pool counts land it cannot reach

`deep_remaining_sites()` (`deepdefence.c:518`) = global free land ÷ our own land-per-city, from a `whole_map_iterate` (`:477`) with no continent or reachability filter. It gates Join City (`deepsettler.c:1383`).

*Emergent behaviour:* on an island cell, free land on continents we can neither reach nor lift to inflates the pool target, so surplus founders are held rather than dissolved, indefinitely. `idle-workers.md` records the opposite failure (dissolving working expanders) being fixed by moving from city-count to site-count; the reachability half was not addressed.

---

## 4. Intelligence gap flags, ranked

What a strong human does in expansion/logistics that nothing here attempts.

**1. Reserve sites ahead of reach — a settling *order*, not a per-settler argmax.** (RC2, RC3.) Every settler re-runs `deep_find_best_city_placement()` from scratch, every turn, and the only cross-settler coordination in the file is `citymap_reserve_tile()` on the *single best-other tile* (`deepsettler.c:1714`) plus the citymindist scan (`deepsite.c:876`). Two settlers can converge; a site chosen for settler A is invisible to settler B until A reserves one tile of it. A human names the next four cities and sequences them. *Ranked first* because it is the purest RC2 instance in the domain, the infrastructure (`citymap`) is already present and half-used, and the failure is silent — it never appears in any current metric.

**2. Plan the crossing before the passenger is stranded.** (RC2, RC5.) The lift is strictly reactive: demand exists only after a settler has failed to walk (§0). A human decides "this island is the next expansion, build a transport, load two settlers and an escort, sail on turn N". The op layer has exactly the right object shape (`struct deep_operation`, named boat + manifest + beachhead) and no producer of *intent* — lifts are formed from stranded units, never from a plan. *Ranked second:* the substrate exists; only the trigger is missing.

**3. Route security and staging for civilians.** (RC5, RC2.) Deep prices per-tile risk well (`deep_settler_apply_risk`, `deep_settler_bodyguard_needed` sampling the route) but has no notion of a *corridor*: no staging point, no "wait here until the escort arrives", no timing a settler's arrival to a garrison's. Bodyguard is a request into `aiguard` with no rendezvous, in contrast with the lift, which does exactly this correctly for boats (`deep_lift_best_meeting`). *Ranked third:* the mechanism to copy is in the same tree.

**4. Value a colony net of what it costs to hold and to work.** (RC1, RC4.) `deep_cityresult_fill()` prices tiles, specials, chokepoints and rivals; it prices **no garrison, no upkeep, no distance-to-worker**. `idle-workers.md` §9 measured 12% of cities with no worker-capable unit within 8 tiles, and STATUS's own conclusion after five delivery repairs is "the colonies land and are not worth what they cost". A human will not settle an island it cannot defend or improve. *Ranked fourth on prize, would be first on evidence* — it is the one the project's own measurements point at hardest — but it is a scoring-model change with a wide blast radius, hence below the three structural gaps.

**5. Remember where you meant to go.** (RC2, RC6.) `colony_want` is a bare scalar refreshed every turn from whichever settler probed last (§2.10). There is no persistent expansion intent, so the shipyard, the explorer want and the site search each re-derive the frontier independently every turn from ground truth.

**6. Deny and time — settle *against* a rival's expansion.** (RC3, RC4.) `site_claim_bonus` (`deepsite.c:709`) rewards founding inside a rival's borders and `site_rival_pressure` penalises proximity; neither knows the rival's *rate* of expansion or which direction it is expanding in, both of which are readable. A human races for the choke and the isthmus on a schedule; deep has the choke test (`deep_site_is_choke`) and no schedule.

**7. Scout to expand.** (RC6.) §3.3. Ranked last only because it is inert until W04, per the `00-context` §7 ordering — but it should be built *shaped for that day*, not retrofitted.

---

## 5. Stale-plan notes (code vs documents)

1. **`00-context` §5 vs `deepsite.c:920`.** The context doc states: "If the inverted `result_defense_bonus` is never fixed upstream, `ai/deep` simply doesn't have that bug." **It does have it** — `deep_city_desirability()` calls `result_defense_bonus(pplayer, cr)` at `deepsite.c:920`, and the function (`daisettler.c:528-552`, `return 100/(total+1) * (100/defense_bonus * DEFENSE_EMPHASIS)`) is bug #1 in `findings/upstream-bugs.md`: higher terrain defence scores **lower**. Same call site also imports `naval_bonus()` (§3.2). This is the clearest doc/code contradiction in the domain.
2. **`site_enough_result` is only half-wired.** `deep_find_best_city_placement()` uses the tunable (`deepsettler.c:711`), but the early-termination break inside `deep_settler_map_iterate()` still compares against the compile-time `RESULT_IS_ENOUGH` (`deepsite.c:1033`). `idle-workers.md` §10 presents the constant as replaced.
3. **`deep_cityresult_fill()`'s header comment is stale** (`deepsite.c:509-515`): it still describes "centre + probability-weighted catchment average… the weighting subsumes it", which `deep_catchment_stream()`'s own comment (`:399-434`) explicitly supersedes ("This REPLACES a weighted average"). Likewise `:922-925` ("Add remaining points, which is our potential") documents a term that is structurally zero.
4. **`deepops.h:57-60` is contradicted by the code it describes.** The header says "Amphibious assaults still dissolve at launch: the convoy hold/escort/invade gates own the crossing unchanged". `deep_op_drive()` at `deepops.c:3332-3348` says the opposite and does the opposite: "Land or sea, the wave marches shadowed."
5. **`lift_manifest_max`: STATUS says default 1, code ships 8.** STATUS ("Hitch-a-ride, final: neutral even with routing live") records **"Recommendation: default `lift_manifest_max = 1` (off)"**; `deeptuning.c:88` ships `8` on a later human call (2026-08-04). Not a defect — but the recorded recommendation and the shipped default disagree and nothing reconciles them.
6. **`boat_idle_damping` similarly.** STATUS's "Two arms now agree: cutting boat production costs deliveries" ends with "Do not cut boat production further without measuring what the idle hulls do when demand arrives"; `deeptuning.c:90` ships `0.5` (the later per-ocean form measured +2.42, t=0.77 — direction, not a claim). Worth an explicit line in STATUS saying which measurement the shipped default rests on.
7. **The SSA suspension: STATUS records the revert, not the reapply.** STATUS says `ac1310ccd4` was "Reverted in `31a8016bc7`". It was reapplied in `b83a74c60d` on a structural argument ("the gate is an OR; shut both sides"), and the code is live at `deepops.c:1485-1491`. A reader following STATUS will believe that block is gone.
8. **Orphaned comment block, `deepdefence.c:1950-1976`.** The long docstring for `deep_expansion_blocked()` (war-footing gate, `expansion_war_survival`, the Europe T125 evidence) sits immediately above an *unrelated* comment at `:1977`; the function it documents is at `:2223`. Doc/code drift inside one file — worth noting because that gate is the single largest on/off switch on expansion (`:3658`).
9. **`findings/idle-workers.md` §10 lists the 1:1 cap as the governing settler/boat rule** with a human "keep 1:1" call and a note that a settlement party "needs the lift-as-an-operation work to exist first". The lift now exists **and bypasses the cap entirely** (§3.7). The finding reads as current and is not.

---

## Confidence

Verified by direct reading: all of §0, §1, §2 (except item 13's "never read" claim, which is verified only across `ai/deep/*.c` by grep), §3.1–3.4, §3.6–3.8, §5. Inferred but with the code path identified exactly: §3.5's failure mode (the early return at `deepsettler.c:1501` provably precedes the validity check at `:1525`; the *frequency* is unmeasured — nothing logs it). All prize sizings are quoted from `STATUS.md` / `findings/` measurements, not re-derived.
