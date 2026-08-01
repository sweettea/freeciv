# Europe playtest, T76: deep vs classic in one live game

Source: the human's Europe autosave `freeciv-T0076-Y-0625-auto.sav.zst`
(2026-08-01). Six `ai_type="deep"` players, seven `ai_type="classic"`,
one barbarian and one dead pirate excluded. Mixed-brain games persist
their module per player, so this is a direct read of the two AIs in
the same world, on the same map, at the same instant -- something no
paired benchmark can give.

## The scoreboard, per player

| metric | deep (n=6) | classic (n=7) |
|---|---|---|
| cities | 12.7 | 14.3 |
| population | 77.7 | 75.0 |
| avg city size | **6.13** | 5.25 |
| techs | 54.5 | 54.4 |
| units | 30.0 | 23.1 |
| of which military | 14.3 | 9.1 |
| workers + engineers | 10.7 | 10.6 |
| diplomats | **4.2** | 2.0 |
| gold | **166** | 603 |
| ladder buildings per city | **0.00** | 0.44 |

**This is a far better showing than the contested benchmark cell
suggests.** Deep is at parity on the two things that decide score --
population per player and techs -- and its cities are meaningfully
BIGGER (6.13 vs 5.25). The gaps are elsewhere: fewer cities, 57% more
military, a quarter of the treasury, and no ordinary buildings.

## 1. Expansion has stopped, and it is a want problem

Zero settlers alive and zero queued across all six deep players.
Classic: four alive, eight queued. The savegame stores each city's
founder want per module, so the cause is visible directly:

    cities whose founder want > 0      deep 9/76 (12%)   classic 41/100 (41%)
    strongest want observed            deep ~545         classic up to 1105

Two classic players (Poniatowski, Hunyadi) want to found from *every
single city* (13/13 and 9/9, minimum want 610 and 857). No deep player
exceeds 3 cities wanting it.

So deep is not failing to *execute* expansion; it is failing to *want*
it. That points at site evaluation (W02) or at what the settler's
journey is priced at (W05). A mechanism worth testing first:
`settler_fearfulness_mult` (PLACEHOLDER 2.0) doubles the advisors'
stacking fearfulness on every settler path, and `amortize()` de-ranks
sites behind expensive paths with no special-casing. On a crowded,
war-heavy Europe that could suppress essentially every site at once --
which is the observed pattern (want present but rare and weak, rather
than uniformly zero).

## 2. Deep builds wonders and skips the ladder

Deep's 76 cities hold **zero** Temples, Marketplaces, Libraries,
Granaries, Aqueducts or Colosseums. What they do hold: Barracks II 29,
City Walls 13, Supermarket 3, Port Facility 2, plus Darwin's Voyage
and Copernicus' Observatory. Classic's 100 cities hold 44 ladder
buildings (Granary 7, Temple 6, Aqueduct 6, Cathedral 6, Colosseum 6,
Marketplace 5, Harbour 5, Library 4, Sewer 4) and *more* barracks per
city than deep (0.57 vs 0.38).

Note the nuance against the earlier benchmark reading: deep is not
losing because it drowns in barracks -- classic builds more of those.
Deep simply never buys the ordinary economic ladder, and buys wonders
instead. Its cities are bigger anyway, so the growth path is working
through citizen management rather than through buildings.

## 3. The treasury gap is cash flow, not policy

Tax rates are near-identical (both mostly 10/0/90 tax/lux/sci), so the
4x gold gap is not a rates decision. It is 57% more military units to
upkeep and no Marketplaces to widen the take. One deep player (Henri
IV) is running 70% luxury, which is a distress signal worth a look.

## 4. Diplomats at 2x

4.2 per deep player against 2.0 for classic. `diplomat_defence_value`
is a PLACEHOLDER (300, "corpus-derived"), and this is the second time
the count has been flagged (STATUS, isolated cell watch item).

## What was changed as a result

Nothing was hand-tuned: rule 7 says constants are set by a tuning pass
against training seeds, not by picking numbers that look reasonable
after reading one game. What this save DID change is which constants
the campaign can reach at all. The first draft of `tuning-blocks.ini`
covered 35 of 81 knobs, and the 46 it missed included
`settler_fearfulness_mult`, `settler_risk_tolerance`,
`diplomat_defence_value`, `upkeep_gold_weight`, `upkeep_unit_weight`
and most site weights -- i.e. every knob this forensics implicates.
Blocks are now S (the split, 20), X (expansion, 16), G (growth, 26),
M (war, 19): 81 of 81, each knob in exactly one block.

## What to test first, in order

1. **Expansion.** One `--blocks X` pass, or faster, a single-knob
   sweep of `settler_fearfulness_mult` on a crowded cell, watching
   founder-want prevalence rather than score. If prevalence moves,
   that is the mechanism; if not, the suppression is in site scoring
   and W02 owns it.
2. **The ladder.** Whether zero Temples/Marketplaces costs anything is
   genuinely open here -- deep matched classic on techs and beat it on
   city size without them. Measure before assuming it is a defect.
3. **Upkeep.** `--blocks S` reaches the upkeep weights now; the gold
   gap is the metric to watch, not score.
