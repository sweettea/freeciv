# Deep AI — the seam: what deep owns vs what it still delegates (reader report, verbatim)

**Scope note (out of scope, one line):** threading (W20), the honest-information / W04 inference layer, and ally conversation (W24) are not analysed here except where they explain a missing read.

**Method:** every claim below is *verified* against the tree at HEAD unless marked *(inferred)*. Line references are from the current tree, not `0896b34`.

---

## 0. Headline

`ai/deep` owns **turn structure, city production, settlers/sites, defence sizing, operations (assault/raid/lift), city-manager parameters, and the treasury floor**. It delegates **every player-level economic and political decision** — taxes, research direction, government, diplomacy and war declaration — plus **all per-unit tactics outside an operation**, to `ai/default`.

Counting the vtable: **~40 hooks forwarded untouched, 6 wrapped, 8 owned/replaced.** But hook count understates deep's share, because the two hooks it *replaced* (`first_activities`, `last_activities`) are where the turn happens. It understates deep's *dependency* too: `deep_do_last_activities` is stock's function with one line swapped.

The single sharpest fact: **`ai/default` has already been opened up substantially** — `daicity.h`, `daihand.h`, `daimilitary.h`, `daisettler.h`, `daiunit.h` all carry visibility-exception exports (verified by `diff -r ai/baseline/default ai/default`). **`daidiplomacy.h` and `daitech.h` are untouched.** Those two are exactly the domains deep does not own, and that is not a coincidence: the decisions there are `static`.

---

## 1. The hook table

Registration block: `ai/deep/deepai.c:1039–1137`.

### Owned — deep's own behaviour, no `dai_*` equivalent called

| Hook | Handler | Handler def | Notes |
|---|---|---|---|
| `game_free` | `dpai_game_free` | `deepai.c:116` | frees deep's site cache |
| `module_close` | `dpai_module_close` | `deepai.c:102` | site cache + private |
| `player_save` | `dpai_player_save` | `deepai.c:180` | `deep_state` namespace (§5) |
| `player_load` | `dpai_player_load` | `deepai.c:260` | §5 |
| `city_got` | `dpai_city_got` | `deepai.c:517` | logging only; **stock has no handler** |
| `city_lost` | `dpai_city_lost` | `deepai.c:533` | logging only |
| `settler_run` | `dpai_auto_settler_run` | `deepai.c:791` → `deepsettler.c:1465` | deep's own settler engine (W05/W02) |
| `restart_phase` | `dpai_restart_phase` | `deepai.c:838` | trivial, same as classic |

### Replaced — deep body derived from stock, stock still called *inside*

| Hook | Handler | Deep body | What is still stock inside |
|---|---|---|---|
| `first_activities` | `dpai_do_first_activities` `deepai.c:826` | `deepdefence.c:1921` | `dai_assess_danger_player` (:1924); `deep_manage_units` (`deepops.c:1884`) still calls `dai_airlift`, `dai_set_defenders`, and `dai_manage_unit`/`dai_manage_ferryboat` per unit |
| `last_activities` | `dpai_do_last_activities` `deepai.c:895` | `deepdefence.c:3909` | **`dai_clear_tech_wants`, `dai_manage_government`, `dai_adjust_policies`, `dai_manage_taxes`, `dai_manage_tech`, `dai_manage_spaceship` — verbatim stock order.** Only `dai_manage_cities` → `deep_manage_cities` (`deepdefence.c:3563`) and one added `deep_balance_treasury` (`:3816`) |

Compare `daihand.c:748` / `daihand.c:774` — the diff is exactly those two lines.

### Wrapped — forward plus deep-only side effect

| Hook | Handler | Addition |
|---|---|---|
| `player_alloc` | `deepai.c:124` | allocates `struct deep_plr` (embeds `ai_plr` first), sets identity stance, then `dai_data_init` |
| `phase_begin` | `deepai.c:452` | `dai_data_phase_begin` + `deep_stance_refresh/log` + `deep-dv` line |
| `unit_lost` | `dpai_ferry_lost` `deepai.c:685` | `deep-loss` census line, then forwards |
| `unit_turn_end` | `deepai.c:714` | `deep-op-stall` tripwire (unit held by a live op that ends with moves left), then forwards |
| `diplomacy_actions` | `deepai.c:846` | `dai_diplomacy_actions`, then a **purely social** retraction of a lapsed war countdown (`:864–889`). No want reads it, no state changes. |
| `player_save_relations` / `player_load_relations` / `city_save` / `city_load` / `unit_save` / `unit_load` | `:152,165,552,563,759,770` | forward with a `"deep"` / `"deep_"` secfile prefix (E00.2 lesson) |

