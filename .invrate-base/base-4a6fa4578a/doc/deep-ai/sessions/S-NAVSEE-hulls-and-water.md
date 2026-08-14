# Session brief — NAVSEE: seeing hulls, grading water (pre-v4, small)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md`
§5, the G1 and D3 STATUS tails (the Trireme find, the nativity fix
`988f284760`, the naval-gate isolation and its yield-vs-connectivity
resolution), and `findings/deep-dive-2026-08-04/military.md` §1a (F2/F3).

## THE JOB — two halves, one domain

**A. See the hulls (G1's highest-value unfixed find).** civ2civ3's
Trireme reads as land move-type, so every `UMT_SEA` test misses the
ancient era's primary transport. Generalize G1's terrain-nativity float
test ("can this unit exist on ocean") to the four blind sites:
`deep_boat_shadow_fear` (F2), `deep_density_fear`'s floats gate (F3),
`deep_reachable_free_land`, and the ferry census. One shared helper, one
counter each. **A/B at n=40** (the D3 bar): isolated-150 + lategame-80 —
F2/F3 finally firing in the trireme era may re-price part of what G1
measured as floor tax; that interaction is the session's headline
question. Riders: G1's class-fact lessons (Animal Kingdom capability
faking; the USP_NATIVE_TILE marines test) folded into the shared helper's
tests.

**B. Grade the water (human design, 2026-08-05).** Split the maritime
site premium into two terms where stock had one 20% WAG and D3's gate
tested only the veto form:
- `site_naval_weight` — coastal YIELD, any workable water (a dead lake
  still feeds a Harbour); carries most of stock's 20.
- `site_naval_connect_weight` — NEW, additive connectivity option value,
  paid only where `deep_water_bridges` says the water leads somewhere
  (IC trade tier, lift launchpoint, useful hulls). A city on a useful
  lake IS slightly more useful than one on a dead lake.
Split ratio: PLACEHOLDERs for the pile; defaults sum to today's 20 with
connect small (state the split and its rationale — D3's isolation showed
veto −11.68 at t −1.46, which also needs the n=40 rerun this session's
batches provide nearly free). The existing refusal census already counts
the population the connect term prices.

## Acceptance

- Nativity helper unit-tested (the deepodds test pattern); per-site
  counters showing F2/F3/free-land/census now seeing Triremes (before:
  provably zero — M3's density census found 0 transport-owners priced).
- The n=40 A/Bs with the floor-interaction question answered explicitly
  (does honest F2 shrink the floor's residual value?).
- Premium split: byte-identity when connect=0 and yield=20 (today's
  shape); the graded A/B on isolated-150.
- `meson test` (4 with the new unit test) + `tune-audit.py` CLEAN at
  counts; STATUS final; sessions/ untouched.

## Scope fences

4-6 commits. No fear-magnitude tuning (pile), no site-scoring redesign
beyond the split, no ferry-system behaviour changes (detection only —
the census sees hulls; nothing new sails).

## Stop conditions

Nativity semantics differ per ruleset in a way one helper can't express
(report); the F2-live A/B regresses beyond noise without attribution
(the floor interaction is the expected explanation — measure it, don't
assume it).
