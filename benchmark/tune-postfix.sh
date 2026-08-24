#!/usr/bin/env bash
# Post-fix multi-day retune (2026-08-23, quota conservation).
# Runs under systemd (user unit deep-tune.service) so it survives
# reboots and sleeps; every stage resumes from the content-addressed
# cache, so Restart=on-failure and reboots lose nothing but the game
# in flight.
#
# Why now: today's wave landed five adjudicated gates (trunk_upgrade,
# site_improvable, site_starve_price, poach_deploy, capital_stake) and
# the improvable epic flagged site_potential_weight for re-levelling.
# All prior tunings were fit on the PRE-fix binary; this campaign
# refits on the post-fix binary, gates on, from defaults
# (benchmark/tuning/postfix-start.ini).
#
# Stage shape is tune-3day.sh's proven design: MIL on contested/mixed
# (the only objective that punishes disarmament), ECON+G blend from
# its winner, MIL round 2 for the separability read, then pure holdout
# reports. ~1400 games, ~2.5 days at 10 jobs; jobs 8 here because the
# grow-worlds batch owns 2 slots for its first day.
#
# Pinned binary .tune-postfix-bin (sha 24867bef2364d609, post
# poach-fix merge 4172eea461). Ports 7700-7799. Everything nice 10
# (user directive: their interactive game keeps priority 0).

set -eu
cd "$(dirname "$0")/.."
SERVER=$PWD/.tune-postfix-bin/freeciv-server
T=benchmark/results/tune-postfix
J=8
N="nice -n 10"
mkdir -p "$T"

echo "=== stage 1: MIL on contested, mixed ($(date)) ==="
$N python3 benchmark/tune.py --blocks MIL --mixed \
  --config benchmark/configs/contested.conf \
  --count 8 --gens 4 --pop 10 --portbase 7700 \
  --start benchmark/tuning/postfix-start.ini \
  --server "$SERVER" --jobs $J --out $T/s1-mil-mixed

echo "=== stage 2: ECON+G on econ blend ($(date)) ==="
$N python3 benchmark/tune.py --blocks ECON,G \
  --config benchmark/configs/lategame-80.conf,benchmark/configs/isolated-150.conf \
  --count 8 --gens 3 --pop 10 --portbase 7700 \
  --start $T/s1-mil-mixed/best-tuning.ini \
  --server "$SERVER" --jobs $J --out $T/s2-econ

echo "=== stage 3: MIL round 2, separability read ($(date)) ==="
$N python3 benchmark/tune.py --blocks MIL --mixed \
  --config benchmark/configs/contested.conf \
  --count 8 --gens 3 --pop 10 --portbase 7700 \
  --start $T/s2-econ/best-tuning.ini \
  --server "$SERVER" --jobs $J --out $T/s3-mil-r2

echo "=== stage 4: holdout reports ($(date)) ==="
$N python3 benchmark/tune.py --report-holdout $T/s3-mil-r2/best-tuning.ini \
  --config benchmark/configs/contested.conf --seeds benchmark/seeds/holdout.txt \
  --count 20 --portbase 7700 --server "$SERVER" --jobs $J --out $T/s4-holdout-war
$N python3 benchmark/tune.py --report-holdout $T/s3-mil-r2/best-tuning.ini \
  --config benchmark/configs/lategame-80.conf --seeds benchmark/seeds/holdout.txt \
  --count 20 --portbase 7700 --server "$SERVER" --jobs $J --out $T/s4-holdout-econ

echo "=== all stages complete ($(date)) ==="
touch "$T/CAMPAIGN-DONE"
# Idle forever so systemd (Restart=on-failure) does not relaunch a
# finished campaign; the DONE marker is the completion signal.
exec sleep infinity
