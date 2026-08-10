# Deep's head-to-head edge inverts at tech-50 (2026-08-10)

Six calibration games on the new `tech50war-200` venue (contested
geometry, techlevel 50, T200), principled-v1, mixed seat-mirrored
deep-vs-stock, pinned binary `9275e9bb6d`. Cost: ~19 min. Result: the
strongest known gap in the project.

## The numbers

Early-era contested (holdout, 20 seeds, same day): deep **+55.3**/seed
mixed — deep beats stock head-to-head decisively.

Tech-50 contested-geometry (3 seeds, mirrored): deep **−115.7**/seed,
1–5 in games:

| game | deep score / cities | stock score / cities | winner |
|---|---|---|---|
| bd/s1 | 616 / 9  | 885 / 27 | stock (blowout) |
| bd/s2 | 780 / 20 | 955 / 25 | stock |
| bd/s3 | 730 / 17 | 970 / 26 | stock |
| db/s1 | 1043 / 27 | 634 / 9 | deep (blowout) |
| db/s2 | 849 / 21 | 880 / 24 | stock |
| db/s3 | 826 / 19 | 867 / 21 | stock |

Totals: cities 113 v 132; standing army at end 296 v 455; kill
exchange deep 1007 killed / 1431 lost vs stock 1860 killed / 1684
lost. Deep is out-expanded, out-armed, AND loses the attrition war
roughly 2:1 by kill rate.

Paired form of the same venue (2 seeds): deep worlds −172/game vs
stock worlds — consistent, not an accounting artifact: deep really is
weaker here.

## Why this was invisible until today

Every combat venue in the canon starts at tech 0 and ends by
Gunpowder–Sanitation (M4a's era-reachability table). Every mechanism
epic validated on those venues; every derby ran on them. The entire
observed history of deep-vs-stock combat is Warriors-through-Knights.
The first six games ever played at Gunpowder+ inverted the sign.

Seat-1 position note: both blowouts are seed-1's seat-1 (whoever
holds it wins huge) — seat effects again dwarf per-game noise;
only mirrored numbers are meaningful.

## Hypotheses (untested — instrument before fixing)

1. **Expansion-phase fear miscalibration.** Deep founds fewer cities
   (113 v 132; 9 v 27 in the blowouts). Fear/menace constants were
   priced against early-era unit power; tech-50 units are ~3-5x
   stronger, so every fear term is inflated ~proportionally — settler
   risk, birth guard, garrison floors may all be overshooting,
   taxing the expansion race stock simply wins.
2. **Unit-choice pricing at Gunpowder+.** Wants for Musketeers /
   Cannon / Dragoons flow through the same amortized machinery, but
   no A/B has ever exercised them; stock's hand heuristics have been
   hardened here for decades.
3. **Research near-frozen** (50 → ~56 techs in 200 turns): deep's
   tech-want machinery (H9, techwall, WRACE) has nothing to do; any
   deep advantage in research direction is neutralized on this venue.
4. **Killstack/rotation dynamics with strong units** — melt/rotation
   cadences assume early-era HP/firepower ratios.

## Consequences

- The MIL tuning finale CANNOT run only on early-era venues: it would
  tune a fighter that collapses the moment the game reaches muskets.
  tech50war-200 (or a sharpened variant) must be in the finale blend.
- An ERA diagnosis epic belongs in the M-queue BEFORE NETTHREAT-class
  refinements: mechanism-level forensics on these six saves (expansion
  tempo turn-by-turn, unit composition, war timing, fear-term logs)
  to locate which subsystem inverts. The saves are under
  `benchmark/results/tuning/calib2-tech50war-mixed/`.
- The human's 2026-08-10 instinct ("can we do a tech level 50 scenario
  to compensate?") is validated in the strongest possible way: the
  venue found a sign inversion, six games in.
