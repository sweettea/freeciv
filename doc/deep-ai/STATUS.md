# Status

Update after **every epic**. Terse and factual. This is what makes the next
session work.

Format:
```
## W## — Name
- [x] E##.1 short description (N commits) — metric before -> after
- [ ] E##.2 short description
- Note: anything the next session needs to know
```

---

## Phase 0 — Foundation

## W00 — Baseline vendoring
- [x] E00.1 vendor ai/default + ai/classic as ai/baseline, register aitype
      (5 commits on `epic/e00.1-baseline-vendoring`, based at `0896b34`)
      — vendored sources are byte-identical to 0896b34; symbol collisions
      avoided by a force-included rename header (167 globals -> `bl_*`,
      generated via `nm`), so `diff -r ai/baseline/default ai/default`
      is empty. Registered as aitype `baseline` behind
      `-Daimodules=classic,tex,baseline`. Freeze guard: MANIFEST.sha256
      + meson test `baseline-frozen` (verified to catch a tamper).
      Smoke-tested: `create Foo baseline` works; 8-turn autogame with
      2 baseline + 4 classic AIs ran to the turn limit, exit 0, no
      assertions (needs `set timeout -1`, `set minp 0` headless —
      see scripts/test-autogame.serv).
- [x] E00.2 confirm `create Foo baseline` plays and saves/loads
      — found and fixed a real bug: every registered module's save hook
      runs for every player, and the vendored classic wrappers hardcode
      the live classic module's secfile prefixes ("ai" / ""), so mixed
      games wrote duplicate keys (player0.ai0.spam, u0.passenger) and
      the save was unloadable. Shim now overrides the six savegame
      hooks with a "baseline" prefix (commit c4f3ac2b38; MANIFEST
      regenerated pre-benchmark). Verified: mixed 2-baseline/4-classic
      game saves at T16, reloads with both aitypes intact, plays to
      T50; a save written by the fixed code (T48, 8 baseline-prefixed
      keys) round-trips and continues too. No assertions in any run.
      Note: the baseline-frozen meson test caught a stale manifest
      mid-fix — the guard works.

## W00 — Harness
- [x] E00.3 headless batch runner with paired seeds (2 commits)
      — benchmark/run-game.sh (one deterministic game: seeds pinned,
      per-game cwd, SCORELOG2 for all players, map hash from the turn-1
      autosave) + benchmark/run.sh (paired batch, parallel with unique
      ports, resume via DONE markers, automatic map-pairing check).
      configs/smoke.conf: 4p/32x32/hard/T100, ~40 s per game; --quick
      (10 seeds x 2 arms) ≈ 2 min at 12 jobs. Verified: 6/6 games,
      0 pairing violations; rerun of same (config,seed,arm) is
      byte-identical; classic vs baseline scorelogs byte-identical per
      seed — re-proving the vendored baseline behaviourally identical.
      Gotchas encoded in the scripts: parallel servers need distinct
      TCP ports; `save` in a --read script runs in pregame (no map), so
      the turn-1 autosave is the map-identity source; FREECIV_DATA_PATH
      must point at src/data when cwd is elsewhere.
- [x] E00.4 metric extraction + content-addressed result cache (3 commits)
      — extract.py: one CSV row per player per game with all 31 scorelog
      tags at last logged turn + alive + mean_city_size derived from
      citizen-class tags / cities (cross-checked against final-save city
      sizes: 5.2 both ways). run.sh results now live in
      results/<config>-<key12>/ keyed on sha256(config, server binary,
      tuning [env TUNING, "none" until W25]); DONE-marker resume doubles
      as the cache, key change → fresh directory, key.txt records
      components, metrics.csv written after every batch (works from
      cache too). Verified: fresh 4-game batch, full cache-hit rerun,
      TUNING change produced a separate keyed dir. Note: hashing the
      server binary over-invalidates on rebuild — deliberate
      (correctness over cache hits).
- [x] E00.5 regression corpus loader + first three positions (4 commits)
      — corpus-run.sh (load position, advance 1 turn, run assert.sh;
      runner asserts clean advance + correct post-turn) + savequery.py
      (turn, extra counts from hex layers, cities, captured, wars, min
      cross-owner distance). Positions checked in with PROVENANCE:
      hut-rich-start (64x64 T1, 63 huts), contested-border (6p T30,
      cross-owner cities 3.16 apart), losing-war (T140, 2 captured
      cities, 7 war pairs). All pass in ~5 s; a deliberately-broken
      assertion was shown to fail. Two harness bugs found and fixed
      along the way: (1) civ2civ3 sets mapsize=PLAYER so xsize/ysize
      were silently ignored in every earlier game — run-game.sh now
      forces mapsize XYSIZE; (2) run-game.sh wasn't part of the result
      cache key, so command-template edits could silently reuse stale
      results — it's hashed into the key now.
- Note: corpus positions currently play all players as classic (the
      aitype recorded at generation). When ai/deep exists, the loader
      will need a way to reassign a position's players to the arm under
      test — likely a small server patch or regenerated positions.
