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

> **Superseded 2026-08-02 (late). The war-zone reading below was wrong,
> and §7 has the measurement that replaced it: 90% of stalled
> mission-turns are settlers trying to WALK to an overseas site,
> because `goto_is_sane()` pathfinds non-omnisciently and unmapped
> water reads as passable. Danger accounts for 0.3%. The paragraphs
> here are kept because the reverted fix `6ab78f8d98` was built on
> them, and because "the mission-continuation block never asks whether
> the settler is getting there" is still true.**

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
reconstructed. — **Partly closed `87ecdc8f55` / `458cf07945`**: every
mission-turn now logs why the settler did not arrive, which is what §7
is built on.

---

## 7. What it actually was: walking to islands (2026-08-02, late)

§3's war-zone story was wrong, and the instrumentation that replaced it
was written first on purpose — the previous session guessed and the
guess cost a benchmark cell.

Each mission-turn now logs an outcome (`arrived` / `moved` / `ferry` /
`nopath` / `wait` / `blocked` / `nomoves` / `dead`) and, when the risk
map has no route, a ladder that relaxes one pf constraint at a time and
names the first relaxation that brings the route back. `lategame-80`,
10 train seeds, deep arm, 11215 mission moves:

    reason     count   share        nopath, why    count   share
    nopath      6323   56.4%        vision          5659   89.5%
    moved       2810   25.1%        behaviour        647   10.2%
    arrived     1062    9.5%        danger            17    0.3%
    ferry       1017    9.1%
    blocked        3    0.0%

`wait` and `blocked` are ~0, so the mover is not refusing a step on a
route that exists: **the route does not exist.** And `danger` — the
W05 veto §3 blamed — accounts for 0.3%.

`vision` means the route reappears only when unknown tiles are assumed
passable. Joining each stall to the pick that created its mission:
**5656 of the 5659 are missions the search itself marked
`overseas=1`.**

### The disagreement, exactly

`deep_settler_gothere()` decided boat-versus-walk by asking
`goto_is_sane()`. That function pathfinds **non-omnisciently**
(`pft_fill_unit_default_parameter()` leaves `omniscience = FALSE` and
`goto_is_sane()` never overrides it), so unexplored tiles count as
passable and a target across unmapped water reports a good land route.
Deep's site search and its mover both pathfind at the player's real
handicap level, which at every difficulty anyone plays is omniscient.

So: the search chose an overseas site and knew it was overseas; the
mission kept only the tile; execution re-derived "does this need a
boat" from a more optimistic pathfinder, got "no", skipped
`aiferry_gobyboat()`, and walked. The risk map found no land path,
`dai_unit_goto_constrained()` did nothing and returned alive, and the
settler stood on the beach. Median stalled episode 5 turns, p90 27,
max 47; only 21% ever moved again.

That is RC5 in one line: **planning and execution disagreed because
they asked different pathfinders the same question.**

It also disabled the guard built for this: a settler that never reaches
`aiferry_gobyboat()` is never counted in `plr_data->stats.passengers`,
so "every boat already has a waiting passenger" never became true and
overseas picks were never throttled.

### The fix and what it moved

`deep_settler_can_walk_there()` — `goto_is_sane()`'s test at the
player's real handicap — replaces `goto_is_sane()` in the boat gate,
the walk gate and the bodyguard gate, computed once per `gothere()`
(`0f126c33b7`).

Same 10 seeds, pinned binaries either side of the fix:

    lategame-80, deep arm      pre      post      diff        t
    score                   183.82    189.22     +5.40     2.60
    cities                    8.27      8.68     +0.42     2.91
    pop                      45.05     48.87     +3.82     4.83
    landarea                 95067     99917     +4850     3.37
    mean_city_size             5.53      5.81     +0.28     3.72
    live settlers at end       4.47      2.73     -1.73    -3.97

