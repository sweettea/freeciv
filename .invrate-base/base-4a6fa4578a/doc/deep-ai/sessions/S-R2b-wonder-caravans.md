# Session brief — R2b: wonder caravans on merit

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5, the R2 tail of
STATUS.md (`ba3c506398`) and `ai/deep/deeptrade.c` as R2 left it — the
seam probe, the paired-pricing census and the virtual-caravan handling
(including the upstream-bug-34 workaround at `8c5c573d29`) are the
machinery this session extends. Also `dai_choose_help_wonder` and
`dai_wonder_advisor` (`daidomestic.c:66-166`) — stock's wonder-caravan
producer, the larger claimant on the caravan queue (winning want p50 197
vs the trade producer's 71, measured in R2).

## THE JOB

Price the wonder caravan the way R2 priced the trade caravan: by what it
actually delivers, amortized over when.

1. **The value side**: a wonder caravan delivers its full shield cost into
   the wonder city's current build. That is worth the wonder's own want
   (already in `building_want[]` for the wonder city — horizon-credited
   and post-D2 honest) × the fraction of the wonder the shields buy,
   amortized over walk time. NOT a new scale: it is the wonder city's
   existing want re-denominated per shield delivered. Stock instead uses
   a count heuristic (build caravans while `total_shields < cost ×
   caravans`) with a distance-blind want.
2. **The delay side**: same discounting R2 landed (build delay + walk),
   same `caravan_delay_gain` family if R2 registered one — reuse, don't
   mint.
3. **The which-wonder question, minimally**: do not build a wonder
   chooser (that is the audit's gap #6, out of scope). Take
   `adv->wonder_city` and its current wonder as given; the session only
   makes the HELP decision honest.
4. **Off-switch** (`wonder_merit` or fold into R2's `trade_merit` key if
   the code naturally shares the path — your call, stated), compiled
   default off, byte-identity 10/10, counters.
5. **A/B**: primary-120-classic (R2's live venue) + one civ2civ3 cell
   (wonder caravans ARE legal on civ2civ3 — verify and state; only trade
   routes were zeroed). Metrics: wonders completed and completion turns,
   caravan counts by producer, help-wonder deliveries, score direction.
   20 paired seeds; counters first.

## Rule 7

Placeholders registered everywhere + flagged; reuse R2's gains where the
shape is shared. R3's corpus follows this session — the deflator table
must see the reformed wonder want, which is why R2b precedes R3.

## Acceptance

Off byte-identity + zero counters; on-arm counter census; the A/B tables;
calibration note (where the reformed want sits vs stock's p50 197 and vs
the building anchor — R3's input); `meson test` 3/3;
`tune-audit.py` CLEAN at its count. STATUS dated section final; leave
NEXT-SESSION.md and sessions/ alone.

## Scope fences

4-6 commits. No wonder-selection logic, no trade-side changes beyond
shared plumbing, no `ai/default` beyond visibility (R2's export may
already suffice — `dai_choose_help_wonder` visibility only if needed).

## Stop conditions

The wonder want turns out not to be deep-interceptable without touching
`dai_wonder_advisor`'s internals beyond visibility; bug-34-class crashes
on the virtual-unit probe for Help Wonder (file it, work around it the
R2 way, or stop if not workaroundable).
