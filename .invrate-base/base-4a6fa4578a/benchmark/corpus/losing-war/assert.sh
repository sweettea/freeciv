#!/bin/sh
# Position validity: an active war in which cities have changed hands.
caps=$("$SQ" "$POS" captured | wc -l)
warcount=$("$SQ" "$POS" wars | wc -l)
[ "$caps" -ge 1 ] || { echo "no captured cities in position"; exit 1; }
[ "$warcount" -ge 1 ] || { echo "no active wars in position"; exit 1; }

# Captures cannot silently revert in one turn.
caps_post=$("$SQ" "$POST" captured | wc -l)
[ "$caps_post" -ge 1 ] || { echo "captured cities vanished"; exit 1; }

# W14 assertions land here later: the war ledger recognises the losing
# side, and suing for peace is keyed on losing rather than on love.
exit 0
