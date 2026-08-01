# The happiness column of the horizon (2026-08-01)

Why deep builds aqueducts and not temples, and what it costs.

Companion to `workstreams/W03-horizon-improvements.md`. Everything here
is measured on the current binary unless marked otherwise.

---

## 1. The question

The 2026-08-01 handoff recorded the ladder gap as an open problem: in
tech50 cells deep builds MORE ladder per city than stock but it is all
CAPACITY, with Temple 0/5, Marketplace 1/4. It predicted:

> if the ladder improves it will be via the fear knobs in MIL, not the
> horizon weights in ECON. If ECON moves and happiness buildings do not,
> the gap needs a NEW credit term, not tuning.

The prediction was right about the conclusion and wrong about the test.
**No knob in any block can move the happiness buildings, because their
want is structurally zero — and you cannot tune a zero.**

## 2. Why a Temple's want is exactly zero

Not small. Zero, by construction, in three steps:

1. `server/advisors/advdata.c:91-100` — `adv_data_city_impr_calc()`
   decides how each improvement gets valued. The entire happiness
   family, `EFT_MAKE_CONTENT` / `FORCE_CONTENT` / `MAKE_HAPPY` and
   friends, sits inside an `#if 0` with a `/* TODO */`. So any building
   whose only effects are content effects keeps `ADV_IMPR_ESTIMATE`.

2. `ai/default/daicity.c:1529` — `base_want()` opens with
   `if (adv->impr_calc[...] == ADV_IMPR_ESTIMATE) return 0;`.

3. So the whole want is the effect WAG,
   `dai_content_effect_value()` (`ai/default/daieffects.c:77`):

   ```
   v = MIN(amount, unhappy_now + entertainers_now) * 35;
   if (num_cities > 1) { ... }          /* city-range effect: num_cities == 1 */
   ```

   The second term is skipped for a city-range effect, so a Temple is
   worth `35 × min(amount, citizens unhappy RIGHT NOW)` and **nothing
   at all** in a city that is not currently unhappy.

Verified against civ2civ3: every effect gated on a Temple is
`Make_Content` or `History`; on a Colosseum, `Make_Content` or
`History`. Neither type is in advdata's switch.

This is the same shape as RC1's headline defect — "an aqueduct scores
~0 because base_want measures the delta at today's city size". W03 fixed
it for the size gate. The happiness gate was left in the identical
condition, and an Aqueduct is `ADV_IMPR_ESTIMATE` too (`Size_Adj` and
`Health_Pct` are also absent from the switch) — which is precisely why
it needed W03 in the first place.

## 3. Two caps, one priced

A city in civ2civ3 stops being able to use another worker for three
different reasons, and deep priced one of them:

| cap | mechanism | deep credit |
|---|---|---|
| hard size gate | `Size_Adj` / `Size_Unlimit`, growth stops dead | `deep_capacity_credit()` |
| food | no surplus | partial (`horizon_growth_weight`) |
| **happiness** | city grows, but the new citizen must entertain | **none** |

The happiness cap is soft and therefore easy to miss: population keeps
rising, so every population-based metric looks fine, while the marginal
citizen earns zero.

## 4. Measured signature

6 paired mixed lategame-80 games (deep and baseline seated in the same
game, so the comparison is in-world), current binary, all three
2026-08-01 growth fixes in:

```
                 baseline    deep
cities                149     146
pop                   667     727
mean city size       4.48    4.98
specialists/city     0.46    0.75      <-- +63%
```

Buildings per 100 cities:

```
CAPACITY   (credited)      3.4     8.9      deep 2.6x
HAPPINESS  (no credit)     4.0     2.1      deep 0.5x
MULTIPLIER (no credit)     2.7     0.0      deep 0x
MILITARY                 108.7   114.4
```

That is the signature of **one-sided inflation of a shared auction**:
deep massively over-builds the one column it credits and under-builds
every column it does not. And the specialist excess is the soft cap
biting — deep buys the size with its capacity credits, then parks the
citizens it bought as entertainers because it never buys the Temple that
would let them work.

## 5. The fix, and what it actually did

`deep_content_credit()` (`ai/deep/deephorizon.c`) prices the citizens a
content building frees, over the same `MORT` horizon and in the same
tile-sum currency as the capacity credit. It reuses stock's own
definition of the pacification load so it extends the WAG rather than
competing with it.

Same 6 paired games, same seeds, only `horizon_content_weight` differing:

```
                         OFF     ON
happiness bldgs /100c    2.1   39.6      (3 -> 55 buildings)
specialists per city    0.747  0.525     (-30%)
max city size              9     15
mean city size          4.98   5.20
capacity bldgs /100c     8.9   10.8
head-to-head score      1.047  1.063
```

The mechanism is confirmed and it is large. The **score is not a
result**: 1.6pp over 3 paired seeds is well inside the noise floor
(rule 6). And 39.6 per 100 cities against baseline's 4.0 says the
placeholder weight of 1.0 is very probably too hot — Colosseum 37 vs
Temple 13 suggests the credit scales with the raw `Make_Content` amount
without pricing the building's cost properly.

`horizon_content_weight` ships as a PLACEHOLDER, registered in
`tuning-params.ini` and the G/ALL/ECON blocks. **It needs a tuning
pass before any claim.**

## 6. What is NOT established

- **The multiplier column.** Deep builds 0 Courthouses/Granaries against
  baseline's 3/1. Those counts are far too small to support a claim, and
  no fix is proposed here. The *mechanism* is different from the
  happiness one and worth stating: Marketplace/Library/Courthouse are
  `ADV_IMPR_CALCULATE`, so `base_want()` does run for them — but on
  `acity->citizen_base`, today's tile arrangement. Deep is buying
  tomorrow's arrangement with its capacity credits and never tells the
  multiplier about it. To settle it, census multiplier counts on a cell
  large enough that baseline builds them in double digits.
- Whether the content credit helps at all cells. Only lategame-80 was
  measured.
