# Session brief — INSTR: light up the dark knobs

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md` §5,
`findings/knob-coverage.md` (this session's spec — §4-6 name every dark
knob and what would light it), `benchmark/knobcensus.py` (the census your
work must turn green), and the D1/census STATUS tails.

## Why

Gate 7 of CAMPAIGN-v4 is RED: 53 of 121 tunables have no instrument, so
the campaign could only search 36 dimensions honestly. This session adds
the missing observability — log lines and counters only, behaviour-
neutral, provably so.

## THE JOB

1. **Post-injection defence line** (unlocks 15 MIL knobs): one line per
   city per turn AFTER `deep_credit_incoming_defence` and the fear
   pipeline, carrying the injected components (overseas, shadow, density,
   prior, barbarian, deficit, floor+class, menace, credits) — the
   knob-coverage doc lists the 15 knobs it must evidence. Extend the
   existing `deep-def` convention; respect DEEP_QUIET.
2. **A `deep-cm` line** (unlocks 17 ECON knobs): per city per turn on CM
   state application — state matched, factors chosen, trade-richness,
   sortie value, survival verdict consumed, rearrange happened or not.
3. **`deepsite` term logging** (unlocks 8 site knobs): per accepted
   candidate (not per tile — volume), the conditional term contributions
   (special cluster/bonus, claim, rival pressure/crowding, choke,
   pressure factor, travel crowding). The census's proxy-only site knobs
   should flip to direct.
4. **A pinned war cell for the stance census**: one small config (pinned
   contested or cramped variant) so gate 5's counter has a venue where
   the threatens-exemption does not swallow every target; add to the
   census rerun protocol, not to the standard matrix.
5. **Census rerun**: `knobcensus.py` over fresh 5-10-seed probes of the
   relevant cells with the new binary; the deliverable is the new table —
   target: NO-INSTRUMENT ≤ the census doc's irreducible list (mode
   switches and genuinely consumerless knobs, which get flagged for the
   pile/removal instead), searchable dimensions ~90+.

## Discipline

- **Behaviour neutrality proven, not assumed**: 10 smoke + 10
  isolated-150 paired seeds byte-identical vs the pre-session binary.
  Log lines must not call anything with side effects (the R1 probe's
  tech_want snapshot/restore is the cautionary pattern).
- Volume budget: stay within the existing log-size envelope order
  (`deep-dv`/`deep-def` precedent); per-candidate not per-tile for site.
- No mechanism changes — the tired-attack seam defect and the convoy
  population defect the census confirmed belong to D1's hand-off/M-queue,
  not here.

## Acceptance

- Byte-identity pasted; the census rerun table pasted with before/after
  verdict counts; `meson test` (4) passes; `tune-audit.py` CLEAN.
- knobcensus.py MAP updated for every new instrument (the script's
  self-check must pass).
- STATUS dated section final; update CAMPAIGN-v4.md gate 7 state to
  green/amber with the new counts (this session owns that cell of the
  table). Leave NEXT-SESSION.md and other sessions/ files alone.

## Scope fences

4-6 commits, `ai/deep` log lines + benchmark/config/doc only.

## Stop conditions

A knob's mechanism turns out to have NO consumer at all (not just no
log) — that is a census reclassification (flag for removal/pile), not an
instrumentation target; if more than a handful surface, stop and report
the list.
