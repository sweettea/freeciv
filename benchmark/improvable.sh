#!/usr/bin/env bash
# SITE-IMPROVABLE: the proofs this epic owes, re-runnable rather than
# described.
#
#   ./benchmark/improvable.sh identity   10 paired smoke seeds against
#                                        the branch point, in a deep
#                                        cell and a MIXED deep+classic
#                                        cell, everything at compiled
#                                        defaults (the gate shut)
#   ./benchmark/improvable.sh ab [N] [J] N (default 16) training seeds
#                                        on isolated-150, FOUR arms
#                                        (off / pricing / starve-price /
#                                        both), ONE pinned binary
#   ./benchmark/improvable.sh corpus     the USER'S OWN position (the
#                                        Carthago stall) replayed three
#                                        turns with both gates shut and
#                                        both open, one key apart: the
#                                        before/after drop-cause counts
#                                        on the map the epic exists for
#   ./benchmark/improvable.sh report     drop-cause counts per arm, the
#                                        improvable census, and the
#                                        paired score comparisons
#   ./benchmark/improvable.sh go         all four, in order, sequential
#
# Machine budget: ports 7600-7649, jobs <= 3, this worktree's own build/
# and its own pinned binary pair. The subcommands run SEQUENTIALLY and
# reuse port blocks accordingly; nothing here may run beside itself.
#
# WHY isolated-150. It is the expansion venue -- four seats, an island
# generator, 150 turns, migration on -- so it is the cell where "the
# site evaluator refused ground it should have taken" can show up as
# cities and as score rather than only as a log counter. It is also the
# cell the Carthago stall's symptom (a seat with room it would not
# settle) is a scaled-up instance of.
#
# WHY FOUR ARMS. The two halves of this epic push OPPOSITE ways on site
# value: the pricing half replaces an undiscounted upper bound with an
# honest discounted price and so takes want off tiles (measured: 42% of
# tile-prices cut, none lifted), while the starve half puts candidates
# back on the ladder that were being dropped. A two-arm off/on could
# only report their sum, and CLAUDE.md's rule about attribution is
# exactly this case -- effects that can interact want one factorial
# design, not two comparisons.
#
# Pre-registered predictions, in order of confidence:
#   1. identity: 10/10 byte-identical scorelogs in both cells
#   2. the off arm emits no deep-siteimprove line at all
#   3. arm S: starvepriced > 0 and the ladder's starve count FALLS
#   4. arm P: cut > 0 -- the honest price differs from the as-is one
#   5. arm S beats arm off on score; arm P is the one that could go
#      either way, and the AB cell says whether they interact
# All five are reported whatever they say.

set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)

BASE_SHA=c644628cb8                 # branch point of epic/site-improvable
BUILD=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.imp-base/base-$BASE_SHA
BASE_BUILD=$BASE_TREE/build/freeciv-server
BASE_PIN=$ROOT/.imp-bin/base/freeciv-server
ON=$here/tuning/improvable-on.ini
CONFIG=$here/configs/isolated-150.conf
# The user's Europe game, the turn the stall was reported from.
CORPUS=${CORPUS:-$ROOT/play-0823-1226/saves/freeciv-T0201-Y01500-auto.sav.zst}

MESON_OPTS="-Dclients= -Dfcmp= -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

# Pin the binary a run executes by copying it -- WITH libfreeciv.so,
# whose RUNPATH is $ORIGIN/ -- out of build/ before a single game
# starts. A re-pin of the SAME bytes must not copy: cp truncates and
# rewrites, which SIGBUSes every server already running from that path.
pin() {
  local src=$1 dst=$ROOT/.imp-bin/$2 f
  mkdir -p "$dst"
  for f in libfreeciv.so freeciv-server; do
    local from to
    from=$(dirname "$src")/$f
    to=$dst/$f
    [ "$f" = freeciv-server ] && from=$src
    if [ -f "$to" ] \
       && [ "$(sha256sum < "$from")" = "$(sha256sum < "$to")" ]; then
      continue
    fi
    cp -f "$from" "$to"
    if [ "$(sha256sum < "$from")" != "$(sha256sum < "$to")" ]; then
      echo "STOP: pin of $f did not verify" >&2
      exit 1
    fi
  done
  echo "$dst/freeciv-server"
}

