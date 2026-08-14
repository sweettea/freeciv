# PRD Addendum — W24: Natural-language ally requests

**Status:** Speculative, optional, and last in the roadmap
**Depends on:** W8 (doctrine), W13 (campaigns), W16 (legibility), W19 (coalitions)

---

## 1. Reframe: the model is a parser, not a decision-maker

The instinct "add a small LLM so the AI can respond to chat" hides a fork in the
road, and which side you take determines whether this is a weekend feature or an
unmaintainable mess.

**Wrong version:** feed chat to a model, let the model decide what the AI does.
Now your AI is non-deterministic, untestable, unexplainable, exploitable by
prompt injection, and every one of the twenty-three workstreams above is
downstream of a black box.

**Right version:** the model does exactly one job — **turn free text into a
structured intent** — and hands it to the goal system, which decides everything.

```
"hey can you hit Berlin from the south? I'll cover the coast"
        │
        ▼  [parse]
{ intent: REQUEST_ATTACK_CITY,
  city: Berlin,
  hint_direction: south,
  offer: { intent: OFFER_DEFEND_REGION, region: coast } }
        │
        ▼  [evaluate — ordinary want machinery]
campaign target score for Berlin  += ally_request_weight
                                   × coalition_standing(ally)
                                   × trust_posterior(ally)
```

The AI's behaviour stays deterministic given an intent. Only the *parse* is
fuzzy, and in the test harness you inject intents directly and never run the
model at all.

**The real deliverable is the intent schema.** The model is one swappable
implementation of text→intent; keyword matching is another. Ship the schema
first and it's useful before any model exists.

---

## 2. The intent schema

Freeciv's request space is small and highly structured — this is an intent+slot
classification problem, not a generation problem. A first cut:

| Intent | Slots | Maps to |
|---|---|---|
| `REQUEST_ATTACK_PLAYER` | player | W13 target scoring, W8 `DIP_HOSTILE` |
| `REQUEST_ATTACK_CITY` | city, [direction] | W13 campaign target bias |
| `REQUEST_DEFEND` | city \| region | W15.3 allied danger assessment |
| `REQUEST_PEACE_WITH` | player | W16 cohesion cost, W14 ceasefire |
| `REQUEST_STOP_SETTLING` | region | W7 contest pressure, negative claim term |
| `REQUEST_GIVE` | tech \| gold \| city, amount | `dai_goldequiv_clause` |
| `REQUEST_SHARE_VISION` / `_MAPS` | — | existing clauses |
| `ANNOUNCE_ATTACK` | player, [turn] | W19 coalition target, W16 stance |
| `OFFER_TRIBUTE` | amount, schedule | W19 ledger credit |
| `OFFER_DEFEND` | city \| region | W19 `leader_debt` credit |
| `COMPLAIN_BORDER` | region | W7 pressure, W16 stance softening |
| `THANK` / `APOLOGISE` | — | love adjustment, rate-limited |
| `UNKNOWN` | raw text | logged, politely declined |

Roughly fifteen intents and six slot types. Slots resolve against **existing game
objects** — player, city, tile, terrain region, resource, integer — so the parser
can be validated: an intent naming a city the speaker has never met is rejected
before it reaches the AI.

**Closed enum, always.** The parser can only ever emit intents from this list. It
cannot emit actions, cannot emit code, cannot emit free-form goals. That single
constraint eliminates most of the prompt-injection surface by construction.

---

## 3. Requests bias, they never command

The critical design rule, and the one that keeps this from being an exploit:

> An ally's request adds **want**. It never sets policy, overrides doctrine, or
> bypasses an abort condition.

So "attack Berlin" raises Berlin's campaign target score. If Berlin is across an
ocean, undefendable, and the AI is in `ECON_CONSOLIDATE` with `MIL_TURTLE`, the
request loses and the AI says so. That's correct behaviour and it's also *good
drama* — an ally who says *"we cannot; our eastern border is bare"* is far more
interesting than one that either obeys or ignores you.

**How much bias?** Reuse machinery already designed rather than inventing a
number:

