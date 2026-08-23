# Session brief — capital stake: price the capital like losing it costs a civil war

From the user's Europe game (QUEUE-europe-track CAPITAL-STAKE entry,
STATUS 2026-08-23): Deep_Osmanli lost its capital ~T123 and the civil
war split 31 cities -> 16 — catastrophic and effectively irreversible.
The defence ledger had priced the capital like any city (danger=7233
urgency=67 logged, no palace term). The Europe ini has since restored
the garrison floor (garrison_floor_danger=40, floor_interior_weight
0.25), but the floor is a blanket instrument; the capital deserves a
stake term of its own. This is a score-track epic: civil war is
catastrophic on any venue, not just flavour.

## The mechanism (gated)

1. **A capital defence stake** feeding the EXISTING danger/urgency
   machinery (the W11 seams in deepdefence.c — read
   deep_apply_garrison_floor and the danger ledger first; extend, do
   not fork the geometry). The stake scales with what the capital's
   loss actually costs: empire size times the civil-war expectation
   (see civil_war_chance()/game rules in common+server for the actual
   odds — DERIVE the stake from the score value of the cities at risk
   of splitting, show the arithmetic in STATUS; PLACEHOLDER only if no
   derivation closes).
2. **Palace-relocation want** when the capital is indefensible
   (stake-priced danger persistently above what the garrison can
   answer): a want for B_PALACE (or ruleset equivalent) in a safer
   city, through the existing building-want seams. Second commit,
   separately gated — ship the stake even if relocation needs more
   design; flag in STATUS if deferred.
3. Gate + knobs in struct tuning (capital_stake_gate, derived gain);
   never append an existing ini key; awk-dedupe; tune-audit CLEAN at
   the new count; knobcensus.

## Proof

- Identity 10/10 at defaults, incl. one mixed cell.
- Liveness: the employment probe's disposition census (garrison
  columns) is the instrument — show the capital's garrison rising
  under the gate where the ungated arm starves it. The user's game
  play-0822-2253 saves T110-T125 are the corpus for the derivation
  (Osmanli's ledger at the fall).
- A/B: one 16-seed cell on the war venue if the box frees up (ports
  7600-7649, jobs <=3) — the holdout batch and G-tune own 7300/7500
  until they finish; check for their DONE/ALLDONE markers before
  launching, else defer the A/B and say so in STATUS.
- Europe rider mandatory: gate ON in derby5/tuning-derby7.ini per
  dedupe rules — the user's next game is the acceptance test. Do not
  duplicate the rider if the orchestrator already applied one.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare before pin). Batch drivers at benchmark/*.sh
launched detached from repo root (`setsid bash ./benchmark/<script>.sh
go` — exact shape; no cd-prefixed compounds). DONE markers record
SUCCESS. Power-loss sweep on inherited dirs. Games byte-deterministic.
One commit one thing (~2-3 commits expected: stake, relocation,
rider/docs); meson test all suites; STATUS with pasted numbers;
standard trailer; do NOT merge — stop after the STATUS commit on your
branch.
