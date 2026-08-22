#!/usr/bin/env bash
# The corridor-power pilot: is there a venue in which a W12 corridor
# effect of a few score points is resolvable at a feasible n?
#
#   venue-corridor.sh pilot [SEEDS] [JOBS]
#   venue-corridor.sh report
#
# Two arms on the SAME seeds, one pinned binary, differing in exactly
# the five keys of the W12 corridor family:
#
#   on  = tuning/w12-div-on.ini      the probe values slices 1-4 ran at
#   off = tuning/w12-family-off.ini  the same five keys at their
#                                    compiled shipping zeros
#
# Both arms are all-deep games on configs/corridor-150.conf. run.sh
# content-addresses its results on (config, binary, runner, tuning), so
# the two arms land in two different directories under OUT and a
# re-invocation skips every game that already wrote a DONE marker --
# which is what makes this restartable after the ~60-minute background
# kill. Re-run the same command; it resumes.
#
# The report stage prints, in order:
#   * the corridor DOSE in this venue (links, gap tiles per link, ally
#     links, beyond-ceiling picks) -- the number lategame-80 lost on,
#   * paircmp.py's paired table for the two arms,
#   * the implied n to resolve a +3 score effect at |t| >= 2, computed
#     from the pilot's own paired sd.
#
# Ports 7500+. Pin your own binary with its libfreeciv.so and pass it
# as --server; do not point this at build/.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
OUT=${OUT:-$here/results/venue-pilot}
SERVER=${SERVER:-$ROOT/.stage-bin/freeciv-server}
CONFIG=$here/configs/corridor-150.conf
ON=$here/tuning/w12-div-on.ini
OFF=$here/tuning/w12-family-off.ini

stage=${1:-pilot}
SEEDS=${2:-8}
JOBS=${3:-4}

case "$stage" in
pilot)
  [ -x "$SERVER" ] || { echo "no pinned server at $SERVER" >&2; exit 1; }
  echo "=== corridor-150 pilot: $SEEDS mirrored seeds, jobs $JOBS ==="
  echo "server=$SERVER sha256=$(sha256sum "$SERVER" | cut -d' ' -f1)"
  for arm in off on; do
    tun=$OFF
    [ "$arm" = on ] && tun=$ON
    echo "--- arm $arm ($(basename "$tun")) ---"
    TUNING=$tun "$here/run.sh" --count "$SEEDS" --config "$CONFIG" \
      --arms deep --jobs "$JOBS" --portbase 7500 \
      --server "$SERVER" --out "$OUT/$arm" || exit 1
  done
  echo "=== pilot stages complete; run '$0 report' ==="
  ;;
report)
  offdir=$(ls -d "$OUT"/off/corridor-150-* 2>/dev/null | head -1)
  ondir=$(ls -d "$OUT"/on/corridor-150-* 2>/dev/null | head -1)
  [ -n "$offdir" ] && [ -n "$ondir" ] \
    || { echo "no pilot results under $OUT" >&2; exit 1; }

  echo "=== read-back audit: which tuning file each arm actually loaded ==="
  for d in "$offdir" "$ondir"; do
    for g in "$d"/deep/seed-*/game.log; do
      [ -f "$g" ] || continue
      grep -m1 -o 'deep-tuning file="[^"]*"' "$g"
    done | sort | uniq -c
  done

  echo
  echo "=== corridor dose in this venue (ON arm) ==="
  "$here/corridorcensus.py" "$ondir"
  echo
  echo "=== corridor dose in the OFF arm (must be a clean zero) ==="
  "$here/corridorcensus.py" "$offdir"

  echo
  echo "=== paired A/B ==="
  "$here/paircmp.py" "$offdir" "$ondir" \
    --pat trunk-turns="deep-trunk turn=" \
    --pat trunk-picks="deep-trunkpick turn=" \
    --pat ally-turns="deep-trunkally turn=" \
    --pat div-turns="deep-trunkdiv turn="

  echo
  echo "=== what effect size this venue can resolve ==="
  "$here/powern.py" "$offdir" "$ondir" --effect 3,4,6,10
  ;;
*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1
  ;;
esac
