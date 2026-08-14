# Part 10 — Exploration, threading, land creation, and CPU

Companion to Parts 1–9. Same tree: `freeciv/freeciv` @ `0896b34`.

---

## 1. Huts and exploration

Worse than under-prioritised. **The AI has no explorer production want at all**,
and it actively destroys explorers it happens to own.

### 1a. Nothing ever wants an explorer

Grep `ai/default/` for any build want referencing `L_EXPLORER` or exploration:
nothing. `military_advisor_choose_build()` and `domestic_advisor_choose_build()`
have wants for defenders, attackers, workers, settlers, caravans, trade-route
units, and ferries. Exploration isn't among them.

Exploration happens only as a fallback: units with `AIUNIT_NONE` may be switched
to auto-explore (`dai_switch_to_explore`, `daiunit.c:3579`). So the AI explores
with whatever it has spare, and never decides to explore.

### 1b. And it disbands them — `daicity.c:542`

```c
/* Disband explorers that are at home but don't serve a purpose.
 * FIXME: This is a hack, and should be removed once we
 * learn how to ferry explorers to new land. */
city_list_iterate(pplayer->cities, pcity) {
  unit_list_iterate_safe(ptile->units, punit) {
    if (unit_has_type_role(punit, L_EXPLORER)
        && pcity->id == punit->homecity
        && def_ai_city_data(pcity, ait)->urgency == 0) {
      unit_do_disband_trad(pplayer, punit, ACT_REQ_PLAYER);
```

An explorer that returns home is scrapped for shields. `doc/README.AI` lists the
consequence in its own bug list: *"The AI will also not build units to explore
new islands, leaving huts alone."*

### 1c. The hut bonus is a tiebreaker on tiles it can already see

`autoexplorer.c:199–204`:

```c
#define OWN_CITY_SCORE  (BEST_NORMAL_TILE + 1)
#define HUT_SCORE       (OWN_CITY_SCORE + 1)
```

A hut is worth `BEST_NORMAL_TILE + 2` — literally two points more than the best
possible ordinary tile. And the bonus only applies when
`map_is_known(ptile, pplayer) && unit_can_enter_hut(...)`: the AI beelines huts
it has **already seen**, and has no notion of exploring *toward* likely hut
country. There's a `FIXME` right above admitting hut desirability should be
ruleset-specifiable.

### 1d. What to change

1. **An explorer want**, scaled to expected return:
   ```c
   explore_want = huts_visible_unclaimed * HUT_TECH_VALUE
                + unknown_tiles_on_our_continent * DISCOVERY_VALUE
                + (contest_pressure * FRONTIER_VALUE)   /* Part 4 §1d */
   ```
   `HUT_TECH_VALUE` should read `game.server.huts` (density) and the ruleset's
   hut outcomes — in a hut-heavy game an early explorer is one of the best
   shield investments available, and in a hut-free game the want should be zero.
   Also check `H_LIMITEDHUTS` / `H_HUTS`, which gate what the AI gets and knows.

2. **Delete the disband hack**, or gate it on `explore_want == 0`.

3. **Make `HUT_SCORE` proportional**, not `+2`. Scale it by the expected value of
   a hut in shields/bulbs so it can outweigh several turns of travel — the same
   `amortize` treatment as everything else.

4. **Explore toward hut probability.** Without `H_HUTS` the AI can't see huts in
   unknown territory, but hut *density* is a known server setting, so unknown
   area itself carries expected hut value. That's a one-line addition to
   `explorer_desirable` and it turns exploration into a resource race rather
   than a map-filling chore.

5. **Ferry explorers** — the README bug. Once campaigns exist (Part 1 §3) this is
   the same mechanism with a trivial payload.

---

## 2. Threading

There's an existing framework, and knowing its actual shape tells you exactly
what's worth moving.

### 2a. What `ai/tex` really does

**One thread total** — `fc_thread_start(&exthrai.ait, texai_thread_start, ait)`
— shared by all AI players, not one per player. It maintains a **shadow copy of
the world** (`texaiworld.c`, plus per-player `units`/`cities` lists in
`struct texai_plr`), fed by a message queue:

