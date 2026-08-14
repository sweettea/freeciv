# ENDGAME: the AI learns the game ends (2026-08-06)

Session `sessions/S-ENDGAME-horizon.md`. Branch `epic/w03-horizon-content`.
Binary PINNED `cc1fab22cea8` (server + `libfreeciv.so`), `--server`,
`--arms deep`, 20 paired seeds per arm, every arm loading a tuning FILE
with the standing pins (`pay_switch_penalty = 1`, `switch_cost_price = 1`,
`peace_seek_gain = 1.0`, `trade_route_love = 5.0`) written out.

Arms archived as `benchmark/tuning/eg-{off,clamp,deflate,mode}.ini`.
Census: `benchmark/endgame.py RESULTS_DIR --window N`.

---

## 0. The evidence the session was launched on, re-verified

`grep -rn "end_turn" ai/ server/advisors/` returns **one** hit, in
`ai/stub/stubai.c`, and it is a function name. `game.server.end_turn`
is plain readable state and no line of `ai/default`, `ai/baseline`,
`ai/deep` or `server/advisors` had ever read it. `MORT` is `#define
MORT 24` (`server/advisors/advtools.h:19`) and every horizon credit in
`deephorizon.c` counts a per-turn value across those 24 turns, discounts
once for the delay, and is charged 24 turns of building upkeep.

So on turn 195 of 200 deep priced 24 turns of payback that do not exist
-- and 24 turns of upkeep that will not be paid either. The clamp is
applied to **both** sides for that reason.

---

## 1. Two mechanisms, and they are NOT the same mechanism

The brief asked for the horizon clamp to be measured separately from the
score mode. Building it made a third distinction necessary, and it is
the finding that matters most for reading the counters.

**horizon clamp** (`endgame_horizon`) -- `MORT` becomes
`MIN(MORT, end_turn - turn)` in the four horizon credits, the plague
term and the upkeep bill. **Provably inert until T-24.**

**arrival test** (`endgame_horizon`) -- a credit whose first instalment
lands after the last turn is refused. `MAX(1, H/ttg)` floors at one
growth step, which is right at 200 turns to run and wrong at five.
Fires **whenever `ttg > turns_left`**, which is mid-game.

**score mode** (`endgame_window` + two gains) -- unfinishable wants
deflated, finishable great wonders paid. Inside the window only.

The counters separate the first two (`clamps=` and `missed=` on the
`deep-endgame` line), and the separation earns its keep immediately:

    endgame_horizon = 1        clamps            missed
    cell             before T-24  after   before T-24  after   first missed
    lategame-80             0   162 865        3 186   7 154        turn 8
    isolated-150            0   252 143        3 473   9 455        turn 32

**The horizon clamp is inert before the window, exactly as the brief's
stop condition required, and the counter proves it: zero, both cells,
40 games.** The arrival test is not, and cannot be -- 31% (lategame) and
27% (isolated) of its refusals happen more than MORT turns from the end,
because a city whose time-to-grow exceeds the entire remaining game has
no growth to credit whatever the turn number is. That is a mid-game
behaviour change and it is stated here rather than buried: it is the
arithmetically correct one, and neither cell's score regressed under it.

The brief's own formula, `amortize(value, MIN(delay, MORT, turns_left))`,
would have been **wrong-signed**. `amortize(benefit, delay) = benefit *
0.958^delay` discounts for a WAIT; clamping the *delay* down to
`turns_left` makes a late benefit worth MORE. The quantity that has to be
clamped is the HORIZON -- the number of turns of payback counted -- which
in this code is the `steps = MORT / ttg` family and the upkeep bill, not
the second argument to `amortize()`. Recorded because the brief's shape
was carried forward from the design note and would have inverted the fix.

---

## 2. lategame-80 (ENDTURN 80, civ2civ3, 6 players), 20 paired seeds

    metric            OFF   CLAMP DEFLATE    MODE
    score          169.47  170.15  170.13  169.92
    pop             42.07   42.31   41.89   41.96
    mean_city_size   5.38    5.42    5.40    5.41
    unitsbuilt      39.67   39.56   40.18   39.98
    cities           8.15    8.12    8.07    8.07
    techs           53.00   53.03   53.02   53.01
    wonders (final)  0.22    0.20    0.17    0.20
    landarea        97 242  97 175  96 583  96 467

    metric            CLAMP-OFF     DEFLATE-OFF        MODE-OFF
    score            +0.68 +1.37    +0.67 +1.61    +0.45 +0.86
    pop              +0.24 +2.40    -0.17 -0.88    -0.11 -0.46
    mean_city_size   +0.04 +2.69    +0.03 +0.90    +0.04 +1.13
    unitsbuilt       -0.11 -0.73    +0.52 +2.69    +0.31 +1.92
    cities           -0.03 -0.55    -0.07 -1.63    -0.08 -1.70
    techs            +0.03 +0.75    +0.03 +0.83    +0.01 +0.16
    wonders (final)  -0.02 -0.81    -0.04 -2.03    -0.02 -1.00
    landarea           -67 -0.19      -658 -1.68     -775 -1.81

    wonders completed in the last 15 turns, per deep seat
      OFF 0.14    CLAMP 0.12    DEFLATE 0.10    MODE 0.12

