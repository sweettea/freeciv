# PEACE: the cease-fire trap, the embassy clause, and three unread signals

Session PEACE, 2026-08-06, branch `epic/w03-horizon-content`. Evidence
base: `findings/deep-dive-2026-08-05/diplomacy-gamestate.md`. All work
in `ai/deep`'s own wrappers; **zero `ai/default`, `ai/baseline` or
`advisors` changes**.

Binary PINNED (`cc43f00c3f7a36`, server + `libfreeciv.so`, `--server`),
`--arms deep`, 20 paired seeds per arm per cell, every arm loading a
tuning FILE that pins `pay_switch_penalty = 1` and
`switch_cost_price = 1` so no arm differs in HEDGE's handicap. Archived
inis: `benchmark/tuning/peace-{off,seek,embassy,all}.ini`.

---

## 0. The seam the brief asked me to report does not exist

The stop condition was "the pre-pass can't reach the proposal machinery
without touching `ai/default` beyond visibility (report the seam)".

`dai_diplomacy_suggest()` (`daidiplomacy.c:937-954`) *is* static, but it
is a three-line wrapper over `could_meet_with_player()` plus
`handle_diplomacy_init_meeting_req()` and
`handle_diplomacy_create_clause_req()`, both already public in
`build/hand_gen.h`. Deep reaches the proposal machinery by calling the
same three functions. Nothing in `ai/default` changed, and nothing
needed to.

Two eleven-line stateless statics are COPIED rather than made visible,
under rule 3's last-resort clause, because the brief forbids touching
`ai/default` at all this session: `get_allied_with_enemy_player()` and
`clear_old_treaty()`. Deep must apply exactly the guard the
counterparty's own valuation applies, or it proposes into a
`-BIG_NUMBER` refusal on every turn of the window.

Both server handlers call `call_treaty_evaluate()` on **both** sides
synchronously, so a proposal is judged, accepted and concluded inside
the suggest call. That fact is what makes the census possible without
stored state (§1a) and it is also what made the first measurement wrong
(§1b).

## 0a. The off switch, proved by byte identity

The pre-session tip `ac153b34a0` built and pinned (`ed0a5ec1b9e355`,
`tuning=none`) against the session binary `cc43f00c3f7a36` loading
`peace-off.ini` (every PEACE gain `0.0`, HEDGE's two knobs at their
compiled defaults): **10 of 10 smoke seeds byte-identical scorelogs,
and identical `metrics.csv`.**

That covers the leader read too, which runs unconditionally in the off
arm and logs — it reads and reports, and it changes nothing.

---

## 1. Lever 1 — the cease-fire trap

### 1a. What it does

A cease-fire lapses back to WAR after 16 turns (`diplhand.c:57`). The
counterparty refuses to price peace at all while `turns_left > 4`
(`daidiplomacy.c:405`). And the AI-AI anti-spam gate skips the entire
diplomacy branch for `fc_rand(4)+3` turns (`:1814,:1824`). The window
is four turns wide and the gate is the same size, so the pass that
would propose peace is frequently not running on any turn the other
side is allowed to say yes.

Deep runs a pre-pass in its own `diplomacy_actions` wrapper, before
`ai/default`'s gated pass, on exactly the turns the window is open.
`peace_seek_gain` is the fraction of `ai/default`'s own
`req_love_for_peace` bar deep will waive; `0.0` asks for the full stock
bar and is both the off switch and the honest control. Every structural
guard is stock's own and none is waived. One guard is deep's instead of
stock's: the no-mercy rule (`aplayer == target`) is replaced by "do we
have a live OPERATION aimed at one of their cities", because stock's
war target is a love-bled preference recomputed every turn while a
formed wave is a commitment peace would strand.

### 1b. A measurement that was wrong in its own output

