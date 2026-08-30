# The operation exchange rate: what the launch gate promised and what it bought

*2026-08-29. Log census over four existing result corpora. No new
benchmark batches; one six-game emission probe, used for nothing but
proving two new log fields fire.*

S-WEAKPREY closed with a hand-off: deep aims at weaker rivals, forms
+63% operations, and trades roughly two losses per extra kill, so
*"that moves the suspicion downstream of target selection, onto force
sizing and the take-odds model — the wave arrives at a weaker enemy
and still trades badly."*

**The second half of that sentence is wrong, and correcting it is the
finding. The wave does not arrive.**

---

## 0. The one-paragraph verdict

Across 640 assault operations in 88 retained games, deep formed 640,
launched 501, reached the walls **39 times**, and took a city it had
fought for **5 times**. The launch gate's own forecast on the
launches that cleared its bar averages **P(take) ≈ 0.89**; the
realized capture rate of those same launches is **4.4%** — a twentyfold
calibration error. It is not a combat-model error: on the two corpora
whose per-operation ledger survived the log filter, 232 closed
operations booked **354 member deaths and 2 kills at the walls**. The
model prices a battle and the operation buys a journey. Everything
below is the decomposition of that sentence.

---

## 1. Corpora, and what each could answer

| corpus | games | arm | assault ops | what it has |
|---|---|---|---|---|
| `weakprey/ab/off` | 24 | deep, `weak_prey_gate=0` | 126 | opodds + deep-op + kill-dyad |
| `weakprey/ab/on` | 24 | deep, `weak_prey_gate=1` | 206 | same |
| `tune-postfix/s6-nogates-war` | 20 | deep, `defaults.ini` | 166 | **full** log (`deep-employop`, `deep-netval`) |
| `tune-postfix/s5-defaults-war` | 20 | deep, `postfix-start.ini` | 142 | **full** log |
| `tune-postfix/*/baseline` | 20 | `ai/baseline` | — | scorelogs only |

All four are `contested.conf`, ~T150, 8 seats. The weak-prey pair is a
paired A/B on training seeds 1–24; the tune-postfix pair is the
**holdout** report (seeds 101–120) and its baseline counterpart on the
same seeds.

The weak-prey logs were filtered to census lines that do **not**
include `deep-employop`, so realized per-operation kills and losses are
readable only from s5/s6. The funnel, the calibration and the eta
analysis are readable from all four, and they agree — which is the
reason to trust them, since the four span two tunings, two seed sets,
and a gate that is on in one of them.

Census code: `benchmark/opxrate.py` (`funnel`, `score`, `dyad`),
`benchmark/opxrate_employ.sh`, `benchmark/opxrate.sh` (the probe).
Saved output: `benchmark/results/opxrate/`.

---

## 2. The funnel

```
                          formed   march    strike   take   take w/ fight
weakprey off (24 games)     126      97        9       7         4
weakprey on  (24 games)     206     155       18       5         1
s6-nogates   (20 games)     166     135        7       0         0
s5-defaults  (20 games)     142     114        5       1         0
                          ------  ------   ------   -----   ------------
TOTAL                       640     501       39      13         5
                                  78.3%     6.1%    2.0%       0.8%
```

`take` is the operation reaching CONSOLIDATE — the target became ours
while the op was live, whether it was stormed or walked into. Eight of
the thirteen were walk-ins: the city was empty or emptied by somebody
else. **Five operations in 640 took a city by fighting for it.**

Terminal causes, pooled: `abort-odds` 302, `abort-march` 58,
`abort-wait` 37, `abort` 34, `abort-strike` 19, `held` 13,
open-at-game-end 177. The march re-gate is the single largest way an
operation ends, by a factor of five over everything else that is not
the end of the game.

---

## 3. The calibration error

Bucketing every launch by the P(take) the launch gate computed for the
force actually standing on the staging tile:

```
predicted P(take)      n    realized take%   (weakprey off / on)
  [0.90, 1.00]        73        12.9 / 2.4
  [0.70, 0.90)        46         7.7 / 9.1
  [0.30, 0.70)        15         6.7 / 0.0
  [0.00, 0.30)       118         2.2 / 1.4
```

and pooled over all four corpora, restricted to launches that cleared
their own bar (mean predicted odds 0.882–0.925):

