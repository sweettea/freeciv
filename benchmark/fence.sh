#!/usr/bin/env bash
# FENCE (2026-08-13, task #63): identity, liveness and the mixed A/B for
# `diplo_deep_only_pacts`.
#
#   ./benchmark/fence.sh identity  # 10 deep + 2 mixed_dc smoke seeds
#   ./benchmark/fence.sh live      # 2 tech50war-200 games, knob on
#   ./benchmark/fence.sh ab        # 16 mirrored tech50war-200 seeds x 2 arms
#
# Machine budget for this epic: ports 7100-7199 only, jobs 4. Two derby
# servers own the rest of the box.
#
# LAUNCH THESE WITH THE HARNESS'S OWN BACKGROUND FACILITY, NEVER WITH
# `nohup ... &` INSIDE A TOOL CALL. Measured here the hard way, four
# times: every batch started that way froze at the exact second its
# launching call returned -- 15:10:12, 15:41:36, 15:39:06, 15:42:08,
# each matching its own call's return to the second, with the load
# average back at 1.0 and the games sitting mid-turn. Suspended, not
# slow. The failure is silent and it mimics a performance problem
# convincingly enough that it cost this session a wrong diagnosis (a
# tuning file was blamed and briefly replaced before the timestamps
# were lined up).
#
# THE ONE-BINARY RULE. Every stage below runs against ONE pinned copy of
# the head server, taken out of build/ before a single game starts, so
# both arms of the A/B are the same bytes and an edit during the run
# cannot taint it. The result cache is keyed on the binary's sha, so a
# tainted pair would not even share a cell -- but the rule is there so
# the question never has to be asked.
#
# The identity stage builds the base binary from a `git archive` export
# into .fence-base/ (no git worktree registered) and asserts the
# scorelogs are byte-identical seed by seed. Two traps it is written
# around, both from STATUS 2026-08-12:
#   * `meson setup <builddir>` with no source argument takes the
#     CALLER's cwd, which would make the "base" binary the branch under
#     test and the proof worthless. The source dir is always explicit.
#   * A proof can pass because both sides are the branch. Stage c greps
#     the BASE arm's logs for `deep-fence`, a tag only this branch can
#     emit, and fails if it is there.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=1bcf470d5a
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.fence-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=${JOBS:-4}

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

# Pin by copying out of build/. The server's RUNPATH is $ORIGIN/, so the
# shared library travels with it and a pinned copy is self-contained.
#
# The copy is SKIPPED when the pin already holds these bytes. Two
# reasons, and the second is not cosmetic: stages overlap (identity and
# the A/B can share the box, per CLAUDE.md's parallel-work clause), and
# `cp` onto a running executable fails ETXTBSY -- so a re-pin that had
# nothing to change would kill a live stage rather than no-op.
pin_server() {
  mkdir -p "$ROOT/.fence-pin"
  pin=$ROOT/.fence-pin/freeciv-server
  want=$(sha256sum "$SERVER" | cut -d' ' -f1)
  have=$(sha256sum "$pin" 2>/dev/null | cut -d' ' -f1 || true)
  if [ "$want" != "${have:-}" ]; then
    cp -f "$(dirname "$SERVER")/libfreeciv.so" "$ROOT/.fence-pin/"
    cp -f "$SERVER" "$pin"
  fi
  SERVER=$pin
  echo "pinned: $SERVER sha256=$want (recopied: $([ "$want" != "${have:-}" ] \
    && echo yes || echo no))"
}

cell() { ls -1dt "$1"/"$2"-* 2>/dev/null | head -n 1; }

