# Tuning defaults from first principles — a Fable pass over all 168 knobs

2026-08-06, orchestrator (Fable). Inputs: the full `deeptuning.h` docs, the
shipped defaults, `benchmark/tuning/a6-best.ini` (evolution's best from the
a6 campaign), the rulesets, and the game arithmetic (`MORT 24`,
`advtools.c:amortize`, `barbarian.c`, buy-cost curves, score formula).

**What this is:** derived CENTERS for the v4 campaign and a principled arm
for derbies — not new code defaults. Rule 7 compliance: nothing here is
"seems reasonable"; every proposed number shows its arithmetic or its
evidence, and everything stays swept by the pile. Deliverable ini:
`benchmark/tuning/principled-v1.ini` (overlay on compiled defaults).

## 0. How to read evolution's file (a6-best.ini)

Three classes of verdict, and they must not be pooled:

1. **Convergent with derivation** — trust these. The flagship:
   `caravan_discount` evolved to **0.9634**; consistency with the MORT-24
   amortization horizon derives `1 − 1/24 = 0.9583`. Two independent
   methods, same answer (stock's 0.95 was already close; the point is the
   *reason*). Similarly `explore_hut_value` evolved 58 against a shipped
   placeholder 15 — and the hut's own expected value supports it (§3).
2. **Venue artifacts — do NOT adopt.** Evolution zeroed the entire
   standing-fear/military apparatus: `garrison_floor`, `garrison_floor_danger`,
   `barbarian_fear`, `boat_shadow_fear`, `density_fear_value`,
   `density_prior_value`, `wall_fear_weight`, `overseas_fear`,
   `survival_fear_value`, `attack_value_weight`, `op_take_target`,
   `raid_radius` — all 0.000. On a6's cells (short horizon, stock rarely
   attacks effectively) insurance never pays and the GA correctly strips
   it. Derby 2 ran the counterfactual at scale: an unpunished-disarmament
   optimum met a raging-barbarian venue and lost 107 cities in 70 turns.
   This is the single strongest argument for v4's venue pinning (MIL
   blocks sweep on war cells only), and these zeros are hereby recorded
   as *cell diagnoses*, not values.
3. **Stale basis.** a6 predates the W02b catchment stream (its
   `site_tail_weight`/`site_depth_weight`/`site_overlap_weight` keys no
   longer exist), predates D2's credit-integrity fixes (its horizon-ladder
   weights were fitted to the sawtooth), and predates G1's floor classes.
   Its site_* and horizon_* magnitudes carry no authority on today's
   binary; only their *directions* are weak evidence.

## 1. Derived centers (the arithmetic)

