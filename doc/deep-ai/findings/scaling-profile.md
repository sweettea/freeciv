# Part 23 — The measured AI profile at scale

**Confirms Part 22 §1. Revises Part 17 §2.2 and §6.** Measured 2026-08-01/02 at
`f1b4f0cd39`, `ai/deep`, civ2civ3.

Part 17 §6 asked for exactly this before any further scaling work: *"Everything
above is a code-reading hypothesis; it needs confirming. My prediction:
`AIT_DANGER` and `AIT_CITY_BUILDINGS` dominate."* Half of that prediction is
right, and the half that is wrong is the half the workstream plan was built on.

---

## 0. What had to be fixed before anything could be measured

Both build directories in this tree were `optimization=0`. **No performance
number this project has ever taken was measured on an optimised build.** The AI
timing buckets were also unreadable: `TIMING_LOG` compiled out unless
`FREECIV_DEBUG` was set (which forces `-O0`), and the only way to print the
totals was a console command a headless benchmark game cannot issue, because
`--read` script commands all execute before the game starts.

`-Daitiming=true` now compiles the buckets in at full optimisation, and the
totals are dumped at the end of every turn beside the city and unit counts.
Everything below is from `-Dbuildtype=release -Daitiming=true`.

**Games were run one at a time.** Four concurrent games inflated each other's
*CPU* time — not wall time — by 60%, presumably cache and memory-bandwidth
contention on a 12600-tile map. An early version of the A/B below was measured
that way and read backwards. Never run these concurrently.

---

## 1. `assess_danger` is the whole problem

`scale-20p`, 20 players, 140x90, T120. AI CPU is ~88% of wall-clock turn time,
so this is the turn cost, not a fraction of it.

| turn | cities | units | AI sec/turn | AIT_DANGER | share | AIT_BUILDINGS |
|---|---|---|---|---|---|---|
| 20 | 226 | 367 | 5.50 | 3.05 | 55% | 0.074 |
| 40 | 236 | 657 | 9.80 | 7.27 | 74% | 0.063 |
| 60 | 243 | 937 | 13.53 | 10.64 | 79% | 0.049 |
| 80 | 250 | 1074 | 16.66 | 12.90 | 77% | 0.057 |
| 100 | 256 | 1255 | 20.13 | 15.79 | 79% | 0.071 |
| 120 | 256 | 1352 | 22.44 | 17.90 | **80%** | 0.057 |

`AIT_DANGER` wraps `assess_danger()` itself (`daimilitary.c:709`, not `:827` —
the tree has moved). Over the run it is **75.5% of all AI CPU, slope 2.49
against city count**.

The mechanism is visible in the table. Between T20 and T120 cities move
226 → 256 (+13%) while units move 367 → 1352 (3.7x), and danger cost moves
3.05 → 17.90 (5.9x). That is `O(own_cities x all_units)` with the city term
nearly pinned: the growth is essentially all in the unit term.

**Part 17 §3's spatial threat index is the epic that follows this one.** Not the
`base_want` collapse.

---

## 2. `base_want` is not a measurable cost, at any cell tried

Part 17 §2.2 predicted `O(own_cities^2 x player-range improvements)` would be a
dominant term. It is not:

| cell | players | cities | own_cities/player | AIT_BUILDINGS | slope |
|---|---|---|---|---|---|
| `scale-20p` T120 | 20 | 256 | ~13 | **0.5%** | 0.51 |
| `scale-20p-s6` T60 | 20 | 188 | ~9 | 1.3% | 0.71 |
| `scale-4p` T120 | 4 | 77 | ~19 | 2.0% | 0.97 |

`AIT_BUILDINGS` covers the path in question: `building_advisor()` runs inside it
(`deepdefence.c:3057`), and that calls the `build_adv_init` / `build_adv_adjust`
hooks, hence `dai_build_adv_adjust` → `adjust_improvement_wants_by_effects`
(`daicity.c:1599`) → `base_want` (`daicity.c:1521`). The bucket sees it.

Every measured slope is **≈1, not ≈2**. Doubling own_cities roughly doubles the
cost; it does not quadruple it.

### The honest limit on this

**No cell reached 100 cities per player**, which is the figure Part 17 §2.2
argues from. The most any player owned was ~19. A quadratic term with a small
coefficient is invisible at 19 and dominant at 100, so this does not *disprove*
the analysis — it disproves that the term matters at the scales reachable here,
and it removes the empirical basis for doing that work first.

