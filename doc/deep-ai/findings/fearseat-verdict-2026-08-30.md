# The fear seat: instrumented, gated, adjudicated (2026-08-30)

S-FEARSEAT is the top-ranked lever of
[the content/military/gold census](content-military-gold-2026-08-30.md):
deep's entire holdout deficit is **0.65 missing cities per player**,
built in T50–75 (contested) / T20–50 (econ) and never moved after; in
that band deep delivers **131 shields per player of military hardware
against 15 of settlers** while `unitslost` matches baseline to ±0.03
through T75 in both venues.

This epic instrumented the three seats the census could not read, put
five constants behind gates, and adjudicated them.

**The headline: the diagnosis survives its own falsification test and
the fix does not pay.** Halving the opening army costs nothing
defensively — exactly as predicted — and buys no durable cities. The
freed shields reach the founder queue and evaporate before T75, which
is the census's *secondary* pre-registration firing: the remaining
defect is downstream of the fear seat.

**Every gate ships at 0.** None clears its SE by a reasonable margin in
both venues, and two of them have *opposite signs* in the two venues.

Cell: `benchmark/fearseat.sh`, **TRAINING seeds 1–20**, deep self-play,
paired against an off rung on the same seeds and maps, two venues
(`contested.conf` = 32×32/8p/T151, `lategame-80.conf` = 40×40/6p/T81),
pinned binary `.pin-bin/fix` (`bc40c9a4540ea1a9`), ports 7960–7999,
nice 10, jobs 4. 243 games. Results in `benchmark/results/fearseat/`,
report at `report.txt`, tables from `fearcell.py` and `paircmp.py`.

---

## 0. Identity: the whole epic is inert at its defaults

Contested seeds 101–103 on compiled defaults, run on the pinned fix
binary, reproduce the archived `s6-nogates-war` holdout scorelogs
**byte for byte** (`id` line excluded) — the same three seeds and the
same provenance the census's own instrument commit used. Vacuity guard
passed: the pinned binary does emit `deep-fear … site=ledger`, so the
comparison is not two runs of the same code.

Tool validation, on those same three games: `fearcell.py` reproduces
the census's seat table to the decimal (T1‑25 `pre` 12.5% / `density`
54.9%; T26‑50 4.5% / 72.1%; T51‑75 15.3% / 71.7% / `shadow` 30.9%;
mean `post` 2.03 / 1.83 / 7.38 / 14.85).

## 1. What landed

Five commits of mechanism, all gated, all inert at their default.

**Instrumentation** (`ai/deep: instrument the ledger seat, the density
zero and the buy danger`). `deep_credit_incoming_defence()` now prints
the same vector as the build seat under `site=ledger`; `deep-density`
is unconditional and carries `site=`, so a measured zero is no longer
indistinguishable from a term that never ran; `deep-buy` carries
`danger=` and `grave=`, so the T51 Settlers→Phalanx flip has a seat.

**`boat_shadow_gate`** (+ `boat_shadow_distance`, `boat_shadow_count`).
The shadow now asks the same reachability question
`deep_overseas_danger()` asks of a *loaded* ferry, pointed at the empty
hull: a hostile transport counts only inside `boat_shadow_distance` ×
its own one-move reach, and `boat_shadow_count` hulls in the ring are
what a landing takes.

**`fear_evidence_gate`** (+ `fear_evidence_floor`,
`density_fear_capability`). A floor of observed non-allied
attack-capable units within `density_fear_radius` below which the
anticipatory terms do not convert at all, plus a per-neighbour
capability discount that is the land twin of `menace_overseas_weight`.

**`mil_opportunity_gate`** (+ `mil_opportunity_price`). The military
bid is debited the founder bid it displaces, at the merge seam, on the
bid the auction is about to compare.

**`garrison_floor_turn`.** Holds the token garrison floor off until
turn N. Ships inert and **unadjudicated** — no rung of this A/B sets
it, so nothing here is attributed to it.

