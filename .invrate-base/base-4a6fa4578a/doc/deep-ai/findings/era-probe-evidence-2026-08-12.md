# ERA epic — probe evidence (2026-08-12)

Read-only analysis of the 32-game ERA A/B probe on `tech50war-200`.
No games run, no code changed, nothing built.

Inputs (all under
`.claude/worktrees/agent-aa720c4a228d4c2cd/benchmark/results/era-probe/`):

- `{a-pv1,b-noexpgate}/evaluate/cfg0/mixed_{db,bd}/seed-{1..8}/game.log`
  — 32 games, the three ERA counters landed at `2f5dbd9694`,
  `58940eef85`, `882880d919`: **22 391** `deep-opodds`, **109 426**
  `deep-expgate`, **437 440** `deep-wave` lines.
- the matching `scorelog.txt` (tags per `server/report.c:194`).
- seat map `mixed_db` = deep at {0,2,4,6}, `mixed_bd` = deep at
  {1,3,5,7}; confirmed — only P1/P3/P5/P7 emit deep counters in `db`,
  only P2/P4/P6/P8 in `bd`.

Arms: **A** = `benchmark/tuning/era-a-pv1.ini` (principled-v1;
`expansion_war_survival` absent ⇒ compiled 0.9). **B** = A plus
`expansion_war_survival = 0.0`.

Harness result: A **−255.5**/seed, B **−334.4**/seed. This document's
own scorelog-T200 aggregation gives A **−204.8**, B **−353.6** — same
ordering, same magnitude class (the harness scores the T201 final save;
this is the T200 scorelog row).

---

## 0. Headline: three of the 2026-08-11 forensics' load-bearing claims
## are refuted by the counters that were built to test them

| forensics claim | direct measurement | verdict |
|---|---|---|
| the expansion gate fires in **83–88%** of city-turns from t76 | **6.25%** of city-turns, whole game; 8.3% at its peak (t60–130) | **refuted** |
| `dai_on_war_footing()` is "permanently TRUE on this venue" | TRUE in **16.7%** of city-turns (7.7% / 20.3% / 19.7% by phase) | **refuted** |
| the odds cluster question: *just under the bar* vs *near zero* | **84.5%** of scored refusals return odds **exactly 0.000**; **0.4%** land in [0.60,0.65) | **answered: near zero** |
| the `UMT_SEA && coastal` clause may admit distant fleets | `by_sea = 0` in **all 437 440** wave evaluations | **closed, inert** |
| arm B = "expansion gate off" | arm B blocks **22.6%** of city-turns vs A's **6.25%** — B is **3.6× stricter** | **the A/B is inverted** |

Two of these were inferences the earlier report was explicit about
being inferences (the "both wants = 0" gate signature; the war-footing
read). They were wrong. The counters were worth building.

---

## 1. THE ODDS DISTRIBUTION — mechanism, not knob