```
                            bar-clearing launches   realized takes
weakprey off                        44                 5  (11.4%)
weakprey on                         75                 4  ( 5.3%)
s6-nogates                          62                 0  ( 0.0%)
s5-defaults                         47                 1  ( 2.1%)
                                   ---                ---
TOTAL                              228                10  ( 4.4%)
```

**Predicted 0.89, realized 0.044.** The model is not merely
miscalibrated in slope; it is answering a different question from the
one the decision needs. `deep_take_odds_cost()` computes
P(this stream beats that garrison, arriving together and fresh). The
decision needs P(this operation ends with us holding that city). The
gap between them is the march, and nothing in the launch path prices
the march at all.

---

## 4. Where the wave goes instead

### 4.1 Travel estimate vs reality

`eta` at both the formation and launch seats is raw map distance over
move rate (plus `invasion_embark_turns` overseas). No terrain, no
zones of control, no ferry queueing, no stock leash.

```
                        reached the walls        never arrived
                     eta@launch  actual     eta@launch  turns spent
weakprey off            1.33      1.22         4.14        8.31
weakprey on             1.41      0.65         4.16        7.75
s6-nogates              1.14      0.29         3.42        8.05
s5-defaults             1.20      0.40         3.34        7.10
```

Waves that reach the walls were launched **already at the walls**
(eta ≤ 1.4). Waves launched at eta ≈ 3.5–4.2 spend 7–8 turns marching
and then abort without arriving. The estimate is not merely optimistic;
past eta 2 it is not predictive of anything.

The cut is brutally clean (pooled, counted not averaged):

```
launch eta      launches   strikes          takes
   0-1             30       30 (100.0%)     1 ( 3.3%)
   2              205        6 (  2.9%)     6 ( 2.9%)
   3-4            149        2 (  1.3%)     6 ( 4.0%)
   5+             117        0 (  0.0%)     0 ( 0.0%)
```

**117 operations launched with a five-turn-or-longer march produced
zero contacts and zero cities.** Not "few". Zero.

### 4.2 The re-gate talks the operation out of itself

Of the marches that died before reaching the walls, the census splits
by whether the wave shrank:

```
                       n    present -> end   odds@launch -> @abort   eta@launch -> @abort
weakprey off, intact   39     5.33 -> 5.85      0.397 -> 0.075          4.51 -> 5.03
weakprey on,  intact   70     5.50 -> 6.03      0.309 -> 0.069          4.07 -> 4.47
s6-nogates,   intact   64     5.38 -> 5.75      0.294 -> 0.072          3.12 -> 3.69
s5-defaults,  intact   58     5.53 -> 5.91      0.376 -> 0.102          3.55 -> 4.33
weakprey off, shrank   29     8.55 -> 5.41      0.458 -> 0.116          3.79 -> 4.95
weakprey on,  shrank   28     7.68 -> 4.68      0.738 -> 0.092          4.61 -> 5.00
s6-nogates,   shrank   45     7.27 -> 4.47      0.623 -> 0.100          3.84 -> 4.92
s5-defaults,  shrank   24     6.79 -> 4.96      0.549 -> 0.103          2.83 -> 4.65
```

Restricted to launches that had cleared the bar (predicted odds
0.86–0.95), the same pattern holds and is if anything sharper: on
weakprey-on, 22 intact waves went from 0.864 at launch to 0.130 at the
abort with members rising 5.18 → 5.95.

**In 57–71% of aborted marches the wave had the same or MORE members
than at launch.** The force did not shrink. The priced odds collapsed
by an order of magnitude anyway — and the eta at the abort is *higher*
than at launch. After five to sixteen turns of marching, the wave is
farther from the target, in the model's own currency, than when it set
out.

That is self-defeating in a precise sense. `deep_take_odds_at_arrival()`
charges the defender `eta × deep_reinforce_rate()` virtual copies of
its best buildable defender. A wave that fails to converge inflates its
own eta, which inflates the phantom garrison, which drives the odds
under `op_abort_floor`, which dissolves the wave — and the whole march
already paid for is nowhere in that comparison. The hysteresis the code
documents (`op_abort_floor` below `op_take_target`) is hysteresis in
the odds dimension only; there is none in the value dimension and no
memory of sunk cost.

