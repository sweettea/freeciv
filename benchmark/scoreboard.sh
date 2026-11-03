#!/usr/bin/env bash
# The standing scoreboard: ONE fixed measurement of where ai/deep
# stands, re-run on every merge (design, 2026-10-28:
# doc/deep-ai/findings/scoreboard-design-2026-10-28.md).
#
# NOT LAUNCHED BY THE SESSION THAT WROTE IT. Like screen-campaign.sh,
# this is the design written down and budgeted; buying the games is a
# decision the user takes by typing `yes`.
#
#   benchmark/scoreboard.sh PINNED_SERVER [--jobs N] [--out DIR]
#                           [--with-vector FILE.ini] [--classic]
#                           [--dry-run] [--yes] [--report]
#
#   --dry-run       print the budget and the pin, launch nothing,
#                   write nothing.
#   --yes           the typed confirmation, for a detached launch
#                   (setsid has no terminal to answer the prompt).
#                   Pass it only on the user's explicit go.
#   --report        only (re)write <out>/SCOREBOARD.md from what is
#                   on disk; no games.
#   --with-vector   run the deep cells again under this tuning file
#                   (e.g. benchmark/tuning/stacked-on.ini), so a
#                   candidate vector gets the same scoreboard. The
#                   compiled-defaults cells always run too.
#   --classic       add the classic generalisation cell (theory-of-play
#                   section H): contested-classic.conf, self-play, n=20.
#   --europe        add the legibility venue: europe-150.conf (the Europe
#                   scenario, 14 nations, classic, T150), self-play and
#                   mixed 7/7, n=20. Games are ~EUROPE_SECS_PER_GAME long.
#
# ============================== WHAT IT MEASURES ==============================
#
# Two axes, because they are different questions (theory-of-play
# section H) and every number the project had before this was only one
# of them, on seeds that had stopped being held out:
#
#   SOCIETAL     self-play deep vs self-play baseline, paired by seed:
#                per-seat score and the paircmp columns, plus the
#                T50/T100 doctrine panel (panel.py). This is the
#                00-context section 6 primary metric. It cannot see a
#                zero-sum transfer.
#   COMPETITIVE  mixed games, deep and baseline seated alternately in
#                ONE game (4/4 on contested, 3/3 on lategame-80), each
#                seed played twice with the seats mirrored (run-game.sh
#                --arm mixed_db / mixed_bd, the seat map tune.py --mixed
#                uses). Read by rankcmp.py as P(a random deep seat
#                outranks a random baseline seat), normalized rank, win
#                share and per-seat score. It sees taking AND denying.
#
# Venues: contested.conf (8p, T150, war) and lategame-80.conf (6p,
# T80, econ) -- the two venues every holdout report since 2026-08 has
# used, so the scoreboard's societal axis is continuous with them.
#
# Seeds: 121-160 of benchmark/seeds/holdout.txt. 101-120 have been
# read as a development set (s4/s6 holdouts, the panel, the stacked
# holdout read) and are burned; train.txt (1-100) adjudicates and may
# never report. 40 seeds, because the paired per-seat score SE is
# ~1.8 at n=40 (below) and 161-200 must stay virgin for the day
# 121-160 burn too. The classic cell uses 121-140 (n=20: the
# theory-of-play section H prescription for a generalisation check).
#
# ================================ THE BINARY =================================
#
# The pin is the server AND the libfreeciv.so beside it: the server's
# RUNPATH is $ORIGIN/, so the library next to the binary is the code
# that plays (readelf -d .w25-bin/freeciv-server, 2026-10-28). The
# scoreboard's identity is sha256 of both; run-game.sh's provenance
# hashes only the server, which does not cover a rebuilt library.
# A path under any build/ directory is refused (CLAUDE.md: pin, do
# not pass build/).
#
# ============================ BASELINE GAME CACHE ============================
#
# Baseline self-play is tuning-independent (no deep seat plays), so it
# is cached under benchmark/results/scoreboard/baseline-cache, keyed on
# the venue's config + run-game.sh content, and hardlinked in (the
# nogates-holdout.sh pattern). But it is NOT binary-independent: server/,
# common/ and advisors/ changes reach ai/baseline games (W28/W29 are
# advisor changes). So a cache generation is CERTIFIED per pin identity:
# a pin not yet certified replays the first three seeds and byte-compares
# their scorelogs with the cache (`id` line excluded, the stacked-lever
# identity proof); identical -> certify and hardlink the other 37;
# any difference -> play all 40 fresh and start a new generation.
#
# ============================== GUARDS =======================================
#
# Every finished game is checked BEFORE its log is pruned, and a failed
# check writes <out>/STOP, which halts further launches:
#   - commands.serv seats exactly the modules the arm says;
#   - a deep-bearing game logs deep- lines, a baseline game logs none
#     (pre-prune grep; s4 baseline game.logs have zero, 2026-10-28);
#   - compiled-defaults games log NO deep-tuning line (deeptuning.c
#     logs one only when a file loads); vector games log one naming the
#     vector's absolute path, and no "cannot load tuning file".
#
# Then the game.log is cut to error lines + the tuning line and
# zstd'd, and saves/ is deleted, AS EACH GAME FINISHES: /home was 96%
# full (6.3G free) on 2026-10-28, and a raw deep contested game.log is
# hundreds of MB (stacked-lever.sh header).
#
# ============================== LAYOUT =======================================
#
#   <out>/BINARY.txt                 the pin, both shas, identity
#   <out>/<label>/<venue>/selfplay/{deep,baseline}/seed-N/
#   <out>/<label>/<venue>/mixed/{mixed_db,mixed_bd}/seed-N/
#   <out>/SCOREBOARD.md              rankcmp.py's report
#
# <label> is `defaults` or `vector-<ini basename>`; baseline self-play
# lives only under defaults (it does not depend on the vector). The
# .../<arm>/seed-N/scorelog.txt shape is what panel.py, extract.py and
# rankcmp.py all read.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT" || exit 1

