# The venue portfolio — what v4 should tune ON, and why that IS the objective

2026-08-06, orchestrator (Fable), at the human's direction ("deep think
about what scenarios are right to do tuning on... maybe some are
head-to-head against classic"). Companion to
`tuning-first-principles-2026-08-06.md` (centers) and CAMPAIGN-v4 §3
(blocks/pins). This doc proposes the CELL MATRIX; the human signs it off.

## 1. Venue choice is the objective function — three proofs from our own history

1. **a6 disarmament.** Evolution on peaceful cells zeroed every fear and
   military knob. Correct for the venue, fatal anywhere with teeth
   (derby 2: −107 cities to the storm).
2. **The 35.9 artifact.** `site_rival_pressure` inflated freely because
   two of three cells were islands where the term never fires — the
   optimizer parked garbage in a knob the venue couldn't see.
3. **Culture inversion.** The culture credit measures −19 at T150 and is
   the plurality of score at T600. Short venues actively PUNISH the
   long-game mechanisms the project just spent a week building.

A knob's tuned value inherits its venue. The portfolio must make every
block's failure VISIBLE somewhere, or the optimizer will hide it there.

## 2. The finding: our training suite has zero head-to-head games

Every cell runs ONE aitype per game (an arm = a homogeneous world;
"beating stock" = higher self-play score on paired maps). Consequences:

- Deep's anti-STOCK statecraft has no training population at all.
  BALANCE's gifts and proxy arming are priced against stock's own
  goldequiv acceptance code; PEACE/TRADE tune their love mechanics
  against deep's own mirror instead of the opponent that matters;
  `leader_check_gain` biases toward a runaway leader that is always
  deep itself.
- Homogeneous self-play also can't see exploitation-resistance: a
  candidate that farms its own mirror's courtesies would farm stock's
  too — or be farmed — and we never observe either.
- The derbies (mixed 7v7 Europe) are exactly this regime, and they
  produced findings (fratricide, storm, war-target quality) that no
  homogeneous cell has ever shown. They are currently INFORMAL — one
  map, one seed, no pairing.

**Proposal: formalize mixed-table cells.** Mechanically cheap: a config
whose EXTRA_SERV issues derby-style `create <name> deep|classic` for a
half/half table on a generated map; ONE arm per game; the metric is the
in-game team score difference (extract.py already parses per-player
score; it needs a team-diff column). Seeds pair as always. Two cells:

- `mixed-8p-150` — 4 deep vs 4 stock, contested-like 40x40, civ2civ3,
  T150. The statecraft block's HOME venue (gifts, proxy, peace, trade
  with stock all fire).
- `mixed-8p-classic` — same table, classic, for the caravan/trade
  family and as the derby's laboratory twin.

Head-to-head is NOT a replacement for paired self-play — a mixed game's
score diff confounds "deep played well" with "stock collapsed" — it is
the complement: self-play cells isolate quality, mixed cells measure
competition. Both, always.

## 3. The axes the portfolio must span (and where it currently doesn't)

| axis | covered today | gap |
|---|---|---|
| map: island/continental/crowded/roomy | isolated, contested, cramped, primary, garden2b | good |
| ruleset | most cells ruleset-split | good |
| horizon | 80/120/150/250/300 | nothing past 300; culture/endgame/wonder mechanisms peak later. Cloud harness has no 10-min limit — add ONE long cell |
| opponents | self-play only | §2: mixed cells |
| barbarians | onsetbarbs 500 (garden2b = OFF), default elsewhere | **no raging cell anywhere** — the storm regime that destroyed derby-2 deep is untested |
| player count | 2-8 (+scale-20p probes) | good |
| start symmetry | generated fair starts | scenario asymmetry (Europe) — keep as EVAL, never train (§5) |

New cells proposed:

- `storm-150`: classic, 8p, 40x40 continental, `barbarianrate` raging,
  `onsetbarbs 30`, T150. Exercises: G1 floors, GROUND's terror reshape,
  reconquest of empty barb cities, consolidation-vs-sprawl. The MIL
  block sweeps HERE or its optimum is a6's disarmament again (recorded
  rule: never sweep MIL on a peaceful cell).
