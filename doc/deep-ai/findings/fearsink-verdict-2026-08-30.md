# The fear seat meets the settler sink: the 2×2, adjudicated (2026-08-30)

S-FEARSINK is the experiment both parent epics asked for by name.
[S-FEARSEAT](fearseat-verdict-2026-08-30.md) §8 and
[S-SITEPLAN](siteplan-verdict-2026-08-29.md) were adjudicated
concurrently in separate worktrees, each on a base **without** the
other's gates, and both act on what the T50–75 founder queue produces.
Neither arm can be subtracted from the other by hand.

The pre-registered hypothesis, in the fear verdict's own words: the
opportunity price frees ~8 shields/city of T1‑25 military at no combat
cost and converts it to **+0.19 cities at T50 that are gone by T75**,
so *"the remaining defect is downstream, in the settler pipeline"* —
and the settler pipeline is exactly what the site cap and the join
guard shut. With both on, the T50 gain should persist and the score
should move.

**It does not, in either venue, and the interaction is null.** Every
gate stays 0.

But the ledger this cell printed says the fear verdict's *mechanism*
was wrong in a way its city curve could not show: the freed shields
never reach the founder queue at all. §5 is the finding.

Cell: `benchmark/fearsink.sh`, **TRAINING seeds 1–20**, deep self-play,
five cells × two venues (`contested.conf` = 32×32/8p/T151,
`lategame-80.conf` = 40×40/6p/T81), paired on the seed so the map
cancels exactly, **203 games**, pinned binary `.pin-bin/fix`
(`87916423ae86faf1`), ports 7960–7999, nice 10, jobs 3. Results in
`benchmark/results/fearsink/`, report at `report.txt`, tables from
`fearsink.py`, `sinkspend.py` and `sinkseam.py`.

    c00   neither mechanism                          the base
    c10   mil_opportunity_gate                       S-FEARSEAT's lever
    c01   founder_site_cap + founder_join_guard      S-SITEPLAN's pair
    c11   all three                                  the corner
    boat  boat_shadow_gate                           outside the factorial

All constants stayed at their PLACEHOLDER 1.0 (`mil_opportunity_price`,
`founder_site_cap_slack`, `founder_join_guard_mult`). The cell tests
the **interaction at the placeholders**; a sweep on top of an
unadjudicated interaction would be unreadable.

---

## 0. No new code was needed, and the seam question is answered twice

The brief's one open code question: the opportunity price charges the
marginal defender the founder bid it displaces, and the site cap
withdraws the founder bid — **is a withdrawn bid still charged as
displacement?**

**No, and it cannot be.** `deep_founder_site_capped()` sets
`founder_want = 0` *before* `domestic_advisor_choose_build()` runs
(`deepdefence.c:8509`), and `domestic_advisor_choose_build()` takes a
founder or founder's-ferry branch only on `founder_want > choice->want`
or `founder_want < -choice->want` with `choice->want >= 0`
(`daidomestic.c:542-603`), so at a withdrawn bid no founder choice can
come back. `deep_mil_opportunity_charge()` (`deepdefence.c:8198`,
called at `:8619`) charges only when `deep_choice_is_founder(dom)`
holds of the choice the advisor actually **returned**. The withdrawn
bid never appears as `dom`. The join guard (`deepsettler.c:1741`) lives
entirely in the settler-run path and reads no military bid at all.

Measured as well as read, `sinkseam.py` over six c11 games:

    cap fired on   370 city-turns
    charge made on 1738 city-turns
    BOTH on          0 city-turns

**No fix was needed and none was written.** What the two seams *do*
have is a real arithmetic overlap, which is a result and not a defect —
§4.

## 1. Identity: the merge is inert at its defaults

This epic adds no mechanism, so the merge of two epics that are each
inert at their defaults must be inert too. Contested seeds 101–103 on
compiled defaults, on the pinned merged binary, reproduce the archived
`tune-postfix/s6-nogates-war/holdout-report/cfg0` scorelogs **byte for
byte** (`id` line excluded — a per-run random token). Config, tuning
and runner shas all match that run's `provenance.txt`, so the binary is
the only difference.

**3 identical, 0 different**, with four guards against a vacuous pass:
the arm must emit `deep-joinguard` and `deep-sitecap`, and must NOT
emit `deep-joinguard … held=1 gate=1`, `deep-sitecap … capped=1 gate=1`
or any `deep-milopp`. All four held.

**And both parents replicate exactly on the merged base**, which is a
stronger check than identity because it exercises the gates:

