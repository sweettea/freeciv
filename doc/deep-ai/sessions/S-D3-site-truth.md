# Session brief — D3: site truth

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5,
`findings/mechanism-map.md` §1 (D-D, D-E),
`findings/deep-dive-2026-08-04/expansion-logistics.md` (§3.1, §3.2, §3.5,
§5.1, §5.2), `findings/upstream-bugs.md` (bug #1), and the code:
`deepsite.c` (`deep_city_desirability` at `:857`, the `:920` call),
stock's `result_defense_bonus`/`naval_bonus` (`daisettler.c:528-560`),
`deep_water_bridges` (`deepdefence.c:2098`), the lift MARCH drive
(`deepops.c:1738-1860`), and `deep_auto_settler_run`'s hold/validity
ordering (`deepsettler.c:1501`, `:1525`).

## THE JOB — four site-truth defects, one session

1. **The inverted defence bonus (D-D).** `deep_city_desirability` calls
   stock's `result_defense_bonus`, which is upstream bug #1: higher
   terrain defence scores LOWER. Per rule 2's doctrine: implement the
   CORRECT behaviour in deep (a deep-side replacement valuing defence
   positively), leave `ai/default` untouched, and fix the 00-context §5
   claim that deep "doesn't have that bug" (it did — cite this session).
   The magnitude constant: reuse stock's DEFENSE_EMPHASIS as the
   compiled default (measured, not invented) and register a PLACEHOLDER
   weight for the pile.
2. **The closed-water naval bonus (D-E's sibling).** Same call site
   imports `naval_bonus`, paying a maritime premium for ANY adjacent
   ocean — including seas the build side's own `boat_water_gate` refuses
   to sail. Gate the bonus on `deep_water_bridges` (extern, free): water
   that closes on its own continent earns no naval premium. The
   site-side mirror of the water gate — the sizing that motivated it
   found closed-water hulls were 6.9% of hulls owning 11.2% of idle
   time.
3. **Held-lift mission re-validation (D-E).** The op-hold early return
   precedes the `city_can_be_built_here` check, so a target founded on
   mid-crossing still gets a full delivery onto an unfoundable tile,
   counted as `lift-arrived`. Add the validity re-check inside the MARCH
   drive (the same predicate the settler path uses), re-aim via
   `deep_lift_serve_stop`'s existing machinery or abort; add a counter
   AND a metric line so the frequency is finally measured (nothing logs
   it today).
4. **`site_enough_result` half-wiring (audit 5.2).** The tunable exists
   but the search's early break still compares compile-time
   `RESULT_IS_ENOUGH` (`deepsite.c:1033`). Wire the tunable through;
   defaults identical.

## Acceptance

- Defence bonus: a corpus-style position check (a high-defence site now
  outranks an equal low-defence one; it ranked LOWER before — paste
  both), plus A/B on contested (where defence matters) 20 paired seeds.
- Naval bonus: counter of sites refused the premium; on isolated-150 the
  closed-water site premium goes to zero (the class the water gate
  measured); A/B direction.
- Lift re-validation: counter of mid-crossing invalidations observed
  (finally a number for D-E's unmeasured frequency); no lift-arrived
  regressions.
- Byte-identity NOT expected except for (4) at default (prove that one).
- `meson test` passes; `tune-audit.py` CLEAN at its count; STATUS dated
  section final (including the 00-context §5 correction note); leave
  NEXT-SESSION.md and sessions/ alone.

## Scope fences

5-7 commits. `ai/deep` + doc only (the 00-context §5 correction is in
scope — it is doc, and it is this session's own subject). No government
valuation (separately queued), no site-scoring redesign beyond these
four, no `ai/default`.

## Stop conditions

The defence-bonus replacement needs `cityresult` internals not exported
(check `daisettler.h`'s existing exports first — E-B.2 landed); the lift
re-check cannot reuse the settler predicate without duplication beyond a
rule-3 copy (state it).
