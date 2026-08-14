#!/usr/bin/env bash
# derby5/launch-derby5.sh -- launch the fifth Europe personality derby,
# this time on the REAL Europe scenario map.
#
# WHY THIS SCRIPT EXISTS (the derby-4 defect it fixes)
# ---------------------------------------------------
# derby4/launch-derby4.sh never pointed the server at a scenario
# savegame.  europe.serv only sets Europe-*flavoured* options and
# /create's the 14 seats; with no map loaded the server generated a
# fresh FRACTAL map, and the tell is in derby4/saves-0812-stalled/game.log:
#
#     Setting 'startpos' has been adjusted from "Generator's choice"
#     to "All players on a single continent".
#
# That line can only be emitted by the map generator (server/srv_main.c
# ~3440, reached from server/generator/mapgen.c).  On a loaded scenario
# the map already carries its start positions (map_startpos_count() > 0,
# mapgen.c ~1412) so the whole random-startpos branch is skipped and the
# line never appears.  ITS ABSENCE IS THE ACCEPTANCE TEST.
#
# THE LOAD MECHANISM (recovered from the derby-1/2 shell history)
# ---------------------------------------------------------------
# The working invocation was, verbatim:
#
#     FREECIV_DEEP_TUNING=deep-best.ini ./build/freeciv-server \
#         --ruleset classic --file data/scenarios/europe.sav \
#         --port 5556 --read europe.serv
#
# i.e. the scenario is loaded with the command-line --file option, NOT
# from inside europe.serv.  Ordering is what makes this work and it is
# guaranteed by the server itself:
#
#   * srv_prepare()  (server/srv_main.c:3207) calls load_command() for
#     srvarg.load_filename -- the map, its rulesetdir and its [settings]
#     block land FIRST, while the server is still in S_S_INITIAL.
#   * srv_main()     (server/srv_main.c:3643) only then reads
#     srvarg.script_filename, so every /create and every /set in our
#     command script runs AFTER the load and therefore wins over the
#     scenario's own stored settings.
#
# That ordering is the entire trick: settings applied before a load are
# thrown away by load_command()'s server_game_free()/server_game_init(),
# settings applied after it survive.
#
# The other half of the derby-1/2 lesson was the path.  load_command()
# searches get_save_dirs() then get_scenario_dirs() (stdinhand.c ~3806),
# so a bare "europe.sav" finds the SYSTEM install's
# /usr/share/freeciv/scenarios/europe.sav.gz, not this repository's copy
# -- the "trying to load the globally installed europe savegame" failure
# the humans hit.  This script passes an ABSOLUTE path into the repo and
# additionally pins FREECIV_SCENARIO_PATH, so there is no way to pick up
# the system file.
#
# The scenario: data/scenarios/europe.sav, "Europe (classic/giant)",
# overhead 177x100, rulesetdir="classic", generator=SCENARIO,
# startpos_count=37, players=FALSE and startpos_nations=FALSE -- which is
# exactly why the 14 seats can still be /create'd with our own names and
# nations after the load.
#
# PERSONALITY MECHANISM (unchanged from derby-4; see derby4/PIN.txt and
# derby4/tuning-derby4-personality.ini for the full trail):
#
#   There is no per-player tuning file in ai/deep.  FREECIV_DEEP_TUNING
#   loads ONE file for the whole server process (ai/deep/deeptuning.c:
#   deep_tuning_init).  Per-seat differentiation comes from
#   ai/deep/deepstance.c: with stance_source=1 each deep player's
#   econ/mil stance axes are seeded from THAT PLAYER'S OWN nation traits,
#   and traits only vary per player when the server actually draws them
#   -- hence `set traitdistribution EVEN` below.
#
# Usage:
#   ./derby5/launch-derby5.sh [--server PATH] [--endturn N] [--port N]
#                             [--savedir PATH] [--scenario PATH]
#
# Safe to run from anywhere -- all paths resolve relative to this
# script's location, not the caller's cwd.
#
# Env overrides (all optional): ENDTURN, SAVETURNS, PORT, SERVER, SEED,
# SAVEDIR, SCENARIO.

set -euo pipefail

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$HERE/.." && pwd)

# derby-4's pinned binary, reused unchanged so the two derbies differ in
# the map and nothing else.  Its RUNPATH is $ORIGIN, so it must be
# executed from where its matched libfreeciv.so lives -- derby4/ -- which
# is why this points into derby4/ rather than copying anything out.
SERVER="${SERVER:-$REPO/derby4/freeciv-server-174fce8cd2}"
PORT="${PORT:-5556}"
# T300: matches derby-3/derby-4.
ENDTURN="${ENDTURN:-300}"
SAVETURNS="${SAVETURNS:-10}"
SEED="${SEED:-}"
SCENARIO="${SCENARIO:-$REPO/data/scenarios/europe.sav}"
SAVEDIR_ARG="${SAVEDIR:-}"

while [ $# -gt 0 ]; do
  case "$1" in
    --server)   SERVER=$2; shift 2;;
    --endturn)  ENDTURN=$2; shift 2;;
    --port)     PORT=$2; shift 2;;
    --savedir)  SAVEDIR_ARG=$2; shift 2;;
    --scenario) SCENARIO=$2; shift 2;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) echo "unknown arg: $1" >&2; exit 1;;
  esac
done

SERVER=$(readlink -f "$SERVER")
[ -x "$SERVER" ] || { echo "server not found or not executable: $SERVER" >&2; exit 1; }

