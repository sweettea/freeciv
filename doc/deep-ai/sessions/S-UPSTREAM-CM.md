# S-UPSTREAM-CM — port the city-governor speedup to upstream freeciv

Brief for one Opus subagent session. Deliverable: a patch series on top
of freeciv/freeciv `main`, ready for the user to attach to a Redmine
Feature ticket, plus the numbers that justify it. Not a change to this
project's branches.

## What is being ported

Perf epic 4 (STATUS S-PERF4), four commits on `epic/w03-horizon-content`
in `common/aicore/cm.c`:

- `3b3707b239` common: hoist per-search invariants out of the cm bound
- `1616a95d78` common: memoise city waste within a cm search
  (adds `cm_city_production()`, a copy of `set_city_production()` whose
  two `city_waste()` calls read a memo keyed by total; search order and
  results unchanged)
- `d2612965b3` common: run the cm solver beside its reference on request
  (the oracle: `FREECIV_CM_CHECK=1` recomputes every result and every
  bound with the unoptimised code and logs mismatches)
- `fa4fa4aa1c` common: count cm solver work per turn (`FREECIV_CM_STATS`,
  `FREECIV_CM_BENCH`)

Measured here: −70% solver instructions, 3.3x per query, byte-identical
results on 24 races + Europe + late game + spot checks, 0 mismatches in
15,199 result and 343,682 bound checks. The governor is also the client's
city governor, so human players feel it too.

`common/aicore/cm.c` on upstream main 761bdd5a2f differs from our base
0896b34 by one commit (6 deletions), so this is close to a cherry-pick.

## Shape upstream wants (learned from #2439/#2440 and the maintainer's habits)

- One ticket, a short series: (1) the hoist, (2) the waste memo, (3) the
  oracle. Drop the stats/bench commit, or fold a minimal counter into the
  oracle patch only if it costs nothing when disabled; env-var knobs are
  not upstream style.
- The oracle goes under a compile-time define that is OFF by default and
  is NOT `CM_DEBUG`: upstream's cm.c has `#define CM_DEBUG` enabled at
  line 88 in every build. Add `/* #define CM_CHECK_REFERENCE */` beside it
  (the `PF_REVERSE_DEBUG` pattern from #2440) and build the oracle under
  that.
- Commit messages: short, plain, mechanism then change, no "See RM" line,
  no AI-assistance boilerplate, author and Signed-off-by
  `Sweet Tea Dorminy <sweettea-kernel@dorminy.me>`. Code comments say what
  the code does now, never what it used to do. Upstream C style:
  `nullptr`, 2-space indent, 80 columns, braces on every if.
- Each patch must build alone, in order, on current `upstream/main`.

## Where to work

- Make a new worktree: `git -C /home/sweettea/freeciv worktree add
  .worktrees/upstream-cm -b upstream/cm-governor-speed upstream/main`
  (run `git fetch upstream main` first; the fork remote is `fork`, the
  upstream remote is `upstream`). Use `git -C <abs path>` for every git
  command; never rely on cd for branch operations; one branch operation
  per Bash call.
- Build: `meson setup build -Dbuildtype=debugoptimized` in the worktree,
  then `ninja -C build`. Pin before any timing or identity game: copy
  `build/freeciv-server` and `build/libfreeciv.so` into
  `.worktrees/upstream-cm/.pin-main/` (plain main) and `.pin-cm/` (your
  series), and `.pin-cm-check/` (your series built with
  `-Dc_args=-DCM_CHECK_REFERENCE`, use a second build dir
  `build-check`). Ports 8100–8129. Everything at `chrt -i 0 nice -n 10`,
  at most 6 games at once; the laptop is in use. Batches via Bash
  `run_in_background: true`; watch files, never sleep-loop in the
  foreground.
- The cherry-picks: `git -C .worktrees/upstream-cm cherry-pick -n <sha>`
  from the main repo's objects (same .git), then edit into upstream shape
  and commit fresh. Expect the oracle commit to need rework (env var →
  define).

## Verification (paste the numbers)

1. Identity: 24 stock-AI autogames on `.pin-main` vs `.pin-cm`: civ2civ3
   and classic, 8 players, 150 turns, seeds 1–12 each (`set gameseed N`,
   `set mapseed N`, `set timeout -1`, `set endturn 150`, `set scorelog
   enabled`, hard). Scorelog data lines identical minus the `id` line.
   Also the Europe T50 and T250 saves in
   `/home/sweettea/freeciv/.worktrees/upstream-pf/attach/` played 10 turns
   on each pin: identical.
2. Oracle: the same 24 games plus the two Europe saves on
   `.pin-cm-check`: 0 `cm-check mismatch` lines (name the log line in the
   patch), and the count of checks performed.
3. Speed: on the Europe T250 save, T250→T260 CPU seconds (`/usr/bin/time`)
   for `.pin-main` vs `.pin-cm`, two runs each on a quiet-ish box; plus
   `cm` share before/after if you take a callgrind of 2 turns
   (`valgrind --tool=callgrind` is installed; use `--toggle-collect` or
   just report inclusive cost of `cm_query_result`).
4. `meson test -C build` in the worktree passes.

## Deliverables

- Branch `upstream/cm-governor-speed` pushed to `fork`.
- Patches via `git format-patch upstream/main -o
  /home/sweettea/freeciv/.worktrees/upstream-cm/attach/`.
- A file `attach/TICKET.md` with: a Feature ticket text in the user's
  register (short, plain, mechanism → change → numbers; see
  `doc/deep-ai/sessions/S-PRICING-1.md`'s neighbour tickets #2453–#2457
  on redmine.freeciv.org for the voice), and a one-paragraph first note
  describing the verification the way #2440's first note does.
- Report back: commit list, identity verdict, oracle counts, speed table,
  anything in cm.c on upstream main that did not match this brief.

Do not touch anything outside `.worktrees/upstream-cm`. Do not push
anything but that branch. Stop when the deliverables exist.