Mission-turns 11527 → 7718, picks 1964 → 1574 (the reverted attempt
went the other way, 1978 → 7152), `nopath` 6323 → 529 and 99% of what
is left is the `behaviour` case, not this one.

Where the cities came from, per 10 games:

    picks land      644 -> 655   arrived 460 -> 461   (71% -> 70%)
    picks overseas 1320 -> 919   arrived  91 -> 114   ( 7% -> 12%)

Land expansion is untouched. Overseas *commitment* fell by a third —
the throttle works now — and overseas *deliveries* rose by 23 over ten
games, which is about +2.3 cities per game and matches the +0.42
cities per player exactly.

### The head-to-head, which is what it was for

deep vs `ai/default` seated in the same game, mirrored `mixed_dc` /
`mixed_cd`, 10 seeds, both sides of the fix. Score margin by turn:

    turn      pre     post    delta
    T20     +1.30    +1.70    +0.40
    T40     +7.67   +10.87    +3.20
    T60     +9.21   +16.12    +6.91
    T70     +5.25   +12.75    +7.50
    T80     -1.49    +9.73   +11.22

The control reproduces the previous session's reference series to
within rounding (T20 +1.3, T40 +7.7, T60 +8.9, T80 -1.7 recorded), so
the instrumentation is behaviour-neutral and the comparison is honest.

Difference-in-differences at T80 — the same seeds either side, so the
noise that dominates the raw margin cancels:

    metric      pre      post     delta      t   up
    score     -1.49     +9.73    +11.22   2.09  7/10
    cities    -2.12     -0.50     +1.62   2.87  8/10
    pop       -3.33     +3.93     +7.27   2.59  9/10
    bnp       -8.28     +1.83    +10.11   3.69  9/10
    mfg       -8.97     +0.30     +9.28   2.06  8/10

The delta is ~0 through T30 and grows monotonically from T40 — the
window where the stall lived. **deep now finishes ahead of
`ai/default` on `lategame-80`**, the cell it was losing, and the city
deficit that caused it is 76% closed rather than reversed.

### The pre-registration, scored

Written before the diagnostic batch finished and before any fix
existed.

- **D1** — `nopath` ≥ 60% of non-arriving, non-moving mission-turns,
  falsified if `wait` + `blocked` reach 40%. **Held**: 56.4% of all
  mission moves, `wait` + `blocked` = 3 events.
- **D2** — "≥ 60% survive with the danger veto off, **so the blocker
  is the tile behaviour**". The stated criterion held (99.7%) and the
  **inference was wrong**: territory accounts for 10%, and the real
  cause was a rung I had not thought to enumerate. The ladder found it
  only because it was written to relax every constraint rather than
  the two I suspected. Worth remembering: a pre-registration can pass
  its own test and still be wrong about the mechanism.
- **D3** — `nopath` concentrated at T40+ as well as early. **Held**:
  56% of them at T40+.
- **F1** — stationary share below 30%. **Held on the narrow
  definition** (`nopath`+`wait`+`blocked`, 56.4% → 7.2%) and **failed
  on the honest one**: counting settlers waiting on a lift, a
  mission-turn with no movement is 65.5% → 39.6%. The remainder is now
  ferry queueing, which is the next problem and not this one.
- **F2** — site evaluations must not double. **Held**, and then some:
  picks 1964 → 1574, against the reverted attempt's 1978 → 7152.
- **F3** — cities per player up ≥ 0.5. **Narrowly missed**: +0.42
  (t=2.91).
- **F4** — the T80 head-to-head margin moves up from -1.7. **Held**:
  -1.49 → +9.73, t=2.09 paired.

### What this leaves open

- **Overseas delivery is still 12%.** Nine of ten settlers that commit
  to a boat trip never arrive. That is the ferry pipeline, not the
  settler, and it is now the largest single loss in this subsystem:
  2398 of 7390 mission moves are a settler waiting on a lift.
