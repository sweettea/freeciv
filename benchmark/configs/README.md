# Configs: the canonical set and the attic

2026-08-06 (Fable pass; human directive: "we should not be biased by
what we have but get a good and usable set"). Designed from the axes a
well-balanced AI must master (`findings/tuning-venues-2026-08-06.md`),
then mapped onto files — not the other way around.

Why there were 36 files: probes that answered one question and stayed
(huts/hutfree, garden2, scale-20p-s3/s6), supersessions that kept their
ancestors (garden2b > garden2 > garden), blanket `-classic` twins made
for every cell when only some blocks need them, and `-traits` variants
for a standing-call probe. The content-addressed cache makes stale
configs harmless, so nothing was ever garbage-collected. Harmless is not
the same as usable; hence the tiers below. Files are kept for cache and
historical comparability — the tier is the statement of intent.

## CANONICAL — the v4 portfolio (claims may cite these)

| cell | world | stresses |
|---|---|---|
| `lategame-80` | 6p 40x40, T80, migration | short-horizon efficiency |
| `primary-120` | 6p fractal 400t/p, T120, migration | roomy continental growth |
| `isolated-150` | 4p 56x56 islands, T150, migration | naval economy, lifts |
| `contested` | 8p 32x32, T150 | crowded early war |
| `garden2b` | 2p fair islands, T300, barbs off | pure development race, naval separation |
| `isolated-classic` | trade-family twin | caravan/route block (civ2civ3 zeroes domestic routes) |
| `storm-150` | TO CREATE: 8p 40x40 classic, raging barbs, onset 30, T150 | floors/terror/reconquest; the a6-disarmament antidote |
| `mixed-8p-150` | TO CREATE: 4 deep + 4 stock, one table, civ2civ3 T150 | statecraft vs the real opponent |
| `mixed-8p-classic` | TO CREATE: same, classic | trade + war vs stock at table |
| `longhaul-300` | TO CREATE: 6p fractal civ2civ3, T300 | culture/endgame/wonder horizon (cloud only) |

## ERA — specialist venues, cited only for what they were built to see

Each buys one era or one geometry the canonical portfolio cannot reach,
by paying for it somewhere else. Read the provenance comment in the file
before citing one; each names its own known limits.

| cell | world | built for | pays with |
|---|---|---|---|
| `tech50war-200` | 8p 32x32 T200, techlevel 50 | Gunpowder-era war; found the head-to-head sign inversion (`findings/tech50-collapse-2026-08-10.md`) | frozen research |
| `tech80air-200` | 8p 32x32 T200, techlevel 80, `techlossforgiveness -1` | Flight/Espionage era: W32 air doctrine, SAM/SDI, W35 Spy slices | research is 80 of 87 advances at turn 1 — degenerate by design |

## HOLDOUT — evaluated, never tuned on

- `europe-eval` (TO FORMALIZE from the derby kit): real-map asymmetric
  7v7, both rulesets. Generalization to worlds nobody generated.
- `wildcard` (TO CREATE): per-seed randomized generator/landmass/
  players/barbarian rate from a fixed distribution.

## TOOLS — development instruments, never cited as results

- `smoke` (+`smoke-traits`): the 5-minute directional check.
- `scale-20p` (+s3/s6): performance/latency probes.

## ATTIC — answered their question; do not use for new work

`garden`, `garden2`, `garden-classic`, `garden2-classic` (superseded by
garden2b), `primary`, `primary-classic`, `primary-island*`,
`primary-single*` (superseded by primary-120), `lategame`,
`lategame-classic` (superseded by lategame-80), `long`, `long-classic`
(superseded by the longhaul-300 design), `isolated` (superseded by
isolated-150), `contested-classic`, `cramped`, `cramped-classic`
(contested covers the axis), `huts`/`hutfree` (the hut-risk probe,
answered), `isolated-150-traits` (standing-call probe, answered),
`largepox-200`, `scale-4p`.

An attic config that turns out to be needed gets PROMOTED by editing
this file in the same commit that uses it — the tier list stays true.