- `longhaul-300`: civ2civ3, 6p, fractal, T300. The culture credit,
  endgame window, wonder race, and trade compounding get a venue whose
  horizon matches their payoff. (Cloud only; the local 10-minute
  foreground block is a local-harness constraint, not a game one.)

## 4. Aggregation: how 10+ cells become one verdict without averaging away a failure

- Per cell: paired score diff (self-play cells) or team diff (mixed
  cells), z-normalized by that cell's baseline spread, so a 40x40 T150
  cell and a 2p island don't trade at absurd exchange rates.
- Objective = mean z **subject to a floor**: no cell below z = −0.5 vs
  current defaults. A candidate that buys +3 somewhere by selling one
  venue's viability is REJECTED, not averaged. (This is the a6 lesson
  as arithmetic.)
- Report per-cell always; "campaign best" without the per-cell table is
  not a result (rule 6's spirit at portfolio scale).

## 5. Holdout VENUES, not just holdout seeds

The project holds out seeds; generalization needs held-out *worlds*:

- **Europe scenario 7v7** (both cohort layouts) — the standing eval
  derby, never tuned on. Asymmetric starts make it noisy training but
  honest evaluation: it answers "does this transfer to a real map".
- **`wildcard`**: per-seed randomized generator/landmass/players/
  barbarian rate drawn from a fixed distribution — 20 seeds of "worlds
  nobody optimized for". Reported, never swept.

Final campaign numbers quote: training cells (tuned), then holdout
seeds, then holdout venues — three concentric claims of generality.

## 6. What this does NOT propose

- Candidate-vs-incumbent head-to-head (two tuning files in one game) is
  IMPOSSIBLE today: the tuning struct is process-global. That plumbing
  is the difficulty epic's per-player jitter work; if it lands early,
  incumbent-vs-candidate mixed cells become the strongest selection
  signal available and this doc should be revisited.
- Difficulty-level venues (vs easy/novice stock) belong to the
  difficulty epic, not the quality campaign.
- No tournament/Elo machinery: two mixed cells + the floor rule buy most
  of the value at a fraction of the complexity.

## 6a. Per-player tuning: not for v4 (human question 2026-08-06, answered)

Asked directly: build per-deep ini values first? **No — complication
without a v4 win.** The struct is process-global behind
`deep_tuning_get()` at hundreds of call sites; per-player needs either a
threaded parameter or a phase-scoped active-pointer switch at every
vtable entry, whose failure mode (a silent wrong-player read) is exactly
the kind of bug that corrupts a campaign invisibly. And v4 never needs
it: mixed cells run the whole deep cohort on ONE candidate file against
stock — the yardstick the project's goal names. The two things
per-player tuning genuinely buys — candidate-vs-incumbent selection at
one table, and per-seat difficulty jitter — are both post-v4 by design
(and incumbent-selection brings its own pathologies: intransitivity,
exploiting the incumbent instead of being good). Build it once, in the
difficulty epic, when it is the point rather than a prerequisite.

## 6b. Cost of more venues (same question)

Portfolio cost does NOT multiply the inner loop. Blocks sweep only their
2-3 home cells (§3 pins; the campaign's existing rule): ECON's inner
loop is unchanged, MIL adds storm to contested, statecraft adds the two
mixed cells it currently lacks entirely. The full 10-cell matrix runs at
STAGE BOUNDARIES (the z-floor check) and at claim time — a handful of
full evaluations per campaign, not per candidate. Net inner-loop growth
~30-50% on the two military-ish blocks, zero on ECON; longhaul-300 is
wall-clock-heavy but sweeps only the culture/endgame family. The
expensive thing was never venues — it is candidates × games; venue
pinning is what keeps that product sane, and it already does.

## 6c. The objective (human question 2026-08-06: "optimize score, or ???")

**Optimize paired score diff; CLAIM on a dashboard.** Reasoning:

- Score is the right GRADIENT: dense (every game yields signal), smooth
  under small knob moves, the game's own composite of everything we
  care about, and comparable across cells once z-normalized. Every
  alternative examined is sparse or gameable as a target: win rate is
  ~0/150-turn-cell (199/199 games end on the bell — nothing to
  optimize); area-under-curve score rewards rushes and punishes exactly
  the long-game mechanisms this week built; kill counts invite farming.
- But score alone is a bad PROOF: endturn score cannot see "was about
  to win", sprawl inflates it until a storm calls the loan (derby 2),
  and a single number hides venue sacrifice. So the success claim is
  four lines, all already cheap to produce:
  1. mean z of paired score diff across canonical cells (the number);
  2. the z-floor: no cell below −0.5 (no venue sold);
  3. decisive events where venues permit them — eliminations, cities
     lost to barbarians (storm), team wipeouts (mixed) — REPORTED with
     signs, not optimized;
  4. holdout seeds and holdout venues (Europe, wildcard) quoted
     separately.
- One deliberate asymmetry: on mixed cells the optimizer sees team
  score diff only; eliminations stay report-only, because making
  "kill stock" a target invites the degenerate all-in rush that a
  T150 bell would never punish.

## 6d. Seeds: the anti-overfit protocol (human worry 2026-08-06, sized from our own data)

The failure has two faces: optimizing ONE seed's quirks (pure noise
chasing) and optimizing a FIXED PANEL's quirks (the subtler one — CRN
pairing makes comparisons sharp, and then the lineage learns those ten
maps). Both are handled structurally, not by hoping n is big enough:

