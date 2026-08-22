# Session brief — fortress saturation: instrument, do not fix

Live play showed deep carpeting city catchments with fortresses
(STATUS 2026-08-15 entry, commit d3d6ad869f — read it first; the
structural analysis of the want is there, deepsettler.c ~2238-2260).
Two guessed fixes on this project were reverted; the measured one won.
So this epic ONLY measures. **No change to any want or behaviour.**

## Counters (log-only; game state untouched)

1. Fortress tasks: per player per game, fortress tasks generated and
   completed (count + the want value distribution — min/median/max —
   so the eventual fix knows the price range it must beat).
2. Saturation: at game end (or every 50 turns), fortress-tile share
   of each deep city's worked catchment.
3. Denial: per turn, tiles in a deep city's catchment that carry a
   fortress AND an enemy unit (extras do not record their builder, so
   this overcounts enemy-built fortresses — say so in the counter name
   and the STATUS entry rather than pretending precision).
4. Opportunity: worker-turns by task class (fortress vs road/irrigate/
   mine/other) so the fortress spend is visible as a share.

Reuse the existing per-player census pattern (see deeptrunk.c:77-84)
and the counter print style the other epics grep from game.log.

## Proof and measurement

- Counters must not perturb play: scorelogs byte-identical to the
  base branch on 10/10 seeds of a short config (game.log may differ).
- Then ONE measurement run on a venue with real enemy contact — pick
  from existing benchmark/configs and justify the choice (the symptom
  venue was Europe vs classic/human; the closest benchmark analogue
  wins). >=16 seeds, ports 7500-7549, jobs <=3 (two sibling epics
  share the box).
- Deliverable: STATUS entry with the distributions (fortress share,
  denial turns, worker-turn shares) and a data-backed statement of
  whether the live-play symptom reproduces on benchmark venues at
  all — a "no, it needs the spicy ini's site_choke_bonus/base_fort
  values" is a perfectly good verdict and changes the fix design.
  If it reproduces only with spicy values, run the measurement once
  more with derby5/tuning-derby7.ini to capture the distributions.

Usual rules: no struct tuning changes, tune-audit CLEAN, one commit
one thing, meson test all suites, stop after the measurement run.
