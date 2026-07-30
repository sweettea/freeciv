# W33 capstone — The decapitation strike

**Worked example spanning W33 (operations), W32 (air), W31 (naval), W35
(espionage), W34 (victory), W04 (knowledge), W14 (war ledger).**

Not a new workstream — the demonstration that the whole military-and-intelligence
stack composes into one deliberate, high-value operation. If the AI can plan and
execute this, every layer is working together.

Baseline: `freeciv/freeciv` @ `0896b34`.

---

## 1. The objective

Take the **enemy capital**, specifically, for two effects the rest of the map
can't produce. Confirmed in `server/citytools.c:2022-2035`:

```c
/* If a capital is captured, then spark off a civil war */
if (is_capital(pcity)
    && (cplayer->spaceship.state == SSHIP_STARTED
        || cplayer->spaceship.state == SSHIP_LAUNCHED)) {
  spaceship_lost(cplayer);            /* (1) the spaceship is DESTROYED */
}

if (is_capital(pcity)
    && civil_war_possible(cplayer, TRUE, TRUE)
    && normal_player_count() < MAX_NUM_PLAYERS
    && civil_war_triggered(cplayer)) {
  try_civil_war = TRUE;               /* (2) the enemy may FRACTURE */
}
```

Two strategic prizes from one tile:

**(1) Spaceship destruction is guaranteed.** If the target is racing to a space
victory (W34 detects this — the existing `daidiplomacy.c:1123` already logs
*"going for space race victory"*), capturing their capital *destroys the
spaceship outright* — `spaceship_lost()`, no probability. This is the hard counter
to a space win, and today the AI's only response to an enemy spaceship is to
**threaten war** (`daidiplomacy.c:1495`). It never plans the one operation that
actually stops it.

**(2) Civil war is a computable probability.** `civil_war_triggered()`
(`server/plrhand.c`) is:

```
prob = EFT_CIVIL_WAR_CHANCE (government base: Despotism/Anarchy high, Democracy 0)
     − sum over their cities of EFT_CIVIL_WAR_CITY_BONUS   (Courthouse, Police, etc.)
fires if fc_rand(100) < prob
```

And `civil_war_possible()` requires the victim to hold at least
`game.server.civilwarsize` cities and more than `GAME_MIN_CIVILWARSIZE` after
losing the capital. **So the AI can compute, before committing, the exact
probability that capturing this capital splits the enemy in half** — a rival that
loses a third of its cities to a spontaneous new nation is often knocked out of
the game.

Both effects are *legible and pre-computable*. This is not a gamble; it's an
operation with a known expected value.

## 2. Why it's the capstone

It requires every military and intelligence layer, in concert, against a target
that is **by definition the enemy's best-defended city** — reaching it means going
over, under, or around the front line, which is exactly what the specialist
doctrines are for:

