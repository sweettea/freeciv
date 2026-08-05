# Mechanism map — what deep has built, what it wastes, what would make it smarter

2026-08-04. Synthesis of a five-reader audit of `ai/deep` (~15k lines) at HEAD
`36bac91163`: four domain readers (economy/growth, expansion/logistics,
military, deep↔stock seam) plus an omniscience census. The full reports, with
every file:line citation, are in `findings/deep-dive-2026-08-04/`. This
document is the consolidated verdict and the ranked action list. Everything
cited here was verified by reading at that SHA unless marked inferred; the
E34.2 gate commits were already in the tree when read.

**The one-paragraph verdict.** Deep's mechanisms are individually careful and
collectively deaf. The audit found the project's own signature pattern —
signals computed and consumed by nothing — reproduced *inside* deep at scale
(a wait census nothing reads, a combat distribution collapsed to one scalar,
a blocker ladder that only prints), plus three classes of verified defect
that between them plausibly explain two standing mysteries: why W03's growth
credits land mechanism after mechanism without moving score, and why three
military disciplines measured inert. The cheapest large wins are not new
mechanisms: they are five-line fixes, couplings between mechanisms that
already exist, and ownership of two stock decisions (rates, government) whose
seams are already open.

---

## 1. The verified defect ledger

Ranked by (evidence strength × blast radius ÷ fix size). Every item is
read-verified, not benchmarked; each needs its own A/B when fixed.

### D-A. The `done`-flag reset silently disables three landed mechanisms
`deepops.c:1903` overwrites every non-op unit's `done` flag unconditionally,
*after* the first-activities passes that set it. Erased every turn: the
tired-attack hold (unit logged as held, then attacks anyway), the convoy HOLD
(boat sails regardless — STATUS's "verified inert" has this second,
independent cause), the worker-flee done-mark (fled workers re-tasked toward
the threat same turn), and the consolidate garrison's fresh `DEFEND_HOME`.
STATUS 2026-08-01 records finding exactly this hole for op holds and fixing
only op holds. ~5-line fix plus a phase-scoped marker (the flag is also stale
from the previous turn). *(military C1)*

### D-B. Horizon credits accumulate 1–9× across stock's recalc schedule
`deep_adjust_building_wants()` does `building_want[id] += add` **every
turn**, but stock clears the array only every `fc_rand(5)+5` turns
(`daicity.c:1985-2079`). The credit a city sees is a phase-randomised
sawtooth, mean ~4.5×, clipped at `DAI_WANT_DOMESTIC_MAX` 750.25. This is a
simpler explanation than "weight too hot" for the happiness-building 10×
overshoot, is consistent with the settlers-share ablation, and inflates
within-arm variance in every credit A/B ever run. **Any tuning pass on
`horizon_*` before this is fixed tunes against the sawtooth.** *(economy 3.3)*

### D-C. The capacity credit dies in exactly the city it was built for
Two independent causes. (1) With `aqueductloss=0` (every benchmark config), a
capped city's food box stays pinned full, `city_turns_to_grow()` returns 0 by
integer division, and `deep_capacity_credit()` bails — so the Aqueduct credit
is nonzero only in a brief refill window and permanently zero thereafter; the
tech-wall route dies with it. (2) Independently, the CM's cap state sheds
food to surplus 0, ttg becomes FC_INFINITY, and the credit's "cap is not the
binding constraint" comment fires exactly when the cap IS the binding
constraint. The CM manufactures the condition under which the credit refuses
to pay; the two run at opposite ends of the turn and share no state. This is
a second and third cause of the Europe zero-Aqueducts evidence that STATUS
attributes wholly to the war gate. Fix shape: trigger on a *counterfactual*
ttg (what growth would be if the cap lifted). *(economy 3.1, 3.2)*

### D-D. Deep imports the inverted defence bonus it documented as avoided
`deep_city_desirability()` calls stock's `result_defense_bonus()`
(`deepsite.c:920`) — upstream bug #1, higher terrain defence scores *lower* —
directly contradicting 00-context §5's "ai/deep simply doesn't have that
bug." Same call site imports `naval_bonus()`, paying a maritime premium for
water the build side's own `boat_water_gate` refuses to sail; the geometry
answer (`deep_water_bridges`) is extern and free. *(expansion 5.1, 3.2)*

### D-E. A held lift passenger's mission is never re-validated
The op-hold early return (`deepsettler.c:1501`) sits above the
`city_can_be_built_here()` check (`:1525`); the beachhead is fixed at
formation. A target founded on mid-crossing still gets a full delivery onto
an unfoundable tile, and the metric counts it `lift-arrived`. Frequency
unmeasured — nothing logs it. *(expansion 3.5)*

