# Where deep's defaults deficit lives (2026-08-25 log census)

Data: the s5/s6 holdout arms of `benchmark/results/tune-postfix/`
(gates-off `defaults.ini` unless noted, 20 held-out seeds per venue,
pinned post-fix binary; see tune-postfix-verdict-2026-08-25.md for the
arm-level numbers). Everything below is from the retained scorelogs
and compressed game.logs — no new games.

## The phenomenon is a late fade, not a weak opening

Mean score gap (deep − baseline), gates-off:

- contested: −4 at T31, **+15 at T76**, −25 at T106, −58 at T151
- lategame-80: **+17 at T25**, −1 at T49, −58 at T81

Deep leads both venues through the early midgame — its science output
*leads* baseline until ~T90 contested (89.9 vs 62.7 at T76) — then the
gap opens and compounds. End-state ratios (deep/base, contested):
cities 0.88, pop 0.87, techout 0.81, culture 0.84, settledarea 0.88,
**settlers alive 0.08, gold 0.12**, munits 1.22.

## Localization: expansion stalls because settler production churns

The chain, each link instrumented:

1. **Cities plateau at ~T50** (contested: deep 23.6/23.6 parity at
   T46, then 29.6 vs 34.7 by T76, 37.0 vs 42.1 at end). Baseline
   keeps founding for ~40 more turns. In lategame-80, baseline
   *rebuilds* its settler stock to ~50 by T57; deep sits at ~5 all
   game.
2. **Not the expansion gate**: `deep-expgate` logs `blocked=0`
   essentially always, with live founder bids of 600–800 at T150.
   Intent survives to the end.
3. **Not production intent**: `deep-want pick="Settlers"` holds 15% of
   all picks from T50 through T100 (2nd most-picked item).
4. **The defect: starts ≈ abandonments.** `deep-prodswitch` into vs
   out of Settlers per 25-turn bucket (20 games): 487/529, 287/288,
   333/375, 99/153. Nearly every started settler is pre-empted before
   completion. Top pre-emptors T50+: Phalanx, Trireme, Colosseum,
   Diplomat, Workers, Barracks, City Walls.
5. **The churn is general**: ~24% of city-turns at T75+ carry a
   production switch; class-crossing switches burn ~1500 shields/game.
   (Unit→unit switches keep stock, so the settler loss is completion
   time, not shields.)

## Secondary signatures

- **Treasury pinned at ~250–450 gold all game vs baseline's ~2000**,
  with *identical* tax/sci/lux rates (42/58/0 at T46 both sides). The
  gold goes to spending — buys plus upkeep of 2× military and the
  building habit below — leaving no buy reserve. (Score does not count
  gold, but zero reserve means no late buy capacity.)
- **Early over-militarization**: munits 7.3 vs 1.0 at T16, 2× baseline
  most of the game (contested). The pre-emption pattern in (4) is the
  same force seen from the settler's side.
- **Colosseum = 23% of ALL picks at T100–125 with riots ≈ 0.1/game
  and luxrate 0.** That is preemptive content-building at enormous
  scale, not emergency response — suspicious given
  `horizon_content_weight` / W03 is unmerged work in this direction.

## Ranked hypotheses for the next epic (instrument before fixing)

1. **No production hysteresis**: re-bid each turn lets any transient
   danger/want spike pre-empt a part-built settler; with in≈out the
   pipeline never drains. Check: per-city switch sequences, count
   A→B→A cycles and settler completions vs starts directly.
2. **Danger flapping** drives the Phalanx/City Walls pre-emptions.
   Check: `deep-def`/`deep-density` menace series for oscillation
   around the garrison floor.
3. **Content/happiness overvaluation** (Colosseum spree with zero
   riots). Check: `deep-founderbid`-style census of building bids vs
   realized unhappiness.
4. **Military want overpriced early** (T16 munits 7×) — opportunity
   cost plus upkeep drag. Interacts with (1) as pre-emptor supply.

Sample sizes: 20 games/venue; per-seed paired SEs on the arm-level
diffs are ~15; the trajectory and census patterns above are
20-game aggregates and consistent across both venues.
