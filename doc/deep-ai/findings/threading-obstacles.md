# Part 22 — Threading obstacles: a server-wide survey

**Extends Part 19.** Baseline: `freeciv/freeciv` @ `0896b34`.

Part 19 covered `ai/` and `server/advisors/` — ten sites. This is everything
underneath them.

---

## 1. The finding that matters most

`common/requirements.c:6749–6784`, `is_req_active()` — **the hottest function in
the entire evaluation path**:

```c
fc_thread_id self = fc_thread_self();
struct thr_req_data *trdata = nullptr;

fc_mutex_allocate(&trmutex);
thr_req_data_list_iterate(trdatas, data) {      /* linear scan of a list */
  if (fc_threads_equal(self, data->thr_id)) {
    trdata = data;
  }
} thr_req_data_list_iterate_end;

if (trdata == nullptr) {
  trdata = fc_malloc(sizeof(struct thr_req_data));
  trdata->thr_id = self;
  thr_req_data_list_append(trdatas, trdata);
}
fc_mutex_release(&trmutex);

eval = tri_req_present(nmap, context, other_context, req);   /* trdata unused */
```

Three things at once:

1. **A global mutex on every requirement evaluation.** This is the single worst
   possible place for one. Any threading serialises here completely — you would
   get zero speedup and pay contention on top.
2. **A linear list scan** inside the lock, O(threads) per call.
3. **`trdata` is never read.** Grep it: assigned at 6762 or 6767–6769, then the
   function proceeds to `tri_req_present()` without touching it. The struct holds
   only `thr_id` — it's scaffolding for per-thread requirement state that was
   never populated.

So on the current single-threaded server this is **pure overhead on the hottest
function in the game.** Given Part 17's `O(cities² × wonders)` in `base_want`,
each evaluating many requirements, this fires tens of millions of times per turn
in a 20-player game.

I'd treat "is this a measurable share of the multi-minute turns" as a hypothesis
rather than a conclusion — uncontended mutexes are cheap and the list is short
today. But it is dead weight, it is on the hottest path, and it is *the* blocker
for any threading design. Profile it first; it may be the cheapest large win
available.

**Fix:** either delete the block (nothing uses it), or convert it to real
thread-local storage (`__thread` / `_Thread_local`) with no lock. Either way the
mutex has to go before any threading work is meaningful.

---

## 2. Full inventory

### Blocking — must fix

| Site | What | Why it blocks |
|---|---|---|
| `common/requirements.c:6756–6771` | Global mutex + list scan per `is_req_active()` | Total serialisation of evaluation |
| `common/aicore/cm.c:915–916` | `static Output_type_id compare_key;`<br>`static double compare_key_trade_bonus;` | `qsort` comparator context. **Directly blocks per-city parallel CM** |
| `common/aicore/citymap.c:53` | `static int *citymap` | Shared settler reservation (Part 19 §1) |
| `utility/rand.c:85` | `static RANDOM_STATE rand_state` | Race + determinism loss |
| `server/srv_log.c:44–45` | `static struct timer *aitimer[AIT_LAST][2];`<br>`static int recursion[AIT_LAST];` | Timing buckets corrupt under threads — **the profiler you need to justify threading isn't itself thread-safe** |
| `server/citytools.c:98, 101` | `arrange_workers_queue`, `send_city_suppressed` | Global deferred-work queue + global suppression flag |
| `server/cityturn.c:88, 94` | `city_refresh_queue`, `uk_rem_gold` | Global deferred-work queues |
| `server/maphand.c:63` | `send_tile_suppressed` | Global suppression flag |

The suppression flags are a subtle one: they're a "batch up sends, flush later"
toggle. Two threads flipping them race, and the effect is *dropped network
updates*, which would show up as client desync rather than a crash.

### Safe — verified read-only after initialisation

| Site | Why it's fine |
|---|---|
| `common/city.c:60–67` — `city_map_index`, `city_map_xy`, `city_map_numtiles` | Built once at ruleset load, read-only thereafter |
| `common/map.c:88–89` — `dir_cardinality`, `dir_validity` | Init-once |
| `common/improvement.c:41` — `improvement_types[B_LAST]` | Ruleset data, read-only after load |
| `common/effects.c:42` — `initialized` | Boolean flag |
| `common/movement.c:973` — `move_points_denomlen` | Init-once |

### Needs checking

- `common/city.c:1828–1829` — `caravan_helped_impr`, `caravan_helped_utype`.
  Determine whether they're populated once at ruleset load (safe) or lazily on
  first use (a benign-looking race that produces wrong caravan behaviour).
- **Static return buffers** — `common/city.c:372` (`citylog_map_line`),
  `common/actions.c`, `common/calendar.c`, `common/featured_text.c` and others.
  Mostly display paths, so the AI touches them only via logging. `CITY_LOG` and
  `UNIT_LOG` gate on `log_do_output_for_level()`, so they're free in release
  builds — but **debug and AI-test builds would corrupt logs under threads**,
  which is exactly when you'd be debugging threading.

