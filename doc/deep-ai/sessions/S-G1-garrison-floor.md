# Session brief — G1: the garrison floor, sized then class-gated

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5, the E34.2/E34.2b tail
sections of STATUS.md (73-74% of military city-turns are floor-driven —
twice measured), `ai/deep/deepdefence.c:750-850` (the floor + menace code
and their comments), `deep_barbarian_fear` and `deep_boat_shadow_fear` in
the same file, and `findings/deep-dive-2026-08-04/military.md` §1a (the
fear lattice and who owns which threat class).

## Why

The floor is the de facto sizing policy for most of deep's standing army
(73-74% of military city-turns sit at raw danger zero), on four day-one
placeholders. The July island-native tunes both disabled it (pure tax on
island regimes); the naval-blindness history says zero was catastrophic
before the fear terms existed. Nobody has measured it on the current
binary, and the human set its design direction on 2026-08-05.

## THE DESIGN DIRECTION (human, 2026-08-05, verbatim intent)

Keep a thought out for barbarians — turn-dependent — and for paratroopers/
helicopters we could not see. But **early game, unless there's an active
threat, do not build a defence unit instead of a settler or a building.**

Translated: the floor becomes **class-gated insurance** — it exists only
while some threat class that the danger assessment CANNOT SEE is actually
live in this game, and is zero otherwise:

1. **Barbarian class** — live only when `barbarianrate != 0 && turn >=
   onsetbarbarian` (the server's own gate, which `deep_barbarian_fear`
   already reads). ONE owner per class: decide whether the floor's
   barbarian component subsumes F5 or vice versa — no double charge, and
   say which in the commit.
2. **Sea-landing class** — live only while a war-capable rival can float
   transports (the boat-shadow test). Same one-owner rule vs F2.
3. **Airborne class (NEW)** — live once any met, non-allied rival's known
   tech can build a paradrop- or helicopter-class unit; a quiet city can
   then be struck with no visible approach. Until then, zero. Capability
   test from the ruleset (unit class / paradrop action), not an invented
   list.
4. **No live class + no active threat ⇒ floor = 0.** The settler or the
   building wins. "Active threat" is danger > 0 or an observed wave —
   already the floor's danger==0 precondition; keep it.

The menace ratio (reachability-weighted militarisation) stays as a scaler
WITHIN live classes; it is not itself a class. Per-class weights are
PLACEHOLDERS (rule 7), registered everywhere tune-audit checks + the
tuning log + blocks; the class GATES are server/ruleset facts and free.

## THE JOB

1. **Size the current floor first** (before touching it): A/B
   `garrison_floor` off vs placeholder, isolated-150 + lategame-80, 20
   paired seeds each, pinned binary, tuning-file switch (file-vs-file or
   file-vs-none both valid post-H1; prove the off arm by counter). Read:
   city losses, floor-driven military city-turn mass, settlers/first
   builds, score. This is the insurance-or-tax baseline the redesign is
   judged against.
2. **Build the class-gated floor** per the direction above, each class
   with its own execution counter (extend the existing stance/fear log
   lines rather than new line types).
3. **Measure the redesign** against both arms of (1) on both cells, plus
   the early-game lens: first-build distribution (firstbuild.py) and
   settlers per 100t in the pre-onset window — the human's acceptance
   sentence is "early game, no live class ⇒ no floor defender instead of
   a settler or building"; show it happened. Guard: city losses must not
   regress toward the pre-naval-fix pattern (63v94-era numbers are the
   cautionary tale) — if losses rise, attribute by class before landing.
4. Byte-identity is NOT expected (behaviour change by design) — the
   discipline is counters-before-scores and the sizing baseline.

## Acceptance

- Sizing table (step 1) pasted for both cells.
- Class gates proven by counter: pre-onset games show zero barbarian-class
  firings; no-transport games show zero sea-class; pre-flight games show
  zero air-class. The early-game window shows floor = 0 and settlers
  winning (paste the first-build comparison).
- Redesign A/B tables with the loss guard explicitly addressed.
- `meson test -C build` (3 tests) passes; `tune-audit.py` CLEAN at its
  new count.
- STATUS.md dated section as final commit; leave NEXT-SESSION.md and
  sessions/ alone.

## Scope fences

5-8 commits. `ai/deep` + benchmark/doc only. No tuning of the per-class
weights (pile). Do not touch the anticipatory fear terms except where the
one-owner-per-class rule forces the coordination; any such change gets its
own counter and its own line in the A/B.

## Stop conditions

City losses regress and attribution fails; the airborne capability test
cannot be expressed from ruleset facts without an invented unit list;
anything needs `ai/default`.