## 3. isolated-150 (ENDTURN 150, civ2civ3, 4 players), 20 paired seeds

    metric            OFF   CLAMP    MODE   CLAMP-OFF      MODE-OFF
    score          184.06  184.31  185.61   +0.25 +0.19  +1.55 +0.90
    unitsbuilt      89.15   90.67   93.09   +1.52 +2.38  +3.94 +5.47
    wonders (final)  1.48    1.52    1.61   +0.05 +1.29  +0.14 +2.60
    munits          22.89   23.51   22.91   +0.62 +2.74  +0.03 +0.04
    cities          14.68   15.03   14.78   +0.35 +1.68  +0.10 +0.39
    techs           27.95   27.75   28.05   -0.20 -1.80  +0.10 +0.88
    pop             86.25   86.67   85.62   +0.42 +0.45  -0.62 -0.44
    mean_city_size   6.66    6.62    6.61   -0.04 -1.37  -0.06 -1.50

    (unitsbuilt MODE-OFF wins on 19 of 20 seeds)

    wonders completed in the last 15 turns, per deep seat
      OFF 0.39    CLAMP 0.44    MODE 0.50    (+28% against OFF)

---

## 4. What the numbers say

**Score is positive on both cells in every arm and resolved in none of
them.** +0.68 / +0.67 / +0.45 (lategame) and +0.25 / +1.55 (isolated),
all at |t| < 1.7. Twenty seeds are direction. The honest verdict is
"never negative, four arms, two cells, forty games" -- not a win.

**The wonder premium works, and only where wonders exist.** On
isolated-150 the deflator alone is not measured, but the mode's
in-window wonder completions run 0.39 -> 0.50 per seat and final wonders
+0.14 at **t +2.60**, the largest t any endgame arm produced. On
lategame-80 -- 0.22 wonders per seat for a whole game against
isolated's 1.48 -- the DEFLATE arm *costs* wonders (-0.04, t -2.03) and
the premium buys back exactly what the deflator took (0.17 -> 0.20) and
no more. A wonder is the longest build in the game, so it is the first
thing an unfinishability test refuses; on a cell with a real wonder
population the premium protects the ones that can still land, and on a
cell without one there is nothing to protect. **A verdict inherits its
venue, for the sixth time in this project.**

**The deflator's own effect is "build more, cheaper things".**
unitsbuilt +0.52 (lategame) and +3.94 at t +5.47 on 19 of 20 seeds
(isolated): shields that were going into something the bell would
interrupt go into something that finishes, and `built/10` is a score
term. The build distribution shows it directly -- on isolated-150 the
last 15 turns pick Musketeers 21.7% and Barracks II 13.0% against
Settlers 7.3%, where Settlers are 1453 of the rest-of-game picks.

**The clamp alone is the cheaper half of the same story.** On
lategame-80 it is the only arm that moves population (pop +0.24
t +2.40, mean city size +0.04 t +2.69) -- growth credits stop being paid
for growth that does not arrive, and the citizens that DO arrive are
worked better. On isolated-150 it buys units and cities instead
(unitsbuilt t +2.38, munits t +2.74, cities +0.35).

**The mode executes at scale.** 4 359 (lategame) / 5 178 (isolated)
improvement wants deflated per game, 191 / 212 unit wants deflated, 29.6
/ 47.7 great wonders paid the premium. Deflated units have a median
turns-to-build of 12 against a median 4 turns left. Nothing here is a
mechanism that failed to fire.

---

## 5. Fold / no-fold

**Recommendation: fold nothing yet; both calls belong to the human.**

    endgame_horizon          NO FOLD YET   never measured negative on
        either cell, both guard columns in its favour, score +0.68 /
        +0.25 unresolved at 20 seeds. THE FOLD CANDIDATE, and it wants
        a 40-seed arm plus the fairness call in section 6 first.
    endgame_window           NO FOLD   the mode's whole positive signal
        is one cell's wonder column.
    endgame_unfinished_gain  NO FOLD   probe value 0.0 (refuse
        outright) is the most aggressive setting in its range; a
        gentler deflator has never been run.
    endgame_wonder_gain      NO FOLD   probe value 3.0; the only knob
        in this session with a t > 2 behind it, on one cell.

