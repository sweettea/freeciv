# W00a — Vendor the frozen baseline

**Phase 0. First session. Purely mechanical — no judgment required.**

## Why

`ai/baseline` is the measurement control for the entire project. Upstream's
`ai/default` will change over time (and we may rebase onto it); if our control
moves, every benchmark number ever taken becomes incomparable.

It also means you can play against **old-style and new-style AIs in the same
game**, which works today and survives save/load.

## Evidence this works already

- `player->ai` is a per-player `const struct ai_type *` — `common/player.h:289`
- `create <player-name> [ai type]` — `server/commands.c:320`. Help text:
  *"The 'ai type' parameter can be used to select which AI module will be used
  for the created player."*
- Persisted per player: `player%d.ai_type` — `savegame3.c:4740` (save), `:3771` (load)
- Module list saved as `game.ai_types` — `savegame3.c:2373-2382`
- There's even `game.scenario.allow_ai_type_fallback`

## Design

1. Copy `ai/default/` → `ai/baseline/default/` and `ai/classic/` →
   `ai/baseline/classic/` at the current checkout.
2. Rename symbols to avoid collision. The registration entry point in
   `ai/classic/classicai.c` sets the module name — that's the key change.
3. Register the aitype as `baseline`.
4. Add to `meson.build` alongside the existing `aimodules` entries
   (see `meson.build:285-303`).
5. Add a `.gitattributes` or CI check that fails if anything under
   `ai/baseline/` changes.

## Acceptance

- `ninja -C build` succeeds
- Server accepts `create Foo baseline`
- A game with one `baseline` and one `classic` AI plays to turn 50
- Save and reload preserves both aitypes
- Attempting to commit a change under `ai/baseline/` fails CI

## Commits

```
build: add ai/baseline module directory to the build
ai/baseline: vendor the stock AI at commit 0896b34
ai/baseline: register the baseline AI type
test: fail the build if ai/baseline is modified
doc: record the vendored baseline commit and its purpose
```
