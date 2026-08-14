# Census of ground-truth dependencies in `ai/deep` (reader report, verbatim)

Raw material for the de-omniscience (RC6) roadmap. Read at HEAD `36bac91163`. All file:line references verified by reading the cited code unless marked *(inferred)*.

---

## 0. The headline findings

1. **Deep never reads remembered state either.** `grep map_get_player_tile\|map_get_player_city ai/deep/` returns nothing — honest-information §1 Tier 2 still holds for the new module. Every deep mechanism reads ground truth, with ~18 `H_MAP`/`H_FOG` gates layered on top.
2. **The biggest new omniscience surface is the probabilistic combat core.** `deep_city_stack_survival` and `deep_take_odds*` read exact enemy unit lists and exact enemy garrisons, and feed ~20 downstream decisions. This one input family is most of the de-omniscience problem.
3. **The failure direction inverts under fog.** Stock under `H_MAP` fails by *paralysis* (skips unknown targets). Deep's gates mostly fail *dangerously optimistic*: an unseen wave reads as `wave == 0` → survival 1.0 → fears satisfied → garrisons melt; an unseen garrison reads as empty → `P(take) = 1` → assaults form everywhere. **Absence of data becomes confidence.** The flag-flip experiment must expect active self-disarmament and overreach, not just stalls.
4. **Deep's targeting reads the enemy's own private self-valuation.** `def_ai_city_data(acity)->worth` (read at deepdefence.c:2989, 3002; deepops.c:2057, 2834, 2956) is filled by the *owner's* `dai_city_want` run (ai/default/daicity.c:1958–1966 via CALL_FUNC_EACH_AI, advbuilding.c:180) — internal surpluses, buildings, owner's priorities included. Corollary *(consequence inferred)*: for a **human**-owned enemy city the slot is never refreshed, so targeting quality silently differs against humans.
5. **The anticipatory-fear architecture is already fog-shaped.** The observed/anticipatory split (deepdefence.c:842–850, 973–974) prices anticipation from lobby settings, contact state, and onset gates. That half survives fog *unchanged*; only the observed half needs its inputs replaced.

---

## 1. Census table

Categories: **(a)** already handicap-gated; **(b)** direct remembered-state substitute exists (`map_get_player_tile`/`map_get_player_city`); **(c)** needs real inference; **(d)** legitimately knowable.

### 1.1 deepcombat.c — the survival/take-odds core

| Site | What is read | Cat | Replacement | Host |
|---|---|---|---|---|
| :217–251 `deep_city_survival_core` | every at-war player's **full unit list**: type, hp, position, transported — no visibility gate. Header (:182) says "Real observed units only" but "real" means *extant*, not *seen* | **(c)** | `known_force`: last-seen sightings, position confidence decaying with age × move rate; unseen mass covered by the existing anticipatory terms (designed for that) | same fn — interface survives; census source changes |
| :121–133 via `deep_take_odds*` | exact **enemy garrison stack** | **(c)** | remembered `vision_site` (size, walls, occupied) + reactive-garrison projection over `intel_age + eta` — the eta-reinforcement model at :353–392 **already is this estimator**; extend its window to staleness | `deep_take_odds_at_arrival` |
| :370–377 | `can_city_build_unit_now()` on an enemy city — enemy tech | **(c)** | embassy-known techs; else era estimate + worst-case best defender | same |
| :360 | `is_barbarian(owner)` | (d) | — | — |

### 1.2 deepdefence.c

