# Part 9 — The call to arms

Companion to Parts 1–8. Same tree: `freeciv/freeciv` @ `0896b34`.

Both of your frustrations are real. The demand doesn't come from where I
described in Part 7, and the missing retraction is about ten lines.

---

## 1. Where the demand actually comes from

Not the `ally_patience` ladder — that's `dai_diplomacy_actions()`. The message
you're seeing is from **`war_countdown()`** (`daidiplomacy.c:~1479–1585`), which
fires once when the AI resolves to go to war and broadcasts to **every ally**.
All five war reasons carry an expectation:

| `war_reason` | What allies are told |
|---|---|
| `DAI_WR_SPACE` | *"Your aid in this matter will be expected. Long live our glorious alliance!"* |
| `DAI_WR_BEHAVIOR` / `DAI_WR_EXCUSE` | *"...expect you to honor your alliance with us and do likewise!"* |
| `DAI_WR_HATRED` | *"As our glorious allies, we expect your help in this war."* |
| `DAI_WR_NONE` | *"If you want a piece of the loot, feel free to join in the action!"* |
| `DAI_WR_ALLIANCE` | *"...declare war on %s in %d turns. We expect you to do likewise."* |

**Not one of them looks at the ally's relationship with the target.** The loop
iterates allies and emits the message. It never consults:

- `player_diplstate_get(ally, target)->type` — the ally may be at **peace or in
  full alliance** with the target and is told to attack them anyway
- `ally->ai_common.love[player_index(target)]`
- `adip->is_allied_with_enemy`, which is computed every phase
  (`daidata.c:189–192`) precisely to detect this situation

That last one is the sharpest bit. The AI **already knows** your ally is friends
with its new enemy. `is_allied_with_enemy` is used in exactly two ways
(`daidiplomacy.c:1059, 1106`): to block love gain, and to **double the love
penalty**. So the AI's entire response to "my ally is close to my enemy" is to
resent them harder and demand war anyway. It never softens the ask, offers
compensation, or reconsiders.

### A likely dead branch worth checking

Separately: by my reading, the three-stage `ally_patience` nag ladder at
`daidiplomacy.c:1858` **cannot execute**. The flow in one iteration is:

```
1813:  adip->spam = MAX(adip->spam - 1, 0);          /* spam >= 0 */
1814:  if (adip->spam > 0 && is_ai(aplayer)) continue;
1822:  if (adip->spam <= 0) { adip->spam = fc_rand(...) + 3; }   /* now >= 3 */
...
1858:  if (adip->spam <= 0) { ...the entire nag ladder... }
```

Line 1822 guarantees `spam > 0` before line 1858 is reached on every path, so
the condition at 1858 is never true — which would also mean the
alliance-breaking punishment below it never fires either. I'd flag this as
**"verify with a log line before acting on it"** rather than treat it as
settled; static reading can miss things. But it's consistent with your
experience: what you're seeing is the `war_countdown` broadcast, not the ladder.

(Also note the comment/code mismatch at 1823–1826: the `!pplayers_allied` branch
carries the comment *"Bugger allies often"* while the `else` branch — the actual
allied case — says *"Others are less important."* The comments are swapped
relative to the condition.)

---

## 2. The missing retraction

Confirmed: there are **42** `dai_diplo_notify()` calls in `daidiplomacy.c`, and
none of them retracts a call to arms. The AI announces alliance formation, peace
agreement, ceasefire agreement, space-race warnings, refusals of every kind —
and never says "we have come to terms, stand down."

The state does reset, silently. In `dai_diplomacy_actions()`:

```c
if (!target || !target->is_alive) {
  adip->ally_patience = 0;
  break; /* No need to nag our ally */
}
```

Once the AI makes peace, `WAR(pplayer, eplayer)` goes false, no target is found,
patience resets to 0, and nothing is said. From the human's side, an obligation
was declared publicly and then quietly abandoned — so you're left honoring a
commitment to a war that ended, against a nation you may now have to fight
alone.

**Fix, roughly ten lines.** Record who was called and for which target:

```c
/* in ai_dip_intel */
struct player *called_to_arms_against;   /* nullptr = no outstanding call */
```

Set it in `war_countdown()` alongside each notify. Then each phase, if
`called_to_arms_against != nullptr` and you are no longer at war with that
player, emit a retraction and clear it:

> *"We have come to terms with the %s. You are released from any obligation
> to fight them on our behalf."*

Do the same on the ally's side of the ledger under Part 7 — a call that was
retracted must not count as a missed obligation.

---

## 3. Your ceasefire idea

Worth separating two mechanics, because one of them isn't available.

**Not possible:** brokering peace between two *other* players. Treaties are
strictly bilateral — `handle_diplomacy_init_meeting_req(pplayer, aplayer)`.
An AI cannot mediate between its ally and its enemy.

**Fully possible and never done:** the AI seeking its *own* ceasefire because
the war is straining its alliances. `CLAUSE_CEASEFIRE` is implemented and the AI
proposes it routinely — just never for this reason.

### Where the cost belongs

`dai_war_desire()` already has a partial version of this. At
`daidiplomacy.c:888–908`, war want is reduced by the treaties you'd strain:

```c
if (!cancel_excuse && pplayers_allied(target, eplayer)) {
  if (ds == DS_ARMISTICE)      want -= abs(want) / 10;  /* 10% */
  else if (ds == DS_CEASEFIRE) want -= abs(want) / 7;   /* 15% */
  else if (ds == DS_PEACE)     want -= abs(want) / 5;   /* 20% */
  else if (ds == DS_ALLIANCE)  want -= abs(want) / 3;   /* 33% */
}
```

So it *does* discount war against someone your allies are allied with. But the
trigger is `pplayers_allied(target, eplayer)` — a **formal alliance** between
your ally and the target. Mere peace, or high mutual love, or a long shared
history counts for nothing.

Two changes:

**(a) Widen the cohesion cost.** Score alliance strain on a gradient rather than
requiring formal alliance:

```c
strain = f(diplstate(ally, target),        /* alliance > peace > armistice */
           ally_love_toward_target,        /* AI allies */
           diplstate_max_state(ally,target)/* humans: "max treaty level ever" —
                                            * player.h already stores it */
           );
want -= abs(want) * COHESION_WEIGHT * strain;
```

`player_diplstate` already carries `max_state` (*"Maximum treaty level ever
had"*), which is a decent proxy for a human ally's historical disposition when
you can't read their love value.

**(b) Add an exit, not just an entry check.** Once at war, if alliance strain is
high and the war isn't going well, seek a ceasefire *with the target*. That's
the concrete version of your suggestion, and it composes with the `DIP_SUE`
work in Part 3 §4 — the war ledger you need there supplies "isn't going well,"
and strain supplies the diplomatic half. A ceasefire that preserves two
alliances is often worth more than the war.

---

## 4. Tiering the demand

The underlying problem is that the call to arms is one message with one tone
regardless of what it costs the recipient. Grade it by the ally's position:

| Ally's relation to target | Appropriate ask |
|---|---|
| War / hostile already | Coordinate: *"let us strike together at X"* — no demand needed |
| Neutral, no contact | Full request, with a deadline that accounts for their distance |
| Peace treaty | Request **with compensation offered** — they're breaking a treaty |
| Alliance / team | **Do not ask.** Inform only, and absorb the strain — or reconsider the war (§3) |

The bottom row is what's generating your frustration, and it's a two-line guard
in `war_countdown()`.

The "with compensation" row is where this meets Part 8: *"declare war on X
within 5 turns, and I pay you 300 gold"* is exactly a conditional promise clause
with escrow, and it's arguably the single most useful one to build first —
better than the plain deferred-gold clause I suggested there, because it's a
thing the AI wants to say and currently can only shout.

---

## 5. Suggested order

Cheapest to most valuable:

1. **The retraction message** (§2). Ten lines, pure win, no design risk.
2. **The alliance guard in `war_countdown()`** (§4). Two lines: don't demand war
   from an ally who is allied with the target; downgrade to informing.
3. **Verify the dead nag ladder** (§1). One log line will settle it. If it is
   dead, that's worth reporting upstream regardless of your AI work — and it
   means the "break the alliance over non-participation" behaviour players
   sometimes attribute to the AI isn't actually happening.
4. **Widen the cohesion cost in `dai_war_desire`** (§3a). Moderate, and it makes
   the AI pick better wars rather than merely complain about them better.
5. **Ceasefire-on-strain exit** (§3b). Depends on the Part 3 war ledger.
