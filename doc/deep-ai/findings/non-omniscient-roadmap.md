# The road to a non-omniscient deep AI

2026-08-05. Analysis only — building any of this is outside the current arc
(human call 2026-08-04: the flag and its breakage census happen now; the
inference layer does not). Companion to `findings/honest-information.md`
(the original stock-AI analysis and knowledge-layer design, which this
document corrects and extends) and to
`findings/deep-dive-2026-08-04/omniscience-census.md` (the per-site census of
every ground-truth read in `ai/deep`, on which every claim here rests).
00-context RC6 is the motivation: **you cannot evaluate a strategy layer
that is being fed the answers.**

---

## 1. What changed since honest-information.md was written

That document analysed stock: ~40 tactical omniscience gates, a strategic
tier (`dai_war_desire`, `daitech.c`) with none, and a server-maintained
remembered world (`player_tile` / `vision_site`) that no AI has ever read.
All still true. But deep has since grown to ~15k lines, and the census
establishes four things the original could not have known:

1. **Deep never reads remembered state either.** Zero calls to
   `map_get_player_tile` / `map_get_player_city` anywhere in `ai/deep`.
   Deep reproduced stock's Tier-2 pattern: ground truth everywhere, ~18
   handicap gates layered on top.
2. **Deep built a second strategic consumer of exact enemy state, bigger
   than the first.** The survival/take-odds economy (`deepcombat.c`,
   `deepodds.c`) reads exact enemy unit lists and garrisons and feeds ~20
   decisions — fear injection, CM states, growth gating, sortie/relay/lend
   gates, op formation, march re-gates, abort floors, poach arbitration.
   De-omniscing deep is mostly de-omniscing this one input family.
3. **The failure direction inverts.** Stock under fog goes *passive* (skips
   unknown targets). Deep under fog goes *actively wrong in the confident
   direction*: an unseen wave reads as `wave == 0` → survival 1.0 → fears
   satisfied → the melt disbands garrisons; an unseen garrison reads as
   empty → `P(take) ≈ 1` → assaults form everywhere. **Absence of evidence
   becomes confidence.** Every consumer of the survival model inherits
   this.
4. **One channel has no honest analogue at all**: deep prices assault
   targets by the *enemy's own private self-valuation*
   (`def_ai_city_data(acity)->worth`, filled by the owner's `dai_city_want`
   run). No fog model fixes that — it needs a deep-side city-value estimator
   over remembered state. (Side effect worth knowing today: for human-owned
   cities that slot is never refreshed, so targeting quality already differs
   silently against humans.)

Also corrected from the original: its §4 "Danger" row conflates the
seen-gated `adv_danger_at` with the un-gated city-danger path deep actually
inherits (`assess_danger` reads all enemy units, only its *pathing* honours
H_MAP); its §4 "Settling" row is stale (deep has an explorer want, pre-wired
to go live under fog); and its §7 "save nothing, rebuild on load" rule needs
a carve-out — **the server's remembered world contains no units**, so a
sighting memory (`known_force`) is new state that must be persisted (deep
already has the save namespace and the ops-persistence precedent).

## 2. The good news, verified

A surprising fraction of deep is **already fog-shaped**:

- The whole anticipatory-fear lattice prices threat from lobby settings,
  contact state, crowding, and onset gates — legitimate knowledge. It
  survives fog unchanged; it was *designed* as insurance against what you
  cannot see.
- The catchment-stream site simulator, horizon credits, CM state machine,
  stance gates, want tiers, treasury, and the ops *state machine* (muster/
  march/strike/consolidate, claims, banking) are own-state — fog-clean by
  construction.
- `deep_free_land_share` has a documented honest fallback; the explorer
  want's discovery term is explicitly written to "return in full the moment
  W04 makes deep honest"; the eta-reinforcement projection in
  `deep_take_odds_at_arrival` *is already the staleness estimator* the
  knowledge layer needs — applied today only over travel time, it extends
  to `eta + intel_age` mechanically.

