# Session brief — W12 slice 2: worker tasks from beyond the radius ceiling

Slice 1 (merged d075c4bfc4, STATUS 2026-08-13) landed trunk roads via
MST corridors and worker locality, and left one named gap: the worker
task SOURCE is still capped by a radius ceiling, so improvement demand
that lives farther than the ceiling never generates tasks at all —
locality then optimizes assignment over an artificially truncated
demand set. Read the W12 slice 1 STATUS entry and
doc/deep-ai/workstreams/ W12 material first; slice 1's counters
(link-gap, walking-distance) are your instruments and must stay live.

## The slice

- Extend the task source so demand beyond the ceiling is visible,
  behind a knob (default = today's ceiling exactly, byte-identity).
  The design is yours to derive from what slice 1 actually built —
  question the plan if the code disagrees with this description.
- Locality (slice 1) must remain the assignment arbiter: the point is
  a complete demand set, not workers commuting across the map. If the
  ceiling lift needs a distance discount to avoid commuting, reuse
  slice 1's existing discounting; do not invent a second one.
- Counters: tasks sourced beyond the old ceiling (count + distance
  histogram line), and slice 1's walking-distance line for regression
  watch. A knob-on run where the beyond-ceiling count is 0 is a miss —
  report, don't ship.

## Guards

- server/advisors/ is touchable ONLY under W28/W29 or inert extension
  points (CLAUDE.md rule 4); slice 1 already widened a hook with the
  double stock-arm proof — if you need that hook again, the same
  proof standard applies (stock arm byte-identical, 20/20).
- Byte-identity 10/10 at defaults including one mixed cell.
- A/B: the box is busy until roughly two hours from now (two A/Bs on
  ports 7500-7549 and 7600-7649 are draining). Build and prove
  identity FIRST; launch your A/B only when a check of those two
  ranges' result dirs shows both arms complete (DONE counts 32/32),
  or after your own smoke games confirm the box has headroom. Venue:
  the same venue slice 1 used for its proof (read its STATUS entry),
  >=16 mirrored seeds, one binary pinned WITH libfreeciv.so.

Usual rules: struct tuning + params/blocks registration, tune-audit
CLEAN, knobcensus entry, one commit one thing, meson test 5/5, STATUS
update with pasted numbers, stop after the A/B.
