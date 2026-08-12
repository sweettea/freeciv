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
- **When to tune at all (human worry 2026-08-06: "maybe we don't tune
  till fully done with knobs?").** "Fully done" is a mirage — the knob
  count grew 121→168 in one week and the post-v4 backlog adds more; a
  completeness gate would defer the campaign forever. The freeze line is
  drawn by BLOCKS instead: v4 launches when the pre-v4 wave's gates are
  green, sweeps the blocks that exist, and later epochs (combat ladder,
  M2/M4, EXPANSION) re-tune ONLY the blocks they touch in mini-campaigns
  on the same infrastructure — the separability design, the sha-keyed
  cache, and the arena tier exist precisely so a later block costs a
  block, not a campaign. Two corollaries, both receipts-backed:
  (1) delaying has a hidden cost — session A/B verdicts are taken at
  untuned defaults, so a mechanism can measure negative purely because
  its default is wrong (the culture credit's short-venue −19 is the
  live example); the campaign explicitly revisits measured-negative-at-
  defaults mechanisms rather than treating retirement as final.
  (2) tuning early on a moving binary is the OTHER waste (the Z2/v3b
  receipt above) — which is why nothing tunes until the wave lands and
  the binary freezes. Between those two failure modes, the block
  freeze is the narrow path, and it is the standing plan.

## 2. Launch gates (all must be green)

| # | Gate | State 2026-08-05 |
|---|---|---|
| 1 | D1 military discipline landed + measured | **DONE 2026-08-05** |
| 2 | M3 small mechanisms (density-fear reachability gate esp.) | **DONE** (gate pre-existed; isolated by G1's rider) |
| 3 | G1 class-gated garrison floor landed + sized | **DONE** (flat floor measured as tax both cells; magnitudes to the pile) |
| 4 | D3 site truth (defence bonus, closed-water naval bonus) | **DONE** (two constants now knobs; defence weight 0 by measurement) |
| 5 | R2 verdict banked (done: deflator 7-9× measured); **R2b wonder-caravans reformed** (the larger claimant — the corpus must see it); R3 class-rescale corpus + deflators shipped | R2+R2b done (deflators ~8 trade / ~5 wonder, anchor stable); R3 queued |
| 5a | HEDGE (see 8e — scope grew: censuses + coinage/salvage legs + self-imposed H_PRODCHGPEN) | in flight |
| 6 | M1 combat E2 (cost-of-victory) + E4 aggregation swap (STATUS pins E4 pre-v4 explicitly) | queued |
| 7 | **Knob-coverage census** (§4) CLEAN: every searched knob fires on ≥1 training cell | **RED, measured 2026-08-05** (`findings/knob-coverage.md`): 54 LIVE / 53 NO-INSTRUMENT / 36 cleanly searchable of 121. The INSTR session (S-INSTR-observability.md) is the remediation; census reruns at freeze per its §7 |
| 7a | Stage-1 arms must pin `rates_enabled=1` (census: the rates model has never executed in any archived run); stage 3's venue pin is isolated-150 (lift family fires 60-100× more there) | folded into §3 by the census |
| 8 | **Separability pilot** (§5) passed | to run |
| 8a | **PEACE** (cease-fire trap + embassy pricing + trade population) — human call 2026-08-05: pre-v4 | queued |
| 8b | **ENDGAME** (endturn awareness / late-game score mode; MORT interaction) — pre-v4 | queued |
| 8c | **DARKFX** (gov-center, Player-range effects, MAX_RATES, buy-per-gold, specialist type, disaster risk) — pre-v4 | queued |
| 8d | **GROUND** (fortress want channel, pillage, capture instrumentation, fortify) — pre-v4 | queued |
| 8e | **HEDGE incl. self-imposed H_PRODCHGPEN** (human call: no AI exemption from the switch cost) — pre-v4; its churn-tax A/B feeds the freeze decision | queued |
| 8e2 | **TRADE portfolio session** (S-TRADE-routes.md: slot opportunity cost + replacement, partner-aware targeting, ceasefire truncation pricing, transit risk, cross-water caravans) — pre-v4, after PEACE | queued |
| 8g | **CONSEQ** (defensive tech-wall, sell-before-fall, upkeep rehoming), **WRACE** (wonder races, after R2b), **BALANCE** (gifts + proxy arming, after PEACE) — human call 2026-08-05: all pre-v4 | queued |
| 8h | **CM-completion** (posture counterfactuals + production-vs-growth arbitration in one currency) — human call 2026-08-05: pre-v4; it re-derives the cm_* factor states as priced argmax, shrinking the ECON block the campaign searches | queued |
| 8f | INSTR runs LAST of the mechanism sessions (its census rerun must capture all new knobs) | queued |
| 9 | Binary frozen at a named commit; `meson test` + `tune-audit.py` CLEAN; STATUS quiesced | — |
| 10 | Human go | — |

Cut-line if the human wants the run sooner: gate 6's naval slice (M2) is
already outside the gates; convoy-family knobs are then EXCLUDED from the
search (census rule, §4) rather than searched dead.

## 3. Blocks, their pins, and their venues

Principle: a block is tuned where its mechanisms have POPULATION, pinned
where pinning makes its objective separable (W34 phase A's purpose), and
its stage order respects currency dependencies.

**Stage 0 — RETIRED BY R3's MEASUREMENT (2026-08-06).** The corpus showed
the exchange-rate disease substantially healed structurally: the honest
anchor shrank the founder gap from 10-20x to 3.7-6.5x, the founder
deflator is exactly neutral at claim power (-0.05, t -0.02, n=40) while
costing 13% of cities, worker inflation is poisonous (bounded-by-
construction want), joint rescales are non-additive, and the military
scales have no derivation (anchor ratios INVERT between cells). All six
scale_* keys ship inert at 1.0 and are EXCLUDED from the search except
scale_settler on isolated-150 at a tight range if stage 3 wants it. The
cross-class currency is owned by the per-mechanism merits (R1/R2/R2b,
combat E2 for the emergency-tier attacker spikes — p50 6485, max 259,976,
which is where W01's disease actually retreated to). Receipt:
findings/want-corpus-2026-08-06.md.

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
5. **Venue portfolio (2026-08-06, `findings/tuning-venues-2026-08-06.md`)**:
   adopt the proposed additions — two mixed-table head-to-head cells
   (the statecraft block's only real population; today's suite is 100%
   homogeneous self-play), a raging-barbarian `storm-150` (MIL block's
   home; the a6-disarmament antidote), `longhaul-300` (culture/endgame
   horizon), the per-cell z-floor aggregation rule (no cell below
   z −0.5), and holdout VENUES (Europe scenario + a `wildcard` cell)
   alongside holdout seeds? Harness cost ~one morning (team-diff column
   + 4 configs), rides INSTR.
6. **Centers**: v4 sweeps start from defaults ⊕
   `benchmark/tuning/principled-v1.ini` ⊕ R3's corpus scales
   (`findings/tuning-first-principles-2026-08-06.md`); baseline arm
   stays pure defaults. Sign-off?
7. **GROUND's `aw_task_value_fn` widening** (STATUS.md GROUND §7,
   2026-08-06): rule 4's inert-extension exception was read to permit
   widening an already-registered hook, not just adding a new one, so
   the fortress/irrigation want could see the same tile's competing
   activities. No consumer-behaviour change with nothing registered;
   the frozen baseline never registers one. Recorded so the call "can
   be reversed if the human disagrees" — it has not yet been reviewed.
   **UNDECIDED. Sign-off?**
8. **BALANCE's mixed-arm-mandatory rule** (`findings/balance-
   statecraft-2026-08-06.md` §0, 2026-08-06): the love-buying gift
   measured as the largest single-lever self-play move this project
   has taken (+8.28 contested / +8.92 lategame-80), and the mixed
   head-to-head showed it is a transfer, not an advantage (deep +1.93
   t+0.72, counterparty +5.26 t+2.15, deep's margin over `ai/default`
   moves −3.34) — "an all-deep cell cannot tell a positive-sum
   mechanism from a competitive one, and the project's primary metric
   lives in all-deep cells." **ADOPTED 2026-08-12 — see §11.4.**

## 11. Adopted doctrines (human sign-off log)

Decisions the human has actually made, as opposed to §10's open
questions. Dated, with the receipt that drove each one.

### 11.1 Per-venue objectives (2026-08-10/11)

- **War venues** (contested, `tech50war-200`, `storm-150`, `cramped`,
  the MIL-block cells of §3 Stage 2) gate on the **MIXED head-to-head
  objective** — deep vs stock, seat-mirrored — **plus outcome facts**:
  wins, cities, eliminations. A war venue is never reported as one
  scalar.
- **Econ venues** (`garden2b`, `isolated-150`, `primary-120` and the
  other ECON-block/§6 search cells) gate on **PAIRED score** — deep vs
  deep, the standing §6 search objective.
- **Never one scalar across venues.** A PAIRED-score read and a MIXED
  head-to-head read of the same venue are different questions with
  different answers, and both classes of venue need their own gating
  metric, not a shared one.
- **ENDGAME caveat**: score is the ratified win condition only at
  endturn (gate 8b). On a war venue reporting PAIRED score before
  endturn is a proxy for the thing that actually wins there — the
  MIXED reading (who took the cities, who is still alive) is closer to
  what the venue is for.
- Receipt: `findings/tech50-collapse-2026-08-10.md` — the PAIRED form
  of `tech50war-200` (deep worlds −172/game, 2 seeds) and the MIXED
  form (deep −115.7/seed, 1–5 in games, 3 seeds) told the same
  direction but only the MIXED reading plus the outcome table (cities
  113 v 132, kills 1007v1860, standing army 296v455) made it legible as
  "the strongest known gap in the project" rather than a score delta.

### 11.2 MIL block-freeze (2026-08-10/11)

- **No MIL tuning** (§3 Stage 2) until the finale. This formalizes what
  STATUS.md's M4b session already flagged ("MIL tuning is DEFERRED to
  the block-freeze finale") into a standing campaign rule rather than a
  session note.
- **The finale's blend must include a tech-50 venue** — `tech50war-200`
  or a sharpened successor. Not optional, not substitutable by an
  early-era cell.
- Evidence: an 8-seed MIL evolution run destroyed ~48 pts of real mixed
  edge — tuning the MIL block on early-era-only venues optimises a
  fighter for a war the finale's own venue portfolio doesn't end at.
  The tech-50 collapse was invisible to every pre-Gunpowder venue: per
  `findings/tech50-collapse-2026-08-10.md`, "every combat venue in the
  canon starts at tech 0 and ends by Gunpowder–Sanitation... The first
  six games ever played at Gunpowder+ inverted the sign." The mechanism
  diagnosis (force-ratio, not threshold — massing brings 0.69–2.69
  attackers against defences needing 8–15) is
  `findings/era-probe-evidence-2026-08-12.md` §6; MIL tuning ahead of
  that fix would be tuning around a known, unaddressed defect.

### 11.3 pv2 adoption (2026-08-12)

- The d6 ECON-block winner is adopted as the composed baseline
  (`bec743c69f` — STATUS.md S5, "Both branches merged; pv2 adopted").
- Holdout deltas: **−10.1** `lategame-80`, **+55.0** `isolated-150`,
  **+43.1** mixed contested.
- Its apparent ~12/seed mixed shortfall against untuned pv1 is **null**:
  paired t = −0.38, n = 20 — not a real regression, per §11.1's own
  rule this is exactly the case where the MIXED read and a naive PAIRED
  delta could be mistaken for disagreeing, and the paired-t check is
  what confirms they don't.

### 11.4 Mixed-arm-mandatory for cross-border mechanisms (2026-08-12)

Human sign-off, with two refinements over the §10.8 proposal:

- **Scope by property, not block name.** The rule covers any mechanism
  whose primary effect crosses a player boundary — gifts, tech
  gifting, proxy arming, embassies, cease-fire pricing, trade-route
  PARTNER selection, future ally-arming — wherever it is filed.
- **The fold criterion.** A cross-border mechanism folds only with a
  mixed head-to-head arm showing deep's margin is a wash or better
  (not significantly negative). Positive-sum is not a defect per se:
  the human's words — "i don't mind multiple folks getting better,
  but it _is_ more fun to get a wash as an alliance benefit." Surplus
  directed INSIDE the alliance is a feature; surplus that subsidizes
  rivals is the failure mode the love-buying gift demonstrated.

Consequence: the love-buying gift is re-openable as an ALLY-TARGETED
mechanism — gate the gift want on stance/alliance so the +5.26
counterparty surplus lands on friends, then re-take the mixed arm.