The expensive list is short and sharp: the survival census source, the
march-re-gate surveillance assumption, `deep_raid_victim` (hunts fogged
civilians by exact position — the purest omniscience site in deep), the
inherited `dai_war_desire`, the enemy-`worth` channel, and two global
capability censuses (boat-shadow, garrison menace — the latter's "public
score" claim is false: the historian broadcasts rank+name only).

## 3. The staged path

Each stage is measurable on its own; order is load-bearing (census §4).

**Stage 0 — the flag and the census run** *(briefed: S-omniscience-flag.md;
in the current arc)*. One tuning-file switch setting H_MAP/H_FOG/H_TARGETS
for deep seats; OFF byte-identical; ON run in two cells recording what
breaks. **Prediction to test, from the census**: not paralysis but active
self-harm — garrisons melting under invisible waves, assaults forming
against invisible garrisons, expansion halting at the unknown-tile-zero
site gate. The failure list is the requirements document.

**Stage 1 — priors over the unknown, in site evaluation.** Without it,
honest deep stops expanding on turn ~1 (`deepsite.c:590` scores unknown
tiles zero). `E[tile]` from generator settings (landpercent, hut density,
crowding) — the machinery pattern already exists in `deep_map_crowding` /
`deep_density_prior`, which read exactly these lobby inputs. Must land
with, or before, any real honest play. The explorer discovery term goes
live automatically and W06's want becomes load-bearing, as designed.

**Stage 2 — the mechanical substitutions.** Category-(b) items: remembered
terrain/owner/cities from `player_tile`/`vision_site` in site evaluation,
density fear, contest pressure, embassy reachability. Cheap, each one a
small diff; the remembered-city scan replaces iterating the rival's real
city list nearly losslessly (city positions are stable).

**Stage 3 — `known_city` and the honest city-value estimator.** A per-phase
build from `map_get_player_city()`: size, walls, occupied, improvements
bitvector, `last_updated` — enough to price a target *from our own side*,
replacing the enemy-`worth` read (and fixing the human-opponent asymmetry
as a side effect). Staleness projection per honest-information §5.2: size
grows at an estimated rate since last seen; defence assumes the best
defender their known tech allows.

**Stage 4 — `known_force`, the persisted sighting memory.** The largest
prerequisite: last-seen (type, position, hp) per enemy unit with position
confidence decaying by age × move rate. NEW deep state, saved (carve-out to
the recompute rule). Feeds the survival core, overseas danger, worker-flee
threat memory (the "walks into the same ambush every turn" fix), and the
observed-transport bits that replace the two global capability censuses.
The survival model's *interface* survives; only its census source changes —
and the anticipatory lattice already covers the unseen mass, which is why
the inversion problem (§1.3) is fixable at all.

**Stage 5 — staleness-tolerant operations + recon as a want.** Extend the
reinforcement window to `eta + intel_age`; the existing abort-floor
hysteresis absorbs estimation noise. Then the op formation bar gains an
intel-age precondition ("scout before you commit"), the raid op doubles as
the recon executor (an honest raid IS a patrol), and a recon want priced by
`intel_staleness × planned_action_value / distance` slots into the
explore-want template. This is where the AI starts *choosing to buy
information* — the behaviour honest-information §5.3 called the most
human-feeling an AI can have.

**Stage 6 — honest strategy.** `dai_war_desire` rebuilt over
known_city/known_force with an uncertainty penalty (scale want down, fear
up, by intel age); embassies become the cheapest intel purchase and
CLAUSE_EMBASSY refusal a real defensive move; menace from historian rank
buckets. This is where W34 phase B (the victory plan) becomes measurable —
the hard ordering W04-before-W34B in 00-context §7 is this stage.

## 4. Difficulty interplay

Unchanged from honest-information §6, now with a concrete hook: **every
level gets honest information; difficulty scales competence** — and the
census adds *staleness tolerance* and *recon-before-commit cadence* as
natural competence knobs that already have homes (the op formation bar, the
projection window). The current arc's difficulty ladder (S6/S7) is built on
the omniscient AI by explicit scope decision; when the honest AI exists,
those knobs join the ladder rather than replacing it.

## 5. Measurement strategy — making "honest and still strong" falsifiable

- **The A/B is the flag itself**: same binary, tuning-file switch, cached
  arms — the harness needs nothing new.
- **Cells**: honest-vs-honest self-play (both deep arms flagged) measures
  the strategy layer without the answer feed; honest-deep-vs-omniscient-
  stock measures the fairness gap we are buying back; the existing
  omniscient cells remain the regression guard.
- **Metrics before score**: expansion rate at the unknown frontier (stage 1
  working), garrison stability under unseen pressure (stage 4 working —
  the melt no longer disarms), assault abort rate and intel age at
  formation (stage 5), and the census counters themselves.
- **The RC6 claim is only discharged at stage 6**: when a war decision
  demonstrably made on estimated inputs outperforms stock's perfect-census
  decision on score, the strategy layer is proven to be strategy, not
  plumbing for answers. That is the falsifiable form of "make it honest
  early or the whole project is unfalsifiable."
- **Two comment fixes are owed now** regardless of any of this
  (mechanism-map D-H): `deepcombat.c:182` and `deepdefence.c:736-743`
  claim honesty the code does not have; they should say what is true.

## 6. Cost sketch

Stages 1–3 are small (priors: one function + tunables; substitutions:
mechanical; known_city: one per-phase scan of server-maintained state).
Stage 4 is the real build (new persisted state + the survival-census swap
+ its 20 consumers re-verified). Stage 5 is medium and mostly re-uses the
ops layer. Stage 6 is W34 phase B plus exports from `daidiplomacy.c` (which
has zero visibility edits today — the seam map notes the decisions there
are all `static`). Everything is O(known things), not O(map), per
honest-information §7 — and the op-level target hoisting already delivered
part of the promised compute savings.