### D-F. Build side and ops side disagree about targets, from the wrong anchor
The ops target menu exempts barbarians from the contact gate; the build-side
assault want does not (comment claims they share gate logic) — so post-T160
barbarian reconquests form as ops the build side never funds. The assault
want also prices the marginal attacker against ALL free attackers on the
continent as one stream, so with four concurrent ops Δp≈0 and attacker
production stops exactly when campaigns run. And `deep_op_form` anchors
target scoring at the *first city in the player list*. *(military C4, C5)*

### D-G. Probability available, presence-count used — four more instances
The 2026-08-01 pass replaced urgency/grave_danger with survival probability
in four places and missed four more: the melt (which also skips the
`_without` survival re-check and the op-claim check), the emergency triage
sort (decides which dying city gets the gold), the veteran-building guard,
and the severe-danger attacker cutoff. The sortie passes are also the only
unit-claiming pool that skips `deep_op_claiming` — a MARCH member can be
poached as a bodyguard mid-crossing. *(military C2, C3, C7)*

### D-H. Small sharp edges
- `caravan_discount = 1.0` is accepted by deep's guard, rejected by
  `caravan_parameter_is_legal()`, and inside the registered tuner range —
  a drawn endpoint makes every destination score inf/NaN. *(economy 3.13)*
- The `deep-tuning` log line omits `horizon_multiplier_weight`,
  `caravan_discount`, `caravan_foreign_trade`, `site_travel_*` — unloggable
  knobs are the exact hazard that cost a 40-game batch. *(economy 3.14)*
- `upkeep_gold_weight` subtracts one turn of gold from a MORT-horizon
  amortized value (dimensionally wrong) and lives in the MIL/S tuning blocks
  while taxing every ECON credit — the block-assignment error STATUS already
  named for `survival_target`, still live twice. *(economy 3.11)*
- The bundle credit pays for half a bundle regardless of whether the partner
  extra is ever reachable (no tech/worker check). *(economy 3.12)*
- `site_enough_result` is half-wired: the tunable exists but the search's
  early break still uses compile-time `RESULT_IS_ENOUGH`. *(expansion 5.2)*
