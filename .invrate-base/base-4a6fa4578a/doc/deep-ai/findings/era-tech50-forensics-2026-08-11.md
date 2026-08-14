# ERA epic — tech-50 forensics (2026-08-11)

Read-only mechanism forensics on the six `calib2-tech50war-mixed` games
behind `findings/tech50-collapse-2026-08-10.md`. No games run, no code
changed. Evidence: the six final saves (`freeciv-T0201-…-final.sav.zst`,
which turn out to be present and parseable), the six `scorelog.txt`, and
the six `game.log` files — which contain deep's own metric stream
(`deep-def`, `deep-bid`, `deep-surv`, `deep-op`, …), ~29k city-turn
records over 4800 deep player-turns.

---

## 0. Headline: the finding's framing needs revising

The collapse is **not** an expansion failure and **not** a spacing
failure. Decomposing the final score by its actual formula
(`server/score.c:355` — `citizens + 2·techs + 5·wonders + built/10 +
killed/3 + culture/50`):

| component (pooled, 24 seats/side) | deep | stock | delta |
|---|---:|---:|---:|
| citizens | 761 | 781 | **−20** |
| 2 × techs | 2592 | 2620 | **−28** |
| 5 × wonders | 80 | 120 | **−40** |
| units_built / 10 | 176 | 218 | **−42** |
| **units_killed / 3** | **326** | **612** | **−286** |
| culture / 50 | 901 | 836 | **+65** |
| **total** | **4836** | **5187** | **−351** |

**82% of the score gap is `units_killed/3`. Strip the two military
terms and the two AIs are level: deep 4334, stock 4357 — −23 over six
games, −3.8/game.** Citizens, techs and culture are a wash; deep is
ahead on culture and dead level on population despite holding 19 fewer
cities.

The turn-by-turn form is even cleaner (scorelog, pooled over six games):

| turn | 20 | 40 | 60 | 80 | 100 | 120 | 140 | 160 | 180 | 200 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| civil diff (pop+2·tech+cult/50) | +37 | +159 | +131 | +34 | +95 | +4 | +20 | −30 | −74 | **−1** |
| military diff (built/10+killed/3) | +2 | +6 | −10 | −46 | −92 | −127 | −171 | −229 | −266 | **−317** |
| total diff | +37 | +167 | +119 | −18 | −3 | −141 | −177 | −291 | −386 | **−359** |

Deep **leads** to turn ~70 and the entire deficit is the military terms,
opening monotonically from **t60**. The civil score never diverges.

### The canon's blind spot is not "tech 50" — it is "tech 50 past t80"

`benchmark/configs/lategame-80.conf` is **also `techlevel 50`** with the
same `startunits ccccccccxxwwwwwwwwww` and `dispersion 3`. The
2026-08-10 claim that "every combat venue in the canon starts at tech 0"
is inaccurate; what is true is that the only tech-50 venue in the canon
**ends at turn 80**, which is exactly the turn deep's score curve
crosses. `tech50war-200` differs from `contested` in four ways at once
(techlevel 50, startunits, dispersion 3, endturn 200 not 150) and from
`lategame-80` in three (8p/32×32 not 6p/40×40, endturn 200 not 80, no
migration). Attribution to "tech 50" alone is not yet earned; the
horizon is at least as good a candidate and is directly supported by the
crossover turn.

---

## 1. Evidence

### 1.A City spacing — hypothesis (a) is REFUTED

Nearest-neighbour own-empire distance, measured from the final saves
with freeciv's own metric (`real_map_distance`, iso-hex, WRAPX|WRAPY
32×32 — `common/map.c:622`), restricted to **self-founded** cities
(`original` field == owner, so conquests are excluded):

| | n | median | mean | ≤3 | ≤4 | ≥6 |
|---|---:|---:|---:|---:|---:|---:|
| deep | 84 | **3.0** | **3.96** | 67% | 82% | 15% |
| stock | 109 | **3.0** | **4.67** | 68% | 80% | 17% |

`citymindist = 3`. **Both AIs plant at the ruleset floor**, and deep's
mean is *tighter* than stock's — stock's larger mean comes from a tail
of far-flung amphibious grabs (nn = 14, 15, 16, 21, 22). Deep is not
spreading out. This independently reproduces the `largepox-200.conf`
header's own conclusion ("the median founded city sits exactly at
citymindist — spacing is pinned by the ruleset minimum, not chosen").