# derby-4's tuning file, reused verbatim (pv2 base, stance_source=1).
# DERBY-7+ (human call 2026-08-13): add diplo_deep_only_pacts = 1 to the deep
# tuning file — the fence REJECTED on benchmark mixed (it cancels the peace
# dividend, task #63) but the derby is team-vs-team where blocking cross-team
# peace is the intent. Pair it with the lean floor zeros (derby-6 winner).
# Also add founder_berth_route = 1: NULL on open sea (t +0.21, 3 firings in
# 16 games) but its stranded-founder population is closed-water geometry —
# Europe's lakes and inlets are exactly where slice 2 counted 60/67 gaps.
# ALL of the above (fence, lean zeros, berth, plus the DECAY schedule at
# its measured anchors) are minted in derby5/tuning-derby7.ini — run
# derby-7 with TUNING=$REPO/derby5/tuning-derby7.ini once DECAY slice 2's
# knobs have merged (the loader must know the horizon_decay_* names).
TUNING="${TUNING:-$REPO/derby4/tuning-derby4-personality.ini}"
[ -f "$TUNING" ] || { echo "tuning file not found: $TUNING" >&2; exit 1; }

EUROPE_SERV="$REPO/europe.serv"
[ -f "$EUROPE_SERV" ] || { echo "europe.serv not found: $EUROPE_SERV" >&2; exit 1; }

# Absolute, inside this repository.  A bare name would resolve against
# the system scenario dir -- the derby-1 failure.
SCENARIO=$(readlink -f "$SCENARIO")
[ -f "$SCENARIO" ] || { echo "scenario not found: $SCENARIO" >&2; exit 1; }
case "$SCENARIO" in
  "$REPO"/*) ;;
  *) echo "refusing scenario outside this repo: $SCENARIO" >&2; exit 1;;
esac
grep -q '^is_scenario=TRUE' "$SCENARIO" \
  || { echo "not a scenario savegame: $SCENARIO" >&2; exit 1; }

if [ -n "$SAVEDIR_ARG" ]; then
  SAVEDIR="$SAVEDIR_ARG"
else
  SAVEDIR="$HERE/saves-$(date +%m%d)"
fi
mkdir -p "$SAVEDIR/saves"
SAVEDIR=$(readlink -f "$SAVEDIR")

COMMANDS="$SAVEDIR/commands.serv"
{
  echo "; --- derby5: everything here runs AFTER the --file scenario load ---"
  echo "; (srv_prepare loads the map; srv_main reads this script afterwards,"
  echo ";  so these settings override the scenario's stored [settings].)"
  echo
  cat "$EUROPE_SERV"
  echo
  echo "; --- appended by launch-derby5.sh ---"
  # europe.serv sets "minplayers 1", but the start command's minplayers
  # check counts HUMAN players only (server/stdinhand.c ~6165). This is a
  # fully headless, 0-human launch, so that check would refuse to start
  # ("Not enough human players") unless overridden.
  echo "set minplayers 0"
  # MANDATORY headless: with a positive timeout the server waits on
  # absent human players and the game never advances past T1.
  echo "set timeout -1"
  # The personality mechanism's other half: traits must actually vary per
  # player for stance_source=1 to do anything.
  echo "set traitdistribution EVEN"
  echo "set endturn $ENDTURN"
  echo "set autosaves \"TURN|GAMEOVER\""
  echo "set saveturns $SAVETURNS"
  echo "set scorelog enabled"
  echo "set scoreloglevel ALL"
  echo "set scorefile scorelog.txt"
  # The scenario carries startpos=DEFAULT and 37 stored start positions;
  # with map_startpos_count() > 0 the generator never reassigns them.
  # Stated explicitly so the log records which mode the seats got.
  echo "set startpos DEFAULT"
  if [ -n "$SEED" ]; then
    echo "set gameseed $SEED"
    echo "set mapseed $SEED"
  fi
  # Verification block: these lines put the values that actually took
  # into game.log, after every set above and before the game starts.
  echo "show generator"
  echo "show startpos"
  echo "show timeout"
  echo "show endturn"
  echo "show saveturns"
  echo "show scorelog"
  echo "show traitdistribution"
  echo "show minplayers"
  echo "list players"
  echo "start"
} > "$COMMANDS"

# Uninstalled server: point it back at the source data dir for rulesets
# (same fix as benchmark/run-game.sh), and pin the scenario dir to this
# repository so nothing can fall through to /usr/share/freeciv.
export FREECIV_DATA_PATH="$REPO/data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
export FREECIV_SCENARIO_PATH="$REPO/data/scenarios"
export FREECIV_DEEP_TUNING="$TUNING"

echo "derby-5 launching:"
echo "  server:    $SERVER"
echo "  scenario:  $SCENARIO"
echo "  savedir:   $SAVEDIR"
echo "  scorelog:  $SAVEDIR/scorelog.txt"
echo "  game.log:  $SAVEDIR/game.log"
echo "  commands:  $COMMANDS"
echo "  tuning:    $TUNING (pv2 base, stance_source=1)"
echo "  endturn:   $ENDTURN   saveturns: $SAVETURNS   port: $PORT"
[ -n "$SEED" ] && echo "  seed:      $SEED"

cd "$SAVEDIR"
exec "$SERVER" -A none -e -p "$PORT" \
  --ruleset classic --file "$SCENARIO" \
  --read "$COMMANDS" --saves saves > game.log 2>&1
