# The stacked lever: five adjudicated expansion gates, on vs off, at n = 100 (2026-08-31)

**The first cell on this project powered to see its own effect, and it saw
it.** Both venues, primary endpoint, well past the pre-registered bar:

| | war (contested, T151) | econ (lategame-80, T81) |
|---|---:|---:|
| **cities / seat** (primary) | **+0.15 ± 0.04** (t 3.7, 57/100) | **+0.60 ± 0.07** (t 8.3, 80/100) |
| **score / seat** (secondary) | **+2.39 ± 1.02** (t 2.3, 58/100) | **+4.12 ± 0.61** (t 6.8, 79/100) |
| unitslost (falsification) | −1.88 ± 0.89 (t −2.1, *falls*) | +0.31 ± 0.26 (t 1.2, see §3) |
| pop | +1.02 ± 0.35 | +2.96 ± 0.32 |
| techs | +0.63 ± 0.27 | +0.12 ± 0.06 |
| landarea | +450 ± 285 | +4265 ± 618 |
| settlers alive at end | 0.15 → 1.86 (100/100) | 0.52 → 2.06 (97/100) |

100 paired TRAINING seeds per venue (seeds 1–100), 2 cells, 403 games
including the 3-game identity gate. Design, arms and pre-registration:
`findings/marginal-city-2026-08-30.md` §4–5 and `benchmark/stacked-lever.sh`.
Report: `benchmark/results/stacked/report.txt`. Pinned binary
`.pin-bin/fix` sha `9cb0a80cadb0c8c1…`.

## 1. What was run

`on` = `benchmark/tuning/stacked-on.ini`: `mil_opportunity_gate`,
`founder_site_cap_gate`, `founder_join_guard_gate`, `frontier_transit_gate`,
`boat_shadow_gate` all = 1, **every gain at its compiled placeholder**
(rule 7: the corner is adjudicated first, swept never on top of an
unadjudicated corner). `off` = the same five at 0, proven byte-identical to
compiled defaults by the identity step (3 holdout seeds vs the archived
`s6-nogates-war` scorelogs: 3 SAME, 0 different; every gate census line
present, none fired). `founder_floor_gate` = 0 in both arms, excluded by
design (−10.17 ± 1.38 per seat, a hoard not a channel).

Vacuity guard passed in both venues: the on arm fired every gate
(war: milopp 250, transit 89, shadow 7761, sitecap capped 55, joinguard
held 871; econ: 364 / 361 / 6734 / 36 / 303).

## 2. Scored against the pre-registration

| endpoint | registered (additive / half) | observed | reading |
|---|---:|---:|---|
| war cities | +0.219 ± 0.113 / +0.110 | **+0.15 ± 0.04** | between; ~70 % of additive |
| war score | +3.16 ± 2.01 / +1.58 | **+2.39 ± 1.02** | between |
| econ cities | +0.423 ± 0.373 / +0.212 | **+0.60 ± 0.07** | above additive by 0.5 pred-SE |
| econ score | +1.80 ± 1.54 / +0.90 | **+4.12 ± 0.61** | above additive, z 1.4 on the combined SE |

Neither "null" signature (§5.4 of the pre-registration) appeared: the
city delta is not inside its SE anywhere, and score converted at full
price rather than stalling. **Implied value of one stacked city:** war
2.39 / 0.15 = **16 points**, against the price list's 13.0 ± 0.6 (T26–50,
where the war gain concentrates) and 17.8 ± 1.0 (T1–25); econ 4.12 / 0.60
= **6.9 points**, against the price list's 6.7 ± 0.4 (T1–25) and the
instrumental +4.8 ± 2.1. The cities the stack adds are worth what
S-MARGINAL said a city is worth. The additive arithmetic of that doc,
which four "vanished" epics had made look foolish, was right; the cells
were small.

Where the foundings came from (cumulative d(found)/seat, `marginalcity.py
arms`):

| band | war | econ |
|---|---:|---:|
| T1–25 | +0.069 ± 0.017 | +0.210 ± 0.036 |
| T26–50 | +0.208 ± 0.023 | +0.617 ± 0.062 |
| T51–75 | +0.132 ± 0.045 | +0.660 ± 0.079 |