Deep's cities are **bigger**: mean size 6.79 v 5.96, and total citizens
761 v 781 — 15% fewer cities carrying the same population. Whatever
deep is doing with sites, it is not costing it citizens.

### 1.B Founding and settlers — the gate is real, and nearly irrelevant

Surviving-city founding turns (pooled):

| | n | t≤10 | 11–50 | 51–100 | 101–150 | 151+ |
|---|---:|---:|---:|---:|---:|---:|
| deep | 112 | 89 | 12 | 9 | 0 | 2 |
| stock | 131 | 95 | 19 | 14 | 2 | 1 |

Standing settlers over time (scorelog tag 6, pooled):

| turn | 10 | 20 | 40 | 60 | 80 | 100 | 140 | 200 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| deep | 45 | 22 | 21 | 7 | 7 | 8 | 3 | **1** |
| stock | 99 | 105 | 160 | 124 | 83 | 57 | 36 | **28** |

Deep's settler pool collapses by t60 and never recovers — **one settler
alive across all six games at T201, against stock's 28.** But stock's
160-settler pool at t40 buys it *no city advantage at t40* (101 v 102):
stock is hoarding settlers it cannot place. The whole realised
difference is 19 cities ≈ **−20 score points of the −351.**

City counts, pooled: t5 52|70, t10 87|93, t20 93|98, t50 106|112,
t100 114|126, t200 112|131. (Note t5: deep plants its eight free
settlers ~5 turns slower than stock, then nearly catches up by t20 — a
small opening-tempo cost the earlier "t10 tempo is EQUAL" reading
missed, worth ~6 cities.)

### 1.C The gate firing rate — measured, from deep's own log

`deep-def` (`ai/deep/deepdefence.c:143`) emits `founder=` / `worker=`
per city-turn, reading the cached wants the build pass wrote. The
comment at `deepdefence.c:3644` names the signature of the expansion
gate: **both wants pinned to zero.**

| turns | city-turns | founder=0 | worker=0 | **both=0 (gate signature)** | ndef mean | menace mean | median danger |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1–10 | 500 | 72.6% | 40.6% | **27.6%** | 0.00 | 0.000 | 0 |
| 11–25 | 1389 | 86.8% | 26.3% | **22.2%** | 0.27 | 0.547 | 0 |
| 26–50 | 2518 | 67.1% | 65.8% | **46.9%** | 0.86 | 0.969 | 658 |
| 51–75 | 2719 | 76.2% | 84.4% | **69.2%** | 1.11 | 0.999 | 1286 |
| 76–100 | 2820 | 90.1% | 90.6% | **82.9%** | 1.36 | 0.998 | 1360 |
| 101–150 | 5633 | 97.2% | 91.1% | **88.3%** | 1.70 | 1.000 | 1476 |
| 151–200 | 5482 | 93.6% | 89.9% | **83.9%** | 1.84 | 0.991 | 2450 |

**The gate fires in 83–88% of city-turns from t76 onward.** That is the
direct, measured answer to "why does deep stop founding after t10".

Two things the same table says:

- **`menace` is saturated at 1.0 from t26 to the end of the game.**
  `deep_garrison_menace()` (`deepdefence.c:1188`) is
  `MIN(1.0, slope · Σ(every non-allied player's `score.units`) / ours)`.
  In an 8-player free-for-all that ratio is 10–40×; the clamp bites
  permanently. The knob `garrison_menace_slope` has **no gradient at all
  on this venue** and the tuner cannot move it. *This is not tech-50
  specific* — `contested.conf` is also 8p/32×32, so this saturation
  exists on the venue deep wins by +55.3. It is a dead knob, not the
  inversion.
- **`ndef` is only 0.86–1.84.** Deep is not stuffing its cities with
  defenders. The gate says "this city will fall" and deep then does not
  garrison it enough to change that verdict — a self-sustaining trap,
  but not an over-garrisoning one at the city tile.

`deep-bid` (`deepdefence.c:6369`) prices the founder against its rivals
in the same city-turn:

