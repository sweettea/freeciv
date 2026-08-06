# Session brief — CONSEQ: consumers for signals we already compute (pre-v4)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md` §5,
the relevant audit sections cited per item, and the STATUS tail. Three
small mechanisms, one session; each is a CONSUMER of an existing signal,
which is why they bundle safely.

1. **The defensive tech-wall** (sibling of `deephorizon.c`'s H9). When
   `deep_city_stack_survival` reports a deficit AND the best buildable
   defender is not the best the RULESET offers at reachable tech, route
   `survival_fear_value × deficit × defensive_techwall_weight`
   (PLACEHOLDER) into `tech_want[]` for the gating advance — exactly how
   H9 routes capacity credits. The defender-vs-tech test reuses
   `can_city_build_unit_later` / the eta-reinforcement model's
   best-buildable scan (`deepcombat.c:370-377`). Counter per routing;
   venue: any war cell.
2. **Sell-before-fall + evacuation** (consumer of the write-off,
   military audit U4). When the counter-claim machinery logs a write-off
   (`deepdefence.c:1878-1885`), act instead of shrugging: order civilian
   units OUT (the flee machinery + `deep_hold_unit` API), and in the
   final window (survival ≈ 0 and enemy adjacent) sell the highest-value
   sellable building (one per turn is the server rule). Guards: never
   sell in a city that later holds (measure false-write-off rate FIRST —
   the write-off signal's precision has never been measured; if it is
   imprecise, that is the finding and the sale threshold tightens to
   certain-loss only). Counters: evacuations, fire-sales, false
   write-offs.
3. **Home-city upkeep redistribution** (action sweep: `Home City` used
   once, mechanically). civ2civ3 charges the HOME city a unit's gold and
   food upkeep; rehome units from strained cities to high-surplus ones.
   Deliberately narrow: only when the current home is in deficit or CM
   food-stress AND a surplus city is adjacent-or-same-tile reachable
   (rehoming needs the unit in the new city). No optimizer — a relief
   valve, one rule, its own counter. Reuse the treasury guard's strain
   signals.

4. **Expiry-aware amortization (human, 2026-08-05: "some units and
   buildings expire too" — the WRACE obsolescence insight generalized).**
   Two consumers of the own-research ETA signal (bulbs ÷ rate toward a
   named tech — readable, no rival modelling):
   - **Buildings**: truncate a building's amortization window at the
     expected arrival of its own `obsolete_by` tech WHEN WE are the ones
     researching toward it (the Barracks-under-Gunpowder case — the AI
     currently spends shields on buildings its own queue is about to
     void). Verify the engine's obsolete-building semantics (disabled?
     auto-sold?) and state them.
   - **Units**: successor-imminent deferral — when the better unit's
     tech ETA is inside the current unit's build time, defer the build;
     the interim shields go to the normal auction (where HEDGE's liquid
     caravan is the natural winner once it lands — note the coupling,
     don't depend on it). NEVER overrides an active-threat defence build
     (tier discipline); hysteresis on the ETA estimate so research
     wobble doesn't churn the queue (one PLACEHOLDER gap, the M3
     pattern).
   Counters: truncations applied, deferrals, deferral-overridden-by-
   threat. A/B lens: shields spent on soon-void buildings and
   soon-obsolete units, before/after.

Discipline: each item its own commits + counter + A/B lens; all knobs
PLACEHOLDERs registered everywhere; `meson test` + `tune-audit.py` CLEAN;
byte-identity where defaults are off-shaped (items 1-2 gate on new
weights defaulting 0 → prove byte-identity at 0; item 3 gate
`rehome_enabled` 0). STATUS dated section final; leave NEXT-SESSION.md
and sessions/ alone. 6-8 commits. Stop conditions: the write-off signal's
false-positive rate exceeds usefulness (report it — that alone justifies
the session); the rehome action's server constraints don't match the
design (state them).
