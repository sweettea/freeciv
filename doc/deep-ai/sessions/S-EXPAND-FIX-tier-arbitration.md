# Session brief — founder wants must compete on value, not lose on tier

The funnel (STATUS 2026-08-22, merge c576a0e9c5) proved deep has
never built a settler: the military EMERGENCY tier (deep's own W01
change) discards 48.5% of positive founder wants on TIER, median
discarded want 1225, 43.7% of which would win under stock's
magnitude rule; auction wins then survive 1.004 production turns
before a military switch preempts. Read that entry, the W01
STATUS/workstream material, and the counter sites in deepdefence.c
(the funnel counters are your instruments and stay live).

## The fix (gated; default off = today's arbitration, byte-identity)

1. **Tier arbitration**: behind `founder_tier_gate`, let a founder
   want compete against the emergency tier on magnitude (stock's
   rule, which the funnel measured as the correct counterfactual) —
   or a bounded variant you justify FROM THE FUNNEL DATA (e.g.
   founder competes only when its want exceeds the emergency want,
   which is literally stock's comparison). Do not invent a discount
   constant; if a scaling knob is needed it is struct tuning
   PLACEHOLDER and flagged.
2. **Run protection**: behind the same gate or a second one
   (justify), a started settler build is not preempted by a
   military switch unless the city is in grave danger (the
   advisor's own grave_danger signal — cite it) — targeting the
   1.004-production-turn pathology. Reuse existing signals; no new
   danger model.

Do NOT touch the emergency tier's behaviour for non-founder wants;
this is arbitration between existing wants, not a re-pricing.

## Proof and A/B

- Byte-identity at defaults 10/10 incl. one mixed cell.
- A/B: tech50war-200 mixed_dc, seeds 37-56 — the funnel probe's 20
  games ARE the OFF arm (same binary lineage; verify the cache/
  provenance makes them reusable, else re-run OFF). ON arm = pv3 +
  the gate(s). 20 new games. Pre-registered, confidence order:
  (1) born > 0 after T20 (the zero breaks); (2) founder runs
  survive >1.004 turns; (3) cities gap shrinks; (4) score and the
  kill/gold columns reported whatever they say (more cities may
  trade against military tempo — report the trade honestly).
- ALSO run the mixed_cd ordering for the cities column only (the
  seat-order interaction, -4.56 vs -1.75, lives there; your fix
  may move one ordering and not the other — report both).
- Europe rider: gate(s) on in derby5/tuning-derby7.ini per dedupe
  rules — the user plays tonight; this fix is what lets deep seats
  expand past their startunits in that game.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare). Ports 7400-7449, jobs <=4 (chatty on
7600s is light; muster A/B may hold 7300s). Tracked background dies
~60 min (DONE-marker resume); verify launches by scorelog growth.
Disk ~8G: prune saves as you score. Games byte-deterministic; n =
seeds. New knobs: struct tuning + params/blocks + knobcensus;
tune-audit CLEAN at the new count. One commit one thing; meson test
all suites; STATUS with the funnel-vs-funnel table and the four
predictions adjudicated; stop after the A/B.