Every one is a registered PLACEHOLDER in all four audit places, in the
G / ECON / ALL blocks, in the generated `deep-tuning` line and in
`benchmark/tuning-params.ini`. The window (15) and both gains were probe
values chosen to make the mechanism visible, not tuned optima, and they
go to the cloud pile as a coupled triple: the deflator and the premium
interact by construction, because the premium's population is exactly
the wonders the deflator would otherwise refuse.

---

## 6. The fairness note, stated once so it is not inherited silently

**Stock does not know the game ends either.** `ai/default` and
`ai/baseline` are unchanged and will keep planning over a fixed 24-turn
horizon on the last turn of every game. With `endgame_horizon = 1` deep
would be the only seat in any benchmark that reads the bell.

That is defensible on two grounds and the human owns the decision:

1. **Benchmark realism.** A human absolutely plays the bell in a scored
   game. Every acceptance verdict in this project is taken on a cell
   that ends on the turn limit (199/199 games), which is the one regime
   where "the game is infinite" is guaranteed false.
2. **Real-game value.** Wherever `endturn` is set at all -- and it is
   set in every cell this project runs -- the clamp is simply correct
   arithmetic, not an exploit of the metric.

Against it: it is one more asymmetry on top of HEDGE's, and HEDGE's went
the other way (deep pays a cost stock does not). This one is a
capability stock lacks. `endgame_horizon = 0` restores comparability
with everything published before today, and every number in this file
was taken with the arms differing in nothing else.

---

## 7. Owed, and what the next session should know

- **A 40-seed arm on `endgame_horizon` alone.** Score +0.68 (t +1.37,
  lategame) and +0.25 (t +0.19, isolated) is the profile the project's
  own method note describes: twenty seeds are direction, forty are a
  claim. Three estimates have already moved under power.
- **The deflator has never been run at a gentler setting.** 0.0 refuses
  an unfinishable build outright, including a 750-want founder sitting
  on `DAI_WANT_DOMESTIC_MAX`. A value between 0 and 1 is the obvious
  next arm and it was not run.
- **`city_turns_to_build()` does not model a caravan arriving to Help
  Wonder.** A wonder a caravan fleet would finish reads as unfinishable
  and is deflated. Conservative in the direction that matters -- it can
  only decline to pay a wonder, never overpay one -- but it under-counts
  exactly the population R2b built.
- **Three consumers were NOT clamped because they ship off.**
  `deeptrade.c`'s route horizon (`MORT` at `:1123`, `:1142`, `:1193`) and
  `deeprates.c`'s three MORT terms are dead code at the compiled
  defaults (`trade_merit = 0`, `wonder_merit = 0`, `rates_enabled = 0`).
  Clamping them would have been untestable. The rates case is worth a
  note: MORT appears in all three of its terms symmetrically, so a
  uniform clamp there is a **no-op on the pick** -- the endgame content
  in the rates is the science term's `wait` test (a research that cannot
  finish before the bell is worth nothing), which is a separate change
  and was not built.
- **Culture is DARKFX's job, not this session's.** `S-ENDGAME-horizon.md`
  §3 deprioritised culture/History as metric-gaming; the derby forensics
  (`findings/europe-derby-hindsight.md` §1) re-sized it as the plurality
  of the metric at long horizons. This session's §3 note should now be
  read as "leave culture to DARKFX", not "culture does not matter". The
  score mode built here is deliberately horizon-blind about culture: it
  pays great wonders (which carry History) only through the score term,
  and it prices no compounding stock at all.
- **The window is the least examined number here.** 15 turns was a probe
  on a T80 and a T150 cell -- 19% and 10% of the game respectively. The
  right shape may be a fixed number of turns, a fraction of the game, or
  a per-city function of turns-to-build; nothing was measured to
  distinguish them.

## 8. Off-switches proven

Smoke cell, 10 paired seeds, `eg-off.ini` on both binaries. The
pre-session binary `6051a0e8bbbe` (HEAD `1a3c2b8689`, rebuilt
bit-for-bit from the checkout, and the same binary the concurrent
defence-tilt run pinned) against the session binary `cc1fab22cea8`:
**10/10 scorelogs bit-identical.**

    70f2c13136a4  ed23c9303eb5  6bfeb8bbf7cf  2314c27e8c5e  7f08e0eb79de
    ca1fe91c60b1  912773e6b97e  825c66af8f8b  4bd40ba5c2f7  7b74bb209f89

Execution counters: **0.00 `deep-endgame` lines per game** in the OFF
arm of both cells (40 games), against 243.7 / 189.1 player-turns a game
with the clamp on; `deep-endgame-unit` 0.00 in OFF and CLAMP against
191.3 / 212.5 in the mode arms.

`meson test -C build` **4/4**; `python3 benchmark/tune-audit.py` CLEAN
**164/164/164/164** (`endgame_horizon`, `endgame_window`,
`endgame_unfinished_gain`, `endgame_wonder_gain`).
