# Combat E2: attrition out of the substrate, and what it costs to win

Session M1, 2026-08-06. Branch `epic/w03-horizon-content`.

The survival substrate (combat E1, 2026-07-31) walked the full duel
chain and returned one number. This session takes the other half of
the walk out of it — what the ATTACKER is expected to lose — and
spends it in four places: the assault want, the pool that want is
priced against, the launch bar of a mustered wave, and the moment a
unit would attack tired.

Numbers here are the session's own; the narrative lives in
`STATUS.md` under the M1 section of the same date.

## 1. The substrate change

`deep_stack_survival_losses()` is the same dynamic program.  At every
state the duel has exactly two outcomes: the defender survives (and
the attacker is dead) or the defender falls (and the attacker lives,
its turn spent).  The survival mass the walk already computes per duel
IS the attacker's death probability, so the extension is one
multiply-add per live state.

Mass absorbed into the annihilated-stack sink is not live, so an
attacker with nothing left to fight is charged nothing.  That is what
makes `n_att - sum(att_death)` an expected FORCE SIZE rather than a
casualty rate, and it is the quantity "can I hold what I take" needs.

Performance, the stop condition the brief named — 32 attackers x 16
defenders at 40 HP, 200 calls, -O2:

    scalar walk      2.406 ms/call
    with losses      2.394 ms/call      ratio 0.995

No measurable cost; the DP's asymptotics are unchanged.

`tests/deepattrition_test.c` is a second L0 target beside
`deepodds_test.c`, all closed forms:

- Four coin-flip duels against one defender: P(attacker j dies) =
  0.5^(j+1) exactly (0.5, 0.25, 0.125, 0.0625), expected survivors
  3.0625, defender survival 0.0625.
- A defender that cannot win a round charges only its killer; the
  three attackers that walk in behind it are charged nothing.
- Degenerate cases clear the array rather than leaving it stale.
- The accounting identity, on one-round duels where every quantity has
  a closed form: every duel kills exactly one unit, so E[duels] =
  E[attackers dead] + P(defender dies), checked at 1.56 = 0.624 +
  0.936.

Everything the output inherits is the substrate's own model, stated in
`civ2civ3-adjustments.md` "Combat workstream epic 1": one wave, no
healing, no promotion, no reinforcement between duels, military
garrison only, observed units only (RC6), and the 32/16 computational
caps.

## 2. The cost census, taken in the arm that does not charge it

`loss=` is logged whether or not `attack_loss_gain` spends it, so the
CONTROL arm is a free counterfactual laboratory — the same trick
HEDGE used to size the switch penalty before imposing it.
lategame-80, 20 paired seeds, `m1-off.ini`, binary `8ce4a7617a7c`:

    per game                          off arm
    deep-kill evaluations             2148.20
    ... priced positive gross         2068.00
    ... would DECLINE at gain 1.0      668.45   32.3%
    ... amortized to zero anyway       660.40
    ... marginal loss NEGATIVE         995.65   48.1%
    gross want, summed              386987.90
    expected loss, shields          -22807.75
    pool members, summed              9459.05
    pool members claimed elsewhere     995.15

Two things fall out of that table.

**The cost term is not a veto.** At gain 1.0 — the loss charged at
SHIELD_WEIGHTING, the codebase's own shields-to-want rate — it refuses
32.3% of the attacks that price positive. HEDGE's switch debit at the
same conversion zeroed 86.8% of the candidates it touched, so the
worry that this conversion is always a near-veto is answered on this
population: it is not.

**Half the marginal blades were priced as SAVING losses.** 48.1% of
positively priced evaluations have a negative marginal loss, mean
-5.40 shields over 63 375 lines. The mechanism is the stream's own
ordering: attackers strike strongest-first, so a strong blade inserted
at the head often kills the defender in the first duel and the weaker
pool members behind it never fight at all.

That is a real modelled effect, and it is NOT spent. Paying it would
credit the build with the same causal event `Dp` already credits it
with — the defender dying sooner — one effect counted twice, with the
second count pointing the opposite way from the term's purpose. The
term is clamped at zero, so it can make deep more selective than the
pre-E2 want and never more aggressive. **"Massing a strong blade at
the head saves the weak ones behind it" belongs to whichever epic
prices force COMPOSITION**; this one only had to stop mispaying for
it.

## 3. The harness defect this session found, and what it cost

The first lategame pair came back **bit-identical on all 20 seeds**
with `attack_loss_gain = 0` in one arm and `1.0` in the other. The
cause is not in `ai/deep`:

    attack_loss_gain = 0        <- an INT token to secfile
    attack_loss_gain = 0.0      <- a FLOAT token

