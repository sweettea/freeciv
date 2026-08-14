#!/bin/sh
# W05: a deep settler with a war corridor across its route must neither
# die to it nor stop expanding.

# Position validity: the deep player (0) is at war with the attacker (1)
# and the corridor of attacker warriors is present.
"$SQ" "$POS" wars | grep -q '^0,1$' || {
  echo "players 0 and 1 are not at war in the position"; exit 1; }
warriors=$("$SQ" "$POS" units 1 Warriors | wc -l)
[ "$warriors" -ge 3 ] || {
  echo "corridor missing: only $warriors attacker warriors"; exit 1; }

# No deep cityfounder may die for any reason; founding ("used") is the
# only permitted way to lose one.
if grep 'deep-loss' "$LOG" | grep 'founder=1' | grep -qv 'reason="used"'; then
  grep 'deep-loss' "$LOG" | grep 'founder=1' | grep -v 'reason="used"'
  echo "a settler died instead of declining the hostile corridor"
  exit 1
fi

# Not paralysed by fear: the deep player must still have founded.
pre=$("$SQ" "$POS" cities | grep -c '^0,')
post=$("$SQ" "$POST" cities | grep -c '^0,')
[ "$post" -gt "$pre" ] || {
  echo "no new city founded under threat: $pre -> $post"; exit 1; }

exit 0