- [x] **HUMAN VERIFICATION GATE** — see workstreams/W00-harness.md §5.
      Checks executed 2026-07-24; evidence below. **Confirmed by human
      2026-07-24.**
      1. Determinism: PASS — two fresh --quick batches (20 games each),
         metrics.csv byte-identical (80 rows).
      2. Pairing: PASS — per-seed map hashes identical across arms for
         all 10 seeds (and the automatic check runs every batch).
         Bonus: classic vs baseline scorelogs byte-identical 10/10.
      3. Holdout isolation: PASS (caveat) — only holdout reference in
         the harness is run.sh's usage warning; tune.py doesn't exist
         yet. **Re-run this check when W25 lands.**
      4. Sensitivity: PASS, with a finding. founder_want/2 (the
         suggested regression) changed behaviour in 10/10 games (arms
         stopped being byte-identical, first divergence ~T44) but had
         NO measurable score effect at n=10/T100/smoke (mean +3.4,
         sd 28 → effect ≈ 0.1σ). founder_want/100 was caught loudly:
         score worse 10/10 seeds, paired-t −9.96; cities −24.0 mean,
         t −12.8. Two lessons recorded: (a) the byte-identity check is
         the most sensitive behaviour-change tripwire the harness has;
         (b) settler-want magnitude is score-neutral on smoke at T100 —
         W05/W02 need their proxy metrics, exactly as 00-context §8
         predicts. Regression reverted; tree verified clean (git diff
         empty, baseline-frozen Ok, classic==baseline byte-identical
         again on a fresh run).

## W00 — Module skeleton
- [x] E00.6 ai/deep forwarding all hooks to dai_* (2 commits)
      — deepai.c derived from classicai.c: every hook forwards to dai_*
      with deep's own ai_type; own savegame namespaces ("deep"/"deep_")
      per the E00.2 lesson. Registered behind -Daimodules=...,deep.
- [x] E00.7 wire player_save / player_load (1 commit)
      — struct deep_plr embeds struct ai_plr first (tex pattern);
      player_save/load (wired by no module before) persist deep state
      under player%d.deep_state, all keys defaulted on load.
- [x] E00.8 per-turn decision vector logging (1 commit)
      — one parseable deep-dv line per player per turn at LOG_VERBOSE
      (-d 4): turn/player/cities/units/gold/techs/science. Persisted
      dv_turns_logged counter proves the save path: 8 at a T8 save,
      resumes to 12 after reload.
- [x] **GATE**: deep vs baseline, 50 paired seeds: scorelogs
      byte-identical 50/50, aggregate score ratio 1.000000 (+0.000%) —
      well inside ±2%. Harness fix en route: a game ending early
      (seed 49: allied victory T58) was misclassified as failed; the
      GAMEOVER autosave is now the completion signal (run-game.sh),
      cache key rotated automatically.

## W25 — Tuning harness
- [ ] E25.1 struct tuning + file loading
- [ ] E25.2 CMA-ES driver, train/holdout split

## Tier B — visibility and purity
- [ ] E-B.1 delete the unused is_req_active mutex (common/requirements.c:6756)
- [ ] E-B.2 export site evaluation internals from daisettler.h
- [ ] E-B.3 export base_want internals from daicity.h
- [ ] E-B.4 req_context government override
- [ ] E-B.5 city_virtual_copy
- [ ] E-B.6 per-player RNG, per-thread timing buckets

---

## Phase 1 — First demonstrable win

## W05 — Settler routing
- [ ] not started

## W11 — Defence bookkeeping
- [ ] not started

## W28 — Worker stacking
- [ ] not started

## W06 — Exploration
- [ ] not started

## W09-lite — City manager parameters
- [ ] not started

---

## Later phases

See `PRD-v3.md`. Extract workstream files as you reach them.

---

## Open questions for the human

(Agent: append here rather than guessing.)

- 2026-07-24: PRD-v3.md carried pre-revision placement/phasing directives that
  contradicted CLAUDE.md and findings/placement-revision.md; amended with a
  supersession banner, corrected baseline date (0896b34 is Jul 2026, not Apr),
  and `ai/deep` placement tags on W2/W3/W5/W6/W10/W11/W27. Review welcome.
- 2026-07-24: work is on branch `epic/e00.1-baseline-vendoring`, based at
  `0896b34` (the declared baseline), which is 21 commits AHEAD of local
  `main` (691fe4de54). ai/ + server/advisors/ are identical between the two.
  Suggest fast-forwarding main to 0896b34 or continuing on epic branches.
- The planning docs (CLAUDE.md, doc/deep-ai/, benchmark/, run-phase.sh) are
  untracked; commit them when ready. `freeciv-deep-ai/` is an unused duplicate
  of the starter kit — delete or keep as reference?
- Autotools (ai/Makefile.am) was NOT taught about ai/baseline; the project
  builds with meson per CLAUDE.md. Flagging in case autotools parity matters.
