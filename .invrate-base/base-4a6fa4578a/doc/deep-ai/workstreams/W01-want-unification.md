# W01 — Want unification

**Phase 2 opener. Hard ordering: before W02/W03 (they manipulate want
magnitudes). Extracted from PRD-v3 §4.3 + findings/better-ai-plan.md
on 2026-07-25; evidence re-verified against `0896b34`.**
Root cause: RC5 (planning and execution disagree about value).

## Problem

Every advisor prices its wants in a different, incommensurable
currency, then dai_city_choose_build() compares them as if they were
one number. Nothing new can be priced in honestly — W06's explorer
want lost every auction not because exploration is worthless but
because there was no principled exchange rate to bid with.

## Evidence (verified at 0896b34)

The currencies in circulation:

- **Output weightings** — `server/advisors/advbuilding.h:19-29`:
  `FOOD_WEIGHTING 30`, `SHIELD_WEIGHTING 17`, `TRADE_WEIGHTING 18`,
  `POLLUTION_WEIGHTING 20 /* Tentative */`. Unitless relative prices,
  uncalibrated against anything.
- **Military wants are percentages** — `daimilitary.c` defender want
  is "danger as % of our defence", capped at `200 + urgency`;
  compared directly against domestic wants that are weighted-output
  sums.
- **Priority by magnitude** — `daitools.h:31-33`:
  `DAI_WANT_BELOW_MIL_EMERGENCY 1000.0`,
  `DAI_WANT_MILITARY_EMERGENCY 1000.1`,
  `DAI_WANT_DOMESTIC_MAX (…/4*3) = 750.25`. Emergencies outrank
  everything by being *numerically bigger*, so every producer must
  know the tier constants to stay in its lane.
- **Sign overloading** — `daicity.h:60-64`: negative
  `founder_want`/`worker_want` means "needs a boat first; -value is
  the degree of want". (The PRD's `-199 == get_a_boat` sentinel is
  stale — it has already evolved into this sign convention, which is
  arguably worse: arithmetic on the value silently breaks.)
  `choice.want = -1` doubles as a "already processed" marker
  (`daicity.c:578, 965`).
- **Traits corrupt scale** — building want × BUILDER trait
  (`daidomestic.c:620`), founder want × EXPANSIONIST × ai_common
  expand (`daidomestic.c:558`): preference multipliers applied to
  *incompatible* scales, so a trait doesn't shift a tradeoff, it
  shifts a currency.
- **The W09 connection** — `cityturn.c:314`'s comment ("those values
  are on a different scale") is the same disease: citizen arrangement
  cannot read strategic priorities because they have no common unit.
- **Lived confirmation (Phase 1)**: deep's explorer want (W06) logged
  values 39-94 and lost every auction; no tuning of a lone constant
  can be principled when the competing bids are in different units.

## Design

One unit — **discounted shield-equivalent output over horizon H**:

```c
typedef double want_t;   /* shield-equivalents, discounted.
                          * No caps, no sentinels. */
```

Placement note (post-revision rules): ai/default cannot be touched, so
unification happens at the boundaries deep already owns —
deep_manage_cities(), deep_military_choose_build(), deep_spend_gold()
and a deep derivative of dai_city_choose_build(). Stock producers keep
emitting their native currencies; deep converts each at the boundary
with an explicit per-source conversion (initially rank-preserving), so
the first landing is behaviour-neutral and later workstreams replace
conversions with real valuations one producer at a time.

1. **struct deep_want { want_t value; enum deep_want_tier tier; }** —
   tier (EMERGENCY / NORMAL) replaces priority-by-magnitude; explicit
   `need_boat` bool replaces the sign overload where deep touches it.
2. **Boundary conversion table**: per producer (domestic building,
   defender, attacker, worker, founder, explorer), a conversion
   function to want_t. First landing: monotone rank-preserving maps
   calibrated so current behaviour is preserved (±2% gate).
3. **Emergency becomes tier**, checked before value comparison;
   the 1000-scale constants stop leaking into deep code.
4. **Traits multiply want_t only** (preference within one currency).
5. Conversion constants that are not derivable from production go into
   struct deep_tuning as placeholders (rule 7).

## Acceptance

- **Behaviour gate (the PRD's own gate for W1): no regression vs
  pre-W01 deep — score within ±2% over 50 paired seeds** on smoke and
  contested; the first landing is a refactor, not a behaviour change.
- Every deep build decision flows through want_t + tier: grep-able
  invariant, no DAI_WANT_* magnitude comparisons left in ai/deep
  outside the conversion table.
- A deep-want log line per build choice (producer, native value,
  want_t, tier) so later workstreams can calibrate conversions from
  data.
- W06's explorer want re-expressed in want_t: with the discovery term
  priced as expected shield-equivalent of hut yields + tile knowledge,
  the auction becomes winnable in hut-heavy games *without* hand
  tuning (directional check, not a threshold).

## Commits (sketch)

```
ai/deep: define want_t and the want tier enum
ai/deep: add a per-producer want conversion table
ai/deep: route city build choice through converted wants
ai/deep: replace emergency magnitudes with the tier field
ai/deep: express the explorer want in shield-equivalents
test: assert no magnitude-tier constants in deep want paths
doc: record want calibration data from the boundary log
```