| Layer | Its role in the strike |
|---|---|
| **W35 espionage** | Establish an embassy → confirm spaceship state and government (civil-war base prob). *Steal the capital's exact garrison and production.* Optionally **sabotage** its defences or **incite** a neighbouring city to open a path. Intelligence is the go/no-go gate. |
| **W04 knowledge** | Everything above is "what have I *seen*." The strike commits only when intel confidence on the garrison is high — a decapitation strike on a *guessed* garrison is how you lose an army. |
| **W32 air — airbase chain** | The capital is deep. A string of forward airbases (escorted engineers through no-man's-land) puts bombers in range of a city no land base could reach. |
| **W32 air — carpet bombing** | Strike wings bomb the capital's garrison down over one or more turns — the PREP phase. |
| **W32 air — paradrop seize** | Paratroopers drop onto the bombed-empty capital the turn the last defender falls. `ACTRES_PARADROP_CONQUER` (`citytools.c:2011`) means a paradrop *can* conquer a city — so the capital falls to paratroopers with no land approach at all. |
| **W31 naval** | If the capital is coastal or overseas: amphibious assault instead of / alongside paradrop — bombard from battleships, land the assault troops, carrier-based air for the softening. |
| **W33 operations** | Binds all of it into one phased, committed plan with a computed expected value and an abort path. Without the operations substrate this is impossible — it is a six-phase, multi-turn, multi-arm sequence, the antithesis of opportunism. |
| **W14 war ledger** | Values the whole thing: `P(civil war) × (enemy fracture value) + (spaceship denial value) − (operation cost) − (blowback)`. Decides it's worth a war, or worth escalating one. |
| **W34 victory** | Supplies *why*: this is how you deny a rival's victory and secure your own. The trigger is often "rival will launch/land a spaceship in N turns" — a deadline the strike must beat. |

## 3. The operation, phase by phase

`op_type = DECAPITATION_STRIKE` over the W33 substrate:

1. **INTEL** (W35 + W04) — establish embassy; confirm spaceship state, government
   civil-war base probability, and the capital's garrison and production. Compute
   `P(civil_war)` and confirm spaceship-denial applies. **Go/no-go:** abort if
   intel confidence is low or expected value is negative.
2. **STAGE** (W32/W31) — build the airbase chain toward the capital under escort,
   *or* muster the amphibious task force at a staging port. Phase-gated: does not
   advance until the staging infrastructure exists and is in strike range.
3. **PREP** (W32/W31/W35) — carpet-bomb the garrison; optionally sabotage
   remaining defences with a pre-positioned spy; naval bombardment if coastal.
   Advance only when the garrison is assessed clear *this turn*.
4. **EXECUTE** (W32/W31) — paradrop onto the emptied capital, or land the
   amphibious assault. `PARADROP_CONQUER` / normal occupation captures it,
   triggering `spaceship_lost()` and the civil-war roll server-side.
5. **HOLD** (W33) — the captured capital is now yours and exposed; garrison it
   against the counter-attack, or if a civil war fired, exploit the enemy's
   sudden fracture with follow-on operations.
6. **ABORT path** at every phase — if intel goes stale, the garrison reinforces
   past viability, or the spaceship already launched-and-landed (too late),
   abandon and bank the assets. A committed decapitation strike that has become
   negative-value is withdrawn, not fed in.

## 4. What makes it *fun to lose to*

This is the scenario the human player described enjoying — now pointed *at* the
human. A competent `deep` AI that:

- notices you're about to win in space,
- quietly runs a string of airbases through the wasteland toward your capital,
- confirms your garrison with a spy,
- and then carpet-bombs and paradrops your capital out of the game in one turn,
  destroying your spaceship and possibly splitting your empire —

is a genuinely thrilling opponent, and every step is legible in the decision log
afterward ("it saw the spaceship, it built toward my capital, it struck"). It is
the single best demonstration that the AI *plans* rather than *reacts*.

And symmetrically, because W32 §3.3 (air defence) and W35 counter-intelligence
exist, a `deep` AI defends its *own* capital against exactly this — spotting the
airbase chain and bombing it flat, stationing counter-spies, keeping CAP over the
capital. The move and its counter are the same knowledge.

## 5. Acceptance

The capstone integration test for the whole military stack:

- **Corpus position — enemy one launch from a space win, capital reachable only by
  air:** the AI builds an airbase chain, confirms the garrison by espionage,
  carpet-bombs and paradrops the capital, destroys the spaceship. Assert the full
  phase sequence in the operation log.
- **Civil-war EV:** given two otherwise-identical targets differing only in
  civil-war probability (one Despotism, one Democracy-with-Courthouses), the AI
  values the high-probability target's capture materially higher (L1).
- **Go/no-go on intel:** with the garrison unknown (no embassy, stale map), the AI
  runs the INTEL phase *before* committing forces, and aborts if it can't confirm
  (L1).
- **Defence symmetry:** as the *target*, a `deep` AI detects the incoming airbase
  chain and raids it, and keeps its capital garrisoned against paradrop (L1).
- **Abort:** if the spaceship lands before the strike completes, the operation
  aborts rather than capturing a now-valueless capital at full cost.

---

## 6. The one-paragraph version

Capturing the enemy *capital* does two things no other tile can, both confirmed in
`citytools.c:2022`: it **destroys a racing rival's spaceship outright**
(`spaceship_lost()`, guaranteed) and it **rolls a computable civil war**
(`civil_war_triggered()` — a probability the AI can read off their government and
buildings *before* committing) that can split an empire in half. So the
carpet-bomb-and-paradrop plan grows a purpose: a **decapitation strike** that
threads espionage (confirm the garrison and spaceship), the airbase chain (reach a
capital no land army could), carpet bombing (empty it), and paradrop-conquer (take
it) into one phased W33 operation with a known expected value — the hard counter to
a space victory the AI currently answers only by *threatening war*, and the single
best proof that it plans instead of reacts.
