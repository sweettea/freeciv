# Session briefs

One file per scheduled session, written ahead of time so a session can be
handed to an agent whole. Each brief is the session's §0 THE JOB; the hard
rules stay in `CLAUDE.md` and the harness rules stay in `NEXT-SESSION.md` §5
(read both first, every session, as always).

## Order and state (2026-08-04)

| # | Brief | State |
|---|---|---|
| 1 | W34 E34.1 — stance surface | **in flight** (no brief; launched from W34 A.7 directly) |
| 2 | [S-E34.2-gates.md](S-E34.2-gates.md) | ready, blocked on E34.1 |
| 3 | [S-E34.3-traits.md](S-E34.3-traits.md) | ready, blocked on E34.2 |
| 4 | [S-omniscience-flag.md](S-omniscience-flag.md) | ready |
| 5 | [S-difficulty-audit.md](S-difficulty-audit.md) | ready |
| 6 | [S-difficulty-ladder.md](S-difficulty-ladder.md) | ready, blocked on the audit |

## What was descoped, by whom

- **The owed lift-cluster tuning pass** (old NEXT-SESSION §0) is **deferred to
  the cloud pile** — human call 2026-08-04: "we can tune on cloud machines,
  later, as one of a pile." It is not to be run locally, and no session below
  runs any tuning campaign. Placeholders keep accumulating with their
  STATUS.md flags, exactly as rule 7 wants; the pile runs once, on a mature
  machine, per the 2026-07-31 mechanism-epoch decision.
- **Threading** (W20), **tuning for non-omniscient play**, and **conversation
  with AI allies** are out of scope for this whole arc (human call
  2026-08-04).

## Where this arc is going

Three end states, set by the human 2026-08-04:

a. **Ready for extensive tuning** — W34 phase A pinning makes the ~110-knob
   space separable into blocks; every placeholder is registered and
   audit-clean; the campaign design is on the shelf for a cloud box.
b. **An omniscient-AI flag** — deep can be run honestly (H_MAP/H_FOG/
   H_TARGETS live at any difficulty) behind a single A/B-able switch, with a
   census of what breaks. Flag and census only; the inference layer
   (findings/honest-information.md §5) stays future work.
c. **A difficulty ladder on deep** — deep at a given skill level is
   *meaningfully similar* to stock at that level (same knobs, comparable
   nerf magnitude) and usually beats stock at the same level.
