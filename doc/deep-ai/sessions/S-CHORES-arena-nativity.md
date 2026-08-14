# Session brief — chores round: tune.py --arena + the owed nativity unit test

Two owed items, both CPU-light. This is a HYGIENE-style round: no
gameplay behaviour changes, no A/B beyond smokes.

## 1. tune.py --arena mode (PERPLAYER follow-on, task #44)

PERPLAYER (merged 935b79ccb8) gave the server per-seat tuning via the
FREECIV_DEEP_ARENA manifest — but tune.py cannot drive it. Add an
--arena mode:

- Input: a manifest mapping seats to tuning files (reuse the exact
  format the server side already parses — read
  ai/deep/deeptuning.c's arena loader first and match it, do not
  invent a second format).
- tune.py --arena should run seat-diverse games (different seats,
  different tunings, one binary) and report per-seat objectives, with
  the same content-addressed result caching the other modes get (the
  cache key must include the whole manifest content, not just one
  tuning hash — a manifest edit must miss the cache).
- Smoke proof: one short venue (e.g. an -80 config), two seats on
  deliberately different tunings, show the per-seat report
  distinguishes them and that the deep-tuning log line in each game
  confirms which seat loaded which file. A smoke is a few games on a
  busy box — use jobs 2 and ports 7900-7949, or wait for headroom.

## 2. MOVETYPE nativity-helper unit test (owed since NAVSEE, task #58)

The nativity helper that MOVETYPE built (deep-side unit-class
nativity/move-type classification) shipped without an L0 unit test
and NAVSEE flagged it. Add one to the existing meson unit-test
suites: cover land/sea/both-native classes and the UMT_SEA dead-code
finding's boundary (utype_move_type is client-only — the test pins
the deep-side helper's behaviour so a future "fix" can't silently
change classification). Read the helper first; test what it DOES.

## Rules

- No behaviour changes anywhere; if the arena loader or the nativity
  helper turns out to have a bug, REPORT it in your return and (if
  it is in ai/default or stock code) append to
  doc/deep-ai/findings/upstream-bugs.md — do not fix stock code.
- meson test -C build 5/5 (or 6 suites if the new test adds one)
  before each commit; one commit one thing.
- No struct tuning changes expected; tune-audit must stay CLEAN and
  unchanged.

Deliverable: the two commits (or three if the manifest-cache change
is separable), the smoke's per-seat report pasted, and a STATUS note.
