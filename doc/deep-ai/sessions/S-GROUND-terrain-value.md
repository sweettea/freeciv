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

## Acceptance

- Counters per mechanism; capture census tables regardless of verdict.
- A/B: contested + one classic cell (fortress value differs by ruleset
  — report venue-split), 20 paired seeds: forts built, pillages,
  captures suffered/made, score direction. Loss-guard: pillage must not
  cannibalize defence (unitslost, city losses watched).
- Byte-identity at zeroed gains; `meson test` + audit CLEAN at counts;
  STATUS final; sessions/ untouched.

## Scope fences

6-9 commits. `ai/deep` + the already-registered advisor hook only (rule
4 — the hook is deep-gated, keep it so). No airbase/airlift work (era-
dead), no ZOC/screening (post-v4 combat ladder). The Marketplace/
Conquer-Extras dead-branch audit rides along only as a STATUS note if
touched paths make it free.

## Stop conditions

The task-value hook can't express a base want without advisor changes
beyond the inert-extension rule (report the seam); pillage regresses the
loss guard without attribution.