### Not a problem

Only **3** `notify_player` / `lsend_packet` call sites exist in
`ai/default/*.c`. Packet emission from AI code is rare and belongs in the serial
apply phase anyway.

---

## 3. One good surprise

`utility/rand.c` already exposes `fc_rand_state()` and `fc_rand_set_state()`,
because savegames need to persist RNG state. So `RANDOM_STATE` is already a
copyable value type — making it per-player is dropping one into `struct player`
and threading it through, not designing a new abstraction.

Same shape as Part 19's `req_context` finding: the extraction work is mostly
already done for an unrelated reason.

---

## 4. More than one thread per AI — yes, and it's the better decomposition

Part 17 established that at 2000 cities, per-city is a far better parallel unit
than per-player: 2000 tasks instead of 20, and a 100-city empire and a 3-city
rump stop being treated as equal work.

The important structural point: **per-city parallelism has the same prerequisite
as per-player parallelism.** Cities within a player share `pplayer`, `adv_data`
and `ai_plr` — so per-city threading needs those read-only during evaluation,
which is exactly what Part 19's purity work delivers. Once you've paid for
per-player, per-city is nearly free and strictly better.

### Stage-by-stage

| Stage | Parallel unit | Blockers beyond Part 19 |
|---|---|---|
| **City evaluation** — building want, danger, CM | **per city** | cm.c comparator globals; `arrange_workers_queue` |
| **Site evaluation** — settler candidates | **per candidate tile** | `citymap` |
| **Unit target scoring** | **per unit** | `invasion` counter accumulation (a reduction — needs per-thread partials, then merge) |
| **Empire planning** — doctrine, roads, campaigns | serial, or per-plan | needs a consistent snapshot |
| **Unit movement** | **serial** | inherently sequential; each move changes the next decision |
| **Production commit, packet emission** | **serial** | ordering must be deterministic |

So the shape is a standard **fan-out / fan-in**: parallel per-city and per-unit
*evaluation*, serial empire *decision*, serial *execution* in fixed order.
Determinism holds because the order of **writes** never varies, regardless of
what order the reads completed in.

The `invasion` counters are worth calling out — `reinforcements_cost_and_value()`
accumulates into `city_data->invasion.attack/occupy` across units. That's a
reduction, not a race to avoid: give each thread private partials and merge in
the serial phase. Standard, but it has to be recognised as a reduction rather
than patched with a lock, or you serialise the thing you were parallelising.

---

## 5. Revised effort

Part 19 estimated ~1.5 weeks for the AI-side purity work. This survey adds:

| Task | Estimate |
|---|---|
| Remove or replace the `is_req_active` mutex with real TLS | **½ day** — and possibly a large single-threaded win |
| Pass CM comparator context through `qsort_r` or a wrapper struct | 1 day |
| Per-player RNG (state type already exists) | ½ day |
| Per-thread timing buckets | ½ day |
| Per-thread or locked deferred-work queues | 1–2 days |
| Audit and fix lazily-initialised caches | 1 day |
| Thread-safe logging (or assert single-threaded logging) | 1 day |
| **Additional** | **~1 week** |

**~2.5 weeks total** for a server where per-city parallel evaluation is possible.
That's still small relative to the multi-minute turns it addresses, and three of
these items — the requirements mutex, the CM comparator, the RNG — are worth
doing on their own merits regardless of whether you ever start a second thread.

## 6. Commits to add to W26

```
common: remove the unused per-thread requirement data lookup
common: pass comparator context explicitly in city management sorting
utility: give each player an independent random state
server: make AI timing buckets per-thread
server: make city refresh and worker arrange queues thread-local
server: audit lazily initialised caches in common for init-once safety
server: accumulate invasion counters as per-thread partials merged serially
test: assert timing buckets and RNG streams are independent per player
```

---

## 7. The order I'd actually take

1. **Delete the `is_req_active` mutex.** Half a day, no design risk, removes dead
   work from the hottest function in the game, and it's the precondition for
   everything else. Do this before profiling, so the profile isn't measuring it.
2. **Profile.** `TIMING_LOG` on the 20-player case — after making the buckets
   per-thread, or the numbers lie the moment you thread anything.
3. **Part 17's algorithmic fixes.** The `O(cities²)` collapse in `base_want` and
   the spatial threat index. Probably the largest wins, and they need no threading.
4. **Part 19 + §5 purity work.** ~2.5 weeks, three items independently worthwhile.
5. **Memoization**, now that evaluation is pure.
6. **Then thread**, per-city fan-out — which by that point is a small change,
   because everything hard was done in step 4.

The through-line: **almost none of this is threading work.** It's removing dead
locks, fixing quadratic loops, and making evaluation pure — each of which pays
off single-threaded. Threading is the last step and the easiest one.
