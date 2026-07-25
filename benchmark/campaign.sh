#!/usr/bin/env bash
# The unattended 48-hour campaign: gates, weight ablations, tuning,
# holdout verdicts, acceptance runs, one summary report. Every stage
# is cached (DONE markers / tuning-keyed result dirs), so the whole
# script is idempotent - rerun it from the top after any interruption
# and it resumes. Designed to run under systemd/nohup with no agent
# attached.
#
#   campaign.sh [--jobs N] [--out DIR] [--phase-hours H]
#
# Stages:
#   0 gates     build check, corpus, quick smoke        (~15 min)
#   1 ablation  per-weight on/off, contested+primary
#     (12/6 seeds: candidates share seeds - common random numbers -
#      so paired ranking stays sharp at small n; holdout catches
#      few-map overfitting)
#   2 tune-A    scale-independent params, contested     (overlaps)
#   3 tune-B    site/horizon params, primary+isolated   (overlaps)
#   4 verdicts  holdout reports + long.conf + primary acceptance
#   5 report    report.md from every metric script

set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$here/.."

# All cores minus two for admin; per-game throughput dips slightly on
# SMT siblings but aggregate throughput wins.
JOBS=$(( $(nproc) - 2 ))
OUT=$here/results/campaign
while [ $# -gt 0 ]; do
  case "$1" in
    --jobs) JOBS=$2; shift 2;;
    --out)  OUT=$2; shift 2;;
    *) echo "unknown arg $1" >&2; exit 1;;
  esac
done
mkdir -p "$OUT"
log() { echo "[campaign $(date -u +%H:%M:%S)] $*" | tee -a "$OUT/campaign.log"; }
export DEEP_QUIET=1

SERVER=$here/../build/freeciv-server

# ---- stage 0: gates --------------------------------------------------
log "stage 0: gates"
ninja -C build > /dev/null || { log "BUILD FAILED"; exit 1; }
meson test -C build > /dev/null || { log "MESON TEST FAILED"; exit 1; }
"$here/corpus-run.sh" --port 5590 > "$OUT/corpus.log" 2>&1 \
  || { log "CORPUS FAILED (see corpus.log)"; exit 1; }
"$here/run.sh" --quick --arms baseline,deep --jobs "$JOBS" \
  --portbase 6500 > "$OUT/gate-smoke.log" 2>&1 \
  || { log "SMOKE GATE FAILED"; exit 1; }
log "gates green"

# ---- stage 1: weight ablations --------------------------------------
# One tuning file per weight, zeroing just that weight; plus all-off
# and all-on. Contested (20 seeds) + primary (10 seeds).
WEIGHTS="site_depth_weight site_potential_discount site_special_bonus
site_special_cluster site_claim_bonus site_pressure_factor
site_overlap_weight horizon_capacity_weight horizon_growth_weight
horizon_bundle_weight"
mkdir -p "$OUT/ablate"
: > "$OUT/ablate/all-off.ini.tmp"
echo "[tuning]" > "$OUT/ablate/all-off.ini"
for w in $WEIGHTS; do
  echo "$w = 0.0" >> "$OUT/ablate/all-off.ini"
  { echo "[tuning]"; echo "$w = 0.0"; } > "$OUT/ablate/no-$w.ini"
done
rm -f "$OUT/ablate/all-off.ini.tmp"

ABL_PAR=3   # ablation configs run concurrently; small per-batch game
            # counts otherwise leave most cores idle on long games