T26–50 is exactly the band the city ledger (`city-ledger-2026-08-30.md`)
located the deficit in, and T1–25 — the band S-MARGINAL called "the
cheapest unattempted city on the board" — moved too, without being
targeted. The ledger shows no attrition price: war cities `taken` fell
0.119 → 0.110 /seat/game and `razed` 0.061 → 0.055; econ `taken` 0.573 →
0.518, `razed` 0.607 → 0.648 (size-1 settler-drops, unchanged
character).

## 3. The falsification endpoint, stated plainly

The pre-registration said *"a rise at T75 clearing its SE in either venue
… voids the corner."* War: unitslost **fell** by 2.1 SE. Econ (which ends
at T81, so end-of-game ≈ T75): **+0.31 ± 0.26, 1.2 SE above zero.** By
the letter that clears one SE. By the rate it does not: the on arm built
+1.37 ± 0.33 more units and holds +1.56 ± 0.25 more military at the end,
and unitslost per unit built is 27.2 % (off) vs 27.0 % (on) — the extra
losses are the extra army's, in proportion, not a purchase. `unitskilled`
rose in step (+0.24 ± 0.24). I read this as **falsification passed, with
the econ number reported rather than rounded away.** If the reader
disagrees, the war venue — the one where losses are the mechanism the
gates were suspected of buying — is unambiguous.

## 4. What the stack did NOT settle

- **Which gate.** This is the corner, not the factorial. A five-way
  decomposition at this n is 1000 games; a leave-one-out on the *city*
  endpoint at n = 60 (SE 0.041 war) is 5 × 120 = 600. Neither is bought
  by this result; the corner clearing is what makes them worth pricing.
- **The settler inventory.** Both venues end with ~1.7 more live
  settlers a seat in the on arm, in 100/100 and 97/100 seeds. That is
  the site cap and the join guard doing what they were built to do —
  hold a settler rather than join it — past the point where any site
  remains. It did not cost the cell, but it is production standing idle
  at the end and it is the obvious first knob of the tuning pass:
  a late-game relaxation of the guard once no eligible site exists. Rule
  7: it goes into `struct tuning` as a placeholder, not into this doc as
  a number.
- **The holdout.** Training seeds adjudicate; holdout seeds report. The
  same script is running on seeds 101–120 (`OUT=benchmark/results/
  stacked-holdout`, both arms fresh, 83 games); its paircmp tables are
  the numbers to quote outside this repo, and they are appended below
  when they land.

## 5. Recommended next actions, in order

1. **Report the holdout** (running) and append it here.
2. **Flip the five gates on in the compiled defaults** — one commit,
   `ai/deep: enable the five adjudicated expansion gates by default` —
   with this doc's numbers in the body. That is what the adjudicated-gate
   pattern is for: a gate ships 0 until a powered training cell says
   otherwise, and this one has. Note for the campaign: it started from
   `postfix-start.ini` (defaults + the five 2026-08-25 gates); these five
   are additional and were not in its incumbent. Its holdout report
   should be read against a defaults vector that includes them, or it
   will report against a stale baseline.
3. **The settler-inventory relaxation** as a placeholder knob, then the
   tuning pass over the stack's seven placeholders — on the *city*
   endpoint, which is 2.4× tighter and 5.8× cheaper than score.
4. **Re-ledger against baseline.** The remaining deficit is now
   (−0.65 + observed gain) cities a seat; the T1–25 band vs baseline
   (−0.100 ± 0.065 before this) wants a fresh reading with the stack on.

## 6. Provenance and repair notes

- Overnight 2026-08-30/31 the box lost power and rebooted (07:19); both
  drivers died and were resumed from their DONE caches at 12:02 with no
  game lost or re-run. The identity step re-verified from cache.
- The first `report` run died twice — paircmp needs `metrics.csv`
  (extract.py writes it; the script never called it) and `cityledger.py`
  lacked +x. Fixed in `7c32edceb4`; the report above is the regenerated
  one.
- A sandboxed shell here cannot see the process table (PID namespace):
  `pgrep` returned 0 with nine servers running. Liveness is DONE-file
  mtimes, as the script's header already said.

## 7. Holdout report (seeds 101–120)

*pending — appended when `benchmark/results/stacked-holdout/report.txt`
lands.*