| quantity | fearseat verdict (`price`) | this cell (`c10`, war) |
|---|---|---|
| munits@25 | −0.61 ± 0.06 | **−0.613 ± 0.063** |
| military shields T1‑25 | 15.8 → 8.0 | **15.81 → 8.00** |
| cities@50 | +0.188 ± 0.064 (t 2.91) | **+0.188 ± 0.064 (t 2.91)** |
| cities@75 | +0.055 | **+0.055** |
| cities@end | +0.018 | **+0.018** |

| quantity | siteplan verdict (war `c10`) | this cell (`c01`, war) |
|---|---|---|
| joins | 24.3 → 0.03 | **25.65 → 0.00** |
| settlers built | −6.70 | **−6.50** |
| founders alive end | +1.25 | **+1.52** |
| cap firings a game | ~47 | **46.75** |
| score | +16.32 ± 9.15 (8 seats) | **+2.08 ± 1.82/seat = +16.6** |

## 2. The 2×2 on score, per seat, paired, n = 20 a cell

| cell | contested | wins | lategame-80 | wins |
|---|---|---|---|---|
| `c10` mil_opportunity | −1.05 ± 2.12 | 9/20 | +0.25 ± 1.86 | 8/20 |
| `c01` sitecap+joinguard | **+2.08 ± 1.82** | 13/20 | **+2.07 ± 1.18** | 12/20 |
| `c11` all three | −0.79 ± 1.86 | 9/20 | +1.57 ± 1.51 | 15/20 |
| `boat` boat_shadow | −0.46 ± 0.85 | 7/20 | −0.71 ± 0.90 | 6/20 |
| **INTERACTION** | **−1.83 ± 2.33** | | **−0.75 ± 2.16** | |

Interaction = `(c11 − c10) − (c01 − c00)`, taken **per seed** — the
four cells play the same map on the same seed, so the map cancels and
the SE is the SE of that per-seed contrast, which is the only honest
one. The cells are not independent samples.

**The corner is not more than the sum. It is, if anything, slightly
less, in both venues, and inside its SE in both.** Nothing here clears
its SE by a reasonable margin, so under rule 6 and the brief's bar
every gate stays at 0.

The venue-sign-flip pattern the fear verdict warns about is *not* what
this is: `c01` is positive in both venues at similar magnitude, `c10`
is a wash in both, and `c11`'s sign difference (−0.79 war, +1.57 econ)
is well inside either SE. This is a null, not a disagreement.

## 3. The city trajectory: the hypothesis, falsified

The whole point of the cell. Cities per seat, paired against `c00`.

**Contested**

| cell | T25 | T50 | T75 | end |
|---|---|---|---|---|
| `c10` | +0.050 | **+0.188** (t 2.91) | +0.055 | +0.018 |
| `c01` | +0.000 | +0.000 | +0.005 | +0.054 |
| `c11` | +0.050 | **+0.144** (t 2.15) | +0.030 | −0.102 |
| interaction | +0.000 | −0.044 ± 0.030 | −0.030 ± 0.081 | −0.174 ± 0.109 |

**lategame-80**

| cell | T25 | T50 | T75 | end |
|---|---|---|---|---|
| `c10` | +0.117 | **+0.392** (t 2.34) | +0.017 | +0.008 |
| `c01` | +0.100 | **+0.217** (t 2.48) | +0.317 | +0.275 |
| `c11` | +0.033 | **+0.325** (t 2.60) | +0.225 | +0.317 |
| interaction | −0.183 ± 0.079 | −0.283 ± 0.196 | −0.108 ± 0.327 | +0.033 ± 0.326 |

**The T50 gain still evaporates with the join shut.** Contested `c11`
goes +0.144 at T50 → +0.030 at T75 → −0.102 at the end, the same shape
`c10` has alone. The join was never what ate it.

The econ venue looks better only because `c01` holds its own cities
there (+0.317 at T75, +0.275 at end, on its own), and `c11` does not
improve on that: `c11` − `c01` at T75 is −0.09 and at end +0.04. The
combination adds nothing the pair did not already have.

`unitslost` did not rise anywhere, so the fear verdict's falsification
stays un-triggered on the combined base too: contested `c11`
unitslost@50 −0.019 ± 0.033, unitslost@75 −0.354 ± 0.326; econ
−0.225 ± 0.240 and −0.517 ± 0.556.

## 4. The two seams overlap, and the overlap is significant

