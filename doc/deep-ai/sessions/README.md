# Session briefs and the arc roadmap

One file per scheduled session, written ahead of time so a session can be
handed to an agent whole. Each brief is the session's §0 THE JOB; the hard
rules stay in `CLAUDE.md` and the harness rules in `NEXT-SESSION.md` §5
(read both first, every session, as always). This README is the canonical
session order — a living document, re-cut by the orchestrator as verdicts
land.

## Scope of the arc (human, 2026-08-04, broadened same day)

**Everything in the backlog is in scope EXCEPT three things:**
1. **Honest-information beyond the flag** — the omniscience flag + breakage
   census happen; the inference/knowledge layer (`honest-information.md` §5)
   does not.
2. **Conversation with AI allies** (W24 / coalitions / treaty machinery).
3. **Refactor-to-threads** (W20; Tier-B purity items that are merely
   thread-PREP stay eligible as filler since they are behaviour-neutral).

Rhythm stays the 2026-07-31 human call: **mechanism epoch now, ONE tuning
finale later on cloud** — every session accumulates placeholders and flags
them; nothing tunes locally.

## The ladder (canonical order; strike-through = done, bold = in flight)

| # | Session | Brief | Depends on |
|---|---|---|---|
| ~~1~~ | ~~W34 E34.1 stance surface~~ | (ran from W34 A.7) | — |
| **2** | **W34 E34.2 remaining gates** | [S-E34.2-gates.md](S-E34.2-gates.md) | E34.1 |
| 3 | W34 E34.3 traits | [S-E34.3-traits.md](S-E34.3-traits.md) | E34.2 |
| 3a | **D1 defect round: military discipline** — done-flag reset (`deepops.c:1903`, disables tired-holds/convoy-holds/worker-flee/consolidate), sortie claim-discipline, melt survival re-check, triage on probability, barbarian gate mismatch, target anchor (mechanism-map §1 D-A/D-F/D-G) | to write | before any military measurement |
| 3b | **D2 defect round: credit integrity** — horizon-credit sawtooth, counterfactual ttg, CM/horizon deadlock, one shared threatened verdict, upkeep dimension, bundle reachability, tuning blocks + log completeness, caravan-discount guard (mechanism-map §1 D-B/D-C/D-H) | to write | **before the R3 corpus and before ANY tuning** — both currently measure the sawtooth |
| 3c | **D3 defect round: site truth** — correct defence bonus in deep (upstream bug #1 imported at `deepsite.c:920`), closed-water naval bonus, held-lift re-validation, enough-result wiring (mechanism-map §1 D-D/D-E) | to write | — |
| 4 | Rates from amortized value + celebration | [S-rates.md](S-rates.md) | — |
| 5 | Trade routes on merit | [S-trade-merit.md](S-trade-merit.md) | — |
| 6 | Class rescale: one scale for city choices | [S-class-rescale.md](S-class-rescale.md) | 3b, 4, 5 (fresh corpus after all three) |
| 6a | Government valuation via the `gov_value` hook (seam map §4: zero seam cost, RC1 at its purest) | to write | — |
| 7 | Combat E2: offensive force sizing | to write | combat E1 (landed) |
| 8 | Naval invasion slice 3: invasion wants + go/no-go | to write | 7 (odds in survival currency) |
| 9 | Small-mechanism round: density-fear reachability gate (spec'd `0112a65112`), hut-risk awareness, watch items (drop-pick cosmetics, explorer census creep) | to write | — |
| 10 | civ2civ3 gap-closer round: tile-quality CM/site scaling, embassy science, war-unhappiness/martial law, settler-factory economics, wonder prereqs (check `civ2civ3-adjustments.md` for what already landed) | to write | — |
| 11 | Combat E3 sorties → E6 assault operations → E7 reserves → E8 raids → E5 sieges → E4 aggregation swap (one session each; order per STATUS 2026-07-31) | to write | 7, 8 |
| 12 | W35 E35.1 spy-action currency valuation (the un-gated espionage slice) | to write | class rescale (6) |
| 13 | W34 E34.4 tech direction + position | to write | E34.3 |
| 14 | Era-reachability check (does any cell reach flight?) — gates W32 air and the rest of W35 | to write | — |
| 15 | Omniscient-AI flag + breakage census | [S-omniscience-flag.md](S-omniscience-flag.md) | — |
| 16 | Difficulty audit + harness | [S-difficulty-audit.md](S-difficulty-audit.md) | late on purpose: after mechanisms stop moving strength |
| 17 | Difficulty ladder | [S-difficulty-ladder.md](S-difficulty-ladder.md) | 16 |
| 18 | Cloud tuning finale (the pile: lift cluster, stance gains, scales, everything flagged) | — | all of the above |

Ordering notes:
- 4-6 (the want rebalance) precede the difficulty measurements because
  conversions move want scales; measuring the ladder first would mean
  measuring it twice.
- 15-17 sit late for the same reason; the flag (15) is independent and can
  be pulled earlier as a gap-filler if a session slot opens.
- **Combat E2-E8 detail warning**: the epic map and human posture notes
  were banked in a PREVIOUS task system's metadata that no longer exists.
  STATUS 2026-07-31 ("Combat workstream E1") and
  `findings/civ2civ3-adjustments.md` (survival model, sequential-attrition
  odds, T153 forensics) are the surviving sources — the E2 brief must be
  reconstructed from those, and any posture note not found there is LOST
  and must be re-asked, not guessed.

## The audit that re-cut this ladder (2026-08-04/05)

A five-reader deep dive (four Opus domain readers + a Fable omniscience
census) produced `findings/mechanism-map.md` (consolidated defects,
unconsumed signals, ranked couplings/gaps) and
`findings/non-omniscient-roadmap.md` (the staged de-omniscience path).
Raw reports: `findings/deep-dive-2026-08-04/`. The D1-D3 rows above come
from its verified-defect ledger; M1's brief must absorb the
attrition/cost-of-victory coupling (mechanism-map §3.1); the ~20 stale-doc
items in its reports are owed a doc-hygiene pass.

## What was descoped, by whom

- **The owed lift-cluster tuning pass** (old NEXT-SESSION §0): deferred to
  the cloud pile — human call 2026-08-04. Not to be run locally.
- The three standing exclusions at the top of this file.

## End states this arc serves (human, 2026-08-04)

a. **Ready for extensive tuning** — separable blocks via W34 pinning, every
   placeholder registered and audit-clean, campaign design on the shelf.
b. **An omniscient-AI flag** — honest handicaps runnable behind one
   A/B-able switch, with a census of what breaks.
c. **A difficulty ladder on deep** — meaningfully similar to stock's
   levels, usually beating stock at each.
d. *(broadened)* The mechanism backlog burned down to "the bound-slam
   diagnostics come back quiet" before the finale.
