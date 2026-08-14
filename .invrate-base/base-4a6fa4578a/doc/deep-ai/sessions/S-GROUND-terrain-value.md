# Session brief — GROUND: the terrain-value round (pre-v4)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md`
§5, `findings/deep-dive-2026-08-05/action-surface.md` IN FULL (your
evidence: the never-touched table, the base-valuation hole, the ruleset
asymmetries), and the G1/D1 STATUS tails (the hold API, the class facts,
the survival machinery you price with).

## THE JOB — one missing want family: ground value that isn't tile yield

1. **Fortress/Fort builds.** The mechanism is wired
   (`autoworkers.c:121-122`) and the want is provably zero (the only
   valuation reads tile output; a base changes none — 0 fortresses ever,
   upstream bug #36). Deep-side want via the task-value hook deep already
   registers (`deep_worker_task_value` — extend, don't add a second
   hook): a base's value = survival delta for a defender ON that tile
   (the substrate prices this directly) × the tile's strategic weight
   (border proximity via G1's machinery; chokepoint via
   `deep_site_is_choke`, exported from site scoring). Classic pays extra
   (borders + NoStackDeath — ruleset facts, venue-split expected).
   PLACEHOLDER gain; counter; the A/B metric is forts built + defended-
   tile survival, not just score.
2. **Pillage as yield denial.** The in-code admission is the spec
   ("We could have ACTRES_PILLAGE here, but currently we don't").
   Deep-side: idle military near enemy improved tiles (rampage's
   population) value pillage at the enemy's LOST yield amortized (their
   tile output is readable — omniscient today, known_city-fed later) ×
   a denial gain (PLACEHOLDER). War-only, and never instead of a live
   kill/defence task (tier discipline). Counter + roads/irrigation
   destroyed metric.
3. **Capture-units, instrumented BOTH directions first** (civ2civ3,
   turn-1): count capturable-adjacency events per game — our caravans/
   workers adjacent to enemy capturers, and theirs to ours — before any
   mechanism. If the defensive population is fat (deep routes caravans
   through contested ground), the defensive half (risk term in caravan/
   worker routing — the machinery exists from W05) lands this session;
   the offensive half (seek captures) lands only if its population
   justifies (W36 discipline).
4. **Fortify as a chokepoint decision**: when a unit garrisons a
   fortress/choke tile this session made wantable, prefer fortify over
   wander — small consumer of (1), uses D1's hold API.
5. **Barbarian terror re-shape (human, 2026-08-06; derby-2 storm: deep
   lost 107 of 207 cities in 70 turns to uprisings the flat terms never
   saw coming).** Deep's two barbarian terms are step functions:
   `fc->barb` flips exactly AT onset (`deepdefence.c:1125`) and
   `deep_barbarian_fear()` returns a flat `tune->barbarian_fear`
   (`deepdefence.c:825`). The server's uprising code has the real shape
   — mirror it, do not invent (`server/barbarian.c:581-613`):
   willingness is `fc_rand(30) + 1 <= city_count * (barbarianrate - 1)`
   (also gated on EFT_CIVIL_WAR_CHANCE), and horde size grows with an
   empire-size ladder (`UPRISE_CIV_SIZE`). Three changes, one scaling
   function shared by both consumers:
   - **Anticipation**: the terror ramps up `barb_anticipation_turns`
     (PLACEHOLDER) BEFORE `game.server.onsetbarbarian`, because the
     garrison must EXIST at onset and defenders take ~10 turns to
     build. Linear ramp is fine; the knob is the lead, not the shape.
   - **Frequency scaling**: scale by `(barbarianrate - 1)` exactly as
     the server does; BARBS_DISABLED and (for the uprising component)
     HUTS_ONLY zero it — in no-barb cells the whole class drops out
     free. Hut risk near cities stays deepexplore's separate term.
   - **Empire-size scaling**: scale per-empire exposure by own city
     count per the server's willingness formula (the derby's "my Nth
     city buys barbarian pressure" tax, priced on the defence side).
     Note the per-CITY floor should NOT each carry the full empire
     term — the uprising picks one victim site; divide the empire
     exposure across coastal/frontier cities by the same eligibility
     the server's site-finder uses (MIN/MAX_UNREST_DIST, empty-tile-
     nearby), approximated readably and stated.
   Both `barbarian_fear` and `floor_barb_weight` become
   base × the shared scaling (bases stay PLACEHOLDER for the pile).
   Counters: pre-onset ramp turns, scaled-terror magnitude histogram.
   A/B: a raging-barbarian venue (set barbarians high) 20 paired seeds,
   cities-lost-to-barbarians before/after; plus byte-identity check in
   a no-barb cell (term must vanish). The demand-side twin (expansion
   want debit for uprising pressure) stays #39/#43 — do not build it
   here, note the seam.

## Acceptance

- Counters per mechanism; capture census tables regardless of verdict.
- A/B: contested + one classic cell (fortress value differs by ruleset
  — report venue-split), 20 paired seeds: forts built, pillages,
  captures suffered/made, score direction. Loss-guard: pillage must not
  cannibalize defence (unitslost, city losses watched).
- Byte-identity at zeroed gains; `meson test` + audit CLEAN at counts;
  STATUS final; sessions/ untouched.

## Scope fences

7-10 commits (item 5 added 2026-08-06). `ai/deep` + the already-registered advisor hook only (rule
4 — the hook is deep-gated, keep it so). No airbase/airlift work (era-
dead), no ZOC/screening (post-v4 combat ladder). The Marketplace/
Conquer-Extras dead-branch audit rides along only as a STATUS note if
touched paths make it free.

## Stop conditions

The task-value hook can't express a base want without advisor changes
beyond the inert-extension rule (report the seam); pillage regresses the
loss guard without attribution.