for cfg in contested:12:6600 primary:6:7300; do
  IFS=: read -r name count base <<< "$cfg"
  slot=0
  for ini in "$OUT"/ablate/*.ini REFERENCE; do
    if [ "$ini" = REFERENCE ]; then
      tag=defaults; tun=
    else
      tag=$(basename "$ini" .ini); tun=$ini
    fi
    log "ablation $name/$tag"
    TUNING=$tun "$here/run.sh" --count "$count" --arms baseline,deep \
      --config "$here/configs/$name.conf" --jobs $((JOBS / ABL_PAR)) \
      --portbase $((base + (slot % ABL_PAR) * 60)) \
      >> "$OUT/ablate/$name-$tag.log" 2>&1 \
      || log "WARN: ablation $name/$tag had failures" &
    slot=$((slot + 1))
    if [ $((slot % ABL_PAR)) -eq 0 ]; then
      wait
    fi
  done
  wait
done
log "ablations done"

# ---- stages 2+3: tuning campaigns -----------------------------------
TUNE_A_CAP=${TUNE_A_CAP:-12h}
TUNE_B_CAP=${TUNE_B_CAP:-16h}
log "tune-A: scale-independent params on contested (cap $TUNE_A_CAP)"
timeout "$TUNE_A_CAP" python3 "$here/tune.py" \
  --params settler_fearfulness_mult,cm_war_shield,cm_war_food,cm_cap_shield,cm_cap_trade,cm_happy_factor \
  --config "$here/configs/contested.conf" \
  --count 12 --gens 10 --pop 8 --jobs "$JOBS" --portbase 6800 \
  --out "$OUT/tune-A" > "$OUT/tune-A.log" 2>&1 \
  || log "WARN: tune-A exited nonzero"

log "tune-B: site/horizon params on primary+isolated (cap $TUNE_B_CAP)"
timeout "$TUNE_B_CAP" python3 "$here/tune.py" \
  --params site_depth_weight,site_tail_weight,site_overlap_weight,site_potential_discount,site_special_bonus,site_claim_bonus,site_pressure_factor,horizon_capacity_weight,horizon_bundle_weight \
  --config "$here/configs/primary.conf,$here/configs/isolated.conf" \
  --count 5 --gens 10 --pop 8 --jobs "$JOBS" --portbase 6900 \
  --out "$OUT/tune-B" > "$OUT/tune-B.log" 2>&1 \
  || log "WARN: tune-B exited nonzero"

# ---- stage 4: verdicts ----------------------------------------------
for t in A B; do
  best="$OUT/tune-$t/best-tuning.ini"
  if [ -r "$best" ]; then
    log "holdout report tune-$t"
    python3 "$here/tune.py" --report-holdout "$best" \
      --config "$here/configs/contested.conf,$here/configs/primary.conf" \
      --count 10 --jobs "$JOBS" --portbase 7000 \
      --out "$OUT/holdout-$t" > "$OUT/holdout-$t.log" 2>&1 || true
  fi
done

log "long.conf late-tech acceptance"
"$here/run.sh" --quick --arms baseline,deep \
  --config "$here/configs/long.conf" --jobs "$JOBS" --portbase 7100 \
  > "$OUT/long.log" 2>&1 || true

log "primary acceptance (30 seeds)"
"$here/run.sh" --count 30 --arms baseline,deep \
  --config "$here/configs/primary.conf" --jobs "$JOBS" --portbase 7200 \
  > "$OUT/primary.log" 2>&1 || true

# ---- stage 5: report -------------------------------------------------
log "writing report.md"
{
  echo "# Campaign report ($(date -u +%F))"
  echo
  echo "## Gates"; tail -3 "$OUT/gate-smoke.log"
  echo
  echo "## Ablations (see ablate/*.log; per-dir metrics in results/)"
  ls "$OUT/ablate" | grep '\.log$' | sed 's/^/- /'
  echo
  for t in A B; do
    echo "## tune-$t"; tail -5 "$OUT/tune-$t.log" 2>/dev/null
    echo "### holdout"; tail -3 "$OUT/holdout-$t.log" 2>/dev/null
    echo
  done
  echo "## Long-config first builds (deep arm)"
  latest_long=$(ls -td "$here"/results/long-* 2>/dev/null | head -1)
  [ -n "$latest_long" ] \
    && python3 "$here/firstbuild.py" --root "$latest_long" --arm deep
  echo
  echo "## Primary regime"
  latest_primary=$(ls -td "$here"/results/primary-* 2>/dev/null | head -1)
  [ -n "$latest_primary" ] && tail -3 "$OUT/primary.log"
} > "$OUT/report.md"
log "campaign complete: $OUT/report.md"
