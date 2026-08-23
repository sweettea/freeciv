# Session brief — the bluewater venue and the capability census

Strategic pivot (user, 2026-08-23): the remaining defect list is empty
only because our venues cannot see the rest of the game. Isolated-150
ends before air power, tech50war-200 is a land war, corridor-150 is a
road exhibition. The user's target list: naval expeditions, naval
bridges, ocean fill, caravan/engineer escorts, air expeditions,
airbases, fighter/bomber/carrier doctrine, nukes. Before ANY of that
is designed, we need (1) venues where those behaviours can occur and
(2) a measured census of what deep and stock currently do there.
This epic builds both. DIAGNOSIS/INFRASTRUCTURE ONLY: no ai/ behaviour
changes, no new knobs.

## Deliverable 1: two venue configs

1. **archipelago venue** (e.g. archipelago-200): island-heavy
   generator settings, endturn deep enough for transports/marines and
   meaningful cross-water conquest. Design goal: a venue where naval
   logistics DECIDES games.
2. **airera venue** (e.g. airera-250 or a tech-boosted variant):
   reaches flight/carriers/(ideally) nukes within the run. Tech pace
   verified from an actual game before the config is committed — do
   not ship a venue whose headline behaviours never fire; paste the
   tech-arrival turns in STATUS.
Both: deterministic under the harness, standard seeds, run.sh
compatible, sized so a game finishes in reasonable wall-clock (state
the measured minutes/game). Configs live in benchmark/configs/ with
comments naming their purpose.

**Equal-start craft (user directives 2026-08-23): do not pay
pre-flight turns, and do NOT try settings-only — techlevel caps out
below flight.** The method is PLAY-FORWARD-THEN-EQUALIZE:
(1) grow the world once with ALL-CLASSIC seats (stock is the neutral
worldbuilder; deep would bake its biases into every evaluation start)
on the target map — ~15% land for the naval venue; ~50% land with
barbarians disabled and war-suppressing settings for the air venue so
every seat reaches the snapshot alive; (2) snapshot a FAMILY of worlds, not one — 4-8 mapseeds grown
identically and frozen at the target era (world tech ~70 for flight),
each on a turn where every seat is alive and viable: one world is a
sample size of one, and the family is what keeps venue conclusions
general; (3) run a committed MINIMAL-REPAIR edit pass over each
snapshot (the sav is editable secfile text): keep seats alive and
stamp ONLY the venue's subject infrastructure — Airports in every
city for the airlift/air venue (pair with airliftingstyle at load),
harbors for the naval one. Do NOT level tech/gold across seats:
module-swap pairing already cancels seat handicaps (same seats, same
world, both arms), and heavy state editing manufactures unreachable
game states where AI behaviour stops reflecting reality;
(4) per-seed variance comes from gameseed at load — frozen worlds,
N divergent games each (paired-seeds logic at its limit). Deliverables:
the grow-world driver, the equalizer script, and the frozen snapshot
save all committed and documented; the equalized save must load
cleanly on the pinned binary with both ai modules (no warnings) and
score correctly from a save-start (verify a scorelog appears and
survives; the exact-turn resume trap applies only to PRE-EXISTING
scorelogs — evaluation dirs start fresh). STATUS notes the
methodology trade: a fixed world narrows variance to AI decisions
(maximum sensitivity, less generality), so census tables must say
which venue kind produced them.

## Deliverable 2: the census

On each venue, a small batch (e.g. 8-10 seeds, deep-vs-stock mixed
seats as the harness supports): count per side per game, from
existing logs/scorelogs plus a census reader you commit to benchmark/:
transports built and loaded (lift ops formed/launched), overseas
cities founded/conquered, air units built vs USED (sorties flown,
if observable from unit activity; say what is not observable),
airbases built, carriers built/loaded, nukes built/used, caravans
lost in transit, engineers lost in transit, long jobs (>5 worker-turn)
started. Where a count cannot be read from existing instrumentation,
DO NOT add ai/ code — record it as "needs an instrument" with the
exact seam in STATUS; that list becomes the instrumentation epic's
brief.

## Deliverable 3: the ranked gap table

STATUS entry: per capability, deep's census vs stock's census vs
"what a competent player would do" (one sentence each), ranked by
expected score leverage on the new venues. This table is the map for
the entire bluewater campaign; write it like it will be read twenty
times.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare). ALL game/tune processes at `nice -n 10`
(user directive — their interactive game shares the box; builds too).
Ports 7200-7249, jobs <=2. Batch drivers at benchmark/*.sh launched
detached from the worktree root as `setsid bash ./benchmark/<script>.sh
go` (exact shape). DONE markers record SUCCESS. Power-loss sweep on
inherited dirs. Games byte-deterministic. One commit one thing
(configs, census reader, driver, STATUS); meson test all suites;
STATUS with pasted numbers; standard trailer; do NOT merge — stop
after the STATUS commit on your branch.
