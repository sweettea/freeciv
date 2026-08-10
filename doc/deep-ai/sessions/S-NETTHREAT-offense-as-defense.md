# Session brief — NETTHREAT: offense discounts the threat it attacks

Human insight (2026-08-10): "we aren't accurately pricing attackers as
part of our defense — if we spend a unit on attack, we likely also shut
down the counterattack we were defending against, so we need less
defense than the current estimate of the opposing army."

This is the third leg of the deterrence work. M4b priced what defenders
DO to attackers (deterrence dividend) and what sorties WIN (killstack
prize). This session prices what OFFENSE is worth to defense: today the
two books never meet.

## The defect, located

`deep_city_stack_survival` (`deepcombat.c:203-228`) sizes every
garrison against the GROSS wave — all at-war units reachable within
the horizon, assembled straight from `aplayer->units`. Meanwhile op
pricing plans offensives that will engage, absorb, and destroy part of
that same army. The empire pays twice for the same enemy unit: once in
defenders sized against it, once in attackers sent to kill it.

## Three nettings, in priority order

1. **Offense consumes the wave.** Enemy units targeted by our planned
   ops/sorties will be fighting (and dying) elsewhere; the home wave
   should be discounted by E[enemy losses to our committed offense] —
   the op pricer and the sortie DP already compute exactly this number
   per engagement; route it into the wave assembly as a subtraction,
   gated by `offense_net_gain` (PLACEHOLDER, 0 = off).
2. **The pin (mirror of M4b's deterrence dividend).** Our offensive
   posture forces rivals to hold defenders home. The wave currently
   counts every reachable enemy unit including their garrisons; units
   whose own city would fall without them are unlikely wave members.
   Symmetric logic to our own hold decisions; discount knob
   (PLACEHOLDER). Cheaper probe first: measure how often units that
   were garrisoning at wave-assembly time actually attacked us within
   the horizon (a counter, no behavior) — if garrison-pinned units
   rarely sortie, the discount is honest.
3. **Attackers garrison too.** A home-based attacker physically joins
   stack survival when present (already priced in the DP), but at the
   WANT seam attacker-vs-defender builds compare as if orthogonal —
   an attacker's build want carries zero defense credit. Credit it
   with its expected garrison contribution weighted by fraction of
   time home (rotation's cadence facts know this), PLACEHOLDER gain.

## Guards (each one is load-bearing)

- **No double-spend of an enemy unit.** One enemy unit may be netted
  ONCE: either it discounts our defense demand (item 1/2) or it feeds
  a sortie/deterrence prize — never both. State the bookkeeping rule
  in the code where the discount lands.
- **The launch-rate reality check.** Planned offense often never
  engages (CONSEQ N3: Iceland 57 Marines, 6 launches). Netting must be
  weighted by OBSERVED launch/engage rates (the hold/launch counters),
  not by intents, or the discount licenses disarmament-by-wishful-
  thinking.
- **Disarmament spiral.** The failure mode of overweighting this is
  skipping real defenders and dying to walk-ins (derby-2's empty-city
  plague). A/B lens must show cities-lost-to-walk-ins alongside any
  economy gained. Default OFF; byte-identity at 0.
- **Venue.** These knobs only get tuning gradient where wars conclude —
  tech50war-200 (2026-08-10) is the prerequisite venue; contested@150
  produced 0 eliminations in 16/16 games and cannot see this.

Usual rules: struct tuning + params/blocks registration, tune-audit
CLEAN, counters on every new path, one commit one thing, off-switch
proof 10/10, A/B with pasted numbers.
