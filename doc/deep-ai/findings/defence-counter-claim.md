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