| turns | n | founder>0 | founder p90 | mil median | mil>0 | build median | founder>mil | founder<0 (needs boat) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1–10 | 591 | 24.7% | 670 | 100 | 93.9% | 316 | 22.7% | 0.0% |
| 26–50 | 2529 | 16.5% | 870 | 225 | 95.8% | 519 | 15.1% | 16.4% |
| 51–75 | 2727 | 7.7% | 0 | 230 | 94.8% | 545 | 6.7% | 15.7% |
| 76–100 | 2827 | 4.0% | 0 | 232 | 93.0% | 419 | 3.4% | 5.1% |
| 101–150 | 5633 | 0.9% | 0 | 211 | 80.3% | 257 | 0.8% | 2.1% |
| 151–200 | 5490 | 3.6% | 0 | 225 | 88.2% | 153 | 3.0% | 2.9% |

Note `mil>0` in **93–96%** of city-turns. The gate's second conjunct
(`city_data->choice.want > 0`, `deepdefence.c:6904`) is therefore
essentially always true, so the gate reduces to `deep_expansion_blocked()`
alone. When the founder *does* bid it bids big (p90 = 870 at t26–50,
beating the building anchor) — it is not losing an auction, it is being
zeroed before the auction.

### 1.D Army trajectory — deep front-loads, then is out-produced

Military units (scorelog tag 5), pooled:

| turn | 20 | 30 | 40 | 50 | 60 | 80 | 100 | 120 | 150 | 200 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| deep | 43 | 111 | 154 | 186 | 212 | 242 | 253 | 267 | 280 | 294 |
| stock | 9 | 45 | 87 | 149 | 199 | 284 | 366 | 379 | 417 | 450 |

Per city at t30: deep **1.12**, stock **0.46**. Deep carries 2.4× the
military per city through the expansion window, is overtaken at **t80**,
and ends at 65% of stock's army. The army crossover and the score
crossover are the same event.

Losses are close to symmetric; kills are not. Pooled at T201: deep
built 1892 / killed 1007 / lost 1433; stock built 2298 / killed 1860 /
lost 1684. `units_lost` counts only combat deaths (`unittools.c:2043`;
upkeep/disband go to `units_used`), so this is real: **deep suffers 46%
of the combat deaths in these worlds and inflicts 32% of them.**
Kill:loss deep 0.70, stock 1.10.

> Caveat on the earlier "2:1 kill exchange against deep" reading: it is
> not a deep-vs-stock exchange rate. The diplstate matrix at T201 shows
> **cross-side alliances and same-side wars in every game** (e.g. db/s1:
> 6 cross-side alliances, 6 same-side wars). Deep seats fight deep
> seats; barbarians take a share. The aggregate is a global body count,
> not a head-to-head one.

### 1.E Production mix — where the shields go

`deep-def`'s `prod=` field, as a share of city-turns (≈ share of
shields, since an expensive item occupies proportionally more turns):

| turns | top six production targets |
|---|---|
| 1–10 | Riflemen 40% · Colosseum 12% · Transport 10% · Destroyer 10% · **Settlers 9%** · Galleon 5% |
| 11–25 | Riflemen 49% · Diplomat 11% · Ironclad 8% · Destroyer 7% · Musketeers 4% · Colosseum 4% |
| 26–50 | Riflemen 26% · Destroyer 14% · City Walls 10% · Diplomat 9% · **Coastal Defense 9%** · Ironclad 8% |
| 51–75 | Riflemen 25% · **Coastal Defense 23%** · Destroyer 14% · Diplomat 8% · City Walls 6% · Colosseum 4% |
| 76–100 | Riflemen 22% · Destroyer 19% · **Coastal Defense 16%** · Diplomat 8% · Colosseum 6% · City Walls 4% |
| 101–150 | Riflemen 17% · Destroyer 15% · Diplomat 11% · Colosseum 10% · Coastal Defense 9% · Supermarket 6% |
| 151–200 | Destroyer 17% · Riflemen 13% · Diplomat 12% · Marines 10% · Cruiser 5% · Transport 5% |

**Settlers vanish from the production mix entirely after turn 10.**
**Coastal Defense peaks at 23% of all production in t51–75** and City
Walls adds another 6–10% at t26–75; combined, roughly a third of deep's
mid-game shields go into static defensive masonry. Marines — the
amphibious attacker that earns kills — do not appear until t151.