### Forwarded untouched

`player_free` (:141), `gained_control` (:420), `split_by_civil_war` (:430), `created_by_civil_war` (:441), `phase_finished` (:482), `city_alloc` (:492), `city_free` (:502), `choose_building` (:574), `build_adv_prepare` (:584), `build_adv_init` (:594), `build_adv_adjust_want` (:604), **`gov_value` (:614)**, `units_ruleset_init/close` (:625,:635), `unit_alloc/free` (:645,:655), `unit_got` (:665), `unit_transformed` (:675), `unit_move` (:737), `unit_task` (:748), `settler_reset` (:781), `settler_cont` (:803), **`want_to_explore` (:815)**, **`treaty_evaluate` (:905)**, **`treaty_accepted` (:916)**, **`first_contact` (:927)**, **`incident` (:938)**, `log_fragment_city/unit` (:952,:962), **`consider_plr_dangerous` (:972)**, `consider_tile_dangerous` (:983), **`consider_wonder_city` (:994)**, `revolution_start` (:1004), `check_sanity` (:1014).

### Left NULL (both modules)

`game_start`, `map_alloc`, `map_ready`, `map_free`, `player_console`, `lost_control`, `city_created`, `city_destroyed`, `unit_created`, `unit_destroyed`, `unit_move_seen`, `refresh`, `tile_info`, `city_info`, `unit_info`. `tile_info`/`city_info`/`unit_info` are the natural seams for a W04 knowledge layer and are unclaimed.

### One non-vtable seam

`deepai.c:1026` registers `deep_worker_task_value` via `aw_set_task_value_hook` (`server/advisors/autoworkers.c:322,327,353`). This is the W30-style extension point in shared advisor code. It is a **global**, not per-ai-type — but `deep_worker_task_value` (`deepsettler.c:1829`) guards with `pplayer->ai != deep_ai_get_self()`, so human auto-workers and `ai/default` are unaffected. Correct, and worth keeping correct: the guard is the only thing making that hook rule-4-legal.

---

## 2. The delegation map

### A. Rates — taxes / science / luxury  *(stock, entirely)*

- `dai_manage_taxes` (`daihand.c:211`), called at `deepdefence.c:3917`.
- **Policy:** treats the whole empire as one city. Scans upward for the *minimum* tax rate giving a non-negative gold balance (allowing a small deficit if the treasury is fat), then the minimum science rate that avoids tech loss, then tests whether a luxury rate can buy celebration; picks by a fixed preference ladder (celebrate > balanced sci/treasury > balanced > best-effort). **No tech is ever priced.** Science is a residual, not a decision.
- **Matters: very high.** `deephorizon.c:441,443,446` *reads* `economic.tax/luxury/science` to value tiles; nothing in `ai/deep` ever writes them (verified by grep).
- **Visibility:** none needed. `dai_manage_taxes` is already exported. Deep can simply stop calling it. This is an **open, ownerless lead** — NEXT-SESSION §3 names it.

### B. Research direction  *(stock, entirely, minus two credits)*

- `dai_manage_tech` (`daitech.c:335`), `dai_select_tech` (`daitech.c:70`, **static**), `dai_tech_effect_values` (`daitech.c:251`, **static**), `dai_clear_tech_wants`.
- **Policy:** each city adds to `plr_data->tech_want[]`; `dai_select_tech` spreads each tech's want over its prerequisites *divided by step count*, argmax subject to a research-switch penalty; goal by summed-prereq-value / steps. Its own comment ("It still sometimes does freaky stuff like setting goal to Republic and learning Monarchy") is the honest admission.
- **Deep's contribution is two additive wants only:** the tech-wall cap-lifter credit (`deephorizon.c:602`) and the inherited `FERRY_TECH_WANT` (`deepsettler.c:812`). Ordering verified correct at `deepdefence.c:3912–3927`.
- **Visibility:** `dai_manage_tech` is public, so *replacing* needs nothing. *Reusing* the per-improvement valuation needs `daicity.c`'s `base_want` (`:1521`) and `want_tech_for_improvement_effect` (`:121`) — both still static; **E-B.3, unstarted**.

