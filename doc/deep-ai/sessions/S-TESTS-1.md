# S-TESTS-1 — a real test suite on Criterion

Brief for one Opus subagent session. One epic, 5–10 commits, then STOP.
Read `CLAUDE.md`, `doc/deep-ai/00-context.md`, the `test(` entries in
`meson.build` (the six existing L0 tests under `tests/`), and
`doc/deep-ai/workstreams/W37-pricing.md` ("how do we know the formula is
right" is what this suite is for).

## Goal

Replace the hand-rolled L0 tests with a Criterion suite and a headless
fixture, so that a new test costs one function and the interesting tests
(dominance over game objects, solver against reference, calibration later)
are cheap to write. The user's words: "a world where we could do a lot of
interesting work in tests without much work ourselves."

## Framework

Criterion (github.com/Snaipe/Criterion), already declared as a meson
subproject in `subprojects/criterion.wrap` (v2.5.0, wrap-git). It brings
its own subprojects (boxfort, debugbreak, klib, nanomsg, nanopb, libgit2,
libffi) as wraps, some through cmake. System packages that avoid
fallbacks: cmake, libgit2-devel, nanomsg-devel (the user installs these;
check with `rpm -q`). Configure with the subproject allowed to fall back:
`meson setup build-tests -Dbuildtype=debugoptimized -Dtests=enabled` once
you have added the option. Network at configure time is expected
(github.com, gitlab.freedesktop.org).

## What to build, in order

1. **meson option `tests`** (feature, default `auto`): when enabled or
   when Criterion resolves, build the suite; when disabled or absent,
   the build is unchanged. `criterion_dep = dependency('criterion',
   fallback: ['criterion', 'criterion_dep'], required: get_option('tests'))`.
   Keep the six existing `test()` entries working until step 3 removes
   them. One commit: "build: add the tests option and the Criterion dep".
2. **The fixture**, `tests/fixture.c` + `tests/fixture.h`: a Criterion
   suite-level setup that initialises the game the way the server does
   for a headless ruleset load (look at how `deepeffects_test.c` and
   `server/ruleset.c` / `fc_game_init()` do it, and at
   `benchmark/run-game.sh`'s data path: `FREECIV_DATA_PATH` must point at
   the source `data/`), loads a named ruleset (parameter: classic or
   civ2civ3), creates a small map (`map_init`/`map_allocate` with a
   generated or flat terrain of ~20x20), and helpers that make virtual
   players, cities (`create_city_virtual`) and units (`unit_virtual_create`)
   on chosen tiles with chosen types. Teardown frees everything. Report in
   the commit how long one setup takes; if it is over a second, cache the
   ruleset load across tests in a suite (Criterion's `init`/`fini` on the
   suite). Rule: the fixture uses only public server/common entry points;
   nothing is copied out of ai/default.
3. **Port the six L0 tests** (`deepodds`, `deepattrition`, `deepeffects`,
   `deeptuning`, `deepterra`, `deepnativity`) to Criterion `Test()`s, one
   commit, same assertions, then delete the old executables from
   meson.build. `meson test -C build-tests` must stay green.
4. **Dominance tests on real objects**, parameterised where natural
   (`ParameterizedTest`): a Musketeers beside an empty enemy city has
   positive capture EV (the pricing branch `epic/pricing-1`, worktree
   `.worktrees/perf`, has the formulas: `deep_capture_*`, the EV op test,
   the per-shield defender chooser; read them there, do not merge them);
   a Rifleman beats Alpine Troops per shield on classic; defence bonus of
   a site is monotone in terrain defence; the stance read clamps to
   [1 − span, 1 + span]; the cm solver's result equals the reference
   solver's on a set of virtual cities (cm.c's `FREECIV_CM_CHECK` oracle
   shows how the reference is invoked). If a function you need is static,
   say so and test through its public caller; do not make ai/default
   functions non-static for tests (rule 2's exception is for ai/deep's
   use, and it needs a neutrality proof).
5. **Harness niceties**: `meson test` runs Criterion with `--tap` so the
   runner reads per-test results; a `tests/README.md` of ten lines on how
   to add a test; a `scripts/check-tests.sh` that configures a fresh
   build-tests dir and runs the suite, for CI-by-hand.

Stop after 5 or when the budget is spent.

## Rules

- Do not touch `ai/baseline/` or `ai/default/`. Do not merge or touch
  `epic/pricing-1`; read it. Do not change `benchmark/` scripts.
- Work in a new worktree: `git -C /home/sweettea/freeciv worktree add
  .worktrees/tests -b epic/tests-1 epic/w03-horizon-content`. Use
  `git -C <abs path>` for every git command, one branch operation per
  Bash call. Build dir `build-tests` inside that worktree only.
- No games are needed for this epic; if you run any, `chrt -i 0 nice -n
  10`, ports 8130–8149, and never from a build dir.
- Commits: one thing each, ≤ ~400 lines, `test:`/`build:` area prefixes,
  bodies short and plain; code comments say what the code does.
- Update `doc/deep-ai/STATUS.md` with an S-TESTS-1 entry (what landed,
  suite size, setup time, how to run) and `CLAUDE.md`'s build/test
  section with the new commands. Commit on `epic/tests-1`, do not push,
  report back: commit list, test count, `meson test` summary line, what
  Criterion's subproject build needed, and anything in this brief that
  was wrong.
