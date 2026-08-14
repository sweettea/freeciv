# Session brief — NETTHREAT item 2: flat discount on home-pinned enemy mass

The pin-probe (merged ee61b90394, STATUS 2026-08-13) measured the
population this knob prices:

- Of enemy units observed garrisoning at their assembly/home city,
  only 3.22% ever reach one of our cities (18,538 windows; arrival
  proxy that only OVERSTATES reach).
- 68.7% of that mass never leaves home at all — this knob's target.
- 28.1% sorties but never reaches us — that mass belongs to
  survival_pf_reach and is explicitly NOT this knob's business
  (separate A/B arms, no double-spend).

The probe's hooks are already live: `deep_pin_poll` /
`deep_pin_note_move` / `deep_pin_reset` (deepcombat.h) populated via
the unit_move_seen seam. This slice turns the observation into a
danger-side discount.

## The slice

- Knob `threat_pin_discount` (double, default 0 = off,
  byte-identity). When > 0: enemy units currently classified as
  home-pinned by the existing pin tracker contribute
  (1 - threat_pin_discount) of their normal threat/danger weight in
  deep's danger assessment. FLAT term — same discount regardless of
  posture, distance, or war state. The posture-conditional variant is
  a later slice if the flat one moves anything; do not build it now.
- Locate the single choke point where enemy unit mass enters deep's
  danger/fear accumulation and apply the discount there — one seam,
  not scattered multiplications. If deep's danger path is inherited
  from ai/default at that point, the visibility exception (with
  neutrality proof) or a deep-side wrapper is the route; no stock
  behaviour changes.
- Counter `deep-pindisc`: turn, player, units discounted, mass before
  and after, share of total danger discounted. A knob-on run where
  the discounted share is ~0 means the seam missed the population —
  say so rather than shipping.

## Guards

- Byte-identity 10/10 at default 0, including one mixed cell.
- A/B: tech50war-200 mixed, >=16 mirrored seeds, one binary pinned
  WITH libfreeciv.so. Value for the on-arm: derive the
  first-cut discount from the probe's own measurement (the 3.22%
  reach / 68.7% never-left split is the measured prior — document the
  derivation in the commit; it is a measured value, not an invented
  one) and flag it in STATUS for a proper tuning pass.
- Report score paired diff AND the defensive side effects: cities
  lost, garrison sizes, units built. The risk mode is under-defending
  against a pinned mass that unpins — the probe says that costs 3.22%
  of windows; the A/B says whether the freed shields beat it.
- No interaction arms: survival_pf_reach stays at its pv3 value in
  BOTH arms.

## Not this slice

No posture-conditional discount, no sortied-mass pricing
(survival_pf_reach's remit), no changes to the pin tracker itself.

Usual rules: struct tuning + params/blocks registration, tune-audit
CLEAN, knobcensus entry with parseable predicate, one commit one
thing, meson test 5/5, STATUS updated with pasted numbers, stop after
the A/B.
