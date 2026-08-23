#!/usr/bin/env bash
# Rail upgrade (W12: a finished trunk is re-examined against the roads
# the player can now build): the proofs this epic owes, re-runnable
# rather than described.
#
#   ./benchmark/railup.sh venue [N] [JOBS]   N (default 4) seeds on the
#                                            venue, gate ON, to answer
#                                            ONE question before any
#                                            batch is spent: does this
#                                            venue ever finish a trunk
#                                            (deep-trunkup whole>0) and
#                                            does railroad arrive in
#                                            time (links>0, picked>0)
#   ./benchmark/railup.sh identity           10 paired smoke seeds vs
#                                            the branch point, in a deep
#                                            cell, a MIXED cell and a
#                                            STOCK-ONLY cell, corridor
#                                            family ON, upgrade gate
#                                            shut
#   ./benchmark/railup.sh ab [N] [JOBS]      N (default 16) mirrored
#                                            seeds on the chosen venue:
#                                            the family as it stands vs
#                                            the same plus the upgrade
#                                            gate
#   ./benchmark/railup.sh report             the tables, both arms
#
# VENUE is a variable and not a constant, deliberately, and the venue
# stage is what settled it. The mechanism needs TWO things: a finished
# trunk, and a better road buildable. `corridor-150` -- the venue the
# rest of the corridor family is measured on -- delivers the first and
# not the second: Railroad is 26 techs deep in classic (Steam Engine +
# Bridge Building) and corridor-150 seats end T150 holding 20-39 techs.
# The probe was stopped at T96/T107/T124 of 150 on three seeds, having
# emitted 49/105/57 census lines with whole>0 and links=0 on every one:
# no seat had a better road to lay by then, and the tech arithmetic says
# most never would. `lategame-80` delivers both from about T10 --
# techlevel 50 and ten workers put the roads down before the corridor
# plan can want them, which is exactly what corridor-150's own header
# criticises it for AS A BUILD-PHASE VENUE and exactly what an UPGRADE
# phase needs. The venue that is wrong for slice 1 is the right one for
# this slice, for the same reason. Set RAILUP_CONFIG to override.
#
# Machine budget: ports 7650-7699, jobs <= 3, this worktree's own build/
# and its own pinned binary. It resumes off run.sh's DONE markers, so
# re-running the same command after a background kill continues it.
#
# The A/B's control arm is tuning/w12g-keep-on.ini -- the corridor
# family at the values W12G measured. That is the comparison this change
# owes: not "corridors on vs off", which the venue pilot settled, but
# "the same corridors, no longer frozen at the tech level they were
# built at".
#
# Pre-registered predictions, in order of confidence:
#   1. deep-trunkup links=/picked= are nonzero ON and zero OFF, while
#      whole= is nonzero in BOTH (the shared denominator)
#   2. corridor picks land on tiles that already carry a road
#   3. unbuilt corridor is NOT starved: the build phase's own counters
#      (deep-trunksrc picked=, deep-trunk gap=) do not fall
#   4. score moves >= 0
# All four are reported whatever they say.

set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)

BASE_SHA=c644628cb8                 # the branch point of epic/rail-upgrade
BUILD=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.railup-base/base-$BASE_SHA
BASE_BUILD=$BASE_TREE/build/freeciv-server
ON=$here/tuning/railup-on.ini
OFF=$here/tuning/w12g-keep-on.ini
CONFIG=${RAILUP_CONFIG:-$here/configs/lategame-80.conf}
CELL=$(basename "$CONFIG" .conf)

MESON_OPTS="-Dclients= -Dfcmp= -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

# Every GAME process runs at nice 10 (2026-08-23 directive: a human is
# playing on this box and background batches were starving the
# interactive server). The niceness is applied at the run.sh call, so it
# is inherited by run-game.sh and by every freeciv-server it forks --
# run-game.sh itself is left alone deliberately, because run.sh hashes
# it into the result cache key and editing it would fork the shared
# harness for every other epic.

# Pin the binary a run executes by copying it -- WITH libfreeciv.so,
# whose RUNPATH is $ORIGIN/ -- out of build/ before a single game
# starts. Per stage and per side, because pinning two binaries to one
# path is how a live server gets its text pages replaced underneath it.
# A re-pin of the SAME bytes must not copy either: cp truncates and
# rewrites, which SIGBUSes every server already running from that path.
pin() {
  local src=$1 dst=$ROOT/.railup-bin/$2 f
  mkdir -p "$dst"
  for f in libfreeciv.so freeciv-server; do
    local from=$(dirname "$src")/$f to=$dst/$f
    [ "$f" = freeciv-server ] && from=$src
    if [ -f "$to" ] \
       && [ "$(sha256sum < "$from")" = "$(sha256sum < "$to")" ]; then
      continue
    fi
    cp -f "$from" "$to"
  done
  echo "$dst/freeciv-server"
}