### C. Government and policies  *(stock, entirely)*

- `dai_manage_government` (`daihand.c:707`) ← `adv->goal.revolution` ← `adv_best_government` (`advdata.c:993`) ← the **`gov_value` hook**, which deep forwards to `dai_gov_value` (`daidata.c:524`).
- **Policy:** for every buildable government, temporarily *set* it, `auto_arrange_workers` **every city**, sum `dai_city_want` + effect values, subtract revolution-length penalty; argmax.
- **Matters: medium-high**, and *cheap to take over* — the seam is already a vtable hook deep merely forwards.
- **Interaction worth naming:** `dai_gov_value` calls `auto_arrange_workers`, which honours `pcity->cm_parameter` — which **deep now sets** (`deepcm.c:290–304`). Deep's CM parameters silently participate in stock's government evaluation. Not a bug; not designed either. *(inferred)*

### D. Diplomacy and war declaration  *(stock, entirely — the largest unowned block)*

- `dai_diplomacy_actions` (`daidiplomacy.c:1594`), forwarded at `deepai.c:851`.
- **The decision chain, all `static`:** `dai_war_desire` (`:797`) → `dai_go_to_war` (`:1341`) → `war_countdown` (`:1470`) → `dai_declare_war` (`:1308`). Plus `dai_share` (`:1248`), `suggest_tech_exchange` (`:1154`), `dai_goldequiv_clause` (`:289`) behind `dai_treaty_evaluate` (`:617`).
- **`dai_war_desire`'s actual policy:** `want` = Σ over every target city of `100 + 20·size + 8·shield surplus + 6·trade surplus + Σ build cost of every improvement` (×3 great wonders, ×2 small); `+100` per enemy settler; `−200` per own settler, `−150` per settler being built; `want −= |want| / max(cities − settlers, 1)`. `fear` = Σ enemy attack power + `gold/5000 × cities` + `100 ×` tech lead − half our attack power. Distance, reputation, `love` modifiers. If target leads space race: `WIN_CAPITAL`, return `BIG_NUMBER` un-amortized.
- **Every input is omniscient.** Deep reads none of it; touches `countdown` only to print a retraction.
- **Visibility:** taking over *only* war-target selection needs `dai_war_desire` and `war_countdown` exported — a clean, small E-B-style change. `daidiplomacy.h` currently has **zero** visibility edits.

### E. Unit micromanagement outside an operation  *(stock, with deep pre-emption)*

