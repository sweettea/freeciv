#!/usr/bin/env bash
# W12G (only bid for workers you can keep): the proofs this epic owes,
# re-runnable rather than described.
#
#   ./benchmark/w12g-keepable.sh identity      10 paired smoke seeds vs
#                                              the branch point, deep AND
#                                              mixed_db cells, family ON
#                                              in both, both gates shut
#   ./benchmark/w12g-keepable.sh ab [N] [JOBS] N (default 29) mirrored
#                                              corridor-150 seeds: the
#                                              family as measured vs the
#                                              same plus both gates
#   ./benchmark/w12g-keepable.sh report        the tables, both arms
#
# Machine budget: ports 7500-7549, jobs <= 4, this worktree's own build/
# and its own pinned binary. corridor-150 runs ~20 min a game, so the
# A/B is ~5 h at jobs 4; it resumes off run.sh's DONE markers, so
# re-running the same command after the background kill continues it.
#
# The A/B's control arm is tuning/w12-div-on.ini -- the corridor family
# at the probe values slices 1-4 ran at, WITH its leak. That is the
# comparison the fix owes: not "corridors on vs off", which the venue
# pilot already measured, but "the same corridors, executed without
# bidding for workers the founder logic reclaims and without turning a
# walking worker around for an epsilon".
#
# Pre-registered predictions, in the brief's order of confidence:
#   1. start-rate (funnel stage 4 / stage 1) rises well above 8.1%
#   2. walking turns per completed road fall
#   3. the techs deficit (pilot t -3.18) shrinks
#   4. score moves toward >= 0
# All four are reported whatever they say.

set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)

BASE_SHA=fc137e1b9d                 # epic/w03-horizon-content, the branch point
BUILD=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.w12g-base/base-$BASE_SHA
BASE_BUILD=$BASE_TREE/build/freeciv-server
ON=$here/tuning/w12g-keep-on.ini
OFF=$here/tuning/w12-div-on.ini
CONFIG=$here/configs/corridor-150.conf

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

# Pin the binary a run executes by copying it -- WITH libfreeciv.so,
# whose RUNPATH is $ORIGIN/ -- out of build/ before a single game
# starts. Per stage and per side, because pinning two binaries to one
# path is how a live server gets its text pages replaced underneath it.
pin() {
  local src=$1 dst=$ROOT/.stage-bin/$2
  mkdir -p "$dst"
  cp -f "$(dirname "$src")/libfreeciv.so" "$dst/"
  cp -f "$src" "$dst/freeciv-server"
  echo "$dst/freeciv-server"
}

case "${1:-}" in
identity)
  OUT=$here/results/w12g-identity
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

  # Family ON in BOTH arms and both cells: the gates must be inert on a
  # path the corridor code actually walks, not merely on one it never
  # reaches. Neither new key appears in this file, so both sit at their
  # compiled 0.
  for side in base head; do
    case $side in
      base) bin=$base_bin; port=7500;;
      head) bin=$head_bin; port=7510;;
    esac
    TUNING=$OFF "$here/run.sh" --count "$SEEDS" --arms deep,mixed_db \
      --config "$here/configs/smoke.conf" --server "$bin" --jobs 4 \
      --portbase "$port" --out "$OUT/$side" || exit 1
  done

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
  if grep -rqs "deep-trunkkeep" "$OUT"/base/; then
    echo "STOP: the base arm emitted a W12G token -- not the base." >&2
    exit 1
  fi
  # ... and the corridor family really was live in both.
  echo "corridor liveness, head arm:"
  grep -rh "deep-trunksrc turn=" "$OUT"/head/smoke-*/deep/seed-1/game.log \
    | tail -1
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  echo "IDENTITY OK"
  ;;

ab)
  OUT=$here/results/w12g-ab
  SEEDS=${2:-29}
  JOBS=${3:-4}
  bin=$(pin "$BUILD" ab)
  echo "=== W12G A/B: $SEEDS mirrored corridor-150 seeds, jobs $JOBS ==="
  echo "server=$bin sha256=$(sha256sum "$bin" | cut -d' ' -f1)"
  # ON first: if the gates do not fire on this venue the comparison is
  # void, and that is worth learning in the first arm rather than the
  # second.
  for arm in on off; do
    tun=$OFF; port=7520
    [ "$arm" = on ] && { tun=$ON; port=7535; }
    echo "--- arm $arm ($(basename "$tun")) ---"
    TUNING=$tun "$here/run.sh" --count "$SEEDS" --config "$CONFIG" \
      --arms deep --jobs "$JOBS" --portbase "$port" \
      --server "$bin" --out "$OUT/$arm" || exit 1
  done
  echo "=== arms complete; run '$0 report' ==="
  ;;

report)
  OUT=$here/results/w12g-ab
  offdir=$(ls -d "$OUT"/off/corridor-150-* 2>/dev/null | head -1)
  ondir=$(ls -d "$OUT"/on/corridor-150-* 2>/dev/null | head -1)
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
  echo "=== funnel, OFF arm (the family with its leak) ==="
  "$here/funnelreport.py" "$offdir"
  echo
  echo "=== funnel, ON arm (both gates) ==="
  "$here/funnelreport.py" "$ondir"

  echo
  echo "=== the gates' own instruments (ON arm; OFF must be a zero) ==="
  for d in "$offdir" "$ondir"; do
    echo "-- $(basename "$(dirname "$d")")"
    "$here/keepcensus.py" "$d"
  done

  echo
  echo "=== paired A/B ==="
  "$here/paircmp.py" "$offdir" "$ondir" \
    --pat trunk-picks="deep-trunkpick turn=" \
    --pat keep-veto="deep-trunkkeep .*veto=[1-9]" \
    --pat keep-held="deep-trunkkeep .*held=[1-9]" \
    --pat keep-kept="deep-trunkkeep .*kept=[1-9]"
  ;;

*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
