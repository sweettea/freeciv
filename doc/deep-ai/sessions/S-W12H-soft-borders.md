# Session brief — W12 slice: risk-priced permeable corridors

Two user-driven design moves, one mechanism: stop treating borders
and coasts as hard constraints in corridor planning and price them
as risk instead. Read the W12c buildability findings (STATUS, merge
919e7f099c), the funnel + W12g entries, and deeptrunk.c/deepsettler.c
first. Corridors are Europe-flavour (benchmark verdict: null-to-
negative, family off by default) — this slice's bar is mechanism
truth and liveness, not benchmark score.

## The mechanism (gated; default off = today's hard clipping)

1. **Soft borders**: the corridor planner and the corridor leg of
   the tile-behavior filter may traverse foreign-OWNED tiles where
   the rules allow the walk and the build. Verify legality per
   treaty state FIRST (players_non_invade, movement.c civilian
   exemption, any incident machinery) and encode exactly what is
   legal: allied (already in), peace partners (the new case), war
   enemies (stay ignored). Cite the rules code in the commit body.
2. **Risk-priced edges**: corridor edge cost gains a risk term —
   danger by distance from foreign cities (near their cities =
   expensive, empty borderland = cheap) plus the coastal naval
   exposure term (adjacent-to-ocean tiles priced by the naval
   threat model in the ship era). REUSE deep's existing danger/
   threat maps and fear scales (boat_shadow_fear and the overseas
   menace machinery for the naval side; the city-threat map the
   idle-policy recycle already reuses for the land side). Do not
   build a second risk model; new weights are struct tuning knobs,
   PLACEHOLDER where no measured value exists, unit-case where the
   term is denominated in an existing currency.

## Counters and proof

- Counters: corridor tiles planned/built on foreign-owned tiles (by
  owner treaty state), risk cost applied per corridor, and worker
  losses while on corridor duty split by tile ownership and coastal
  adjacency — the last one feeds the standing exposure question in
  the employment epic.
- Byte-identity at defaults 10/10 incl. one mixed cell.
- Liveness: a venue where peace/alliance borders exist (lategame-80
  forms alliances; check whether peace treaties occur on corridor-150
  mixed) — show a corridor tile planned and a road built on a
  peace-partner-owned tile, savegame-verified (NATIVE vs MAP coords,
  NATIVE_TO_MAP_POS), plus the risk term visibly rerouting at least
  one corridor away from a city/coast vs the gate-off plan (dump both
  plans for one seed and diff them).
- **Meet-in-the-middle check** (user question, answer it with data):
  two allied deep players plan the same link independently; locality
  builds each half from its own end and already-roaded tiles are
  skipped (funnel: `roaded` drops are not a leak). Under fog their
  paths may misalign and never meet. In the liveness smoke, dump both
  allies' corridor plans for their shared link and report tile
  overlap; count built-but-parallel (misaligned) tiles. Coordination
  must stay via observable game state only — no out-of-band shared
  state between module instances (honest-information rule). If
  misalignment is material, RECOMMEND a same-information tie-break
  (e.g. prefer the lexicographic-min equal-cost path) — do not
  implement it in this slice.
- Score A/B: OPTIONAL. If the box is free, one marginal corridor-150
  A/B (family+W12g on, one new gate apart, 16 seeds) is welcome;
  a flavour verdict without it is acceptable. Do not run more than
  one.
- Europe rider: gates on in derby5/tuning-derby7.ini per the dedupe
  rules — this slice exists for that game.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare before any pin), ports 7500-7549, jobs
<=4 (the employment probe owns 7300-7349). Tracked background dies
~60 min (DONE-marker resume); verify launches by scorelog growth.
Games are byte-deterministic in (config, seed, module, tuning). One
commit one thing; meson test all suites; knobcensus + tune-audit
CLEAN at the new count; STATUS with pasted numbers; stop after the
liveness proof (or the optional A/B).
