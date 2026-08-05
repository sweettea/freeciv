# Session brief — difficulty audit + per-level harness support

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5 harness rules, the
STATUS.md tail, and `ai/difficulty.c` (all of it — it is short and it IS the
stock difficulty model).

## Why this session exists

Goal (c) of this arc: a difficulty ladder on deep that is *meaningfully
similar* to stock's levels and usually beats stock at the same level. Before
building anything: know exactly what a skill level does to deep TODAY, and
be able to measure deep-vs-stock at any level. This session is audit +
harness only — no mechanism changes to deep.

## The stock model (verified 2026-08-04, `ai/difficulty.c:39-232`)

Four knobs, all set by `set_ai_level_directer()`:

1. **Handicap bitvector** — novice/restricted: 14 handicaps (H_RATES,
   H_TARGETS, H_HUTS, H_NOPLANES, H_DIPLOMAT, H_LIMITEDHUTS, H_DEFENSIVE,
   H_DIPLOMACY, H_REVOLUTION, H_EXPANSION, H_DANGER, H_CEASEFIRE,
   H_NOBRIBE_WF, H_PRODCHGPEN); easy: same minus H_DANGER and
   H_PRODCHGPEN; normal: 6 (H_RATES, H_TARGETS, H_HUTS, H_DIPLOMAT,
   H_CEASEFIRE, H_NOBRIBE_WF); hard/cheating: H_RATES only.
2. **Fuzzy** (`ai_fuzzy()` random decision flip): novice 400/1000,
   easy 300, everything else 0.
3. **Science cost**: novice/restricted 250 (research at 40% speed),
   everyone else 100. Applied server-side — free for deep.
4. **Expansionism**: novice/easy 10, normal+ 100. Read by stock's settler
   want; check whether anything deep forwards to still applies it.

Known deep-side coverage (verified by grep 2026-08-04): deep's own code
reads only `H_MAP` (17 sites), `H_FOG` (1), `H_AWAY` (1), and calls
`ai_fuzzy()` at exactly 2 sites (`deepdefence.c:2457,3210`). Everything
else reaches deep only where deep still forwards to `dai_*` code.

## THE JOB

1. **The audit.** For each knob above, trace stock's consumers
   (grep `ai/default` + `server/advisors` + server core) and answer: does
   the equivalent deep path respect it, bypass it, or not exist? Deep has
   replaced stock's choose_build, settler placement, explore want, CM
   parameters, danger assessment (partially), gold spending, ops — each
   replacement is a place a handicap stock honours may have silently
   stopped applying. Produce a table in
   `findings/difficulty-audit.md`: knob × (stock consumer file:line,
   deep equivalent, VERDICT respected/bypassed/n.a.), plus the ranked
   list of gaps by expected behavioural size. Cite line numbers you
   actually read.
2. **Harness support.** `run-game.sh` already threads `SKILL=` from
   configs (all current configs say `hard`). Add the cheap generalisation
   the measurement needs: a way to run existing configs at another level
   without duplicating every .conf (e.g. a SKILL override that composes
   with the cache key — the key MUST change when skill does; verify by
   showing two dirs). Both arms of a mixed game share one skill level,
   which is exactly the comparison goal (c) wants.
3. **Baseline snapshot, direction only.** One 10-seed paired mixed run
   (deep+stock same game, the mixed_db/mixed_bd pattern) at `novice` and
   one at `normal` on one cell: where does deep-vs-stock stand at lower
   levels TODAY? Expectation from the audit is that deep is
   under-handicapped (its own wants ignore H_EXPANSION etc.) and will
   look artificially strong at novice — the number that motivates the
   ladder session. 10 seeds is direction; say so.

## Acceptance

- `findings/difficulty-audit.md` exists with the table, every row cited.
- Skill override demonstrated: same config at two levels → two result
  dirs, DONE counts complete, different behaviour in the logs (paste the
  `ai_level` evidence from the server log or savegame).
- The two direction runs' numbers in STATUS.md, labelled as direction.
- `meson test -C build` passes (harness-only changes; expect no code
  effect — say if any).
- STATUS.md dated section as the final commit; leave NEXT-SESSION.md
  alone.

## Scope fences

No changes to `ai/deep` behaviour, `ai/default`, `ai/baseline`, or
`server/advisors`. Harness + findings + STATUS only. The ladder mechanisms
belong to the next session and must be designed FROM this audit, not
alongside it.

## Stop conditions

The audit finds a knob whose stock consumer moved since 0896b34 (stale
plan — say so); the harness override can't be made cache-safe.