*(Whether the garrison also grew was not separable from the log: the
re-gate line carried eta and members and nothing about the defender.
Commit `fbdbc10198` adds `def_n`/`def_best`/`marched` to it, which is
what makes the two hypotheses distinguishable in the next corpus.)*

**A third candidate the new fields already hint at.** In the emission
probe, one operation launched at odds 0.456 with five members at eta 2
and re-gated the very next turn at odds 0.01 with the same five members
and the same eta 2, against a garrison of 2 rated 10. Members constant,
eta constant, odds down 45×. The remaining free variable is member
**hit points**: `deep_take_odds_cost()` reads live HP, `dai_gothere()`
spends the wave's moves and sometimes its blood getting there, and
nothing in the model carries a damage-or-healing term across the
march. One instance is a hypothesis, not a finding — it is what the
proposed `deep-march` per-turn line exists to settle.

### 4.3 Half of all launches never satisfied the model

The launch block is

```c
if (go || (!op->muster
           && game.info.turn - op->formed_turn >= tune->op_muster_wait
           && present >= (op->required + 1) / 2)) {
```

The second clause is a head count on a clock. It consults no odds, no
survivor floor and no net value.

```
                    launches   patience releases   mean odds   died pre-walls   take%
weakprey off            97          53 (55%)         0.109        93.3%          3.8
weakprey on            155          80 (52%)         0.078        90.2%          1.2
s6-nogates             135          73 (54%)         0.090        96.9%          0.0
s5-defaults            114          67 (59%)         0.146        96.2%          0.0
                       ---         ---
TOTAL                  501         273 (54.5%)                                   1.1
```

Pooled and counted: **cleared the bar** n=228, 26 strikes (11.4%),
10 takes (4.4%); **patience release** n=273, 12 strikes (4.4%),
3 takes (1.1%).

**More than half of every wave deep has ever launched went out at a
predicted take probability under 0.15, through a gate that never
asked.** `op_muster_wait` is a patience knob being used as a launch
authority.

**And it overrides the net-value refusal, not just the odds bar.** The
net-value test runs inside `if (go && tune->op_net_value)`; when it
refuses it sets `go = FALSE`, and the patience clause is evaluated
*after* that with `||`. The new `eworth=`/`patience=` fields catch it
directly — from the six-game emission probe, two of 27 launches:

```
march ... present=7 required=9 waited=10 eta=2 odds=0.762 surv=4.20 \
      loss=61.93 cost=1052.9 eworth=1042.0 patience=1
march ... present=9 required=8 waited=10 eta=3 odds=0.992 surv=5.72 \
      loss=98.27 cost=1670.7 eworth=905.4 patience=1
```

value = odds × eworth = 794 against cost 1053, and 898 against 1671.
The gate priced both, said no to both, and the clock launched both.
(Six games prove the mechanism exists; they do not measure how often.)

### 4.4 `required` is doing two incompatible jobs

`required` is the smallest prefix of the pool that cleared the
formation bar. It is also the `k` in `deep_op_eta()`'s k-th-smallest
member arrival, and contact fires at `eta <= 1`. So the quorum that
makes an operation *legitimate* is the same number that makes contact
*unreachable*:

```
required     launches   strikes         takes
  1-2            27      4 (14.8%)      2 ( 7.4%)
  3-4           124     20 (16.1%)      4 ( 3.2%)
  5-6           133      9 ( 6.8%)      6 ( 4.5%)
  7-8            84      3 ( 3.6%)      1 ( 1.2%)
  9+            133      2 ( 1.5%)      0 ( 0.0%)
```

**133 launches with a nine-or-more quorum, two contacts, zero cities.**
A wave with three blades at the walls and five stragglers six tiles
back is priced as if it were still six tiles back, so it stands there,
re-gates, and aborts.

### 4.5 The garrison draft fills the quorum with the wrong blades

The draft borrows one blade per city, chosen as the **weakest defender
in that city** (`adv_unit_def_rating_basic` minimum). It is how
`required` gets met when the free pool is short — and it fires on most
formations:

```
                    formations   strikes        takes
drafted > 0            406       23 (5.7%)     3 (0.7%)
drafted = 0            234       16 (6.8%)    10 (4.3%)
```

A wave that needed the draft to reach its number takes a city at
**one-sixth** the rate of one that did not — and drafted formations are
63% of everything deep forms. The draft adds head count to
a model whose odds walk is driven by attack rating, so the marginal
release test (`with - held > cost`) is buying almost nothing per blade
while paying real defence for it.

