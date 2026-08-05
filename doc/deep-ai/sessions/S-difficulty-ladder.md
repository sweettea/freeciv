# Session brief — the deep difficulty ladder

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`,
`doc/deep-ai/00-context.md`, `NEXT-SESSION.md` §5,
`findings/difficulty-audit.md` (produced by the audit session — **this brief
was written before it; where they disagree, the audit wins**), and the
STATUS.md tail.

## THE GOAL (human, 2026-08-04)

Deep at skill level L is (a) *meaningfully similar* to stock at L — same
knobs, comparable nerf magnitude, legible to a player who chose that level —
and (b) usually beats stock at L. Both clauses matter: a deep that ignores
its handicaps "wins at novice" vacuously and is not shippable as a
difficulty setting.

## THE JOB

1. **Close the audit's gaps, by priority.** Make deep's own decision paths
   respect the same handicaps stock respects, wherever the audit found a
   bypass. The likely list (audit decides the real one): H_EXPANSION on
   deep's expansion wants (settler rush, colony lift, site search),
   H_HUTS/H_LIMITEDHUTS on the explore want, H_DEFENSIVE on build choice,
   H_DANGER on deep's danger/fear terms, H_TARGETS on deep ops targeting,
   H_DIPLOMAT/H_NOPLANES where deep chooses those units, H_PRODCHGPEN
   where deep switches production. Reuse stock's semantics for each
   handicap (same functions where the visibility rule allows) rather than
   inventing deep-only meanings; `expand` and `science_cost` are
   server/common-side and should already bind — verify, don't assume.
   Fuzzy: deep calls `ai_fuzzy()` at 2 defence sites today; extend to the
   decision points that correspond to stock's usage, not to every deep
   decision (fuzzy at a NEW class of decision would make deep-novice
   arbitrarily worse than stock-novice, violating "similar").
2. **No invented constants.** The per-level values (fuzzy 400/300,
   expand 10, science 250, the bitvectors) are STOCK's — reuse
   `ai/difficulty.c`'s assignments as-is via the existing
   `set_ai_level_directer()` path. If any deep-side scaling constant is
   genuinely needed (e.g. a handicap-conditional damping on a deep want),
   it is a `struct tuning` PLACEHOLDER flagged for the cloud pile, and its
   compiled default must leave `hard` byte-identical.
3. **The guard that outranks everything: `hard` is untouched.**
   Byte-identity at SKILL=hard vs the pre-session binary, 10 smoke + 10
   isolated-150 paired seeds. Every current benchmark number in this
   project was taken at hard; a hard-level behaviour change invalidates
   the corpus.
4. **The ladder measurement.** Mixed deep+stock games (both arms at the
   same level, seat-paired, the mixed_db/mixed_bd mirror pattern), per
   level {novice, easy, normal, hard}, one primary cell + one guard cell,
   20 paired seeds per level minimum (direction; 40+ for any claim):
   - **beats-stock check**: deep/stock seat-paired score ratio > 1 and
     deep wins the majority of seeds, at every level;
   - **similarity check**: deep's self-ladder is monotonic and its
     level-to-level nerf is within the same ballpark as stock's
     (report deep@L vs deep@hard alongside stock@L vs stock@hard — the
     two nerf profiles should be the same order of magnitude, not
     identical);
   - **legibility spot-check**: at novice, the handicaps visibly fire
     (execution counters / log lines — e.g. expansion damping engaged,
     fuzzy flips logged), so "similar" is proven by mechanism, not
     inferred from score.
5. **Report per level with n, t, and win counts.** "Usually beats" is the
   human's bar: majority of seeds at every level, stated per level. If a
   level fails it, that is the finding — report which handicap costs
   deep disproportionately; do not quietly weaken deep's handicap
   compliance to buy the win back.

## The build-choice sampler (human, 2026-08-05 — part of this epic)

Replace blind fuzzy for deep's BUILD CHOICES with want-proportional
sampling: softmax over the city's candidate builds, temperature set per
difficulty level. A weaker deep picks a plausible-but-not-best build —
legible weaker play — instead of coin-flipping arbitrary decisions.
Constraints, each load-bearing:
- **Temperature 0 at hard/cheating = exact argmax = byte-identical to
  today** (the hard guard above must pass WITH the sampler in the tree).
- **Tiers are sacred**: sample within/below the normal tier only; the
  emergency tier's ordering is untouched (a grave-danger defender never
  loses a roll).
- **Determinism**: draw from `fc_rand` (the seeded server PRNG fuzzy
  already uses) so paired seeds stay paired.
- **Per-level temperatures are calibrated, not invented**: choose them so
  deep's level-to-level nerf profile lands in the same ballpark as
  stock's (the similarity check below is the measuring stick); register
  as placeholders like everything else.
- Stock's `ai_fuzzy` sites that deep inherits stay as they are (that IS
  stock-similarity); the sampler is deep's own-path replacement for
  fuzzy's role in build choice.

## Acceptance

- Hard byte-identity pasted (the corpus guard).
- Per-level table: ratio, t, wins/n for deep-vs-stock; the two nerf
  profiles side by side.
- Mechanism evidence at novice pasted (counters/log lines).
- `meson test -C build` passes; `tune-audit.py` CLEAN at its count.
- STATUS.md dated section as final commit; leave NEXT-SESSION.md alone.

## Scope fences

This is 1-2 epics of work; if the audit's gap list is long, land the
handicap-compliance epic first (with the hard guard) and the measurement
epic second — do not bundle past ~400 lines per commit. No changes to
`ai/default`/`ai/baseline`; `server/advisors` only under rule 4's inert
extension-point exception if a handicap must be threaded through a shared
path. No tuning campaigns.

## Stop conditions

The hard-identity guard fails; a level cannot both stay similar and beat
stock (report the tension with numbers — the human owns that trade-off);
a handicap's stock semantics require `ai/default` changes beyond
visibility.