- **`behaviour` stalls, 523 turns.** Non-allied territory or a unit
  standing in the corridor, mid-walk, with no re-search. Small, and
  the honest fix is the one §5.1 wanted — but it should be aimed at
  this case only, which is a tenth the size it was thought to be.
- `goto_is_sane()` is used by `ai/default` too, with the same
  optimism. Not ours to fix (rule 2); it belongs in
  `findings/upstream-bugs.md`.

---

## 8. The lift does not arrive, and the obvious repair does not help
    (2026-08-02, night)

§7 left overseas delivery at 12% and named the ferry pipeline as the
largest remaining loss. This section is the measurement of that, and a
fix that was built, measured and refused.

Both halves of the protocol were instrumented at once (`fae8a0f778`),
because the pipeline is two independent searches — the passenger raises
`FERRY_WANTED` and sets `done`, the boat looks for flagged passengers —
and neither half is evidence on its own. All read-only; `ai/default`
untouched.

### It is not a boat-supply problem

`lategame-80`, 10 train seeds, 2398 settler waiting-turns. "Willing" is
stock's own `is_boat_free()`, asked about that settler:

    willing boat 2-5 tiles away     1110   46%
    willing boat 6+ tiles away      1110   46%
    no willing boat anywhere         127    5%
    willing boat on the same tile     49    2%
    willing boat adjacent              2    0%

**95% of the time a boat exists that would carry it.** More boats, or a
bigger colony-lift want, fixes nothing.

The settler cannot go to it: `aiferry_find_boat_nearby()` is a
`square_iterate(..., 1, ...)`, one tile. In 92% of these turns the boat
is outside that radius by design — the boat is supposed to come. On the
settler side, 90% of waiting turns are spent flagged `FERRY_WANTED` and
only 0.4% ever reach "holds a boat". 61.7% of waiting turns are still
waiting when the game ends.

### The boat side, and the branch that swallows it

8463 boat-turns, 6076 of them idle with no passenger and no cargo:

    nobody flagged anywhere         4694   77%
    flagged passenger, REACHABLE     778   13%
    flagged passenger, unreachable   416    7%

The 778 are the defect: a boat with full moves, marked available,
usually sitting in a city, with a flagged settler it can provably path
to, doing nothing.

`aiferry_findcargo()` was reproduced exactly from the deep side — same
overlap parameter, same `omniscience`, same public `no_fights`
callback, same `pf_map_tiles_iterate(..., TRUE)`, same match condition
— and **it found the waiting settler every time**. So the scan is not
the problem; it is never run. A bitmask of every predicate on the way
to it identified the branch, and one type accounted for all of it:

    Galleon    gate=0   220 boat-turns
    Transport  gate=4    84 boat-turns      (bit 4 = worker/cityfounder)

`dai_manage_unit()` dispatches on `ptype->adv.worker ||
unit_is_cityfounder()` **before** it dispatches on ferry, and civ2civ3's
Transport carries the `Workers` flag (`roles = "Ferryboat",
"FerryStartUnit"`, `transport_cap = 8`). The colonisation boat is handed
to `dai_manage_settler()`, parked on `SSA_AUTOWORKER`, marked `done`,
and never reaches the ferry code in its life. Recorded as
`upstream-bugs.md` #33.

### The repair was built and refused

Routing ferries with waiting demand straight to
`dai_manage_ferryboat()` — skipping the dispatch that swallows them —
is a three-line deep-side change that touches no `ai/default` code. It
was measured on the same 10 seeds against the §7 binary:

    metric                     pre      post      diff        t
    score                   189.22    190.42     +1.20     0.59
    cities                    8.68      8.68     +0.00     0.00
    mean_city_size            5.81      5.67     -0.14    -2.03
    settlers alive at end     2.73      2.15     -0.58    -2.82

    Transport boat-turns carrying    488  ->  196
    overseas picks -> arrivals    12%  ->  11%
    settler turns reaching "aboard"   86  ->   60
    settler turns "holding a boat"    10  ->    0

