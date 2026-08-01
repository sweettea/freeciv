# The objective layer, and why it comes before tuning

Human proposal, 2026-08-01, in response to "the score gap is
economic": *put the hardness/objective layer on top first, force all
deep AIs to seek an economic win, tune the economies in that regime,
then tune military, then tune both together.*

This document records why that is the right order and what the layer
minimally has to be. It supersedes the block-tuning plan sketched in
the same STATUS chapter (blocks S/E/M, coordinate descent), which is
kept below as the fallback.

## 1. The problem with tuning first

`benchmark/tuning-params.ini` exposes **82 constants**. The optimiser
is a diagonal (mu, lambda) ES at pop ~6: it cannot search 82
dimensions, so the knobs must be tuned in blocks. But the blocks are
not separable, and the coupling is the exact defect we measured
today: economy and military bid for **one shield budget**, so an
economy optimum found while the fear knobs are wrong is an optimum
for spending a starved budget. Coordinate descent over coupled blocks
converges to a joint optimum only under conditions we cannot assume
here.

The standard mitigation is alternation and re-checks -- expensive,
and it never removes the assumption.

## 2. What forcing the objective changes

An explicit objective **creates** the separability instead of
assuming it. With every deep player pinned to ECONOMIC:

- military spend collapses to the survival floor, so the shield
  budget the economy knobs allocate is stable across candidates;
- the objective function (score, or its economic components) becomes
  a direct read of the economy knobs -- higher signal per game, so
  fewer games per candidate;
- war outcomes stop injecting variance that has nothing to do with
  what is being tuned.

Then MILITARY-pinned tuning does the same for the war knobs, and the
final joint pass tunes the *choice* between objectives -- which is a
handful of parameters, not 82.

**The regime must be real, not decreed.** A deep player pinned to
ECONOMIC on a contested map still gets attacked; if it refuses to
build defenders it dies, and the economy constants get tuned in a
world that never occurs. So the economy pass runs on the low-contact
cells we already have (`isolated`, `garden`), and ECONOMIC means
"minimise military spend subject to survival", not "no army".

## 3. Why it is worth building for its own sake

This is not scaffolding. Every open question from today's combat work
is objective-dependent and currently unanswerable:

- **The doomed-city triage** (findings/defence-counter-claim.md):
  reinforce, delay, or evacuate? A conquest-seeking player trades
  differently from an economic one.
- **"Attack is much more highly powered"** (human, same session): true
  under a conquest objective; under an economic objective the correct
  answer to a big wave may be to buy it off or outgrow it.
- **Wonder and tech priority**, expansion aggression, whether a raid
  is worth the shields -- all of it is "it depends on what we are
  trying to win".

`00-context.md` §1 says the stock AI "doesn't decide where its empire
should be... doesn't decide to build up rather than out... doesn't
know whether a war is going well." The objective layer is the missing
decision that sits above all of those. W08 (doctrine) is the planned
home; this pulls it forward, ahead of the tuning campaign and ahead
of the remaining combat refinements.

## 4. Minimum viable layer

```
enum deep_objective { DEEP_OBJ_ECONOMIC, DEEP_OBJ_MILITARY,
                      DEEP_OBJ_BALANCED };
```

- Lives in `struct deep_plr`, saved with the rest of deep state.
- **Forceable for benchmarking** (env var / tuning file key), so a
  tuning pass can pin every deep player. This is the property the
  whole plan rests on; build it first.
- Chosen by the AI when not forced -- initially from position
  (land available, neighbour militarisation, victory conditions
  enabled in the ruleset), later from the war ledger (W14).

What it gates, in the order the code already offers seams:

1. **The fear -> want conversion** (`deep_credit_incoming_defence`,
   the injected fear terms): ECONOMIC scales the anticipatory fears
   down toward the survival floor; MILITARY leaves them.
2. **Operation formation** (`deep_op_form`): ECONOMIC forms assaults
   only against opportunistic or threatening targets; MILITARY forms
   freely.
3. **Build wants** (`deepwant`, `horizon_*`): objective-weighted
   growth ladder vs barracks/walls.
4. **Expansion** (settler rush, colony lift): ECONOMIC expands while
   land remains; MILITARY converts land pressure into targets.

Each gate is a multiplier on an existing want, not a new scale -- RC4
says do not add another incommensurable currency.

## 5. Hardness is a separate axis

"Hardness" (difficulty presets: handicaps, `H_MAP`/`H_FOG`, cheat
levels) is orthogonal to objective and matters for a different
reason: RC6 says the AI is omniscient at every level anyone plays,
which makes the strategy layer unfalsifiable. Constants must be
tuned at the hardness we intend to ship and measure, or they encode
compensation for information we will later take away. Fix hardness
for the campaign; revisit when W04 lands the honest-information
path.

## 6. Order of work

1. Objective enum + forcing mechanism + the four gates (one epic).
2. Verify the regimes are real: with every deep player pinned
   ECONOMIC on `isolated`, military spend should fall to the floor
   and the army ratio approach baseline's; if it does not, a gate is
   missing.
3. Tune ECONOMIC on the low-contact cells.
4. Tune MILITARY on `contested` / `lategame`.
5. Tune the objective *choice* jointly, on the blended config set.
6. Report on held-out seeds only (`tune.py --report-holdout`).

## Fallback (if the layer is deferred)

Block coordinate descent, coupling block first: **S** (the split:
`survival_fear_value`, `survival_target`, `garrison_floor_danger`,
`garrison_menace_slope`, `barbarian_fear`, `boat_shadow_fear`,
`density_fear_value`, `overseas_fear`, `wall_fear_weight`,
`satisfied_fear_discount`) -> **E** (`site_*`, `horizon_*`, `cm_*`,
`settler_rush_weight`, `colony_lift_weight`) -> **M** (`op_*`,
`raid_radius`, `invasion_reinforce_rate`, `attack_value_weight`,
`sortie_*`, `convoy_*`) -> re-run S as an alternation check. Blend
configs so no block overfits a regime.
