# Session brief — upgrade the trunk: rails where roads already run

From the user's Europe requests and the QUEUE rail-upgrade entry: once
an international trunk road exists, deep should upgrade it to better
road types as tech allows ("perfect. and then we'll also want to
upgrade to better road types if we've already built an international
road"). The queue names the defect precisely: the corridor machinery's
notion of a gap is UNROADED tiles only — a tile carrying a road when
railroad is buildable is "done" and never revisited, so trunks fossilise
at the tech level they were built at.

## The mechanism (gated)

1. **Redefine the gap** (deepterra.c, the trunk/corridor machinery):
   a corridor tile is incomplete when its best extra is below the best
   BUILDABLE extra for the player (road < railroad < whatever the
   ruleset offers), not merely when it is unroaded. The existing
   corridor walk, claim, and stacking logic then does the rest —
   extend the gap predicate, do not fork the walk.
2. **Value the upgrade honestly**: the trunk gain terms
   (trunk_road_gain, trunk_trade_gain, trunk_source_gain) already
   price a corridor tile; the upgrade's want should flow from the
   move-cost/trade delta of the better extra through the SAME terms,
   scaled by what the tile's current extra already delivers (an
   upgrade is worth the difference, not the whole). Derive, show
   arithmetic; no new magic numbers.
3. Gate in struct tuning (e.g. trunk_upgrade_gate), default off.
   Mind priorities: upgrades must not starve unbuilt corridor gaps —
   state in STATUS how the ordering falls out of the existing want
   comparison (or gate upgrades behind no-open-gaps if that is what
   the code naturally does; say which).

## Proof

- Identity 10/10 at defaults (gate off), incl. one mixed cell.
- Liveness: corridor-150 (or a venue where railroad tech arrives; if
  corridor-150 games end pre-rail, say so and pick/extend the venue
  accordingly — check tech pace first, do not discover this after the
  batch): show deep-trunk log lines claiming upgrade jobs on tiles
  that already carry roads; paste counts per game.
- Score A/B: 16 training seeds on the chosen venue, gate off vs on,
  one pinned binary. Paste per-seed pairs and mean.
- Europe rider: gate on in derby5/tuning-derby7.ini per dedupe rules —
  the user's next game should visibly upgrade its trunks.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare before pin). Ports 7650-7699, jobs <=3
(poach-fix owns 7300s+7400s, G-tune owns 7500s, site epic owns
7600s). Batch drivers at benchmark/*.sh launched detached from the
worktree root as `setsid bash ./benchmark/<script>.sh go` (exact
shape). DONE markers record SUCCESS. Power-loss sweep on inherited
dirs. Never append an existing ini key; awk-dedupe. tune-audit CLEAN
at the new count; knobcensus. One commit one thing; meson test all
suites; STATUS with pasted numbers; standard trailer; do NOT merge —
stop after the STATUS commit on your branch. If SINGLE_MOVE units
appear in your corridor path code (the known July bug at
infracache.c:237 is live in classic), note interactions in STATUS but
do not fix ai/default or common code beyond your own module.