**The mechanism went backwards.** Freed from auto-worker duty, the
Transport runs stock's ferry manager, which makes it available, fails
to select a passenger-in-charge, and parks it — where before it at
least drifted around under `SSA_AUTOWORKER` and was boarded
opportunistically. Not shipped (`75664db56e` keeps only the metric
line and records the result in the code).

### The 12% was partly an artefact, and the churn was an escape hatch

Tracing every mission from its pick to its terminal event changed the
denominator. Overseas missions, 10 games:

    abandoned, another target picked      602   66%
    held to game end or died              203   22%
    arrived and founded                   114   12%

    last state of the ones that never arrived:  ferry=728  moved=61

Two thirds of "failed" overseas missions were the SAME settlers
re-picking. The cause is deep's own `drop-boatless` condition:
`unit_ai.ferryboat <= 0` treats `FERRY_WANTED` (-1) -- *I have asked
for a boat and I am queued* -- as *no boat was assigned*, so the
mission is dropped every turn the settler waits, and the
`dai_unit_new_task()` that drops it calls `aiferry_clear_boat()`, which
takes the flag back off and decrements `stats.passengers`. **The
request never survived to the turn's boat pass.** That is why 77% of
idle boat-turns saw nobody flagged while settlers stood on the coast.

Correcting the test to `== FERRY_NONE` was built and measured:

    metric                     pre      post      diff        t
    score                   189.22    187.72     -1.50    -0.99
    cities                    8.68      8.55     -0.13    -0.77
    settlers alive at end     2.73      2.15     -0.58    -2.00

    overseas picks           919 ->  406    abandoned  602 -> 130
    overseas arrivals        114 ->  115    delivery   12% ->  28%
    ferry-wait turns        2398 -> 3312

The churn stops and the delivery *rate* more than doubles -- but the
**absolute number of overseas cities does not move at all**, 114 to
115. The 88% loss was mostly double-counting. And the change is
slightly negative across the board, because the churn was a crude
escape hatch: a settler that gave up went back to land work, and one
that waits honestly sits in a queue nothing serves. Not shipped.

### What this establishes

Two cheap repairs were built and both were refused on measurement. The
dispatch bug is real and it is not the binding constraint. Boats
exist, are willing, are reachable, and the stock manager still does not
put settlers on them. That is RC2 with a number on it — the ferry
system's own docs call it "statistical mechanics rather than logic" —
and it says the deep-side answer is to **own the lift** rather than to
route units into stock's manager: a named boat, a named passenger, a
named beachhead, held against re-tasking, with an abort condition. The
operations layer in `deepops.c` already has that machinery, including
holds that survive `dai_manage_units()` clearing `done`.

Two things must land together when it is. The lift operation itself,
and the `== FERRY_NONE` correction above -- which is a prerequisite,
not an improvement: an honest ferry queue is what an operation would
schedule against, and on its own it only removes an escape hatch.

Not attempted here. It is an epic, not a commit, and this one spent its
budget establishing that neither cheap repair works and that the real
overseas yield is ~11 cities per game either way.

### The refused fixes are shelved, not lost (2026-08-03)

Both are real commits on branch `shelf/refused-2026-08-03`, taken from
the epic branch tip, so the next session cherry-picks rather than
re-derives:

    07c48d84ed  drop-boatless <= 0 -> == FERRY_NONE
    302f185f58  route a ferry with waiting demand past the worker dispatch

`07c48d84ed` is the one that matters. It is a **correct bug fix** --
`FERRY_WANTED` is -1, so the old `ferryboat <= 0` dropped the mission
of a settler that had just successfully asked for a boat, and the drop
itself cleared the flag through `aiferry_clear_boat()`. It measures
negative alone (score -1.50) only because the churn it removes was
acting as an escape hatch from a lift that never arrives. **It is a
prerequisite for the lift work and belongs in the same epic.**

The per-city founder pool is in history at `5d422d0b44`, superseded by
the per-site formula.
