# W09-lite — City manager parameters

**Phase 1. No dependency on W01 for the simple version.**
Root cause: RC5. **Highest leverage-to-effort item in the project.**

## Problem

The AI computes elaborate strategic priorities and then ignores them when
assigning citizens to tiles.

## Evidence

`set_default_city_manager()` — `server/cityturn.c:314` — sets the CM factors for
**every AI city**, with this comment:

> *We used to look at pplayer->ai.xxx_priority to determine the values to be used
> here. However that doesn't work at all because those values are on a different
> scale. Later the AI may wish to adjust its priorities — this should be done via
> a separate set of variables.*

The disconnection was **deliberate**, because of RC4, and the TODO was never done.
So every AI city — at war or peace, growing or building, magnet or backwater —
uses hardcoded factors:

```
food 10-20, shield 5, trade 0, gold 2, luxury 0, science 2, happy_factor 0
```

Note `happy_factor = 0`: the migration score (W17) weights happy at 1.0 and angry
at −0.5, so the AI's citizen arrangement actively **fights its own migration
score**. And `factor[O_TRADE] = 0` means trade specials register only through a
lagging tax split.

**And the mechanism exists.** `auto_arrange_workers()` (`cityturn.c:403`) honours
`pcity->cm_parameter` when set. `grep -rn "cm_parameter" ai/` finds only a local
variable in the tax code. **The AI never sets it.**

## Design — the "lite" version

Full W09 derives parameters from doctrine (W08). The lite version derives them
from **city state alone**, which needs nothing new:

| Situation | Emphasis |
|---|---|
| Small city, below growth cap | Food-heavy (near current default) |
| At or near size cap | Trade and shields; relax the shield floor |
| `urgency > 0` or `grave_danger` | Shields; accept brief food deficit |
| Building a wonder | Shields, with trade to fund buying |
| High `worth`, no growth blocker | Balanced with trade weighted up |

Set `pcity->cm_parameter` each phase in the `city_alloc`/phase hook. Leave the
default in place for cities that don't match a rule, so the change is incremental
and reversible per-city.

Do **not** invent the factor numbers — put them in `struct tuning` (W25) and tune
them. This is exactly the case rule 7 exists for.

## Acceptance

- Cities under `urgency > 0` show **≥15% higher shield output** than baseline
- No increase in starvation events (guard against over-weighting shields)
- Score improvement measurable on paired seeds — this is a strong candidate for
  the project's first headline number

## Commits

```
ai/deep: derive city manager parameters from city state
ai/deep: set city manager parameters on owned cities each phase
ai/deep: emphasise shields in cities under urgency
ai/deep: emphasise trade in cities at their growth cap
ai/deep: add city manager factors to the tuning struct
test: add metric — shield output of cities under urgency
test: assert starvation events do not increase
```