| knob | shipped | proposed center | derivation |
|---|---|---|---|
| `caravan_discount` | 0.95 | **0.9583** | 1 − 1/MORT: one patience constant for the whole codebase; a caravan's per-turn discount should equal the horizon every other credit amortizes over. Evolution: 0.9634 — agrees. |
| `survival_target` | 0.50 | **0.85** | The DP's "city survives the observed wave" bar. Expected-loss balance: fear should fire while p(loss) × defence_worth_mult(2) × city value > marginal defender cost; any real city is worth hundreds of shields against a ~20-shield defender, so tolerating a coin-flip (0.5) is indefensible. 0.85 still accepts 15% overnight risk before panic. Evolution never moved it (flat gradient at its disarmed optimum — fear value was 0 so the target gated nothing). |
| `op_take_target` | 0.70 | **0.60** | Since `op_net_value = 1` (2026-08-06), the go/no-go already subtracts expected losses at shield weight; the odds bar's remaining job is filtering hopeless formations, not pricing. 0.7 double-charges caution. Keep `op_abort_floor` at bar/2 (**0.30**) to preserve the hysteresis ratio. |
| `invasion_reinforce_rate` | 0.34 | **0.20** | Virtual defenders per sail-turn. A defended target city rebuilds one ~20-shield defender per 20/surplus turns; at a typical 4–5 shield surplus that is 0.20–0.25/turn. 0.34 assumes a war-economy city. (Buy-response argues higher; the range keeps 0.34 inside.) |
| `settler_fearfulness_mult` | 2.0 | **1.5** | Loss arithmetic: settler = 30 shields + 1 pop (~20 food ≈ a citizen-turn stream) vs a ~20-shield military unit → ratio ≈ 2.0 justifies the shipped value at spawn, BUT the walk already sunk is double-counted by the pathfinder's own per-tile pricing. Evolution: 0.95 (braver, peaceful cells). Center between arithmetic and evolution, war-venue sweeps decide. |
| `diplomat_defence_value` | 300 | **225** | Its own doc: sit *just above* the defender-emergency corpus (Phalanx p90 native 1224 → tier 224). 300 overshoots the stated anchor by a third; 225 IS the stated anchor. Evolution: 150 (below — venue artifact, diplomacy-light cells). |
| `cm_grow_below` | 6 | **8** | The ruleset anchors this: both classic and civ2civ3 hard-cap size at 8 without an Aqueduct. Grow-hard exists because growth compounds; stopping the state two sizes short of the cap forfeits the two cheapest compounding steps. (Evolution set 1 = state off, while *also* cranking `cm_grow_food` to 26 — a hitchhiker pair fitted to the pre-D2 credit regime; direction unusable.) |
| `cm_grow_safety_margin` | 1.0 | **1.5** | Above 1 prices sequential attrition (the doc's own reading); the survival substrate's chained-duel model shows linear sums undercount attackers' compounding odds. Evolution: 1.59 — agrees from data. |
| `worker_danger_discount` | 1.0 | **2.0** | Evolution's 2.03 is one of its few *risk-averse* moves — striking against its disarmament elsewhere, so it is load-bearing: losing a worker loses the improvement AND the walk, and the discount is the cheap substitute for the flee/guard machinery it zeroed. |
| `explore_hut_value` | 15 | **50** | Classic hut outcomes: 25 gold (common), 50 gold, a tech (stock prices ≈ 3× remaining bulbs — hundreds of gold-equiv), mercenaries, rarely a city; blended EV ≈ 40–60 gold-equiv even with the barbarian branch. The shipped 15 prices a hut at half its *worst* common outcome. Evolution: 58 — agrees. |
| `explore_discovery_value` | 0.05 | **0.30** | Evolution moved it 7× (0.365) and the derby adds mechanism: map knowledge feeds site scoring, and the fog-refresh/patrol value (non-omniscient roadmap) consumes the same term. Between shipped and evolved, nearer evolution. |
| `endgame_horizon` | 0 | **1** | Not a preference — a correctness clamp, arithmetically inert until `end_turn − turn < MORT` (the counter proves it). Every benchmark cell and every derby sets endturn. The only argument for 0 is comparability with pre-ENDGAME measurements, which a campaign baseline arm preserves anyway. |

## 2. Already-principled (documented here so nobody "fixes" them)

- `trade_route_love = 5.0` — hidden derivation worth stating: stock decays
  love 4%/turn and prices peace at req_love ≈ 125; equilibrium love of a
  single route at 5/turn is 5/0.04 = **exactly 125**. One live route
  sustains precisely peace-bar affection; two make it robust against the
  50/turn best_target bleed. The "placeholder" is in fact the fixed point.
- `conquest_worth_mult = 2.0 / defence_worth_mult = 2.0` — the denial
  argument gives exactly 2 in a bipolar contest; with k independent
  rivals the denied value spreads and the multiplier tends to 1 + 1/k.
  Benchmarks are bipolar (deep vs stock), so 2.0 is *derived* for the
  measurement venue; free-for-all derbies would justify ~1.5. The ratio
  stays the tunable tilt (human call).
- `op_hold_survivors = 1.0` — you need one living unit to hold a city;
  the floor is the definition.
- `site_travel_discount = 0.9` band [0.75, 1.0] — human call 2026-08-02,
  reasoning recorded in the header. (Evolution's 1.295 predates the W02b
  currency; inadmissible.)
- `floor_interior_weight = 0.25` — deliberately reuses
  `menace_overseas_weight` rather than minting a number.
- `founder_pool_per_site = 0.06` — derivation recorded in-header; the
  3.5× cell disagreement is a per-cell campaign question, not a center
  question.

## 3. Knobs whose *shape* changes before their value matters

- `barbarian_fear`, `floor_barb_weight` — GROUND item 5 rewrites both as
  base × (onset-lead ramp × (barbarianrate−1) × empire-size mirror of
  `barbarian.c:581-613`). Center-picking the flat versions is wasted
  effort; the campaign should sweep the reshaped bases.
- The first-defender flat 100 (CONSEQ item 6) — same story.
- `upkeep_gold_weight` (0.125) — the header admits it owes a pass after
  its dimension correction, and R1 gave deep a *computed* marginal gold
  price; the principled endpoint is consuming R1's price, not tuning a
  constant. Flagged for CONSEQ-adjacent follow-up; a6's 1.5 is in
  pre-correction units and cannot be transplanted.

## 4. Explicitly empirical (the tuner's, with sane shipped centers)

Fear magnitudes (`density_fear_*`, `boat_shadow_fear`, `overseas_*`,
`survival_fear_value`, `garrison_floor_danger`), the six stance gains and
`stance_trait_span`, all `scale_*` (R3's corpus file is the center),
`emergency_value_cap` (corpus), CM factor tables (war/cap/wonder), the
lift/convoy window family (time-geometry, venue-dependent), gift/proxy
gains (BALANCE ships them 0), `trade_merit_gain`/`wonder_merit_gain`
(calibration residues), rates gains. No derivation pretends otherwise.

## 5. Campaign guidance distilled

1. v4 centers = compiled defaults ⊕ `principled-v1.ini` ⊕ R3's corpus
   scale file. Baseline arm stays pure defaults for continuity.
2. Never sweep the MIL block on a peaceful cell — a6's disarmed optimum
   is what the optimizer will always find there (§0.2), and derby 2
   shows what it costs on any venue with teeth.
3. a6 values on deleted/stale keys are historical only; do not port.
4. Where evolution and derivation agree (§1 rows marked "agrees"), treat
   the center as strong and narrow the sweep range; where they disagree,
   the disagreement IS the experiment.
