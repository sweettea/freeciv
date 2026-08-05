# Session brief — the omniscient-AI flag and its breakage census

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md` (RC6 is the motivation),
`findings/honest-information.md` in full (§8 P0.0 is this session),
`NEXT-SESSION.md` §5 harness rules, and the STATUS.md tail.

## Scope, set by the human 2026-08-04

Flag and census ONLY. No inference layer, no knowledge structs, no recon
wants, no staleness projection (all of honest-information §5 stays future
work), and no tuning for non-omniscient play. The failure list IS the
deliverable — honest-information §8: "the failure list *is* the
requirements document for the knowledge layer."

## THE JOB

1. **The flag.** A single switch that makes deep seats run with `H_MAP`,
   `H_FOG` and `H_TARGETS` set regardless of skill level. Implementation
   constraint, not a suggestion: it must be A/B-able through the existing
   tuning-file mechanism, because that is what the harness switches on and
   hashes into the result cache — a `struct tuning` key (say
   `honest_info`, 0 = today, compiled default 0), applied to deep players
   only, from deep's own hooks (e.g. OR the three handicaps onto the
   player at phase start / player creation). This touches `ai/deep` only.
   Register in all four places `tune-audit.py` checks.

   Verified 2026-08-04: deep's own code already gates on `H_MAP` at 17
   sites (+1 `H_FOG`), and the forwarded stock code has ~40
   `parameter.omniscience = !has_handicap(H_MAP)` sites plus scattered
   `H_FOG`/`H_TARGETS` checks — the flag has real paths to exercise on
   both sides. Note what it does NOT close: the Tier-3 strategic reads
   (`dai_war_desire`'s perfect census, `daitech.c`, `daidata.c` — zero
   handicap checks) stay omniscient. Say so in the findings doc rather
   than trying to fix them.

2. **Prove the off state.** `honest_info = 0` must be byte-identical to
   the pre-change binary (10 smoke + 10 isolated-150 paired seeds, pinned
   binaries). The 40-game batch lost to an unverified switch is why:
   prove the ON state by execution evidence too (handicap bits visible in
   behaviour or a one-line counter).

3. **The census.** Run the flag ON in at least two cells (isolated-150
   and one land cell — smoke or contested; 10-20 paired seeds each,
   `--arms deep` A/B on the tuning file, one pinned binary) and record
   what breaks, against the specific predictions in honest-information
   §4's table: settling stops (city_desirability NULL on unknown tiles),
   danger amnesia, targeting starvation, plus whatever actually happens
   that the table did not predict. Metrics: score vs the OFF arm, cities,
   foundings, explore fraction, losses; plus assertion failures/crashes
   (a crash is a finding, not a session failure). Use proxy metrics, not
   just score — the AI may limp rather than collapse.

4. **Write `findings/omniscience-flag-breakage.md`**: what broke, in
   which subsystem, with numbers; what the §4 table predicted vs what
   happened; the Tier-3 reads left open; what the knowledge layer would
   have to provide first. This document is goal (b)'s deliverable.

## Acceptance

- OFF byte-identity pasted; ON divergence + execution evidence pasted.
- Census run to DONE-marker completion in both cells, numbers in the
  findings doc.
- `meson test -C build` passes; `tune-audit.py` CLEAN at its new count.
- STATUS.md dated section as the final commit. Leave NEXT-SESSION.md
  alone.

## Scope fences

~4-6 commits (flag, counter/proof, findings doc, STATUS). Do NOT fix
anything the census finds. Do NOT touch `ai/default` — the Tier-3
omniscient reads live there and stay there (they are already recorded as
upstream bug candidates 11-14 in honest-information §9). Deep's own H_MAP
sites may be adjusted only if the flag exposes a deep-side crash; a stock
crash under handicaps is a finding for `findings/upstream-bugs.md`.

## Stop conditions

The flag can't be expressed without touching `ai/default` or `common/`
(report the seam instead); the OFF arm fails byte-identity.
