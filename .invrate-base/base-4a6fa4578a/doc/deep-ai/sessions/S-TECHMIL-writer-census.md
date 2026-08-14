# Session brief — TECHMIL slice 1: which military tech_want writers still fire for deep?

Human design conversation 2026-08-13: "techs can enable better
defensive or offensive units, so it might be worth pushing tech for
mil reasons." Before any design, the census: deep may currently have
LESS military tech appetite than stock, by accident.

## The suspicion, located

Tech direction is inherited: deep registers no tech hook, so
`dai_select_tech()` picks the research goal from `tech_want[]`. That
array is fed by stock writers, and the MILITARY writers live inside
machinery deep REPLACED:

- daimilitary.c:1195 and :1460 — "Update tech_want for appropriate
  techs for units we want to build" (attacker-want walking the tech
  tree, defender-want crediting enabling techs).
- `dai_wants_role_unit()` (daitech.c:523) — writes tech_want for a
  desired unit's enabling advance. deeptrade.c deliberately routes
  caravan wants back through it (three comments say so) precisely to
  keep tech_want fed — evidence the seam is real and known.

If the daimilitary writers only run inside stock's military advisor
paths that deep bypasses (deep_military_choose_build and friends),
then deep players' tech_want is econ-heavy and their research goals
systematically under-weight military techs. In a defense-dominant era
(Gunpowder doctrine) that is exactly backwards.

## The census (read-only, this slice's whole scope)

1. Instrument every tech_want WRITE with a `deep-techwant` log line:
   writer site, player, advance, amount. Prefer ONE choke point if a
   common helper exists; otherwise a line per writer site. This must
   be visibility-only w.r.t. behaviour — no values change, and if the
   only way to instrument is a static-to-nonstatic visibility change
   in ai/default, that is rule 2's permitted exception and needs the
   benchmark-neutrality proof.
2. Run a mixed corpus (tech50war-200, >=8 mirrored seeds is enough
   for a census) and tabulate per side (deep seats vs stock seats):
   total tech_want written, share by writer site, share
   military-vs-economic, and the chosen research goals' class.
3. Deliverable: a findings table in STATUS answering (a) which
   writers are DEAD for deep seats, (b) the deep:stock ratio of
   military tech_want, (c) whether chosen goals differ in military
   share. Byte-identity of the instrumented binary at defaults is
   required if any knob is added (prefer no knob: log lines only).

## What this slice does NOT do

No direction override, no new valuation, no knobs unless forced. The
design conversation (deep-priced tech direction via survival
counterfactuals; era/parity terms; the want_t seam) waits on this
census — if the military writers are alive and balanced, the whole
motivation shrinks and we say so.

Usual rules: one commit one thing, counters are the deliverable,
meson test 5/5, STATUS updated with the table, stop after the census.
