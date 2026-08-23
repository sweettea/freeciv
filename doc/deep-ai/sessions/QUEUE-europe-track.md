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
- **SITE-IMPROVABLE** (user call 2026-08-23, "we can improve crappy
  sites after we build them"): the site evaluator's starve gate and
  potential term judge tiles AS-IS; price achievable-after-improvement
  yields instead (irrigable/minable/transformable), discounted by
  worker-turns, REUSING W29's chain valuation. Evidence: Carthago
  stalled at 23 cities with 118 mindist + 36 starve drops/turn while
  "room" was unimproved desert. Starve gate becomes a starve price.
  BLOCKED BEHIND: epic/pox-spacing (owns deepsite.c) and
  epic/w29-transform-chains (provides the chain valuation).
- **NAVAL-FOUNDING for Europe** (same debrief): Carthago evaluated
  boat=0 sites all game facing Sicily/Sardinia; site_naval_gate=0 in
  derby-7. One-line ini experiment next game; weights already tuned.
- **CAPITAL-STAKE defence** (user live-play 2026-08-23: Osmanli lost
  its capital T~123, civil war split 31 cities -> 16, "permanently
  fucked"): the defence ledger priced the capital like any city
  (danger=7233 urgency=67 seen, no palace term). Design: a capital
  defence stake = f(empire size x civil-war expectation) feeding the
  existing danger/urgency machinery (W11 seams), plus possibly a
  palace-relocation want when the capital is indefensible. The
  employment probe's disposition census (garrison columns) is the
  instrument; the user's T110-T125 game is the corpus. Gate + derived
  stake, no invented constants. NEXT WAVE, high rank - this is a
  score-track epic (civil war is catastrophic on any venue), not just
  flavour.
- **OCEAN-FILL expeditions** (user design note 2026-08-23): a transform
  chain that STARTS on ocean is not a worker job, it is a small
  expedition — the engineer needs a hull to stand on. Design
  constraints fixed now: (a) the chain's want must EMIT transport
  demand through the hull-commission/lift machinery (founder_berth_route
  precedent) or it prices a chain that can never execute — the ferry
  chicken-and-egg one level up; (b) gather + crossing ETA charges into
  the chain's worker-turn discount; (c) this is W28 stacking's real
  use-case (one lift, N engineers, one mega-job — the close-out said
  stacking needs long jobs; ocean fill is the longest job there is);
  (d) engineers parked offshore for 20 turns need the escort thread.
  Check the ruleset's reclaim/adjacency requirements before designing.
  BLOCKED BEHIND: archipelago/late-game venue + capability census.