### 4.6 Overseas

```
              formations   strikes         takes
land             333       25 (7.5%)      9 (2.7%)
overseas         307       14 (4.6%)      4 (1.3%)
```

Overseas is 48% of every operation deep forms and it converts at half
the rate of a land assault. The crossing is priced with the *land*
model — raw distance over the best ferry's move rate, plus a flat
`invasion_embark_turns` — and the ferry system that has to execute it
is the one whose own docs call it *"statistical mechanics rather than
logic"* (RC2). This is a real failure mode but it is second-order: the
land number is 2.7%.

### 4.7 What is NOT wrong: walls, terrain, veterancy

Worth stating because it was on the suspect list.

```
best defender rating at the chosen target, pooled over 640 formations
   0      1-5     6-10    11-20    21+
  74      166      391       9       0
```

Nine of 640 targets had a defender rated above 10. Mean garrison at the
chosen target is **1.2–1.6 defenders** with a best rating of 5.4–6.3 —
Phalanx-and-Warriors country. Deep is not being stopped by Musketeers
behind city walls. It is being stopped by distance and by its own
gates. Any epic that starts by improving the duel model is fixing a
part that is not broken.

---

## 5. What it costs, per operation

From the two corpora whose `deep-employop` ledger survived:

```
                              s6-nogates   s5-defaults
closed operations                 128          104
members at formation             7.09         7.01
members at close                 4.98         5.58
ordered attacks: kills          0.016        0.000
ordered attacks: losses         0.000        0.000
departures: dead in transit      1.78         1.21
departures: poached              0.28         0.16
departures: recalled to defence  0.05         0.06
```

```
TOTAL over 232 closed operations:
  member deaths            354
  kills at the walls         2
  cities taken               1
  exchange              0.006 kills per member death
```

**Caveat, and it matters.** `kills` is booked only at
`deep_op_drive_strike()`'s ordered-attack seat. A member that kills
something while walking under stock's leash during MARCH is not
credited to the operation. So 2 is the count of kills the operations
layer *decided*; the true number bought by those units is higher and
unknown. That is exactly the point: **354 units died inside operations
that made two decisions between them.** Everything else those blades
did, they did as stock units — which is the RC2 state the operations
layer exists to replace.

`losses = 0.000` is consistent rather than contradictory: an attacker
can only be booked lost at the ordered-attack seat, and there were
almost no ordered attacks.

### The refusals price it honestly

The one place the cost side *is* logged is the net-value refusal:

```
              refusals   E[loss] shields   value (odds x eworth)   cost
s6-nogates       85           94.6                 865            1608
s5-defaults     110           97.9                 882            1664
```

The gate refuses when cost ≈ 1.9× value, which is the arithmetic
working. Note the magnitudes: a refused assault expects to destroy
~95 shields of its own force. That is a real quantity and until
commit `7e2fc949ca` it was written down **only when the answer was no**.

---

## 6. Deep against baseline, same seeds

The whole operations apparatus buys no exchange-rate advantage over
stock's per-unit `find_something_to_kill()`.

```
holdout seeds 101-120, contested, 158 player-games per arm

                       built   killed    lost    k/l   munits(T150)  score
deep  (s6-nogates)     50.30    9.91    14.72   0.673     15.47       85.1
deep  (s5-defaults)    50.00    8.96    13.79   0.649     15.60       88.1
baseline               49.90    9.31    14.04   0.663     12.70       92.4
```

Same units built. Same kill/loss ratio, to three decimals of nothing.
**+22% standing military** and a lower score.

The turn slices locate when the advantage evaporates:

```
            T50 k/l    T100 k/l   T150 k/l
deep s6      0.786      0.804      0.676
deep s5      0.737      0.729      0.650
baseline     0.708      0.610      0.662
```

Deep is comfortably ahead on exchange at T100 (0.80 vs 0.61) and back
at parity by T150. Operations run T75–T150 (formations by 25-turn band
in s6: 1, 12, 44, 55, 54, 0). This is correlation, not attribution —
the late game is also when everyone's armies get big — but the window
matches, and no other deep subsystem starts in it.

Where the army actually is, averaged over 24 000 player-turns of s6:

```
military units per player-turn          7.094
  in an assault operation               0.567   ( 8.0%)
    of which at the walls (STRIKE)      0.009   ( 0.1%)
  garrison (DEFEND_HOME)                3.270   (46.1%)
  attack-capable and idle               1.121   (15.8%)
```

Eight percent of the army is in the operations layer and one part in a
thousand is fighting for a city.

---

## 7. What the weak-prey gate actually did, re-read

S-WEAKPREY's own numbers, now with the mechanism attached. The gate
raises conquest *worth*; it does not touch the odds bar. So it opens
cheaper targets, the odds prefix terminates earlier, and the wave is
**smaller**:

```
                                    OFF      ON     delta
required force at formation        7.65     6.66    -13%
members present at launch          7.2      6.3     -12%
E[survivors] at launch             3.74     3.01    -20%
defenders at the chosen target     1.59     1.42
best defender rating               6.29     6.24
------------------------------------------------------
operations formed                   126      206    +63%
marched                              97      155    +60%
reached the walls                     9       18   +100%
took a city                           7        5    -29%
conversion AT THE WALLS           44.4%     5.6%
```

The directive was answered — the standing army stops idling, deep
strikes twice as often. But +63% of a pipeline that leaks 94% is +63%
of the leak, and the extra `unitslost` is the leak widening. The
kill-dyad ledger says the same thing by unit type:

```
unit deaths per game        OFF     ON
  all types                73.8   86.4   +17%
  Chariot (deep's blade)   15.2   20.3   +34%
  Phalanx (deep's wall)    21.7   25.0   +15%
  Archers                   2.0    3.0   +47%
```

The offensive arm dies a third more often. The defensive arm dies
15% more often too, which is the garrison draft's bill arriving.

**The correction to S-WEAKPREY §5 is one clause:** the suspicion does
belong downstream of targeting, and it belongs on force sizing — but
"the wave arrives at a weaker enemy and still trades badly" describes
5 events out of 640. The wave does not arrive.

---

## 8. Ranked verdict: where the launch model is wrong

1. **It prices a battle and buys a journey.** The gate computes
   P(win | arrived together and fresh) and uses it as P(take). The
   missing factor is P(arrive), whose measured proxy — contacts over
   launches — is **7.8%** and falls to 0 past eta 4. This is the whole
   twentyfold calibration error and every other item is a special case
   of it.

2. **The half-quorum patience release bypasses the model entirely.**
   54.5% of launches; mean predicted odds 0.08–0.15; 1.1% take rate
   against the bar-clearing population's 4.4%. It overrides the odds
   bar, the survivor floor *and* an explicit net-value refusal, because
   it is the right-hand side of an `||` evaluated after `go` has been
   set false. A patience knob is acting as a launch authority.

3. **`required` gates contact as well as legitimacy.** The k-th
   smallest arrival with k = required means a big wave can never reach
   `eta <= 1`. 133 launches at required ≥ 9 produced two contacts and
   no cities.

4. **The march re-gate has no memory and an inflating denominator.**
   The abort is 60% of all terminal causes; in 57–71% of them the wave
   was intact and the eta had grown. Reinforcement is charged against
   an eta that rises precisely because the operation is failing, and
   the sunk march is not in the comparison.

5. **The garrison draft meets the quorum with the weakest blade in
   each city.** Drafted waves take cities at 0.7% against 4.3%, and
   they are 63% of everything deep forms.

6. **Overseas is priced with the land model plus a constant.** 48% of
   operations, 1.3% take rate against land's 2.7%. Real, and
   second-order behind (1)–(4).

7. **NOT the defender's terrain, walls or veterancy.** Ruled out:
   9 of 640 targets held anything rated above 10.

---

## 9. Proposed epic: `op-arrival` — instrument, then price the journey

Shape follows the project's own rule (`findings` first, then a gate
that defaults to identity, then an A/B on training seeds). Two of the
instrument commits are already in this branch.

**Instrument (2 landed, 1 proposed)**

- ✅ `ai/deep: log the launch gate's loss forecast on the go path`
  (`7e2fc949ca`) — `loss=`, `cost=`, `eworth=`, `patience=` on the
  march line. Without `patience=` the two launch populations cannot be
  separated at all in a future corpus.
