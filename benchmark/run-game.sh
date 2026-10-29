#!/usr/bin/env bash
# Run one headless, deterministic Freeciv game and leave its artefacts
# in an output directory.
#
#   run-game.sh --config FILE --seed N --arm AITYPE --out DIR
#               [--server BIN] [--port N]
#
# Environment: TUNING names one tuning ini for every deep seat; ARENA
# names a manifest that additionally gives individual seats their own
# inis (benchmark/arena.py writes it); DEEP_QUIET demotes deep's metric
# lines; KEEP_SAVES keeps every Nth autosave.
#
# Parallel invocations must use distinct --port values: every server
# binds a TCP port and simultaneous binds to the same one fail.
#
# Determinism: mapseed and gameseed are both set to --seed, so two runs
# with the same config, seed and arm are identical, and two arms with the
# same seed play on the same map (verified via maphash.txt, a hash of the
# terrain rows of the turn-1 autosave; an explicit `save` in the command
# script would run in pregame, before the map exists).
#
# Artefacts in DIR:
#   game.log       server output
#   scorelog.txt   SCORELOG2 per-turn statistics for every player
#   maphash.txt    sha256 of the turn-1 autosave's terrain rows
#   commands.serv  the exact server commands used
#   provenance.txt sha256 of the server binary, its libfreeciv.so, config, tuning and this
#                  runner -- what any later pairing of two result dirs
#                  has to agree on to be a single-variable comparison
#   DONE           written last; presence marks a completed run
#   saves/         turn-1 + final savegames (intermediates pruned)

set -uo pipefail

usage() { grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

CONFIG= SEED= ARM= OUT= SERVER= PORT=5555
while [ $# -gt 0 ]; do
  case "$1" in
    --config) CONFIG=$2; shift 2;;
    --seed)   SEED=$2; shift 2;;
    --arm)    ARM=$2; shift 2;;
    --out)    OUT=$2; shift 2;;
    --server) SERVER=$2; shift 2;;
    --port)   PORT=$2; shift 2;;
    *) usage;;
  esac
done
[ -n "$CONFIG" ] && [ -n "$SEED" ] && [ -n "$ARM" ] && [ -n "$OUT" ] || usage

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${SERVER:=$here/../build/freeciv-server}"
SERVER=$(readlink -f "$SERVER")
[ -x "$SERVER" ] || { echo "server not found: $SERVER" >&2; exit 1; }

# Config values (see configs/*.conf): PLAYERS, ENDTURN required;
# XSIZE/YSIZE required unless TILESPERPLAYER sizes the map instead;
# SKILL, RULESET, TIMEOUT_S, HUTS, GENERATOR, SPECIALS,
# EXTRA_SERV (verbatim server commands) optional.
SKILL=hard RULESET= TIMEOUT_S=1800 HUTS= GENERATOR= SPECIALS= TILESPERPLAYER= EXTRA_SERV=
. "$(readlink -f "$CONFIG")"
for v in PLAYERS ENDTURN; do
  [ -n "${!v:-}" ] || { echo "config missing $v" >&2; exit 1; }
done
if [ -z "$TILESPERPLAYER" ]; then
  for v in XSIZE YSIZE; do
    [ -n "${!v:-}" ] || { echo "config missing $v" >&2; exit 1; }
  done
fi

mkdir -p "$OUT/saves"
OUT=$(readlink -f "$OUT")

