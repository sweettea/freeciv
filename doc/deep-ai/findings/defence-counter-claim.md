# Defence counter-claim — the ops layer's missing bidder

Status: evidence banked 2026-08-01, design open. The referee question
for the next combat session.

## The evidence (B13, contested-fead11354a2e)

26 deep cities lost across 10 paired games (B11: 10, B12: 22 — the
rise tracks the strike-control arc making offense real).
Forensics over the ledger:

- **1/26** was an op staging city — the muster holds are exonerated.
- **21/26** were seen threatened by the survival tripwire before
  falling.
- **18/26** fell with their last recorded survival p < 0.5.

The defence model *watches* cities fall. The response side never
answers. Meanwhile conquests went 5 → 16: the aggression trade is
roughly score-neutral so far, but the asymmetry is structural, not
tuned away.

## Why the response side is mute

Offense has three ways to CLAIM a standing unit: op formation (pool
collection), the garrison draft (survival-gated borrow), raid
enrollment. Defence has none:

- The fear auction (deepwant) converts threat into BUILD wants —
  turns away when the threat is turns close.
- `dai_set_defenders` assigns only units already standing on the
  city tile, task-NONE, once per turn — it cannot summon.
- The incoming-defence credit (W11) prices en-route defenders into
  danger, but nothing *creates* en-route defenders on demand.
- Op-claimed units are invisible to every defensive pool by design
  (`deep_op_claiming` fences them), and `deep_op_holds` keeps
  MUSTER/STRIKE/CONSOLIDATE members out of stock management
  entirely.

So when a city drops below the survival bar with a wave two turns
out, the only units that can answer are free-pool units that stock's
own logic happens to route there.

## Design sketch (to referee, not to assume)

A below-bar city becomes a **bidder in the same currency ops use**:

1. Each turn, for each city with survival p below `survival_target`
   under the observed wave: compute the deficit and the arrival
   window (the wave's eta — already computed by
   `deep_city_stack_survival`).
2. The city may claim: (a) free-pool units within the window
   (ordinary reassignment — cheap, uncontroversial); (b) MUSTERING
   op members whose op has not launched, if the op's target value
   discounted by its odds is less than the city's loss value — the
   op is postponed or shrunk (its `required` re-checks at next
   drive); (c) never MARCH/STRIKE members — a launched wave turning
   around mid-assault recreates stock's dithering, the exact thing
   W33 exists to kill.
3. Banked survivors (abort paths) should prefer routing through
   below-bar cities on their way home — free reinforcement from
   units already released.

The comparison in (b) is the first place offense value and defence
value meet in one currency — it should reuse the city `worth` that
op targeting already uses (a city lost is a city worth taken,
symmetric), NOT a new scale (RC4).

## Interaction warnings

- The garrison draft borrows FROM walls for reconquest; the
  counter-claim pulls TOWARD walls. Both are survival-gated; make
  sure the gates cannot thrash (draft takes blade out at p=0.51,
  counter-claim pulls one back at p=0.49). Hysteresis exists
  already in op_take_target vs op_abort_floor — mirror that shape
  if needed, with the existing knobs.
- Raider sit-tight idles (6948 stalled raider-turns in B13) are
  claimed units stock declines to move — the counter-claim is a
  natural consumer for them, and may resolve that question without
  per-member idle state.
- W14 (war ledger / evacuation) owns the "this city is LOST, take
  the units and go" branch; the counter-claim must not fight it.

## Metrics for the referee

- Cities lost with last p < 0.5 (the 18/26 headline) — should fall.
- op_form / marches — should not collapse (defence outbidding
  everything = the old timid AI reborn).
- Counter-claim events per loss avoided (new ledger line).
- Score, obviously, contested + lategame.

---

## Implementation notes (2026-08-01, as landed)