`TILE_INFO`, `CITY_CREATED/CHANGED/DESTROYED`, `UNIT_CREATED/CHANGED/DESTROYED/MOVED`,
`MAP_ALLOC/FREE`, `FIRST_ACTIVITIES`, `PHASE_FINISHED`.

Results come back as requests: **`TEXAI_REQ_WORKER_TASK`** and
`TEXAI_REQ_TURN_DONE`. That's the entire outbound vocabulary.

And `TEXAI_DFUNC` is just `_func(ait, ...)` — a direct call on the **main
thread**. So in practice tex threads *one thing*: worker task selection. Everything
else forwards to `dai_*` synchronously.

### 2b. The rule that makes this tractable

The framework works for computation that is a **pure function of world state
producing a plan**. It does not work for anything that moves units, because each
move changes the state the next decision depends on.

That's a very good fit for what these documents propose, because almost all the
expensive new work is read-only analysis:

| Subsystem | Threadable? |
|---|---|
| Full-catchment site scoring (Part 1 §1) | **Yes** — pure over map + player state |
| Road-network MST planner (Part 1 §2) | **Yes** — runs once/turn/player |
| Chokepoint / articulation analysis (Part 5 §3) | **Yes** — pure over terrain |
| Doctrine probe simulation (Part 3 §2b) | **Yes** — virtual cities, no side effects |
| Migration score + intel (Part 2 §3) | **Yes** |
| Campaign target scoring (Part 1 §3) | **Yes** — the *planning* half |
| Unit movement, combat, rampage | **No** — sequential by nature |
| Production choice | Marginal — cheap, and needs fresh state |

The pattern to follow: extend the request enum with `REQ_ROAD_PLAN`,
`REQ_SITE_RANKING`, `REQ_CAMPAIGN_TARGET`, etc. The thread computes a plan from
the shadow world; the main thread validates and applies it. **Plans must be
re-validated on application** — the shadow world is a turn behind, so a site may
have been taken and a target may have been reinforced.

Practical notes: the shadow world costs memory proportional to map size, and the
message volume is proportional to unit moves — on a large map with many AIs
that's real overhead, so measure before assuming a win. If you want per-player
parallelism rather than tex's single thread, note that AI players share
`wld.map` and read each other's `player` structs, so you'd need the shadow-world
approach per thread or heavy locking. Tex chose one thread for a reason.

---

## 3. Creating land

**Mechanically supported and never considered.** `data/civ2civ3/terrain.ruleset`:

```
[terrain_ocean]      transform_result = "Grassland"
[terrain_lake]       transform_result = "Grassland"
[terrain_deep_ocean] transform_result = "no"
```

So Engineers can turn shallow ocean into grassland. Deep ocean can't be
transformed, which conveniently bounds the mechanic — you can bridge across
shelf, not across an abyss.

The AI's worker code *does* include `ACTRES_TRANSFORM_TERRAIN` in
`aw_actions_transform` (`autoworkers.c:100–106`), so terraforming is evaluated —
**but only for tiles inside a city's work radius** (`autoworkers.c:471–475`,
the same limitation as roads, Part 1 §2). Ocean tiles inside a radius get scored
purely on their post-transform tile yield.

So the two interesting uses are structurally unreachable:

- **Land bridging to a new continent** — the connecting tiles are, by definition,
  outside any city radius.
- **Expanding a city's usable catchment** by filling a bay so a *future* city
  site becomes viable — requires reasoning about a city that doesn't exist yet.

### What to add

Once the road planner exists (Part 1 §2), land bridging is the same object with
a different cost model. Add "transformable ocean" as traversable-at-a-cost in
the network planner:

```c
bridge_value = (turns_saved_per_crossing * expected_crossings * INFRA_WEIGHTING)
             + (new_continent_access ? land_available_beyond * CLAIM_WEIGHTING : 0)
             - (tiles_to_transform * transform_time * WORKER_TURN_COST);
```

Two cautions worth building in:

- **A land bridge is bidirectional.** It's also an invasion route *into* your
  continent, which previously required boats. Under the chokepoint analysis
  (Part 5 §3) a bridge you create is a chokepoint you now must garrison. Price
  that in, and prefer bridges you can fortify.