`ds` points at live state and the treaty concludes inside
`deep_treaty_suggest()`. The first probe therefore logged `left=16` —
the armistice's counter — on every proposal that worked, when the
pre-pass only ever runs at `turns_left <= 4`. The tell was that the
number was impossible. `turns_left` is now snapshotted before the
proposal, and the line gained `now=`, read deliberately after: a peace
that carried has already moved the pair to `DS_ARMISTICE` (= 0), so one
field separates "asked" from "got it" with no stored state.

### 1c. Why deep refuses, when it refuses (contested, 1070 windows)

    why="yes"          608     proposed
    why="love"         259     love below the bar -- with the bar at 0
                               (gain 1.0) these are NEGATIVE love
    why="enemyally"     90     they are allied to somebody we fight
    why="allyswar"      84     our ally is at war with them
    why="countdown"     29     we have already committed to attacking

**Five of the ten guards never fired once**, `ourtarget` among them: in
1070 windows deep never had a live operation aimed at a cease-fire
partner. The commitment guard costs nothing and buys nothing in this
population, and that is worth knowing before anyone builds on it.

## 2. Lever 2 — the embassy clause

`ai/default` prices RECEIVING an embassy at `tech_leak_pct / 10` = 10
gold flat forever and GIVING one at peace at `-5 * game.info.turn`,
unbounded — in the one ruleset where embassies ARE the tech leakage
(upstream bug #46). The swap nets `10 - 5T` and is impossible from turn
2; it is moot anyway because the only path that offers the clause,
`dai_share()`, is alliance/team-gated. The observed count of peace-level
embassy swaps in 800 saves was structurally **zero**.

Deep prices the clause from the ruleset's own arithmetic: under
`TECH_LEAKAGE_EMBASSIES` (`common/research.c:954-981`) a tech costs
`tech_leak_pct / alive_players` percent less for each player who knows
it and with whom we hold an embassy, and that count is per player and
NOT de-duplicated — so one new embassy is worth exactly one such step
on every tech that player knows and we do not. Priced at this
research's own remaining bulb cost (the post-leak figure, so
conservative by the leak already banked) and converted to gold at
`ai/default`'s own 3x. Rate, denominator and exchange rate are all
read; the residue `embassy_price_gain` owns the one unmodelled term,
which is discounting.

**Scope, decided before the batch and not after it: peace level only.**
The first cut also fired at `DS_ALLIANCE`/`DS_TEAM`, where `dai_share()`
already swaps embassies and prices giving at zero. Deep's give side is
a real cost, so charging an ally for a clause stock hands over free
could only REMOVE embassies from the 22.6% of pairs that end allied.
That arm was five minutes in and was thrown away.

