# op-arrival: the census replicates, the gates do not pay

*2026-08-30. One 2x2x2 factorial plus a pre-registered falsification
cell, 144 games, TRAINING seeds 1-16, contested.conf, arm deep, paired
against a control that is the shipped binary at its compiled defaults.
Adjudicates the epic proposed in
`findings/op-exchange-rate-2026-08-29.md` section 9.*

---

## 0. The one-paragraph verdict

**Every diagnosis in the exchange-rate census replicates on an
independent seed set. None of the three gates built to act on them
moves the score.** In the control cell, 21 launches at eta >= 5
produced zero contacts and zero cities — exactly the pre-registration's
claim, on seeds the census never saw. Waves with a quorum of nine or
more struck 6.2% of the time and took nothing. Drafted formations took
cities at 3.8% against undrafted 15.2%. And then: the patience floor
cuts launches 38% with takes unchanged, the arrival pricing cuts them
73% and costs a third of all captures, the re-gate's memory does
nothing measurable, and **the largest score effect in the eight-cell
factorial is +0.34 +- 0.53** — under one standard error. All three
gates ship at 0, and the reason is not that they failed to fire.

---

## 1. What was built

Seven commits. Every knob is a PLACEHOLDER at its identity value, so
the shipped binary is byte-for-byte the one the census measured:
contested holdout seeds 101-103 reproduce the archived
`tune-postfix/s6-nogates-war` scorelogs exactly (the `id` line, a
per-run random token, excluded), both before and after the whole
epic.

| commit | what |
|---|---|
| `ai/deep: log each marching turn's convergence` | `deep-march`, one line per marching op per turn: eta, members, summed HP, quorum distance |
| `ai/deep: give the half-quorum patience release an odds floor` | `op_patience_odds_floor` |
| `ai/deep: price the march in the assault launch gate` | `op_arrival_gate` over `op_arrival_decay`, `op_launch_eta_cap`, `op_march_loss_rate` |
| `ai/deep: give the march re-gate a memory of the march` | `op_regate_gate` over `op_regate_sunk_gain`, `op_regate_eta_freeze` |
| `ai/deep: separate the contact quorum from the required force` | `op_contact_quorum` |
| `ai/deep: put an attack-rating floor under the garrison draft` | `op_draft_attack_floor` |
| `benchmark: add the op-arrival factorial and its funnel census` | `oparrival.sh`, `oparrival.py`, nine cell tunings |

---

## 2. The design, and why it is one design