- **Terraforming is slow.** Ocean transform times are long; a bridge is a
  multi-worker, multi-decade project. It belongs to `ECON_CONSOLIDATE`/`ENRICH`
  doctrines with spare worker capacity, never to `LANDGRAB`.

---

## 4. Boat bridges

Good idea, and I think you're right about the CPU argument specifically.

**Legality:** the `Transport Embark` enabler in `civ2civ3/actions.ruleset`
requires only `MinMoveFrags 1` plus non-hostile relations with the transport
owner. There is no requirement that the actor be on land or not already
transported, so a land unit aboard transport A hopping to adjacent transport B
should be legal. I'd **verify this empirically before building on it** — there
may be hardcoded requirements in `actions.c` beyond the ruleset enabler that I
haven't traced.

**Why the CPU argument holds.** Compare against the ferry system, which
`README.AI` describes as *"better described by statistical mechanics than by
logic"* and where *"each turn both boats and passengers forget all about prior
arrangements."* Every passenger calls `aiferry_find_boat()`
(`daiferry.c:497`) — a fresh PF map — **every turn**, and every boat runs its
own search. That's O(passengers × PF) per turn, repeated indefinitely because
nothing persists.

A static bridge replaces all of it with ordinary land pathfinding across a fixed
corridor. Build cost is paid once; the rendezvous problem disappears entirely.
That's a genuine algorithmic improvement, not just a tactical one.

**What it needs:**

1. A `bridge_plan` — same structure as the road plan, over ocean tiles, with
   transports as the "extra" being placed. Sentry the boats and mark them
   `AIUNIT_NONE`-immune so the rest of the AI doesn't repurpose them.
2. Pathfinding that treats a friendly sentried transport as traversable. The PF
   layer already handles transports via `cargo_depth` and move scopes, so the
   `get_MC`/move-scope machinery is mostly there — this is the part to prototype
   first, since if PF won't route over a boat chain the rest is moot.
3. A defence model. **A boat bridge is fragile**: one enemy warship sinks a
   segment and everything on it. Only build where you have naval control or the
   crossing is short, and treat a broken segment as a campaign-abort condition
   (Part 1 §3).
4. Don't build one where a *land* bridge (§3) is cheaper — the two planners
   should share a cost comparison.

---

## 5. Other CPU savings

### 5a. Use the profiler that's already there

`server/srv_log.h:41–65` defines ~25 timing buckets — `AIT_MOVEMAP`,
`AIT_SETTLERS`, `AIT_WORKERS`, `AIT_CITIES`, `AIT_DANGER`, `AIT_FSTK`,
`AIT_EXPLORER`, `AIT_CITY_MILITARY`, `AIT_CITY_TERRAIN`, `AIT_CITY_SETTLERS`,
and more — with `TIMING_LOG(bucket, TIMER_START/STOP)` calls already scattered
through the AI. It compiles to nothing unless enabled.

Turn it on before optimising anything. My guess from reading the code is that
`AIT_FSTK` (`find_something_to_kill`, ~300 lines building a PF map **per unit**
and iterating every enemy city) and `AIT_SETTLERS` (`cityresult_fill`, flagged
in-source as `/* Burn CPU, burn! */`) dominate — but guessing is exactly what
the buckets exist to replace.

### 5b. The single biggest structural win: hoist per-unit PF maps

`find_something_to_kill()` builds a fresh `pf_map` for every attacking unit,
every turn, and re-derives the same enemy-city rankings each time. The target
*ranking* is nearly identical across units of the same type and continent.

With the campaign object (Part 1 §3), target selection happens **once per player
per turn** and units consult the plan. That's not just better play — it's an
O(units) → O(1) reduction on the most expensive function in the AI. The
architectural change and the performance change are the same change.

### 5c. Recalculation cadence is already a knob, and it's crude

`daicity.c:72`:

```c
/* TODO: AI_CITY_RECALC_SPEED should be configurable to ai difficulty. */
#define AI_CITY_RECALC_SPEED 5
```

used as `founder_turn = turn + fc_rand(5) + 5`, with this admission at line 956:

```c
/* This means AI is not very opportunistic if there happens to open up
 * spot for a new city. */
```

So settler evaluation runs every 5–10 turns per city, and the AI is explicitly
blind to opportunities in between. Better than a fixed period: **event-driven
invalidation.** Recompute a city's founder want when something actually changed
nearby — a city founded or lost within N tiles, a border shift, a war
declaration. Cheaper *and* more responsive, which is the rare optimisation that
improves play.

The same applies to `building_turn`/`building_wait`.

### 5d. Extend the tile cache rather than bypassing it

`tdc_plr_get`/`tdc_plr_set` (`daisettler.c:514–560`) already cache per-tile
output with a turn stamp. Full-catchment scoring (Part 1 §1c) multiplies the
lookups but *not* the distinct tiles, so a properly warmed cache absorbs most of
it. Make sure the cache key includes the government you're evaluating under
(`adv->goal.govt.gov` is swapped in at `daisettler.c:292`) or you'll serve stale
values across a revolution.

### 5e. Cheap wins

- **Early-out on hopeless targets.** `find_something_to_kill` evaluates every
  enemy city on the map before discarding unreachable ones. Filter by continent
  and rough distance first.
- **Cache `dai_migration_score`** — the server already does exactly this with
  `mgr_score_calc_turn` (`cityturn.c:4099`). Mirror the pattern.
- **Compute chokepoints once per map**, not per turn. Terrain rarely changes;
  invalidate on transform/global warming.

---

## 6. Free choice: persistence and reproducibility

The thing that will bite hardest and that nobody plans for.

### 6a. Your new state must be saved, and one hook isn't wired

Every structure across Parts 1–9 — campaign, coalition ledger, road plan,
doctrine vector, migration intel, war ledger, `called_to_arms_against` — is
long-lived planning state. If it isn't persisted, **every save/load resets your
AI's strategy**, which is both a bug and a nightmare for debugging, since your
batch-test reruns won't reproduce.

The vtable has `player_save`/`player_load`, and `dai_player_save()` exists in
`daiplayer.c:65`. But in `ai/classic/classicai.c:628–629`:

```c
/* ai->funcs.player_save = NULL; */
/* ai->funcs.player_load = NULL; */
ai->funcs.player_save_relations = cai_player_save_relations;
```

**The per-player save hook is commented out.** Only the per-player-*pair*
relations blob is wired (spam, countdown, war_reason, ally_patience,
asked_about_*). So `struct ai_plr` — including `tech_want[]` — isn't saved at
all today. Wire `player_save`/`player_load` in your module from day one; adding
it later means retrofitting every struct.

Follow the existing convention: `secfile_insert_int` with named keys and
`secfile_lookup_int_default` on load (`daiplayer.c:87–99`, `daicity.c:1201`).
Missing keys default cleanly, so old saves load into a new AI without special
handling — which also means you can add fields incrementally.

**Design implication:** prefer plan state that is *cheap to recompute* over
state that must be exactly restored. A road plan can be regenerated from the map
in one turn; a campaign's `committed_attack` cannot be re-derived from unit
positions. Save the latter, recompute the former, and you cut the persistence
surface substantially.

### 6b. Reproducibility for your test harness

Part 1 §5 recommends 50+ headless games per config. That only works if reruns
are deterministic:

- **Seed everything.** `fc_rand()` is used all through the AI —
  `AI_CITY_RECALC_SPEED` jitter, `fc_rand(5) == 1` nag gates, `spam` intervals.
  Fix the seed per run and record it.
- **Beware threading.** If you take §2's threading, message ordering becomes a
  nondeterminism source. Keep a single-threaded mode for testing and only run
  threaded for play, or you'll chase phantom regressions.
- **Log the decision vector each turn** — doctrine axes, campaign target,
  budget split, coalition role. With this many interacting subsystems, a
  post-hoc log is the only way to answer "why did it do that on turn 140."
- **Gate everything behind `H_EXPERIMENTAL`** (`ai/handicaps.h`) so you can run
  new-AI against classic in the same game with the same map and diff the score
  curves directly. That's the cleanest A/B you'll get.
