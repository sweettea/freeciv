# Session brief — chatty deep: outbound ally communication

User-driven flavour epic (the standing no-AI-ally-conversation
exclusion is LIFTED for outbound coordination, by user direction
2026-08-22: "i'd love to coordinate a little better with allies").
Deep talks TO its allies — especially human allies — about its own
plans and knowledge. This slice is OUTBOUND ONLY: no parsing of
incoming chat, no negotiation loops. The bar is Europe-flavour:
mechanism truth, liveness, and taste; no score A/B.

## First: find the real channel

Read how the stock AI already sends player-visible text (ai/default
diplomacy has canned messages; server has notify_player/chat
machinery). REUSE the same server-side mechanism the stock AI uses —
do not invent a transport. Messages must arrive in a connected
client's chat window addressed from the deep player. Cite the call
path in the commit body.

## The message set (each its own tiny formatter, one commit)

All content is deep's OWN state — plans and its own threat reads —
which is honest by construction. Behind one gate (chatty_gate) plus
a per-turn rate cap knob (chatty_max_per_turn) so it never spams:

1. **Road/corridor announcements**: when an ally-corridor plan forms
   or completes ("Building a road toward your lands via <direction/
   nearest-city-name>"). Coordinates with the W12H alignment work —
   if that merge landed, hook its plan events; if not, hook trunk
   link formation for ally links.
2. **Threat warnings**: when deep's threat model sees a danger to an
   ALLY's city (enemy stack within its incoming-defence ETA window),
   warn the ally once per (city, threat) — "Enemy force approaching
   <city>". Deep already computes this for itself; saying it aloud
   leaks only deep's own sight.
3. **Operation notices**: when deep launches an assault operation
   while allied, tell allies the target ("Marching on <city>") at
   launch (not at muster - no lying, but no leaking a plan so early
   it changes deep's own behaviour either; launch-time is when the
   commitment is real).
4. **Pact/stance notices**: on signing peace/alliance and on deciding
   war (at declaration), one line of intent.

Tone: short, plain, in-character-neutral (no lore cosplay); the
formatter takes the message through fc_ translation macros like the
stock AI's messages do.

## Proof

- Byte-identity at defaults 10/10 (gate off) incl. one mixed cell.
- Liveness smoke: a scripted-alliance game (the W12c smoke pattern,
  two deep on a team) showing each message class fired in game.log/
  chat with the rate cap respected; paste examples. One cell with a
  derby-7-style tuning to confirm Europe will emit them.
- Determinism: messages must not perturb play — they are output
  only; the identity proof covers gate-off, and a gate-on pair of
  identical-seed runs must produce identical scorelogs too (text
  output must not touch game state or RNG).
- Europe rider: chatty_gate on, a sane rate cap, in
  derby5/tuning-derby7.ini per dedupe rules.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare). Ports 7600-7649, jobs <=2 — CPU-light
session; three heavier epics are live (7300s employment probe,
7400s expand staggered, 7500s soft-borders). If a message needs
state that only exists in stock code, reuse via the visibility
exception (non-static + header) with the behaviour-neutrality
argument stated; NEVER modify stock behaviour. New knobs: struct
tuning + params/blocks + knobcensus; tune-audit CLEAN at the new
count. One commit one thing; meson test all suites; STATUS with
pasted message examples; stop after the liveness smoke.
