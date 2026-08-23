# Session brief — grow the frozen world family

Mechanical half of the bluewater venue plan (S-BLUEWATER, play-forward
-then-equalize): grow evaluation worlds ONCE with all-classic seats,
freeze them at the target era. NO equalizer, NO census, NO ai/ changes,
NO new knobs — those belong to the bluewater epic, which consumes your
output. Scripts + game runs + a selection table only.

## Deliverables

1. **`benchmark/grow-worlds.sh`** — a driver that, for a given venue
   flavor and mapseed list, runs headless all-classic games (12 seats,
   aifill, hard, `set timeout -1`) with autosaves every 5 turns to a
   generous endturn (250), one dir per (flavor, mapseed) under
   `benchmark/results/grow-worlds/`. Two flavors:
   - **naval**: archipelago-style map, ~15% land (use the server's
     generator/landmass settings; verify the actual land fraction by
     counting terrain lines in a save and PASTE the measured fraction —
     do not trust the setting name).
   - **air**: ~50% land, `set barbarians DISABLED`, otherwise default
     continents.
   All game processes run under `nice -n 10` (hard rule — the user is
   playing interactively on this box).
2. **Snapshot selection** — a small committed reader
   (`benchmark/pick-snapshot.py`) that scans a grow dir's autosaves
   and reports, per save: turn, seats alive, and per-seat known-tech
   count plus whether each seat knows the flavor's KEY TECH:
   - naval key tech: Industrialization (Transports exist)
   - air key tech: Flight
   Selection rule: the EARLIEST autosave where >=6 of 12 seats know
   the key tech and ALL 12 are alive. If a mapseed never satisfies it
   by endturn (seats died or tech stalled), DISCARD that mapseed and
   grow a replacement — do not hand-pick a compromised world.
3. **The family**: 6 accepted snapshots per flavor (grow 8 mapseeds
   per flavor to have discards), copied to
   `benchmark/worlds/<flavor>-w<NN>-T<turn>.sav.zst` and committed
   (saves are small; if a save exceeds ~5 MB, note it and commit
   anyway — they are the venue).
4. **STATUS entry**: the selection table (per accepted world: mapseed,
   snapshot turn, land fraction, seats alive, key-tech count, wall
   minutes to grow), plus the discard list with reasons. Paste real
   numbers.

## Method notes

- Server: pin /home/sweettea/freeciv/build binaries FIRST into your
  own dir (cp freeciv-server AND libfreeciv.so, sha256-compare) and
  run everything from that pin. All-classic games never load deep
  tuning, but pin anyway — the build dir must stay rebuildable by
  others.
- Ruleset classic. Check exact server setting names with the server's
  own help (e.g. `help landmass`) rather than guessing; the first
  smoke game validates the settings file before the batch.
- Smoke first: ONE game per flavor to endturn, verify autosaves appear
  and the reader parses them, THEN the batch.
- Batch driver launched detached from the worktree root as
  `setsid bash ./benchmark/grow-worlds.sh go` (exact shape). DONE
  markers per (flavor, mapseed) recording SUCCESS vs FAIL. Ports
  7250-7299, jobs <=2.
- Games are deterministic; a re-grow of the same (flavor, mapseed,
  settings) reproduces the same world.

## Guards

Own worktree /home/sweettea/freeciv/.worktrees/growworlds (branch
epic/grow-worlds). This epic needs NO ai module build changes — build
with the standard flags (-Daimodules=classic,tex,baseline,deep
-Dtools= -Dclients= -Dfcmp=) only if the tree lacks a build; prefer
pinning from the main build dir as above. One commit one thing
(driver, reader, worlds, STATUS); commit messages end with exactly:
Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01BJGMPMJdZfqAZGstrvmA8s
Never `git add -A`; add files by name. Do NOT merge; stop after the
STATUS commit on your branch and report: branch tip sha, the selection
table, and anything that surprised you.
