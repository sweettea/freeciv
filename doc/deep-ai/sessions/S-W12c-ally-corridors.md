# Session brief — W12 slice 3: trunk corridors to allied cities

Slices 1+2 (merged d075c4bfc4, e0dc5f193b) built MST trunk corridors
and beyond-ceiling task sourcing — but the node set in
ai/deep/deeptrunk.c is `pplayer->cities` only. The network can never
leave the shape of deep's own empire. This slice adds corridors whose
far endpoint is an ALLIED city, crossing neutral or allied ground.
Read both slices' STATUS entries and deeptrunk.c first; slice 1's
counters and slice 2's beyond-set are your instruments and must stay
live. Origin of the idea: live play showed worker turns dumped into
catchment fortresses while roads to the human ally went unbuilt
(STATUS 2026-08-15 fortress-saturation entry — related, separate fix).

## The slice

- Extend the trunk node set with allied cities behind a knob
  (`trunk_ally_gate` or similar; default 0 = today's graph exactly,
  byte-identity). Nearest-allied-city per border region or similar
  sparsification — not the full allied city list, or the MST degrades.
- Check buildability law BEFORE designing: what can a worker legally
  build on unowned tiles and on allied-owned tiles in the classic
  ruleset? The corridor must terminate at the last legally-workable
  tile (border or the allied city itself accordingly). Cite the
  ruleset/common code you checked, in the commit body.
- Locality (slice 1) stays the assignment arbiter; reuse the existing
  distance discounting. No second discount — CLAUDE.md rule 7 applies
  to any new constant: struct tuning + PLACEHOLDER + flag in STATUS.
- Counters: ally-links planned, ally-corridor tiles exposed, tasks
  sourced on them (reuse the per-player census pattern at
  deeptrunk.c:77-84). A knob-on run in an allied venue where the
  ally-link count is 0 is a miss — report, don't ship.

## The venue problem (read this twice)

Standard benchmark venues may never form alliances, so a stock A/B
would prove nothing except off-path identity. Your liveness proof
needs a venue where deep actually HAS an ally:

- First check whether deep-deep alliances form in any existing config
  (the diplomacy knobs shipped in DIPLO; read that STATUS entry), or
  whether a config can seat deep on a team.
- If no benchmark venue produces alliances, prove liveness on a
  scripted smoke (server "team" or alliance setup commands, one short
  game, counters + a road actually appearing on a corridor tile
  outside deep's borders) and say plainly in STATUS that the score
  A/B is deferred to an alliance-forming venue — the same gap already
  flagged for the FENCE variant. Do NOT bury a null from a venue that
  cannot exercise the mechanism as if it were evidence.

## Guards

- server/advisors/ only under the slice-1 hook rules (inert extension
  point + stock-arm byte-identity proof if widened again).
- Byte-identity 10/10 at defaults including one mixed cell, one
  binary pinned WITH libfreeciv.so, own worktree/build/ports — a
  holdout run may be draining on 7300; use 7900-7949.
- Europe rider: when merged, turn the knob ON in
  derby5/tuning-derby7.ini (spicy-guess policy). New keys only —
  NEVER append a key the file already defines; awk-dedupe before and
  read back the deep-tuning line in game.log.

Usual rules: tune-audit CLEAN, knobcensus entry, one commit one
thing, meson test all suites, STATUS with pasted numbers, stop after
the proof (score A/B only if an alliance venue exists).