{
  [ -n "$RULESET" ] && echo "rulesetdir $RULESET"
  echo "set minplayers 0"
  echo "set timeout -1"
  if [ -n "$TILESPERPLAYER" ]; then
    # Scale the map with the player count, like the ruleset default,
    # but at the configured land-per-player density.
    echo "set mapsize PLAYER"
    echo "set tilesperplayer $TILESPERPLAYER"
  else
    echo "set mapsize XYSIZE"
    echo "set xsize $XSIZE"
    echo "set ysize $YSIZE"
  fi
  [ -n "$HUTS" ] && echo "set huts $HUTS"
  [ -n "$GENERATOR" ] && echo "set generator $GENERATOR"
  [ -n "$SPECIALS" ] && echo "set specials $SPECIALS"
  [ -n "$EXTRA_SERV" ] && printf '%s\n' "$EXTRA_SERV"
  echo "set aifill $PLAYERS"
  # Ordinary arms put every player on that AI module. The mixed arms
  # seat two modules in the SAME game (head-to-head): mixed_XY puts
  # module X on odd seats and Y on even, where each letter is the first
  # letter of an aitype -- d=deep, b=baseline, c=classic (ai/default).
  # mixed_db and its mirror mixed_bd are the long-standing pair; the
  # form generalises so deep can also be seated against ai/default
  # directly. ALWAYS run a mirrored pair (mixed_XY and mixed_YX) per
  # seed, so every seat sees each module once and seat bias cancels --
  # which matters most at PLAYERS=2, where one seat is one island.
  mixed_mod() { # letter -> aitype
    case "$1" in
      d) echo deep;; b) echo baseline;; c) echo classic;;
      *) echo "unknown mixed arm letter: $1" >&2; exit 1;;
    esac
  }
  for i in $(seq 1 "$PLAYERS"); do
    case "$ARM" in
      mixed_??)
        letters=${ARM#mixed_}
        if [ $((i % 2)) -eq 1 ]; then
          mod=$(mixed_mod "${letters%?}")
        else
          mod=$(mixed_mod "${letters#?}")
        fi;;
      *) mod=$ARM;;
    esac
    echo "create P$i $mod"
  done
  echo "$SKILL"
  echo "set endturn $ENDTURN"
  echo "set autosaves \"TURN|GAMEOVER\""
  echo "set saveturns 1"
  echo "set scorelog enabled"
  echo "set scoreloglevel ALL"
  echo "set scorefile scorelog.txt"
  echo "set gameseed $SEED"
  echo "set mapseed $SEED"
  echo "start"
} > "$OUT/commands.serv"

# What produced this game, beside the game. run.sh writes the same
# components into key.txt for the dirs IT names, but a driver that
# names its own arm directories (a night batch, an ad-hoc probe) writes
# no key at all, and then two result dirs can be paired months later
# with nothing on disk saying whether they shared a binary.
#
# That is not hypothetical: the G2 wait-discount verdict of 2026-08-12
# paired an arm built at 17:21 against a control run the previous
# night, 32 ai/deep commits earlier, and the confound was invisible
# until the games were re-read tile by tile (STATUS, W36-DIM). One
# sha256 per game costs nothing next to a game and makes the question
# answerable from the artefacts.
{
  echo "server=$SERVER sha256=$(sha256sum "$SERVER" | cut -d' ' -f1)"
  # The server loads the libfreeciv.so beside it, so that library is the
  # code that plays; a pin that copies only the binary is not a pin
  # (the 2026-08-30 screen campaign lost 3660 games to exactly that).
  solib="$(dirname "$(readlink -f "$SERVER")")/libfreeciv.so"
  if [ -f "$solib" ]; then
    echo "libfreeciv=$solib sha256=$(sha256sum "$solib" | cut -d' ' -f1)"
  else
    echo "libfreeciv=none sha256=none"
  fi
  echo "config=$(readlink -f "$CONFIG") sha256=$(sha256sum "$(readlink -f "$CONFIG")" | cut -d' ' -f1)"
  if [ -n "${TUNING:-}" ]; then
    echo "tuning=$(readlink -f "$TUNING")" \
         "sha256=$(sha256sum "$(readlink -f "$TUNING")" | cut -d' ' -f1)"
  else
    echo "tuning=none sha256=none"
  fi
  if [ -n "${ARENA:-}" ]; then
    echo "arena=$(readlink -f "$ARENA")" \
         "sha256=$(sha256sum "$(readlink -f "$ARENA")" | cut -d' ' -f1)"
    # The manifest's own hash does not cover the candidate files it
    # names, and those are the values the game actually played.
    sed -n 's/^[A-Za-z0-9_]* *= *"\{0,1\}\([^"]*\)"\{0,1\} *$/\1/p' \
        "$(readlink -f "$ARENA")" | sort -u | while read -r cand; do
      [ -f "$cand" ] || continue
      echo "arena-file=$cand sha256=$(sha256sum "$cand" | cut -d' ' -f1)"
    done
  else
    echo "arena=none sha256=none"
  fi
  echo "runner=$(readlink -f "$0") sha256=$(sha256sum "$0" | cut -d' ' -f1)"
  echo "arm=$ARM seed=$SEED"
} > "$OUT/provenance.txt"

