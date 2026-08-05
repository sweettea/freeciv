# Session brief — D2: credit integrity

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5 harness rules,
`findings/mechanism-map.md` §1 (D-B, D-C, D-H — this session's defect
list), `findings/deep-dive-2026-08-04/economy-growth.md` (the full evidence,
§3.1-3.3 and §3.11-3.14), and the E34.2 tail section of STATUS.md (its
regime-check failure is partly this session's subject: gate 3 scales
credits that the sawtooth then multiplies unpredictably).

## Why this session gates everything

Three read-verified defects make every horizon-credit measurement and every
future tuning pass unsound until fixed. **The R3 calibration corpus and ALL
tuning wait on this session.** Fixes are behaviour-CHANGING by design — no
byte-identity gate; the acceptance is mechanism metrics plus score guards.

## THE JOB, in commit-sized pieces

1. **Kill the sawtooth (D-B).** `deep_adjust_building_wants()` adds into
   `building_want[]` every turn; stock clears it only every `fc_rand(5)+5`
   turns (`daicity.c:1985-2079`). The intended credit is applied once; the
   delivered credit is 1-9×, phase-randomised per city. Fix so the
   delivered credit equals the intended credit every turn regardless of
   stock's recalc phase (deep-owned per-turn credit tracking, or
   subtract-before-add of deep's own prior contribution; choose and justify
   — do NOT change stock's recalc schedule, that is `ai/default`).
   Prove with a per-city log trace: credit magnitude flat across a recalc
   window before/after.
2. **Capacity credit on a counterfactual ttg (D-C cause 1).**
   `city_turns_to_grow() == 0` forever in a steady-state capped city
   (aqueductloss 0 keeps the box pinned full), so the Aqueduct credit and
   the tech-wall route both die. Trigger on what ttg WOULD be if the cap
   lifted (catchment surplus under the city's actual CM factors). Prove:
   nonzero capacity credit in a long-capped city in a live game log.
3. **Break the CM/horizon deadlock (D-C cause 2).** The CM's cap state
   sheds food to surplus 0, which reads back as "cap is not binding".
   The counterfactual ttg from (2) is most of the fix; also make the cap
   state's food factor leave the counterfactual computable. No new
   constants — reuse the existing cm_* factors.
4. **One threatened verdict (mechanism-map coupling #2).** Compute the
   survival verdict once per city per turn; have `deephorizon.c`'s gate
   (`survival_target`) and `deepcm.c`'s state machine (`cm_war_survival`)
   read the same probability from the same call with their own thresholds
   intact (two thresholds may be legitimate; two independent COMPUTATIONS
   with two currencies are not). Also move the four remaining
   presence-count consumers to the probability ONLY if trivially safe —
   else leave for D1 and say so.
5. **Small sharp edges (D-H):**
   - upkeep charge dimension: charge amortized gold over the same horizon
     as the credit it nets against (`deephorizon.c:661-672`), not one turn
     against a MORT-horizon value;
   - bundle credit: count only tiles whose partner extra is actually
     reachable (tech + buildable-by-worker check), `deephorizon.c:462-509`;
   - `caravan_discount` guard: call `caravan_parameter_is_legal()` instead
     of the re-derived check that admits exactly 1.0; fix the registered
     range endpoint in `tuning-params.ini`;
   - tuning-log completeness: every registered knob prints in the
     `deep-tuning` line (`horizon_multiplier_weight`, `caravan_discount`,
     `caravan_foreign_trade`, `site_travel_*`); fix the drifted labels;
   - block assignments: `upkeep_gold_weight` and `survival_target` join
     the ECON block they tax (`tuning-blocks.ini`).

## Acceptance

- Sawtooth: per-city credit trace flat across a recalc window (paste
  before/after traces). Happiness-building count per 100 cities on the
  happiness-ladder cell moves toward baseline's (the 39.6-vs-4.0 overshoot
  was the sawtooth's headline symptom — report the new number, no target
  invented).
- Capacity credit: nonzero in a steady-state capped city (log evidence);
  capital-cap first-build table (firstbuild.py) before/after on a garden
  or long cell — Aqueduct-class buildings should appear where they never
  did. Report, don't promise.
- A/B at 20 paired seeds on garden2b + isolated-150, smoke guard, pinned
  binaries: score direction with the full metric table. A regression needs
  explaining before landing; a flat score with the mechanism metrics moved
  is acceptable per 00-context §8.
- `meson test` passes; `tune-audit.py` CLEAN at its count; every changed
  credit path has its execution counter read before its score.
- STATUS.md dated section as final commit — including an explicit list of
  WHICH past A/B interpretations this invalidates (happiness ladder,
  horizon ablations, W03 acceptance numbers) so nobody quotes them
  unqualified again.

## Scope fences

`ai/deep` + benchmark/doc only; `ai/default`'s recalc schedule is off
limits (rule 2). No gate/gain changes (E34.2b owns the fifth gate). No new
constants — everything reuses existing tunables or ruleset values. 7-9
commits. Leave NEXT-SESSION.md and sessions/ alone.

## Stop conditions

The sawtooth fix cannot be expressed without touching `daicity.c`'s
schedule (report the seam); the capacity-credit fix requires a new
invented constant (stop and flag per rule 7); any A/B regresses beyond
noise and attribution fails.
