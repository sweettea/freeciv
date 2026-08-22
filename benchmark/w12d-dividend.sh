#!/usr/bin/env bash
# W12 slice 4 (corridor trade dividend): the proofs this epic owes,
# re-runnable rather than described.
#
#   ./benchmark/w12d-dividend.sh identity    # 10 paired smoke seeds vs
#                                            # the branch point, deep
#                                            # AND mixed_db cells, at
#                                            # trunk_trade_gain 0.0
#   ./benchmark/w12d-dividend.sh identity-live # the same, with slices
#                                            # 1-3 LIVE and only the
#                                            # dividend absent
#   ./benchmark/w12d-dividend.sh ab [N]      # N (default 16) mirrored
#                                            # lategame-80 seeds:
#                                            # slice-3 ON vs the same
#                                            # plus the dividend
#
# Machine budget: jobs 2, ports 7900-7949, this worktree's own build/.
#
# The A/B design is the brief's and is worth stating. Slice 3's ON arm
# (trunk_ally_gate=1 on top of slices 1-2, score +0.65 t +0.66, NULL)
# is the BASELINE here, not the OFF arm: this slice adds a term to a
# corridor price that already exists, so what it can be credited with
# is its MARGINAL effect on the arm slice 3 measured. The two tuning
# files therefore differ in exactly one key, trunk_trade_gain.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=e6d64d7345          # epic/w03-horizon-content, this branch's base
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.w12d-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=${JOBS:-2}

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

# Pin the binary a run executes by copying it (with libfreeciv.so --
# RUNPATH is $ORIGIN/) out of build/ before a single game starts. Per
# stage, because pinning two stages to one path is how a live server
# gets its text pages replaced underneath it (STATUS 2026-08-13).
pin_server() {
  pin=$ROOT/.w12d-pin/$1
  mkdir -p "$pin"
  cp -f "$(dirname "$SERVER")/libfreeciv.so" "$pin/"
  cp -f "$SERVER" "$pin/freeciv-server"
  SERVER=$pin/freeciv-server
  echo "pinned: $SERVER sha256=$(sha256sum "$SERVER" | cut -d' ' -f1)"
}

case "${1:-}" in
identity)
  OUT=$ROOT/benchmark/results/w12d-identity
  SEEDS=10
  echo "=== identity stage a: base binary for $BASE_SHA ==="
  if [ -x "$BASE_SERVER" ]; then
    echo "cached: $BASE_SERVER"
  else
    rm -rf "$BASE_TREE"
    mkdir -p "$BASE_TREE"
    git archive "$BASE_SHA" | tar -x -C "$BASE_TREE"
    # The source dir is passed EXPLICITLY. meson setup with only a
    # build dir configures the CWD, which in a worktree is the branch
    # -- the base build would silently be the branch build and the
    # proof would compare HEAD with itself.
    # shellcheck disable=SC2086
    meson setup "$BASE_TREE/build" "$BASE_TREE" $MESON_OPTS
    nice -n 10 ninja -C "$BASE_TREE/build" -j "$JOBS"
  fi

  pin_server "$1"
  echo "=== identity stage b: 10 paired smoke seeds, base vs head ==="
  export TUNING=$ROOT/benchmark/tuning/identity.ini
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7900;;
      head) bin=$SERVER;      port=7910;;
    esac
    ./benchmark/run.sh --count "$SEEDS" --arms deep,mixed_db \
      --config benchmark/configs/smoke.conf \
      --server "$bin" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$side"
  done
  unset TUNING

  echo "=== identity stage c: scorelog assertion ==="
  fail=0
  for arm in deep mixed_db; do
    same=0; diff_n=0; missing=0
    for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
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
  # deep-trunkdiv and flip= are tokens only this branch can write.
  if grep -rqs "deep-trunkdiv\|deep-trunkpick.*flip=" "$OUT"/base/; then
    echo "STOP: the base arm emitted a slice-4 token -- not the base." >&2
    exit 1
  fi
  echo "provenance: base logs carry no slice-4 token, as required"
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

