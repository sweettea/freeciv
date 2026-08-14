# Session brief — TECHMIL slice 2: put the research goal's distance discount in deep's hands

Slice 1's census (STATUS 2026-08-13, task census merged ef71c98c8d)
falsified the dead-writer hypothesis and localized the real defect:

- Deep seats WRITE ~9% MORE military tech_want than stock (61.0% vs
  56.5% share) — every writer is alive.
- Yet deep researches military techs on only 47.5% of turns vs
  stock's 68.4%.
- Cause: `dai_select_tech()` (ai/default/daitech.c) divides
  accumulated want by tech distance, and deep's military wants sit
  farther out (mean 7.76 vs 7.03 steps to the wanted advance — 25%
  relatively less on-deck). The argmax then repeatedly picks nearer
  econ techs; the military mass never wins a round.

## The slice

Give deep its own research-goal selection so the distance shape is
tunable from OUR side. Rule 2 forbids touching dai_select_tech's
behaviour; deep registers/overrides from ai/deep instead (deep
currently inherits — find the cleanest seam: the tech hook deep does
not yet register, or a post-pass that re-sets the research goal.
Visibility-only static→nonstatic in ai/default is permitted with the
neutrality proof if reading tech_want[] requires it).

- Knob `techgoal_distance_power` (double). Default MUST reproduce
  stock selection byte-identically — the safest shape is a gate knob
  `techgoal_deep` (int, 0 = deep does not touch research goals; stock
  path runs exactly as today) with the power knob only read when the
  gate is on. PLACEHOLDER values; no invented constants — the power
  gets tuned, not chosen.
- When on: recompute the goal from the same tech_want[] mass with
  want / distance^power (stock behaviour is power=1 over
  research_goal_unknown_techs steps — verify the exact stock formula
  by reading it, do not assume this sentence).
- Counter `deep-techgoal`: turn, player, chosen goal, stock's
  would-be choice, both distances, both wants, military-class flags.
  The would-be field is the honesty check (same pattern as FENCE).

## Guards

- Byte-identity 10/10 at defaults including one mixed cell.
- A/B venue: tech50war-200 mixed, >=16 mirrored seeds, one binary,
  pinned WITH libfreeciv.so. Sweep the gate on with power at a small
  grid (e.g. tune a single value on training seeds first if time
  allows; otherwise A/B the gate at power=1 vs off to isolate the
  reselection seam itself, and report that the power tune is owed).
- Report: score paired diff, military-research-share shift (the
  census instrument from slice 1 still logs), goals-differ rate.
- If the seam requires copying stock selection logic into ai/deep,
  say so in the commit per rule 3.

## Not this slice

No new valuation terms (parity/era/compounding — that design waits on
this seam existing). No changes to tech_want writers. No trade/diplo
tech pricing changes.

Usual rules: struct tuning + params/blocks registration, tune-audit
CLEAN, knobcensus entry, one commit one thing, meson test 5/5, STATUS
updated with pasted numbers, stop after the A/B.