Predicted from the code before the run and confirmed by it. The cap
counts founders **alive**; the guard raises founders alive by
+0.85/seat at T75 (contested); so with both on the cap binds more often
and withdraws more founder bids — and the price then finds fewer
founders to charge against.

Contested, paired interaction terms:

| counter | value | se | t |
|---|---:|---:|---:|
| `cap_fired` (a game) | **+22.55** | 8.55 | +2.64 |
| `milopp_fired` (a game) | **−41.70** | 9.80 | −4.25 |
| `milopp_zeroed` | −29.00 | 6.27 | −4.62 |

`c10` charges 305.9 times a game; `c11` only 264.2. **The two gates
partially disarm each other, by construction, with no defect
involved** — each is doing exactly what its comment says. This is the
attribution the two parent epics could not have made from concurrent
single-factor arms, and it is why the corner is sub-additive.

(The econ venue shows the same signs and does not clear its SE:
`milopp_fired` −29.3 ± 20.4, `cap_fired` +3.5 ± 11.6.)

## 5. THE FINDING: the freed shields never reach the founder queue

The fear verdict inferred that the price's shields *"reach the founder
queue and evaporate"*. That inference was drawn from the city curve.
This cell printed the **delivery ledger** (`sinkspend.py`, delivered
shields per seat per game, priced at the ruleset's build cost), and it
says something else entirely.

Contested, `c10` − `c00`:

| class | T1‑25 | T26‑50 | T51‑75 |
|---|---:|---:|---:|
| military | **−7.81 ± 1.17** | **+17.56 ± 2.84** | +2.81 ± 5.37 |
| diplomat | **−5.44 ± 1.55** | **−9.38 ± 3.93** | +7.31 ± 3.37 |
| **expansion** | **−1.88 ± 0.79** | **−0.94 ± 3.25** | +0.19 ± 2.60 |
| mil-infra | +0.19 | −1.88 | +2.44 |
| content | +0.00 | +0.31 | +4.62 |
| civil | +0.00 | −1.75 | +2.62 |
| **TOTAL** | **−14.94 ± 2.20** | +4.25 ± 4.34 | +20.00 ± 7.59 |

Three things, none of which the city curve could have said:

1. **Expansion delivery FALLS under the price, in every band and in
   both venues** (contested −1.88 / −0.94 / +0.19; econ T1‑25 −3.00 ±
   3.37). The freed shields are not spent on settlers at any point.
   *"They reach the founder queue and evaporate"* is not what happens:
   they never arrive.

2. **The opening army is deferred, not cancelled.** Military delivery
   T1‑25 −7.81 and T26‑50 **+17.56** — net **+9.8 more** military
   shields over T1‑50 than the base spends. This is the fear verdict's
   own "anticipation deferred is not anticipation avoided", which it
   found for `density_fear_ramp_turns`, now shown to hold for the
   opportunity price as well. The knob buys twenty-five turns, not an
   army.

3. **The price's largest proportional casualty in the opening is the
   Diplomat**, not the defender: −5.44 of 6.56 in T1‑25 (83%) and
   −9.38 of 29.06 in T26‑50, a 42% cut of the T1‑50 diplomat line at no
   measurable cost. `deep_want_class_of()` classifies the defensive
   diplomat as a defender, so the charge lands on it. The siteplan
   STATUS entry flagged the Diplomat as the third-largest shield line
   in the game (138.0 shields + 43.5 gold per player-game, from
   `dai_choose_diplomat_defensive`'s self-described *"totally arbitrary
   16000"*). This cell is the first measurement of what removing a
   large slice of it costs, and the answer is **nothing detectable**.

So where do the +0.19 cities at T50 come from, if not from settler
shields? From **conversion, not supply**. Contested `c10` builds
**2.30 fewer settlers a game** and founds **0.95 more cities**;
`c01` builds 6.50 fewer and founds 0.40 more; `c11` builds 7.20 fewer
and founds 0.35 more. Every arm in this cell delivers fewer settlers
and gets at least as many cities out of them. The binding constraint
on deep's expansion is not how many settlers it builds.

## 6. `boat_shadow_gate`, the secondary read

The fear verdict asked for a confirm on the cheapest correctness fix on
the board: the gate deletes 81.2% of a geography-blind flat term's
firings — the ones where no hostile hull could reach the city — and the
question was whether that is free.

| venue | score | cities@75 | cities@end | unitslost@end |
|---|---|---|---|---|
| contested | −0.46 ± 0.85 | +0.019 ± 0.016 | −0.031 ± 0.030 | **−0.92 ± 0.58** |
| lategame-80 | −0.71 ± 0.90 | +0.142 ± 0.089 | +0.142 ± 0.073 (t 1.95) | −0.33 ± 0.50 |

It reproduces the fear verdict on a fresh base to the decimal
(`unitslost@end` −0.92 here against −0.86 ± 0.57 there) and it remains
**score-inert with `unitslost` down in both venues**. It still does not
clear its SE, so it still ships at 0 — but this is now the second
independent measurement saying the term it deletes costs nothing, and
the case for a **held-out** confirm is stronger than it was, not
weaker. Note it is measured here on training seeds again, which is
adjudication, not report.

## 7. Verdict

| gate | contested | econ | ships |
|---|---|---|---|
| `mil_opportunity_gate` | −1.05 ± 2.12 | +0.25 ± 1.86 | **0** — a wash, on a second base |
| `founder_site_cap_gate` + `founder_join_guard_gate` | +2.08 ± 1.82 | +2.07 ± 1.18 | **0** — positive in both, 1.1σ and 1.7σ |
| both together | −0.79 ± 1.86 | +1.57 ± 1.51 | **0** — sub-additive, inside SE |
| `boat_shadow_gate` | −0.46 ± 0.85 | −0.71 ± 0.90 | **0** — inert on score |

**Nothing defaults on.** The ship rule wanted the both-on cell to clear
its SE by a reasonable margin in **both** venues; it clears neither.

The honest summary of the pair `c01`: it is the only thing in either
epic that is positive in both venues at consistent magnitude
(+2.08 and +2.07 per seat, 1.1σ and 1.7σ), and it has been measured
that way twice now, on two different bases. It is not evidence of a
win at the bar this project uses. It is the best candidate on the
board for a **held-out confirm at n ≥ 60**, which is the cheapest way
to find out whether ~2 points per seat is real.

## 8. Constants owed to tuning (rule 7)

Nothing new was added. Every constant in this cell is a PLACEHOLDER
inherited from a parent epic and **deliberately not swept here**:

| constant | shipped | note |
|---|---|---|
| `mil_opportunity_price` | 1.0 | Still the RC4 currency mismatch the fear verdict named: a founder bid runs ~750 against a military bid of ~175–235, so 1.0 zeroes the military bid on **22.5%** of its firings here (68.9 of 305.9 a game, contested `c10`). §5 adds a reason to want a value well below 1.0: at 1.0 the charge is large enough to defer the army rather than decline it. |
| `founder_site_cap_slack` | 1.0 | Reachable — 46.8 firings a game in `c01`, **69.3 in `c11`** (§4). The cap's reach is now known to depend on the guard, so any sweep of it must state which arm it was swept in. |
| `founder_join_guard_mult` | 1.0 | Still equivalent to switching the join off (the blocked-land signal fires on 99.6% of attempts). Fix the signal before sweeping the window. |
| `boat_shadow_distance` | 1.0 | Second measurement, same answer: inert on score. |
| `boat_shadow_count` | 1 | Axis still untested. |
| `garrison_floor_turn` | 0 | Still never run. |

## 9. What this changes about the ranking

The content/military/gold census ranked the early military want first.
S-FEARSEAT confirmed the army is free to remove and concluded the
defect was downstream in the settler pipeline. S-SITEPLAN removed the
join entirely and found the war venue's city curve did not move.
**This cell closes that loop: with both interventions on, the T50 city
gain still does not survive to T75, and the two mechanisms are not
even additive.**

And §5 relocates the question. The price does not move shields from
soldiers to settlers — expansion delivery falls under every arm here —
while foundings hold or rise on 2 to 7 fewer settlers a game. Deep is
not settler-starved. Restated for the next epic:

> **The binding constraint is not how many settlers deep builds, and
> not what it does with them at the end. It is that the T50 cities it
> does gain do not survive to T75.** Nothing measured in either parent
> epic or in this one touches that, and the city curve says the loss
> happens in T50–75 in the war venue with `unitslost` flat — so either
> the cities are not being founded to begin with, or they are being
> founded and lost to something that does not show in `unitslost`.
> Instrument the T50–75 city *births and deaths* before building
> anything else.

Two by-products worth carrying forward, both free:

- **The Diplomat line survives a 42% cut of its T1‑50 shields with no
  measurable score cost** (§5.3). That is the cheapest untested saving
  on the board, and unlike the boat shadow it is a *large* line.
- **`boat_shadow_gate` has now been measured inert twice**, on two
  bases, with `unitslost` down both times.
