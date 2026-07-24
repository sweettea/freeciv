# W05 — Settler routing

**Phase 1. No dependency on W01. MUST precede W07.**
Root causes: RC5 (planning/execution disagree).

## Problem

Site *selection* prices travel as if the map were empty and at peace. That number
feeds the distance discount that dominates the decision.

## Evidence

**The land search parameter is bare** — `daisettler.c:954`:
```c
pft_fill_unit_parameter(&parameter, nmap, punit);
parameter.omniscience = !has_handicap(pplayer, H_MAP);
```
No `get_TB`, no `get_EC`, no `adv_avoid_risks`, no `is_pos_dangerous`.

**The asymmetry that gives it away:**
- Worker search sets `get_TB = autoworker_tile_behavior` (`TB_IGNORE` for
  non-allied territory) — `autoworkers.c:468`
- Overseas settler search sets `get_TB = no_fights_or_unknown` — `daisettler.c:966`
- **Only the land settler search is bare.** Same subsystem, same vtable hook.

**Execution inverts the protection** — `daitools.c:456`, `LONG_TIME 4`:

| Settler journey | Zone of control | Hard fight avoidance |
|---|---|---|
| Short (≤4 tiles) | respected | `no_fights` |
| **Long (>4 tiles)** | **disabled** | **none** |

The long-path cityfounder branch's entire body is a comment: *"ignore the presence
of enemy units when computing the path."* Workers get `no_fights`
unconditionally. Five lines above sits
`/* TODO: ... probably ought to be more cautious for non military units */`.

**`is_pos_dangerous`** — a documented `pf_parameter` callback reading *"The unit
will never end a turn at a dangerous position"* — is **assigned by nothing
anywhere in the AI.**

**Bodyguards are destination-only.** `dai_gothere_bodyguard()` (`daitools.c:160`)
counts hostile units **on the destination tile**. A settler crossing a corridor of
enemy cavalry to reach an empty meadow requests no escort.

**The per-step check isn't avoidance.** `advgoto.c:280` only refuses to *end a
turn* on a dangerous tile when currently safe. Next turn it resumes the same
danger-blind path. A stutter, not a reroute.

## Design

Apply `adv_avoid_risks()` and a `settler_tile_behavior` to the **search**
parameter, with fearfulness **above** `NORMAL_STACKING_FEARFULNESS` — a settler's
loss also wastes every turn already spent walking.

```c
static enum tile_behavior settler_tile_behavior(const struct tile *ptile,
                                                enum known_type known,
                                                const struct pf_parameter *param)
{
  const struct player *owner = tile_owner(ptile);
  if (owner != nullptr && !pplayers_allied(owner, param->owner)) return TB_IGNORE;
  if (is_non_allied_unit_tile(ptile, param->owner, FALSE))       return TB_IGNORE;
  return TB_NORMAL;
}
```

Consider `TB_DONT_LEAVE` rather than `TB_IGNORE` for *neutral* territory bordering
a hostile — expensive, not unreachable. That's what the extra-cost function is for.

Set `is_pos_dangerous` for civilians by wrapping `adv_danger_at()`. Note it takes
a `struct unit *`, so stash it in `parameter->data` alongside the risk cost or
widen `adv_risk_cost`.

Fix the long/short inversion: give long-path cityfounders at least
`no_intermediate_fights`, and stop nulling `get_zoc` for civilians — the
ZoC-ignoring rationale is about *attackers* pushing toward a target that may
clear, which doesn't apply to a settler that must not die.

Sample path danger for bodyguard requests — the path object is available.

**Effect is automatic:** dangerous routes get longer, `turns` rises, and
`amortize(total, PERFECTION * turns)` discounts the site. Sites behind a war zone
de-rank with no special-casing.

## Acceptance

- Settlers lost to hostiles per 100 turns **down ≥60%**
- No increase in mean turns-to-found on peaceful maps (guard against over-timidity)
- L1 corpus position: settler declines a route through a hostile corridor

## Commits

```
ai/deep: add a settler tile-behaviour callback mirroring the worker one
ai/deep: apply risk avoidance to the settler site search parameter
ai/deep: raise settler fearfulness above the default stacking value
ai/deep: implement is_pos_dangerous for civilian pathfinding
ai/deep: give long-path city founders intermediate fight avoidance
ai/deep: stop disabling zone of control for civilian long paths
ai/deep: sample path danger when requesting a settler bodyguard
test: add metric — settlers lost to hostiles per 100 turns
test: add corpus position — hostile corridor between start and best site
```