`TUNE_DOUBLE` reads with `secfile_lookup_float_default()`, which
cannot parse an int-typed entry and returns the compiled default. The
key name validates, the entry exists, the result key differs, and the
arm silently runs the DEFAULT. The `deep-tuning` line is what caught
it: both arms printed `attack_loss_gain=1 tired_attack_floor=0.9`
while the file asked for 0 and 0.

This is the third member of a family the harness already guards two
members of (no `[tuning]` header; a key the loader never reads).
`run.sh` now reads each key's declared type out of `tuning-params.ini`
and refuses an int-formatted value for a float key. **All 20 archived
inis under `benchmark/tuning/` were scanned with the same test and are
clean**, so no published result is affected — the two files this
session wrote were the only ones.

The wasted pair is not wasted evidence: because both its arms charged
the cost and neither ran the ops-side changes, it isolates the OPS
pair (pool reach + odds launch + hold test) exactly.

## 4. The cost of victory, isolated (contested, 20 paired seeds)

The arm that changes ONLY `attack_loss_gain`, 0 -> 1.0. Binary pinned
`8ce4a7617a7c`, both arms pinning HEDGE's two switch keys.

    metric                    cost OFF     cost ON    delta      se      t  wins
    score                        79.12       80.39    +1.27    1.08  +1.18 14/20
    unitsbuilt                   54.52       50.31    -4.21    0.68  -6.17  2/20
    UNITSLOST (ours)             23.99       19.58    -4.41    1.00  -4.40  3/20
    unitskilled                  17.93       14.64    -3.29    0.92  -3.58  4/20
    munits                       14.49       14.17    -0.33    0.54  -0.60  9/20
    pop                          24.13       24.68    +0.54    0.45  +1.20 13/20
    mean_city_size                5.29        5.41    +0.12    0.10  +1.23 13/20
    cities                        4.51        4.52    +0.01    0.09  +0.14  7/20

**This is the behaviour the epic was asked to produce, and it shows up
on the metric the acceptance criteria named.** Deep declines attacks
it used to price positive, builds 4.21 fewer units a game, and loses
4.41 fewer of its own (t -4.40, worse on only 3 of 20 seeds). It kills
3.29 fewer too — that is the honest other half: fewer attacks is fewer
kills. Score is +1.27 (t +1.18, 14/20): positive, and twenty seeds
cannot call it.

The pricing side, same seeds:

    per game                     cost OFF     cost ON        t
    deep-kill evaluations         3069.50     3086.45     0.51
    ... DECLINED on cost          1668.00     1757.65     1.33
    ... want reaching zero            0.10     1757.35    17.35
    net want, summed            165334.48    89291.96   -12.76
    operations formed                6.30        5.15    -1.13
    operations launched              5.25        4.45    -0.70

**The offensive want mass falls by 46%** and 57.8% of positively
priced attacks are refused outright. Fewer campaigns form (-1.15) and
fewer launch (-0.80), which is the same decision reaching the
operations layer.

## 5. The whole epic (contested, 20 paired seeds)

    metric                     all OFF     all ON    delta      se      t  wins
    score                        79.12      80.87    +1.75    1.37  +1.27 11/20
    unitsbuilt                   54.52      42.72   -11.80    1.28  -9.22  0/20
    UNITSLOST (ours)             23.99      12.98   -11.01    1.16  -9.47  0/20
    unitskilled                  17.93       9.82    -8.11    0.99  -8.21  0/20
    munits                       14.49      13.07    -1.43    0.57  -2.50  4/20
    mean_city_size                5.29       5.65    +0.36    0.12  +3.07 15/20
    wonders                       0.28       0.41    +0.13    0.05  +2.43 11/20
    techs                        17.13      17.71    +0.57    0.40  +1.43 13/20
    cities                        4.51       4.41    -0.10    0.11  -0.93  9/20

**Deep loses HALF the units it used to lose** (23.99 -> 12.98, worse
on zero of twenty seeds) while building 22% fewer, and the freed
shields are visible where the brief asked us to look for them: city
size +0.36 (t +3.07), wonders +0.13 (t +2.43), techs +0.57. Score
+1.75 (t +1.27, 11/20) — the same direction as the cost term alone
and still inside the noise twenty seeds can resolve.

The decomposition, per game, all three arms:

    counter                        OFF     cost only    whole epic
    DECLINED on cost           1668.00       1757.65       2431.90
    ... share of positive        55.1%         57.8%         81.9%
    marginal loss NEGATIVE      782.30        717.60        144.20
    ... share of positive        25.8%         23.6%          4.9%
    net want, summed         165334.48      89291.96      26700.21
    pool members, summed      12043.95      12739.25       3203.00
    ... OUT of reach              0.00          0.00      10702.05
    operations formed             6.30          5.15          2.90
    operations LAUNCHED           5.25          4.45          2.60
    march re-gate aborts          4.00          3.20          1.65

