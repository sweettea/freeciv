# Session brief — trade routes on merit

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5,
`findings/trade-routes.md`, `ai/deep/deeptrade.c` (all of it — the header
comment documents the existing income model), and stock's
`dai_choose_trade_route()` in `ai/default/daidomestic.c` (~line 168 on).

## The lead (NEXT-SESSION §3; human call 2026-08-04)

Stock prices a caravan on *initial* route income, undiscounted — there is no
`amortize()` anywhere in `daidomestic.c` — then clamps want to 0.1 when
caravans pile up, and the fallback ladder builds one at want 1 exactly when
the city wants nothing. Deep's `caravan_discount` intelligence is spent on
choosing the DESTINATION (`deeptrade.c`), not on the build want. **The human
call: merit should amortize the same way as everything else.**

## THE JOB

Replace the caravan build want deep inherits with an amortized one: the
discounted stream of route income over the horizon (initial + ongoing,
double-payout rule per `traderoutes.c:478` already encoded in deeptrade.c's
header), amortized over delivery ETA the same way horizon credits amortize
over completion — minus what the shields would otherwise buy (that
comparison is the auction itself; do not double-price it). Wonder-caravan
want (`daidomestic.c:66` path) is a separate producer — leave it unless the
audit shows it flows through the same seam trivially.

Kill the two stock pathologies at deep's seam, not in `ai/default`:
- the pile-up clamp (0.1) — replaced by honest marginal value of the Nth
  caravan (existing assigned/enroute counts are already computed);
- the fallback ladder's want-1 caravan — deep's choose_build already owns
  the final merge; make sure the ladder path cannot inject it under deep.

## Acceptance

- Off-switch (`trade_merit` tuning key, compiled default off) proven by
  counter; off-arm byte-identical to pre-session binary, 10 smoke seeds.
- On-arm A/B: 20 paired seeds, `--arms deep`, one trade-friendly cell
  (garden2b or isolated-150) + smoke guard. Report score direction plus
  the mechanism metrics: routes established/game, caravan count, want
  distribution at the seam (the W01 calibration line already logs final
  build decisions — use it), fallback-ladder injections (must be 0 on-arm).
- `meson test` passes; `tune-audit.py` CLEAN at its new count.
- STATUS.md dated section as final commit.

## Rule 7

Amortization horizon/discount: reuse the existing horizon machinery's
constants — do NOT mint a trade-specific discount unless the design forces
it, and then it is a PLACEHOLDER flagged for the cloud pile.

## Scope fences

One epic (5-8 commits). No rates changes, no class-rescale (separate
sessions). `ai/default` untouched beyond visibility. Leave NEXT-SESSION.md
and sessions/ alone.

## Stop conditions

The build-want seam turns out not to be deep-owned for caravans (i.e. the
want forms somewhere deep cannot intercept without touching `ai/default`
beyond visibility) — report the seam; that finding gates the session.
