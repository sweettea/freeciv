# W00c — ai/deep module skeleton

**Phase 0. Gate: `deep` must measure identical to `baseline`.**

## Why

Prove the module machinery and the harness both work before any behaviour
changes. If `deep` doesn't equal `baseline` here, something already differs and
you don't know what.

## Evidence

`struct ai_type::funcs` vtable: `ai/classic/classicai.c:618-698`.
`ai/tex` proves a second module can reuse `ai/default` internals.
Build wiring: `meson.build:285-303`.

**`ai->funcs.player_save` and `player_load` are commented out** at
`classicai.c:628-629`, so `struct ai_plr` — including `tech_want[]` — doesn't
persist today. Wire them from the start; retrofitting means touching every struct.

## Design

Copy `classicai.c`, forward every hook to `dai_*`. Add `struct plr_ai_state`
extending `ai_plr`, with save/load using the existing convention
(`secfile_insert_int` + `secfile_lookup_int_default`, so missing keys default
cleanly and fields can be added incrementally).

**Persistence rule:** save what cannot be recomputed (campaign commitments, war
ledgers, coalition obligations, terraform progress). Recompute the rest (road
plans, chokepoints, site rankings, knowledge derived from `player_tile`).

## Acceptance

- 50 paired seeds: `deep` vs `baseline` score distributions within ±2%
- All state round-trips through save/load
- `AIT_*` timings dump on demand
- Decision vector logged once per player per turn

## Commits

```
build: add 'deep' to the aimodules option
ai/deep: add module skeleton with capability string and self pointer
ai/deep: forward player, city and unit lifecycle hooks to default AI
ai/deep: forward build advisor and settler hooks to default AI
ai/deep: forward diplomacy and turn-phase hooks to default AI
ai/deep: add per-player state struct and allocation hooks
ai/deep: wire player_save and player_load hooks
ai/deep: persist per-player state with defaulted missing keys
ai/deep: log a per-turn decision vector line per player
test: assert AI state round-trips through save and reload
doc: document the deep AI module and how to select it
```