Standing composition at T201, pooled:

| unit | deep | stock | | unit | deep | stock |
|---|---:|---:|---|---|---:|---:|
| Riflemen | 149 | 200 | | **Engineers** | **105** | **30** |
| **Marines** | **63** | **119** | | **Diplomat** | **101** | **83** |
| **Destroyer** | **47** | **100** | | **Settlers** | **1** | **28** |
| Cavalry | 17 | 6 | | Partisan | 2 | 18 |
| Transport | 39 | 39 | | Explorer | 7 | 13 |

Deep's standing force is **47% civilian** (258 of 553); stock's is
**31%** (202 of 657). Engineers and Settlers cost 30 each in civ2civ3,
so the standing civilian surplus is only ~2000 shields (≈40 Riflemen)
against a 160-unit military deficit — civilian over-investment is real
but is **at most a quarter** of the army gap. The rest is fewer units
built (1892 v 2298) against 13% less end-game production (mfg 815 v
937), i.e. deep's smaller empire plus its masonry.

### 1.F Offensive operations — the engine is not running

`deep-op` verbs across all six games (24 deep player-games, 4800 deep
player-turns):

| verb | count |
|---|---:|
| raid | 380 |
| poach | 169 |
| **form** | **83** |
| **abort-odds** | **72** |
| march | 55 |
| retarget | 45 |
| regate | 42 |
| lift | 21 |
| **strike** | **18** |
| invade | 10 |

**83 operations formed in 4800 player-turns — one per 58 player-turns —
of which 72 aborted on odds and 18 reached a strike.** Mean at
formation: `required = 5.2, pool = 5.9, drafted = 0.95`. An operation
that drafts one unit is not an operation.

Meanwhile stock has no such gate at all: `find_something_to_kill()` sends
each unit at whatever it can reach, and banks 1860 kills for 612 score
points.

---

## 2. Code trace: the founding decision path

Entry, per city, in deep's build-choice pass:

```
deepdefence.c:6890   city_list_iterate(pplayer->cities, pcity)
deepdefence.c:6894     if (city_data->choice.want <= 0)
deepdefence.c:6897       choice = deep_military_choose_build(...)    <- mil bid, >0 in 93-96%
deepdefence.c:6903     if (deep_expansion_blocked(ait, pplayer, pcity)
deepdefence.c:6904         && city_data->choice.want > 0) {
deepdefence.c:6905       city_data->worker_want  = 0;
deepdefence.c:6906       city_data->founder_want = 0;
deepdefence.c:6907       city_data->founder_turn = game.info.turn;   <- recompute next turn
deepdefence.c:6912       continue;                                    /* "Go, soldiers!" */
deepdefence.c:6940     if (city_data->founder_turn <= game.info.turn)
deepdefence.c:6942       deep_contemplate_new_city(ait, pcity)        -> deepsite.c:1252
deepdefence.c:6944       founder_turn = turn + rand(AI_CITY_RECALC_SPEED) + AI_CITY_RECALC_SPEED
deepdefence.c:6959       founder_want *= 1 + settler_rush_weight * deep_map_crowding()
deepdefence.c:6976       founder_want *= deep_stance_expand_mult(pplayer)   [gate 4, W34 A.2]
```

The gate itself:

```c
/* ai/deep/deepdefence.c:3905 */
static bool deep_expansion_blocked(ait, pplayer, pcity) {
  if (tune->expansion_war_survival <= 0.0)  return dai_on_war_footing(ait, pplayer);
  if (!dai_on_war_footing(ait, pplayer))    return FALSE;
  surv = deep_city_stack_survival(&wld.map, pcity, tune->survival_horizon, &wave);
  return wave > 0 && surv < tune->expansion_war_survival;      /* 3924 */
}
```

`dai_on_war_footing()` (`ai/default/daidiplomacy.c:1968`) is TRUE if any
player carries `countdown >= 0` — the sticky presence flag the comment at
`deepdefence.c:3635` documents. On this venue every seat is at war with
4–7 others by t30, so it is permanently TRUE and deep's gate collapses
to `wave > 0 && surv < 0.9`.

The survival number, and its era sensitivity:

```
deepcombat.c:224   deep_city_stack_survival(nmap, pcity, horizon, &wave)
deepcombat.c:312     deep_city_survival_core(...)
deepcombat.c:338       players_iterate_alive(aplayer)      at-war + contact-made only
deepcombat.c:372         flies_in = other continent && airborne && dist <= air_reach
deepcombat.c:377         skip if other continent && !transported
                              && !(UMT_SEA && city is coastal) && !flies_in
deepcombat.c:387         rate  = MAX(at->move_rate, 1)
deepcombat.c:388         turns = ceil(real_map_distance(unit, ctile) / moves_per_turn)
deepcombat.c:390         if (turns > horizon) continue
deepcombat.c:393         deep_combatant_add(att, &n_att, DEEP_WAVE_MAX=32, aunit, att_rating)
deepcombat.c:142     deep_garrison_vs_stream_full(): per-pair
                       att_str = deep_fresh_attack_power(), def_str = get_total_defense_power(),
                       firepower from get_modified_firepower()  -> deepodds.c chained duel
```

Measured behaviour of this function on these games (`deep-surv`, 11.5k
reads):

| turns | n | wave median | wave mean | wave ≥16 | **p median** | **p < 0.9** | p < 0.5 | p < 0.1 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 26–50 | 817 | 1.0 | 1.94 | 0.0% | **0.274** | **71.1%** | 55.8% | 44.4% |
| 51–75 | 1612 | 2.0 | 2.79 | 0.2% | **0.213** | **67.6%** | 59.3% | 47.5% |
| 76–100 | 1779 | 3.0 | 3.41 | 1.2% | **0.391** | **63.9%** | 54.7% | 41.9% |
| 101–150 | 3357 | 3.0 | 4.23 | 3.9% | 0.902 | 49.9% | 39.4% | 29.3% |
| 151–200 | 3891 | 3.0 | 4.23 | 1.6% | 0.919 | 49.3% | 41.1% | 31.4% |

The waves are **small** (2–4). I expected the sea-reachability clause
(`deepcombat.c:379`) to admit the whole enemy navy — a Destroyer moves 6
and `horizon = 3`, giving an 18-tile reach on a 32×32 torus whose
maximum distance is ~16 — and the data does **not** support that: the
wave never approaches `DEEP_WAVE_MAX = 32`. The low survival comes from
the **duel odds**, not the wave size: 2–4 tech-50 attackers (Cavalry
a=8, Marines a=8, Cruiser a=6) against the 0.9–1.8 defenders deep
actually keeps on the city tile.

So the era sensitivity is in the *numerator of the comparison*, and the
threshold it is compared to is not:

**`expansion_war_survival = 0.9` (`deeptuning.c:61`, marked
"PLACEHOLDER, untuned")** demands a 90%-survival city before deep will
consider a settler. Median survival on this venue is 0.21–0.39 through
t100. The threshold was set where early-era survival numbers live; at
Gunpowder+ nothing clears it.

### Every knob and every era-sensitive quantity on this path

