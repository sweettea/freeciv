# The standing scoreboard: design (2026-10-28)

**Status: designed, budgeted, not launched.** `benchmark/scoreboard.sh`
is the driver and `benchmark/rankcmp.py` the reader. Nothing here has
bought a game; every number below is either a dry-run count or a
re-read of games already on disk.

## Why

Every "where does deep stand" number so far has two defects. It is
**summed self-play score**, which measures societal output and cannot
see a zero-sum transfer (theory-of-play §H). And it was taken on
**holdout seeds 101–120**, which the s4/s6 holdouts, the panel and the
stacked holdout read have since used as a development set. The
scoreboard is one fixed measurement, re-run on every merge, on seeds
nobody has looked at, and it reports both axes side by side.

## What it runs

Seeds **121–160** (40) for every core cell. Binary pinned by a required
argument. Venues `contested.conf` (8p, T150) and `lategame-80.conf`
(6p, T80), the two venues every holdout report since 2026-08 used.

| cell | contents | games per venue |
|---|---|---:|
| self-play deep | compiled defaults, no tuning file | 40 |
| self-play baseline | tuning-independent, cached per pin (below) | 40 or 0 |
| mixed | deep and baseline alternate seats in one game (4/4, 3/3); each seed played twice with the seats mirrored (`mixed_db`, `mixed_bd`) | 80 |
| `--with-vector F` | self-play deep and mixed again, deep seats on F | 120 |
| `--classic` | `contested-classic.conf`, self-play both arms, seeds 121–140 | 40 (+20 with a vector) |

`contested-classic.conf` already existed (8p, 32×32, T150,
`RULESET=classic`), so the brief's conditional file was not created.

**Seat map.** It is reused, not reinvented: `run-game.sh --arm
mixed_db|mixed_bd` (odd seats to the first letter, even seats to the
second) is the code `tune.py --mixed` drives, and the layout
`.../mixed_db/seed-N` is the one `screen-2026-08` holds. The *driver*
is not `tune.py`. Reusing it for one fixed vector is awkward in five
ways. It takes the first N seeds of a file, and `--report-holdout`
hard-codes `holdout.txt`, so 121–160 cannot be reached. It gives each
game its own port (80 per cell). It leaves full `game.log`s (hundreds
of MB each for a deep contested game), and `/home` had **6.0 G free**
on 2026-10-28. It does not delete a partial game directory before
retrying, so a stale `scorelog.txt` can survive. And it has no guards.
The driver follows `stacked-lever.sh` instead: a DONE-keyed resume,
`flock`, partial directories deleted, logs pruned and zstd'd as each
game finishes, nice 10, and one port per job slot in 7600–7639.

**The pin is two files.** The server's RUNPATH is `$ORIGIN/`, so the
`libfreeciv.so` beside it is the code that plays. The scoreboard's
identity is the sha256 of both files. The script refuses any path
under `build/` and refuses a library that `ldd` resolves anywhere
else. `run-game.sh`'s `provenance.txt` hashes only the server, which
is a gap (left unfixed here because this brief was read-only).