case "${1:-}" in
identity)
  OUT=$ROOT/benchmark/results/fence-identity
  SEEDS=10
  echo "=== identity stage a: base binary for $BASE_SHA ==="
  if [ -x "$BASE_SERVER" ]; then
    echo "cached: $BASE_SERVER"
  else
    rm -rf "$BASE_TREE"
    mkdir -p "$BASE_TREE"
    git archive "$BASE_SHA" | tar -x -C "$BASE_TREE"
    # shellcheck disable=SC2086
    meson setup "$BASE_TREE/build" "$BASE_TREE" $MESON_OPTS
    nice -n 10 ninja -C "$BASE_TREE/build" -j "$JOBS"
  fi

  pin_server
  # Two tunings, on purpose. identity.ini is the compiled defaults, the
  # standing control for a one-key A/B; pv3.ini is the A/B's own control
  # arm, where the whole diplomacy line (peace pre-pass, love gift,
  # proxy arming) is live and therefore where a default-0 knob has the
  # most to accidentally disturb. The mixed cell runs on pv3 because a
  # fence can only express where deep and stock coexist.
  echo "=== identity stage b: base vs head, same seeds both sides ==="
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7180;;
      head) bin=$SERVER;      port=7190;;
    esac
    TUNING=$ROOT/benchmark/tuning/identity.ini \
      ./benchmark/run.sh --count "$SEEDS" --arms deep \
        --config benchmark/configs/smoke.conf \
        --server "$bin" --jobs "$JOBS" --portbase "$port" \
        --out "$OUT/$side"
    TUNING=$ROOT/benchmark/tuning/pv3.ini \
      ./benchmark/run.sh --count 2 --arms mixed_dc \
        --config benchmark/configs/smoke.conf \
        --server "$bin" --jobs 2 --portbase $((port + 5)) \
        --out "$OUT/$side-mixed"
  done

  echo "=== identity stage c: provenance + scorelog assertion ==="
  if grep -rqm1 'deep-fence' "$OUT"/base/smoke-*/deep/seed-*/game.log; then
    echo "STOP: the BASE arm emits deep-fence, a tag only this branch" >&2
    echo "      has; the base build is the branch and the proof is void." >&2
    exit 1
  fi
  same=0; diff_n=0; missing=0; total=0
  check() { # base_cell head_cell arm seed
    total=$((total + 1))
    a=$1/$3/seed-$4/scorelog.txt
    b=$2/$3/seed-$4/scorelog.txt
    if [ ! -r "$a" ] || [ ! -r "$b" ]; then
      echo "$3 seed $4: MISSING scorelog"; missing=$((missing + 1))
    elif cmp -s "$a" "$b"; then
      same=$((same + 1))
    else
      echo "$3 seed $4: SCORELOGS DIFFER"; diff_n=$((diff_n + 1))
    fi
  }
  # Resolve each side's cell EXPLICITLY rather than by glob: the result
  # key includes the server sha, so a rebuilt head lands in a SECOND
  # cell beside the first, and a glob expanding to two paths makes every
  # seed read as MISSING -- a passing-looking failure of the wrong kind.
  bc=$(cell "$OUT/base" smoke); hc=$(cell "$OUT/head" smoke)
  bm=$(cell "$OUT/base-mixed" smoke); hm=$(cell "$OUT/head-mixed" smoke)
  echo "base cell: $bc / $bm"
  echo "head cell: $hc / $hm"
  [ -n "$bc" ] && [ -n "$hc" ] && [ -n "$bm" ] && [ -n "$hm" ] || {
    echo "STOP: could not resolve all four cells." >&2; exit 1; }
  [ "$bc" != "$hc" ] && [ "$bm" != "$hm" ] || {
    echo "STOP: a side resolved to the SAME cell as the other." >&2; exit 1; }
  i=0
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt | head -n "$SEEDS"); do
    check "$bc" "$hc" deep "$seed"
    i=$((i + 1))
    [ "$i" -le 2 ] && check "$bm" "$hm" mixed_dc "$seed"
  done
  echo "identity: $same/$total byte-identical, $diff_n differ, $missing missing"
  [ "$same" -eq "$total" ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

live)
  # The venue the knob was written for, with the knob on. TWO games
  # price nothing (rule 6) -- this reads counters only: does the fence
  # intercept at all, and does `would=` ever come back 1.
  OUT=$ROOT/benchmark/results/fence-live
  pin_server
  echo "=== liveness: 2 tech50war-200 mixed_dc games, knob on ==="
  TUNING=$ROOT/benchmark/tuning/fence-on.ini \
    ./benchmark/run.sh --count 2 --arms mixed_dc \
      --config benchmark/configs/tech50war-200.conf \
      --server "$SERVER" --jobs 2 --portbase 7176 --out "$OUT"
  grep -ho 'deep-fence .*' "$OUT"/tech50war-200-*/mixed_dc/seed-*/game.log \
    | head -5 || echo "NO deep-fence LINES -- the fence never fired"
  ;;

ab)
  # The arm the brief asks for: tech50war-200 mixed, >= 16 mirrored
  # seeds, knob 0 vs knob 1, one binary. tune.py --mixed seats deep and
  # ai/baseline in the SAME game and runs both seat mirrors per seed, so
  # the objective is the within-game deep-side minus stock-side total
  # and seat bias cancels. --report-holdout forces the holdout seed file
  # (00-context section 6: always report held out).
  OUT=$ROOT/benchmark/results/fence-ab
  COUNT=${COUNT:-16}
  pin_server
  mkdir -p "$OUT"
  for arm in off on; do
    case $arm in
      # 7110/7142 rather than 7100/7132: each arm needs COUNT x 2
      # consecutive ports (one per game, tune.py increments per game,
      # not per job), and 7100-7109 may still be held by a suspended
      # batch -- a socket outlives the process that stopped writing.
      off) ini=$ROOT/benchmark/tuning/pv3.ini;      port=7110;;
      on)  ini=$ROOT/benchmark/tuning/fence-on.ini; port=7152;;
    esac
    echo "=== A/B arm $arm: $ini ==="
    ./benchmark/tune.py --report-holdout "$ini" --mixed \
      --config benchmark/configs/tech50war-200.conf \
      --count "$COUNT" --jobs "$JOBS" --server "$SERVER" \
      --portbase "$port" --out "$OUT/$arm" 2>&1 | tee "$OUT/$arm.log"
  done
  ./benchmark/fencecmp.py "$OUT/off/holdout-report" "$OUT/on/holdout-report"
  touch "$OUT/ALLDONE"
  ;;

*)
  echo "usage: $0 {identity|live|ab}" >&2; exit 1;;
esac
