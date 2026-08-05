# Session brief — HEDGE: the caravan as a liquid shield bank (SIZE FIRST)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md` (RC1 — nothing in the codebase prices option
value), `NEXT-SESSION.md` §5, the R2/R2b STATUS tails, and
`common/city.c`'s `city_change_production_penalty()` (the cost of
illiquidity) plus the ruleset's disband-recovery and help-wonder shield
mechanics.

## The idea (human, 2026-08-05)

Shields committed to a half-built Barracks are illiquid: switching
production pays the penalty and strands the rest. Shields banked in a
caravan are LIQUID: deliverable to a wonder, disbandable for partial
recovery, or held. So when the anticipatory lattice says a military need
MAY arrive soon — elevated menace/fear, but no active threat yet, exactly
the window between "build economy" and "build defender now" — a caravan
can dominate starting a long illiquid build. This would be the first
option-value mechanism in the codebase. Nothing needs inventing: the
probability side is the existing fear/menace terms, the cost side is the
existing penalty function, the conversion mechanics are ruleset facts.

## STEP 0 IS THE SESSION GATE: size the population (W36 discipline)

Before ANY mechanism code, instrument and measure on the current binary:

1. Production switches per game (deep seats): count, what-to-what class
   (unit↔improvement↔wonder), penalty shields actually paid, and the
   share paid in the window the hedge targets (city under elevated
   menace/fear at switch time). `city_change_production_penalty()` and
   the choose-build seam give this with a log line; W36's commit-time
   census is the pattern.
2. The counterfactual: of the penalties paid, how many switch-to-military
   events had a caravan-capable city (tech, cost) at the time — i.e. the
   hedge's actual addressable population.
3. Ruleset facts stated with numbers: disband recovery %, help-wonder
   delivery %, caravan upkeep in civ2civ3, penalty matrix by class pair.

**Verdict rule**: if the addressable penalty mass is small (order: less
than a settler's cost per game, or firing on a minority of seeds — state
the threshold you use and justify it from the data), the deliverable is
the census + a WONTFIX-for-now note in STATUS, and the session ENDS
there. That is a success (W36 precedent: three of four steps retired by
sizing). Only a fat population unlocks step 1.

## STEP 1 (only if sized fat): the mechanism

A hedge term in the build choice: when menace/fear (the existing terms —
no new probability source) exceeds the level at which a switch is likely
within the caravan's build horizon, credit caravan-class builds with the
EXPECTED penalty avoided: `P(switch) × penalty(current_build) −
carrying_cost(caravan)`. P(switch) must be an empirical mapping from the
step-0 census (menace level → observed switch frequency), not an invented
curve; the mapping's gain is the one PLACEHOLDER. Off-switch, counters,
byte-identity at default-off, A/B on one war-pressure cell + one economy
guard, 20 paired seeds.

## Acceptance

Step 0 tables in STATUS regardless of verdict; if step 1 runs: counter
census, A/B with the switch-penalty mass before/after (the mechanism's
own metric — did the hedge actually reduce penalties paid?), score
direction. `meson test` 3/3; `tune-audit.py` CLEAN. STATUS dated section
final; leave NEXT-SESSION.md and sessions/ alone.

## Scope fences

Step 0: 2-3 commits (instrumentation + census + STATUS). Step 1 if
unlocked: +3-4. No `ai/default`. The hedge NEVER outbids an active-threat
defender (emergency tiers untouched) and never fires where the floor's
classes are all dead AND menace is zero (no hedging against nothing).

## Stop conditions

The penalty function or disband mechanics differ from the design's
assumption in a way that guts the arbitrage (state it — that IS the
finding); step-1 A/B regresses the economy guard beyond noise.