case "${1:-}" in
identity)
  OUT=$here/results/improvable-identity
  SEEDS=10
  # The base binary was pinned from the branch-point build BEFORE a
  # line of this epic was written; it is used directly when present, and
  # rebuilt from the sha when it is not.
  if [ -x "$BASE_PIN" ]; then
    base_bin=$BASE_PIN
    echo "cached base pin: $base_bin"
  else
    if [ ! -x "$BASE_BUILD" ]; then
      rm -rf "$BASE_TREE"
      mkdir -p "$BASE_TREE"
      git -C "$ROOT" archive "$BASE_SHA" | tar -x -C "$BASE_TREE"
      # Source dir passed EXPLICITLY: `meson setup DIR` alone configures
      # the CWD, which in a worktree is the BRANCH -- the base build
      # would silently be the branch build and the proof would compare
      # HEAD with itself.
      # shellcheck disable=SC2086
      meson setup "$BASE_TREE/build" "$BASE_TREE" $MESON_OPTS || exit 1
      nice -n 10 ninja -C "$BASE_TREE/build" -j 4 || exit 1
    fi
    base_bin=$(pin "$BASE_BUILD" identity-base)
  fi
  head_bin=$(pin "$BUILD" identity-head)
  echo "base: $base_bin sha256=$(sha256sum "$base_bin" | cut -d' ' -f1)"
  echo "head: $head_bin sha256=$(sha256sum "$head_bin" | cut -d' ' -f1)"

  # No tuning file in either arm: the new key sits at its compiled 0,
  # which is the setting every existing run of this project is at and is
  # therefore the setting the identity claim has to be made about. The
  # mixed cell shares one server and one RNG stream between a deep seat
  # and a classic seat, which is the arm that catches an accidental
  # extra fc_rand().
  for side in base head; do
    bin=$base_bin
    [ "$side" = head ] && bin=$head_bin
    TUNING= nice -n 10 "$here/run.sh" --count "$SEEDS" --arms deep,mixed_dc \
      --config "$here/configs/smoke.conf" --server "$bin" --jobs 3 \
      --portbase 7600 --out "$OUT/$side" || exit 1
  done

  fail=0
  for arm in deep mixed_dc; do
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
  if grep -rqs "deep-siteimprove" "$OUT"/base/; then
    echo "STOP: the base arm emitted a SITE-IMPROVABLE token." >&2
    exit 1
  fi
  # ... and the gate really is shut in head.
  if grep -rqs "deep-siteimprove" "$OUT"/head/; then
    echo "STOP: head priced an improvable tile with no tuning file --" \
         "the gate is not shut at its compiled default." >&2
    exit 1
  fi
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  echo "IDENTITY OK"
  ;;

ab)
  OUT=$here/results/improvable-ab
  SEEDS=${2:-16}
  JOBS=${3:-3}
  bin=$(pin "$BUILD" ab)
  echo "=== SITE-IMPROVABLE 2x2: $SEEDS isolated-150 seeds, jobs $JOBS ==="
  echo "server=$bin sha256=$(sha256sum "$bin" | cut -d' ' -f1)"
  # The treatment arms first: if a mechanism does not fire on this venue
  # the comparison is void, and that is worth learning early.
  for arm in both starve pricing off; do
    tun=
    case $arm in
      pricing) tun=$here/tuning/improvable-on.ini;;
      starve)  tun=$here/tuning/improvable-starve.ini;;
      both)    tun=$here/tuning/improvable-both.ini;;
    esac
    echo "--- arm $arm (${tun:-compiled defaults}) ---"
    TUNING=$tun nice -n 10 "$here/run.sh" --count "$SEEDS" --config "$CONFIG" \
      --arms deep --jobs "$JOBS" --portbase 7620 \
      --server "$bin" --out "$OUT/$arm" || exit 1
  done
  echo "SUCCESS" > "$OUT/DONE"
  echo "=== arms complete; run '$0 report' ==="
  ;;