`deep-opodds` fires once per formation attempt on the best target the
scoring loop picked, with `odds` = the best take-probability any
strongest-first prefix of the pool reached (`deepops.c:3259`,
`:3314` — the model's own return values, never a second call).

Arm A: **11 232** attempts over 16 games (0.88 per deep player-turn),
**199 formed** (1.77%), 11 033 refused. Of the refusals **4 857
(44%) had `pool = 0`** — no free attacker existed and the odds model
was never consulted. The histogram below is the 6 176 refusals where
it *was* consulted, i.e. the population the question is about.

### Histogram: best-target odds at refusal, `pool > 0`, arm A

`bar = 0.650` on every one of the 11 232 lines (the stance multiplier
never moved it).

| odds bin | t<60 | t60–130 | t>130 | ALL |
|---|---:|---:|---:|---:|
| **exactly 0** | 590 **73.5%** | 2207 **85.4%** | 2423 **86.9%** | 5220 **84.5%** |
| 0.001–0.05 | 134 16.7% | 222 8.6% | 221 7.9% | 577 9.3% |
| 0.05–0.10 | 9 1.1% | 34 1.3% | 30 1.1% | 73 1.2% |
| 0.10–0.20 | 24 3.0% | 47 1.8% | 33 1.2% | 104 1.7% |
| 0.20–0.30 | 8 1.0% | 26 1.0% | 34 1.2% | 68 1.1% |
| 0.30–0.40 | 19 2.4% | 22 0.9% | 16 0.6% | 57 0.9% |
| 0.40–0.50 | 1 0.1% | 8 0.3% | 16 0.6% | 25 0.4% |
| 0.50–0.55 | 7 0.9% | 8 0.3% | 7 0.3% | 22 0.4% |
| 0.55–0.60 | 0 0.0% | 2 0.1% | 4 0.1% | 6 0.1% |
| **0.60–0.65 (just under)** | 11 **1.4%** | 8 **0.3%** | 5 **0.2%** | 24 **0.4%** |
| **TOTAL** | **803** | **2584** | **2789** | **6176** |

Quantiles of the same population: median **0.000** in all three
phases; p90 = 0.043 / 0.007 / 0.003; max 0.643 / 0.648 / 0.643.

Arm B is the control for this table — the ops layer is untouched by the
expgate arm — and reproduces it: 83.5% exactly zero, 0.4% in
[0.60,0.65).

**The population is not sitting under the bar. It is pinned at the
floor, and it gets more pinned as the game goes on.**

### The bar has no leverage — counterfactual sweep

`odds` is the max over prefixes, so "would this attempt have formed at
bar *b*" is `odds >= b`. Arm A, all 11 232 attempts:

| bar | forms | % of attempts | × vs 0.65 |
|---:|---:|---:|---:|
| **0.65 (live)** | **199** | **1.77%** | 1.00 |
| 0.55 | 229 | 2.04% | 1.15 |
| 0.45 | 261 | 2.32% | 1.31 |
| 0.35 | 308 | 2.74% | 1.55 |
| 0.30 | 333 | 2.96% | 1.67 |
| 0.20 | 401 | 3.57% | 2.02 |
| 0.10 | 505 | 4.50% | 2.54 |
| 0.05 | 578 | 5.15% | 2.90 |

**Cutting `op_take_target` by 13× (0.65 → 0.05) buys 2.9× the
formations, and every extra formation is an operation the model says
will lose.** Halving it buys 1.67×. Only 0.70% of all refusals lie in
[0.40, 0.65) and 1.83% in [0.20, 0.65). A threshold re-derivation
cannot move this venue.

### What the model is actually saying: the force-ratio wall

The odds *are* responsive. They rise monotonically with pool size and
fall with the target's garrison. Arm A, refusals with `pool > 0`,
mean best-odds:

| def_n \ pool | 1 | 2–3 | 4–7 | 8–15 | 16+ |
|---|---:|---:|---:|---:|---:|
| 0 (empty city) | 0.049 | 0.168 | 0.344 | — | — |
| 1 | **0.000** | 0.005 | 0.055 | 0.282 | 0.614 |
| 2 | 0.000 | 0.000 | 0.030 | 0.091 | 0.380 |
| 3 | 0.000 | 0.000 | 0.006 | 0.063 | 0.000 |
| 4+ | 0.000 | 0.000 | 0.001 | 0.009 | 0.016 |

Share of attempts that clear 0.65 (n in brackets):

| def_n \ pool | 1 | 2–3 | 4–7 | 8–15 | 16+ |
|---|---:|---:|---:|---:|---:|
| 0 | 2.2% (178) | 34.0% (144) | 53.1% (49) | 100% (4) | — |
| 1 | 0.0% (618) | 0.5% (643) | 8.4% (359) | **36.1%** (72) | 66.7% (3) |
| 2 | 0.0% (439) | 0.0% (557) | 3.4% (356) | 16.2% (111) | 50.0% (2) |
| 3 | 0.0% (331) | 0.0% (528) | 0.2% (526) | 8.7% (195) | 25.0% (4) |
| 4+ | 0.0% (242) | 0.0% (408) | 0.0% (402) | **0.0%** (173) | 0.0% (31) |

At Gunpowder+, taking a city held by **one** defender needs **8–15
attackers** to be even 36% likely to clear the bar. Against **four or
more** defenders, **0 of 1256 attempts ever cleared it**, at any pool
size the game produced.

And deep's pool is:

| phase | attempts | pool = 0 | mean pool | pool ≥ 8 |
|---|---:|---:|---:|---:|
| t<60 | 2366 | **63.4%** | 0.69 | 0.1% |
| t60–130 | 4472 | **40.9%** | 1.90 | 4.2% |
| t>130 | 4394 | **34.8%** | 2.69 | 9.2% |

Deep brings a mean of 0.69–2.69 attackers to a problem that needs
8–15. It is not refusing winnable fights; **it never assembles a force
large enough for a tech-50 fight to be winnable.**

Where the army went: at t>130 a deep seat holds a mean of **13.9**
military units (scorelog tag 5) and the ops pool sees **2.69** of them
— **81% of the army is excluded** by `deep_op_collect_pool()`'s filters
(`deepops.c:2662`): `AIUNIT_DEFEND_HOME`, `charge > 0`, already claimed,
or off the medoid city's continent. The garrison draft that exists to
relieve exactly this (`deepops.c:3269`) fires on 19.4% of attempts and
adds a mean of **0.28** units, because it is gated on
`survival_target = 0.85`, which only **39.9%** of survival reads clear.

### Verdict, Q1

**Mechanism fix, not a knob.** The refused population is degenerate at
zero (84.5%), not clustered under the bar (0.4%), in every phase; the
bar sweep shows a 13× cut buys 2.9× formations. `op_take_target` is
*not* the defect — it is correctly reporting that a 2-unit stack cannot
take a tech-50 city. The defect is upstream: **the force that reaches
the odds test is an order of magnitude smaller than the era requires**,
because 44% of attempts have an empty pool and 81% of the standing army
is pinned outside it.

Secondary reading: the era sensitivity is real but it lives in the
*force ratio*, not the threshold. At tech 0 a 2-unit stack plausibly
takes a Warriors-held city; at tech 50 it needs 8–15 against one
Musketeer/Rifleman behind walls. Nothing in deep's massing machinery
scales with that.

---

## 2. THE B-ARM AUTOPSY — the experiment is inverted, not the result

### B did not turn the gate off. It turned deep's gate into stock's.

`deep_expansion_blocked()` (`deepdefence.c:3940`):

```c
if (tune->expansion_war_survival <= 0.0)  return dai_on_war_footing(...);
if (!dai_on_war_footing(...))             return FALSE;
...
return wave > 0 && surv < tune->expansion_war_survival;
```

Setting the knob to 0 does not remove a gate; it replaces a
survival-conditioned gate with an unconditional war-footing gate. The
commit that added the arm (`6dcf9fad83`) says exactly this. The
counters price it:

| arm | phase | city-turns | **blocked** | footing (= stock's verdict) |
|---|---|---:|---:|---:|
| A | t<60 | 14 624 | **2.6%** | 7.7% |
| A | t60–130 | 20 540 | **8.3%** | 20.3% |
| A | t>130 | 20 057 | **6.8%** | 19.7% |
| A | **all** | 55 221 | **6.25%** | 16.72% |
| B | t<60 | 14 646 | **7.4%** | 7.4% |
| B | t60–130 | 20 005 | **24.9%** | 24.9% |
| B | t>130 | 19 554 | **31.6%** | 31.6% |
| B | **all** | 54 205 | **22.58%** | 22.58% |

In B, `blocked == stock_blocked` on all 54 205 lines, as the code
requires. **Arm B blocks 3.6× more often than arm A.** B is the
*stricter* arm.

### So the prediction is "B founds fewer cities", and B founds fewer

Cities over time, deep side, pooled over 16 games per arm (scorelog
tag 3):

| turn | 10 | 20 | 40 | 60 | 80 | 100 | 120 | 150 | 180 | 200 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| A deep | 230 | 258 | 272 | 280 | 286 | 292 | 293 | 284 | 286 | **294** |
| B deep | 230 | 258 | 272 | 282 | 282 | 282 | 283 | 280 | 279 | **275** |
| A stock | 246 | 258 | 277 | 324 | 352 | 355 | 357 | 367 | 367 | 363 |
| B stock | 246 | 258 | 277 | 329 | 358 | 368 | 374 | 379 | 381 | **387** |

The arms are identical to t60 (the gates agree while nobody is on a
war footing) and separate exactly where the block rates separate.

### There is no walk-in story. The kill and loss ledgers are the same.

Deep side, pooled T200:

| | A deep | B deep | A stock | B stock |
|---|---:|---:|---:|---:|
| units_killed | 2289 | 2173 | 5721 | 5734 |
| units_lost | 3726 | 3710 | 5040 | 5039 |
| units_built | 5054 | 4927 | 6759 | 6850 |
| munits @200 | 911 | 927 | 1347 | 1389 |
| settlers @200 | 2 | 6 | 48 | 40 |
| cities | 294 | 275 | 363 | 387 |
| wonders | 31 | 34 | 59 | 80 |
| culture | 103 061 | 97 288 | 113 571 | 126 454 |
| **score** | **12 124** | **11 743** | **13 762** | **14 572** |

Deep's losses are flat between arms (3726 → 3710, −0.4%) and its kills
are flat (2289 → 2173, −5%). **B did not found more cities and then
lose them.** B's extra deficit is almost entirely civil and almost
entirely on the *stock* side of the ledger: stock gained 24 cities,
21 wonders and 12 883 culture (+258 score points of culture/50 alone)
in the room deep's stricter gate left empty.

Per-seed, mirrored (`sum(deep) − sum(stock)` over db+bd at T200):

| seed | A delta | B delta |
|---:|---:|---:|
| 1 | −4 | −513 |
| 2 | −204 | −362 |
| 3 | −151 | −51 |
| 4 | −204 | −114 |
| 5 | **+189** | −178 |
| 6 | −398 | −540 |
| 7 | −398 | −315 |
| 8 | −468 | −756 |
| **mean** | **−204.8** | **−353.6** |

B is worse on 5 of 8 seeds; the paired difference is
**−148.9/seed (SD 229.8, SEM 81.2, t = −1.83)** — directional, not
significant at n = 8.

Arm A seeds 1–3 against the calib2 games the forensics used
(calib2 in brackets): bd/s1 468|883 (616|885), bd/s2 779|952
(780|955), bd/s3 731|841 (730|970), db/s1 1042|631 (1043|634),
db/s2 845|876 (849|880), db/s3 823|864 (826|867). Four of six
seats reproduce to within 5 points; two differ (bd/s1 deep, bd/s3
stock), consistent with the branch tip carrying merged work beyond
the calib2 binary. The counters are not perturbing play.

### Verdict, Q2

**Un-gating expansion was never tested.** Arm B is deep with stock's
*stricter* gate; it blocks 3.6× more, founds 19 fewer cities, and loses
149/seed relative to A. Read correctly, **B is evidence in favour of
deep's replacement gate, and the strongest evidence the probe
produced**: deep's survival-conditioned gate is worth roughly
+149/seed against the stock war-footing gate it replaced, on the venue
where deep loses.

The forensics' mechanism #2 ("the gate is the reason deep stops
founding") is dead either way: the gate fires in 6.25% of city-turns,
and `founder_want > 0` in only **5.0% of *unblocked*** city-turns
(11.0% at t<60, 3.1% at t60–130, 1.9% at t>130). Deep stops founding
because `deep_contemplate_new_city()` stops wanting cities, not because
the gate zeroes a want that exists. That decision is not instrumented.

Supporting number: on the 16.7% of city-turns where the survival model
was actually consulted (footing = 1), `surv ≥ 0.90` in **65.4%**
(t<60: 66.8%, t60–130: 58.9%, t>130: 65.4%) and `surv = 0` in 15–17%.
The distribution is bimodal, not a mass under 0.9 — the
`expansion_war_survival = 0.9` placeholder is *also* not a threshold
with much to tune.

---

## 3. THE WAVE CENSUS — the sea clause is inert; the land arm is not

437 440 evaluations, 32 games. Route sums equal `n_att` on all 437 400
uncapped rows (census integrity verified).

| phase (arm A) | n | mean n_att | mean n_def | mean surv | n_att ≥ 16 | **capped** |
|---|---:|---:|---:|---:|---:|---:|
| t<60 | 30 031 | 2.42 | 1.03 | 0.365 | 0.00% | 0.00% |
| t60–130 | 94 495 | 4.26 | 1.75 | 0.477 | 1.31% | 0.00% |
| t>130 | 98 627 | 6.08 | 2.26 | 0.515 | 6.22% | 0.00% |

`n_att` distribution (arm A): 1–2 37.5%, 3–4 24.3%, 5–8 21.8%,
9–15 13.1%, 16–31 3.3%, 32 (cap) 0.002%. `capped = 1` on **40 of
437 440** rows across both arms.

### Route shares — all 32 games, both arms

| route | contributors | share |
|---|---:|---:|
| land | 1 069 774 | **99.5%** |
| **sea (`UMT_SEA && coastal`)** | **0** | **0.000%** |
| air (airborne) | **0** | **0.000%** |
| transport (passenger aboard a hull) | 4 782 | 0.5% |

`by_sea = 0` and `by_air = 0` on **every one of the 437 440 lines**.
The forensics' open worry — that the unbounded coastal clause at
`deepcombat.c:434` admits the enemy navy — is **closed: it admits
nothing on this venue.** (Warships in port sit on the city's land tile
and count as `by_land`; airborne types are past techlevel 50.) The
clause is dead code here, so bounding it would change nothing and must
not be phase-3 work.

### But the reach is real, on the land arm

`horizon = 3` on all rows; `max_turns` = 3 on 76.7%. Distance of the
furthest qualifying attacker:

| phase | mean max_dist | ≥ 8 | ≥ 12 | ≥ 16 |
|---|---:|---:|---:|---:|
| t<60 | 9.19 | 66.8% | 29.1% | 8.7% |
| t60–130 | 10.99 | 74.2% | 48.1% | 23.2% |
| t>130 | 11.55 | 75.3% | **58.1%** | **27.8%** |

On a 32×32 torus (observed maximum `real_map_distance` 18), a
three-turn horizon times a Cavalry/Dragoon move rate reaches most of
the continent — and the qualification test is
`real_map_distance / move_rate` with **no terrain cost, no ZOC, no
pathfinder and no fog** (`deepcombat.c:443`). By t>130, 58% of survival
verdicts are shaped by a unit at least 12 tiles away that is assumed to
arrive.

Survival responds to that: mean surv 0.706 at n_att 1–2, 0.147 at
9–15, 0.028 at 16+; and 0.372 at n_def = 1 versus 0.978 at n_def ≥ 6.

Arm A vs B, whole game: mean n_att 4.82 / 4.99, n_def 1.88 / 1.90,
surv 0.479 / 0.461 — the arms see the same world.

### Verdict, Q3

**The wave is not grossly inflated in size** — mean 4.8, `DEEP_WAVE_MAX`
reached 40 times in 437 440 evaluations, and the sea/air clauses
contribute exactly zero. The forensics' spot-check was right about the
magnitude and right to doubt its own sea hypothesis. **What is
unbounded is the land arm's straight-line reach**: 58% of late-game
verdicts include an attacker ≥12 tiles out, admitted by a
distance-over-move-rate test that ignores terrain and ZOC. That is a
candidate mechanism for over-fearfulness, but this probe only shows the
reach, not that it is wrong — it needs its own instrument (the
pathfinder's turn estimate beside the straight-line one) before anyone
touches it.

---

## 4. EXPGATE VS STOCK — deep's gate is strictly the more permissive one

Arm A contingency, 55 221 city-turns:

| phase | blocked=0 stock=0 | blocked=0 **stock=1** | blocked=1 stock=1 | **blocked=1 stock=0** |
|---|---:|---:|---:|---:|
| t<60 | 13 496 (92.3%) | 754 (5.2%) | 374 (2.6%) | **0** |
| t60–130 | 16 378 (79.7%) | 2 450 (11.9%) | 1 712 (8.3%) | **0** |
| t>130 | 16 112 (80.3%) | 2 579 (12.9%) | 1 366 (6.8%) | **0** |
| **all** | 45 986 (83.3%) | **5 783 (10.5%)** | **3 452 (6.25%)** | **0 (0.00%)** |

**Deep never blocks where stock would not** — structurally guaranteed
by the `if (!footing) return FALSE` early exit, and confirmed empirically
on 55 221 rows. Deep blocks on 6.25% of city-turns against stock's
16.72%: **deep's gate lets expansion through on 5 783 city-turns
(10.5%) that stock's would have stopped**, and the A-vs-B score
difference is what that permissiveness is worth (+149/seed for A).

What the gate destroys when it does fire is small: on blocked rows
`founder_want > 0` in **1.0%** and `worker_want > 0` in **1.7%**
(unblocked: 5.0% and 19.4%). The opportunity cost of deep's gate is
~35 founder-wants and ~59 worker-wants per 55 221 city-turns.

### Verdict, Q4

Deep's replacement gate is priced and it is **cheap and positive**.
It is not a phase-3 target. The remaining question it raises — what a
world with *no* gate at all looks like — is not answerable from this
probe and needs a code path that does not exist (the knob has no
"never block" setting; `<= 0` means "use stock's").

---

## 5. Instrumentation defect found while reading the streams

`deep_expgate_log()` (`ai/deep/deepdefence.c:3930`) passes
`city_data->choice.want` — an `adv_want`, i.e. **`float`**
(`common/fc_types.h:1052`) — to a **`%d`** conversion. The float is
promoted to `double` and passed in an SSE register while `%d` reads the
next general-purpose argument, so the varargs shift by one from that
point:

- **`mil=` actually carries `city_size_get(pcity)`** (verified: `mil=1`
  on turn-1 cities, `mil` ranges 1..20 like a size, never like a want);
- **`size=` is uninitialised garbage** (observed values ≈ 3.7e8, drifting
  monotonically like a stack address).

Consequences for this document: `mil` was not used, and the gate's
second conjunct (`choice.want > 0` at `deepdefence.c:6938`) is
**unobserved** — the *effective* firing rate the counter was written to
make derivable is not derivable. Every other field is before the shift
and is sound. Fix is one line (`%d` → `%.1f`, or cast); it is a logging
bug with no behavioural effect, but it should be corrected before the
counter is trusted for the second conjunct.

---

## 6. Phase-3 recommendation

### Do not tune `op_take_target`. Do not touch the expansion gate. Do not bound the sea clause.

All three are priced above and all three are dead ends: the bar sweep
gives 2.9× for a 13× cut, the gate fires on 6.25% of city-turns and is
worth +149/seed as it stands, and the sea clause admits zero units.

### The one thing to change: make the massing layer see the era

The measured defect is a **force-ratio** defect. At tech 50 the odds
model says 8–15 attackers are needed against one defender and 4+
defenders are unreachable at any pool size the game produces; deep
brings 0.69–2.69, and 44% of the time brings nothing. Three
sub-mechanisms feed that, in descending order of measured mass:

**3a. The pool filter, not the bar, is the binding constraint
(44% of attempts, `pool = 0`).** `deep_op_collect_pool()`
(`deepops.c:2662`) excludes every `AIUNIT_DEFEND_HOME` unit
unconditionally; 81% of a deep seat's 13.9 late-game military units
never appear. The garrison draft that exists to relieve this contributes
0.28 units/attempt because it is gated on `survival_target = 0.85`,
which only 39.9% of survival reads clear.
*Change:* make the draft's release test **relative** — release a blade
when the marginal survival it provides is smaller than the marginal
take-probability it buys — rather than an absolute survival floor. That
is a mechanism change in `deepops.c` around `:3269`, and it needs no new
constant: both quantities are already computed
(`deep_city_stack_survival_without()` and
`deep_take_odds_at_arrival()`).

**3b. Nothing accumulates force across turns.** The formation attempt
is memoryless: 0.88 attempts per player-turn, each one asking "can I
take it *now*", each one answering no, each one leaving no state behind.
An era where 8–15 attackers are required needs a *muster* that persists
across turns and grows. The `DEEP_OP_MUSTER` state already exists but is
only reachable *after* a formation clears the bar — the bar it cannot
clear without the muster.
*Change:* allow a slot to enter `MUSTER` on a **target worth taking**
plus a **credible force trajectory**, and let the odds test be the
*launch* gate it already is (`deepops.c:4311`) rather than the
*formation* gate as well. This is the highest-value change and the
largest; it is its own epic, not a phase-3 commit.

**3c. The survival model's land reach is unbounded and terrain-free.**
58% of late-game verdicts admit an attacker ≥12 tiles away via
`real_map_distance / move_rate` with no terrain, ZOC or pathfinder.
This inflates `n_att` (mean 6.08 at t>130) and therefore depresses
survival everywhere — which is what pins units at `DEFEND_HOME` and
starves 3a. *Do not change this yet.* Instrument it first: log the
pathfinder's turn estimate beside the straight-line one at
`deepcombat.c:443` and measure the gap. Guessing here is exactly the
failure mode `00-context.md` §5 names.

### If a threshold is nonetheless wanted

Any threshold that survives this analysis (`op_abort_floor`,
`survival_target`, `expansion_war_survival`) must be **tuned on
`tech50war-200`'s training seeds via `benchmark/tune.py`** and reported
on held-out seeds — never picked. Note that the evidence here says the
gradient is small on all three: the odds distribution is degenerate at
zero and the gate-survival distribution is bimodal at 0 and ≥0.9. A
tuning pass on these knobs should be expected to find nothing, and that
result is worth recording.

### The A/B that would prove 3a

Arm A (control) = `era-a-pv1.ini` as run here. Arm A′ = A with the
garrison draft's absolute `survival_target` gate replaced by the
marginal comparison, one commit, no new constant. Mirrored, ≥16 seeds
on `tech50war-200` (n = 8 gave SEM 81 on the A−B contrast; the effect
being chased is smaller than that).

**Pre-registered predictions, all readable from the counters already
landed** — this is the point of pricing them now:

| counter | prediction if 3a is the mechanism |
|---|---|
| `deep-opodds` `pool = 0` share | falls from 44% |
| `deep-opodds` mean pool | rises from 2.69 (t>130) toward 8 |
| `deep-opodds` odds histogram | mass moves out of "exactly 0" |
| `deep-op form` count | rises from 199/16 games |
| `deep-op strike` count | rises from 36/16 games |
| scorelog `units_killed` | rises from 2289 toward stock's 5721 |

If the pool rises and the odds histogram does **not** move out of the
zero bin, 3a is wrong and 3b is the whole story. That is a clean
falsification and it costs one arm.

### What must not be claimed

n = 8 mirrored. A = −204.8/seed (harness −255.5), B = −353.6 (harness
−334.4), paired difference −148.9 ± 81.2 SEM. **No magnitude claim about
the tech-50 gap or about the gate's worth is significant at this n.**
The mechanism measurements — 22 391 formation attempts, 109 426 gate
evaluations, 437 440 wave evaluations — are the solid part of this
document. The score deltas are directional context.

---

## 7. Reproduction

Extraction was throwaway `awk` over the 32 `game.log` files (never read
whole — ~8–14 MB each, split by tag into per-game temp files first).
Field layouts as emitted by:

- `deep-expgate` — `ai/deep/deepdefence.c:3930`, one line per
  `deep_expansion_blocked()` return. **`mil` is city size and `size` is
  garbage** (§5).
- `deep-opodds` — `ai/deep/deepops.c:3073`, called at
  `deepops.c:3328` (`refused`) and `:3360` (`formed`). One line per
  formation attempt, on the single best target; targets scored and
  discarded earlier in the loop are still invisible.
- `deep-wave` — `ai/deep/deepcombat.c:348`, one line per
  `deep_city_survival_core()` call, including the `_without()`
  counterfactual reads, so a city under repeated evaluation is
  over-represented in the row counts (route *shares* and per-row
  distributions are unaffected).
- scorelog tags per `server/report.c:194`: 0 pop, 3 cities, 4 techs,
  5 munits, 6 settlers, 7 wonders, 25 score, 26 built, 27 killed,
  28 lost, 29 culture.
