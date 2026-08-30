#!/usr/bin/env bash
# The designed group-screen campaign (W25, 2026-08-30).
#
# NOT LAUNCHED BY THE EPIC THAT WROTE IT. This script is the design,
# written down and budgeted, so the decision to buy ~6500 games is a
# decision somebody takes deliberately rather than one an agent takes
# by starting a loop.
#
# ==================================================================
# WHAT THIS IS, AND WHY IT IS SHAPED LIKE THIS
# ==================================================================
#
# The 2026-08-23 tune-postfix campaign moved 195 of 269 constants and
# its holdout came back -52.5 / -32.6. The post-mortem
# (findings/tune-postfix-verdict-2026-08-25.md) put that on two
# defects, and this campaign fixes both:
#
#   1. It selected the MAX of ~40 unreplicated draws from an objective
#      whose SE was ~41.  Here every block best is replicated:
#      --accept paired --accept-z 2 makes a challenger clear two
#      standard errors of its PAIRED diff against the incumbent, and
#      --reeval takes the block best from fresh seeds the search never
#      saw, with the incumbent re-measured on the same fresh sample.
#
#   2. It fitted ~80 free parameters against 8 noisy seeds.  Here the
#      widest block is 9 constants against 20 seeds, and the knob
#      screen (findings/knob-screen-2026-08-30.md, extended by
#      findings/screen-corpus-2026-08-30.md) chose them: every tier-A
#      constant has a validated replay rule and a measured flip rate,
#      and the 163 knobs that cannot be shown to move a single logged
#      decision in 40 games are PINNED, not mutated.
#
# Venue, from the same document's Part 2: screen on contested.conf
# --mixed at FULL length (the mixed venue's T100 surrogate r is +0.92
# / +0.93 because both arms share the game's shocks), report on the
# paired contested + lategame-80 holdout at full length.  T100
# truncation was measured and rejected: its efficiency bracket is
# 0.90-1.41, straddling 1.0.
#
# Two stages, because the tiers ask different questions:
#
#   Stage 1 FIT     AUCTION (9 knobs), DEFOPS (9).  Tier A: measured
#                   flip rates, worth fitting.  Deeper search.
#                   Eighteen constants, not the eleven this script
#                   was first written for: the corpus retake of
#                   2026-08-30 priced seven more (the four
#                   deep-decay-sum credit components and the three
#                   military scale keys) and all seven flip.  The
#                   two blocks are cut at the domestic/military
#                   MERGE.  It costs nothing: a block pass is
#                   pop x gens x seeds games whatever it moves.
#   Stage 2 SCREEN  BFEAR (9), BASSAULT (6), BSITE (6), BCIVIL (7).
#                   Tier B: the instrument fires but the log cannot
#                   price a flip.  The question is "does this
#                   SUBSYSTEM move score at all", and the answer a
#                   block returns is exactly that -- with --accept
#                   paired, a block that finds nothing significant
#                   returns its incumbent unchanged, which IS the
#                   screen's negative result.  Only a block that
#                   clears its own replicated test earns a later
#                   individual pass; nothing here fits a tier-B knob.
#
# Honest about the instrument: tune.py runs a (mu,lambda) ES per
# block, not a Plackett-Burman design.  What makes it a GROUP screen
# is the blocking and the acceptance rule, not the sampler.  A real
# fractional-factorial over the four tier-B blocks would answer the
# same question in fewer arms and nobody has written one.
#
# ==================================================================
# BUDGET -- from tune.py --dry-run --sigma-seed 104 (2026-08-30)
# ==================================================================
#
#   Stage 1  AUCTION  9 params, 50 candidates, <= 1656 games
#            DEFOPS   9 params, 50 candidates, <= 1656 games
#            subtotal                          <= 3312 games
#   Stage 2  BFEAR    9 params, 24 candidates, <=  784 games
#            BASSAULT 6 params, 24 candidates, <=  784 games
#            BSITE    6 params, 24 candidates, <=  784 games
#            BCIVIL   7 params, 24 candidates, <=  784 games
#            subtotal                          <= 3136 games
#   Holdout  2 configs x 20 seeds x 2 arms         80 games
#            (40 if the s4/s6 baseline dirs are hardlinked in, as
#             benchmark/nogates-holdout.sh does -- baseline games are
#             tuning-independent)
#
#   TOTAL <= 6448 campaign games + 80 report games = 6528
#   (the first draft of this header added the two subtotals wrong and
#    said 6608; 3312 + 3136 = 6448, and --dry-run prints both)
#   At the corpus median 192 s/game and --jobs 10, ~35 hours.
#   Cache hits make it fewer, never more.
#
#   Expected SE at the MEASURED mixed sigma, both stages
#   (findings/screen-corpus-2026-08-30.md Part 2: 1120 archived
#    --mixed games, 70 candidates x 8 seeds x 2 mirrored arms):
#
#     objective   +/- 23.3   (20 seeds x 1 config, sigma 104.4)
#     race rung   +/- 36.9   ( 8 seeds x 1 config, sigma 104.4)
#     reeval obj  +/- 36.9   ( 8 seeds x 1 config, sigma 104.4)
#     ACCEPTANCE  +/- 42.2   ( 8 fresh seeds, sigma 119.2)
#
#   READ THE LAST ROW BEFORE STARTING, because it is the one the
#   campaign actually gates on and the one the first draft of this
#   header did not have. --accept paired --accept-z 2 tests the
#   champion's PAIRED diff against the incumbent on the reeval
#   sample, and on the mixed venue that contrast is NOISIER than the
#   objective, not quieter: there is no shared baseline term to
#   cancel (the opponent is inside each game), so pairing removes
#   only the seed effect while differencing two nearly independent
#   games adds. Measured rho between two candidates at one seed is
#   0.16, and the paired sigma is 119.2 against the objective's
#   104.4.
#
#   So the bar a champion must clear is 2 x 42.2 = ~84 points, and
#   the effects on the table are the ~16-24 point gate contrasts from
#   2026-08-25. This budget can detect a LARGE block effect and
#   cannot resolve a small one; a block that returns its incumbent is
#   evidence of "not big", never evidence of "nothing".
#
#   Raising --reeval-seeds is the lever, at 2 games per candidate per
#   seed (train.txt holds 100 seeds and --count is 20, so up to 80 is
#   available without touching the seed file):
#
#     --reeval-seeds  8  ->  SE 42.2,  bar 84   (as written)
#     --reeval-seeds 16  ->  SE 29.8,  bar 60   (+64 games/generation)
#     --reeval-seeds 32  ->  SE 21.1,  bar 42   (+192 games/generation)
#
#   The previous draft of this header used sigma = 115, the per-seed
#   sigma of the PAIRED CONTESTED gap, because the mixed venue's own
#   sigma had never been measured. It has now: 104.4, i.e. the mixed
#   venue is not a quieter venue (paired contested is 94.9-115.3
#   across the s4/s5/s6 holdout arms). Its advantage is its T100
#   surrogate correlation, which is a different property.
#
# ==================================================================
# USAGE
# ==================================================================
#
#   benchmark/screen-campaign.sh /path/to/pinned/freeciv-server
#
# The binary MUST be pinned: copy a built freeciv-server somewhere no
# build will touch and pass that path (CLAUDE.md, "Always pin the
# binary for a benchmark in flight"). The script prints both stages'
# --dry-run budget and asks for confirmation before the first game.
#
set -eu

