#!/usr/bin/env bash
# Paired benchmark batches: run every arm on the IDENTICAL seed list.
#
#   run.sh [--quick | --full | --count N] [--seeds FILE] [--config FILE]
#          [--arms A,B,...] [--jobs N] [--out DIR] [--server BIN]
#          [--portbase N]
#
#   --quick     first 10 seeds of the seed file (default)
#   --full      first 100 seeds
#   --seeds     seed list, one integer per line (default seeds/train.txt;
#               pass seeds/holdout.txt ONLY when reporting final numbers)
#   --config    game config (default configs/smoke.conf)
#   --arms      comma-separated aitypes to compare (default classic,baseline)
#   --jobs      parallel games (default: nproc)
#
# Results are content-addressed: they land in
# OUT/<config>-<key12>/<arm>/seed-<n>/, where the key is the sha256 of
# the config file, the server binary, and the tuning file (env TUNING,
# "none" until W25 lands). Completed games (DONE marker present) are
# skipped, so an interrupted batch resumes — and a change to the config,
# the binary, or the tuning changes the key, so stale results are never
# silently reused. OUT/<config>-<key12>/key.txt records the components.
#
# After the batch, the pairing check asserts that every arm saw the same
# map per seed (maphash.txt equality), then per-player final metrics are
# extracted to OUT/<config>-<key12>/metrics.csv (see extract.py).

set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

COUNT=10
SEEDS=$here/seeds/train.txt
CONFIG=$here/configs/smoke.conf
ARMS=classic,baseline
JOBS=$(nproc 2>/dev/null || echo 4)
OUT=$here/results
SERVER=
PORTBASE=5600

usage() { grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

while [ $# -gt 0 ]; do
  case "$1" in
    --quick)  COUNT=10; shift;;
    --full)   COUNT=100; shift;;
    --count)  COUNT=$2; shift 2;;
    --seeds)  SEEDS=$2; shift 2;;
    --config) CONFIG=$2; shift 2;;
    --arms)   ARMS=$2; shift 2;;
    --jobs)   JOBS=$2; shift 2;;
    --out)    OUT=$2; shift 2;;
    --server) SERVER=$2; shift 2;;
    --portbase) PORTBASE=$2; shift 2;;
    *) usage;;
  esac
done

[ -r "$SEEDS" ] || { echo "seed file not readable: $SEEDS" >&2; exit 1; }
[ -r "$CONFIG" ] || { echo "config not readable: $CONFIG" >&2; exit 1; }

cfgname=$(basename "$CONFIG" .conf)

# Content-address the result set by everything that determines a game's
# outcome besides the seed: game config, server binary, tuning file.
server_bin=${SERVER:-$here/../build/freeciv-server}
server_bin=$(readlink -f "$server_bin")
[ -r "$server_bin" ] || { echo "server not readable: $server_bin" >&2; exit 1; }
confhash=$(sha256sum "$CONFIG" | cut -d' ' -f1)
srvhash=$(sha256sum "$server_bin" | cut -d' ' -f1)
runhash=$(sha256sum "$here/run-game.sh" | cut -d' ' -f1)
if [ -n "${TUNING:-}" ]; then
  # A tuning file without the [tuning] section header loads as an EMPTY
  # override set: secfile lookups miss, every field keeps its compiled
  # default, and the run silently measures the defaults twice. That has
  # now cost one 40-game A/B (2026-08-04), which came back byte-identical
  # in both arms with the "off" arm still executing the feature. The
  # result key differs, so nothing downstream notices.
  grep -qE '^\[tuning\]' "$TUNING" || {
    echo "tuning file has no [tuning] section, so NOTHING in it would" \
         "be applied: $TUNING" >&2
    exit 1
  }
  # Same failure at key level: a name the loader never reads is a
  # silently inert line. tuning-params.ini is the registry tune-audit.py
  # checks the C side against.
  bad=$(grep -oE '^[a-z_0-9]+' "$TUNING" \
        | grep -vxFf <(grep -oE '^[a-z_0-9]+' "$here/tuning-params.ini") || true)
  [ -z "$bad" ] || {
    echo "tuning file sets keys the server does not know: $bad" >&2
    exit 1
  }
  tunhash=$(sha256sum "$TUNING" | cut -d' ' -f1)
