# W35 — Espionage & intelligence

**Phase 3–4, ai/deep. NEEDS W04.** Full design: `findings/espionage.md`.
Root causes: RC4 (spy actions on a private scale), RC6 (espionage IS the mechanism
for info the AI currently gets free).

## The problem — a priority ladder disguised as a valuation
`dai_action_value_unit_vs_city` (`daiactions.c:96`) decides spy actions off fixed
constants (its own FIXME admits they're arbitrary/unscaled):
Embassy 10000, StealTech 9000, Incite 7000, Sabotage 6000, StealGold 4000,
Plague 2500. Steal-tech is 9000 whether the tech is a game-changer or a footnote;
incite is 7000 whether the city is a backwater or the capital. The AI PERFORMS
espionage; it doesn't REASON about it.

## The deep point
An embassy is the ONLY honest way to know enemy tech/treasury/government. Under W04
(honest info), espionage becomes the AI's PRIMARY INTELLIGENCE APPARATUS. W04 = what
the AI knows; W35 = how it finds out and what it does with knowing. Same subsystem.

## Design
Value every spy action in the W1 currency, scaled by real stakes:
- Embassy = intel staleness × decision-dependence (the W04 recon-want formula).
- Steal tech = value of THAT tech to my plan (W34); targeted picks what I need.
- Incite = city worth (W2 scoring) − cost, VS the cost of storming it.
- Sabotage = target's ACTUAL production/buildings (a near-done wonder, not a temple).
Then espionage as operations (W33): maintain embassies with exposed rivals;
pre-assault intel sweep; **incite vs assault as competing plans for the same city**;
counter-intel in wonder-cities. Sabotage a rival spaceship (today the AI only
THREATENS war over spaceships, `daidiplomacy.c:1495`).

## Acceptance
Tech-steal priority tracks the AI's own tech needs; embassy established before an
assault commits; incite chosen when cheaper than storming (and vice versa);
sabotage targets a near-complete wonder; counter-spies defend wonder-cities;
metric: tech gap closed by theft in a race game vs baseline.

## Commits
`findings/espionage.md` §8.
