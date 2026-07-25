#!/bin/sh
# W11: simultaneous city emergencies must be resolved in severity
# order, not city list order.

# Position validity: advancing one turn must produce at least two
# emergencies for one player in the same turn (a rank=1 line).
grep -q 'deep-emergency .*rank=1' "$LOG" || {
  echo "no simultaneous emergencies fired; position is stale"
  exit 1
}

# Ordering invariant: within each (turn, player), severity keys
# (grave, urgency, capital, worth) must be lexicographically
# non-increasing with rank.
python3 - "$LOG" <<'EOF'
import re, sys
rx = re.compile(r'deep-emergency turn=(\d+) player="([^"]*)" rank=(\d+)'
                r' city=\d+ grave=(\d+) urgency=(\d+) capital=(\d+)'
                r' worth=(-?\d+)')
groups = {}
for m in rx.finditer(open(sys.argv[1], errors="replace").read()):
    t, p, rank = m.group(1), m.group(2), int(m.group(3))
    key = tuple(int(x) for x in m.group(4, 5, 6, 7))
    groups.setdefault((t, p), []).append((rank, key))
bad = 0
for (t, p), rows in groups.items():
    rows.sort()
    for (_, a), (_, b) in zip(rows, rows[1:]):
        if a < b:
            print(f"turn {t} {p}: rank order violates severity: {a} < {b}")
            bad = 1
sys.exit(bad)
EOF