`deep_manage_units` (`deepops.c:1884`) diverges from stock in three ways: seeds `done` from `deep_op_holds()`; calls `deep_choose_trade_targets` first; routes Workers-flagged ferries to `dai_manage_ferryboat` (bug #33 fix). Everything else → `dai_manage_unit` (`daiunit.c:2742`):

| Branch | Stock function | Policy | Deep's coverage |
|---|---|---|---|
| Diplomat/Spy | `dai_manage_diplomat` | fixed unscaled constant ladder (Embassy 10000, StealTech 9000, Incite 7000…) | **none**; W35 (needs W04) |
| Worker/founder | `dai_manage_settler` | dispatches into `settler_run` hook | **deep owns via hook** |
| Caravan | `dai_manage_caravan` (static) | re-search when `AIUNIT_NONE`; `fc_rand(2)` between FTL levels | **destination pre-empted** by deep; execution stock |
| Barbarian leader | `dai_manage_barbarian_leader` | flee | none |
| Paratrooper | `dai_manage_paratrooper` | falls back to military | none |
| Ferry | `dai_manage_ferryboat` | two-sided flag protocol | **only lift-claimed boats** deep-driven |
| Air | `dai_manage_airunit` | target-first (`find_something_to_bomb`) | **none**; W32 unstarted |
| Military | `dai_manage_military` (`daiunit.c:2587`) | hunter → findjob → defend/attack → `find_something_to_kill` (fresh per-unit pf map, RC2) | partial: op members held/driven; sorties/tired/convoys/flee pre-empt; **rest stock** |

- **Visibility:** `find_something_to_kill`, `dai_military_rampage`, `dai_set_defenders`, `dai_airlift` already exported. `dai_manage_diplomat`, `dai_manage_airunit`, `dai_military_attack`, `dai_military_findjob` not; `daiair.c` and `daidiplomat.c` have **no** exports at all.

### F. Production wants deep still delegates

- **`domestic_advisor_choose_build`** (`daidomestic.c`), called at `deepdefence.c:3436,3439`: improvement wants (via `base_want` `:1521` and `adjust_improvement_wants_by_effects` `:1599`, both static), founder want, worker want, wonder-help, trade-route want. Deep post-adjusts `building_want[]` and replaces `founder_want`'s source, but base valuation and merge machinery are stock-derived.
- **Wonder city selection entirely stock**: `consider_wonder_city` and `build_adv_prepare` plain forwards; `adv->wonder_city` gates deep's own military choose (`deepdefence.c:2556`, `:3409`).
- Unit-type selection: all stock helpers (`dai_process_defender_want`, `dai_choose_attacker`, `kill_something_with`, etc.). Deep re-tiers/gates several, writes none.
- **Hunters wholly stock.**

### G. Danger model  *(shared)*

`assess_danger` (stock, exported) is called by `deep_military_choose_build` (`deepdefence.c:2271`). Deep adds fear terms and owns an independent survival model, but the base assessment — including boat-invasion blindness — is stock's.

---

## 3. Stock signals deep never reads

**Verified still unread:**

| Signal | Computed at | Read by | Deep |
|---|---|---|---|
| `adv->oceans[].size` | `advdata.c:313` | nothing (only `.threat`) | **reimplemented, not read** (`deep_idle_ferries_on`, comment at `deepdefence.c:2177`) |
| `ai_invasion` counters | `daiunit.c:1059,1073,1089` (enemy cities) | `daimilitary.c:1412,1626`, `daiair.c:151` | **never read**; still not computed for own cities |
| `adv->threats.invasions` | `advdata.c:303,346,353` | `daieffects.c:537,543` | **never read** |
| `enum winning_strategy` / `WIN_CAPITAL` | writes `daidata.c:88`, `daidiplomacy.c:880,1125,1128` | **one read: `daidiplomacy.c:1630` (`!= WIN_SPACE`)** | **never read.** `WIN_CAPITAL` computed and consumed by nothing — STATUS's claim confirmed. |
| `struct player_tile` / `vision_site` | server-maintained | nothing in either AI | **never read.** Deep's only knowledge check: `tile_get_known` at `deepdefence.c:478`, dead at every played difficulty. |
| `adv->goal.revolution` / `.govt` | `advdata.c:1085` | `dai_manage_government` only | never read by deep |
| `love[]`, `spacerace_leader` | diplomacy | stock diplomacy | never read by deep |

**Now STALE in 00-context §3 — table needs updating:**

| Row | Current state |
|---|---|
| `pcity->cm_parameter` "never set by the AI" | **Deep sets it** (`deepcm.c:170,290–304`, W09-lite) |
| `is_pos_dangerous` "assigned by nothing" | **Partially resolved**: `deep_civilian_pos_dangerous` assigned at `deepsettler.c:220` for civilians; military/ferry/`dai_gothere` still NULL |
| `TIMING_LOG` "compiled out" | **Partially resolved**: `aitiming` meson option (default false); deep instruments its own code |
| `create_city_virtual` "never for strategy simulation" | Still true for strategy; used for site scoring; `unit_virtual_create` heavily used by deep combat/odds |
| `player_save`/`player_load` "commented out" | **Resolved** — wired at `deepai.c:1049,1050` |
| `struct req_context` "needs one added field" | Untouched; deep uses read-only; E-B.4 open |

---

## 4. The biggest unowned decisions, ranked

(Prior: score gap is economic, not military — `STATUS.md:1483`.)

**1. Tax / science / luxury rates. Genuinely unplanned; highest value per line.**
Stock never prices a tech — science is a solvency residual. Deep already computes amortized value everywhere else and *reads* the rates while never setting them. **Seam: none needed** — replace one call at `deepdefence.c:3917`. NEXT-SESSION §3 names it ownerless. Cheapest large unowned lever in the tree.

**2. Research direction beyond the tech-wall credit. Unplanned as a workstream.**
`dai_select_tech` = greedy scalar, no plan. Stance-biased tech want is scheduled (E34.4), but the *selection algorithm* has no owner. **Seam: `dai_manage_tech` public; reusing valuation needs E-B.3.**

**3. War declaration / target selection (`dai_war_desire`). Deferred, not planned.**
W34 phase B deferred behind W04 — sound, uncontested. What IS unplanned and cheap: **`WIN_CAPITAL` is already inferred correctly and thrown away** — wiring it to anything is a small, honest win not requiring W04. **Seam: needs `dai_war_desire`/`war_countdown` exported.**

**4. Government choice. Unplanned, moderate value, cheapest seam in the tree.**
`gov_value` is a vtable hook deep forwards — ownership costs *zero* `ai/default` change. Stock sums `dai_city_want` at today's size — RC1 applied to the most structural decision. Horizon-aware valuation is machinery `deephorizon.c` already has. Governments change a handful of times per game, hence rank 4.

**5. Air units and airbases. Planned (W32), unstarted.** Total blind spot; late-game cells only.

**6. Diplomats/spies. Planned (W35), blocked on W04.** Unscaled constants (`daiactions.c:96`). Correctly blocked.

**7. Residual military tactics (`find_something_to_kill`).** E4 (aggregation swap) and E5 (sieges) remain open. Units outside ops still build per-unit pf maps. Ranked low: fixing attack coordination moved nothing measurable so far.

**8. Ferry internals for non-lift boats.** NEXT-SESSION §2: "Do not build more transport." **Do not take this on.**

**9. Wonder-city selection.** Fully forwarded, no owner, small but real.

---

## 5. Savegame / state audit

**Persisted** (`deepai.c:180`, `:260`), under `player%d.deep_state.*`, all defaulted on read:
- `dv_turns_logged`; `stance.{econ,mil,cult}` + four gate counters (load defaults to **identity, not zero**; re-resolved from live tuning at next `phase_begin` — a pin belongs to the run, not the position); `last_strike_city`/`last_strike_turn`; all 8 op slots (full member lists; unknown `kind` loads as ASSAULT).

**Deliberately recomputed, correctly:** `ops_reissue`, `colony_want`/`colony_turn`, `free_land_*`, `cm_parameter`, deepsite tile cache.

**Load-order handling correct and documented:** hook fires before units exist; member ids restored raw, pruned by per-turn ops pass; drive dispatch on `kind` so a LIFT restored into an army slot is handled.

**Things that should persist and don't:**
1. `war_countdown_seen[]` — explicitly not saved (`deepai.h:78–81`); one missed retraction per reload. Documented, defensible.
2. **Member `unit_ai` tasks for a marching op** — recovered only for `MARCH && ASSAULT` (`deepai.c:409–413`). A **LIFT in MARCH does not set `ops_reissue`**; survives only because the lift re-issues orders every turn. True today; undocumented dependency. *(inferred — worth a comment)*
3. `op->ferry` restored without validation; handled downstream (`lift-noboat` dissolve) but the raw-restore contract is undocumented.
4. No opponent memory persisted, because none exists (W04).

**Save-path note:** `player_save` fires for every AI type for every player, so `deep_state` keys are written for stock/baseline players too — by design (E00.2 lesson), inert.

## Two incidental notes

- `deeptuning.c:303–408` log line is arg-balanced (101/101, counted programmatically) but `horizon_multiplier_weight` never appears in it; labels after `hcont=` have drifted from their arguments' names. Cosmetic.
- **`STATUS.md:172–182` Tier B checkboxes stale**: E-B.2 (daisettler.h exports) is unchecked but has clearly landed; E-B.3 (`base_want`) genuinely has not.