**Guards**, checked before each log is pruned. A failure writes
`STOP` and halts launches. `commands.serv` must seat exactly the arm's
modules. A deep-bearing game must log `deep-` lines and a baseline
game must log none (the archived s4 baseline logs have zero). A
defaults game must log no `deep-tuning` line. A vector game must log
one that names the vector's absolute path, and no `cannot load tuning
file`. A resumed run must have the same pin and the same vector sha
it started with.

**Baseline cache.** Baseline self-play is tuning-independent but *not*
binary-independent: `server/`, `common/` and `advisors/` (W28/W29)
reach it. So the cache is certified per pin. A new pin replays seeds
121–123 and byte-compares the scorelogs with the cache, leaving out
the `id` line (the stacked identity proof). If all three are
identical, the pin is certified and the other 37 are hardlinked in.
Any difference means all 40 are played fresh and become a new cache
generation. The whole flow was exercised end to end against a mock
`run-game.sh` in the scratchpad: origin, certify, a failed probe
starting a new generation, a guard `STOP`, resume, a pin mismatch,
and zero port overlaps in 640 mock games.

## Budget (from `--dry-run`, which buys nothing)

At 192 s/game (the corpus median; measured cross-check: deep 172 s
contested / 217 s lategame-80, mixed contested 185 s, baseline 160 s /
200 s) and `--jobs 6`:

| invocation | games | hours |
|---|---:|---:|
| core, first run (no cache) | 320 | 2.8 |
| core, baseline cache certified for the pin | 240 | 2.1 |
| core, new pin, probe passes / fails | 246 / 320 | 2.2 / 2.8 |
| `--classic` | 360 | 3.2 |
| `--with-vector F` | 560 | 5.0 |
| `--classic --with-vector F` | 620 | 5.5 |

Expected per-seat SEs at n = 40. Self-play score is about 1.85
(contested) and 1.75 (lategame-80): panel §1 and §4 measured se 2.61
and 2.48 at n = 20, scaled by √½. Mixed per-seat score on contested is
about 2.06: screen-corpus Part 2 measured σ 104.4 per seed over 8 + 8
seats. Mixed lategame-80 and the AUC have never been measured, and the
first run measures them.

## What each number means, and what it does not

- **Self-play Δ and ratio (societal).** The 00-context §6 primary
  metric. It is the output of a world of four deep seats against a
  world of four baseline seats on the same map. It is blind to a city
  taken from a rival, because that transfer nets to zero in the sum.
- **AUC = P(a random deep seat outranks a random baseline seat in the
  same game)** (competitive). It is the Mann–Whitney U over a game's
  deep×baseline seat pairs, with ties counting ½ and eliminated seats
  ranked last. Each seed's two mirrors are averaged into one datum,
  and the SE is taken across seeds, since seats in one game are not
  independent. 0.5 is parity. This is the §H rank statement made
  scale-free. A city taken from a baseline seat raises AUC twice, once
  for each side, while summed score cannot see it ("the victim's −14
  is as good as your +14"). The Derby's "+70 at T50 … −337 at T300" is
  the same kind of statement in score units. With n seats a side and
  no eliminations, *deep mean rank − baseline mean rank = n(1 − 2·AUC)*
  exactly (the selftest checks this). Normalized rank is therefore
  AUC in other units, and it is printed so that the day the two part
  (through eliminations or ties) is visible.
- **Win share.** One bit per game, so it is noisy. Read it only beside
  the AUC.
- **Per-seat mixed score.** It shares one game's shocks, so it is not
  societal output. It is tune.py's mixed objective divided by the seat
  count.
- **AUC by seating** (odd seats vs even seats). If the two disagree
  strongly, seat geometry rather than the module is carrying the
  number.
- **Panel** (`panel.py BASELINE DEEP`, so its deltas are deep −
  baseline). It is descriptive and adjudicates nothing. Its cuts are
  T50/T100, and T50/T80 on lategame-80, which ends at T80.

What none of these mean:

- None of them is a test against another opponent. The only opponent
  is `ai/baseline`, which 00-context §6 warns about; `mixed_dc` (deep
  vs `ai/default`) is the cheap next opponent and is not in the
  scoreboard.
- None of them may select anything. `vector − defaults` is printed
  paired by seed, but choosing a vector because of it is adjudicating
  on holdout. Training seeds adjudicate (CLAUDE.md rule 7).

**Reader check on archived games** (illustration only: different
vectors, different seeds). Self-play contested, s6 gates-off deep vs
s4 baseline, seeds 101–120: Δ **−7.29 ± 2.65** per seat (ratio 0.920),
paired by seed. Mixed contested, `screen-2026-08` s1-fit generation-0
incumbent, 52 training seeds: **AUC 0.492 ± 0.018**, per-seat −1.06 ±
2.12, by seating 0.521 / 0.464. The two axes can read differently,
and that is the reason both are on the board.

## Why 121–160

101–120 are burned. `train.txt` (1–100) adjudicates and never
reports. 40 seeds bring the paired self-play SE under 2 points per
seat. Taking 40 rather than all 80 remaining holdout seeds keeps
161–200 virgin for the day 121–160 burn too. **Burn rule:** these
seeds are burned for any knob whose value was chosen after someone
read a scoreboard number. When that happens, move to 161–200 and say
so in STATUS.

## Launch (the user's decision)

```sh
mkdir -p .scoreboard-bin benchmark/results/scoreboard
cp build/freeciv-server build/libfreeciv.so .scoreboard-bin/
benchmark/scoreboard.sh .scoreboard-bin/freeciv-server --dry-run   # read the budget
setsid bash benchmark/scoreboard.sh .scoreboard-bin/freeciv-server --yes \
  > benchmark/results/scoreboard/driver-$(date +%m%d-%H%M).log 2>&1 < /dev/null &
```

Interactive launch: drop `--yes` and type `yes` at the prompt. The
output goes to `benchmark/results/scoreboard/run-<pin12>/SCOREBOARD.md`
(regenerate it with `--report`). Add `--with-vector
benchmark/tuning/stacked-on.ini` and/or `--classic` as wanted. The
same pin plus the same `--out` resumes. Do not rebuild
`.scoreboard-bin` while a run is live.
