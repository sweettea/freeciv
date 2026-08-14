# The growth emergency has no escape hatch (2026-08-01)

Specced, not built. Human proposal from the multiplier-column session;
the code reading below is what turns it from an idea into a defect
report.

---

## 1. The proposal

A city whose growth is hard-blocked by a missing Aqueduct or Sewer
System should treat that as an emergency: switch production to the
cap-lifter, and rush-buy it if it can afford to. A size gate does not
reopen on its own, and every turn spent capped is a citizen never
earned.

## 2. The want half already exists

`deep_capacity_credit()` (`ai/deep/deephorizon.c`) prices exactly this,
and it is the ONE credit exempt from the wartime gate in
`deep_adjust_building_wants()` -- a growth ceiling is structural, so
deep pays for the cap-lifter even in a city under threat. Nothing more
is needed on the want side.

## 3. The buy half is structurally unreachable

Deep runs its own `dai_spend_gold()` derivative
(`ai/deep/deepdefence.c:2632`), and it demonstrably works: at Europe
T153 deep's treasuries were 9-128 gold against classic's 1968 / 1023 /
2586 hoards (`civ2civ3-adjustments.md:279`). Deep spends; stock does
not. **A low deep treasury is the buy loop working, not poverty.**

But the gate inherited from `daicity.c:655` reads:

```c
if (gold - expenses >= buycost
    && (!expensive
        || (city_data->grave_danger != 0 && assess_defense(ait, pcity) == 0)
        || (bestchoice.want > 200 && city_data->urgency > 1)))
```

where `expensive` is `shield_stock == 0 || gold - buycost < limit`.

**Both overrides are military.** A peaceful city has `urgency == 0` and
`grave_danger == 0`, so neither branch can fire however high the
cap-lifter's want climbs. The only route left is `!expensive`, which a
just-started Aqueduct (`shield_stock == 0`, so double cost) fails by
construction.

So the AI has a "buy it anyway, this is an emergency" concept, and it
is available exclusively to defence. Growth has no equivalent.

## 4. What to build

A third override, symmetric with the defence one: the city is at its
hard cap, the best choice is the cap-lifter, and the purchase leaves
the reserve intact. It needs a tunable for how much of the reserve a
growth emergency may eat -- **do not reuse the defence limit**, for the
same reason `cm_war_survival` was split from `survival_target`
(`STATUS.md`, 2026-08-01): being wrong about defence loses a CITY,
being wrong here loses some FOOD, and one scalar cannot serve both.

## 5. What must be MEASURED first, not assumed

The preemption half of the proposal -- "switch production immediately"
-- is not yet a defect report. The AI re-chooses each turn, so a raised
want may already switch production on its own; but switching between
the unit and improvement categories forfeits accumulated shields, so
the switch may be happening and being penalised rather than not
happening.

**Measure which before speccing it.** The census question is: how many
city-turns does deep spend at its hard cap with the cap-lifter
buildable and NOT in production? `deep-horizon` already logs the credit
at `LOG_DEBUG`; a tripwire beside it would answer this directly.

## 6. Why it is not in the multiplier commit

Different mechanism -- this is the spend path, not want pricing -- and
a different workstream seam. See `multiplier-column.md` for the credit
that session did land.