else
  tunhash=none
fi
key=$(printf '%s %s %s %s\n' "$confhash" "$srvhash" "$runhash" "$tunhash" | sha256sum | cut -d' ' -f1)
cfgdir=$cfgname-${key:0:12}
mkdir -p "$OUT/$cfgdir"
{
  echo "config=$CONFIG sha256=$confhash"
  echo "server=$server_bin sha256=$srvhash"
  echo "runner=$here/run-game.sh sha256=$runhash"
  echo "tuning=${TUNING:-none} sha256=$tunhash"
} > "$OUT/$cfgdir/key.txt"

mapfile -t seeds < <(grep -E '^[0-9]+$' "$SEEDS" | head -n "$COUNT")
[ "${#seeds[@]}" -eq "$COUNT" ] || {
  echo "seed file has only ${#seeds[@]} of $COUNT requested seeds" >&2
  exit 1
}
IFS=, read -r -a arms <<< "$ARMS"

echo "config=$cfgdir arms=$ARMS seeds=${#seeds[@]} jobs=$JOBS out=$OUT"

joblist=$(mktemp)
trap 'rm -f "$joblist"' EXIT
total=0 skipped=0 port=$PORTBASE
for arm in "${arms[@]}"; do
  for seed in "${seeds[@]}"; do
    dir=$OUT/$cfgdir/$arm/seed-$seed
    total=$((total + 1))
    if [ -e "$dir/DONE" ]; then
      skipped=$((skipped + 1))
    else
      # Every concurrent server needs its own TCP port.
      printf '%s %s %s %s\n' "$seed" "$arm" "$dir" "$port" >> "$joblist"
      port=$((port + 1))
    fi
  done
done
echo "$total games; $skipped already done; $((total - skipped)) to run"

if [ -s "$joblist" ]; then
  xargs -P "$JOBS" -L 1 <"$joblist" \
    bash -c 'exec "$1" --config "$2" ${3:+--server "$3"} \
                       --seed "$4" --arm "$5" --out "$6" --port "$7"' \
    _ "$here/run-game.sh" "$CONFIG" "$SERVER" || true
fi

failed=0
for arm in "${arms[@]}"; do
  for seed in "${seeds[@]}"; do
    [ -e "$OUT/$cfgdir/$arm/seed-$seed/DONE" ] || {
      echo "MISSING: $arm seed-$seed"
      failed=$((failed + 1))
    }
  done
done

# Pairing check: every arm must have seen the same map for each seed.
pairfail=0
for seed in "${seeds[@]}"; do
  ref=
  for arm in "${arms[@]}"; do
    f=$OUT/$cfgdir/$arm/seed-$seed/maphash.txt
    [ -r "$f" ] || continue
    h=$(cat "$f")
    if [ -z "$ref" ]; then
      ref=$h
    elif [ "$h" != "$ref" ]; then
      echo "PAIRING VIOLATION: seed $seed maps differ across arms"
      pairfail=$((pairfail + 1))
    fi
  done
done

# Per-player final metrics for every completed game.
extractfail=0
if ls "$OUT/$cfgdir"/*/seed-*/DONE >/dev/null 2>&1; then
  "$here/extract.py" --root "$OUT/$cfgdir" --csv "$OUT/$cfgdir/metrics.csv" \
    || extractfail=1
fi

echo "----------------------------------------------------------------"
echo "completed: $((total - failed))/$total   pairing violations: $pairfail"
if [ "$failed" -gt 0 ] || [ "$pairfail" -gt 0 ] || [ "$extractfail" -gt 0 ]; then
  exit 1
fi