1. **Per-cell n, from measured σ.** The paired-diff spread is not one
   number: contested measures σ_d ≈ 8 score (D3: se 1.87 at 20 seeds);
   isolated-150 measures σ_d ≈ 16-36 depending on arm (DARKFX: se 3.5
   and 8.0 at 20). Detecting an effect E at 2se wants n ≈ (2σ_d/E)².
   So +3 score on contested needs ~30 seeds — and the SAME +3 on
   isolated needs 100+. Inner-loop n is set per cell from the archived
   σ_d table, not globally; a cell too noisy to afford is a cell that
   only sees large effects, and the protocol says so out loud.
2. **Racing, not flat n.** Triage every candidate at n=10 (only effects
   ≳ 2σ_d/√10 survive — coarse but cheap); escalate survivors to 20;
   claims and stage winners at 40+. Most of the seed budget lands on
   the few candidates where it changes the answer.
3. **Panel rotation per generation.** Each generation draws its panel
   fresh from the ~100-seed training pool: within a generation,
   candidates compare CRN-paired on identical maps; across generations
   the panel moves, so panel-quirk fitness is not heritable. Elites
   carried forward are RE-evaluated on the new panel — persistent luck
   dies in two generations, real quality survives rotation.
4. **The winner's curse is priced.** The selected best is best partly
   by luck, so its training-panel number is biased upward. The claimed
   improvement is the HOLDOUT number (seeds never in the training
   pool), full n, quoted with its CI — and the holdout VENUES (§5) sit
   above even that.
5. **One-seed diagnostics stay one-seed.** Derbies and forensics
   generate hypotheses; nothing tuned moves on their evidence alone
   (rule 6 already says this; restated here because a storm this vivid
   tempts).

## 7. Cost sketch (cloud, per full sweep pass)

Core self-play: lategame-80, isolated-150, primary-120, contested,
garden2b (+ classic twins where the block needs them) — the current
regime, unchanged. Adds: 2 mixed + storm-150 + longhaul-300 ≈ +40% game
volume, with longhaul the dominant wall-clock (T300 ≈ 2× a T150 game).
The floor rule costs nothing extra — it reuses the same runs. Harness
work before freeze: team-diff column in extract.py, mixed-cell configs,
storm/longhaul configs (~1 session-morning, can ride INSTR).