identity-live)
  # The identity above is at COMPILED DEFAULTS, where trunk_road_gain
  # is 0 and no corridor plan exists at all -- so it proves the new
  # code is inert on a path it never reaches. This stage proves the
  # stronger thing: with slices 1-3 fully live (tuning/w12-ally-on.ini,
  # which does not mention trunk_trade_gain and therefore leaves it at
  # its compiled 0.0), the branch is still byte-identical to the branch
  # point. Corridors are planned, priced and worked in both arms; only
  # the dividend is absent.
  OUT=$ROOT/benchmark/results/w12d-identity-live
  SEEDS=10
  [ -x "$BASE_SERVER" ] || { echo "run the identity stage first" >&2; exit 1; }
  pin_server "$1"
  export TUNING=$ROOT/benchmark/tuning/w12-ally-on.ini
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7900;;
      head) bin=$SERVER;      port=7910;;
    esac
    ./benchmark/run.sh --count "$SEEDS" --arms deep \
      --config benchmark/configs/smoke.conf \
      --server "$bin" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$side"
  done
  unset TUNING

  same=0; diff_n=0
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                | head -n "$SEEDS"); do
    a=$(echo "$OUT"/base/smoke-*/deep/seed-"$seed"/scorelog.txt)
    b=$(echo "$OUT"/head/smoke-*/deep/seed-"$seed"/scorelog.txt)
    if cmp -s "$a" "$b"; then
      same=$((same + 1))
    else
      echo "seed $seed: SCORELOGS DIFFER"; diff_n=$((diff_n + 1))
    fi
  done
  echo "on-path identity deep: $same/$SEEDS byte-identical, $diff_n differ"
  if grep -rqs "deep-trunkdiv" "$OUT"/base/; then
    echo "STOP: the base arm emitted a slice-4 token -- not the base." >&2
    exit 1
  fi
  # ... and the corridor machinery really was live in both arms.
  grep -rh "deep-trunk turn=" "$OUT"/head/smoke-*/deep/seed-1/game.log \
    | tail -1
  [ "$same" -eq "$SEEDS" ] || { echo "STOP: not inert on path." >&2; exit 1; }
  ;;

ab)
  # One knob, one binary, both arms. off = tuning/w12-ally-on.ini
  # (slices 1-3 at their probe values, which is slice 3's ON arm),
  # on = the same plus trunk_trade_gain = 1.0.
  OUT=$ROOT/benchmark/results/w12d-ab
  SEEDS=${2:-16}
  pin_server "$1"
  # ON first: if the dividend turns out not to fire on this venue, the
  # comparison is void and that is worth learning in the first arm.
  for arm in on off; do
    case $arm in
      on)  ini=$ROOT/benchmark/tuning/w12-div-on.ini;  port=7920;;
      off) ini=$ROOT/benchmark/tuning/w12-ally-on.ini; port=7935;;
    esac
    echo "=== A/B arm $arm: $SEEDS lategame-80 games ==="
    TUNING=$ini ./benchmark/run.sh --count "$SEEDS" --arms deep \
      --config benchmark/configs/lategame-80.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$arm"
  done
  off_cell=$(echo "$OUT"/off/lategame-80-*)
  on_cell=$(echo "$OUT"/on/lategame-80-*)
  echo "=== paired compare ==="
  ./benchmark/paircmp.py "$off_cell" "$on_cell" \
    --pat div-turns="deep-trunkdiv turn=" \
    --pat div-links="deep-trunkdiv .*links=[1-9]" \
    --pat div-picks="deep-trunkdiv .*picked=[1-9]" \
    --pat div-flips="deep-trunkpick.*flip=1" \
    --pat trunk-picks="deep-trunkpick turn=" \
    --pat ally-picks="deep-trunkpick.*ally=1" \
    --arm deep
  ;;

*)
  echo "usage: $0 {identity|identity-live|ab [seeds]}" >&2
  exit 1;;
esac
