#!/usr/bin/env bash
# W12H (risk-priced permeable corridors): the proofs this epic owes,
# re-runnable rather than described.
#
#   ./benchmark/w12h-soft.sh identity        10 paired smoke seeds vs the
#                                            branch point, deep AND
#                                            mixed_db cells, corridor
#                                            family ON in both, both new
#                                            knobs at their compiled 0
#   ./benchmark/w12h-soft.sh live [N] [JOBS] N (default 4) lategame-80
#                                            seeds in three arms: family
#                                            only, +soft borders,
#                                            +soft borders +risk
#   ./benchmark/w12h-soft.sh report          the soft-border, risk,
#                                            exposure and alignment
#                                            tables over the live arms
#
# Machine budget: ports 7500-7549, jobs <= 4, this worktree's own build/
# and its own pinned binary.
#
# Venue. lategame-80 is the cell W12c measured alliances forming in, and
# alliances are what the meet-in-the-middle question needs (two allied
# deep players planning the same link). Peace treaties, which the soft
# border gate needs, occur on both it and corridor-150 -- the first
# deep-trunksoft line this epic ever printed showed a corridor crossing
# four cease-fire tiles on corridor-150 seed-1 T22 that were a signed
# peace by T23. The three arms are one gate apart each way, so what
# separates arm 2 from arm 1 is the border and what separates arm 3 from
# arm 2 is the risk term, and no line has to be read across two changes.
#
# This epic's bar is mechanism truth and liveness, not score: the
# corridor family's own benchmark verdict is null-to-negative and the
# family ships off (STATUS, W12g). Nothing here computes a score delta.

set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)

BASE_SHA=50fdc4a620                 # epic/w03-horizon-content, the branch point
BUILD=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.w12h-base/base-$BASE_SHA
BASE_BUILD=$BASE_TREE/build/freeciv-server
FAMILY=$here/tuning/w12g-keep-on.ini
SOFT=$here/tuning/w12h-soft-on.ini
RISK=$here/tuning/w12h-risk-on.ini
CONFIG=$here/configs/lategame-80.conf

MESON_OPTS="-Dclients= -Dfcmp= -Dtools= -Daimodules=classic,tex,baseline,deep"

# Pin the binary a run executes by copying it -- WITH libfreeciv.so,
# whose RUNPATH is $ORIGIN/ -- out of build/ before a single game
# starts, and ONLY when the bytes differ: cp truncates and rewrites, so
# re-pinning an identical binary replaces the text pages of every server
# already running from that path and they die of SIGBUS (W12G measured
# it, four games at ~T94).
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
  OUT=$here/results/w12h-identity
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

  base_bin=$(pin "$BASE_BUILD" w12h-identity-base)
  head_bin=$(pin "$BUILD" w12h-identity-head)
  echo "base: $base_bin sha256=$(sha256sum "$base_bin" | cut -d' ' -f1)"
  echo "head: $head_bin sha256=$(sha256sum "$head_bin" | cut -d' ' -f1)"

  # Family ON in BOTH arms and both cells: the new gates must be inert
  # on a path the corridor code actually walks, not merely on one it
  # never reaches. Neither new key appears in $FAMILY, so both sit at
  # their compiled 0.
  for side in base head; do
    case $side in
      base) bin=$base_bin; port=7500;;
      head) bin=$head_bin; port=7510;;
    esac
    TUNING=$FAMILY "$here/run.sh" --count "$SEEDS" --arms deep,mixed_db \
      --config "$here/configs/smoke.conf" --server "$bin" --jobs 4 \
      --portbase "$port" --out "$OUT/$side" || exit 1
  done

  # scorelog.txt and not game.log: this epic ADDS log lines by design,
  # so the game record is what has to match, not the commentary.
  fail=0
  for arm in deep mixed_db; do
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
  if grep -rqs "deep-trunksoft" "$OUT"/base/; then
    echo "STOP: the base arm emitted a W12H token -- not the base." >&2
    exit 1
  fi
  echo "corridor liveness, head arm:"
  grep -rh "deep-trunksrc turn=" "$OUT"/head/smoke-*/deep/seed-1/game.log \
    | tail -1
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  echo "IDENTITY OK"
  ;;

live)
  OUT=$here/results/w12h-live
  SEEDS=${2:-4}
  JOBS=${3:-4}
  bin=$(pin "$BUILD" w12h-live)
  echo "=== W12H liveness: $SEEDS lategame-80 seeds x 3 arms ==="
  echo "server=$bin sha256=$(sha256sum "$bin" | cut -d' ' -f1)"
  for arm in family soft risk; do
    case $arm in
      family) tun=$FAMILY; port=7500;;
      soft)   tun=$SOFT;   port=7515;;
      risk)   tun=$RISK;   port=7530;;
    esac
    echo "--- arm $arm ($(basename "$tun")) ---"
    TUNING=$tun "$here/run.sh" --count "$SEEDS" --config "$CONFIG" \
      --arms deep --jobs "$JOBS" --portbase "$port" \
      --server "$bin" --out "$OUT/$arm" || exit 1
  done
  echo "=== arms complete; run '$0 report' ==="
  ;;

report)
  OUT=$here/results/w12h-live
  python3 "$here/softcensus.py" "$OUT"/family "$OUT"/soft "$OUT"/risk
  ;;

*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1
  ;;
esac
