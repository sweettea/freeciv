#!/usr/bin/env bash
# W29 (terrain chains): the proofs this epic owes, re-runnable rather
# than described.
#
#   ./benchmark/w29-chains.sh identity        10 paired smoke seeds vs
#                                             the branch point, in a
#                                             deep cell, a MIXED cell
#                                             and a STOCK-ONLY cell,
#                                             everything at compiled
#                                             defaults (the gate shut)
#   ./benchmark/w29-chains.sh live [N] [JOBS] N (default 6) mirrored
#                                             garden seeds, gate shut
#                                             vs gate open: the venue
#                                             where terrain changes
#                                             actually happen
#   ./benchmark/w29-chains.sh report          the census and the
#                                             terrain-change counts,
#                                             both arms
#
# Machine budget: ports 7650-7699, jobs <= 2, this worktree's own build/
# and its own pinned binary. garden.conf is a 300-turn solo game and
# runs ~12 min, so `live 6` is ~2.5 h at jobs 2; it resumes off run.sh's
# DONE markers, so re-running the same command after a kill continues.
#
# WHY GARDEN AND NOT A CORRIDOR VENUE. The mechanism needs a city that
# wants a terrain it cannot reach in one step, and time. garden.conf is
# one AI alone on a scarce island for 300 turns with no war noise --
# land runs out mid-game with centuries left, which is exactly when a
# city stops being able to buy a better tile and has to make one.
#
# Pre-registered predictions, in order of confidence:
#   1. identity: 10/10 byte-identical scorelogs in all three cells
#   2. base emits no deep-terrachain token; head at defaults emits the
#      line with lift=0 (instrument live, knob shut)
#   3. gate open: lift > 0 -- the mechanism is reachable on this venue
#   4. gate open: lifted tiles that go on to change terrain (work > 0)
#   5. score >= the shut arm
# All five are reported whatever they say. As of the epic that wrote
# this file, 1-3 held and 4 did NOT: see STATUS.

set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)

BASE_SHA=6e2bc86cca                 # epic/w03-horizon-content, branch point
BUILD=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.w29-base/base-$BASE_SHA
BASE_BUILD=$BASE_TREE/build/freeciv-server
ON=$here/tuning/w29-chain-on.ini
CONFIG=$here/configs/garden.conf

MESON_OPTS="-Dclients= -Dfcmp= -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

# Pin the binary a run executes by copying it -- WITH libfreeciv.so,
# whose RUNPATH is $ORIGIN/ -- out of build/ before a single game
# starts. Per stage and per side, because pinning two binaries to one
# path is how a live server gets its text pages replaced underneath it.
# A re-pin of the SAME bytes must not copy either: cp truncates and
# rewrites, which SIGBUSes every server already running from that path.
pin() {
  local src=$1 dst=$ROOT/.stage-bin/$2 f
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

case "${1:-}" in
identity)
  OUT=$here/results/w29-identity
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
    nice -n 10 ninja -C "$BASE_TREE/build" -j 4 || exit 1
  fi

  base_bin=$(pin "$BASE_BUILD" identity-base)
  head_bin=$(pin "$BUILD" identity-head)
  echo "base: $base_bin sha256=$(sha256sum "$base_bin" | cut -d' ' -f1)"
  echo "head: $head_bin sha256=$(sha256sum "$head_bin" | cut -d' ' -f1)"

  # No tuning file in either arm: the new key sits at its compiled 0,
  # which is the setting every existing run of this project is at, and
  # is therefore the setting the identity claim has to be made about.
  # The `classic` cell has no deep seat at all and is the one that
  # answers "does the default server still behave identically" -- this
  # epic adds a hook to SHARED advisors code, and that cell is what
  # makes the inertness checkable rather than asserted.
  for side in base head; do
    case $side in
      base) bin=$base_bin; port=7650;;
      head) bin=$head_bin; port=7665;;
    esac
    TUNING= "$here/run.sh" --count "$SEEDS" --arms deep,mixed_db,classic \
      --config "$here/configs/smoke.conf" --server "$bin" --jobs 2 \
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
  if grep -rqs "deep-terrachain" "$OUT"/base/; then
    echo "STOP: the base arm emitted a W29 token -- not the base." >&2
    exit 1
  fi
  # ... and the instrument really was live in head, with the knob shut.
  echo "head arm, gate at its compiled default (lift must be 0):"
  grep -rh "deep-terrachain turn=" "$OUT"/head/smoke-*/deep/*/game.log \
    | tail -1
  if grep -rhq "deep-terrachain .*lift=[1-9]" "$OUT"/head/; then
    echo "STOP: head lifted a price with no tuning file -- the gate is" \
         "not shut at its compiled default." >&2
    exit 1
  fi
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  echo "IDENTITY OK"
  ;;

live)
  OUT=$here/results/w29-live
  SEEDS=${2:-6}
  JOBS=${3:-2}
  bin=$(pin "$BUILD" live)
  echo "=== W29 liveness: $SEEDS mirrored garden seeds, jobs $JOBS ==="
  echo "server=$bin sha256=$(sha256sum "$bin" | cut -d' ' -f1)"
  # ON first: if the mechanism does not fire on this venue the
  # comparison is void, and that is worth learning in the first arm.
  for arm in on off; do
    tun=; port=7670
    [ "$arm" = on ] && { tun=$ON; port=7680; }
    echo "--- arm $arm (${tun:-compiled defaults}) ---"
    TUNING=$tun "$here/run.sh" --count "$SEEDS" --config "$CONFIG" \
      --arms deep --jobs "$JOBS" --portbase "$port" \
      --server "$bin" --out "$OUT/$arm" || exit 1
  done
  echo "=== arms complete; run '$0 report' ==="
  ;;

report)
  OUT=$here/results/w29-live
  offdir=$(ls -d "$OUT"/off/garden-* 2>/dev/null | head -1)
  ondir=$(ls -d "$OUT"/on/garden-* 2>/dev/null | head -1)
  [ -n "$offdir" ] && [ -n "$ondir" ] \
    || { echo "no liveness results under $OUT" >&2; exit 1; }

  echo "=== read-back audit: which tuning file each arm loaded ==="
  for d in "$offdir" "$ondir"; do
    for g in "$d"/deep/seed-*/game.log; do
      [ -f "$g" ] || continue
      grep -m1 -o 'deep-tuning file="[^"]*"' "$g"
    done | sort | uniq -c
  done

  echo
  echo "=== the gate's own instrument, OFF arm (lift must be 0) ==="
  "$here/chaincensus.py" "$offdir"
  echo
  echo "=== the gate's own instrument, ON arm ==="
  "$here/chaincensus.py" "$ondir"

  echo
  echo "=== terrain actually changed, OFF then ON ==="
  "$here/terraform.py" --root "$offdir" --arm deep | tail -2
  "$here/terraform.py" --root "$ondir" --arm deep | tail -2

  echo
  echo "=== outcome identity: how many seeds did the gate not move? ==="
  same=0; total=0
  for a in "$offdir"/deep/seed-*/scorelog.txt; do
    seed=$(basename "$(dirname "$a")")
    b=$ondir/deep/$seed/scorelog.txt
    [ -r "$b" ] || continue
    total=$((total + 1))
    cmp -s "$a" "$b" && same=$((same + 1))
  done
  echo "$same/$total seeds byte-identical between the arms"

  echo
  echo "=== paired A/B ==="
  "$here/paircmp.py" "$offdir" "$ondir" \
    --pat chain-lift="deep-terrachain .*lift=[1-9]"
  ;;

*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
