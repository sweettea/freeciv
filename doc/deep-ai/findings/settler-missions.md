# Settler missions: frozen, then repurposed (2026-08-02)

Measured on the W02b pre-epic reference runs (binary `3e338f8475`,
`deep-best.ini`, 10 paired train seeds per cell). Companion to
`settler-routing.md`, which is about where settlers are SENT; this is
about what happens to them afterwards.

Prompted by a human observation: *a live settler is generally a
failure — it should be in transit to a destination, and auto-worker is
almost always the wrong answer for it.* The data says the observation
is right and the cause is one layer further up than the fallback.

---

## 1. What the settler count actually counts

The scorelog `settlers` tag is `get_settlers()` (`server/report.c:794`):
every live unit that `unit_can_do_action(ACTION_FOUND_CITY)`. In
civ2civ3 the `Settlers` unit carries the **`Workers` flag**, so the tag
counts founders doing terrain duty as well. "32 live settlers" is not
32 stalled founders, and any reading of that column has to say which.

Final-save census, worker-capable units by server-side agent:

    lategame-80    games  founders  on worker duty  share  worker units
    baseline          10       408             407   100%           505
    deep              10       253             246    97%           451

    primary-120    games  founders  on worker duty  share  worker units
    baseline          10       599             574    96%           677
    deep              10       647             637    98%           102

At game end essentially every surviving founder is under
`SSA_AUTOWORKER` in **both** arms, so the fallback itself is stock
behaviour, not a deep regression — deep carries fewer of them than
baseline on `lategame-80` (25/game vs 41/game).

`primary-120` is where the arms separate, and not on the founder count
(599 vs 647, both ~60 per game) but on what does the terrain work:
**baseline builds 677 dedicated `Workers`, deep builds 102.** Deep runs
its entire terrain economy on repurposed founders, i.e. on `pop_cost 2`
units doing a `pop_cost 0` unit's job, 6.6x fewer purpose-built workers
than the control.

The plausible mechanism — not yet confirmed — is that a `Settlers`
carries the `Workers` flag, so it counts toward whatever demand test
would otherwise ask for a `Workers`: the repurposed founders satisfy
the worker requirement, and the cheap unit is never ordered. If that is
right, the two defects feed each other: settlers get built because
`founder_want` is high, freeze or fail to find a site, fall back to
worker duty, and by existing they suppress the order for the unit that
should have done that job at no population cost.

## 2. They are not idle. They are frozen mid-mission

Sites keep being found to the last turn — `primary-120` seed 7, P4:
49 distinct units received a site, 19 founded a city, and picks
continue to turn 119. The missions are not being abandoned for lack of
a target; they are not completing.

Per-player, same game, from the `deep-settler ... mission` metric line
(stationary = the unit is at the same tile as its previous mission
turn):

    plr  mission-turns  stationary  share  retargets
    P1             701          45     6%          8
    P2             197          42    21%          1
    P3             304           9     3%          8
    P4             897         654    73%         11
    P5             314          15     5%         15
    P6              38           0     0%          0

Pooled over every deep game in each cell:

    pre-primary-120   mission-turns 23499   stationary 4840   21%
    pre-lategame-80   mission-turns 10170   stationary 5275   52%

**Half of all mission-turns on the crowded cell are a settler standing
still while holding a city mission.**

P4 is the player at war with three neighbours (`wars` in the final
save: 0-3, 1-3, 2-3). Its frozen settlers sit among enemy Chariots,
Phalanxes and Musketeers. The peaceful players in the same game run
3-6% stationary.

One unit's whole trace, unit 395:

    turn  71  at 47,33 -> 55,16  sane1     walking, ~1 tile/turn
    turn  76  at 51,29 -> 55,16  sane1
    ...
    turn 115  at 59,16 -> 67,62  sane1     re-targeted across the map
    turn 120  at 59,16 -> 67,62  sane1     has not moved in 6 turns

## 3. The mechanism

`deep_settler_apply_risk()` sets
`parameter->is_pos_dangerous = deep_civilian_pos_dangerous`
(`deepsettler.c:217`). That is W05 and it is correct — it cut settler
losses. The gap is on the other side:

- `goto_is_sane()` is a reachability test. It does **not** know about
  danger, so a mission whose every route is vetoed still logs `sane=1`
  (887 of 897 of P4's mission-turns).
- The mission-continuation block (`deepsettler.c:611`) only calls a
  mission off when `!city_can_be_built_here()` at the target. "I could
  not move toward it, again, for the fortieth turn" is not a condition
  anything tests.

So a settler in a war zone holds its mission indefinitely, and only
reaches the auto-worker fallback once something else kills the mission.
The auto-worker outcome is a symptom; the freeze is the defect.

**There is already a precedent for the fix in the same function.** The
`drop-boatless` path releases an undeliverable overseas mission and
re-searches land-only *within the same turn*, added after a 1200-event
pick-drop loop. The land equivalent — the risk pathfinder refused to
move, so drop and re-search now — is the same shape and needs no new
constant.

## 4. The other tail: 40-turn walks

Mission length in turns, first mission line to last, pooled per cell:

    cell            missions  median  p75  p90  max   >20t   >40t
    primary-120         2380       7   13   20   52    10%     1%
    lategame-80          911       7   17   38   80    21%     9%

The median mission is 7 turns and unremarkable. The tail is not: on an
**80-turn** cell, 9% of missions exceed 40 turns and the longest runs
the entire game.

This is priced, not accidental. `cr->result = amortize(cr->total,
d * turns)` with `d = site_travel_discount`:

    40-turn walk, stock d = 3.0    (23/24)^120 = 0.6% of value retained
    40-turn walk, b6   d = 0.65    (23/24)^26  =  33% retained

The tuner drove `d` 4.6x below stock's PERFECTION, on the strength of
`europe-T80` (where stock's discount left 17 cities with room for 18
more five tiles away). The correction went too far and the cost landed
in the tail.

**W02b gives the parameter a meaning it did not have.** With the
catchment valued as a stream of discounted citizen-turns, founding T
turns later delays *every* citizen arrival by exactly T turns — which
is `amortize(stream, turns)` with `d = 1`. `d = 0.65` asserts that
delay is cheaper than reality; `d = 3` asserts it is three times worse.
Both were fudge factors for a delay the model can now state directly,
and a genuinely better site still wins a long walk because its stream
is larger.

**Human call 2026-08-02: keep it a little BELOW 1, not at 1.** The
reason is that `d = 1` prices the wrong counterfactual. By the time the
discount is applied the settler exists — its shields and its `pop_cost`
are spent — so the choice is not "a city now versus a city in T turns",
it is "a city in T turns versus no city at all", and the settler's
alternative uses (auto-worker, Join City) are worth far less than a
city. A full-delay charge therefore over-favours refusing the walk.
Below 1 prices the wait honestly while granting that the comparison is
against a poor alternative, not against a free city today.

Landing values, to be recorded as **human-set, untuned** in the same
style as the other human calls in `deeptuning.c`:

    site_travel_discount   default 0.9   range [0.75, 1.0]

The range matters as much as the default: it keeps the tuner from
returning to "delay is nearly free" (0.65 is outside it) without
keeping it from finding the honest 1.0. **`deep-best.ini` carries
`site_travel_discount = 0.654809` and the loader does not clamp file
values to the params.ini range** — so whoever lands this must update
the play file too, or the change is inert wherever it matters most.

Not yet measured. It is one parameter's semantics, independent of the
mission-freeze fix, and should be measured on its own.

## 5. What to do, in order

1. **Drop a mission the settler cannot advance**, mirroring
   `drop-boatless`, and re-search the same turn. Biggest of the three:
   52% of mission-turns on `lategame-80`. — **LANDED `6ab78f8d98`**,
   folded into the W02b epic at the human's request.
2. **On re-search prefer another site, then Join City, then
   auto-worker.** This is W02b's "join a city when no site beats the
   population it would return" (and `upstream-bugs.md` #31 — no AI ever
   performs Join City). The census in §1 is its size: 637 of 647
   surviving founders were on worker duty.
3. **Hold the travel discount a little below 1** (§4). — **LANDED
   `84df35bf28`**, default 0.9, range [0.75, 1.0].

Not in this list because it is a different workstream, but adjacent
and now quantified: deep builds 102 dedicated worker units where
baseline builds 677 (§1). Whatever orders `Workers` is being satisfied
by units that cost two citizens each.

## 6. Instrumentation gap

The repurposing is `UNIT_LOG(LOG_DEBUG)` ("improves terrain instead of
founding"), so it cannot be counted from benchmark logs — everything in
§1 is inferred from an end-of-game snapshot. Promoting that line to
`deep_metric_log_level()`, and adding one for a mission dropped without
progress, makes all of this directly countable per turn instead of
reconstructed.