- ✅ `ai/deep: log the garrison the march re-gate re-priced against`
  (`fbdbc10198`) — `def_n=`, `def_best=`, `marched=` on both re-gate
  lines. Separates "the garrison grew" from "the eta inflated".
- ⬜ `ai/deep: log each marching turn's convergence` — one
  `deep-march` line per marching operation per turn carrying eta,
  members, **summed member HP** and distance-closed. The arrival curve
  P(arrive | eta) has to be **fitted**, not assumed, and today the log
  has only its endpoints; the HP column is what settles §4.2's third
  candidate.

**Then price it.** Every knob below defaults to the identity value, so
the off arm is byte-identical and the A/B is a clean paired
comparison. **No values are proposed here** (rule 7); each ships as a
PLACEHOLDER with a range in `tuning-params.ini` and is registered in
the MIL and ALL blocks for the sweep.

| tunable | what it does | identity default |
|---|---|---|
| `op_arrival_decay` | per-turn-of-eta multiplier folded into the launch product, making the gate compare `P(arrive) × P(win) × worth` against cost instead of `P(win) × worth` | 1.0 |
| `op_launch_eta_cap` | refuse to launch when the quorum eta exceeds it | 0 = off |
| `op_march_loss_rate` | expected members lost per turn of march, charged into the net-value cost so the cost side stops being the duel only | 0.0 |
| `op_regate_sunk_gain` | fraction of the march already spent credited against the abort test — hysteresis in the value dimension, which today has none | 0.0 |
| `op_regate_eta_freeze` | re-price reinforcement at the LAUNCH eta rather than the current one, so a stalled wave stops inflating its own phantom garrison | 0 = off |
| `op_patience_odds_floor` | the odds the half-quorum wait-cap release must still clear | 0.0 = off (today's behaviour) |
| `op_contact_quorum` | the k in `deep_op_eta()`'s k-th-smallest arrival, decoupled from `required` | 0 = use `required` |
| `op_draft_attack_floor` | minimum attack rating for a drafted blade, so the quorum stops being met with the weakest defender in each city | 0.0 = off |

One item on the list is deliberately **not** a constant. If §4.2's
third candidate survives the `deep-march` instrument — that the wave
arrives wounded and the launch gate priced it healthy — the fix is to
price the stream at its *expected arrival* HP rather than its current
HP, which is a change to `deep_take_odds_cost()`'s inputs and not a
knob. It is listed here so the epic does not quietly turn it into one
(rule 7 cuts both ways: a modelling error must not be papered over with
a tuned fudge factor).

**Sequencing, and it is not negotiable in one direction.**
`op_patience_odds_floor` and `op_contact_quorum` are on the same causal
chain as `op_arrival_decay` — all three move which waves fight — so
they want **one factorial design**, not three concurrent arms
(CLAUDE.md, "Parallelise scheduling, not attribution").
`op_draft_attack_floor` and `op_launch_eta_cap` are separable and can
run beside it.

**Falsifiable prediction to pre-register.** If (1) is the right
diagnosis, then a cell with `op_launch_eta_cap` set to refuse eta ≥ 5
loses **nothing**: 117 such launches in this corpus produced zero
contacts and zero cities, so the arm should show strictly fewer
operations formed, strictly fewer `unitslost`, and unchanged
`unitskilled`. If `unitskilled` falls, the diagnosis is wrong and this
document should be re-read rather than the tuning re-swept.

**What this epic must not do:** improve the duel model, add
terrain/wall/veterancy terms, or touch target selection. §4.7 and
S-WEAKPREY respectively measured those and found them innocent.

---

## 10. Reproducing

```sh
python3 benchmark/opxrate.py funnel \
    <weakprey>/ab/off <weakprey>/ab/on \
    benchmark/results/tune-postfix/s6-nogates-war/holdout-report \
    benchmark/results/tune-postfix/s5-defaults-war/holdout-report
python3 benchmark/opxrate.py score  <deep arms> <baseline arms>
python3 benchmark/opxrate.py dyad   <weakprey>/ab/off <weakprey>/ab/on
bash     benchmark/opxrate_employ.sh <arm dir>
```

Saved output for the tables above:
`benchmark/results/opxrate/{funnel,score,dyad,employ-s6}.txt`.

`benchmark/opxrate.sh probe` runs the six-game emission check for the
two new log fields on training seeds 1–6. Its output is used for
nothing in this document beyond proving the fields fire.
