# Europe-track queue (user-approved, post-reset)

- **ARENA CAST** (approved 2026-08-22 "let's do that next time"): wire
  FREECIV_DEEP_ARENA into play-europe.sh — per-seat inis derived from
  tuning-derby7.ini (trader Carthago, aggressor Rus, wonder Hellas,
  etc.), manifest per deeptuning.c's [arena] format; tune.py --arena
  is the format reference. NEXT game, not the imminent one.
- Chatty inbound (deep parses ally requests) — after outbound lands.
- Rail-upgrade pass (gap = below best buildable extra, not just
  unroaded).
- Corridor forts (traffic x threat, ETA-race capture term, garrison
  as-needed via reserve; after employment fix + rail).
- Pollution reserve retest on a venue with industry.
- Risk-weight decision for derby-7 (fear knobs zeroed there; W12H
  risk term inert until they are not).
- Alignment remedy: canonicalise walk direction (W12H rec #1).