`op_arrival_decay`, `op_patience_odds_floor` and the re-gate terms all
move **which waves fight**. Two effects on one causal chain cannot be
read from two separate A/Bs (CLAUDE.md, *"Parallelise scheduling, not
attribution"*), so they are one 2x2x2 factorial over cells named APR:

- **A** = `op_arrival_gate` — decay 0.447, eta cap 4, march loss 0.56
- **P** = `op_patience_odds_floor` = 0.35
- **R** = `op_regate_gate` — sunk gain 1.0, eta freeze 1

`000` is the control: compiled defaults, which is the tuning
`s6-nogates-war` ran, so the control arm **is** the corpus the census
was taken on. Every cell loads a tuning file, so "a file was loaded" is
never a second difference between arms.

**No value was chosen** (rule 7). Each is derived from the census and
the derivation is written at the top of every cell ini:

- `op_launch_eta_cap = 4` — refuse eta >= 5, which is the
  pre-registration's own threshold.
- `op_arrival_decay = 0.447` — the decay reproducing the pooled
  arrival rate 39/501 = 0.078 at the pooled mean launch eta 3.17
  (`0.078 = d^3.17`).
- `op_march_loss_rate = 0.56` — the `deep-employop` ledger's 1.78
  members per closed operation dead in transit, over that same 3.17.
- `op_patience_odds_floor = 0.35` — **structural, not fitted**: it is
  `op_abort_floor`. A wave launched below the floor its own first march
  turn will re-gate it against has made launch and abort the same
  decision one turn apart.
- `op_regate_sunk_gain = 1.0` — the unit case, credit the whole
  fraction of the journey covered. `op_regate_eta_freeze = 1` is a
  switch.

The **ETA** cell runs beside the factorial rather than inside it,
because the pre-registered falsification has to be readable alone:
`op_launch_eta_cap` and nothing else.

---

## 3. The census replicates

Everything below is the control cell — 16 training seeds, 85 assault
operations — against the census's four-corpus, 88-game, 640-operation
pooled figures. These seeds were not in any of them.

```
                                     census (640 ops)   control (85 ops)
funnel: formed -> march                    78.3%              84.7%
        -> strike                           6.1%              10.6%
        -> take                             2.0%               8.2%
```

The control converts better than the pooled census — training seeds,
and 85 operations is a small sample — but every *shape* the census
named is present, and several are sharper:

**Take rate by launch eta.** The whole diagnosis, and it replicates
with the cut in exactly the same place:

```
   launch eta     n    strike%   take%
     0-1           6    100.0     16.7
     2            25     12.0     16.0
     3-4          20      0.0     10.0
     5+           21      0.0      0.0
```

**21 launches at eta >= 5, zero contacts, zero cities.** The census
found 117 of them across four corpora with the same two zeros. The
pre-registered claim is confirmed on data it was not fitted to.

*(Two denominators appear in this document and they are both right.
`opxrate.py` reconstructs one episode per operation and sees 72
marches; the raw `deep-op ... march` line count is 78, because a
retargeted operation launches again under the same slot. The eta
histogram in §5 counts lines, the table above counts episodes.)*

**`required` gates contact.** Census: required >= 9 gave 2 contacts and
0 cities over 133 launches. Control:

```
   required     n     strike%   take%   present   eta
     1-2         3      66.7     33.3      2.33   1.33
     3-4        15      20.0     20.0      3.87   2.67
     5-6        26      11.5      7.7      5.69   4.15
     7-8        12       0.0      8.3      6.92   4.67
     9+         16       6.2      0.0     11.94   5.19
```

**The garrison draft.** Census: drafted 0.7% take against undrafted
4.3%. Control: **drafted 3.8% (n=52) against undrafted 15.2% (n=33)**
— the same four-fold gap, and drafted formations are 61% of everything
formed (census: 63%).

**The patience release.** Census: 54.5% of launches, take 1.1% against
the bar-clearing 4.4%. Control: 36 of 72 marches (50%), mean predicted
odds **0.109** against the bar-clearing population's **0.923**, take
5.6% against 13.9%.

**The re-gate's inflating eta.** Census: in 57-71% of aborted marches
the wave was intact or larger and the eta had risen. Control: 31 of 48
(65%) intact, present 5.32 -> 5.87, odds 0.428 -> 0.063, **eta
4.71 -> 5.12**.

**Eta honesty.** Census: reached the walls at eta_at_launch 1.14-1.41;
never arrived at 3.34-4.16. Control: **1.33 vs 4.39**, with 7.31 turns
spent marching before giving up.

Six independent claims, six replications. The census is sound.

---

## 4. The factorial

Paired per seed, 16 training seeds, arm deep, control mean 83.90.

```
cell   gates          score    delta      se      t   wins
000    control        83.90        —       —      —      —
100    A              83.81    -0.09    0.56  -0.15   5/16
010    P              84.04    +0.14    0.65  +0.22   7/16
001    R              84.23    +0.33    0.29  +1.14   7/16
110    A+P            84.16    +0.27    0.56  +0.48   6/16
101    A+R            83.98    +0.09    0.64  +0.13   6/16
011    P+R            83.92    +0.02    0.65  +0.04   6/16
111    A+P+R          84.24    +0.34    0.53  +0.65   6/16
eta    eta cap only   83.76    -0.14    0.53  -0.27   4/16
```

Main effects, averaging the four cells that carry each gate against the
four that do not: **A +0.03, P +0.11, R +0.12**. The largest single
cell effect is +0.34 against its own 0.53. **Nothing in this table
clears one standard error.**

The gates did fire, hard:

```
per-seed counters, paired against 000       launched          takes
000  control                                 4.88               0.44
100  A            -0.81 +- 1.26 (t -0.64)   4.06     -0.31 +- 0.12 (t -2.61)
010  P            -1.88 +- 0.55 (t -3.38)   3.00     +0.06 +- 0.11 (t +0.56)
001  R            -0.12 +- 0.39 (t -0.32)   4.75     -0.12 +- 0.09 (t -1.46)
110  A+P          -3.56 +- 1.01 (t -3.52)   1.31     -0.31 +- 0.12 (t -2.61)
011  P+R          -2.19 +- 0.71 (t -3.09)   2.69     +0.00 +- 0.13 (t  0.00)
111  A+P+R        -3.62 +- 0.90 (t -4.03)   1.25     -0.31 +- 0.12 (t -2.61)
eta  cap only     -1.69 +- 0.78 (t -2.17)   3.19     -0.12 +- 0.09 (t -1.46)
```

Three readings, and they are the finding:

**(a) The arrival gate costs cities.** Every cell carrying A lands at
0.12 takes per game against the control's 0.44 — the same number in all
four, at t = -2.61. At these values the decay crushes the net-value
product so completely that `go` is *never* true: in cells 100, 110, 101
and 111 the patience release is **100% of all launches**. The gate meant
to price the journey ended up handing the launch decision entirely to
the clock it was supposed to help discipline. That is an interaction no
separate A/B could have seen, and it is exactly what the factorial
exists for.

**(b) The patience floor is free.** Cell 010 cuts launches 38%
(4.88 -> 3.00, t = -3.38), cuts patience releases 66% (2.94 -> 1.00,
t = -4.48), cuts `abort-odds` 32% (t = -2.78) — and takes go *up*
(+0.06 +- 0.11). Cell 011 cuts launches 45% with takes exactly
unchanged. Thirty fewer wasted marches per sixteen games, and the same
cities. It buys no score, but it costs none either, and it is the only
gate here whose funnel improves without a bill.

**(c) The re-gate's memory does nothing.** Cell 001 moves launches by
-0.12 +- 0.39. `abort-odds` falls 40 -> 36 and `abort-march` rises
10 -> 14: the waves the sunk-cost credit rescues from the odds test die
one gate later, of the wander clock, having marched longer. Score
+0.33 +- 0.29 is the factorial's best t (1.14) and is still noise.

---

## 5. The pre-registered falsification

The census wrote it down in advance:

> If (1) is the right diagnosis, then a cell with `op_launch_eta_cap`
> set to refuse eta >= 5 loses **nothing**: 117 such launches in this
> corpus produced zero contacts and zero cities, so the arm should show
> strictly fewer operations formed, strictly fewer `unitslost`, and
> unchanged `unitskilled`. If `unitskilled` falls, the diagnosis is
> wrong and this document should be re-read rather than the tuning
> re-swept.

The ETA cell, paired, 16 seeds:

```
                     OFF       ON     delta      se       t
launches            4.88     3.19     -1.69    0.78   -2.17
unitslost          12.79    12.32     -0.47    0.37   -1.25
unitskilled         9.00     8.70     -0.30    0.38   -0.78
takes               0.44     0.31     -0.12    0.09   -1.46
score              83.90    83.76     -0.14    0.53   -0.27
launches at eta 5+    26        0
```

**Fewer operations: yes** (t = -2.17), and the population it was aimed
at is gone — the eta >= 5 bucket goes 26 -> 0, against a total launch
drop of 27. The other buckets move too (eta 2 goes 25 -> 31, eta 3-4
goes 21 -> 15), which is not the cap reaching them but the games
diverging after the first refusal; only the 5+ bucket is emptied by
construction. **Fewer `unitslost`: directionally yes**, -0.47 at
1.25 SE.
**Unchanged `unitskilled`: it fell by 0.30, which is 0.78 SE.**

By the letter of the rule, `unitskilled` fell. By its arithmetic, it is
indistinguishable from unchanged, and so is everything else in the
table. The honest verdict is neither confirmation nor falsification:

**The capped launches really do take no cities — that is confirmed
directly, 21 of them in the control with zero strikes and zero takes —
and refusing them buys nothing measurable.** The blades freed do not
convert anywhere else. The diagnosis of *where the operations layer
leaks* survives intact; what does not survive is the assumption that
plugging the leak is worth score.

---

## 6. What this actually says, and it is bigger than the gates

The census closed with 8.0% of deep's army inside an operation and
0.1% of it at the walls. This factorial is the causal version of that
sentence:

**Cutting deep's assault launches by 75% moves its score by less than
one standard error, in either direction.**

Cell 111 launches 1.25 waves per game against the control's 4.88 and
scores +0.34 +- 0.53. Cell 010 launches 3.00 and scores +0.14 +- 0.65.
Cell 100 launches 4.06 and scores -0.09 +- 0.56. Across 144 games the
operations layer's launch volume is very nearly a free variable.

That is not an argument that the operations layer is worthless — a
capture is worth a great deal when it lands, and the control does take
7 cities in 16 games. It is an argument that **the launch gate is not
where the marginal score is**, and that further epics tightening it
are optimising a term whose coefficient this experiment cannot
distinguish from zero. The 354-deaths-for-2-kills exchange the census
found is real; what these 144 games add is that fixing it does not pay,
because the deaths were not buying anything the score noticed either.

Where the next look should go, on this evidence:

1. **The garrison draft is the one funnel term with a large, replicated
   effect that was NOT tested here.** Drafted formations take cities at
   a quarter the undrafted rate and are 61% of everything formed.
   `op_draft_attack_floor` shipped gated at 0 with no value, because
   the census contains no number from which one can be derived (§8).
   That is the cheapest remaining question in this area.
2. **The operations layer's *size*, not its gates.** If launch volume
   is free at the margin, the question is not which waves launch but
   whether the 46% of the army sitting in `DEFEND_HOME` should be
   there at all — which is a defence-side question, not this one.
3. **Not the duel model, not target selection.** §4.7 of the census and
   S-WEAKPREY measured those and found them innocent, and nothing here
   changes that.

---

## 7. Threats to this reading

Stated because they are real.

**Sixteen seeds.** The score SEs are 0.3-0.65 on a mean of 84, which is
tight pairing, but a true effect of +0.5 would be missed about half the
time. What the experiment establishes firmly is the *absence of a large*
effect, not the absence of any effect.

**One venue, one horizon.** contested.conf, 8 players, T150. Operations
run T75-150, so a longer game gives them more room; `tech50war-200`
concludes wars and is the natural second venue. The census's four
corpora were all this venue too, so the replication in §3 is a
replication across seeds and tunings, not across venues.

**Overdispersion.** Operation counts per seed run 0-38 in the archive
and have sd ~4.3 on a mean of 5.3 here. Every per-seed counter in §4 is
reported with its own paired SE for that reason; the pooled funnel
totals in §3 are not, and should not be read as if they were.

**The values, not the mechanisms, were on trial.** A found no score at
decay 0.447 — a decay derived honestly from the pooled arrival rate,
but one that turned out to refuse so much that the patience clause
became the sole launch path. A gentler decay is a different experiment
and is not evidence for or against what ran here. This is why all four
A-cells agreeing at 0.12 takes matters more than any one of them: the
*failure mode* is a property of the parameterisation, and it is a
documented one.

**Butterfly.** Freeciv is chaotic across a gate flip; the eta cell's
-2 takes over 16 games cannot be attributed to the capped launches
themselves, since those launches took zero cities by direct count.
Second-order effects are why per-seed pairing is the design and why
nothing here is claimed at under 2 SE.

---

## 8. Constants owed to a tuning pass

Every one is a PLACEHOLDER, none has been tuned, and all are registered
in `benchmark/tuning-params.ini` and in the M, MIL and ALL blocks:

| tunable | identity | status |
|---|---|---|
| `op_patience_odds_floor` | 0.0 | tested at 0.35 (= `op_abort_floor`); free but not paying |
| `op_arrival_gate` | 0 | tested; harmful at the derived values |
| `op_arrival_decay` | 1.0 | tested at 0.447; too aggressive — see §7 |
| `op_launch_eta_cap` | 0 | tested at 4; removes the right population, buys nothing |
| `op_march_loss_rate` | 0.0 | tested at 0.56, confounded with the decay |
| `op_regate_gate` | 0 | tested; no measurable effect |
| `op_regate_sunk_gain` | 0.0 | tested at 1.0 |
| `op_regate_eta_freeze` | 0 | tested at 1 |
| `op_contact_quorum` | 0 | **NOT tested** — on the same causal chain as A and P, so it needs the *next* factorial, not a separate A/B |
| `op_draft_attack_floor` | 0.0 | **NOT tested** — no value derivable from the census; §6 item 1 |

`op_contact_quorum` and `op_draft_attack_floor` shipped gated off and
untested deliberately. The first is on the arrival/patience chain, and
running it alone would be the exact attribution error the factorial was
built to avoid. The second has no measured magnitude anywhere in the
census — the finding reports *that* the draft picks the weakest
defender and *what it costs*, but nothing from which an attack-rating
threshold follows — and rule 7 forbids inventing one to have something
to run.

---

## 9. Reproducing

```sh
bash benchmark/oparrival.sh ident     # 3 archived holdout seeds, byte-identical
bash benchmark/oparrival.sh run       # the 9 cells, 144 games, ~2.5 h at JOBS=6
bash benchmark/oparrival.sh report

python3 benchmark/oparrival.py funnel   benchmark/results/oparrival/ab/*
python3 benchmark/oparrival.py pair     benchmark/results/oparrival/ab/000 \
                                        benchmark/results/oparrival/ab/010
python3 benchmark/oparrival.py exchange  benchmark/results/oparrival/ab/*
python3 benchmark/opxrate.py   funnel    benchmark/results/oparrival/ab/000
```

`opxrate.py funnel` on the control cell is what §3's replication table
is read from; it is the same script the original census used, pointed
at a new corpus, which is the point.
