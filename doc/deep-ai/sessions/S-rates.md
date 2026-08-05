# Session brief — rates: set sci/tax/lux from amortized value

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5 harness rules, the
STATUS.md tail, and `ai/deep/deephorizon.c` around line 430 (the credit
conversion that READS the rates).

## The lead (NEXT-SESSION §3, ownerless until now)

Deep reads `pplayer->economic.tax / luxury / science` when valuing tiles and
horizon credits (`deephorizon.c:430-445`) and never sets them. Science, tax
and luxury should be set from the amortized value of what they buy — techs
(which the tech-wall credits already price), gold (which the upkeep work
already prices), and happiness (which the CM already prices) — instead of
whatever stock leaves them at.

## THE JOB

1. **Establish the current path first.** Stock manages rates somewhere
   (find it — `dai_manage_taxes` or similar in `ai/default`); determine
   whether deep still forwards to it and what policy it implements. Write
   what you find into the session's STATUS section — this brief
   deliberately does not assert it.
2. **Design the deep policy as a comparison of amortized marginal values,
   not a schedule.** One more point of science is worth the amortized
   tech-want it advances (the tech-wall credits, `horizon_tech_weight`
   lineage); one more point of tax is worth gold at the treasury's
   marginal utility (upkeep bills, buy opportunities, the treasury guard);
   luxury is worth the CM's happiness value. Respect `H_RATES` (set at
   EVERY stock level including hard — deep must stay inside government
   maxima like everyone else) and the government's rate bounds.
3. **A/B-able via the tuning file** (`rates_*` keys, compiled defaults =
   off / stock behaviour preserved), execution counter proving the
   off-switch, and the deep-dv or a `deep-rates` log line showing the
   chosen splits per turn.
4. **Measure**: 20 paired seeds, `--arms deep`, one economy cell
   (isolated-150 or garden2b) + smoke guard. Off-arm byte-identity to the
   pre-session binary. Report score direction + proxy metrics (techs,
   gold curve, unhappy city-turns). 20 seeds is direction; say so.

## Rule 7

Any weighting constant is a `struct tuning` PLACEHOLDER registered in all
four audit places, flagged for the cloud pile. Prefer reusing already-priced
quantities (tech wants, upkeep costs, CM factors) over new constants — the
design above needs at most a gain or two.

## Scope fences

One epic (5-8 commits). No trade-route changes (next session), no founder
changes. `ai/default` untouched beyond the visibility exception. STATUS.md
dated section as final commit; leave NEXT-SESSION.md and sessions/ alone.

## Stop conditions

Stock's rate policy turns out to already be forwarded AND adequate (report
with evidence instead of building); the marginal values needed are not yet
priced anywhere (naming the missing price is the finding).
