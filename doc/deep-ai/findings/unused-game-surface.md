# Unused game surface — what the game offers that no AI touches

2026-08-05. Synthesis of the three-reader sweep in
`findings/deep-dive-2026-08-05/` (action-surface, economy-effects,
diplomacy-gamestate — full evidence and file:line citations live there).
Method note: every claim was filtered through the two lessons the arc keeps
re-learning — **check the ruleset enables it** (the Rapture_Grow precedent)
and **check our cells reach it** (era, settings, diplomacy states).

## The four session-shaped opportunities (human call 2026-08-05: ALL join
## v4's launch gates)

**ENDGAME — no AI knows the game ends.** Zero reads of `end_turn` anywhere
in `ai/` or advisors; MORT=24 amortizes identically on turn 5 and turn 195
of 200; 199/199 sampled games end on the turn limit; score is citizens ×1,
techs ×2, future techs ×5, great wonders ×5. A turns-remaining horizon and
a late-game score mode are farmable on every cell, and the metric is the
primary metric. MUST land pre-v4 (it changes what every amortized credit
means near the end) and must not be bundled with credit tuning.
Metric-gaming guard: the culture/History channel (civ2civ3 only) is
reported, not led with.

**PEACE — the cease-fire trap, the embassy absurdity, and the trade
population.** Measured: 20-42% of every pacified pair sticks at cease-fire
forever (4-turn peace window vs 3-6-turn proposal gate + best_target love
bleed at 50/turn vs the 125-love peace bar). The embassy swap is
arithmetically impossible from turn 2 (receive=10 gold flat, give=−5×turn)
in a ruleset where embassies ARE the tech leakage — deep pays diplomat
units for a free clause. And the trade chain: ceasefire/armistice/peace all
classify as the IN 100/200 route tier (`traderoutes.c:58-83`), stock's
foreign-route want is a provable dead branch on civ2civ3 (the guard at
`daidomestic.c:283` compares two NATIONAL percentages), deep's R2 merit
path doesn't share the bug — so a deep-side peace-window pre-pass + embassy
re-pricing (both in deep's own wrappers, zero `ai/default` changes) turns
20-42% of pairs into durable trade partners. Census scripts already exist.

**DARKFX — the dark-effects round.** 39 of 107 ruleset-present effects are
valued at zero by everything; the money items: `EFT_GOV_CENTER` (the
Ecclesiastical Palace's distance-waste relief, invisible because base_want
sees one city and the trigger switch omits it — a 12-tiles-out Republic
city loses 49% of trade), `EFT_UNIT_UPKEEP_FREE_PER_CITY` (King Richard's
+ Statue of Zeus: 40 gold + 20 shields/turn at 20 cities, priced today at
HISTORY×5), `EFT_MAX_RATES` (governments' rate ceilings unpriced — pairs
with deeprates), Palace relocation (impossible on merit; random on
capture), classic-UN (its only effect dark ⇒ literally zero want).
Fix venue: deep-side credits beside the horizon ladder, same shape.
Sub-items: buy-on-gold-per-shield (quadratic top-ups), specialist type as
a decision (both CM factors pinned at 2 today), building-induced disaster
risk (civ2civ3 runs disasters at 10×; Mfg/Nuclear Plant buy unpriced pop
loss; nothing anywhere reads check_disasters).

**GROUND — the terrain-value round.** Fortresses have never been built by
any AI ever (the only base valuation reads tile output; a base changes
none — 0 in 175 cities); pillage was left out with an in-code admission
("We could have ACTRES_PILLAGE here, but currently we don't") — 664
undefended enemy roads at T162; capture-units on civ2civ3 is turn-1 and
unplayed in BOTH directions (nothing hunts capturable caravans; nothing
protects ours); fortify is a fallback, never a chokepoint decision. All
one family: a want for ground value / yield denial that isn't tile output.
Capture gets instrumentation before mechanism.

## Facts and negative results (record, do not build)

- **The flight era is unreachable in every measured cell** (T80-T151 = 16-29
  techs; Flight=50). W32 air, paradrop, airlift, nukes, most of W35: parked
  until a long-era cell exists. STATUS's 2026-07-30 question is answered.
- **Worklists / the switch-penalty exemption** — the AI is exempt from the
  production-switch penalty at hard (`citytools.c:3221` + no
  H_PRODCHGPEN), and stock docks wants for it anyway. **Human call
  2026-08-05: the exemption itself is wrong — deep self-imposes
  H_PRODCHGPEN (HEDGE session), the dock becomes true pricing, and the
  hedge's penalty leg revives with a real population.** Expect the
  imposition to tax endemic churn before discipline catches up; that is
  the finding, not a reason to revert. Queue-ahead worklists remain
  low-value while builds are re-chosen each turn.
- Spaceship / pollution / global warming / civil war / rally points:
  empirically dead in our cells. Migration ON in 5 cells, unmodelled (W17).
- Tech-for-gold is structurally unavailable; tech trading is alliance-only;
  MAP/SEAMAP/VISION receive-side valued 0 BY omniscience (W04 checklist:
  an honest AI must not refuse free maps).
- Barbarian ransom/hut-EV/Storm handling: real, correctness-sized;
  upstream-bugs appendix, no session.
- `tech_leader`/`production_leader`: computed every turn, read by nothing —
  joins WIN_CAPITAL in the free-third-party-signals slice (PEACE or M4).

## Upstream-bug candidates surfaced (to append to upstream-bugs.md)

Action sweep: bombard priced as a kill; base extras structurally
unvaluable; EFT_AIRLIFT want blocks Airports. Economy sweep: the
civ2civ3 trade-route dead branch (`daidomestic.c:283`); daieffects'
covered-list wrong for 8 of 25; `allow_specialists` read by nothing.
Diplomacy sweep: war-target clobber (`daidiplomacy.c:1839`); hard AI can
never open a cease-fire; alliances can never dissolve (dead ally_patience);
tech/production leader can name a dead player. Deep-side doc fix:
"cm_parameter never set by the AI" → "never set by ai/default".

## Ladder placement (post-M3 order, all pre-v4 per the human call)

G1 → D3 → R2b → HEDGE-census → R3 → M1/E4 → **PEACE → ENDGAME → DARKFX →
GROUND** → INSTR (last: its census rerun captures all new knobs) →
separability pilot → freeze → v4.
