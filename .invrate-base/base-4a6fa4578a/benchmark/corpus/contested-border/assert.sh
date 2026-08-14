#!/bin/sh
# Position validity: two empires' cities in close contact.
dist=$("$SQ" "$POS" min-cross-distance)
python3 -c "import sys; sys.exit(0 if float('$dist') <= 6 else 1)" || {
  echo "closest cross-owner cities are $dist apart, not a contested border"
  exit 1
}

# One turn of play must not collapse either side of the contact zone.
pre=$("$SQ" "$POS" cities | wc -l)
post=$("$SQ" "$POST" cities | wc -l)
[ "$post" -ge $((pre - 1)) ] || {
  echo "city count collapsed in one turn: $pre -> $post"; exit 1; }

# W07 assertions land here later: claim/denial terms in site scoring,
# contest-pressure adjustment of the perfection requirement.
exit 0