| Site | What is read | Cat | Replacement |
|---|---|---|---|
| :325–368 overseas danger | at-war embarked units, exact positions | **(c)** | known_force for seen boats; boat-shadow prior covers unseen |
| :422–446 boat shadow | global census: any at-war player owns any sea transport | **(c)** | per-rival observed-capability memory bit + embassy tech inference |
| :465–498 free-land share | **(a)** gated with documented honest fallback | (a)+(b) | fogged owners from `player_tile->owner` |
| :544–587, :610–648 crowding/prior | lobby settings only | **(d)** — model §5.4 example, already built | — |
| :658–714 density fear | met rivals' city positions (b); lazy transport scan (:695–703) (c) | (b)/(c) | known_city; observed-transport bit |
| :758–814 garrison menace | `aplayer->score.units` exact ratio | **(c)-lite** | comment (:736–743) claims this is public via the historian; **it is not** — historian broadcasts *rank + name only* (server/report.c:283–303, 335). Replace with rank-bucket estimate + own observations; embassy refines |
| :852–937 deficit fear / satisfaction | via stack survival | **(c)** input | inherits deepcombat fix |
| :1257–1322, :1416–1457 sorties/tired | adjacent tiles — own vision in practice | **(d)**-ish *(adjacency⇒visibility inferred)* | explicit seen gate for hygiene |
| :1468–1580 worker flee/guards | at-war units in radius, ungated | **(c)-lite** | seen-only + **threat memory** (the ambush-amnesia problem) |
| :1921–1948 first_activities | delegates stock `assess_danger`: iterates ALL rivals' units, no vision gate; only pathing honours H_MAP (daimilitary.c:824) | **(c)** inherited | same known_force |
| :2596–2648 embassy want | `player_has_embassy` (d); rival city reachability (b) | (d)/(b) | known_city |
| :2937–3149 assault want | target menu gated map_is_known + DS_NO_CONTACT (:2972–2986) **(a)**; then `->worth` **(c)** (finding #4); take-odds **(c)** | mixed | own-side valuation of known_city (`vision_site` carries size, walls, improvements bv — enough to price a city without asking its owner) |
| :3192+, :3563+, :3816+ gold/cities/treasury | own state | **(d)** | — |
| :3396 | `H_AWAY` gate | (a) | — |

### 1.3 deepops.c

| Site | What | Cat | Replacement |
|---|---|---|---|
| :2026–2067 pick target, :2771–2906 retarget | rival menu: map_is_known + contact **(a)** — but "known" = *ever seen*, no staleness; `->worth` **(c)**; take-odds **(c)** | (a)+(c) | staleness stamp (`player_tile->last_updated` is free); recon precondition at the formation bar |
| :2187–2219 `deep_raid_victim` | every at-war unit list, hunting **civilians by exact position incl. fog** | **(c)** — purest omniscience site in deep | seen-only victims + sightings; an honest raid becomes patrol/recon — natural double-duty as the recon executor |
| :2390–2405 target-threatens | barbarian ownership, own continents | (d)/(b) | — |
| march/strike/poach/consolidate take-odds re-gates (:2538…:3437) | **continuous exact surveillance of the target garrison every turn of a march** | **(c)** | staleness-projected garrison; existing hysteresis (`op_abort_floor` < formation bar) gives estimation-noise tolerance for free |
| :3180–3218 strike loop | `get_defender` at the walls (adjacent) | (d) | — |
| :997, 1308 | pf omniscience gates | **(a)** | — |
| censuses/convoy/manage | own state | **(d)** | — |

### 1.4 deepsite.c

| Site | What | Cat | Replacement |
|---|---|---|---|
| :526, 590, 870–871 | `H_MAP` gate: unknown tile → **zero / rejected** | **(a), degenerate** | §5.4 priors: `E[tile] = P(land)·mean + hut·… + P(unclaimed)·claim` from generator settings — without this, honest expansion stops |
| :591 | `tile_worked()` — incl. unseen rivals' | (b/c) | infer from remembered city ownership + radius |
| :566–580, :731–756 pressure/contest | rival city positions | **(b)** | `map_get_player_city` — nearly lossless here |
| :651–653 etc. | terrain of fogged tiles | **(b)** | `player_tile->terrain`; unknown → priors |
| :675, :958–966 | `tile_owner` | (b) | `player_tile->owner` |
| :127–151 choke | neighbour terrain | (b)+priors | — |
| :355–385, growth sim, stream | own/virtual state | **(d)** — the catchment-stream simulator is fog-clean by construction | — |
| :882 | `adv_danger_at` (seen-gated) | (a/d) | — |

### 1.5 deepsettler.c

- :144–158 — the only `H_FOG` gate in deep; fallback "cannot see it: assume no danger" is the ambush-amnesia degenerate case → wants threat memory.
- :262, 306, 358, 484, 704, 751, 830, 1188, 1237, 1403 — uniform pf omniscience gates **(a)**.
- :1844–1858 worker task value — at-war positions in radius, ungated — **(c)-lite**, same fix as flee.
- Everything else own-state **(d)**.

### 1.6 deepexplore.c, deeptrade.c

- deepexplore.c:128–145 — unknown count honest; hut extras/continent of fogged tiles are mild leaks (b-lite). :166–181 — discovery term **zeroed while omniscient**, documented to return "the moment W04 makes deep honest": **(a), exemplary** — the one mechanism already written *for* the honest world.
- deeptrade.c:225 — the `!H_MAP` flag gates only **pathfinding** (caravan.c:227); the *valuation* reads foreign cities' trade internals — **(c)** under fog; estimate from remembered size.

### 1.7 Own-state files (all category d)

deephorizon.c, deepcm.c, deepstance.c, deepwant.c, deepodds.c, deeptuning.c — own state only, except horizon (:544–551) and CM (:153–165, :209–217) consume stack survival (category-c input); they inherit whatever honesty it has.

---

## 2. Stock paths deep still delegates

| Path | Delegation | Fog status |
|---|---|---|
| **War/peace** | `dai_diplomacy_actions` → `dai_war_desire` (daidiplomacy.c:797) | Tier-3 perfect census, unchanged. honest-information §5.5 remains 100% unbuilt. |
| **Tech** | `dai_manage_tech` | daitech.c still **0** handicap checks (verified); mostly own-state; deep's injected wants are own-state. |
| **Gold** | replaced — fog-clean | — |
| **Taxes/government** | stock — own-state | — |
| **City danger** | `dai_assess_danger_player` | units ground truth; only pathing gated (daimilitary.c:824); no H_FOG on the units themselves. |
| **Unit targeting** | unclaimed units → `find_something_to_kill`; `kill_something_with` called directly (deepdefence.c:2680, 2712) | tier-1/2: real lists + skip-gates; knows the city exists, declines to target. |
| **Ferries/hunters/diplomats/air/paradrop** | stock | tier-1 gated per honest-information §1. |
| **Replaced by deep** (stale rows of honest-information): site scoring, settler search+routing, explorer want, emergencies, build merge, caravan params, assault sizing (ops = partial delivery of §7's campaign hoisting). |

---

## 3. Fog-fragile vs fog-robust

**Load-bearing on exact knowledge (the expensive list):**
1. `deep_take_odds*` input side — model survives estimation; census doesn't. Server remembers cities but **not units** — `known_force` is new deep-persisted state, **not recomputable from `player_tile`** (contradicts honest-information §7's "save nothing"; carve-out needed).
2. Continuous march re-gating — needs staleness projection + recon refresh; hysteresis already gives noise tolerance.
3. `deep_raid_victim` — no honest analogue without recon behaviour.
4. `dai_war_desire` (inherited).
5. Enemy-city `worth` — the self-valuation channel has no honest analogue; needs a deep-side estimator over known_city.
6. Boat shadow / density-fear floats — global capability census; needs observation memory.
7. Garrison menace — exact score ratio mislabelled as public.

**Fog-robust already:** the anticipatory fear lattice (prior, crowding, land pressure, ramp, onset, contact gating); free-land share; explorer want (pre-wired); catchment-stream simulation; horizon credits; CM state machine; stance gates; want tiers; treasury; ops *state machine* (control flow needs no omniscience); `deep_op_eta` (geometry); eta-reinforcement projection (is itself the §5.2 estimator).

---

## 4. Ordering constraints for the roadmap

1. **P0.0 flag-flip experiment first** — but expect *active self-harm*, not stalls: melt/rotation and fear-satisfaction run on wave==0; assault formation on empty-looking garrisons. The failure list is the requirements doc; headline items are predictable from this census.
2. **§5.4 priors must land before (or with) the flag flip in site eval** — deepsite.c:590 zeroes unknown tiles; honest deep stops expanding turn ~1 otherwise. Explorer discovery term turns itself on automatically — W06 is load-bearing-ready.
3. **`known_city` before honest targeting/fears** — buildable from `map_get_player_city` per phase (cheap; includes improvements bv + walls + size). Unblocks density fear, site pressure, target menus, embassy reachability, honest `worth` replacement.
4. **`known_force` before honest survival/take-odds** — new deep-persisted sighting state. Everything in §1.1 and inherited assess_danger depends on it. The single largest prerequisite.
5. **Staleness projection before staleness-tolerant re-gating** — extend reinforce window from `eta` to `eta + intel_age`; only then can march re-gates run on remembered garrisons without thrashing.
6. **Recon wants after staleness stamps, before op formation preconditions** — the formation bar (deepops.c:2478–2486) is the natural seat for "intel newer than N turns"; the raid op is the natural recon executor; `deep_explore_want`'s structure is the template. Staleness tolerance is already a difficulty knob in honest-information §6.
7. **Honest `dai_war_desire` last of the core sequence** — needs known_city + known_force + embassy; W04 precedes W34 phase B; W34's gates are where uncertainty-penalized war appetite plugs in.
8. **Independent, any time:** menace via historian rank buckets; observed-transport bits; seen-gates on flee/worker-task-value with threat memory; `player_tile` terrain/owner substitution in site eval ((b) items are mechanical).

---

## 5. What honest-information.md gets wrong or stale

1. **§4 "Settling" row stale**: deep has an explorer want; disband hack gated; discovery term pre-wired. Remaining gap is §5.4 priors, not exploration.
2. **§4 "Danger" row conflates two paths**: `adv_danger_at` is seen-gated as stated; the city-danger path deep inherits (`assess_danger`) reads all enemy units with no vision gate.
3. **§7 "save nothing" needs a carve-out**: `known_force` must be saved (unit sightings are not in the server substrate; deep has a save namespace and ops-persistence precedent).
4. **§7 "cheaper in places" partially delivered**: op-level target picking already hoists FSTK for claimed units.
5. **New and unanticipated**: deep added a *second* strategic consumer of exact enemy state — the survival/take-odds economy — larger than `dai_war_desire` in decision-fanout. §5's estimators still fit it, but "read the fog-of-war model that has been sitting there" undercounts: **the fog-of-war model has no units in it.**
6. **Deep's own comments overclaim honesty twice**: deepcombat.c:182 ("Real observed units only" — no observation gate exists) and deepdefence.c:736–743 (score.units called public — the broadcast is rank+name only). Latent findings items, not existing honesty.

---

## 6. Gated-site tally (category a)

deepexplore.c:179; deepsite.c:526/590, 870–871; deepdefence.c:468, 3396 (H_AWAY); deepops.c:997, 1308; deeptrade.c:225; deepsettler.c:144 (H_FOG), 262, 306, 358, 484, 704, 751, 830, 1188, 1237, 1403 — 18 sites. All pathfinding/tile-knowledge gates uniform; the two degenerate fallbacks are site-eval's unknown-tile-worth-zero (deepsite.c:590) and the overnight veto's assume-no-danger (deepsettler.c:146).
