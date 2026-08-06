# Session brief — ENDGAME: the AI learns the game ends (pre-v4)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md`
§5, `findings/deep-dive-2026-08-05/diplomacy-gamestate.md` §5 (the
evidence: zero reads of `end_turn` anywhere in ai/ or advisors; MORT=24
fixed; 199/199 games end on the turn limit; score = citizens ×1, techs
×2, future techs ×5, great wonders ×5, units/10, kills/3).

## THE JOB

1. **Turns-remaining horizon.** A deep-side helper: `deep_turns_left()`
   = `end_turn - turn` (readable game state). Amortization callers in
   deep clamp their horizon to it: `amortize(value, MIN(delay, MORT,
   turns_left))`-shaped, so a granary started on turn 195 of 200 stops
   pretending 24 turns of payback exist. Behind `endgame_horizon` (0 =
   off, byte-identity); counter = clamps applied.
2. **The endgame score mode.** In the final `endgame_window` turns
   (PLACEHOLDER, e.g. 10-20, tunable), bias wants toward score-dense
   sinks: finish in-progress great wonders (×5), techs (×2, future ×5 —
   couples with R1's rates machinery: max science when gold no longer
   buys anything that completes in time), citizens (growth that lands
   before the bell). This is a want multiplier family on existing
   scales, not new wants; RC4 discipline.
3. **What NOT to do**: no culture/History chasing as a headline (partial
   metric-gaming — civ2civ3-only, flagged in unused-game-surface); no
   MORT global change (v4 tunes MORT-adjacent weights — this session
   must not be bundled with credit tuning, per the campaign doc).
4. **Fairness note for STATUS**: stock doesn't know the game ends
   either; this is a benchmark-realism feature (humans absolutely play
   the bell in scored games) and a real-game feature wherever endturn is
   set. Score-mode gains are expected to be REAL score, and the A/B
   must separate horizon-clamp effects (small, correctness) from
   score-mode effects (the farmable window).

## Acceptance

- Byte-identity at defaults-off; clamp and mode counters.
- A/B on two cells with different endturns (isolated-150 T150 +
  lategame-80 T80), 20 paired seeds: score (the metric IS the target —
  expect the cleanest positive signal this arc has had, or the finding
  that the endgame was already efficient), final-10-turn build
  distribution, wonders completed in the window.
- The horizon clamp alone (mode off) measured separately.
- `meson test` + audit CLEAN; STATUS final; sessions/ untouched.

## Scope fences

5-7 commits. Deep-side only. `endgame_window` and the mode multipliers
are PLACEHOLDERs for the pile; defaults chosen by the A/B per the D3
precedent (measured, not invented).

## Stop conditions

The clamp regresses mid-game behaviour (it must be inert before the
window — prove by counter); score-mode gains fail to appear (report —
"the auction was already end-efficient" is a fine verdict).
