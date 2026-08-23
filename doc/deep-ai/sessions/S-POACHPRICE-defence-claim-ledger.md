# Session brief — the poach's price: is deep's defence right to dismantle its own waves?

From the departure-cause addendum (STATUS 2026-08-23): 89.4% of wave
member departures are `deep_op_poach_defender()` — deep's own threatened
cities counter-claiming blades out of forming waves. 76% of musters are
poached on formation+1; 93.9% of zero-member dissolves lost at least as
many blades to the poach as they formed with. What that epic explicitly
did NOT settle: whether the poach is *right*. Every poach is a priced
decision, and there is no counter on the value it paid. This epic builds
that counter. DIAGNOSIS ONLY: log/counter changes, no behaviour change.

## The instrument

At each poach event, log both sides of the ledger and the aftermath:

1. **The defence claim**: the claiming city, its danger/urgency at claim
   time, the floor state (was the city already at/above its garrison
   floor without this unit?), and the priced worth the defence bid.
2. **The offence loss**: the operation's id, age (turns since
   formation), member count before/after, the assault's valuation of
   the blade (its share of op_net_value or the muster's bid — read the
   existing worth fields in the poach path; do not invent a metric).
3. **The aftermath** (the part that adjudicates): within a fixed window
   after the poach — window derived from existing horizon knobs, not
   invented — was the claiming city actually attacked? Did the poached
   unit fight (kills/damage/defence events), idle in the city, or get
   re-poached by a later wave? Existing event plumbing where possible.

## Corpus

Re-run the muster batch (`benchmark/muster-depart.sh` shape, tree at
`benchmark/results/employ-muster/`, replace-in-place as before) on the
instrumented binary. Also the stated cheapest next run: the **OFF arm
re-run on this binary** — the addendum flags it as inference-only today.

## Deliverable

STATUS entry with the poach-price table: fraction of poaches where the
claimed danger materialised in-window; fraction where the defender idled
un-attacked; distribution of defence-worth / offence-worth ratio at
claim time; musters vs waves split. Verdict, stated as one of: (a) the
defence over-claims and an arbitration term is the fix (sketch which
seam prices it), (b) the offence forms blades it was never entitled to
keep (sketch where formation should check the floor first), or (c)
mixed, with the split. No fix implementation in this epic.

## Guards

Own worktree + build (source dir explicit, -Daimodules=classic,tex,
baseline,deep, -Dtools= -Dclients= -Dfcmp=), pinned binary WITH
libfreeciv.so (sha-compare before pin). Ports 7400-7449, jobs <=3
(holdout batch owns 7300s, G-tune owns 7500s, capital-stake epic owns
7600s). Batch drivers at benchmark/*.sh launched detached from repo
root (`setsid bash ./benchmark/<script>.sh go` — the sandbox exclusion
needs that exact shape; no cd-prefixed compounds). DONE markers record
SUCCESS, not completion. Power-loss sweep on inherited dirs (exact
final-turn block count + nonzero maphash). Log-only changes prove
identity by byte-comparing scorelogs vs current tip on 2 seeds. One
commit one thing; meson test all suites; tune-audit CLEAN if any knob
is touched (prefer none); STATUS with pasted numbers; standard trailer;
do NOT merge — stop after the STATUS commit on your branch.
