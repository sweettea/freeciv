#!/bin/sh
# Barbarian reconquest (T160 forensics, findings/civ2civ3-adjustments.md):
# 47 of 52 barbarian-held cities sat garrisonless for ~40 turns and no AI
# walked in. This position holds that world; the assertions demand the
# walk-in machinery exists AND at least one city actually comes home.
# Player ids 13 (Pirate) and 14 (Barbarian) are position facts, recorded
# in PROVENANCE.txt.

# --- Position validity: the world still poses the problem. ---
barb_pos=$("$SQ" "$POS" cities | awk -F, '$1==13||$1==14' | wc -l)
[ "$barb_pos" -ge 40 ] || {
  echo "position rot: only $barb_pos barb-held cities (want >= 40)"; exit 1; }

# Most of them undefended: garrison units on barb city tiles are rare.
barb_gar=$("$SQ" "$POS" units 13; "$SQ" "$POS" units 14)
empties=$("$SQ" "$POS" cities | awk -F, '$1==13||$1==14 {print $2","$3}' \
  | while read xy; do
      echo "$barb_gar" | awk -F, -v xy="$xy" '$3","$4==xy' | grep -q . \
        || echo free
    done | wc -l)
[ "$empties" -ge 30 ] || {
  echo "position rot: only $empties empty barb cities (want >= 30)"; exit 1; }

# --- Mechanism: the reconquest paths fired, not just got lucky. ---
grep -Eq 'deep-op .* form .* required=1' "$LOG" || {
  echo "no singleton operation formed in the window"; exit 1; }
# Garrison drafting is position-dependent (fires only where the free
# pool alone cannot clear the bar), so report it without failing.
drafts=$(grep -Ec 'deep-op .* form .* drafted=[1-9]' "$LOG")

# --- Outcome floor: at least one held tile changed hands away from
# the barbarians (count-based checks are fooled by barbs capturing
# NEW cities elsewhere while losing old ones). ---
retaken=$("$SQ" "$POS" cities | awk -F, '$1==13||$1==14 {print $2","$3}' \
  | while read xy; do
      "$SQ" "$POST" cities | awk -F, -v xy="$xy" \
        '($2","$3)==xy && $1!=13 && $1!=14' | grep -q . && echo took
    done | wc -l)
[ "$retaken" -ge 1 ] || {
  echo "no barb-held city retaken in $TURNS turns"; exit 1; }

# Guard: reconquest must not cost a nation -- nobody dies in a
# 12-turn window that the live game survived idle.
alive_pos=$(zstdcat "$POS" | grep -c '^is_alive=TRUE')
alive_post=$(zstdcat "$POST" 2>/dev/null | grep -c '^is_alive=TRUE' \
             || grep -c '^is_alive=TRUE' "$POST")
[ "$alive_post" -ge "$alive_pos" ] || {
  echo "a nation died during reconquest ($alive_pos -> $alive_post)"
  exit 1; }

echo "barb-reconquest: pos=$barb_pos empty=$empties retaken=$retaken drafts=$drafts"
exit 0