- Two comments overclaim honesty: `deepcombat.c:182` ("Real observed units
  only" — no observation gate exists) and `deepdefence.c:736-743`
  (`score.units` called public — the historian broadcasts rank+name only).
  *(census 5.6)*

---

## 2. The unconsumed-signal ledger

The pattern 00-context §3 documents in stock, now in deep:

| Signal | Where | What could read it |
|---|---|---|
| Combat HP distribution + attacker attrition | `deepodds.c` computes it; every one of 19 consumers reads one scalar | cost-of-victory term in assault want; the W14 war ledger's missing input |
| Wait census (per-passenger spells) | `deepops.c:785` | the W36 surviving lead: expected-wait pricing of overseas candidates |
| Rendezvous cost `meet_turns` | `deepops.c:1311` | form-or-don't, wait-or-walk comparisons |
| Blocker ladder (why a mission can't route) | `deepsettler.c:465` | mission-drop policy re-derives a coarser answer |
| Colony-demand LOCATION | logged, scalar kept | the shipyard never learns which water demand pointed at |
| Contest margin (how much sooner a neighbour claims a tile) | computed, collapsed to bool | site scoring under contest |
| `deep_committed_away` | computed, ×1.0 placeholder | committed-army fear |
| Idle-work census by continent | `deepsettler.c:907` | the cross-water worker fallback acts on exactly this and doesn't read it |
| `WIN_CAPITAL` (stock side) | computed in daidiplomacy, read by nothing | ops target bias when a rival leads the space race — cheap, W04-independent |
| caravan NPV `result.value` | used as `>0` gate only | the caravan build want (R2's whole subject) |

Kills are never counted anywhere (losses are), so raid/sortie/op value is
structurally unmeasurable at runtime — the other half of the war-ledger gap.

---

## 3. Ranked coupling opportunities (emergent interactions)

1. **Attrition out of the survival substrate** (military U1→G1). Extend
   `deep_stack_survival` to report expected attacker losses; subtract a
   cost-of-losses term in `deep_assault_want`, same currency. Unblocks the
   war ledger and the raid metric at once. The single highest-leverage
   coupling in the audit.
2. **One `threatened` verdict per city per turn** (economy 3.4 + military
   C7). The CM, the horizon gate, the triage sort, and the melt currently
   answer "will this city hold" with two thresholds and two currencies.
   Compute once, read everywhere; removes a variance source from every ECON
   tuning cell.
3. **CM ↔ horizon**: price the marginal citizen with the CM factors that will
   actually place it (economy 3.5), and give the CM a "developed city" state
   (3.6) plus a solvency state coupled to the treasury guard (3.7).
4. **Site ↔ delivery**: overseas candidates discounted by beachhead
   hull-reachability (test already exists in the lift) and expected wait from
   the wait census tail (expansion 3.1) — the surviving W36 lead.
5. **Ops ↔ expansion**: don't price down ground an own op is about to take
   (`deep_op_targeted()` is O(8)); weight targets by settling room opened
   (expansion 3.4).
6. **Trade want from the real destination NPV** (economy 3.8 — R2's session,
   confirmed from both sides) plus a danger/escort term from machinery deep
   already owns (3.9).
7. **Lift as military reinforcement**: give the counter-claim/draft a
   cross-water producer (military U2/C8) — the lift already carries anyone
   with a cross-continent goto; nothing military ever sets one.
8. **Stance gates into the CM and trade** (economy 3.10): the sharpest
   econ/mil trade-off in the game (food vs shields, tile by tile) is ungated;
   an econ player doesn't look further for routes.
9. **Explorer want shaped for the honest day** (expansion 3.3): discovery
   term weighted by reachable-continent unexplored land × remaining sites —
   inert today, load-bearing the day the flag flips.

## 4. Ranked intelligence gaps (what no mechanism attempts)

1. **Cost of victory** (RC4/RC1) — merged with coupling #1.
2. **A settling order** (RC2/RC3): site reservation ahead of reach, sequenced
   next-N-cities, instead of a per-settler argmax every turn; `citymap` is
   half-used already. Planned lifts (intent → operation) are the naval half
   of the same gap.
3. **Cities have roles; the empire allocates** (RC2/RC4): the structural
   ceiling on every per-city credit — an unarbitrated auction is why
   mechanisms land and score doesn't. Wonder-city choice (stock, shield-only)
   is the existing single instance of a role.
4. **Rates and celebration as strategic levers** (RC5/RC4): R1's session,
   now including celebrate-to-grow (human call 2026-08-04); deep never pulls
   the one lever `dai_manage_taxes` already tests feasibility for.
5. **Timing windows** (RC6/RC2): attack while a rival fights elsewhere; no
   `pplayers_at_war(a,b)` read anywhere in the military files; WIN_CAPITAL
   thrown away.
6. **Force concentration** (RC2): merge under-strength ops; one decisive
   campaign instead of four marginal ones.
7. **Colony value net of holding cost** (RC1/RC4): garrison/upkeep/
   worker-distance terms in site scoring — the evidence-heaviest gap
   ("colonies land and are not worth what they cost") but the widest blast
   radius.
8. **Corridor security for civilians** (RC5): staging and rendezvous for
   escorts — the lift already solves this shape for boats.
9. **Corruption/distance shape** (RC3), **wonder economics** (RC4),
   **population moves** — recorded, behind the above.

## 5. Unowned decisions worth taking (from the seam map)

| Decision | Seam cost | Status |
|---|---|---|
| Rates (tax/sci/lux) | zero — `dai_manage_taxes` already exported; swap one call | R1 session, brief updated for celebration |
| Government valuation | zero — `gov_value` is a forwarded vtable hook; stock's version is RC1 at its purest (values at today's size) and already silently reads deep's CM params | NEW session row |
| Tech selection beyond credits | `dai_manage_tech` public; reusing valuation needs E-B.3 (`base_want` still static) | E34.4 + later |
| War/targets | needs `dai_war_desire`/`war_countdown` exports; correctly deferred behind W04 — except the free WIN_CAPITAL read | slice into M-queue |
| Wonder-city choice | forwarded hook | small, queue |
| Air, spies | W32/W35 as planned | unchanged |

## 6. What this does to the session ladder

- **New defect round D1 (military discipline):** done-flag fix + sortie
  claim-discipline + melt survival check + triage on probability + barbarian
  gate + target anchor (§1 D-A, D-F, D-G). Before any military measurement.
- **New defect round D2 (credit integrity):** sawtooth + counterfactual ttg +
  CM/horizon deadlock + shared threatened verdict + upkeep dimension + bundle
  reachability + block assignments + tuning-log completeness + caravan guard
  (§1 D-B, D-C, D-H). **Must precede the R3 corpus and any tuning anywhere**
  — both currently measure the sawtooth.
- **New defect round D3 (site truth):** defence-bonus correct behaviour in
  deep + closed-water naval bonus + held-lift re-validation + enough-result
  wiring (§1 D-D, D-E).
- R1 (rates) gains celebration; R2 (trade) confirmed from both sides; M1
  (combat E2) should absorb coupling #1 (attrition/cost-of-victory) — it IS
  force sizing.
- A doc-hygiene pass (orchestrator work, not a session): the ~20 stale-plan
  items across the five reports, including the 00-context §3 table updates
  (cm_parameter now set, is_pos_dangerous partially assigned, TIMING_LOG
  option, player_save wired) and the recovered posture notes re-anchored.
