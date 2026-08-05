# Session brief — TRADE: routes as a portfolio (pre-v4; after PEACE)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md` §5,
the R2 STATUS tail, `findings/unused-game-surface.md` (trade thread),
`findings/deep-dive-2026-08-05/{economy-effects,diplomacy-gamestate}.md`
(B1, the IN-tier classification), `ai/deep/deeptrade.c` as R2+R2b left it,
and `common/traderoutes.c` (`city_trade_removable`, `max_trade_routes`).

## The design calls (orchestrator, delegated by the human 2026-08-05:
## "you make choices, make them tunable")

1. **Posture: pillar-lite.** Route value enters deep's OWN military
   decisions — `deep_op_pick_target` discounts targets we hold routes
   with by `trade_partner_target_discount` (PLACEHOLDER; **0 = the pure
   opportunist posture**, so the stance is itself tunable). War/peace
   stays stock's until W34-B; this only stops deep burning its partners
   when equal targets exist.
2. **Ceasefire routes: priced by TRUNCATION, not banned.** A ceasefire's
   lapse turn is KNOWN state — so a ceasefire partner's route NPV is the
   annuity truncated at the lapse plus the one-time establish bonus,
   plus `trade_ceasefire_peace_odds` (PLACEHOLDER, default 0) × the
   post-lapse annuity for the chance peace follows. At default 0 this is
   pure known-mechanics pricing: no farming of fragile routes unless the
   tuner decides optimism pays. No invented ban, no invented bonus.
3. **Cross-water caravans: in scope, pre-v4** (human call). Trade
   caravans with cross-continent gotos become lift-eligible behind
   `lift_carry_caravans` (0/1, default 0 until measured); the IC 2×
   premium on island cells is the payoff and isolated-150 the venue.

4. **Love from trade routes (human, 2026-08-05).** Active routes with a
   partner add `trade_route_love × active_routes` to `love[partner]` per
   turn, applied from deep's own phase wrapper (`deep_plr` embeds
   `ai_plr`; stock's war/peace code reads whatever love holds — zero
   `ai/default` changes). Stateless by construction: stock's 4%/turn
   decay unwinds it once routes cancel. It counteracts the
   `best_target` bleed (−50/turn) exactly where interdependence should,
   and compounds with PEACE (routes → love → the 125 peace bar → routes
   survive). PLACEHOLDER, tunable to 0 = off; clamp so it cannot
   single-handedly cross the alliance thresholds (love-cap at a
   fraction of MAX_AI_LOVE, itself the second knob only if a structural
   clamp won't do). Counter: love added per partner per turn on the
   stance/diplomacy log line. Exploitability note for STATUS: a rival
   can farm routes to pacify deep — that is real interdependence,
   symmetric, and tunable away; say it, don't hide it.

## THE JOB

1. **Slot opportunity cost, structurally.** civ2civ3 gives 2 slots per
   city, flat. The want for a route must be its value MINUS the best
   alternative partner's for the same slot (a comparison, not a knob),
   and when slots are full, `city_trade_removable()` — which no AI has
   ever called — prices replacement: upgrade routes when a new partner
   beats the worst incumbent by more than the caravan's cost.
2. **Partner-aware targeting** (design call 1): the discount in
   `deep_op_pick_target`, its own counter, measured on a war cell for
   the cost side (do we decline profitable conquests?) and a
   post-PEACE cell for the benefit (do partnerships survive?).
3. **Ceasefire truncation pricing** (design call 2) in the R2 evaluator:
   read the pair's diplstate + ceasefire turns_left; truncate.
4. **Transit risk**: discount route NPV by path danger using the
   worker/settler risk machinery deep already owns
   (`trade_route_risk_gain`, PLACEHOLDER); escort requests reuse the
   observed-sea-threat scaling posture (boat-shadow evidence), never
   unconditional.
5. **Cross-water eligibility** (design call 3): lift accepts trade
   caravans behind the gate; the caravan evaluator's ETA must then
   include the expected lift wait (the wait census exists) rather than
   pretending to walk on water.

## Acceptance

- Off-switches: all new knobs at defaults reproduce the pre-session
  binary byte-identically EXCEPT where R2's `trade_merit` is already the
  master switch — state the switch tree explicitly and prove each level.
- Counters: partner-discount applications, replacements executed,
  ceasefire truncations, lift-carried caravans, escort requests.
- A/B on primary-120-classic (the live venue today) + a post-PEACE
  civ2civ3 cell if PEACE has landed (it should have — sequencing), 20
  paired seeds direction: routes established/replaced, route income
  curve, caravan losses (the capture/risk metric), score direction.
- `meson test` passes; `tune-audit.py` CLEAN at its new count; STATUS
  dated section final; leave NEXT-SESSION.md and sessions/ alone.

## Scope fences

6-9 commits. Requires PEACE landed first (population) and R2b landed
(the wonder producer shares the unit). No war/peace ownership, no
`ai/default` changes beyond existing visibility. The upstream dead-branch
guard bug gets FILED (upstream-bugs.md), not fixed in place.

## Stop conditions

The lift-eligibility change destabilizes settler lifts (the manifest is
shared — watch lift-arrived for settlers specifically); the slot
comparison cannot be expressed without a second full evaluator pass per
city per turn (report the cost, gate on the recalc cadence).
