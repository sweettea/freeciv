# Session brief — D1: military discipline

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5,
`findings/mechanism-map.md` §1 (D-A, D-F, D-G),
`findings/deep-dive-2026-08-04/military.md` §3 (C1-C5, C7 — the full
evidence with file:line), and the STATUS tail from D2 onward.

## Why

Three landed mechanisms are silently disabled by one line, and a family of
claim/probability inconsistencies undermines the ops layer's otherwise
rigorous discipline. All read-verified; none has ever executed in a
benchmark. This is the round that makes the military layer's measurements
mean something — G1's floor sizing runs after it for exactly that reason.

## THE JOB, in commit-sized pieces

1. **The done-flag fix (C1, the headline).** `deepops.c:1903` overwrites
   every non-op unit's `done` unconditionally, erasing the tired-attack
   hold, the convoy HOLD, the worker-flee mark, and the consolidate
   garrison's fresh `DEFEND_HOME` every turn. Fix with a phase-scoped
   marker (the flag is also stale from last turn, so "preserve if set"
   alone is wrong — the military audit's fix shape: preserve holds set
   THIS phase, reset everything else). Each revived mechanism must then
   be shown EXECUTING in a live game: tired-hold counter > 0 with the
   unit actually not attacking; convoy boat actually holding; fled worker
   staying fled; converted garrison keeping DEFEND_HOME.
2. **Sortie claim discipline (C2).** `deep_kill_sorties` and
   `deep_cover_sorties` are the only unit-claiming pools that skip
   `deep_op_claiming()` — a MARCH member can be poached as a bodyguard
   mid-crossing. Add the check both places.
3. **The melt asks the question everyone else asks (C3).**
   `deep_melt_surplus_defenders` disbands without
   `deep_city_stack_survival_without` and without the op-claim check.
   Add both — the machinery exists, this is consistency not design.
4. **Triage on probability (C7's one that matters).**
   `deep_emergency_cmp` sorts on grave_danger/urgency presence; it
   decides which of two dying cities gets the gold. Move it to the shared
   survival verdict (D2 landed one verdict per city per phase — read it,
   don't recompute). The other presence-count consumers (veteran guard,
   severe-danger cutoff) only if trivially safe; else leave and say so.
5. **Build/ops target agreement (D-F).** The two-line barbarian-gate fix
   (`deep_assault_want` gets the same barbarian contact exemption
   `deep_op_pick_target` has). The op-aware attacker pool (build side
   counts all free attackers as one stream across concurrent ops) is a
   REAL DESIGN QUESTION — fix only if a clean small form exists
   (e.g. divide the pool by live op count, NO invented constant);
   otherwise document the design need for combat E2 and skip.
6. **The target anchor (C5).** `deep_op_form` scores targets from the
   first city in the player list. Anchor at the best staging candidate
   (the machinery that later picks staging) or the empire centroid —
   whichever is smaller code. Its own A/B lens: target choices per game
   that changed.

## Acceptance

- Counters first, on a war-heavy cell (lategame-80 + contested or
  cramped): every revived mechanism shows nonzero executions where its
  log said "held/fled/converted" before. Paste the before-arm evidence
  that they were dead (counter fired, behaviour absent) and the
  after-arm evidence they live.
- A/B 20 paired seeds on both cells, pinned binaries: score direction
  plus the mechanism metrics (unitslost, tired attacks that waited,
  convoy load factors, sortie availability). Bundled A/B is acceptable
  BUT each fix needs its own counter so a regression can be attributed;
  if score regresses beyond noise, bisect by counters before landing.
- `meson test -C build` (3 tests) passes; `tune-audit.py` CLEAN (no new
  tunables expected — if a knob is genuinely needed, PLACEHOLDER + flag).
- STATUS.md dated section as final commit; note which "measured inert"
  claims from past STATUS entries are now void (the convoy-hold
  "verified inert" line at minimum). Leave NEXT-SESSION.md and sessions/
  alone.

## Scope fences

5-8 commits, `ai/deep` only. No fear-term changes, no floor changes (G1
owns those, next). No gain/weight tuning.

## Stop conditions

The done-flag fix cannot preserve op holds and phase holds simultaneously
without redesigning the manage loop (report the shape); a revived
mechanism regresses score beyond noise and attribution fails.