die() { echo "scoreboard: $*" >&2; exit 1; }
usage() { sed -n '2,/^set -uo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 1; }

[ $# -ge 1 ] || usage
case $1 in -*) usage;; esac
SERVER=$1; shift
OUT= VECTOR= CLASSIC=0 EUROPE=0 DRY=0 YES=0 REPORT_ONLY=0
JOBS=6          # brief, 2026-10-28: JOBS default 6 (the user plays on this box)
NICE=10         # CLAUDE.md / every driver since stacked-lever.sh: nice 10
PORT0=7600      # brief: port block 7600+; the campaign took 7800/8000+,
                # the stacked cell 7900 (stacked-lever.sh), nogates 7700
PORTMAX=7639    # so at most 40 slots before touching 7640+ (kept free)
SECS_PER_GAME=192  # corpus median s/game (screen-campaign.sh budget).
EUROPE_SECS_PER_GAME=1900  # europe-150: 14 nations to T150 on the
                # 2026-11-02 pin: the T1-20 smoke game took 169 s beside 10 other
                # jobs (8.5 s/turn); later turns are slower, so ~32 min a game is
                # the budget until a full cell measures it (configs/europe-150.conf).
                # Cross-check 2026-10-28 from DONE - commands.serv mtimes:
                # stacked war/off deep 172, econ/off deep 217, screen mixed
                # contested 185, s4 baseline war 160 / econ 200.
while [ $# -gt 0 ]; do
  case $1 in
    --out)         OUT=$2; shift 2;;
    --jobs)        JOBS=$2; shift 2;;
    --with-vector) VECTOR=$2; shift 2;;
    --classic)     CLASSIC=1; shift;;
    --europe)      EUROPE=1; shift;;
    --dry-run)     DRY=1; shift;;
    --yes)         YES=1; shift;;
    --report)      REPORT_ONLY=1; shift;;
    *) usage;;
  esac
done
case $JOBS in ''|*[!0-9]*) die "--jobs must be a positive integer";; esac
[ "$JOBS" -ge 1 ] && [ $((PORT0 + JOBS - 1)) -le "$PORTMAX" ] \
  || die "--jobs $JOBS needs ports $PORT0..$((PORT0 + JOBS - 1)); block ends at $PORTMAX"

