# W11 — Defence bookkeeping and emergency arbitration

**Phase 1. No dependency on W01. MUST precede W13.**
Root cause: RC2 (no shared plan). Closes two `README.AI` bugs.

## Problem A — cities don't know defenders are coming

`assess_defense_backend()` — `daimilitary.c:329`:
```c
unit_list_iterate(pcity->tile->units, punit) {
  defense += assess_defense_unit(ait, pcity, punit, igwall);
} unit_list_iterate_end;
```

Only units **physically on the city tile**. A defender two turns out with
`AIUNIT_DEFEND_HOME` and `goto_tile == pcity->tile` contributes nothing. So a
threatened city keeps computing `danger > defense` and keeps requisitioning at
`DAI_WANT_MILITARY_EMERGENCY` — which outranks everything domestic.

**The telling asymmetry:** the AI already does exactly this bookkeeping for
*enemy* cities. `reinforcements_cost_and_value()` fills
`city_data->invasion.attack/occupy` (`daicity.h:31`) so units can see how many
friends are converging. The same accounting for one's own cities doesn't exist.

`README.AI`: *"Cities don't realize units are on their way to defend it."*

## Problem B — simultaneous emergencies

`dai_manage_cities()` — `daicity.c:875`:
```c
city_list_iterate(pplayer->cities, pcity) {
  if (CITY_EMERGENCY(pcity)) auto_arrange_workers(pcity);
  if (CITY_EMERGENCY(pcity)) resolve_city_emergency(ait, pplayer, pcity);
  dai_city_sell_noncritical(pcity, TRUE);
  sync_cities();
} city_list_iterate_end;
```

Resolved in isolation, in **city list order**, with no arbitration. The first city
in list order grabs what it needs; the second gets the remainder — regardless of
which matters more. Both also compete for the same gold via `maxbuycost`.

`README.AI`: *"Emergencies in two cities at once aren't handled properly."*

## Design

**A.** An `incoming_defence` counter per owned city, populated each phase from
units with `AIUNIT_DEFEND_HOME` targeting it, weighted by ETA and survival
probability. Subtract from effective danger. Mirror the existing `ai_invasion`
structure — ~40 lines reusing a known pattern.

**B.** Two passes. Collect all emergencies with severity (`urgency`,
`grave_danger`, city `worth`, capital / wonder-city / migration-magnet status),
sort, resolve in priority order against a **shared** gold budget.

Note these compose: once cities know help is coming, fewer are in "emergency" at
once and the arbitration has less to do.

## Why before W13

Campaigns draw units away from cities. Without incoming-defence accounting, the
drained cities panic and requisition replacements at emergency priority, and
production oscillates against the campaign. Fix this first or campaigns will fight
the defence system.

## Acceptance

- Defenders produced beyond assessed need **down ≥40%**
- No production oscillation across 50 games (measure: build-target changes per
  city per 10 turns)
- L1 corpus position: two simultaneous emergencies, the more valuable city served
  first

## Commits

```
ai/deep: add an incoming defence counter per owned city
ai/deep: credit defenders en route against assessed city danger
ai/deep: weight incoming defence by arrival time and survival odds
ai/deep: collect city emergencies before resolving any of them
ai/deep: rank emergencies by urgency, worth and strategic role
ai/deep: resolve emergencies against a shared gold budget
test: add metric — defenders produced beyond assessed need
test: add corpus position — two simultaneous city emergencies
```