# deep-trunkup, summed over every player-turn of every game in a result
# dir. One line per seed, so a per-game count can be pasted rather than
# described.
upcensus() {
  python3 "$here/upcensus.py" "$1"
}

# Stage DONE marker. A detached batch is only trustworthy if its exit
# status is on the record: a killed or crashed stage must not be
# readable as a finished one, and a stage that ran to completion must
# say so without anyone having to reconstruct it from timestamps.
STAGE=${1:-}
MARK=$here/results/railup-DONE-$STAGE
if [ -n "$STAGE" ]; then
  mkdir -p "$here/results"
  rm -f "$MARK"
fi
finish() {
  local rc=$?
  [ -n "$STAGE" ] || return 0
  if [ "$rc" -eq 0 ]; then
    echo "SUCCESS rc=0 $(date -Is)" > "$MARK"
  else
    echo "FAIL rc=$rc $(date -Is)" > "$MARK"
  fi
}
trap finish EXIT

case "${1:-}" in
venue)
  # Deliberately the A/B's OWN on-arm directory: run.sh keys its
  # cache on (tuning_hash, config, seed) and resumes off DONE
  # markers, so the venue probe IS the first N games of the ON arm
  # and none of it is thrown away when the batch follows.
  OUT=$here/results/railup-ab/on
  SEEDS=${2:-4}
  JOBS=${3:-3}
  bin=$(pin "$BUILD" venue)
  echo "=== venue probe: $SEEDS $CELL seeds, gate ON, jobs $JOBS ==="
  echo "server=$bin sha256=$(sha256sum "$bin" | cut -d' ' -f1)"
  TUNING=$ON nice -n 10 "$here/run.sh" --count "$SEEDS" --config "$CONFIG" \
    --arms deep --jobs "$JOBS" --portbase 7680 \
    --server "$bin" --out "$OUT" || exit 1
  d=$(ls -d "$OUT"/"$CELL"-* 2>/dev/null | head -1)
  echo
  echo "=== deep-trunkup, summed per game ==="
  upcensus "$d"
  echo
  echo "whole=0 everywhere: this venue never finishes a trunk -- extend it."
  echo "whole>0, links=0:   trunks finish but no better road is buildable"
  echo "                    in time -- extend the horizon or the tech pace."
  echo "links>0, picked=0:  upgrades priced and outbid -- a null, not a"
  echo "                    dead venue."
  ;;