# scorefile is written relative to the server's cwd; run from OUT so
# parallel games never share files. The uninstalled server finds its
# rulesets relative to cwd, so point it back at the source data dir.
export FREECIV_DATA_PATH="$here/../data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
# Tuning overrides for ai/deep: TUNING (also hashed into run.sh's
# result-cache key) reaches the server as an absolute path.
if [ -n "${DEEP_QUIET:-}" ]; then
  export FREECIV_DEEP_QUIET=1
fi
if [ -n "${TUNING:-}" ]; then
  export FREECIV_DEEP_TUNING=$(readlink -f "$TUNING")
fi
# ARENA names a manifest that gives individual deep seats their own
# tuning files -- several candidates judged inside ONE game (the arena
# tier, findings/tuning-venues-2026-08-06.md section 6a). The two
# compose at SEAT level, not at key level: a seat the manifest names
# runs its own file loaded from the compiled defaults, exactly the way
# TUNING's file is loaded, and every seat the manifest does NOT name
# runs TUNING's. A candidate ini is therefore a whole tuning file, like
# every archived one. benchmark/arena.py writes the manifest.
if [ -n "${ARENA:-}" ]; then
  export FREECIV_DEEP_ARENA=$(readlink -f "$ARENA")
fi
if ! (cd "$OUT" \
      && timeout "$TIMEOUT_S" "$SERVER" -A none -e -p "$PORT" \
           --read commands.serv --saves saves > game.log 2>&1); then
  echo "FAILED: config=$CONFIG seed=$SEED arm=$ARM (see $OUT/game.log)" >&2
  exit 1
fi

# A game can end at the turn limit or early (conquest, allied victory,
# spaceship); the GAMEOVER autosave is the completion signal for all of
# them, where log wording differs.
if ! ls "$OUT"/saves/*-final.sav* >/dev/null 2>&1; then
  echo "FAILED: game did not reach an ending (no final save): $OUT" >&2
  exit 1
fi

initial=$(ls "$OUT"/saves/freeciv-T0001-*-auto.sav* 2>/dev/null | head -1)
if [ -z "$initial" ]; then
  echo "FAILED: no turn-1 autosave for map hash: $OUT" >&2
  exit 1
fi
hash=$(zstdcat "$initial" | grep -E '^t[0-9]+=' | sha256sum | cut -d' ' -f1)
if [ "$hash" = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ]; then
  echo "FAILED: turn-1 save has no terrain rows: $initial" >&2
  exit 1
fi
echo "$hash" > "$OUT/maphash.txt"

# Keep the turn-1 save (map identity) and the final save; prune the rest.
# KEEP_SAVES=N additionally keeps every Nth turn (N=1 keeps all) for
# post-game analysis of bug-verification games; ~10 gives a 30-save
# series over a 300-turn game. The env var is not part of the result
# key, so an already-DONE pruned game is cache-hit, not rerun -- use a
# fresh --out dir when the save series is wanted.
if [ -z "${KEEP_SAVES:-}" ]; then
  find "$OUT/saves" -name 'freeciv-T*-auto.sav*' ! -name 'freeciv-T0001-*' \
    -delete
else
  for f in "$OUT"/saves/freeciv-T*-auto.sav*; do
    t=${f##*/freeciv-T}
    t=$((10#${t%%-*}))
    if [ "$t" -ne 1 ] && [ $((t % KEEP_SAVES)) -ne 0 ]; then
      rm -f "$f"
    fi
  done
fi

touch "$OUT/DONE"