`density_fear_ramp_turns` needed no code: it already existed at 0 and
already reaches both seats. Verified live (§2).

## 2. Every rung reached its seat

Share of BUILD-seat evaluations carrying a nonzero term, contested:

| rung | density T1‑25 | density T26‑50 | shadow T51‑75 |
|---|---|---|---|
| off | 54.0% | 56.9% | 42.1% |
| ramp | **9.8%** | 46.7% | 41.0% |
| evid | 45.4% | 55.7% | 40.3% |
| all | **3.8%** | 40.8% | 46.2% |
| boat | 54.0% | 56.9% | **3.3%** |

The census's second pre-registration is satisfied: the ramp moves the
T1‑25 density share from 54.0% to 9.8%, so the knob reaches the seat
and the arm's score means something.

The evidence gate is *mild by construction*, and the instrument says so
before the score does: `evid=0` — no non-allied attack-capable unit
within `density_fear_radius` — holds on only **38.0%** of T1‑25 and
32.5% of T26‑50 evaluations, so a floor of 1 can zero at most a third
of the opening. Its measured effect (54.0% → 45.4%) is that third.

The boat geometry is the largest single change to the seat: over three
boat-rung games, 13 051 coastal city-turns had an at-war player owning
a hull somewhere (the flat term's entire trigger) and **10 593 of them
— 81.2% — had no hull within reach of that city**. The distance test
removes four fifths of the term's firings.

## 3. The falsification test: not triggered, in either venue

Pre-registered: *fewer T10–50 military deliveries with **no rise** in
`unitslost` before T75; if `unitslost` rises the diagnosis is wrong and
the census gets re-read, not the tuning re-swept.*

Contested, paired per-seed, rung − off:

| rung | munits@25 | unitsbuilt@25 | mil shields T1‑25 | unitslost@50 | unitslost@75 |
|---|---|---|---|---|---|
| ramp  | +0.01 ±0.03 | −0.05 ±0.03 | 15.8 → 16.6 | −0.01 ±0.03 | −0.14 ±0.28 |
| evid  | +0.01 ±0.03 | −0.05 ±0.03 | 15.8 → 16.6 | −0.01 ±0.03 | −0.14 ±0.28 |
| price | **−0.61 ±0.06** (t −9.8) | **−0.74 ±0.09** (t −8.4) | **15.8 → 8.0** | +0.07 ±0.09 | −0.16 ±0.36 |
| all   | **−0.59 ±0.06** (t −9.4) | **−0.76 ±0.10** (t −7.5) | **15.8 → 8.4** | +0.08 ±0.09 | −0.23 ±0.34 |
| boat  | 0.00 | 0.00 | 15.8 → 15.8 | 0.00 | −0.05 ±0.10 |

Econ, same quantities: `price` munits@25 −0.35 ±0.11 (t −3.2),
unitslost@75 −0.54 ±0.46; `all` munits@25 −0.38 ±0.15 (t −2.5),
unitslost@75 +0.07 ±0.54.

**The opening army was halved and nothing died for it.** Not one rung,
in either venue, shows a rise in `unitslost` at T50 or T75 that clears
its SE. The census's claim that the early army is pure opportunity cost
is now confirmed by intervention rather than by correlation alone.

Note which factor did the work: **`mil_opportunity_price`, not the fear
terms.** The ramp and the evidence gate change *what the danger is* and
moved the opening army by nothing measurable; the opportunity price
changes *what the danger has to beat* and halved it. Deep's opening
defenders were not bought because the danger was large — they were
bought because nothing ever told the auction what they cost.

## 4. The outcome: the shields do not become cities

Contested, `paircmp` on the same 20 seeds (`fearcell`'s per-player
score agrees in sign everywhere):

| rung | score Δ | se | wins | cities Δ | se |
|---|---|---|---|---|---|
| ramp  | **−2.48** | 1.63 | 3/20 | −0.10 | 0.06 |
| evid  | **−2.40** | 1.32 | 1/20 | −0.08 | 0.06 |
| price | +0.10 | 2.37 | 10/20 | +0.09 | 0.06 |
| all   | +0.98 | 2.08 | 11/20 | **+0.13** | 0.06 (t +2.15) |
| boat  | +0.00 | 0.74 | 8/20 | +0.00 | 0.02 |

Econ (lategame-80):

| rung | score Δ | se | wins | cities Δ | se |
|---|---|---|---|---|---|
| ramp  | +1.93 | 1.27 | 15/20 | −0.20 | 0.20 |
| evid  | **+3.84** | 1.42 | 13/20 | −0.09 | 0.17 |
| price | +0.25 | 1.86 | 8/20 | +0.01 | 0.26 |
| all   | **+3.22** | 1.54 | 15/20 | +0.23 | 0.18 |
| boat  | −0.71 | 0.90 | 6/20 | +0.14 | 0.07 |

**The city trajectory is the finding.** Contested, `price` − off:

    cities@25  +0.050 ±0.031
    cities@50  +0.188 ±0.064   (t +2.91)
    cities@75  +0.055 ±0.069
    cities@end +0.018 ±0.075

The extra cities appear exactly where the shields were freed and are
**gone twenty-five turns later**. `all` is the same shape: +0.100
±0.041 at T50 (t +2.43), −0.001 at T75, +0.105 ±0.100 at end. The
delivered-shield ledger says it from the other side — contested `price`
spends 8.0 instead of 15.8 shields on military in T1‑25 and puts **2.2
instead of 4.1** into expansion. The shields went somewhere, and in the
contested venue that somewhere was not a durable city.

This is the census's own secondary pre-registration, and it fires:
*"if the shields freed do not turn into cities, the defect is
downstream and belongs to S-SETTLERSINK, not here."* Expansion delivery
was 24.2%; freeing shields into a 24%-delivery pipe returns 24% of
them.

## 5. The venues disagree, and the disagreement is the second finding

`density_fear_ramp_turns` and `fear_evidence_gate` are **negative at
~2σ in contested and positive at 1.5–2.7σ in lategame-80**. Same
binary, same knob, same training seeds, opposite sign.

The seat table says why. Contested `ramp` vs `off`, T76 onward:

| band | density share | mean post |
|---|---|---|
| off T76‑100 | 55.7% | 10.83 |
| ramp T76‑100 | 60.8% | **14.03** |
| off T101‑125 | 54.8% | 11.55 |
| ramp T101‑125 | 60.4% | **20.36** |

Anticipation deferred is not anticipation avoided: the fear the ramp
withheld before T62 arrives *larger* afterwards, the late army grows
(military shields T101‑125 84.9 → 92.6, T126‑150 74.8 → 85.2),
`unitslost@end` rises +1.36 ±1.65, and the empire ends smaller
(pop −1.77 ±0.75, techs −1.06 ±0.49, mean city size −0.22 ±0.09,
t −2.55).

**`lategame-80` ends at T81 and never receives that bill.** Its
positive reading is the first eighty turns of a story whose second half
is what contested reports. Two venues that disagree by more than their
SEs about a knob whose whole mechanism is a *time* constant are not two
samples of one effect; the 150-turn venue is the one that saw all of
it. Recorded here so that no future campaign reads
"`fear_evidence_gate = 1`, econ +2.7σ" as an unqualified win.

## 6. Verdict, gate by gate

| gate | contested | econ | ships |
|---|---|---|---|
| `density_fear_ramp_turns = 62` | −2.48 ±1.63 | +1.93 ±1.27 | **0** — opposite signs, ~2σ apart |
| `fear_evidence_gate` | −2.40 ±1.32 (1/20 wins) | +3.84 ±1.42 | **0** — same |
| `mil_opportunity_gate` | +0.10 ±2.37 | +0.25 ±1.86 | **0** — a wash, though it is the one factor that moves the mechanism |
| `boat_shadow_gate` | +0.00 ±0.74 | −0.71 ±0.90 | **0** — inert on score |
| `garrison_floor_turn` | not run | not run | **0** — unadjudicated |

Nothing clears its SE by a reasonable margin in both venues, so nothing
defaults on. The all-on corner is the only cell positive in both
(+0.98 ±2.08, +3.22 ±1.54) and it does not clear contested either;
reading it as a win would be reading the econ venue's truncation.

Two of these deserve a follow-up rather than a shelf:

- **`boat_shadow_gate` is the cheapest correctness fix available.** It
  deletes 81.2% of a term's firings — the ones where no hull could
  reach the city — at a measured score cost of zero (+0.00 ±0.74) and
  with `unitslost` *down* in both venues (−0.86 ±0.57, −0.31 ±0.35). A
  confirm run on held-out seeds is worth more than another sweep.
- **`mil_opportunity_gate` is the only lever that reached the quantity
  the census named**, and it is score-neutral because of §4, not
  because it failed. It becomes interesting again the moment the
  settler pipeline holds what it is given.

## 7. Constants owed to the tuning pile

Every one is a PLACEHOLDER with a range in `tuning-params.ini`, and all
seven are in the new tier-B block **BANTICIP**.

| constant | shipped | note |
|---|---|---|
| `density_fear_ramp_turns` | 0 | 62 was measured, not chosen: the median turn combat first touches a deep player is 62.5 (min 27, max 120, n=20 archived games); first city loss median 105. The value is sound; the *sign* is venue-dependent. |
| `fear_evidence_floor` | 1 | The unit case. Bites 38.0%/32.5% of T1‑25/T26‑50 evaluations — measured, so the next sweep knows its reach before it buys games. |
| `density_fear_capability` | 0.25 | Same magnitude as `menace_overseas_weight` / `floor_interior_weight`. |
| `mil_opportunity_price` | 1.0 | **Calibration fact worth more than the knob:** in the opening a founder bid runs ~750 against a military bid of ~175–235, so at 1.0 the charge zeroes the military bid on **19.5%** of its firings (882 over 3 games). The two sides are not in the same currency (RC4). A value well below 1.0 — or a currency fix — is what this wants. |
| `boat_shadow_distance` | 1.0 | Hull reaches, not tiles. At 1.0 only 18.8% of the hulls the flat term feared are within reach. |
| `boat_shadow_count` | 1 | Untested axis: the rung ran at the shipped unit case. |
| `garrison_floor_turn` | 0 | Never run. Owes an arm as well as a value. |

`upkeep_gold_weight` was deliberately **not** swept here: it moves the
T100+ building auction and would have made the military result
unreadable.

## 8. Attribution

**S-SETTLERSINK (site cap + join guard) was adjudicated concurrently in
another worktree, and both epics act on what the T50–75 queue
produces.** This A/B's base is a tree *without* those gates, so every
number above is the fear seat's effect on that base. The combined
reading needs a **2×2 after both merge**; neither epic's arm can be
subtracted from the other's by hand. §4 makes that urgent rather than
bookkeeping: the fear seat demonstrably delivers shields into the
founder queue at T50, and whether they survive to T75 is exactly the
question S-SETTLERSINK is asking.

## 9. What this changes about the census's ranking

The census ranked the early military want first because it was the only
signature present in the causal window. That ranking was right about
the *window* and right about the *cost* — the army is free to keep, its
shields are taken when they are worth most, and removing half of it
costs no units — but the lever does not reach the score, because the
queue it hands the shields to does not hold them.

Restate the top line: **the early army is not the cause of the missing
cities; it is the largest single claimant on a settler pipeline that
would not have kept the shields anyway.** Fix the pipe first. The fear
seat's gates are built, instrumented, inert, and waiting for a base on
which freeing a settler's worth of shields at T50 still shows up at
T75.
