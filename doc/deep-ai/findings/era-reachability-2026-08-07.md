# How far up the tech tree do our cells actually get? (M4a, 2026-08-07)

**One line: only `garden2b` reaches Flight, in 11 of 24 games, and it is
the 2-player development race — the cell with the least air combat in
it. Nothing else in the portfolio reaches Flight at all. W32 (air
doctrine) has no venue, and neither does any W35 slice that needs a
Spy.**

Tooling: `benchmark/era-check.py`, which reads archived final savegames
(`saves/freeciv-T*-final.sav.zst`), parses the per-research bitstring in
the `r={...}` block, and maps the bits back through the ruleset's own
advance order. `metrics.csv` has always carried a `techs` COUNT; nothing
recorded WHICH, and a count cannot answer a reachability question.

## The table

Newest archived result dir per canonical cell (configs/README.md), all
arms and all seeds in that dir. `techs` is the per-player mean at the
final turn; `max` the best single player; the landmark column counts
GAMES in which any player knew the advance.

| cell | T | games | techs mean | max | highest landmark | games reaching it |
|---|---|---|---|---|---|---|
| `smoke` | 101 | 40 | 15.1 | 24 | Gunpowder | 2/10 |
| `primary-120` | 121 | 6 | 17.3 | 22 | Gunpowder | 1/1 |
| `contested` | 151 | 160 | 17.9 | 29 | Sanitation | 1/20 |
| `isolated-150` | 151 | 82 | 29.2 | 43 | Sanitation | 16/20 |
| `lategame-80` | 81 | 120 | 53.0 | 57 | Espionage | 1/20 |
| `garden2b` | 301 | 51 | 53.3 | 83 | **Space Flight** | 2/24 |

`isolated-classic` is canonical but had no archive; measured fresh (see
below).

Full ladder where it matters:

    lategame-80  Bronze Working 20/20 ... Electricity 20/20,
                 The Corporation 8/20, Espionage 1/20. No Flight.
    garden2b     Sanitation 22/24, Railroad 23/24, Industrialization
                 22/24, Electricity 23/24, The Corporation 14/24,
                 Espionage 7/24, Automobile 5/24, FLIGHT 11/24,
                 Advanced Flight 5/24, Rocketry 5/24, Computers 3/24,
                 Space Flight 2/24
    contested    Gunpowder 16/20, Sanitation 1/20. Nothing above.
    isolated-150 Gunpowder 20/20, Sanitation 16/20. Nothing above.
    smoke        Monarchy 10/10, Gunpowder 2/10.

## What this gates

**W32 (air doctrine): PARKED, with evidence.** Flight exists in exactly
one cell, at 11/24 games, and that cell is two players on separate
islands with barbarians off — a development race whose military content
is close to zero. Building an air doctrine to be measured there means
tuning a combat mechanism in the venue least able to see combat. The
verdict inherits its venue (NEXT-SESSION §6), and this venue says
nothing. Air work is post-v4 unless a cell is built FOR it, and building
that cell is the prerequisite, not the doctrine.

**W35 beyond E35.1: gated the same way.** Espionage — the advance that
turns a Diplomat into a Spy — is reached in 7/24 garden2b games and
1/20 lategame-80 games. Every W35 design element that assumes a Spy
(escape variants, targeted sabotage as the normal case,
counter-intelligence in wonder cities) is priced for a unit the
portfolio almost never fields. E35.1 was scoped to Diplomat-era actions
and that turns out to be the only scope the cells can measure.

**The endgame cells are the only place the top half of the tree
exists.** `garden2b` (T300) and `longhaul-300` (TO CREATE) are it.
Anything that needs Industrialization or above — Automobile-era units,
spaceship parts, Computers-gated buildings — is a garden2b-only claim
today, on 24 games, from one map shape.

**A cheaper reading of the same fact:** four of the six measured cells
end in the Gunpowder-to-Sanitation band, i.e. medieval-to-early-
industrial. That is where the mechanism backlog should be aimed. It also
explains why several W32/W35-adjacent mechanisms have measured as "no
effect": they were never reachable.

## Method notes

- **A count is not a name.** `metrics.csv:techs` has been in every
  result dir since the harness was written and could never have
  answered this: 29 techs on `contested` and 29 techs on `garden2b` are
  different halves of the tree.
- **Report the per-GAME share, not "any player ever".** The first cut of
  this table said garden2b "reaches Space Flight", which is true of 2
  games in 24. Both numbers are in the table now.
- **`?tech:` qualifiers.** civ2civ3 writes `name = _("?tech:Railroad")`
  for names that collide with an improvement; a parser that takes the
  string literally silently loses that advance. The first run of this
  script reported Railroad at 0/6 cells while reporting Industrialization
  — which requires it — at 2. An impossible pair in the output is worth
  more than a plausible one.

## Reproducing

    benchmark/era-check.py --results benchmark/results
    benchmark/era-check.py --results ... --cells garden2b --ruleset classic

Reads only; runs nothing.
