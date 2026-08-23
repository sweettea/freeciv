# Session brief — poxiness: a personality axis for settlement spacing

Live-play finding (STATUS 2026-08-23, the smallpox table): with the
tier gate live on classic, every deep seat packs cities at the legal
citymindist floor (median NN distance 2.0-2.2, ~100% catchment
overlap, median size 2), while the human plays spacing 5 / size 8-12.
The user wants poxiness to be a PERSONALITY, not a constant: the
EXPANSIONIST trait already exists, deepstance.c already seeds
econ/mil axes from traits (deep-stance-load), and the spacing levers
exist unconnected. Read deepstance.c, deepsite.c, and the smallpox
STATUS entry first. Reference data: the user's game save
play-0822-2253/saves/freeciv-T0110-*.zst decodes to the per-seat
spacing table (parser logic in STATUS).

## The mechanism (gated)

1. **A spacing preference term in site evaluation** (deepsite.c):
   price catchment overlap with the founder's EXISTING cities —
   overlap tiles shared with own catchments charge a penalty scaled
   by the new gain. Reuse the radius/catchment math the site
   evaluator already has; no second geometry model. (Rival crowding
   is already priced by site_rival_crowding — this is OWN-empire
   crowding, which the table shows is the unpriced one.)
2. **Personality coupling**: the penalty's strength scales with the
   player's expansion stance axis (from TRAIT_EXPANSIONIST via the
   existing stance machinery — read how econ/mil axes are seeded and
   add or reuse an expand axis; deep-stance-load should log it).
   High expansionist = weak penalty (smallpox tolerated), low = strong
   (largepox). Gate + gain knobs in struct tuning; the gain's on-value
   must be DERIVED (e.g. from the observed want margins in the user's
   save: the penalty needed so a spacing-2 site drops below a
   spacing-4 alternative at median site quality) — show the
   arithmetic in STATUS; PLACEHOLDER only if no derivation exists.

## Proof

- Identity 10/10 at defaults incl. one mixed cell.
- Liveness: a scripted or short venue with traitdistribution EVEN —
  show per-seat NN spacing DIVERGING by trait draw (the counter:
  per-player founded-site NN distance census at game end, plus the
  stance axis in deep-stance-load). Two seats with opposite
  expansionist draws must produce measurably different spacing.
- Benchmark A/B optional (one 16-seed cell if the box is free);
  Europe rider mandatory: gate on in derby5/tuning-derby7.ini per
  dedupe rules — the user's next game is the acceptance test.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare). Ports 7400-7449, jobs <=3 (W29 on
7650s, chatty2 on 7600s, employment may return on 7300s). Batch
drivers at benchmark/*.sh launched detached from repo root (the
sandbox exclusion needs that exact shape; no cd-prefixed compounds
— if blocked, tracked background + DONE resume). DONE markers must
record SUCCESS, not completion (the tier-fix lesson). Power-loss
sweep on inherited dirs. Games byte-deterministic. Tuning inis:
never append an existing key; awk-dedupe. tune-audit CLEAN at the
new count; knobcensus; one commit one thing; meson test all suites;
STATUS with the spacing-divergence table; stop after liveness (or
the optional A/B).