corpus)
  # THE POSITION THE EPIC EXISTS FOR. Deep_Carthago at ~23 cities,
  # every settler search losing 70-90 tiles to citymindist and 66-73 to
  # the starve gate while the user could see improvable ground. Played
  # on three turns twice, one key apart, from the identical save -- as
  # tight a pair as exists, and on the only map the complaint was
  # actually made about.
  #
  # The tuning is the user's OWN derby-7 file with the two gates
  # STRIPPED and then set, never appended: once a key is in that file a
  # plain append produces it twice, secfile_load refuses the WHOLE file,
  # and both arms silently run on compiled defaults with no derby-7
  # tuning at all (S-CAPSTAKE paid for this lesson). The awk below is
  # the check, and it must print nothing.
  OUT=$here/results/improvable-corpus
  END=${2:-204}
  bin=$(pin "$BUILD" corpus)
  echo "server=$bin sha256=$(sha256sum "$bin" | cut -d' ' -f1)"
  echo "corpus=$CORPUS"
  [ -r "$CORPUS" ] || { echo "no corpus save at $CORPUS" >&2; exit 1; }
  port=7646
  for arm in off on; do
    gate=0
    [ "$arm" = on ] && gate=1
    d=$OUT/$arm
    rm -rf "$d"; mkdir -p "$d/saves"
    zstdcat "$CORPUS" > "$d/game.sav" || exit 1
    { grep -v '^[[:space:]]*site_improvable_gate' \
           "$ROOT/derby5/tuning-derby7.ini" \
        | grep -v '^[[:space:]]*site_starve_price_gate'
      echo "site_improvable_gate = $gate"
      echo "site_starve_price_gate = $gate"
    } > "$d/tuning.ini"
    awk -F= '/^[a-z]/{gsub(/ /,"",$1); if (seen[$1]++) print "DUP:"$1}' \
        "$d/tuning.ini"
    # Every `set` goes AFTER the load: a savegame RESTORES server
    # settings, and this corpus was played with a human in it, so it
    # carries minplayers=1 and timeout=0 and a headless server that
    # sets those first has them overwritten and then waits forever.
    { echo "load $d/game.sav"
      zstdcat "$CORPUS" \
        | awk -F'"' '/^name=/{n=$2} /^username=/{if ($2 != "Unassigned")
                                                   print "aitoggle \"" n "\""}'
      echo "set minplayers 0"
      echo "set timeout -1"
      echo "set endturn $END"
      echo "set autosaves \"GAMEOVER\""
      echo "start"
    } > "$d/commands.serv"
    sha256sum "$bin" "$d/tuning.ini" > "$d/key.txt"
    ( cd "$d" \
      && FREECIV_DEEP_TUNING="$d/tuning.ini" LD_LIBRARY_PATH=$(dirname "$bin") \
         nice -n 10 timeout 7200 "$bin" -A none -e -p "$port" \
         --read commands.serv --saves saves > game.log 2>&1 ) &
    port=$((port + 1))
  done
  wait
  rm -f "$OUT"/*/game.sav "$OUT"/*/saves/*.sav*
  ok=1
  for arm in off on; do
    grep -aq "turn=$END" "$OUT/$arm/game.log" || ok=0
  done
  [ "$ok" -eq 1 ] && echo "SUCCESS" > "$OUT/DONE" \
                  || echo "FAILED" > "$OUT/FAILED"

  echo
  echo "=== read-back: the gates each arm actually ran with ==="
  for arm in off on; do
    printf '%-4s ' "$arm"
    grep -h "site_improvable_gate\|site_starve_price_gate" \
         "$OUT/$arm/tuning.ini" | tr '\n' ' '
    echo
  done
  echo
  echo "=== Deep_Carthago's site ladder, per arm, summed over the replay ==="
  printf '%-4s %7s %8s %8s %8s %8s %7s %7s\n' \
    arm seen nofound mindist starve scored neg found
  for arm in off on; do
    grep -ah 'deep-siteladder .*Carthago' "$OUT/$arm/game.log" \
      | awk -v lbl="$arm" '
          { for (i = 1; i <= NF; i++) { split($i, kv, "="); s[kv[1]] += kv[2] } }
          END {
            printf "%-4s %7d %8d %8d %8d %8d %7d %7d\n", lbl, s["seen"],
                   s["nofound"], s["mindist"], s["starve"], s["scored"],
                   s["neg"], s["found"]
          }'
  done
  echo
  echo "=== every deep seat, same table ==="
  printf '%-4s %7s %8s %8s %8s %8s %7s %7s\n' \
    arm seen nofound mindist starve scored neg found
  for arm in off on; do
    grep -ah 'deep-siteladder ' "$OUT/$arm/game.log" \
      | awk -v lbl="$arm" '
          { for (i = 1; i <= NF; i++) { split($i, kv, "="); s[kv[1]] += kv[2] } }
          END {
            printf "%-4s %7d %8d %8d %8d %8d %7d %7d\n", lbl, s["seen"],
                   s["nofound"], s["mindist"], s["starve"], s["scored"],
                   s["neg"], s["found"]
          }'
  done
  echo
  echo "=== the gates' own instrument ==="
  for arm in off on; do
    grep -ah "deep-siteimprove " "$OUT/$arm/game.log" \
      | awk -v lbl="$arm" '
          { for (i = 1; i <= NF; i++) { split($i, kv, "="); s[kv[1]] += kv[2] }
            n++ }
          END {
            printf "%-4s lines=%d tiles=%d lifted=%d cut=%d gain=%.0f"\
                   " chain=%d starvepriced=%d starveturns=%d starvehard=%d\n",
                   lbl, n, s["tiles"], s["lifted"], s["cut"], s["gain"],
                   s["chain"], s["starvepriced"], s["starveturns"],
                   s["starvehard"]
          }'
    [ "$(grep -ac 'deep-siteimprove ' "$OUT/$arm/game.log")" = 0 ] \
      && echo "$arm: no deep-siteimprove line at all"
  done
  echo
  echo "=== founder want, Deep_Carthago, last line per turn ==="
  for arm in off on; do
    printf '%-4s ' "$arm"
    grep -ah 'deep-founder .*Carthago.*fresh=1' "$OUT/$arm/game.log" \
      | grep -o 'want=[0-9-]*' | awk -F= '
          { s += $2; n++; if ($2 > 0) p++ }
          END { printf "lines=%d meanwant=%.1f positive=%d\n", n, s/n, p }'
  done
  ;;

report)
  OUT=$here/results/improvable-ab
  ARMS="off pricing starve both"
  for a in $ARMS; do
    d=$(ls -d "$OUT"/$a/isolated-150-* 2>/dev/null | head -1)
    [ -n "$d" ] || { echo "arm $a has no results under $OUT" >&2; exit 1; }
    eval "dir_$a=\$d"
  done

  echo "=== read-back audit: which tuning file each arm loaded ==="
  for a in $ARMS; do
    eval "d=\$dir_$a"
    printf '%-8s ' "$a"
    for g in "$d"/deep/seed-*/game.log; do
      [ -f "$g" ] || continue
      grep -m1 -o 'deep-tuning file="[^"]*"' "$g"
    done | sort | uniq -c | tr '\n' ' '
    echo
  done

  echo
  echo "=== the site ladder's drop causes, per arm, summed over games ==="
  printf '%-8s %10s %10s %10s %8s %10s %8s\n' \
    arm seen nofound mindist starve scored neg
  for a in $ARMS; do
    eval "d=\$dir_$a"
    grep -rh "deep-siteladder " "$d"/deep/seed-*/game.log 2>/dev/null \
      | awk -v lbl="$a" '
          {
            for (i = 1; i <= NF; i++) { split($i, kv, "="); s[kv[1]] += kv[2] }
          }
          END {
            printf "%-8s %10d %10d %10d %8d %10d %8d\n", lbl,
                   s["seen"], s["nofound"], s["mindist"], s["starve"],
                   s["scored"], s["neg"]
          }'
  done

  echo
  echo "=== the gates' own instrument (no line at all in the off arm) ==="
  for a in $ARMS; do
    eval "d=\$dir_$a"
    grep -rh "deep-siteimprove " "$d"/deep/seed-*/game.log 2>/dev/null \
      | awk -v lbl="$a" '
          {
            for (i = 1; i <= NF; i++) { split($i, kv, "="); s[kv[1]] += kv[2] }
            n++
          }
          END {
            printf "%-8s lines=%d tiles=%d lifted=%d cut=%d gain=%.0f"\
                   " chain=%d starvepriced=%d starveturns=%d starvehard=%d\n",
                   lbl, n, s["tiles"], s["lifted"], s["cut"], s["gain"],
                   s["chain"], s["starvepriced"], s["starveturns"],
                   s["starvehard"]
          }' || echo "$a: no deep-siteimprove line"
  done

  echo
  echo "=== outcome identity: which arms moved a seed at all? ==="
  for a in pricing starve both; do
    eval "d=\$dir_$a"
    same=0; total=0
    for x in "$dir_off"/deep/seed-*/scorelog.txt; do
      seed=$(basename "$(dirname "$x")")
      y=$d/deep/$seed/scorelog.txt
      [ -r "$y" ] || continue
      total=$((total + 1))
      cmp -s "$x" "$y" && same=$((same + 1))
    done
    echo "$a: $same/$total seeds byte-identical to off"
  done

  for a in pricing starve both; do
    eval "d=\$dir_$a"
    echo
    echo "=== paired A/B: off -> $a ==="
    "$here/paircmp.py" "$dir_off" "$d" \
      --pat improve-cut="deep-siteimprove .*cut=[1-9]" \
      --pat starve-priced="deep-siteimprove .*starvepriced=[1-9]"
  done
  ;;

go)
  # Everything, in order, sequentially -- so the box never sees more
  # than JOBS games from this epic at once. Launch detached from the
  # worktree root:
  #   setsid bash ./benchmark/improvable.sh go
  OUT=$here/results
  rm -f "$OUT/improvable-GO-DONE" "$OUT/improvable-GO-FAILED"
  ok=1
  for step in identity corpus ab; do
    echo "########## $step ##########"
    bash "$0" "$step" || ok=0
  done
  bash "$0" report || ok=0
  [ "$ok" -eq 1 ] && echo SUCCESS > "$OUT/improvable-GO-DONE" \
                  || echo FAILED > "$OUT/improvable-GO-FAILED"
  ;;

*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