| knob | file:line | value in these games | era-sensitive input it is compared against |
|---|---|---|---|
| `expansion_war_survival` | `deeptuning.c:61` | **0.9** (compiled default; principled-v1 does not override) | duel-model survival — falls with attacker `attack` |
| `survival_horizon` | `deeptuning.c:167` | 3 turns | × unit `move_rate` ⇒ reach radius grows with era |
| `survival_target` | `deeptuning.c:165` → **0.85** in principled-v1 | 0.85 | same duel model, used by garrison sizing |
| `garrison_floor` / `_danger` | `deeptuning.c:72,73` | 1 / 40 | absolute unit counts, era-free |
| `garrison_menace_slope` | `deeptuning.c:74` | 1.0 | **saturated at the 1.0 clamp from t26** — dead knob here |
| `menace_overseas_weight` | `deeptuning.c:75` | 0.25 | irrelevant while clamped |
| `birth_guard_gain` | `deeptuning.c:98` | **0.0 (off)** | — hypothesis (b)'s birth guard is *not even enabled* |
| `settler_fearfulness_mult` | `deeptuning.c:32` → **1.5** in principled-v1 | 1.5 × `NORMAL_STACKING_FEARFULNESS` | multiplies pf danger cost ∝ enemy `attack` ⇒ scales with era |
| `settler_risk_tolerance` | `deeptuning.c:33` | 1.0 | bar is `a²·10 ≥ d·tol` (`deepsettler.c:162`); `a` = Σ adjacent enemy attack rating, `d` ≈ 10–20 for a D=1 settler ⇒ **quadratic in enemy attack, saturated at tech-50 and near-saturated at tech-0** |
| `settler_guard_tolerance` | — | — | same bar, escort decision (`deepsettler.c:319`) |
| `site_travel_discount` / `_crowding` | `deeptuning.c:54,55` | 0.9 / 0.0 | `amortize()` over pathfinder turns; era-free |
| `site_settler_cost_weight` / `site_boat_cost_weight` | `deepsite.c:1193` | — | subtract shields from a tile-yield sum |
| `site_enough_result` | `deeptuning.c:35` | 250.0 | absolute want scale |
| `settler_rush_weight` | `deeptuning.c:113` | 1.0 | × `deep_map_crowding()` |
| `scale_settler` | `deeptuning.c:222` | 1.0 (no rescale) | — |
| `op_take_target` | `deeptuning.c:182` → **0.65** in principled-v1 | 0.65 | take-probability from the *same* duel model ⇒ falls against Riflemen-behind-walls |
| `op_abort_floor` | `deeptuning.c:194` → **0.3** in principled-v1 | 0.3 | ditto — **72 of 83 operations died here** |
| `invasion_reinforce_rate` | `deeptuning.c:189` → 0.2 | 0.2 | — |

**The pattern:** every threshold on this path is an absolute
probability, and every quantity compared to it is a duel outcome whose
distribution shifts left when attack strengths triple. `survival_target`,
`expansion_war_survival`, `op_take_target` and `op_abort_floor` are four
constants calibrated in a Warriors-to-Knights world, gating a
Gunpowder-to-Conscription one. That is a single defect with four
instances, and it explains both halves of the picture: deep will not
expand (survival too low to permit it) and will not attack (take odds
too low to commit).

---

## 3. Ranked mechanism diagnosis

