# S-PRICING-2 — founding as position (epic 2 of the pricing workstream)

Brief for one Opus subagent session. One epic, 5–10 commits, then STOP.
Read `CLAUDE.md`, `doc/deep-ai/00-context.md`,
`doc/deep-ai/workstreams/W37-pricing.md` (rows 2 and the beachhead
sections), `doc/deep-ai/sessions/S-PRICING-1.md` and the STATUS entries
"Design item from the Europe WILD game" and "threatened nation stops
expanding" (Turkey at T220: 11 of 13 cities closed by the expansion gate
at forecast survival 0.32, founder want 24–34 against a military want of
18,551, two cities founded in 60 turns; Italy 11 of 16 closed).

Branch from `epic/pricing-1` (the formulas in `ai/deep/deeppricing.*`
or wherever epic 1 put them; read its commits first) into a new worktree:
`git -C /home/sweettea/freeciv worktree add .worktrees/pricing2 -b
epic/pricing-2 epic/pricing-1`. Its build dir is fresh: `meson setup
build -Dbuildtype=debugoptimized` then `ninja -C build`. Pin to
`.worktrees/pricing2/.pricing2-bin/` (server + libfreeciv.so) before any
game. Ports 8070–8099. `chrt -i 0 nice -n 10`, ≤ 6 games at once; the
box also runs other agents' games.

## What to build, in order, each behind a gate defaulting OFF

1. **Site strategic value.** In deep's site evaluator (`deepsite.c`,
   `deep_cityresult_fill` and the bid it feeds) add a term, in the same
   currency as the yield value: Σ over enemy cities within reach of
   (ETA removed to that target by a city here) × (target worth) ×
   (odds gain from the shorter ETA, from the existing take-odds model),
   plus the worth of enemy candidate sites this site denies, minus the
   holding cost (expected losses from the wave model at the site). Gate
   `site_position_gate`; any scale that is not a ratio of existing
   prices is a `struct tuning` PLACEHOLDER with a registry line. Log
   `deep-siteposition turn= player= site= yield= position= hold= total=`.
2. **The expansion gate asks about the site.** Replace the source-city
   survival test (`deep-expgate`, survival < 0.90 blocks) with: found if
   P(hold the site over the horizon) × (site value incl. position) >
   settler cost + escort cost, where P(hold) comes from the wave/survival
   model evaluated AT the site. Keep the old test when the gate is off.
   Gate `expgate_site_gate`. Same log line, add `site_p_hold=` and
   `site_ev=`.
3. **Escorted founding as one bid.** When the site is contested (P(hold)
   below the identity for an unescorted founder), bid settler + defender
   together against the city's defence wants, so a forward city competes
   as "a held position there" rather than a settler alone that loses to
   the first-defender clamp (1100). The escort uses the per-shield
   defender chooser from epic 1. Gate `founder_escort_gate`. Log
   `deep-founderbid ... escort=<type> joint=<want>`.
4. **Decision replay tool**, `benchmark/replay.py SAVE --turns N --seeds
   K --tuning A.ini --tuning B.ini`: resumes a save under each tuning and
   K reseeds (run-game.sh START= / RESEED=1 / TUNING=), plays N turns,
   and prints per-tuning mean score, cities, gold and the europemeter
   deltas. Use it for the position check below. Small; reuse what exists.

Stop after 4 or when the budget is spent.

## Rules

Hard rules of CLAUDE.md. Nothing in `ai/default` or `ai/baseline`. Code
comments say what the code does, not what it used to do. One commit does
one thing; titles `ai/deep: <verb> <object>` or `benchmark: ...`. New
tunables: registry line + documented struct field + default that is
identity when the gate is off. Never run games from `build/`; never edit
`benchmark/run-game.sh` while games run; `git -C <abs path>` for every
git command, one branch op per Bash call; batches via
`run_in_background: true`, progress by files.

## Acceptance (paste numbers)

1. `meson test -C build` green, plus L0 tests: a site 3 tiles from an
   enemy capital outscores the same yield 20 tiles away when the gate is
   on and equals it when off; an escorted bid for a contested site
   exceeds the settler-alone bid; P(hold) of an interior site ≥ P(hold)
   of a border site.
2. Identity, gates off: contested seeds 1–4 and europe-late 121–122
   (self-play deep, `EUROPE_STARTS=/home/sweettea/freeciv/benchmark/results/scoreboard/run-ed04f89a54e5/europe-starts`)
   scorelogs identical to epic 1's gates-off games under
   `/home/sweettea/freeciv/.worktrees/perf/.pricing-runs/id-contested/`
   and `id-europe/` (same base, your changes are all gated).
3. Position check with `replay.py`: the WILD game's
   `/home/sweettea/freeciv/play-1107-1433/saves/freeciv-T0327-Y01901-interrupted.sav.zst`
   (Turkey 13 cities, 11 closed by the gate), 20 turns, 3 reseeds, gates
   off vs on (your gates on top of epic 1's `benchmark/tuning/pricing-1.ini`
   → make `pricing-2.ini`): cities founded, founder bids, expgate
   open/closed counts, score.
4. Directional, gates on: europe-late seeds 121–125, deep self-play and
   mixed_db, vs the defaults games on disk, via europemeter.py and
   rankcmp.py: cities, land, roads, score. n = 5 is directional; say so.

## Reporting

STATUS.md entry S-PRICING-2 (what landed, identity, replay table,
directional table, what was left). Commit on `epic/pricing-2`, do not
push, do not merge, report back the commit list and the tables.
