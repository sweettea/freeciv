#!/usr/bin/env bash
# Re-take the 40-game knob-screen corpus under the RESCREEN
# instrumentation (W25, 2026-08-30).
#
# THIS IS NOT A TUNING RUN AND SELECTS NOTHING. It re-plays the exact
# 40 games `findings/knob-screen-2026-08-30.md` screened -- compiled
# defaults (benchmark/tuning/defaults.ini), contested.conf and
# lategame-80.conf, the 20 seeds of benchmark/seeds/holdout.txt --
# under a binary whose ai/deep log lines carry the fields the old
# corpus lacked (deep-decay-sum bund/cont/mult/grow, deep-bid
# milclass, the deep-milcap tag). The GAMES are identical; only the
# LOG is richer, which is why the scorelogs are checked byte for byte
# against the archived s6 corpus afterwards.
#
# It runs on holdout seeds and that is not a violation: tune.py's
# --report-holdout evaluates ONE fixed tuning file and has no
# optimiser, and the tuning file here is the compiled defaults. No
# parameter is chosen, ranked or moved by anything below.
#
# Baseline games are tuning- AND ai/deep-independent (the arm is
# ai/baseline, frozen), so s6's are hardlinked in rather than re-run,
# exactly as benchmark/nogates-holdout.sh does: tune.py skips any game
# dir carrying DONE. Only the 40 deep games are bought.
#
# Results land in the MAIN checkout, not in a worktree, so the corpus
# outlives the branch that took it.
#
#   benchmark/rescreen-corpus.sh /path/to/pinned/freeciv-server
#
set -eu

SERVER=${1:?pinned server binary (see CLAUDE.md: pin it, do not pass build/)}
SERVER=$(readlink -f "$SERVER")
[ -x "$SERVER" ] || { echo "not executable: $SERVER" >&2; exit 1; }

MAIN=${MAIN:-/home/sweettea/freeciv}
T=$MAIN/benchmark/results/tune-postfix
OUT=${OUT:-$MAIN/benchmark/results/w25-screen-corpus}
TUNING=$MAIN/benchmark/tuning/defaults.ini
PORTBASE=${PORTBASE:-7930}
JOBS=${JOBS:-4}
N="nice -n 10"

cd "$(dirname "$0")/.."

[ -f "$TUNING" ] || { echo "missing $TUNING" >&2; exit 1; }

for pair in war:s6-nogates-war econ:s6-nogates-econ; do
  new=${pair%%:*} old=${pair##*:}
  mkdir -p "$OUT/$new"
  [ -d "$OUT/$new/baseline" ] || cp -al "$T/$old/baseline" "$OUT/$new/baseline"
done

echo "=== rescreen corpus: contested ($(date)) ==="
$N python3 benchmark/tune.py --report-holdout "$TUNING" \
  --config benchmark/configs/contested.conf \
  --seeds benchmark/seeds/holdout.txt --count 20 \
  --portbase "$PORTBASE" --server "$SERVER" --jobs "$JOBS" \
  --out "$OUT/war"

echo "=== rescreen corpus: lategame-80 ($(date)) ==="
$N python3 benchmark/tune.py --report-holdout "$TUNING" \
  --config benchmark/configs/lategame-80.conf \
  --seeds benchmark/seeds/holdout.txt --count 20 \
  --portbase "$PORTBASE" --server "$SERVER" --jobs "$JOBS" \
  --out "$OUT/econ"

touch "$OUT/CORPUS-DONE"
echo "=== rescreen corpus complete ($(date)) ==="