# ------------------------------------------------------------------ the pin
SERVER=$(readlink -f "$SERVER") || die "no such server"
[ -x "$SERVER" ] || die "not executable: $SERVER"
case $SERVER in
  */build/*) die "refusing $SERVER: copy the binary AND libfreeciv.so to a
  pinned directory no build touches (CLAUDE.md, pinning rule)";;
esac
LIB=$(dirname "$SERVER")/libfreeciv.so
[ -f "$LIB" ] || die "no libfreeciv.so beside $SERVER (RUNPATH is \$ORIGIN/: pin both)"
got=$(ldd "$SERVER" 2>/dev/null | awk '/libfreeciv\.so/ {print $3}')
[ "$(readlink -f "${got:-/nonexistent}")" = "$(readlink -f "$LIB")" ] \
  || die "$SERVER resolves libfreeciv.so to '${got:-nothing}', not $LIB"
SSHA=$(sha256sum "$SERVER" | cut -d' ' -f1)
LSHA=$(sha256sum "$LIB" | cut -d' ' -f1)
PIN=$(printf '%s\n%s\n' "$SSHA" "$LSHA" | sha256sum | cut -d' ' -f1)
: "${OUT:=$ROOT/benchmark/results/scoreboard/run-${PIN:0:12}}"
OUT=$(readlink -m "$OUT")
CACHE_ROOT=$ROOT/benchmark/results/scoreboard/baseline-cache

# ------------------------------------------------------------------ seeds
SEEDS=$(seq 121 160)          # holdout 121-160; 101-120 burned (header)
CLASSIC_SEEDS=$(seq 121 140)  # n=20, theory-of-play section H
EUROPE_SEEDS=$(seq 121 140)   # n=20: the games are 5-10x longer
PROBE_SEEDS="121 122 123"     # 3 identity seeds, as stacked-lever.sh's proof
for s in $SEEDS; do
  awk -v s="$s" '$1==s {f=1} END {exit !f}' benchmark/seeds/holdout.txt \
    || die "seed $s is not in holdout.txt"
  awk -v s="$s" '$1==s {f=1} END {exit !f}' benchmark/seeds/train.txt \
    && die "seed $s is a TRAINING seed; the scoreboard reports, it never adjudicates"
done

# ------------------------------------------------------------------ cells
LABELS=defaults
if [ -n "$VECTOR" ]; then
  VECTOR=$(readlink -f "$VECTOR") && [ -f "$VECTOR" ] || die "no vector file"
  VLABEL=vector-$(basename "$VECTOR" .ini)
  VSHA=$(sha256sum "$VECTOR" | cut -d' ' -f1)
  LABELS="defaults $VLABEL"
fi
VENUES="contested lategame-80"
[ "$CLASSIC" = 1 ] && VENUES="$VENUES contested-classic"
[ "$EUROPE" = 1 ] && VENUES="$VENUES europe-150"

cfg_of() {
  case $1 in
    contested)         echo "$here/configs/contested.conf";;
    lategame-80)       echo "$here/configs/lategame-80.conf";;
    contested-classic) echo "$here/configs/contested-classic.conf";;
    europe-150)        echo "$here/configs/europe-150.conf";;
  esac
}
seeds_of() {
  case $1 in
    contested-classic) echo $CLASSIC_SEEDS;;
    europe-150)        echo $EUROPE_SEEDS;;
    *)                 echo $SEEDS;;
  esac
}
secs_of() { [ "$1" = europe-150 ] && echo $EUROPE_SECS_PER_GAME || echo $SECS_PER_GAME; }
has_mixed() { [ "$1" != contested-classic ]; }   # classic: self-play only
tune_of() { [ "$1" = defaults ] && echo "" || echo "$VECTOR"; }
for v in $VENUES; do [ -r "$(cfg_of "$v")" ] || die "missing $(cfg_of "$v")"; done

game_dir() { # label venue arm seed
  case $3 in
    mixed_*) echo "$OUT/$1/$2/mixed/$3/seed-$4";;
    *)       echo "$OUT/$1/$2/selfplay/$3/seed-$4";;
  esac
}

# ------------------------------------------------------------------ cache
cache_key() { cat "$(cfg_of "$1")" "$here/run-game.sh" | sha256sum | cut -c1-12; }
cache_dir() { echo "$CACHE_ROOT/$1-$(cache_key "$1")"; }
gen_complete() { # gen venue
  local s; for s in $(seeds_of "$2"); do [ -f "$1/seed-$s/DONE" ] || return 1; done
}
cache_state() { # venue -> "certified GEN" | "probe GEN" | "none"
  local d g newest=; d=$(cache_dir "$1")
  for g in $(ls -td "$d"/gen-* 2>/dev/null); do
    gen_complete "$g" "$1" || continue
    [ -z "$newest" ] && newest=$g
    if grep -q "^$PIN " "$g/CERTIFIED" 2>/dev/null; then echo "certified $g"; return; fi
  done
  if [ -n "$newest" ]; then echo "probe $newest"; else echo none; fi
}

# ------------------------------------------------------------------ budget
count_games() { # -> "total todo todo_secs"
  local total=0 todo=0 secs=0 l v s a
  for l in $LABELS; do for v in $VENUES; do
    for s in $(seeds_of "$v"); do
      for a in deep $(has_mixed "$v" && echo mixed_db mixed_bd); do
        total=$((total + 1))
        [ -f "$(game_dir "$l" "$v" "$a" "$s")/DONE" ] \
          || { todo=$((todo + 1)); secs=$((secs + $(secs_of "$v"))); }
      done
    done
  done; done
  echo "$total $todo $secs"
}
hours() { awk -v s="$1" -v j="$JOBS" 'BEGIN {printf "%.1f", s/j/3600}'; }

print_budget() {
  local v st n bmin=0 bmax=0 smin=0 smax=0 nsecs nonbase total todo
  echo "=== PIN ==="
  echo "  server        $SERVER"
  echo "    sha256      $SSHA"
  echo "  libfreeciv.so $LIB"
  echo "    sha256      $LSHA"
  echo "  pin identity  $PIN"
  [ -n "$VECTOR" ] && echo "  vector        $VECTOR sha256 $VSHA"
  echo "  out           $OUT"
  echo "  ports         $PORT0..$((PORT0 + JOBS - 1)) (one per job slot)  nice $NICE  jobs $JOBS"
  echo
  echo "=== CELLS (seeds $(echo $SEEDS | awk '{print $1"-"$NF}'); classic $(echo $CLASSIC_SEEDS | awk '{print $1"-"$NF}')) ==="
  printf '  %-36s %-18s %6s\n' label/venue cell games
  for l in $LABELS; do for v in $VENUES; do
    n=$(seeds_of "$v" | wc -w)
    printf '  %-36s %-18s %6d\n' "$l/$v" "self-play deep" "$n"
    if has_mixed "$v"; then
      printf '  %-36s %-18s %6d\n' "$l/$v" "mixed (x2 mirror)" $((2 * n))
    fi
  done; done
  echo
  echo "=== BASELINE SELF-PLAY (cached per pin; tuning-independent) ==="
  for v in $VENUES; do
    n=$(seeds_of "$v" | wc -w)
    set -- $(cache_state "$v"); st=$1
    case $st in
      certified) printf '  %-18s certified for this pin  -> 0 games (hardlinked)\n' "$v";;
      probe)     printf '  %-18s cache exists, pin not certified -> 3 probe games, +%d if the probe differs\n' "$v" $((n - 3))
                 bmin=$((bmin + 3)); bmax=$((bmax + n))
                 smin=$((smin + 3 * $(secs_of "$v"))); smax=$((smax + n * $(secs_of "$v")));;
      none)      printf '  %-18s no cache -> %d games (then cached)\n' "$v" "$n"
                 bmin=$((bmin + n)); bmax=$((bmax + n))
                 smin=$((smin + n * $(secs_of "$v"))); smax=$((smax + n * $(secs_of "$v")));;
    esac
  done
  set -- $(count_games); total=$1; nonbase=$2; nsecs=$3
  echo
  echo "=== BUDGET ==="
  echo "  deep-bearing games  $total ($nonbase not yet DONE)"
  echo "  baseline games      $bmin..$bmax"
  echo "  TO BUY              $((nonbase + bmin))..$((nonbase + bmax)) games"
  echo "  wall clock          $(hours $((nsecs + smin)))..$(hours $((nsecs + smax))) h at ${SECS_PER_GAME}s/game (europe-150 ${EUROPE_SECS_PER_GAME}s), --jobs $JOBS"
  echo
  echo "=== EXPECTED SEs at n=40 (per seat, paired by seed) ==="
  # panel-2026-10-27.md section 1 and 4: score delta se 2.61 (contested
  # T150) and 2.48 (lategame-80 T80) at n=20 holdout -> x sqrt(20/40).
  echo "  self-play score   contested ~1.85   lategame-80 ~1.75"
  # screen-corpus-2026-08-30.md Part 2: pooled mixed per-seed sigma
  # 104.4 on the sum over both mirrors of (deep total - base total),
  # i.e. 8 deep minus 8 baseline seats -> 13.05 per seat / sqrt(40).
  echo "  mixed score       contested ~2.06   lategame-80 unmeasured"
  echo "  rank AUC          unmeasured: rankcmp.py prints it from the per-seed spread"
  echo
  local free; free=$(df -Pk "$ROOT/benchmark/results" | awk 'NR==2 {printf "%.1f", $4/1048576}')
  echo "  disk free under benchmark/results: ${free}G (need ~1G per job slot"
  echo "  in flight for raw logs, ~0.6M per finished game after pruning)"
}

if [ "$REPORT_ONLY" = 1 ]; then
  python3 "$here/rankcmp.py" "$OUT" --md "$OUT/SCOREBOARD.md" \
    && echo "wrote $OUT/SCOREBOARD.md"
  exit
fi

print_budget
[ "$DRY" = 1 ] && { echo; echo "dry run: nothing launched, nothing written"; exit 0; }

if [ "$YES" != 1 ]; then
  printf '\nLaunch? this buys the games above [yes/NO] '
  read -r reply || reply=
  [ "$reply" = yes ] || { echo "not launched"; exit 0; }
fi

# ================================================================== RUN
mkdir -p "$OUT" "$CACHE_ROOT"
# ONE DRIVER PER OUT, and one per cache: two drivers on one output dir
# and one port block cost a whole batch on 2026-08-23.
exec 9>"$OUT/.driver.lock"
flock -n 9 || die "another scoreboard driver holds $OUT/.driver.lock"
exec 8>"$CACHE_ROOT/.lock"
flock -n 8 || die "another scoreboard driver holds the baseline cache lock"
# No `exec > >(tee ...)` here: bash's bare `wait` also waits for the
# last process substitution, and tee never exits -- the driver hung at
# its first wait in the mock run of 2026-10-28. Redirect the driver's
# own output at launch instead (the design doc's launch command does).

# A resumed run must be the same pin (and the same vector) it started as.
if [ -f "$OUT/BINARY.txt" ]; then
  grep -q "^identity $PIN$" "$OUT/BINARY.txt" \
    || die "$OUT was started with a different pin; see $OUT/BINARY.txt"
else
  { echo "server $SERVER $SSHA"; echo "lib $LIB $LSHA"; echo "identity $PIN"
    echo "started $(date -Is)"; } > "$OUT/BINARY.txt"
fi
if [ -n "$VECTOR" ]; then
  mkdir -p "$OUT/$VLABEL"
  if [ -f "$OUT/$VLABEL/VECTOR.txt" ]; then
    grep -q " $VSHA$" "$OUT/$VLABEL/VECTOR.txt" \
      || die "$OUT/$VLABEL was started with a different $VLABEL.ini"
  else
    echo "$VECTOR $VSHA" > "$OUT/$VLABEL/VECTOR.txt"
  fi
fi
rm -f "$OUT/STOP"

# Lines worth keeping from a game.log: errors (levels 0-1), the tuning
# line (proof of what the deep seats played), the tuning-load failure.
KEEP='^[01]: |deep-tuning file=|cannot load tuning file'

guard() { # dir label arm -> writes guard.txt; appends to STOP on failure
  local d=$1 label=$2 arm=$3 tune log=$1/game.log nd nt nv=0 nf why=
  tune=$(tune_of "$label")
  nd=$(grep -ac 'deep-[a-z]' "$log")
  nt=$(grep -ac 'deep-tuning file=' "$log")
  nf=$(grep -ac 'cannot load tuning file' "$log")
  [ -n "$tune" ] && nv=$(grep -acF "deep-tuning file=\"$tune\"" "$log")
  local mods; mods=$(awk '/^create P/ {printf "%s%s", sep, $3; sep=","}' "$d/commands.serv")
  local p1=${mods%%,*} rest=${mods#*,}; local p2=${rest%%,*}
  case $arm in
    deep)     echo ",$mods," | grep -qv ',baseline,' && [ "$p1" = deep ] || why="$why seats=$mods";;
    baseline) echo ",$mods," | grep -qv ',deep,' && [ "$p1" = baseline ] || why="$why seats=$mods";;
    mixed_db) [ "$p1" = deep ] && [ "$p2" = baseline ] || why="$why seats=$mods";;
    mixed_bd) [ "$p1" = baseline ] && [ "$p2" = deep ] || why="$why seats=$mods";;
  esac
  if [ "$arm" = baseline ]; then
    [ "$nd" = 0 ] || why="$why baseline-game-logs-deep-lines=$nd"
  else
    [ "$nd" -gt 0 ] || why="$why no-deep-lines"
    [ "$nf" = 0 ] || why="$why tuning-load-failed=$nf"
    if [ -n "$tune" ]; then
      [ "$nv" -ge 1 ] && [ "$nt" = "$nv" ] || why="$why vector-not-loaded(nt=$nt nv=$nv)"
    else
      [ "$nt" = 0 ] || why="$why defaults-game-loaded-a-tuning-file=$nt"
    fi
  fi
  echo "deep_lines=$nd tuning_lines=$nt vector_lines=$nv tuning_fail=$nf seats=$mods verdict=${why:+FAIL}${why:-OK}${why:+ why=$why}" > "$d/guard.txt"
  if [ -n "$why" ]; then
    echo "$d:$why" >> "$OUT/STOP"
    echo "STOP: guard failed at $d:$why" >&2
  fi
}

one() { # label venue arm seed port
  local label=$1 venue=$2 arm=$3 seed=$4 port=$5 d tune
  d=$(game_dir "$label" "$venue" "$arm" "$seed")
  tune=$(tune_of "$label")
  # A partial directory from a kill is deleted, never resumed: a
  # leftover scorelog.txt silently disables scorelogging (STATUS
  # 2026-08-13, stacked-lever.sh).
  rm -rf "$d"; mkdir -p "$d"
  env -u TUNING -u ARENA -u FREECIV_DEEP_TUNING -u FREECIV_DEEP_ARENA \
      -u DEEP_QUIET -u KEEP_SAVES ${tune:+TUNING="$tune"} \
    nice -n "$NICE" "$here/run-game.sh" --config "$(cfg_of "$venue")" \
      --seed "$seed" --arm "$arm" --out "$d" --server "$SERVER" \
      --port "$port" > "$d/runner.log" 2>&1
  if [ ! -f "$d/DONE" ]; then
    echo "FAILED $label $venue $arm seed $seed $(date -Is)" >> "$OUT/failures.log"
    return
  fi
  guard "$d" "$label" "$arm"
  grep -aE "$KEEP" "$d/game.log" > "$d/game.log.new"
  mv "$d/game.log.new" "$d/game.log"
  zstd -q --rm -f "$d/game.log"
  rm -rf "$d/saves"
  touch "$d/.filtered"
}

# One port per job SLOT, held until that slot's game exits, so no two
# live servers can ever share a port (a modulo rotation can collide
# when one long game outlives a run of short ones).
declare -a SLOTPID
launch() { # label venue arm seed
  local k p
  while :; do
    [ -s "$OUT/STOP" ] && return 1
    for ((k = 0; k < JOBS; k++)); do
      p=${SLOTPID[k]:-}
      if [ -z "$p" ] || ! jobs -rp | grep -qx "$p"; then
        one "$@" $((PORT0 + k)) &
        SLOTPID[k]=$!
        return 0
      fi
    done
    wait -n 2>/dev/null || true
  done
}
drain() { local p; for p in "${SLOTPID[@]}"; do wait "$p" 2>/dev/null; done; SLOTPID=(); }

identical() { # a b : scorelogs equal with the per-run `id` line excluded
  [ -r "$1" ] && [ -r "$2" ] && diff -q <(grep -v '^id ' "$1") <(grep -v '^id ' "$2") >/dev/null
}

link_in() { # cache-seed-dir dest note
  rm -rf "$2"; mkdir -p "$(dirname "$2")"
  cp -al "$1" "$2"
  rm -f "$2/cached.txt"; echo "$3" > "$2/cached.txt"
}

echo "=== scoreboard start $(date -Is)  pin $PIN ==="

# ---- phase 1: baseline self-play from the cache, or its identity probe
declare -A BSTATE
for v in $VENUES; do
  set -- $(cache_state "$v"); BSTATE[$v]=$1; gen=${2:-}
  [ "$1" = probe ] || continue
  echo "=== $v: baseline cache $gen is not certified for this pin; probing $PROBE_SEEDS ==="
  for s in $PROBE_SEEDS; do
    [ -f "$(game_dir defaults "$v" baseline "$s")/DONE" ] || launch defaults "$v" baseline "$s"
  done
done
drain
[ -s "$OUT/STOP" ] && die "STOP: $(cat "$OUT/STOP")"
for v in $VENUES; do
  set -- $(cache_state "$v"); st=$1; gen=${2:-}
  if [ "$st" = probe ]; then
    same=0
    for s in $PROBE_SEEDS; do
      identical "$gen/seed-$s/scorelog.txt" \
                "$(game_dir defaults "$v" baseline "$s")/scorelog.txt" && same=$((same + 1))
    done
    if [ "$same" = 3 ]; then
      echo "$PIN probe $PROBE_SEEDS identical $(date -Is)" >> "$gen/CERTIFIED"
      st=certified; echo "=== $v: probe identical 3/3, $gen certified for this pin ==="
    else
      echo "=== $v: probe $same/3 identical; baseline plays fresh and starts a new generation ==="
      BSTATE[$v]=fresh
    fi
  fi
  if [ "$st" = certified ]; then
    for s in $(seeds_of "$v"); do
      d=$(game_dir defaults "$v" baseline "$s")
      [ -f "$d/DONE" ] || link_in "$gen/seed-$s" "$d" "hardlinked from $gen, certified for pin $PIN"
    done
    BSTATE[$v]=certified
  fi
done

# ---- phase 2: everything else, seed-major so an interrupted run holds
# complete seeds (every cell of a seed) rather than one complete cell.
for s in $SEEDS; do
  for v in $VENUES; do
    seeds_of "$v" | tr ' ' '\n' | grep -qx "$s" || continue
    for l in $LABELS; do
      for a in deep $([ "$l" = defaults ] && echo baseline) \
               $(has_mixed "$v" && echo mixed_db mixed_bd); do
        [ -f "$(game_dir "$l" "$v" "$a" "$s")/DONE" ] && continue
        launch "$l" "$v" "$a" "$s" || break 4
      done
    done
  done
done
drain
[ -s "$OUT/STOP" ] && die "STOP: guards failed, nothing reported: $(cat "$OUT/STOP")"

# ---- phase 3: a freshly played, complete baseline becomes a new
# generation of the cache, certified for this pin as its origin.
for v in $VENUES; do
  [ "${BSTATE[$v]}" = certified ] && continue
  src=$OUT/defaults/$v/selfplay/baseline
  gen_complete "$src" "$v" || { echo "=== $v: baseline incomplete, not cached ==="; continue; }
  gen=$(cache_dir "$v")/gen-${PIN:0:12}
  mkdir -p "$gen"
  for s in $(seeds_of "$v"); do
    [ -f "$gen/seed-$s/DONE" ] || cp -al "$src/seed-$s" "$gen/seed-$s"
  done
  echo "$PIN origin $OUT $(date -Is)" >> "$gen/CERTIFIED"
  echo "=== $v: baseline cached as $gen ==="
done

# ---- phase 4: completeness, then the report
bad=0
for l in $LABELS; do for v in $VENUES; do
  for a in deep $([ "$l" = defaults ] && echo baseline) $(has_mixed "$v" && echo mixed_db mixed_bd); do
    want=$(seeds_of "$v" | wc -w)
    if [ "$a" = deep ] || [ "$a" = baseline ]; then dir=$OUT/$l/$v/selfplay/$a; else dir=$OUT/$l/$v/mixed/$a; fi
    got=$(ls "$dir"/seed-*/DONE 2>/dev/null | wc -l)
    if [ "$got" -eq "$want" ]; then r=SUCCESS; else r=FAIL; bad=1; fi
    echo "$r $got/$want games $(date -Is)" > "$dir/DONE-BATCH"
    echo "  $r $l/$v/$a $got/$want"
  done
done; done
python3 "$here/rankcmp.py" "$OUT" --md "$OUT/SCOREBOARD.md" \
  && echo "=== wrote $OUT/SCOREBOARD.md $(date -Is) ==="
[ "$bad" = 0 ] && touch "$OUT/SCOREBOARD-DONE"
exit "$bad"