**Which channel owns which case.** The TREATY channel owns every AI
deep is at peace or armistice with, because there a clause delivers the
embassy for free. The DIPLOMAT channel (`embassy_science_value`) keeps
everyone else — at war, never met, allied (stock's), and every human,
none of whom a clause can be relied on to reach. The diplomat want
stops counting the treaty channel's players only when the gate is open.

## 3. Lever 3 — the three signals nobody reads

`tech_leader` and `production_leader` are recomputed for every player
every turn (`advdata.c:486-500`) and read by nothing. `WIN_CAPITAL` is
written at `daidiplomacy.c:880` and only `WIN_SPACE` is read back.

Deep computes its own (the stock loop uses `players_iterate`, not
`_alive`, with a strict `<`, and can name a dead player — upstream bug
#45), skips barbarians, and resolves a tie to nobody. It logs them
whether or not anything consumes them.

**`WIN_CAPITAL` is structurally unreachable in these cells: 0 of 24 000
player-turns.** It is set only when `adv->dipl.spacerace_leader ==
target`, and `player_leading_spacerace()` answers nobody until a
spaceship is STARTED, which never happens in a 150-200 turn game. The
third "free signal" the finding listed is not a signal here at all. The
other two are live: a distinct tech leader is named on 92% of
player-turns.

`leader_check_gain` is the first consumer — an assault-target bias
toward the runaway's cities. `0.0` skips the multiply rather than
performing it.

---

## 4. Results

### 4a. contested, 20 paired seeds

Diplomatic census from the final saves (`benchmark/diplcensus.py`,
`closest` = the server's own `max_state`):

    contested                        OFF      SEEK   EMBASSY
    met pairs / game                33.9      38.7      36.9
    PEAKED AT CEASE-FIRE           29.7%     15.9%     28.6%
    ended at War                   74.2%     65.1%     75.2%
    ended peace-or-better          24.8%     34.4%     23.7%
    ever reached peace-or-better   44.0%     52.7%     42.1%
    embassy (ordered pairs)        31.6%     35.8%     35.5%
    peace windows / game               0      53.5         0
    ... PROPOSED                       0      30.4         0
    ... SIGNED                         0      19.8         0
    embassy treaties judged/game       0         0      48.7
    ... CLEARED                        0         0      26.9

Paired score and metrics (`benchmark/paircmp.py`, deep seats only):

    contested                 OFF     SEEK   delta     se      t   wins
    score                   80.87    84.47   +3.60   2.42  +1.48  12/20
    techs                   17.71    18.94   +1.24   0.63  +1.96  14/20
    pop                     24.83    26.87   +2.04   0.93  +2.19  14/20
    mean_city_size           5.65     6.07   +0.41   0.18  +2.32  13/20
    cities                   4.41     4.54   +0.14   0.09  +1.45  11/20
    unitsbuilt              42.72    46.14   +3.42   1.45  +2.36  14/20
    unitskilled              9.82     6.82   -3.00   0.97  -3.11   5/20
    unitslost               12.98    11.06   -1.92   1.28  -1.50   6/20

    contested                 OFF  EMBASSY   delta     se      t   wins
    score                   80.87    80.94   +0.07   2.13  +0.04  13/20
    techs                   17.71    17.98   +0.27   0.53  +0.51  11/20
    unitslost               12.98    14.12   +1.14   1.11  +1.03  12/20

    contested                 OFF      ALL   delta     se      t   wins
    score                   80.87    85.51   +4.64   2.17  +2.14  15/20
    techs                   17.71    19.03   +1.32   0.51  +2.61  15/20
    cities                   4.41     4.64   +0.23   0.09  +2.68  12/20
    pop                     24.83    26.82   +1.99   0.79  +2.53  13/20
    landarea                49075    50588  +1512.5  720.5  +2.10  15/20
    settlers                 0.30     0.14   -0.16   0.06  -2.64   3/20

ALL's diplomatic census: peaked-at-cease-fire 17.2%, ended
peace-or-better 33.5%, embassy 41.7% (from 31.6%).

### 4b. lategame-80, 20 paired seeds

    lategame-80                      OFF      SEEK   EMBASSY       ALL
    met pairs / game                20.4      20.6      20.4      20.9
    PEAKED AT CEASE-FIRE           41.9%     28.6%     42.4%     28.5%
    ended at War                   77.7%     71.4%     78.2%     73.0%
    ended peace-or-better          19.9%     26.7%     19.9%     25.6%
    embassy (ordered pairs)        26.3%     32.0%     28.6%     34.8%
    peace windows / game               0      51.3         0      53.1
    ... PROPOSED                       0      19.7         0      20.9
    ... SIGNED                         0       8.3         0       8.7
    embassy treaties judged/game       0         0      21.3      23.9
    ... CLEARED                        0         0      10.9      12.4

    lategame-80               OFF     SEEK   delta     se      t   wins
    score                  168.05   169.97   +1.93   1.31  +1.47  13/20
    techs                   52.75    53.12   +0.37   0.16  +2.36  13/20
    pop                     41.67    42.38   +0.71   0.69  +1.02  13/20
    munits                  15.09    14.28   -0.82   0.44  -1.84   6/20

    lategame-80               OFF  EMBASSY   delta     se      t   wins
    score                  168.05   169.49   +1.44   0.66  +2.18  10/20
    techs                   52.75    52.87   +0.12   0.05  +2.48   7/20
    mean_city_size           5.32     5.39   +0.07   0.03  +2.41  10/20
    cities                   8.08     8.01   -0.07   0.05  -1.53   2/20
    unitsbuilt              39.76    39.36   -0.40   0.21  -1.94   1/20
    landarea                95258    94592  -666.7  451.1  -1.48   3/20

    lategame-80               OFF      ALL   delta     se      t   wins
    score                  168.05   170.28   +2.22   1.27  +1.76  11/20
    techs                   52.75    53.08   +0.33   0.10  +3.27  15/20
    unitskilled             11.84    10.68   -1.17   0.52  -2.26   5/20
    munits                  15.09    14.22   -0.87   0.50  -1.72   5/20

### 4c. THE HONEST TRADE

The brief asked for the peace-vs-conquest trade reported rather than
hidden. On contested it is visible and it points the same way as the
score: **deep kills 31% fewer units and loses 15% fewer**, and the
growth columns are where the gain lands (`pop` +2.04, `mean_city_size`
+0.41, `techs` +1.24). The cell also keeps more rivals alive — met
pairs per game rise 33.9 -> 38.7, which is civs not being eliminated.

That last number is the part to watch: on a cell decided by conquest,
peace is a conquest forgone, and a rival deep did not eliminate is a
rival still scoring.

**The trade did not turn negative on either cell.** Every arm on both
cells scores at or above OFF:

    score delta vs OFF        SEEK      EMBASSY          ALL
    contested          +3.60 (1.48) +0.07 (0.04) +4.64 (2.14)
    lategame-80        +1.93 (1.47) +1.44 (2.18) +2.22 (1.76)

That is a real answer and it is not the one the brief expected. The
conquest deep forgoes is visible in the mechanism -- contested's
unitskilled falls 31% under SEEK (t -3.11) and lategame-80's falls
under ALL (t -2.26) -- and the score still goes up, because what deep
buys with the shields and the citizens it stops spending on war is
growth and research. `techs` is the one column positive on both cells
in every live arm, and it is the column the score weights x2.

**Nothing here clears twenty seeds on score alone.** Contested's ALL
(t +2.14, 15/20) is the closest, and by this project's own standard
twenty seeds are direction and forty are a claim. The GUARD metrics do
clear: the trap population halves on contested (29.7 -> 15.9) and falls
a third on lategame-80 (41.9 -> 28.6), and both are far outside noise.

The trade-route dividend that would make peace pay a second time is
TRADE's, not this session's: nothing here builds a route.

### 4d. What the embassy lever is, on the evidence

It works and it is nearly free. 26.9 treaties cleared a game on
contested against a structural zero, embassy coverage 31.6% -> 35.5%,
and score +0.07. On lategame-80 it clears 10.9 a game for +1.44
(t +2.18) -- but look at what moves underneath: `techs` +0.12
(t +2.48) and `mean_city_size` +0.07 (t +2.41) against `cities` -0.07
(t -1.53), `unitsbuilt` -0.40 (t -1.94), `landarea` -667 (t -1.48) and
wonders down on 19 of 20 seeds. The lever buys research and spends
expansion, which is what a research discount should do, and on a
lategame cell that is a good trade while on contested it is a wash.

### 4e. Lever 3 measured on its own

Run on a second pin (`13bf5746b5a720`) that adds the bias's execution
counter and nothing else. Proof it added nothing else: the OFF arm's
`metrics.csv` is IDENTICAL to the first pin's OFF arm and all 20
scorelogs are byte-identical.

    contested                 OFF     LEAD   delta     se      t   wins
    score                   80.87    82.31   +1.44   1.38  +1.04  11/20
    unitsbuilt              42.72    45.23   +2.51   1.14  +2.21  12/20
    unitskilled              9.82    11.34   +1.52   0.68  +2.24  13/20
    unitslost               12.98    15.04   +2.06   0.97  +2.12  12/20
    mean_city_size           5.65     5.52   -0.13   0.08  -1.61   7/20
    pop                     24.83    24.40   -0.43   0.55  -0.78   5/20

    target-bias applications, max per seat:  2216 (LEAD) vs 0 (OFF)

**It executes, it does exactly what it says, and it is the mirror image
of lever 1.** Pointing waves at the runaway makes deep fight more: more
built, more killed, more lost, and growth down. Score +1.44 at t +1.04
is not a result. It is filed as a signal that is now READ and consumed
minimally, which is all this session was briefed to do -- the statecraft
that would make the read pay (dogpiling a distracted leader, counting a
target's existing enemies) is BALANCE's.

Deep also stops paying diplomats for what the clause delivers, but only
for peace-level AI counterparties -- a population that is 3-5% of pairs
in the OFF world and 25-34% once the pre-pass runs. **The two levers
compound in that specific way and it is worth saying out loud: lever 2
gets most of its population from lever 1.** Embassy coverage bears it
out -- SEEK alone moves it 31.6 -> 35.8 on contested without touching
the clause at all, and the two together reach 41.7.

---

## 5. What ships, and what does not

**Every knob ships at its off value** (`peace_seek_gain 0.0`,
`embassy_price_gain 0.0`, `leader_check_gain 0.0`), and that is a
deliberate refusal, not caution about the mechanism.

`peace_seek_gain = 1.0` was chosen for the batch as "waive the whole
bar, maximum population" -- it is not a tuned value and there is no
evidence any particular value is right. Shipping it as a compiled
default would be picking a number to chase a benchmark, which is rule
7. The knobs are registered in `tuning-params.ini` and in the ALL /
ECON / MIL blocks and are ready for a pass; the pass is what should
choose the value, on training seeds, reported on held-out ones.

What the evidence supports handing to the human:

- **Lever 1 is the strongest thing here** and the one worth a 40-seed
  run at power. Both cells, guard metrics far outside noise, score
  direction positive on both, no cell where the conquest-forgone cost
  materialises.
- **Lever 2 is cheap, works, and is worth roughly nothing on its own**
  -- the seventh mechanism in this project to prove out and measure
  flat. It compounds with lever 1 because lever 1 supplies its
  population.
- **Lever 3 is a read, not yet a policy.** Filed.
- **A timing-only arm was never run.** `peace_seek_gain` bundles two
  effects: the pre-pass RUNS at all, and the love bar is waived. An arm
  at a small gain would separate "stock would have proposed if it had
  been asked on the right turn" from "deep proposes to players stock
  never would". That decomposition is the first thing a follow-up
  should measure, and it is the difference between fixing a timing bug
  and changing the AI's diplomatic character.

## 6. For the upstream-bugs appendix

Nothing new to file -- #43, #44, #45 and #46 all held up under
measurement and none was fixed in place. Two of them now have numbers:

- **#45** (`tech_leader`/`production_leader` can name a dead player):
  deep's own alive-only, tie-resolving version names a distinct tech
  leader on 92% of player-turns, so the signal is live and the bug is
  a real corruption of a real signal, not a corner case.
- **#46** (the embassy swap is arithmetically impossible): confirmed
  by construction. Deep's re-priced clause clears 26.9 treaties a game
  on contested where stock's clears zero, and the population is
  entirely peace-level pairs -- exactly where `dai_share()` never looks.

And one measured non-signal, which belongs with them:

- **`WIN_CAPITAL` is unreachable in every cell we run.** 0 of 24 000
  player-turns across four arms and two cells. It is set only when
  `adv->dipl.spacerace_leader == target`, and `player_leading_
  spacerace()` names nobody until a spaceship is STARTED, which never
  happens inside 150-200 turns. The finding listed it as one of three
  free third-party signals; it is not a signal here at all.