SERVER=${1:?pinned server binary (see CLAUDE.md: pin it, do not pass build/)}
cd "$(dirname "$0")/.."
SERVER=$(readlink -f "$SERVER")
[ -x "$SERVER" ] || { echo "not executable: $SERVER" >&2; exit 1; }

OUT=benchmark/results/screen-2026-08
CFG=benchmark/configs/contested.conf
TRAIN=benchmark/seeds/train.txt
HOLD=benchmark/seeds/holdout.txt
N="nice -n 10"
JOBS=${JOBS:-10}
PORTBASE=${PORTBASE:-7800}

# Shared discipline for both stages. Changing any of these changes
# the budget above: re-run --dry-run rather than guessing.
COMMON=(--config "$CFG" --mixed --seeds "$TRAIN" --count 20
        --accept paired --accept-z 2
        --race 8 --race-z 1.0
        --server "$SERVER" --jobs "$JOBS" --portbase "$PORTBASE")

S1=(--blocks AUCTION,DEFOPS --pop 10 --gens 5 --reeval 3 --reeval-seeds 8)
S2=(--blocks BFEAR,BASSAULT,BSITE,BCIVIL --pop 8 --gens 3
    --reeval 2 --reeval-seeds 8)

echo "=== BUDGET: stage 1 (fit, tier A) ==="
python3 benchmark/tune.py "${COMMON[@]}" "${S1[@]}" \
  --out "$OUT/s1-fit" --dry-run --sigma-seed 104
echo
echo "=== BUDGET: stage 2 (group screen, tier B) ==="
python3 benchmark/tune.py "${COMMON[@]}" "${S2[@]}" \
  --out "$OUT/s2-screen" --dry-run --sigma-seed 104
echo
echo "Holdout report at the end: 2 configs x 20 seeds x 2 arms = 80 games."
echo "Tier C (163 knobs the screen cannot show moving a decision) is"
echo "PINNED at compiled defaults for the whole campaign."
echo
printf 'Launch? this buys <= 6528 games, ~35h at --jobs %s [yes/NO] ' "$JOBS"
read -r reply
[ "$reply" = yes ] || { echo "not launched"; exit 0; }

mkdir -p "$OUT"

echo "=== stage 1: fit tier A on mixed contested ($(date)) ==="
$N python3 benchmark/tune.py "${COMMON[@]}" "${S1[@]}" --out "$OUT/s1-fit"

echo "=== stage 2: group-screen tier B, from stage 1's vector ($(date)) ==="
$N python3 benchmark/tune.py "${COMMON[@]}" "${S2[@]}" \
  --start "$OUT/s1-fit/best-tuning.ini" --out "$OUT/s2-screen"

# ------------------------------------------------------------------
# REPORT ONLY. Nothing below selects anything: --report-holdout
# evaluates one fixed tuning on seeds the search never saw, on the
# paired venue rather than the mixed one it was screened on. If it
# disagrees with the training numbers, the training numbers are wrong
# -- that is what the holdout is for, and 2026-08-25 is what it looks
# like when the holdout is believed.
# ------------------------------------------------------------------
BEST=$OUT/s2-screen/best-tuning.ini

echo "=== holdout report: contested ($(date)) ==="
$N python3 benchmark/tune.py --report-holdout "$BEST" \
  --config "$CFG" --seeds "$HOLD" --count 20 \
  --portbase "$PORTBASE" --server "$SERVER" --jobs "$JOBS" \
  --out "$OUT/holdout-war"

echo "=== holdout report: lategame-80 ($(date)) ==="
$N python3 benchmark/tune.py --report-holdout "$BEST" \
  --config benchmark/configs/lategame-80.conf --seeds "$HOLD" --count 20 \
  --portbase "$PORTBASE" --server "$SERVER" --jobs "$JOBS" \
  --out "$OUT/holdout-econ"

touch "$OUT/CAMPAIGN-DONE"
echo "=== screen campaign complete ($(date)) ==="