```c
request_weight(ally, intent) =
    base_weight(intent)                       /* schema constant */
  × coalition_standing(ally)                  /* W19 — have they contributed? */
  × trust_posterior(ally)                     /* W22 — kept vs broken promises */
  × MIN(1.0, leader_debt_owed(ally) / cost)   /* W19 — have we underdelivered? */
  × doctrine_affinity(intent, doctrine)       /* W8 — does it fit our plan? */
```

That composition gives you all the behaviours you'd want for free:

- **A free-riding ally gets little.** `coalition_standing` is low, so their
  requests barely move anything. They can *ask*; asking isn't currency.
- **A tribute-paying ally gets a lot.** They've bought influence, which is
  precisely what W19's ledger is for. This is the payoff that makes tribute
  rational for the junior.
- **An ally the AI has underserved gets priority.** `leader_debt` rising means
  the AI owes them; their next request should land hard.
- **A serial promise-breaker gets ignored.** The Beta posterior handles it with
  no special case.
- **A request against doctrine still loses**, and the AI can explain why —
  because doctrine is already logged (W8.2).

**Also: rate-limit using the counter that already exists.** `adip->spam`
(`daidata.h:59`) is described in-source as *"timer to avoid spamming a player
with chat."* Run inbound requests through the same mechanism. One meaningful
request per few turns per ally; extras get a templated "one thing at a time."

---

## 4. Architecture: external process, never a linked dependency

Freeciv is C with a conservative dependency policy. **Do not link a model
runtime into the server.** That proposal dies in review, correctly.

```
freeciv-server                        freeciv-intent (optional, separate)
  handle_chat_msg_req()  ─── text ──▶   parse → validated intent JSON
  (server/handchat.c:320)                        │
  intent queue per player  ◀── intent ───────────┘
        │
  ai/deep phase_begin: drain queue, evaluate, respond
```

Properties this buys:

- **No new server dependency.** If `freeciv-intent` isn't running, chat is
  ignored exactly as it is today. Feature degrades to current behaviour.
- **Swappable backend.** v1 can be regex/keyword matching in ~300 lines of
  Python and still be genuinely useful. v2 swaps in a 100–500M parameter model
  doing intent+slot extraction. Same wire format.
- **Testable without a model.** The harness injects intents directly. All 50-game
  batch runs stay deterministic (PRD Part V).
- **Sandboxable.** The parser sees text and a whitelist of entity names. It never
  sees game state it shouldn't, never touches the server's memory, and can be
  killed without consequence.
- **Multiplayer-safe.** A slow or malicious parser delays only its own queue;
  server turn processing never blocks on it.

`handle_chat_msg_req()` (`server/handchat.c:320`) is a single, clean hook — every
player chat message passes through it.

---

## 5. Parse only. Do not generate.

There's a decisive technical argument against LLM-generated AI dialogue that has
nothing to do with quality: **Freeciv is heavily internationalised.**
`daidiplomacy.c` alone has 41 `gettext`-wrapped strings, and the AI's entire
voice ships through `_()` with plural forms via `PL_()`.

LLM output cannot go through gettext. Adding generated dialogue means either
English-only AI speech in a project translated into dozens of languages, or a
translation pipeline nobody is going to build.

Keep W16's templated narration. It's deterministic, translatable,
regression-testable, and cannot say anything embarrassing to a stranger on a
public server. The AI listening in natural language and replying in polished
templated prose is not a compromise — it's the better product.

Where you *do* want variety, expand the template set. `dai_diplo_notify` already
supports parameterised messages; give each intent-response a handful of
variants selected by personality trait.

---

## 6. Async is a feature

Parsing happens between turns; the AI acts on the intent in its next
`phase_begin`. This is forced by the architecture — and it's better than
instant response.

An ally who takes a turn to consider your request, then commits, reads as
*deliberate*. An ally who reacts within milliseconds reads as a chatbot. The lag
also gives you a natural place for W16 narration: *"We have heard you. Our
counsel meets."* then, next turn, *"We will move on Berlin. Hold the coast."*

---

## 7. Abuse surface, honestly

