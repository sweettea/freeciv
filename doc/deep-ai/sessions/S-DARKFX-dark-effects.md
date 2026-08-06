# Session brief — DARKFX: the dark-effects round (pre-v4)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md`
§5, `findings/deep-dive-2026-08-05/economy-effects.md` IN FULL (your
evidence: the coverage tables, A1's trigger-switch mechanics, A2's money
list, the ranked opportunities), and the D2/D3 STATUS tails (the credit
machinery and the constants-into-knobs precedent you follow).

## THE JOB — deep-side credits beside the horizon ladder, same shape

1. **Empire-scope the gov-center value** (the Ecclesiastical Palace):
   credit `EFT_GOV_CENTER` carriers with the distance-waste saved across
   OTHER cities (the ruleset waste formulas are readable; the sum is
   over own cities — cheap). Same ledger and delivery as the horizon
   credits (post-D2 honest). One gain. Also: Palace relocation on merit
   is now possible to WANT (base_want returns 0 for owned small wonders
   — your credit doesn't), but actually moving it is a build choice that
   exists already; measure whether the want ever fires.
2. **Player-range effects at `c` cities** (King Richard's, Statue of
   Zeus): the `cities[REQ_RANGE_PLAYER]` count is already computed and
   passed as `c` — price `UNIT_UPKEEP_FREE_PER_CITY` and the MinSize
   free-upkeep ladder at their empire-wide worth. Cheapest structural
   win in the audit's ranking.
3. **Buy on gold-per-shield**: rank `deep_spend_gold`'s greedy loop by
   `want / buycost` instead of want alone (the quadratic unit curve
   makes top-ups ~4× cheaper per shield). Emergencies exempt (their
   tier already outranks; do not touch the triage).
4. **Specialist type as a decision**: derive the CM's gold vs science
   factors from treasury/research state (the solvency signals exist —
   treasury guard, tech upkeep) instead of both pinned at 2. Two
   PLACEHOLDERs. (The full CM-completion session, #28, owns the bigger
   state-machine work — keep this to the two factors.)
5. **`MAX_RATES` in government choice**: closed-form ceiling term in
   deep's `gov_value` wrapper (the hook is forwarded today): the value
   of a higher rate cap priced by R1's marginal-value machinery at the
   current split's binding constraint. This is the seam-map's
   "cheapest seam in the tree" made concrete.
6. **Disaster risk, minimally**: civ2civ3 runs disasters at 10×; charge
   Mfg/Nuclear Plant's pop-loss expectation and the Hills-city
   shield-stock exposure as a small credit debit (ruleset frequencies ×
   consequences — measured facts, one weight). If the expected values
   are negligible, report the numbers and skip the code.
7. **Border-push check (from the mechanisms round)**: verify which
   border/culture effects civ2civ3 buildings carry; if any are dark and
   material, add to the credit list; else close with evidence.

## Acceptance

- Every credit behind its weight (PLACEHOLDER), defaults byte-identical
  at 0 EXCEPT where the audit's evidence justifies a measured non-zero
  (state which, with the number, D3-style).
- Counters per credit; A/B on garden2b + isolated-150 (economy cells) +
  contested guard, 20 paired seeds: score direction, the specific
  buildings' build rates (EP, KRC, SoZ — currently HISTORY-WAG-priced),
  buy efficiency (shields per gold spent), specialist mix.
- `meson test` + `tune-audit.py` CLEAN at counts; STATUS final;
  sessions/ untouched.

## Scope fences

7-9 commits — the largest round in the wave; if it runs long, land 1-2
(the structural pair) + 3 and defer 4-7 with written designs rather than
rushing. No trade-route work, no wonder-race logic (WRACE), no CM state
machine beyond the two factors.

## Stop conditions

The gov-center distance sum needs per-tile waste internals not exported
(check `common/city.c` first — likely fine); any credit regresses beyond
noise without attribution.
