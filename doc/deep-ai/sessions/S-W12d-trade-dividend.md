# Session brief — W12 slice 4: price corridors by the trade routes they unlock

Slices 1-3 (STATUS entries; read all three) built trunk corridors,
beyond-ceiling sourcing, and allied endpoints. Slice 3's A/B was a
score NULL with the mechanism strongly live: corridors are cheap to
build and buy nothing. This slice gives them something to buy.

## The idea

Deep already amortizes caravan route value by travel time
(`caravan_discount` = 0.95/turn, ai/deep/deeptrade.c ~246, 1125).
Route face value GROWS with distance while the discount buries distant
routes exponentially — so the biggest routes on the map are exactly
the ones slow travel suppresses. A road that cuts caravan ETA from 30
to 10 turns multiplies discounted route value by 0.95^-20 ~ 2.8x.
That dividend is currently credited to nobody: trunk corridor pricing
is flat logistics (`trunk_road_gain`).

Add, behind a gate (default off, byte-identity): a corridor trade
dividend — for feasible (own city, partner city) route pairs whose
caravan path the corridor serves,

    route_value * (d^ETA_with_road - d^ETA_without)

summed into the corridor's value alongside the logistics term.
REUSE deeptrade's route valuator and `caravan_discount` for d — do
not invent a second discount or a second route value. Foreign/ally
partners already carry `caravan_foreign_trade`; the pairing with
slice 3's ally endpoints is the point of the design.

## Constraints

- ETA-with-road needs an estimate, not a second exact pathfind per
  pair per corridor: design a bounded approximation (e.g. corridor
  length at road move-rate vs at terrain move-rate), state it in the
  commit body, and counter the number of pair evaluations per turn so
  CPU cost is visible. If per-turn cost is measurable in game wall
  time, cache by (corridor, partner) and say so.
- New knobs (gate + gain) go in struct tuning; gain's on-value in
  runs is 1.0, the unit case — the dividend is denominated in the
  same want units the valuator already emits, so 1.0 is a derivation,
  not a guess. Register in tuning-params/blocks + knobcensus;
  tune-audit CLEAN at the new count.
- Slices 1-3 counters stay live; add a census field for
  dividend-priced corridors and for picks the dividend flipped.

## Proof

- Byte-identity at defaults 10/10 including one mixed cell, vs a
  `git archive` base build (pass the source dir to meson; worktree
  builds need explicit `-Daimodules=classic,tex,baseline,deep`).
- A/B: lategame-80, >=16 mirrored seeds — slice 3 PROVED this venue
  forms deep-deep alliances within 80 turns, and slice 3's on-arm
  (score +0.65, t +0.66, NULL) is the baseline to beat: run the A/B
  as slice3-on vs slice3-on+dividend, one key apart, so the dividend's
  marginal effect is what you measure. Counters: dividend picks > 0
  or it's a miss — report, don't ship.
- Europe rider: gate on in derby5/tuning-derby7.ini at the unit gain
  (sed existing keys, append only new ones, awk-dedupe, read back the
  game.log deep-tuning line).

Usual rules: one commit one thing, meson test all suites, STATUS with
pasted numbers, no ai/default or ai/baseline changes, stop after the
A/B.