| Risk | Mitigation |
|---|---|
| Prompt injection | Closed intent enum; parser cannot emit anything else. Slots validated against known game objects. |
| Chat spam / DoS | Reuse `adip->spam` rate limiting; queue depth cap per player; parser out-of-process. |
| Talking the AI into suicide | Requests bias want; abort conditions (W13.4) and doctrine constraints (W8) are never bypassed by a request. |
| Free-rider extracting help | `coalition_standing` gates weight — README's own warning that *"diplomacy usually gains only human players"* applies directly here. |
| Non-determinism in testing | Harness injects intents; the model never runs in batch runs. |
| Offensive content | Parse-only; the AI's output is templated and translated. |

The one risk I'd genuinely watch: **an ally who learns the intent vocabulary can
steer the AI more precisely than one who doesn't**, which advantages players who
read the source. Mitigate by making the schema *discoverable in game* — have the
AI, on `UNKNOWN`, reply with a nudge: *"We do not understand. Do you ask us to
attack, to defend, or to give?"* That turns a hidden vocabulary into a taught one.

---

## 8. Why it's worth doing

Highest fun-per-line in this entire document, by a wide margin.

Everything else in the PRD makes the AI *play better*, which a player perceives
slowly and indirectly. This makes the AI *feel like a participant*. "Help me,
I'm being invaded from the north" landing and being answered is a categorically
different experience from clicking checkboxes in a treaty dialog, and it costs a
fraction of what the campaign system costs.

It also completes a loop the PRD already half-built: W16 makes the AI narrate its
intent, and W19 gives it obligations to allies. W24 lets allies talk back. The
three together are the difference between an opponent and a *character*.

---

## 9. Delivery

Following Part VII conventions. Three epics; the first is useful alone.

**E24.1 — Intent schema and injection path.** *Useful with zero NLP.*
```
common: define the ally request intent schema and slot types
server: expose player chat to AI modules via an intent queue
ai/deep: drain and log the intent queue each phase
ai/deep: validate intent slots against known game objects
ai/deep: rate-limit inbound requests using the existing spam counter
test: add intent injection to the batch harness
```

**E24.2 — Evaluation and response.** *This is where the behaviour lands.*
```
ai/deep: weight ally requests by coalition standing and trust
ai/deep: bias campaign target scoring from attack requests
ai/deep: bias allied danger assessment from defend requests
ai/deep: bias contest pressure from border complaints
ai/deep: price give requests through the existing clause valuation
ai/deep: credit offers of tribute and defence to the coalition ledger
ai/deep: decline requests that conflict with current doctrine, with a reason
ai/deep: narrate acknowledgement and commitment across two turns
test: add corpus position — ally requests aid during an invasion
```

**E24.3 — The parser.** *Optional, external, swappable.*
```
tools: add freeciv-intent, a standalone chat-to-intent service
tools: implement keyword and pattern matching for the intent schema
server: connect to an optional intent service when configured
tools: add an optional small language model backend to freeciv-intent
doc: document the intent schema and how to run an intent service
```

**Acceptance.** With the service absent, behaviour is byte-identical to W23.
With keyword matching, ≥70% of a hand-labelled corpus of 200 real chat requests
parses to the correct intent. With a model backend, ≥90%. In both cases, AI
behaviour given an intent is deterministic and matches the injected-intent tests.

---

## 10. Honest assessment

**Build the schema early, the parser last.** E24.1 is worth doing during W19
regardless — a structured request queue is useful even if the only thing filling
it is a client-side dropdown menu. Which, incidentally, is a perfectly good
shipping product on its own and needs no NLP at all: a "Request…" button in the
diplomacy dialog emitting the same intents.

That's the test of whether the design is right. **If the feature is valuable with
a dropdown instead of a model, the architecture is correct and the model is a
genuine enhancement. If it's only valuable with the model, the model is doing
work the goal system should be doing.**

Upstream, I'd expect the schema and the dropdown to be welcomed, the optional
external service to be tolerated, and any proposal to link a model runtime into
`freeciv-server` to be refused — rightly. Pitch it in that order.