**1. Era-miscalibrated probability thresholds gate deep out of offence
(dominant; ~82% of the score gap).** Hypothesis (d), refined. The score
deficit *is* `units_killed/3`, it opens at t60, and the offensive engine
is measurably idle: 83 operations formed in 4800 player-turns, 72
aborted on odds, 18 strikes. `op_take_target = 0.65` / `op_abort_floor =
0.3` are compared against a take-probability computed by the same duel
model whose outputs collapse at tech-50. Confidence: high on the
*attribution* (the score decomposition is arithmetic), medium-high on
the *mechanism* (the op census is unambiguous about paralysis; whether
the odds cluster just under threshold or near zero is exactly what is
not yet instrumented — see counter #3).

**2. The same miscalibration gates deep out of expansion and into
masonry (secondary; explains the observable the earlier finding
chased, worth ~20–40 points).** Hypothesis (c), refined —
`deep_expansion_blocked()` fires in 83–88% of city-turns from t76,
zeroing `founder_want` and `worker_want` together, because
`expansion_war_survival = 0.9` sits far above the 0.21–0.39 median
survival of a tech-50 city. The freed shields go to Riflemen, City Walls
and Coastal Defense (23% of production at t51–75) rather than to
settlers or to attackers. This is the answer to the assigned central
question — *and the answer is that it costs about a sixth of what the
kill deficit costs.*

**3. Civilian over-investment (contributing, ~25% of the army gap).**
105 Engineers v 30, 101 Diplomats v 83; 47% of deep's standing force is
civilian against stock's 31%. Real, quantified, and not large enough to
be the story.

**4. Settler fear-tax (hypothesis (b)) — weakly supported, mostly
refuted as stated.** The birth guard is **off** (`birth_guard_gain =
0.0`). The garrison floor is 1 and actual `ndef` is 0.86–1.84 — deep is
thinly garrisoned, not over-garrisoned. What *is* supported is a small
opening-tempo cost: deep plants its eight free settlers ~5 turns slower
than stock (t5: 52 v 70 cities), consistent with
`settler_fearfulness_mult = 1.5` inflating pf danger costs. Worth ~6
cities. The `deep_settler_overnight_veto` bar (`a²·10 ≥ d·tol`) is
quadratic in enemy attack against a settler's constant D=1, so it is
already near-saturated at tech 0 and cannot be much *more* saturated at
tech 50 — it is a bad bar, but not an era-inverting one.

**5. Wide spacing exhausting sites (hypothesis (a)) — REFUTED.** Both
AIs plant at `citymindist`; deep's mean nearest-neighbour is *smaller*
than stock's; deep's cities are bigger and its total citizens equal.
No further work needed here.

**6. Saturated `menace` — a dead knob, not a cause.**
`deep_garrison_menace()` returns the 1.0 clamp from t26 to game end.
Worth fixing because the tuner cannot see through it, but it saturates
identically on `contested`, the venue deep wins.

### What the evidence cannot say

- **The magnitude is not significant.** Three mirrored seeds:
  **+132, −206, −277** (mean −117/seed, SD 219, SEM 126, t = −0.93).
  One of three mirrored seeds is a deep *win*. The military-terms
  component is tighter (−5, −167, −156; mean −109, SEM 52) which is
  itself evidence that the decomposition finds the real signal — but
  **no claim about the size of the tech-50 gap is supported at n = 3.**
  The mechanism measurements (29k city-turns, 11.5k survival reads, 893
  op events) are the solid part; the headline number is not.
- **Techlevel is confounded** with startunits, dispersion, endturn and
  (vs lategame-80) player count and map size. The t80 crossover against
  lategame-80's t80 endturn makes *horizon* a live alternative
  explanation that no run yet separates.
- **No mid-game saves exist** (only T0001 and T0201-final survive in
  each dir), so per-turn city geography could not be reconstructed —
  the spacing measurement is end-state only, and end-state includes
  conquest churn.
- **No stock-side counterfactual.** `ai/baseline` emits none of these
  logs, so "how often would stock's gate have fired" is inferred, not
  measured. Counter #1 below fixes this without touching `ai/baseline`.

---

## 4. Instrumentation plan for the ERA epic

All counters are `log_base(deep_metric_log_level(), "deep-<tag> turn=…")`
lines in deep's existing metric style — the same mechanism as
`deep-def` / `deep-bid` / `deep-surv`, quieted by `FREECIV_DEEP_QUIET`
(`deeptuning.c:534`). **No behaviour change: every value logged is
already computed, and nothing is recomputed in a way that could perturb
the decision it watches** (the rule `deep_bid_log` states at
`deepdefence.c:6339`). Volume estimates are per game.

### Top three

**#1 `deep-expgate` — `ai/deep/deepdefence.c` :: `deep_expansion_blocked()` (line 3905), emit just before each `return`.**

```
deep-expgate turn=%d player=%s city=%d footing=%d wave=%d surv=%.3f
             thresh=%.2f blocked=%d stock_blocked=%d
             founder=%d worker=%d ndef=%d size=%d
```
Discriminates: (a) the true firing rate of deep's gate against the
counterfactual `stock_blocked` (= `dai_on_war_footing()` alone, already
computed on line 3914/3916) — this is the **only** way to learn whether
deep's replacement gate is more or less permissive than the one it
replaced, without running or touching `ai/baseline`; (b) the
distribution of `surv` against `thresh`, which says whether 0.9 is
merely mis-set (survivals cluster in 0.5–0.9) or structurally
unreachable (cluster below 0.2); (c) the `founder`/`worker` want the
gate destroys, so the opportunity cost is a number. ~5.5k lines/game.

**#2 `deep-opodds` — `ai/deep/deepops.c`, in the target-scoring loop that precedes the existing `form` line, and at the `abort-odds` site.**

```
deep-opodds turn=%d player=%s target=%d verdict=%s odds=%.3f
            take_target=%.2f abort_floor=%.2f required=%d pool=%d
            def_n=%d def_best=%d walls=%d coastal_def=%d dist=%d
```
with `verdict ∈ {refused, formed, aborted, struck}`. Today only
*formations* (83) and *aborts* (72) are visible; every target
**considered and refused** is invisible, so we cannot distinguish
"no target was ever near the bar" from "targets sat at 0.60 against a
0.65 bar". That distinction decides whether the ERA fix is a threshold
re-derivation or a force-composition change, and it is the highest-value
unknown in this report. Also records what the defence actually was, so
the Riflemen+Walls+Coastal-Defense hypothesis is testable. Estimated a
few thousand lines/game; if that is too much, log a per-player-turn
histogram of refused odds instead of one line per candidate.

**#3 `deep-wave` — `ai/deep/deepcombat.c` :: `deep_city_survival_core()` (line 312), emit after the wave loop closes (~line 395).**

```
deep-wave turn=%d player=%s city=%d n_att=%d n_def=%d capped=%d
          by_land=%d by_sea=%d by_air=%d by_transport=%d
          max_att=%d max_dist=%d max_turns=%d
          def_best=%d surv=%.3f
```
Discriminates *why* survival is low: few strong local attackers (fix the
threshold) versus a reach radius that admits distant fleets (fix
`survival_horizon` or bound the `UMT_SEA && coastal` clause at
`deepcombat.c:379`). This report's spot-check says wave sizes are small
and the reach clause is **not** the problem — but that rests on the
aggregate `wave=` field alone, with no composition breakdown and no
distance, and the sea clause genuinely has no distance bound beyond
`horizon × move_rate`, which exceeds the map diameter for anything
moving ≥6. Worth one counter to close. Also reports `capped=1` when the
wave hit `DEEP_WAVE_MAX`, which today is silent. ~2k lines/game.

### Supporting three

**#4 `deep-shieldmix` — `ai/deep/deepdefence.c` :: `deep_city_choose_build()` (from line 6426), one line per player-turn.** Accumulate
`pcity->surplus[O_SHIELD]` by the class of the chosen build:
`def_bldg / def_unit / att_unit / naval / civilian / settler / infra /
wonder`. The `prod=` share in §1.E is city-turns, a proxy; this makes
the Coastal-Defense sink a shield number and gives the ERA epic a
before/after metric that is not the score. ~200 lines/game.

**#5 `deep-menace` — `ai/deep/deepdefence.c` :: `deep_garrison_menace()` (line 1242), one line per player-turn.** Log the **unclamped** ratio
alongside the clamped return, plus `theirs`, `ours`, and the count of
players contributing. Proves the knob is dead on 8p venues and sizes by
how much; a knob with no gradient must not be left in the tuner's search
space pretending to have one. ~200 lines/game.

**#6 `deep-civforce` — `ai/deep/deepdefence.c` :: extend `deep_log_defence()` (line 128) with a per-player summary line.** Standing counts by
role: `settlers / workers / engineers / diplomats / ferries / military`,
plus shields tied up in each. Turns the end-state 47%-civilian
observation into a trajectory and shows when the Engineer and Diplomat
stacks accumulate. ~200 lines/game.

### What to run once instrumented

Not part of this phase, but the counters are chosen to answer it: the
decisive experiment is **`contested` extended to endturn 200 at
techlevel 0** against **`tech50war-200`**, mirrored, ≥10 seeds each.
That separates *era* from *horizon* — the two explanations this report
cannot distinguish — and n=10 mirrored is the minimum that makes any
magnitude claim about either.

---

## 5. Reproduction

Analysis scripts are throwaway and were not committed; the inputs are:

- saves: `benchmark/results/tuning/calib2-tech50war-mixed/evaluate/cfg0/mixed_{db,bd}/seed-{1,2,3}/saves/freeciv-T0201-Y01500-final.sav.zst` (`zstdcat`, text; city table key `c={…}` carries `x`,`y` as **native** coords, `size`, `turn_founded`, `original`, `deep.founder_want`; `[scoreN]` sections carry the score breakdown)
- scorelogs: `…/seed-N/scorelog.txt`, SCORELOG2, tags per `server/report.c:194` (0 pop, 3 cities, 4 techs, 5 munits, **6 settlers**, 25 score, 26 built, 27 killed, 28 lost, 29 culture)
- deep's metric stream: `…/seed-N/game.log`, tags `deep-def`, `deep-bid`, `deep-surv`, `deep-op`
- seat map: `mixed_db` = deep at players {0,2,4,6}; `mixed_bd` = deep at {1,3,5,7} (confirmed against `commands.serv` and each save's `ai_type=`)
- distances computed with freeciv's own `real_map_distance` semantics for `topology=ISO|HEX`, `wrap=WRAPX|WRAPY`, 32×32 (`common/map.c:622`, `common/map.h:170`)
