Continue the Freeciv deep-AI work. Read CLAUDE.md, doc/deep-ai/00-context.md,
then doc/deep-ai/STATUS.md — start at "The expansion stall was settlers walking
to islands" and read forward through the final addendum. Branch
epic/w03-horizon-content.

## 0. FIRST, AND BEFORE ANY NEW CODE: re-run the head-to-head

**Six behaviour changes have landed since it was last taken.** The
+9.73 T80 margin is from `0f126c33b7`; since then: the idle-worker
fallback, Join City, the founder pool (twice), and both ferry fixes.
Individually they range from inert to +4.37 (t=1.84). Nobody has
measured the stack against `ai/default`.

    ./benchmark/run.sh --quick --config benchmark/configs/lategame-80.conf \
      --arms mixed_dc,mixed_cd --jobs 8 --out RESULTS --server PINNED

Take it against a pinned control on the same seeds and report the
**difference-in-differences**, not the raw margin — that is what turned
a t=1.55 raw result into t=2.09. Report the score margin **by turn**;
the endpoint hid the whole story on this cell once already.

If it has fallen, bisect: every change is its own commit.

## 1. THE EPIC: let workers cross water

`findings/idle-workers.md` §9 has the full diagnosis. Short version:
**workers cannot cross water at all.**
`worker_evaluate_improvements()` uses a plain land pf parameter, so an
overseas city's radius is enumerated and then every tile silently
discarded as unreachable; and the FIND_WORK branch has no ferry leg —
every `aiferry_*` call in `deepsettler.c` is on the BUILD_CITY mission
path. A colony is improved only by founders that happen to land there.

    lategame-80: 12% of cities have NO worker-capable unit within 8
    tiles; the tail runs to 24.

This is the biggest specified gap left, and it closes a loop: overseas
expansion measured as barely worth having partly *because* overseas
cities never get terrain work. Colonisation and the worker pipeline are
failing each other.

Three pieces, and the machinery already exists
(`pft_fill_amphibious_parameter`, `pft_amphibious`, and
`aiferry_goto_amphibious()` already carry settlers):

1. **See it** — when the land search finds nothing, re-run the
   enumeration amphibiously so other landmasses become
   reachable-with-a-boat rather than invisible.
2. **Reach it** — route the assignment through `aiferry_gobyboat()`
   instead of a bare walk.
3. **Discount it** — `worker_cross_continent_discount`, new tunable
   with a placeholder and a tuning pass, so this stays *backup*
   behaviour (human call: "a little bit, significantly
   down-prioritized") and workers do not queue for boats ahead of
   settlers.

It is also the only thing identified that would make
`deep_idle_fallback_work()` non-inert: "nothing to do here" versus "a
whole unimproved colony over there" is exactly what it cannot currently
see.

## 2. Join City target quality

Currently ranked by soonest arrival only. Human call — it should weigh:

- **granary presence** (a stocked granary carries the food forward
  rather than evaporating),
- **the citizens' value to that city** (a small city gains more per
  citizen than a large one),
- against **travel time and route risk**.

One weight, not four, if value is discounted by `amortize(value, turns)`
the way the site search already does, with route risk from the shared
`deep_settler_overnight_veto()`.

## 3. Tune the settler/boat block

**Not one of the seventeen parameters there has ever been tuned** —
all placeholders or inherited stock values (`findings/idle-workers.md`
§10). Highest-leverage first:

- `site_enough_result` (new, placeholder 250.0 = stock's compile-time
  `RESULT_IS_ENOUGH`). Above it phase 2 never runs, so this single
  number decides whether colonisation is evaluated at all.
- `founder_pool_per_site` (placeholder 0.06). Measured per-cell
  calibrations disagree 3.5x: 0.06 on `primary-120`, 0.207 on
  `lategame-80`. **Report per-cell values; do not average the spread.**
- `settler_guard_tolerance` (new, placeholder 1.0 = stock bar).
- `colony_lift_weight`, `overseas_fear`, `overseas_decay`,
  `site_travel_discount`, `site_stream_weight`.

## 4. The colony lift as an operation

`findings/settler-missions.md` §8. Home is `deepops.c`. Also where the
**settlement party** belongs — human call: keep the ferry cap at 1:1
for ordinary settlers, because a settler occupies a boat for a round
trip and a capacity multiplier would queue eight behind one Transport
and recreate the dock-parking bug. The exception is several settlers on
ONE boat making ONE low-risk trip, i.e. *the cap counts trips, not
settlers* — which needs the operation to exist.

## WHAT NOT TO REBUILD

- **Site scoring to change city spacing.** W02b falsified it and this
  session's negative control confirmed it: median founded city sits on
  `citymindist`, ~half are on the floor, land per city moved <1% across
  every variant.
- **A founder pool sized per city.** Superseded; in history at
  `5d422d0b44`. Worse than joining freely on every count.
- **A ferry capacity multiplier on the 1:1 cap.** See §4.
- Branch `shelf/refused-2026-08-03` holds the two ferry fixes as
  standalone commits; both are now cherry-picked onto the epic branch,
  so the shelf is history rather than a to-do.

## METHOD NOTES THAT EARNED THEIR PLACE

- **Two fixes repairing opposite ends of one broken protocol cannot be
  measured separately.** Both ferry fixes measured negative alone and
  +4.37 (t=1.84) together; each one's failure mode was the other one's
  absence. They were nearly discarded on individually-honest numbers.
- **Instrument before fixing, and build the diagnostic as a LADDER**
  that relaxes every candidate constraint, not the two you suspect. The
  settler stall's cause was a rung nobody had enumerated.
- **Check your denominators.** "12% of overseas settlers arrive"
  survived two rounds of analysis before two thirds of the denominator
  turned out to be the same settlers re-picking.
- **A pre-registration can pass its own stated criterion and still be
  wrong about the mechanism.** D2 did exactly that.

## BENCHMARK DISCIPLINE

- PIN the binary (freeciv-server + libfreeciv.so + tuning file to a
  scratch dir; RUNPATH is `$ORIGIN`) and pass `--server`. New pin
  directory per binary; never overwrite one a batch is using.
- **/tmp is tmpfs here and IS wiped between sessions.** Anything worth
  keeping goes in a commit, not the scratchpad.
- Long batches must use the harness's background mechanism; a
  `nohup ... &` inside a tool call is reaped and dies silently mid-run.
- NEVER edit `deep-best.ini` or a config while a batch is in flight.
- Filter non-benchmark players — barbarians make some games write 61 or
  64 rows for 60 seats.
- `meson test -C build` is the suite; `python3 benchmark/tune-audit.py`
  must print CLEAN (96/96/96/96 now).

## STATE

- deep-best.ini: the site block is NOT TUNED. `site_stream_weight` 1.0,
  `site_travel_discount` 0.9 (human-set), `founder_pool_per_site` 0.06,
  `site_enough_result` 250.0, `settler_guard_tolerance` 1.0 — all
  placeholders.
- `lategame-80` STAYS in the campaign (human call 2026-08-03). It is an
  island, tech-saturated regime no other cell tests — 60% of its
  settler targets are overseas — and it surfaced every bug this session
  found. The guard is to report per-cell tuned values and treat sharp
  disagreement as overfit evidence, not to drop the cell.
- Both campaign boxes are STOPPED, not terminated (a6 us-east-1
  i-0e9a257ef3b781349, b6 us-east-2 i-05162168cf393cb20 — DIFFERENT
  REGIONS). EBS caches preserved. NEVER terminate.