**Claim sources, in order:** nearest free land defender arriving within
the horizon; a raider released from the hunt; a BRIGADE relay (below);
last, a poach from a mustering op priced worth-vs-worth. One claim per
city per turn (the sorties' discipline).

**The brigade (human design 2026-08-01).** Marching one blade five
turns from the nearest genuinely safe city is strictly worse than five
blades each stepping one city along a connected chain: the front is
reinforced *now*, every intermediate city ends the pass net even (sends
one forward, receives one from behind), and the hole migrates to the
far safe end. Implemented as BFS from the endangered city over own
cities on its continent, hops of at most `survival_horizon` tiles,
*through* cities holding any movable blade, *stopping* at the first
that can spare one and stay above the bar; then one blade moves per
edge along the path. If no chain terminates in a genuinely sparable
garrison, nothing moves -- a brigade that would only shuffle the hole
between two unsafe cities is never started.

## OPEN DOCTRINE QUESTION: reinforcing the doomed

First observation from the T160 fossil: claims fire on cities at
p=0.003, p=0.019, p=0.081 against waves of 6-14. One blade cannot save
those; sending it is *feeding units in*, the precise failure the
operations layer exists to stop on the attacking side. Three possible
doctrines, none chosen:

1. **Triage floor** -- claim only when the city plausibly clears the
   bar with the help (computable: survival of stack+blade vs target);
   below that, write the city off. Risks abandoning cities that a
   *sequence* of claims would have saved (the brigade delivers one per
   turn by design).
2. **Delay is worth something** -- a doomed city that costs the enemy
   two more turns and two more units is a fair trade at this price.
   Wants the war ledger (W14) to price it.
3. **Evacuate** -- W14's losing-war branch: take the garrison and the
   valuables out, keep the blades for a line that can hold.

This is a human call, not a tuning knob. Until it is made, the
counter-claim reinforces everything below the bar, and the ledger
counts what that costs.

---

## DOCTRINE DECIDED (human, 2026-08-01): evacuate the doomed

Answering the open question above. A city that is nearly falling
should **sell its buildings** and **rehome its defenders to nearby
cities if they can get there within a turn**, rather than buy its
third defender or feed relief into a loss.

Rationale: both halves recover value that is otherwise burned. A
captured city hands its buildings to the enemy intact; a garrison
that dies in a city that falls anyway converts into nothing, while
the same blades in a city that CAN hold convert into a defence.

### Gates

The doomed test must be stricter than the counter-claim's bar, or
the two fight: the counter-claim relieves cities below
`survival_target`, so evacuation must trigger only well below that.
Needs a knob -- `city_writeoff_floor`, PLACEHOLDER, tuned later --
with the invariant `city_writeoff_floor < survival_target` asserted
at load. Cities between the two bars are RELIEVED; cities below
`city_writeoff_floor` are EVACUATED. Nothing may be both.

### Mechanism

1. **Sell.** One building per city per turn is the server's limit
   (`do_sell_building`). Sell highest-value first; skip City Walls
   and Coastal Defense while any relief is still inbound, since
   selling them lowers survival further and could flip a relievable
   city into a doomed one -- the hysteresis trap.
2. **Rehome.** For each garrison unit, find own cities reachable
   THIS turn (move_rate, not eta) whose survival stays >= target
   after receiving it; send the unit and change its home city so the
   upkeep follows. Units that cannot reach safety stay and fight --
   a unit stranded outside a city is worth less than one behind
   walls.
3. Order matters: rehome first, then sell, so the sale decision sees
   the garrison that is actually leaving.

### Risks to measure, not assume

- **Self-fulfilling write-off.** Withdrawing the garrison turns
  "probably falls" into "certainly falls", and an empty city is
  walked into by anything. If the survival model is even slightly
  pessimistic, this abandons holdable cities. Metric: cities
  evacuated that the enemy then FAILED to take (should be near
  zero; a high number means the floor is too generous).
- **Cascade.** Evacuating city A reinforces city B, which may push
  A's neighbours below the floor next turn. Watch for evacuation
  chains -- more than ~2 in a row on the same front is a runaway.
- **Gold spike.** Mass selling is a one-off treasury bump that can
  look like an economic win in the score while representing lost
  infrastructure. Report buildings sold alongside gold.

### Placement

`ai/deep` (deepdefence.c), beside the counter-claim so the two share
the survival read and cannot both fire on one city. This is W14's
losing-war branch arriving early; W14 keeps the war ledger that
would eventually price the decision properly.

NOT IMPLEMENTED YET -- spec only, written while the tuning campaign
holds the binary. Implement, then verify on the T160 fossil or the
Europe save (a losing war is exactly the position that exercises it)
before any benchmark claim.