identity)
  OUT=$here/results/railup-identity
  SEEDS=10
  if [ -x "$BASE_BUILD" ]; then
    echo "cached base binary: $BASE_BUILD"
  else
    rm -rf "$BASE_TREE"
    mkdir -p "$BASE_TREE"
    git -C "$ROOT" archive "$BASE_SHA" | tar -x -C "$BASE_TREE"
    # Source dir passed EXPLICITLY: `meson setup DIR` alone configures
    # the CWD, which in a worktree is the BRANCH -- the base build would
    # silently be the branch build and the proof would compare HEAD with
    # itself.
    # shellcheck disable=SC2086
    meson setup "$BASE_TREE/build" "$BASE_TREE" $MESON_OPTS || exit 1
    nice -n 10 ninja -C "$BASE_TREE/build" -j 3 || exit 1
  fi

  base_bin=$(pin "$BASE_BUILD" identity-base)
  head_bin=$(pin "$BUILD" identity-head)
  echo "base: $base_bin sha256=$(sha256sum "$base_bin" | cut -d' ' -f1)"
  echo "head: $head_bin sha256=$(sha256sum "$head_bin" | cut -d' ' -f1)"

  # Family ON in BOTH arms and all three cells: the gate must be inert
  # on a path the corridor code actually walks, not merely on one it
  # never reaches. The new key appears in neither file, so it sits at
  # its compiled 0. The `classic` cell has no deep seat at all and is
  # the one that answers "does the default server still behave
  # identically" -- this epic touches only ai/deep, and that cell is
  # what makes the claim checkable rather than asserted.
  for side in base head; do
    case $side in
      # Sequential arms, so one block serves both: 10 seeds x 3 cells
      # is 30 consecutive ports, 7650-7679, inside this epic's
      # 7650-7699 budget with the A/B's 7680+ left clear.
      base) bin=$base_bin; port=7650;;
      head) bin=$head_bin; port=7650;;
    esac
    TUNING=$OFF nice -n 10 "$here/run.sh" --count "$SEEDS" --arms deep,mixed_db,classic \
      --config "$here/configs/smoke.conf" --server "$bin" --jobs 3 \
      --portbase "$port" --out "$OUT/$side" || exit 1
  done

  fail=0
  for arm in deep mixed_db classic; do
    same=0; diff_n=0; missing=0
    for seed in $(grep -E '^[0-9]+$' "$here/seeds/train.txt" \
                  | head -n "$SEEDS"); do
      a=$(echo "$OUT"/base/smoke-*/"$arm"/seed-"$seed"/scorelog.txt)
      b=$(echo "$OUT"/head/smoke-*/"$arm"/seed-"$seed"/scorelog.txt)
      if [ ! -r "$a" ] || [ ! -r "$b" ]; then
        echo "$arm seed $seed: MISSING scorelog"; missing=$((missing + 1))
      elif cmp -s "$a" "$b"; then
        same=$((same + 1))
      else
        echo "$arm seed $seed: SCORELOGS DIFFER"; diff_n=$((diff_n + 1))
      fi
    done
    echo "identity $arm: $same/$SEEDS byte-identical, $diff_n differ," \
         "$missing missing"
    [ "$same" -eq "$SEEDS" ] || fail=1
  done

  # The proof must not be able to pass because both sides are HEAD.
  if grep -rqs "deep-trunkup" "$OUT"/base/; then
    echo "STOP: the base arm emitted a rail-upgrade token -- not the base." >&2
    exit 1
  fi
  # ... and the corridor family really was live in both.
  echo "corridor liveness, head arm:"
  grep -rh "deep-trunksrc turn=" "$OUT"/head/smoke-*/deep/seed-1/game.log \
    | tail -1
  echo "upgrade denominator, head arm (gate shut: links must be 0):"
  grep -rh "deep-trunkup turn=" "$OUT"/head/smoke-*/deep/*/game.log \
    | tail -1
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  echo "IDENTITY OK"
  ;;

ab)
  OUT=$here/results/railup-ab
  SEEDS=${2:-16}
  JOBS=${3:-3}
  bin=$(pin "$BUILD" ab)
  echo "=== rail-upgrade A/B: $SEEDS mirrored $CELL seeds, jobs $JOBS ==="
  echo "server=$bin sha256=$(sha256sum "$bin" | cut -d' ' -f1)"
  # ON first: if the gate does not fire on this venue the comparison is
  # void, and that is worth learning in the first arm rather than the
  # second.
  for arm in on off; do
    tun=$OFF; port=7650
    [ "$arm" = on ] && { tun=$ON; port=7680; }
    echo "--- arm $arm ($(basename "$tun")) ---"
    TUNING=$tun nice -n 10 "$here/run.sh" --count "$SEEDS" --config "$CONFIG" \
      --arms deep --jobs "$JOBS" --portbase "$port" \
      --server "$bin" --out "$OUT/$arm" || exit 1
  done
  echo "=== arms complete; run '$0 report' ==="
  ;;

report)
  OUT=$here/results/railup-ab
  offdir=$(ls -d "$OUT"/off/"$CELL"-* 2>/dev/null | head -1)
  ondir=$(ls -d "$OUT"/on/"$CELL"-* 2>/dev/null | head -1)
  [ -n "$offdir" ] && [ -n "$ondir" ] \
    || { echo "no A/B results under $OUT" >&2; exit 1; }

  echo "=== read-back audit: which tuning file each arm loaded ==="
  for d in "$offdir" "$ondir"; do
    for g in "$d"/deep/seed-*/game.log; do
      [ -f "$g" ] || continue
      grep -m1 -o 'deep-tuning file="[^"]*"' "$g"
    done | sort | uniq -c
  done

  echo
  echo "=== the gate's own instrument, OFF arm (links must be 0) ==="
  upcensus "$offdir"
  echo
  echo "=== the gate's own instrument, ON arm ==="
  upcensus "$ondir"

  echo
  echo "=== paired A/B ==="
  "$here/paircmp.py" "$offdir" "$ondir" \
    --pat trunk-picks="deep-trunkpick turn=" \
    --pat up-live="deep-trunkup .*links=[1-9]" \
    --pat up-pick="deep-trunkup .*picked=[1-9]"
  ;;

*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
