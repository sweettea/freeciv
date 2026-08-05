# Campaign v4 — the tuning finale

Design doc, drafted 2026-08-05 (orchestrator), for human review. This is
the ONE big run the mechanism epoch has been accumulating placeholders for
(human call 2026-07-31: "mechanisms until the bound-slam diagnostics come
back quiet, then ONE tuning finale on the mature machine"). It does not
launch until every gate in §2 is green and a human says go.

Everything here is derived from lessons this project already paid for;
each rule cites its receipt.

---

## 1. Posture

- **One campaign, staged in blocks, folded once.** Not a rolling series —
  every prior interleaving of tuning and mechanism work perished the
  values it bought (receipt: Z2/Z3/v3b lineage; "code fixes moved the
  local pairing +0.38 median, tuning on fixed code +0.04").
- **The campaign is also a bug detector — plan for it.** Every prior
  campaign's chief product was a defect (Z2 → worker-guard questions;
  v3b gen-0 → hut-barbarian blindness). §7 defines stop-and-diagnose
  triggers so that when it happens we bank the finding cheaply instead of
  discovering it in the fold.
- **Values are provisional until folded under §8's rules.** No mid-run
  folds, no quoting un-confirmed numbers.

## 2. Launch gates (all must be green)

| # | Gate | State 2026-08-05 |
|---|---|---|
| 1 | D1 military discipline landed + measured | in flight |
| 2 | M3 small mechanisms (density-fear reachability gate esp.) | queued |
| 3 | G1 class-gated garrison floor landed + sized | queued (design set) |
| 4 | D3 site truth (defence bonus, closed-water naval bonus) | queued |
| 5 | R2 verdict banked (done: deflator 7-9× measured); **R2b wonder-caravans reformed** (the larger claimant — the corpus must see it); R3 class-rescale corpus + deflators shipped | R2 done; R2b, R3 queued |
| 5a | HEDGE step-0 census (production-switch penalty mass). NOT a gate unless its population is fat — then its step-1 mechanism joins the gates (it moves build wants) | queued, cheap |
| 6 | M1 combat E2 (cost-of-victory) + E4 aggregation swap (STATUS pins E4 pre-v4 explicitly) | queued |
| 7 | **Knob-coverage census** (§4) CLEAN: every searched knob fires on ≥1 training cell | to build |
| 8 | **Separability pilot** (§5) passed | to run |
| 9 | Binary frozen at a named commit; `meson test` + `tune-audit.py` CLEAN; STATUS quiesced | — |
| 10 | Human go | — |

Cut-line if the human wants the run sooner: gate 6's naval slice (M2) is
already outside the gates; convoy-family knobs are then EXCLUDED from the
search (census rule, §4) rather than searched dead.

## 3. Blocks, their pins, and their venues

Principle: a block is tuned where its mechanisms have POPULATION, pinned
where pinning makes its objective separable (W34 phase A's purpose), and
its stage order respects currency dependencies.

**Stage 0 — the exchange rate.** R3's class deflators first, alone: they
set the cross-class currency every other want trades in; any within-block
optimum found under wrong exchange rates is an optimum for the wrong game
(receipt: E34.2's gates-3+4 collision; W02-v1). Small dimension, biggest
interaction. Venue: isolated-150 + contested, unpinned (deflators must
hold in mixed play). Warm start: R3's corpus-derived values.

**Stage 1 — ECON block, PINNED ECONOMIC.** horizon_* weights,
cm_* factors, upkeep weights, rates gains (R1's measured ranges:
`rates_luxury_gain` 0.1-0.25 or `rates_science_gain` 4-13 as start
bounds), growth/treasury knobs, `stance_growth_gain`+`stance_expand_gain`
AS A PAIR (E34.2b: they collide if moved independently). Venue: garden2b
+ isolated-150 (+ primary guard). Pinning receipt: W34 A.6.

**Stage 2 — MIL block, PINNED MILITARY.** Fear lattice (overseas, boat
shadow, density, barbarian), G1's per-class floor weights, menace,
sortie/rotation/relief, `attack_value_weight` + E2's cost-of-victory
knobs, `stance_fear_gain`/`stance_op_gain`/`stance_assault_gain`. Venue:
cramped + lategame-80 (+ contested). Note: the floor classes are
venue-split by design (island vs land) — report per-cell, expect
covariates, do not average them away (receipt: H/H2 island tunes zeroing
the floor; "a verdict inherits its venue").

**Stage 3 — EXPANSION/SITE block.** site_*, settler_*, the LIFT CLUSTER
(the owed NEXT-SESSION §0 list: lift_gather_turns, lift_hitch_wait_ratio,
lift_drop_radius, boat_idle_damping, colony_lift_weight), worker knobs.
Venue: isolated-150 primary + primary-120 + smoke guard. Unpinned (site
choices interact with everything; pinning corner TBD — open question §9).

**Stage 4 — TRADE block, CLASSIC CELLS ONLY.** R2's discovery is binding:
civ2civ3 zeroes domestic route revenue, so caravan knobs have population
only on `*-classic` cells. `trade_merit` family, `caravan_*`. If R2's
verdict is "mechanism ships off pending rescale", this stage folds into
stage 0's aftermath.

**Stage 5 — cross-block confirm.** Best-of-each-block composed, measured
UNPINNED (FREE mode, identity stances per the standing no-fold call) on
the full cell matrix at claim power. Interactions between block optima
are expected; if the composition underperforms the blocks, bisect by
block before touching any value by hand.

## 4. The knob-coverage census (gate 7)

Before the run: for EVERY tunable in `tuning-params.ini`, an automated
table: knob → its execution counter → training cells where the counter is
nonzero at defaults (from existing logs where possible, one sweep batch
where not). Rules:

- **Zero population on every training cell ⇒ EXCLUDED from the search**,
  listed in the campaign report as excluded-dead (receipts: "four changes
  measured as no-effect while never executing"; tech-wall 0 executions in
  70 games; convoy knobs inert pending invasion wants).
- Knobs whose mechanism is venue-locked get their venue pinned in the
  block map (trade → classic; lift → island).
- The census script joins the campaign artifacts and reruns at freeze.

## 5. The separability pilot (gate 8)

W34 A.6's untested claim, now cheap to test: one small ECON-block pass
(few generations, few seeds) run twice — pinned ECONOMIC vs unpinned —
showing per-candidate objective variance FALLS under the pin. If it does
not fall, the premise of stages 1-2 is wrong; the campaign design reverts
to unpinned blocks and says so (the workstream's own honesty rule).

## 6. Protocol (the methods bar, all receipts)

- **Search objective**: T200-truncated paired diff (per-seed), r=0.77 vs
  final on primary — good enough to RANK candidates, never to report
  (receipt: turncorr.py 2026-07-28). Cramped is never truncated (r=0.41).
- **Gen-bests confirmed at full T300** before entering any holdout claim
  (candidate-level fade guard).
- **Three tiers**: train seeds (per-block) → holdout-select (fixed
  holdout file, tune.py's refusal guard active) → **virgin-confirm at
  n≥50-60 paired seeds** for any number that gets quoted (receipt:
  12-seed batch means are unusable — identical binaries scored 0.57-0.85
  across three batches; both G-final verdicts dissolved at n=60).
- **Per-seed paired stats only**; report wins/n and fired-on subsets for
  minority-firing changes.
- **Execution counters logged per candidate**; a candidate whose counters
  read zero for its changed knobs is flagged, not scored (H1's guard
  covers file integrity; this covers mechanism integrity).
- **Optimiser**: tune.py ES, population ≥16 (receipt: tune-B's 56 evals
  for 9 dims "is not a search"), generations capped by COUNT not hours
  (receipt: v3 box overrun), warm starts from the preserved lineage
  (v3b cand-5 +179.8 on six cells; G-final as the island-specialist data
  point; Z2-gen4 harvest for the fear family).
- **Traits/stance**: all campaign cells run the standing baseline — even
  traits, FREE identity stances; PINNED arms only where a stage's block
  map says so, and every result labelled with its mode (W34's "a tuning
  result taken free and reported as pinned is a wasted campaign").

## 7. Stop-and-diagnose triggers (the bug-detector budget)

Pause the affected block (not the campaign) and bank a finding when:
- a winner ZEROES a mechanism repeatedly across generations (Z2/H/H2
  pattern — it is telling you the mechanism is a tax somewhere);
- a winner SLAMS a registered bound for two consecutive generations
  (the bound is wrong or the mechanism is — "bound-slam diagnostics");
- train improves while holdout-select degrades for two selections
  (overfit signature, tune-A pattern);
- any candidate's counters go silent on knobs it changed (loader/logic
  defect — H1 class).
Each trigger's finding goes to findings/ with the candidate reconstructed
from the log (Z2 precedent: every candidate reconstructible).

## 8. Fold rules

A value changes a compiled default only if: virgin-confirm at n≥50 paired
shows the composed fold ≥ defaults (or the mechanism metrics justify a
neutral fold), the affected counters prove live execution, and the human
signs off per block. Partial folds by block are allowed; no value lands
"because the optimiser liked it" without its virgin number printed.

## 9. Ops

- Boxes: a6 us-east-1 `i-0e9a257ef3b781349`, b6 us-east-2
  `i-05162168cf393cb20` — STOPPED, never terminate, EBS caches preserved,
  DIFFERENT REGIONS. One box per concurrent stage at most; stages 1+2 may
  run in parallel (disjoint blocks, disjoint cells) if budget allows.
- campaign.sh inherits the recorded fixes: unset DEEP_QUIET for
  firstbuild stages; report.md computes paired ratios itself; `timeout
  -k` on self-stop; result archival under benchmark/results/ with the
  campaign name; cache keys on the frozen binary sha.
- Budget sketch: local measured 12-way ≈ 12 min per 20-game isolated arm,
  9 for lategame; prior three-box campaign ≈ $120. v4 is bigger (5
  stages, n=60 confirms) — order $200-400, bounded by generation caps.
  Refine when the census fixes the searched-dimension count.

## 10. Open questions for the human

1. Stage-3 pinning: expansion knobs under a pinned corner (which?) or
   unpinned as drafted?
2. Is M2 (naval slice 3) in or out of the gates? Out = convoy knobs
   excluded this campaign (drafted as out).
3. Do E34.4 / W35 E35.1 land before freeze (adding their knob families)
   or explicitly after (drafted: after — excluded from v4)?
4. Parallel stages on both boxes vs sequential on one (cost vs calendar).
