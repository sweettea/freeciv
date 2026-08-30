#!/bin/sh
# Verify that the rulesets this project benchmarks do NOT let a unit
# found a city on a tile claimed by a FOREIGN player.
#
# Why this is a test and not a comment. S-FRONTIER opened on the census
# finding that 39% of every tile a failed T51-75 site search sees is a
# peaceful neighbour's border, refused categorically by ai/deep's
# "Land theft does not make for good neighbors" rule (deepsite.c,
# mirrored from ai/default/daisettler.c). The first question that epic
# had to answer was whether that refusal is a POLICY -- something a
# price could replace -- or a restatement of a rule the server enforces
# anyway.
#
# It is the second. common/city.c city_can_be_built_here() refuses a
# foreign-owned tile unless the ruleset has an ACTRES_FOUND_CITY
# enabler that permits DRO_FOREIGN ("Cannot steal borders by settling.
# This has to be settled by force of arms."), and both benchmarked
# rulesets spell the opposite out: the "pioneer" enabler requires the
# target tile to be UNCLAIMED, and the "domestic" enabler requires it
# CLAIMED and the actor NOT Foreign. Their union is "unclaimed, or
# ours" -- and note that this holds at WAR too, so deep's refusal is
# not merely redundant, it is strictly weaker than the server's.
#
# So the frontier-theft axis is dead by construction, and this check
# exists so that it stays dead quietly rather than being rediscovered:
# if a benchmarked ruleset ever grows an enabler that permits founding
# abroad, the axis re-opens and this test is where that shows up.
#
# RULESETS is the benchmarked set: civ2civ3 is the server default
# (GAME_DEFAULT_RULESETDIR) and every benchmark config leaves RULESET
# empty, and classic is the other one the venues can select. The
# remaining shipped rulesets are reported for information and do NOT
# fail the test -- `granularity`, for one, really does permit founding
# abroad under three of its four government-gated enablers, which is
# the useful proof that this check can tell the two cases apart.
#
# The check is deliberately conservative: an enabler is judged unsafe
# unless it either restricts the target tile to unclaimed ground or
# carries an actor requirement forbidding a foreign target.

srcdir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

cd "$srcdir" || exit 1

RULESETS="civ2civ3 classic"

# Prints one line per Found City enabler that could target a claimed
# tile owned by someone else. Exits 1 if it printed anything.
scan() {
  awk -v file="$1" '
    function judge(   claimed_false, foreign_false) {
      if (!is_enabler || !is_found_city) { return }
      seen++
      # "CityTile","Claimed","Tile",FALSE => unclaimed tiles only.
      claimed_false = (body ~ /"CityTile"[^\n]*"Claimed"[^\n]*(FALSE|false)/)
      # Actor req "DiplRel","Foreign",...,FALSE forbids a foreign target.
      foreign_false = (body ~ /"DiplRel"[^\n]*"Foreign"[^\n]*(FALSE|false)/)
      if (!claimed_false && !foreign_false) {
        printf "%s: [%s] may found a city on a foreign-claimed tile\n", \
               file, section
        bad++
      }
    }
    /^\[/ { judge(); section = $0; gsub(/[][]/, "", section)
            body = ""; is_found_city = 0
            is_enabler = (section ~ /^enabler/); next }
    /^[ \t]*action[ \t]*=/ { if ($0 ~ /"Found City"/) { is_found_city = 1 } }
    { body = body "\n" $0 }
    END {
      judge()
      if (seen == 0) {
        printf "%s: no Found City enabler found at all\n", file
        bad++
      }
      exit (bad > 0)
    }
  ' "$1"
}

rc=0

for rs in $RULESETS; do
  f="data/$rs/actions.ruleset"
  if [ ! -f "$f" ]; then
    echo "ERROR: benchmarked ruleset $rs has no actions.ruleset." >&2
    rc=1
    continue
  fi
  out=$(scan "$f") || rc=1
  [ -n "$out" ] && echo "$out" >&2
done

if [ "$rc" -ne 0 ]; then
  echo "ERROR: a benchmarked ruleset permits founding inside a foreign" >&2
  echo "border. The S-FRONTIER theft axis is no longer dead; see the" >&2
  echo "header of $0." >&2
  exit 1
fi

echo "Benchmarked rulesets ($RULESETS) refuse founding on a foreign tile."

for f in data/*/actions.ruleset; do
  [ -f "$f" ] || continue
  case " $RULESETS " in
    *" $(basename "$(dirname "$f")") "*) continue ;;
  esac
  scan "$f" || true
done

exit 0
