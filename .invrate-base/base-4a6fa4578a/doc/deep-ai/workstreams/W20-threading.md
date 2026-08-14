# W20 — Threading

**Phase 4+, ai/deep + common + server. DO NOT START YET.**
Root cause: none. W20 attacks no root cause — it is a cost workstream, and
that is precisely why it must not be scheduled by enthusiasm.
Full reasoning + line refs: `findings/threading-obstacles.md` (Part 22) and
`findings/scaling.md` (Part 17).

This file exists mainly to record **why W20 does not start**, so the next
session does not have to re-derive it. The findings say it in prose; the
project needs it in the workstream index.

## The problem in one look

- Turn cost at 20 players is dominated by two terms that are quadratic in game
  size: `assess_danger()` (`daimilitary.c:827`, O(own_cities x all_units)) and
  `base_want()` (`daicity.c:1521`, O(own_cities^2 x player-range improvements)).
- One of them collapses algorithmically (`scaling.md` §3). Threading a
  quadratic loop buys hardware to run a bad algorithm faster.
- Evaluation is not pure. `base_want()` mutates `game.info.great_wonder_owners[]`
  — *global game state visible to every player* — to price one building in one
  city. `cityresult_fill()` writes `pplayer->government` and tile ownership.
  Nothing can run concurrently over state like that.
- The hottest function in the evaluation path, `is_req_active()`, took a global
  mutex on every call. Any threading serialises there completely.

## Hard ordering, and where it comes from

`00-context.md` §7: **W26 before W20 and before memoization.** That is not a
preference. `ai/deep` does not own its own building-want or danger path — it
delegates to `ai/default` (`dai_build_adv_init()`, `dai_assess_danger_player()`,
called from `deepai.c:330` and `deepdefence.c:1789`). The impure functions are
exactly the ones a threading epic would need to run concurrently, so W20 has no
surface to attack until W26 has given it one.

`threading-obstacles.md` §7 gives the full order, and W20 is step 6 of 6:

| # | Step | State (2026-08-01) |
|---|---|---|
| 1 | Delete the `is_req_active` mutex | **done**, `common/requirements.c` |
| 2 | Profile the 20-player case | **done**, `findings/scaling-profile.md` |
| 3 | Algorithmic fixes (`scaling.md` §3) | not started — W27 |
| 4 | Purity work | not started — W26 |
| 5 | Memoization | blocked on 4 |
| 6 | **Threading** | **this workstream** |

W26 was verified genuinely unstarted, not partially done: it appears nowhere in
`STATUS.md`, has no workstream file, and all four blockers named in Part 22 were
present verbatim — `common/aicore/cm.c:915` comparator statics,
`common/aicore/citymap.c:53` shared reservation array, `utility/rand.c:85` global
RNG state, and (until step 1 above) the requirements mutex.

## Design, for when it is time

Standard fan-out / fan-in, at **per-city** rather than per-player granularity
(`scaling.md` §5): 2000 tasks instead of 20, and a 100-city empire stops being
scheduled as one unit of work alongside a 3-city rump.

- **Parallel:** per-city evaluation (building want, danger, CM), per-candidate-tile
  site evaluation, per-unit target scoring.
- **Serial:** empire planning, unit movement, production commit, packet emission.
- **Determinism** holds because the order of *writes* never varies, whatever order
  the reads completed in. W33's committed/uncommitted split is the same boundary.
- The `invasion` counters are a **reduction**, not a race to lock away — per-thread
  partials merged in the serial phase. Locking them serialises the thing being
  parallelised.

## Acceptance

- Identical final savegame and score to the single-threaded build on 10 paired
  seeds, at every thread count. Threading that changes results is not a speedup.
- Turn time on the `scale-20p` cell reduced by a stated factor, with the
  single-threaded time measured on the *same* binary.
- A single-threaded test mode that remains the default.

## Dependencies

W26 (purity) and W27 (the algorithmic collapse), in that order, both complete
and measured. Do not accept "mostly done" for either.
