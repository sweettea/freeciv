# Session brief — chatty slice 2: deep listens to allies

Slice 1 (merged bcc4db4b02) gave deep an outbound voice. This slice
gives it ears: a SMALL, EXACT command set parsed from real in-game
chat, allies only, gated. The bar is Europe-flavour: mechanism
truth, liveness, taste. Read deepchat.c and the slice-1 STATUS
entry first.

## The protocol (exact-match, no NLP)

Commands deep accepts, from ALLIED players only (server-
authenticated sender; a non-ally's command gets a polite refusal
line, rate-capped):

1. `defend <city>` — city may be the ally's or deep's own. Deep
   raises the defence priority of that city for N turns via an
   EXISTING seam (read how the threat/garrison machinery takes
   inputs; do not build a parallel priority system). Reply with
   what it will do ("We will reinforce Neapolis") or an honest
   refusal ("We cannot reach Neapolis").
2. `attack <city|player>` — deep treats the target with an
   increased attack valuation for N turns (again through an
   existing seam - e.g. the ops target pricing), replying with
   launch-time honesty per slice 1's class 3.
3. `road to <city>` — inject/boost an ally-corridor link toward
   that city in the trunk planner (the machinery exists since
   W12c; this is a demand injection behind the gate).
4. `status` — one line: stance, current op target if any, top
   threat it sees to the asking ally.

Rules: commands are REQUESTS — deep prices them, it does not obey
blindly; every acceptance/refusal is answered in chat (slice 1's
channel); all effects decay after N turns (struct tuning knob,
PLACEHOLDER); per-ally rate cap reuses slice 1's cap machinery.
Honest-info: inbound text is observable game chat; the effects
only adjust deep's own valuations - no hidden state between
players.

## Find the real inbound channel first

How does a server-side AI see player chat? Read the chat/packet
path (server/handchat.c or equivalent) and cite it. If AIs cannot
receive ordinary chat without a server patch, the fallback is the
server command channel or a prefix convention — pick whatever
needs NO stock-behaviour change, and say what you chose and why.
server/ commits are allowed for a log-only or inert-until-gated
observation hook (the dyad logger precedent), NOT for behaviour.

## Proof

- Identity 10/10 gate-off incl. mixed cell; gate-on determinism
  with no inbound traffic (identical scorelogs — parsing only
  activates on messages).
- Liveness: scripted smoke where a scripted ally (or server
  script) issues each command; paste the request->reply->effect
  triple for all four (the effect shown via existing counters,
  e.g. defence want moving, corridor link appearing).
- Europe rider: gate on in derby5/tuning-derby7.ini per dedupe
  rules.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare). Ports 7600-7649, jobs <=2 (sibling
A/Bs draining). Batch drivers at benchmark/*.sh launched detached.
Power-loss sweep on inherited DONE dirs. Disk ~6G: prune. tune-audit
CLEAN at the new count; knobcensus; one commit one thing; meson
test all suites; STATUS with the pasted triples; stop after the
liveness smoke.
