# Unit tests

The unit test suite is one [Criterion](https://github.com/Snaipe/Criterion)
executable, `freeciv-tests`, built from `tests/*_test.c` and the
headless game fixture in `tests/fixture.[ch]`. It links the server,
the advisors and the AI modules the way `freeciv-server` does, so a
test can call into `common/`, `server/`, `server/advisors/` and
`ai/default/` directly.

## Configure and build

The suite is controlled by the meson feature option `tests`
(default `auto`). It needs the server (`-Dserver=enabled`, the
default). Criterion is taken from the system if installed; otherwise
meson fetches v2.5.0 from github.com as a subproject on the first
configure (`subprojects/criterion.wrap`).

    meson setup build-tests -Dtests=enabled -Dbuildtype=debugoptimized \
      -Dclients=[] -Dfcmp=[] -Dtools=[] -Daudio=none -Dnls=false
    ninja -C build-tests

`-Dtests=enabled` makes a missing Criterion an error; with `auto` the
suite is quietly left out. `scripts/check-tests.sh [build-dir]` does
a fresh configure, build and run in one go.

## Run

All tests, with one result per test reported to meson as TAP:

    meson test -C build-tests --print-errorlogs

One suite, or one test, run directly:

    build-tests/freeciv-tests --verbose --filter 'bodyguard/*'
    build-tests/freeciv-tests --verbose --filter 'fixture/classic'
    build-tests/freeciv-tests --list

When run directly the fixture reads rulesets from the source tree's
`data/` unless `FREECIV_DATA_PATH` is set. Each test runs in its own
process, in parallel; `-j1` runs them one at a time, and `--debug`
waits for a debugger on each.

Criterion talks to its test processes over a socket in `/tmp`, so the
suite needs a writable `/tmp`.

## Add a test

A test is one function in a `tests/*_test.c` file. A new file is also
listed in the `freeciv-tests` sources in the top-level `meson.build`.

    TestSuite(mysuite, .init = fx_init_classic, .fini = fx_fini);

    Test(mysuite, armor_threatens_city)
    {
      struct ai_type *ait = ai_type_by_name(default_ai_type_name());
      struct player *us = fx_ai_player(nullptr);
      struct player *them = fx_ai_player(nullptr);
      struct city *pcity = fx_city(us, 5, 5, "Home");

      fx_war(us, them);
      fx_unit(them, "Armor", 8, 5, 0);
      fx_show_all(us);
      fx_phase_begin();
      (void) assess_danger(ait, &(wld.map), pcity, nullptr);
      cr_expect_gt(def_ai_city_data(pcity, ait)->danger, 0);
    }

(`assess_danger()` is static; see below for how a test reaches it.)

`fx_init_classic`, `fx_init_civ2civ3` and `fx_init_sandbox` load
those rulesets; `fx_init("name")` loads any ruleset under `data/`.
The map is 20x20, flat, non-wrapping Grassland, with native (x, y)
equal to map (x, y). The helpers, all in `tests/fixture.h`:

- `fx_player()` makes a human-controlled player, `fx_ai_player(name)`
  an AI-controlled one of AI type `name` (`nullptr` for the default,
  "classic"), with the AI's per-player data allocated.
- `fx_city()` and `fx_unit()` place virtual cities and units,
  registered as real ones are, with the AI's and advisors' per-city
  and per-unit data. `fx_set_terrain()` changes a tile.
- `fx_diplstate()` and `fx_war()` set diplomatic states, `fx_tech()`
  makes a tech known, `fx_government()` changes government.
- `fx_show_all()` shows a player the whole map.
- `fx_phase_begin()` starts a phase as the server does: advisor data
  and each AI's phase data. Call it after placing things and before
  calling AI or advisor code that reads them.

Each test is a fresh process with a fresh fixture, so a test may
change any global state (settings in `game.server`, a unit's hp, ...).
Setup costs about a second per test, nearly all of it the ruleset
load. A failed `fc_assert()` raises SIGABRT and fails the test.

For one test over many inputs use `ParameterizedTest`, with a
parameter struct holding names, not pointers (it is copied into each
test process), and call `fx_init()` in the test body.

A test that pins a known bug states what the code does now, and
says so: `.signal = SIGSEGV` for a crash (see `bodyguard_test.c`),
`.signal = SIGABRT` for a failed assertion.

## Testing a static function

A test of `static` functions in one source file includes that file:

    /* ai/default */
    #include "daimilitary.c"

and calls them by name (see `daimilitary_test.c`). Production sources
are not changed. The included copy defines every global symbol of
that object file, so the linker never pulls the library's copy out of
the static `libfc_ai.a` or `libfc_server.a`, and the program has one
definition of each. Two rules follow:

- Include each `.c` file in at most one test file; a second would be
  a duplicate definition.
- Files in `common/` (including `common/aicore/`) are in the shared
  `libfreeciv`, so including one gives the program two copies. On ELF
  platforms the test's copy takes over the file's global functions
  for the library's callers too, but that rests on symbol
  interposition and is not portable. Prefer testing a `common/`
  function through its public entry points.
