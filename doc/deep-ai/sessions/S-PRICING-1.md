# S-PRICING-1 — price opportunity in one currency (epic 1 of the pricing workstream)

Brief for one Opus subagent session. One epic, 5–10 commits, then STOP.
Read `CLAUDE.md`, `doc/deep-ai/00-context.md`, `doc/deep-ai/findings/objective-layer.md`
and the STATUS entries from 2026-11-04..07 (search STATUS.md for
"S-EUROPE", "Defect seen in play", "Design item from the Europe WILD game",
"Barbarian-held cities", "late-game passivity", "threatened nation stops
expanding") before touching code. They contain the measured mechanisms this
epic exists to fix, with log evidence and the file/line of each decision.

## Why

The Europe late venue (scoreboard `--europe`, T150→T250, STATUS S-EUROPE)
is the first venue where deep is clearly worse than the stock AI: head-to-head
AUC 0.41, self-play score −47, gold 77 vs 738, half the kills. The
interactive Europe game showed the mechanisms turn by turn. Every one of
them is a decision made by threshold or multiplier where an expected value
in one currency would have decided correctly:

1. **Empty enemy cities nobody takes.** 26 of 28 barbarian-held cities
   stood empty for 100 turns (Marseille among them). The kill pass prices
   them (deep-kill barb=1) but the attacker pool is empty and the target is
   far; nothing sends the unit next door. The retake of a just-lost city has
   the same shape: the kill pool drops every unit tasked AIUNIT_DEFEND_HOME
   before computing odds (deepdefence.c, the loop above the deep-kill log
   line), so a Musketeers standing beside a city held by one wounded
   barbarian is never a candidate.
2. **Operations refused forever.** `op_take_target` is a probability bar
   (0.70, 0.87–0.96 overseas). With empty pools the odds are 0.00–0.03 and
   27–31 evaluations per nation per 30 turns are refused (deep-opodds).
   A cheap sure thing and an expensive one are priced the same.
3. **No opportunity term.** The stance position read (deepstance.c,
   `deep_stance_seed_from_position`) raises mil only through threat and war
   share, econ through free land. A winner with weak neighbours slides to
   the economic corner and gate 5 zeroes its offensive build
   (Russia: assault_gate 2 of 1000 at 141 cities).
4. **Self-pillage.** `deep_pillage_sorties()` (deepdefence.c) pillages any
   extra on an enemy-owned tile at war; after Marseille fell to barbarians a
   French Musketeers burned the French road beside it.
5. **Defenders chosen by score, not per shield.** Deep uses the stock
   defender chooser (`dai_unit_defense_desirability`, max desirability,
   cost only breaks ties), so every slot from Tactics to Labor Union is
   Alpine Troops (64 of Turkey's 101 units). The stock function must not
   change (rule 2); deep needs its own choice.

## What to build, in this order; stop when the epic budget is spent

Each item is a small expected-value formula over values that already exist:
`ai_city.worth` (city worth, gold-equivalent), unit build cost, the combat
model (`unit_win_chance`, `deep_take_odds_cost`), the wave/survival model.
No new constants: where a number is needed it is a ratio of existing prices,
or it goes into `struct tuning` as a PLACEHOLDER with the registry line and
a STATUS flag (rule 7). Every new behaviour is behind a tuning gate that
defaults OFF in the compiled defaults so the current pins stay comparable;
the acceptance protocol below reads the gate ON.

1. **Direct capture pass** (`deep_capture_pass`, new, deepdefence.c or a
   new deepcapture.c): for every enemy city within reach of one of our
   military land units, including barbarian cities, with no defender or
   with defenders our unit beats: EV = P(win) × worth(city) − P(loss) ×
   cost(unit). Attack when EV > 0. Candidates include units tasked
   AIUNIT_DEFEND_HOME when the target is a city we lost within
   `capture_retake_turns` turns (placeholder) or is barbarian-held; a
   just-lost city of ours gets its worth as we last knew it. Runs BEFORE
   the operation layer and BEFORE the pillage pass. Gate: `capture_gate`.
   Log line `deep-capture turn= player= unit= target= p= worth= ev= did=`.
2. **Pillage scope**: skip tiles inside the radius of a city we lost within
   `capture_retake_turns`, and tiles whose owner is a barbarian holding one
   of our former cities. Gate: fold into `capture_gate` or its own
   `pillage_scope_gate` (your call; say which in the commit).
3. **EV operation formation**: beside the probability bar, an EV test:
   form when P(take) × worth(target) − expected losses (the model already
   prices losses: `loss=` in deep-kill) ≥ 0, with the bar kept as a floor
   only for targets that cannot be re-tried (overseas). Gate: `op_ev_gate`.
   Keep `op_take_target` semantics unchanged when the gate is off.
4. **Opportunity in the stance read**: add a relative-strength term to
   `deep_stance_seed_from_position`: the ratio of our military strength to
   the weakest adjacent rival's (strength = Σ attack × hp over units, read
   as the existing code reads threat), mapped to [−1, +1], added to m_idx
   with gain `stance_opportunity_gain` (placeholder, default 0.0 = off).
   Document the direction in the comment the way the existing terms are.
5. **Defender choice per shield**: a deep chooser that scores
   `dai_unit_defense_desirability(...) / utype_build_shield_cost` and picks
   the max, used wherever deep asks stock for a defender type for ITS OWN
   cities (find the call sites; `grep -n dai_choose_defender\|defense_desirability ai/deep`).
   Gate: `defender_per_shield_gate`. If the only way is to copy a stock
   function, say so in the commit message and why (rule 3).

If you reach item 5 within budget, stop there regardless. Do not start
anything else.

## Rules that apply here

- NEVER touch `ai/baseline/` or `ai/default/` (visibility-only exception,
  demonstrate neutrality if used). The crash in `dai_unit_attack` is
  upstream-bugs #52 and is offered upstream from another branch; do not fix
  it in the tree. If your capture pass calls `dai_unit_attack`, re-fetch the
  attacker and its bodyguard by id after the call in YOUR code.
- Work in `.worktrees/perf` (detached HEAD now): `git checkout -b
  epic/pricing-1 epic/w03-horizon-content` there. Its `build/` is
  configured debugoptimized. Pin your server for every game: copy
  `build/freeciv-server` AND `build/libfreeciv.so` into
  `.worktrees/perf/.pricing-bin/` and pass that path to every run; never run
  games from `build/`. Port block **8000–8069**. Every game at
  `nice -n 10` AND in the idle scheduling class: launch drivers as
  `chrt -i 0 nice -n 10 bash ./benchmark/...`. The user may be playing or
  working on the same laptop (2 P + 8 E cores); keep to ≤ 6 jobs.
- Launch batch drivers with the Bash tool's `run_in_background: true`
  from the worktree root and watch DONE markers; `setsid ... &` dies here.
  Never edit `benchmark/run-game.sh` or a driver while games run.
- One commit does one thing, ≤ ~400 lines, title `ai/deep: <verb> <object>`.
  Each gate lands with its registry line in `benchmark/tuning-params.ini`
  and its `struct tuning` field documented like its neighbours.

## Acceptance (paste numbers in commit bodies and STATUS)

1. `meson test -C build` passes; add L0 tests for the formulas: a retake of
   a city held by one wounded barbarian beats pillaging the road beside it;
   an empty enemy city adjacent to a Musketeers has EV > 0; a Rifleman beats
   Alpine Troops per shield; the EV op test forms a cheap sure thing that
   the 0.70 bar would also form and refuses an expensive coin flip.
2. **Byte identity with all new gates OFF**: 4 contested seeds (1–4) and
   2 europe-late seeds (121, 122, self-play deep, `START=` the starts under
   `benchmark/results/scoreboard/run-ed04f89a54e5/europe-starts/`) give
   scorelogs identical (minus the `id` line) to the same games on the
   current tuning branch build. This is the convention that keeps the
   scoreboard controls valid; it is not optional.
3. **Directional read with the gates ON** (a tuning file
   `benchmark/tuning/pricing-1.ini` that sets every new gate to 1 and
   `stance_opportunity_gain` to its placeholder): europe-late seeds 121–125,
   deep self-play and mixed_db, via `benchmark/run-game.sh --config
   benchmark/configs/europe-late.conf` with `EUROPE_STARTS` pointing at the
   starts above and `TUNING=` the file. Compare with the defaults games of
   the same seeds already on disk
   (`benchmark/results/scoreboard/run-ed04f89a54e5/defaults/europe-late/...`)
   using `benchmark/europemeter.py` (`--partner` not needed) and
   `benchmark/rankcmp.py` on the two roots. Report: captures made
   (`deep-capture did=1` count), barbarian cities held at T250, ops formed
   vs refused, self-play score and gold, the meter's land/road/city deltas.
   n = 5 is directional only; say so. The full venue is the user's call.
4. Unit-level evidence from the saved positions the defects came from:
   `play-1104-2100/saves/freeciv-T0120-Y00380-auto.sav.zst` (Marseille
   barbarian-held and EMPTY two tiles from French units) and
   `freeciv-T0220-Y01595-auto.sav.zst` (26 empty barbarian cities). Load
   each with your pin and `--file`, gates on, play 5 turns at `set timeout
   -1`, and count `deep-capture did=1` and cities changing hands
   (`deep-city got=`). Use `play-europe-resume.sh`'s HEADLESS mode as the
   template for the command script (timeout, minplayers 0, endturn,
   start); those saves are classic ruleset and load their own rulesetdir.

## Reporting

Update `doc/deep-ai/STATUS.md` with an S-PRICING-1 entry: what landed, the
identity result, the directional numbers, what was left undone and why.
Append anything you find wrong in stock to
`doc/deep-ai/findings/upstream-bugs.md` (do not fix it). Commit on
`epic/pricing-1`, push nothing, and STOP. The orchestrator merges.
