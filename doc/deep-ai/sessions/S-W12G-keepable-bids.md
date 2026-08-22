# Session brief — W12 fix: only bid for workers you can keep

The funnel (STATUS 2026-08-22, merge 55c8d8e208) localised the
corridor loss precisely. Read that entry and deepfunnel.c first.
The measured facts your fix must answer to:

- 22.4% of picks are overridden by deep_find_best_city_placement()
  in the SAME call (Settlers carry UTYF_WORKERS and the corridor
  auction bids on prospective founders).
- 43.6% of never-renewed picks end with the unit on a city mission.
- Drop causes on the walk: norun 39.6%, taken 19.0%, outbid 13.1%
  (roaded 13.7% is not a leak). Mid-build preemption is ~zero.
- ~235 worker-turns/game walk toward tasks that get dropped; 96% of
  begun work completes.

## The fix (two mechanisms, separately gated)

1. **Eligibility**: the corridor source must not bid on a unit the
   founder logic is about to reclaim — exclude units whose city-
   placement result outbids the corridor want (the comparison already
   exists at the override site; move the knowledge upstream of the
   pick, do not duplicate the valuation), or more simply exclude
   would-be founders entirely. Gate: e.g. trunk_keepable_gate.
   Design from the override site the funnel instrumented; whichever
   variant you pick, justify it from the `taken`/same-call numbers.
2. **Stickiness**: a worker en route to a corridor task keeps it
   unless the task became invalid (roaded/gone/unsafe) or the new
   bid beats the old by a margin. The margin is a struct tuning
   knob (PLACEHOLDER per rule 7 if no measured value presents
   itself; a margin of 0 must reproduce today's churn exactly —
   that is the off-state identity). This addresses norun/outbid
   churn. Keep it corridor-scoped; do not touch the general worker
   auction in this epic.

Both default OFF = byte-identity 10/10 (family ON both arms, one
mixed cell). The funnel counters are your instruments and must stay
live — the A/B's funnel table is the mechanism evidence.

## The A/B

corridor-150, the venue's measured power: n=29 mirrored seeds
resolves +6 score at t>=2 (~5 h at jobs 4). Arms: family+funnel ON
vs the same plus both fix gates. Pre-registered predictions, in
order of confidence: start-rate (stage 4 / stage 1) rises well above
8.1%; walking turns per completed road fall; techs deficit (pilot
t -3.18) shrinks; score moves toward >= 0. Report all four whatever
they say. If score stays negative with the funnel healthy, say so
plainly — that would mean corridors are a bad buy even executed
well, and the family's future is Europe-flavour only.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= alongside -Dclients= -Dfcmp=), own pinned
binary WITH libfreeciv.so, ports 7500-7549, jobs <=4 (pv4 shares the
box on 7300-7349). corridor-150 ~20 min/game — budget ~5 h for the
A/B, prune saves once scored, >=8G free. Tracked background dies at
~60 min; resume off DONE markers. New knobs: struct tuning +
params/blocks + knobcensus, tune-audit CLEAN at the new count. One
commit one thing; meson test all suites; STATUS with the funnel
table and the four predictions; stop after the A/B.
