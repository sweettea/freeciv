#!/bin/sh
# Position validity: a genuinely hut-dense early map.
huts_pre=$("$SQ" "$POS" extra-count Hut)
[ "$huts_pre" -ge 30 ] || { echo "position has only $huts_pre huts"; exit 1; }

# Huts are only ever consumed, never created.
huts_post=$("$SQ" "$POST" extra-count Hut)
[ "$huts_post" -le "$huts_pre" ] || {
  echo "huts increased: $huts_pre -> $huts_post"; exit 1; }

# W06 assertions land here later: explorer production want > 0,
# no explorer disbanded while exploration is wanted.
exit 0
