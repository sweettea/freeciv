# Session brief — M1: combat E2, force sizing in the currency of losses

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5, STATUS's "Combat
workstream E1" section (2026-07-31) and the D1 tail,
`findings/civ2civ3-adjustments.md` ("Combat workstream epic 1",
"Sequential-attrition odds", "T153 live-game forensics"),
`findings/deep-dive-2026-08-04/military.md` (U1, G1, G2, C4),
`findings/mechanism-map.md` §3 coupling 1, and the code: `deepodds.[ch]`,
`deepcombat.[ch]`, `deep_assault_want` (`deepdefence.c:2937`),
`deep_op_form`/`deep_op_eta` (`deepops.c`).

## Provenance warning

The E1-E8 epic map survives in STATUS; the detailed POSTURE NOTES for
E2+ were banked in a task store that no longer exists. Two notes were
recovered from code comments (D1/audit): "cover/kill sorties priced as
survival-deltas" (implemented today as a survival FLOOR, not a delta) and
"escort demand scales from zero with observed sea threat" (escort
recruitment today is unconditional on threat). **Anything else the human
banked for E2 is LOST — if a design choice below feels underdetermined,
flag it in STATUS and ask, do not guess.**

## THE JOB

1. **Attrition out of the substrate (the audit's #1 coupling).**
   `deep_stack_survival` computes the full defender HP distribution and
   returns one scalar; nothing anywhere reports what the ATTACKER loses.
   Extend the substrate to return expected attacker losses (units and/or
   shield value) alongside survival — the DP already walks the duels;
   unit-test it beside the existing `deepodds` test (distribution sums,
   degenerate cases).
2. **Cost-of-victory in the assault want.** `deep_assault_want` prices
   `worth × Δp` with no losses term. Subtract expected attacker losses in
   shield value, amortized the same way the want already is. One gain
   (PLACEHOLDER). The emergent behaviour the human should be able to see:
   deep DECLINES more attacks — report the decline rate and what happens
   to the freed shields.
3. **The pool question D1 deferred (C4's second half).** The build side
   prices the marginal attacker against ALL free attackers on the
   continent as one stream, so concurrent ops starve attacker production.
   E2 owns the fix properly: price against the force that would fight
   THIS target (per-op pools / the op's own manifest + unclaimed units in
   its range). No invented constant — the pool definition is structural.
4. **Force sizing at formation.** `deep_op_form`'s quorum is count-based;
   make the launch bar odds-based: form/launch when the assembled force's
   take-odds (now attrition-aware) clears the existing formation bar,
   sized so the EXPECTED SURVIVING force still meets the hold-the-city
   test (`deep_take_odds_at_arrival` covers the garrison it will face;
   the surviving-attackers output from (1) is what makes "can I hold it
   AFTER taking it" answerable for the first time).
5. **The two recovered posture notes**, if they fit the epic cleanly:
   sortie pricing as survival-DELTAS (value = Δsurvival × city worth, not
   just floor-passing), and escort demand scaled from observed sea threat
   (boat-shadow's evidence terms) instead of unconditional. If either
   bloats the epic, defer with a written note — do not half-land.

## Acceptance

- Substrate: unit tests for the attrition output; a hand-checkable
  numeric example in the test.
- Counters: declines (attacks priced but refused on cost), per-op pool
  sizes, odds-based launches vs count-based (before/after).
- A/B on two war cells (lategame-80 + contested or cramped), 20 paired
  seeds direction: score, cities taken/lost, unitslost (OURS should fall
  if the mechanism works), ops launched/aborted, the four-marginal-
  campaigns pattern (concurrent op count × their sizes).
- `meson test` (4 tests incl. the new one) passes; `tune-audit.py` CLEAN
  at its count; STATUS dated section final; leave NEXT-SESSION.md and
  sessions/ alone.

## Scope fences

6-9 commits. E2 ONLY — no E3-E8 content, no naval invasion wants (M2),
no fear-term changes, no floor changes (G1 already landed by this
point). `ai/default` untouched.

## Stop conditions

The attrition extension breaks the substrate's unit tests or its
performance envelope (the DP is O(defenders × HP) per stack — measure
before/after on a big-stack case); an underdetermined lost-posture
design choice (flag and ask); regression without attribution.