The measured slope of ~1 is itself evidence against the model, though. If the
cost were `own_cities^2`, the slope would already read ~2 at any size; a
quadratic does not hide by being small. Candidate reasons the loop does not
behave as written: `building_wait` / `should_force_recalc` (`daicity.c:1944`)
throttle full recalculation, and `city_range_iterate` only walks the whole empire
for improvements whose widest requirement range is player-wide, which in civ2civ3
is a short list. Both are hypotheses — nobody has counted the calls.

---

## 3. The requirements mutex: −9.5% of all AI time, for deleting dead code

Part 22 §1 flagged `is_req_active()` (`common/requirements.c:6749`) as taking a
global mutex plus a linear list scan on every requirement evaluation, to look up
a `trdata` the function then never reads, and hedged: *"I'd treat 'is this a
measurable share' as a hypothesis rather than a conclusion — uncontended mutexes
are cheap."*

It is measurable. `scale-20p-s6`, seed 1, one binary apart, run alone and
sequentially, two independent pairs:

| pair | mutex | no mutex | delta |
|---|---|---|---|
| 1 | 280.5 s | 254.8 s | **−9.2%** |
| 2 | 279.6 s | 252.2 s | **−9.8%** |

Same-binary repeat spread was 0.3% and 1.0%, so the effect is about ten times
the noise. Wall clock moved with it, 6:03 → 5:24.

**State was bit-identical**: city and unit counts matched on all 60 turns of both
pairs. That is the acceptance criterion for a `common/` change — it has to be
provably behaviour-neutral, and here it is checkable rather than argued.

The per-bucket breakdown is the part worth keeping, because it is an independent
check on the attribution rather than a restatement of the headline:

| bucket | delta | what it is |
|---|---|---|
| Tech | −54.7% | requirement-heavy |
| Citizen arrange | −50.6% | requirement-heavy |
| Government | −50.3% | requirement-heavy |
| Buildings | −33.4% | requirement-heavy |
| **Danger** | **−1.2%** | geometry and pathfinding |

The saving lands precisely where requirement evaluation lives and leaves
`assess_danger` alone. Buckets that halve are ones dominated by `is_req_active`;
the one bucket that dominates total runtime barely notices. Both facts are what
the mechanism predicts.

Incidentally, `register_at_thread_exit_callback()` (`utility/fcthread.c:32`)
accepts **one** registration process-wide and logs an error on a second. The dead
requirements bookkeeping was holding that slot.

---

## 4. What this changes

Part 17 §6 and Part 22 §7 both order the work: mutex, profile, algorithmic fixes,
purity, memoization, threading. Steps 1 and 2 are done. Step 3 changes:

1. ~~Delete the `is_req_active` mutex.~~ Done — 9.5%.
2. ~~Profile.~~ Done — this document.
3. **Algorithmic fixes, `assess_danger` ONLY.** The spatial threat index, which
   turns `own_cities x all_units` into `all_units + own_cities x nearby_units`.
   At 80% of AI time and slope 2.5, this is the entire scaling problem.
4. **Do not do the `base_want` collapse for performance.** It is 0.5% of AI time
   at the largest cell measured. Part 17 §3 also argues it is a *better model* —
   separating "is this wonder worth building" from "where should it go" — and
   that argument stands on its own merits under W01/W03. It is not a scaling
   workstream. If it is ever revisited for cost, the first step is counting
   `base_want` calls at 100 cities per player, not rewriting the loop.
5. Purity (W26), memoization, threading (W20) — unchanged in order, and all still
   behind step 3.

Note what step 3 does *not* need: `assess_danger` reads shared state but the
threat index is a per-turn read-only structure built once. **The largest
available win needs neither purity nor threading.** That is the same shape as
Part 22 §7's closing point — almost none of this is threading work.

---

## 5. Method notes that cost time here

- **Never run profiling games concurrently.** +60% CPU-time inflation, and it
  inverted an A/B.
- **`startunits` is capped at 21 characters.** A longer value is rejected with
  one line in the log and the setting silently stays at the ruleset default of
  `cwsx` — one settler. A 24-settler cell ran to completion looking healthy and
  produced 31 cities across four players. Check city counts, not exit status.
- **`LOG_TIMERS` is defined unconditionally in `utility/timing.h`.** Anything
  gated on it is gated on nothing, and a per-turn dump added there would have
  changed the logs every existing benchmark reads.
- **One game per arm cannot resolve a 9% effect.** Two pairs plus two
  same-binary repeats is what made the number trustworthy; the same-binary
  repeats are what establish the noise floor, and they are cheap.