The two mechanisms compound in one direction here: the reach test
removes 10 702 member-slots a game, which makes the marginal blade a
FIGHTER rather than a shield (negative-loss population 25.8% -> 4.9%)
and drives the decline rate to 81.9%. The offensive want mass ends at
16% of what it was.

**The launch bar's direction is venue-dependent and that is a
finding.** On contested the ops pair cuts launches (4.45 -> 2.60); on
lategame-80 it RAISES them (4.85 -> 6.35, section 6). Same code, same
knobs, opposite sign — because contested's waves are small and rarely
clear an odds bar the head count would have passed, while
lategame-80's are large enough that the present force often clears the
bar before the last body arrives. A verdict inherits its venue.

## 6. Both mechanisms on lategame-80 (20 paired seeds, corrected arms)

    metric                     all OFF   cost only    whole epic
    score                       169.54   170.68        168.05
                                       +1.14 (t +0.78)  -1.49 (t -0.84)
    cities                        7.78     7.85          8.08  (+1.97)
    unitsbuilt                   38.98    39.18         39.76
    UNITSLOST                    13.07    13.01         13.39
    munits                       14.49    15.20         15.09
    mean_city_size                5.47     5.43          5.32  (-2.31)
    settlers                      0.59     0.37 (-2.08)   0.52
    operations LAUNCHED           4.65     4.85          6.35  (+1.99)

    per game                   all OFF   cost only    whole epic
    DECLINED on cost            682.60    668.45       1392.65
    ... share of positive        33.1%     32.3%         65.0%
    marginal loss NEGATIVE      962.70    995.65        279.15
    net want, summed         200391.91 145463.90      83092.22
    pool members, summed       8982.00   9459.05       2144.10
    ... OUT of reach               0.00      0.00       7097.15
    evals the reach bound          0.00      0.00       1608.75

**The cost term points the same way on both cells** (+1.14 here, +1.27
on contested) and never clears twenty seeds. **The ops pair does not**:
it costs 2.63 here and it is part of contested's +1.75. The reach test
removes three quarters of the pool on both cells and binds on 75% of
lategame evaluations, so this is not a question of whether it executes.

**The four-marginal-campaigns pattern G2 hoped for did not appear.**
Force size per launch is essentially unchanged (4.29 -> 4.27 members);
what moved is the NUMBER of launches, up here and down on contested.
Concentrating an empire's blades into one decisive campaign is not
what a per-target pool definition does, and it remains open.

## 7. Two mechanisms with no population

- **The re-seated tired-attack hold fires zero times in 120 games** on
  both cells. The seat is now provably correct -- D1 showed the old one
  ran where movement had just been restored, and this one sits inside
  the op's own battle where a unit IS tired -- but the window (a member
  adjacent, `0 < moves_left < SINGLE_MOVE`, tired odds below the floor
  AND fresh odds above it) is empty here. Marching members arrive with
  either full moves or none. Two sessions have now paid for this
  mechanism; a third should measure how often any deep unit is
  adjacent-and-tired at all before touching it again.
- **`deep-air` is zero in 120 games**, exactly as G1's floor census
  predicted: neither standing cell reaches flight. The air term ships
  proven-inert-here, not proven-correct.

## 8. What is deferred, and why

- **Sortie pricing as survival-DELTAS** and **escort demand scaled
  from observed sea threat** (the two posture notes recovered from
  code comments) are NOT built. Both are pre-registered in
  `civ2civ3-adjustments.md`, and the escort one is explicitly filed to
  naval epic #16 with its own rule that empty seas justify unescorted
  caravans. Landing either inside an epic that already changed the
  pool definition, the launch bar and the want would have contaminated
  every attribution above.
- **The `deep-bid` military split** R3 asked for is not built either,
  and the reason is structural: the military ladder is a running MAX
  over producers, so a line at that seam reports who raised the max,
  not what each class bid. A true split needs each producer to report
  its own want before the max -- free for deep's own producers (the
  assault want already logs itself in `deep-kill`) and impossible for
  stock's `kill_something_with()` without touching `ai/default`.
- **An underdetermined design choice, flagged rather than guessed**
  (the brief's lost-posture warning): the cost of victory is shipped
  as the MARGINAL loss this blade adds. The alternative reading of the
  same instruction is the assault's TOTAL expected loss discounting
  the prize, `Dp x (worth - cost)`. The marginal form was chosen
  because it matches the benefit's own form; it is also the form that
  needed the clamp. The census logs what the other form would need.
