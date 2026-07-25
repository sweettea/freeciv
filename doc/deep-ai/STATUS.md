# Status

Update after **every epic**. Terse and factual. This is what makes the next
session work.

Format:
```
## W## — Name
- [x] E##.1 short description (N commits) — metric before -> after
- [ ] E##.2 short description
- Note: anything the next session needs to know
```

---

## Phase 0 — Foundation

## W00 — Baseline vendoring
- [x] E00.1 vendor ai/default + ai/classic as ai/baseline, register aitype
      (5 commits on `epic/e00.1-baseline-vendoring`, based at `0896b34`)
      — vendored sources are byte-identical to 0896b34; symbol collisions
      avoided by a force-included rename header (167 globals -> `bl_*`,
      generated via `nm`), so `diff -r ai/baseline/default ai/default`
      is empty. Registered as aitype `baseline` behind
      `-Daimodules=classic,tex,baseline`. Freeze guard: MANIFEST.sha256
      + meson test `baseline-frozen` (verified to catch a tamper).
      Smoke-tested: `create Foo baseline` works; 8-turn autogame with
      2 baseline + 4 classic AIs ran to the turn limit, exit 0, no
      assertions (needs `set timeout -1`, `set minp 0` headless —
      see scripts/test-autogame.serv).
- [x] E00.2 confirm `create Foo baseline` plays and saves/loads
      — found and fixed a real bug: every registered module's save hook
      runs for every player, and the vendored classic wrappers hardcode
      the live classic module's secfile prefixes ("ai" / ""), so mixed
      games wrote duplicate keys (player0.ai0.spam, u0.passenger) and
      the save was unloadable. Shim now overrides the six savegame
      hooks with a "baseline" prefix (commit c4f3ac2b38; MANIFEST
      regenerated pre-benchmark). Verified: mixed 2-baseline/4-classic
      game saves at T16, reloads with both aitypes intact, plays to
      T50; a save written by the fixed code (T48, 8 baseline-prefixed
      keys) round-trips and continues too. No assertions in any run.
      Note: the baseline-frozen meson test caught a stale manifest
      mid-fix — the guard works.

## W00 — Harness
- [x] E00.3 headless batch runner with paired seeds (2 commits)
      — benchmark/run-game.sh (one deterministic game: seeds pinned,
      per-game cwd, SCORELOG2 for all players, map hash from the turn-1
      autosave) + benchmark/run.sh (paired batch, parallel with unique
      ports, resume via DONE markers, automatic map-pairing check).
      configs/smoke.conf: 4p/32x32/hard/T100, ~40 s per game; --quick
      (10 seeds x 2 arms) ≈ 2 min at 12 jobs. Verified: 6/6 games,
      0 pairing violations; rerun of same (config,seed,arm) is
      byte-identical; classic vs baseline scorelogs byte-identical per
      seed — re-proving the vendored baseline behaviourally identical.
      Gotchas encoded in the scripts: parallel servers need distinct
      TCP ports; `save` in a --read script runs in pregame (no map), so
      the turn-1 autosave is the map-identity source; FREECIV_DATA_PATH
      must point at src/data when cwd is elsewhere.
- [x] E00.4 metric extraction + content-addressed result cache (3 commits)
      — extract.py: one CSV row per player per game with all 31 scorelog
      tags at last logged turn + alive + mean_city_size derived from
      citizen-class tags / cities (cross-checked against final-save city
      sizes: 5.2 both ways). run.sh results now live in
      results/<config>-<key12>/ keyed on sha256(config, server binary,
      tuning [env TUNING, "none" until W25]); DONE-marker resume doubles
      as the cache, key change → fresh directory, key.txt records
      components, metrics.csv written after every batch (works from
      cache too). Verified: fresh 4-game batch, full cache-hit rerun,
      TUNING change produced a separate keyed dir. Note: hashing the
      server binary over-invalidates on rebuild — deliberate
      (correctness over cache hits).
- [x] E00.5 regression corpus loader + first three positions (4 commits)
      — corpus-run.sh (load position, advance 1 turn, run assert.sh;
      runner asserts clean advance + correct post-turn) + savequery.py
      (turn, extra counts from hex layers, cities, captured, wars, min
      cross-owner distance). Positions checked in with PROVENANCE:
      hut-rich-start (64x64 T1, 63 huts), contested-border (6p T30,
      cross-owner cities 3.16 apart), losing-war (T140, 2 captured
      cities, 7 war pairs). All pass in ~5 s; a deliberately-broken
      assertion was shown to fail. Two harness bugs found and fixed
      along the way: (1) civ2civ3 sets mapsize=PLAYER so xsize/ysize
      were silently ignored in every earlier game — run-game.sh now
      forces mapsize XYSIZE; (2) run-game.sh wasn't part of the result
      cache key, so command-template edits could silently reuse stale
      results — it's hashed into the key now.
- Note: corpus positions currently play all players as classic (the
      aitype recorded at generation). When ai/deep exists, the loader
      will need a way to reassign a position's players to the arm under
      test — likely a small server patch or regenerated positions.
- [x] **HUMAN VERIFICATION GATE** — see workstreams/W00-harness.md §5.
      Checks executed 2026-07-24; evidence below. **Confirmed by human
      2026-07-24.**
      1. Determinism: PASS — two fresh --quick batches (20 games each),
         metrics.csv byte-identical (80 rows).
      2. Pairing: PASS — per-seed map hashes identical across arms for
         all 10 seeds (and the automatic check runs every batch).
         Bonus: classic vs baseline scorelogs byte-identical 10/10.
      3. Holdout isolation: PASS (caveat) — only holdout reference in
         the harness is run.sh's usage warning; tune.py doesn't exist
         yet. **Re-run this check when W25 lands.**
      4. Sensitivity: PASS, with a finding. founder_want/2 (the
         suggested regression) changed behaviour in 10/10 games (arms
         stopped being byte-identical, first divergence ~T44) but had
         NO measurable score effect at n=10/T100/smoke (mean +3.4,
         sd 28 → effect ≈ 0.1σ). founder_want/100 was caught loudly:
         score worse 10/10 seeds, paired-t −9.96; cities −24.0 mean,
         t −12.8. Two lessons recorded: (a) the byte-identity check is
         the most sensitive behaviour-change tripwire the harness has;
         (b) settler-want magnitude is score-neutral on smoke at T100 —
         W05/W02 need their proxy metrics, exactly as 00-context §8
         predicts. Regression reverted; tree verified clean (git diff
         empty, baseline-frozen Ok, classic==baseline byte-identical
         again on a fresh run).

## W00 — Module skeleton
- [x] E00.6 ai/deep forwarding all hooks to dai_* (2 commits)
      — deepai.c derived from classicai.c: every hook forwards to dai_*
      with deep's own ai_type; own savegame namespaces ("deep"/"deep_")
      per the E00.2 lesson. Registered behind -Daimodules=...,deep.
- [x] E00.7 wire player_save / player_load (1 commit)
      — struct deep_plr embeds struct ai_plr first (tex pattern);
      player_save/load (wired by no module before) persist deep state
      under player%d.deep_state, all keys defaulted on load.
- [x] E00.8 per-turn decision vector logging (1 commit)
      — one parseable deep-dv line per player per turn at LOG_VERBOSE
      (-d 4): turn/player/cities/units/gold/techs/science. Persisted
      dv_turns_logged counter proves the save path: 8 at a T8 save,
      resumes to 12 after reload.
- [x] **GATE**: deep vs baseline, 50 paired seeds: scorelogs
      byte-identical 50/50, aggregate score ratio 1.000000 (+0.000%) —
      well inside ±2%. Harness fix en route: a game ending early
      (seed 49: allied victory T58) was misclassified as failed; the
      GAMEOVER autosave is now the completion signal (run-game.sh),
      cache key rotated automatically.

## W25 — Tuning harness
- [x] E25.1 struct tuning + file loading (branch `epic/w25-tuning`,
      which starts by merging w06 + w09-lite so all 14 placeholders
      exist in one tree; main untouched pending review).
      deep_tuning_init() reads $FREECIV_DEEP_TUNING (registry format,
      [tuning] section, keys = struct field names); absent keys keep
      compiled defaults; loaded values logged as a deep-tuning line.
      run-game.sh forwards the harness TUNING file, whose content
      run.sh already hashes into the result-cache key. Verified end to
      end: a file zeroing explorer values drove deep-explore want to 0
      on a hut map and cm_war_shield=15 appeared in the log.
- [x] E25.2 optimiser with train/holdout split — benchmark/tune.py:
      dependency-free diagonal (mu,lambda) ES (no numpy/cma on this
      box; upgrade is mechanical, the evaluation plumbing is the hard
      part and is optimiser-agnostic), rank-weighted mean, sigma decay
      on stagnation, gen-0 includes exact defaults as control. Params
      + bounds in benchmark/tuning-params.ini. Every game runs through
      run-game.sh with a per-candidate tuning file: cached, resumable,
      spot-interruption-safe. REFUSES to optimise on holdout seeds
      (guard demonstrated); --report-holdout evaluates a fixed file.
      Demo (2 gens x 4 cands x 6 train seeds, smoke): machinery works;
      train best +6.5 collapsed to -5.2 on 4 holdout seeds — the
      overfitting-catch working as designed at toy n.
- [x] W00 gate follow-up "re-run holdout isolation when W25 lands":
      PASS — the only holdout references in the harness are run.sh's
      usage warning and tune.py's refusal guard/report path.
- **Campaigns deliberately deferred** (human decision 2026-07-25):
      want-denominated constants (explorer values, anything competing
      in build auctions) would be tuned against the current broken
      want scale that W01 exists to replace — throwaway work. Scale-
      independent constants (settler fearfulness, CM factors) hold
      value across W01 and are candidates for cheap micro-sweeps any
      time. First full campaign scheduled after W01 stabilises the
      want scale.

## Tier B — visibility and purity
- [ ] E-B.1 delete the unused is_req_active mutex (common/requirements.c:6756)
- [ ] E-B.2 export site evaluation internals from daisettler.h
- [ ] E-B.3 export base_want internals from daicity.h
- [ ] E-B.4 req_context government override
- [ ] E-B.5 city_virtual_copy
- [ ] E-B.6 per-player RNG, per-thread timing buckets

---

## Phase 1 — First demonstrable win

## W05 — Settler routing
- [x] implemented (11 commits on `epic/w05-settler-routing`, based on the
      W00 tip 81adeb1ced): risk-aware site search (get_TB refusing
      non-allied territory/unit tiles, adv_avoid_risks at tuned
      fearfulness, is_pos_dangerous wrapping adv_danger_at — assigned by
      nothing in stock), risk-aware execution leg replacing dai_gothere
      for founders, bodyguard requests sampling the whole planned route,
      loss instrumentation (deep-loss lines + server loss_reason
      extension point + benchmark/losses.py), contested.conf (8p/32x32/
      T150), corpus position hostile-corridor (4/4 corpus pass),
      --portbase for concurrent batches.
- [ ] **ACCEPTANCE PARTIALLY MET — stopped per protocol.** Contested,
      30 paired seeds, matched before (stock deep, pre-W05 binary
      rebuilt in a worktree; byte-reproduces stock: seed-1 scorelog
      identical) vs after:
      - hostile founder losses 107 -> 63 per 4530 turns
        (2.36 -> 1.39 /100t, **-41.1%**; target was >=60%).
        By reason: killed 97->60, bribed 10->3.
      - foundings 27.62 -> 26.95 /100t (-2.4%): no over-timidity
        collapse; smoke foundings 38.3 -> 39.4 /100t.
      - starvation deaths 51 -> 74: timid settlers sit in cities
        longer; the real cost of the change so far.
      - score: smoke deep/baseline 0.9832 (t=-0.98 n=30), contested
        1.0137 (t=+0.71 n=30) — both noise, no regression.
      Pushing 41% -> 60% means raising settler_fearfulness_mult (2.0,
      PLACEHOLDER, deeptuning.c) or similar — hand-tuning that rule 7
      reserves for W25 tune.py, which does not exist yet (W25 is
      unstarted Phase-0 work). Flagged; do a W25 pass then re-measure.
- Note: measurement integrity incident: the first contested before-run
      mixed binaries (run-game.sh resolves build/freeciv-server at game
      exec; a mid-batch rebuild swapped behaviour under it). Detected
      via the byte-identity tripwire (8/10 pairs identical = stock),
      redone properly with a pinned worktree binary. Lesson: do not
      rebuild while batches run, or pass --server explicitly.
- Note: remaining hostile losses look structural for routing alone:
      mobile attackers catch 1-move settlers regardless of planning;
      guard requests are made but stock guard fulfilment is weak
      (W11 territory).
- Note: contemplate_new_city() (production want) still uses the stock
      bare search; only actual settler units use the risk-aware one.

## W11 — Defence bookkeeping
- [x] implemented (7 commits on `epic/w11-defence-bookkeeping`, stacked
      on the W05 tip for the shared measurement harness — review W05
      first): deep turn drivers (first/last activities), per-city
      deep-def instrumentation + benchmark/defence.py, en-route
      defender credit (deep_incoming_defence_credit, ETA/survival
      discounts as deep_tuning placeholders), ranked two-pass emergency
      resolution in deep_manage_cities (lexicographic: grave danger,
      urgency, capital, worth — no invented weights), and
      deep_military_choose_build derived from stock so the credit is
      applied where production actually reads danger (the stock
      function re-runs assess_danger() internally — first after-run
      measured the phase-level credit as a no-op because of this).
      Corpus position twin-emergencies asserts the ordering invariant;
      5/5 corpus pass. Visibility-only exposure of daicity/daihand/
      daimilitary statics.
- [ ] **ACCEPTANCE NOT MET — mechanism-A premise is empirically hollow;
      stopped per protocol.** Contested, 30 paired seeds, matched
      before/after with pinned binaries:
      - surplus defenders per city-turn 0.4504 -> 0.4512 (target -40%);
        oscillation 2.383 -> 2.404 changes per city per 10 turns.
      - score: contested deep/baseline 1.0174 (t=+0.91 n=30), smoke
        0.9555 (t=-1.18 n=10; W11 paths never fire on smoke — deep
        games there are unchanged).
      - Root cause, instrumented directly: the state the credit counts
        (AIUNIT_DEFEND_HOME with a remote goto to an own city)
        essentially never exists — a full T100 smoke game showed 61
        DEFEND_HOME city-turn sightings, 51 with no goto at all, rest
        already arrived; a full T150 contested game produced ZERO
        credit hits. Stock cities do not dispatch defenders; they
        produce them locally and never disband them. The workstream's
        premise ("a defender two turns out contributes nothing") is
        code-true but dynamics-false in stock play.
- Note for the human — three options:
      (a) accept W11 as Problem-B-only (ranking works, proven by
          corpus; measurably inert on aggregate score so far);
      (b) redesign Problem A as defender dispatch + surplus disband
          policy (new scope, not in the workstream's design);
      (c) treat the credit as pre-positioned infrastructure for W13:
          the workstream's own "why before W13" note says campaigns
          drawing units away create exactly the en-route dynamic the
          credit needs — it may only become measurable then.
- Note: standing-surplus operationalisation of "defenders produced
      beyond assessed need" is recorded in defence.py's docstring; the
      40% target, like W05's 60%, predates any instrument and should be
      re-baselined (user discussion 2026-07-25).

## W28 — Worker stacking
- [x] core implemented (5 commits on `epic/w28-worker-stacking`,
      stacked on W11): struct workermap grows a transient join layer
      (extra_rate/extra_n; primary claim + displacement semantics
      untouched), auto_worker_setup_work records joiners instead of
      tripping the displacement asserts, and consider_worker_action
      gains a join mode valuing an occupied tile at the marginal
      present value of the completion turns this worker's rate saves —
      amortize(joined) - amortize(without), rate-weighted (Engineers
      2x), progress-aware via total_activity() (exported, visibility
      only), and worth nothing unless a whole turn is saved (the
      pile-on cap; no invented constants). Shared with human
      auto-workers per the approved rule-4 exception.
- [x] acceptance, measured (before = W11-tip pinned binary, after =
      W28, same-arm across binaries because advisors changes move the
      baseline arm too):
      - Stacks form: corpus position worker-stack (T55 smoke position
        with zero stacks; two workers pile on by T61, second player
        stacks T65; 6/6 corpus green). End-of-game snapshots show 2-3
        worker stacks organically.
      - Partial-turn joins declined: the whole-turn rule is the
        implemented cap (structural, not a tuned constant).
      - Score: contested deep/baseline 1.0245 (t=+1.31 n=30, best leg
        so far), smoke 0.9931 (t=-0.35 n=10, recovered from the
        W05-era -1.7% lean). No regression anywhere.
      - **Long transforms: STILL ZERO** — terraform.py counts 0
        terrain-changed tiles in all 40 before-games AND all 40
        after-games. Root cause is a bootstrap gap the workstream's
        design implies but its commit list defers: joining only
        amplifies work already started, and no FIRST worker ever
        starts a long transform because its solo evaluation is still
        amortized to ~1.7% of value (the workstream's own table).
        Claim-time valuation would need to anticipate prospective
        joiners — that is the deferred "plan tiles" concept (needs
        W02/W07 plan machinery, or a speculative idle-worker-count
        heuristic that would be an invented behaviour).
- Note: acceptance bullet 3 therefore unmet pending the bootstrap
      piece; bullets 1-2 met. Flagged rather than hacked around.
- Scoping notes from source reading (kept for the bootstrap follow-up):
      - struct workermap { int enroute; int eta; } at autoworkers.c:72;
        the hard skip is the `continue` at ~499; displacement compare
        at ~508-531; reservation written by auto_worker_setup_work.
      - get_turns_for_activity_at() = ceil(tile_activity_time / my
        rate); marginal stacking math needs tile_activity_time,
        total_activity() progress (server/unittools.c) and
        get_activity_rate() per unit — all reachable.
      - consider_worker_action() owns the compare bookkeeping (8 out
        params) and its own discounting; a stacked candidate needs
        marginal value = amortize(v, t_with_me) - amortize(v,
        t_without_me), so add a sibling compare helper rather than
        wedging a scaled base_value through the existing one.
      - "Cap by danger" is already half-true: adv_worker_safe_tile()
        at ~485 refuses dangerous tiles for ANY assignment.
      - "Prefer stacking on plan tiles" is unimplementable until a
        plan concept exists (W02/W07) — defer that commit.
      - Array capacity for multi-assignment is structural (precedent:
        MAX_DEP_ROADS 5 in the same file); the behavioural cap must
        come from the whole-turn-saving rule, not a tuned constant.
      - MEASUREMENT WARNING: advisors are shared with ai/baseline, so
        a W28 change moves the baseline arm too. Cross-arm comparison
        no longer isolates it; use same-arm across pinned binaries
        (the W05/W11 pattern) and keep within-binary deep-vs-baseline
        only as a score-regression guard.

## W06 — Exploration
- [x] implemented (4 commits on `epic/w06-exploration`, stacked on
      W28): deep_explore_want() (visible unclaimed huts + unexplored
      area, per-unit values as deep_tuning placeholders, diminishing
      returns per explorer, zero-gated on hutless maps) competing for
      the city build choice in deep_manage_cities();
      deep_spend_gold() derived from stock with the
      disband-explorers-at-home hack gated on want==0; huts/hutfree
      benchmark configs (run-game.sh HUTS support) + hutstats.py
      (techs@T50, huts remaining, explored fraction from the [map]
      known layers, max want).
- [x] acceptance, measured (10 seeds/config, before = W28-tip pinned,
      after = W06):
      - **Hut-free requirement: MET exactly** — zero nonzero want
        lines across all hut-free games AND deep scorelogs
        byte-identical before/after.
      - Smoke score guard: deep/baseline 1.0088 (t=+0.40 n=10) — fine.
      - **Hut-heavy metrics: unchanged** (techs@T50 6.00 -> 6.00, huts
        left 4.8 -> 4.8, before/after byte-identical): the want fires
        every turn (logged 39..94) but never wins a build auction
        against early settler/military wants at placeholder
        magnitudes. Raising them to force a win is exactly the
        hand-tuning rule 7 forbids — third epic now gated on W25.
      - Finding: under fog the huts_visible term is structurally ~0
        (an explorer that sees a hut pops it), so the discovery term
        must carry the want; fold hut density into it at tuning time
        (the workstream's design point 4 says as much).
      - Explorer-disband gate is live but unobserved in these games
        (stock's hack never fired on these seeds either — explorers
        stay out exploring the whole T80 horizon).

## W09-lite — City manager parameters
- [x] implemented (3 commits on `epic/w09-lite-city-manager`, forked
      from the W28 tip in parallel with W06 — needs a trivial rebase
      after w06 merges): deep_set_city_manager_params() fills
      pcity->cm_parameter each phase from city state (shields under
      urgency/grave danger, shields+trade at the growth cap and under
      wonder builds, happy factor above zero in every matched rule;
      unmatched cities get the parameter cleared = stock default).
      auto_arrange_workers() honours the parameter, and NOTHING in the
      stock AI ever set it (the cityturn.c comment says priorities
      should someday flow in this way). All factors are deep_tuning
      placeholders. Second commit rearranges citizens on rule
      transitions after the first after-run measured a completely flat
      metric: setting the parameter without rearranging leaves old
      citizen layouts running for many turns.
- [x] acceptance, measured (contested, 20 seeds, before = instr-only
      pinned binary, after = v2 with transition rearrange):
      - urgency-city shield output 4.773 -> 4.934 per city-turn
        (**+3.4%**; target >= +15%) — mechanism verified live (was
        +0.0% before the rearrange fix) but magnitude gated on W25
        factor tuning, same as W05/W06.
      - starvation guard: PASSED with margin — starved-unit deaths
        544 -> 435 (-20%); food-deficit city-turns flat (4287 -> 4367).
      - all-city shields +2.2%.
      - score: contested deep/baseline 1.0110 (t=+0.40 n=20), smoke
        0.9836 (t=-0.51 n=10) — neutral.
- Note: fourth epic gated on W25 tuning; the headline-number hope in
      the workstream likely needs tuned factors + the wonder/cap rules
      firing more often in longer games.

---

## Phase 2

## W01 — Want unification
- [x] workstream file extracted from PRD-v3 §4.3 + findings, evidence
      re-verified at 0896b34 (two stale-plan notes: the -199 boat
      sentinel evolved into sign-overloaded founder_want, and
      adv_choice already carries the explicit need_boat field the PRD
      asked for).
- [x] first landing (4 commits on `epic/w01-want-unification`, based
      on the w25 integration tip): want_t + explicit priority tiers
      (deepwant.[ch]), exact boundary conversions (the 1000-scale
      emergency magnitude becomes a tier field), deep_city_choose_build
      derived from stock with tier-gated merging through
      deep_want_cmp() (provably the same ordering as
      adv_better_choice), emergency magnitudes in
      deep_military_choose_build migrated to the API with exact
      round-trips, and a deep-want calibration line per final build
      decision (4,345 lines in one contested game).
- [x] gates: **byte-identity 20/20** (10 smoke + 10 contested paired
      seeds, pre-W01 vs W01 deep, pinned binaries) — the layer is
      empirically exact, far stronger than the PRD's own ±2% gate.
      Grep invariant: DAI_WANT_* magnitudes appear only in the
      conversion table (deepwant.c).
- [x] calibration corpus recorded (10 contested games, 45k decisions).
      Native want scale per producer — the disease, quantified:
        buildings (delta-measured)  median 10-48
        Workers                     median 47
        Phalanx (defender %)        median 155, p90 1224
        Settlers (trait-rigged)     median 311, p90 473
        Diplomat                    median 1101, p90 16000
        Trireme (ferry wants)       max 63505
        Barracks                    max 14673
      9% of decisions ride the emergency tier. Buildings vs settlers
      alone is a 10-20x scale gap; diplomats/ferries are a different
      currency entirely. W06's explorer want (39-94) never stood a
      chance against 311-median settlers.
- [ ] next landings (calibration-driven, each measured separately):
      real shield-equivalent conversions per producer, starting where
      the distortion is largest (diplomat, ferry, settler); the
      explorer re-pricing (W06's auction) likely becomes winnable as a
      side effect of deflating those, not by inflating itself. Feeds
      directly into W02.

---

## Later phases

See `PRD-v3.md`. Extract workstream files as you reach them.

---

## Open questions for the human

(Agent: append here rather than guessing.)

- 2026-07-24: PRD-v3.md carried pre-revision placement/phasing directives that
  contradicted CLAUDE.md and findings/placement-revision.md; amended with a
  supersession banner, corrected baseline date (0896b34 is Jul 2026, not Apr),
  and `ai/deep` placement tags on W2/W3/W5/W6/W10/W11/W27. Review welcome.
- 2026-07-24: work is on branch `epic/e00.1-baseline-vendoring`, based at
  `0896b34` (the declared baseline), which is 21 commits AHEAD of local
  `main` (691fe4de54). ai/ + server/advisors/ are identical between the two.
  Suggest fast-forwarding main to 0896b34 or continuing on epic branches.
- The planning docs (CLAUDE.md, doc/deep-ai/, benchmark/, run-phase.sh) are
  untracked; commit them when ready. `freeciv-deep-ai/` is an unused duplicate
  of the starter kit — delete or keep as reference?
- Autotools (ai/Makefile.am) was NOT taught about ai/baseline; the project
  builds with meson per CLAUDE.md. Flagging in case autotools parity matters.

---

## Agreed roadmap (human, 2026-07-25)

1. Finish W09-lite measurement; hand the five Phase-1 branches over for
   review (linear stack e00.1 -> w05 -> w11 -> w28 -> w06; w09-lite
   forks from the w28 tip and needs a trivial rebase after w06 lands).
2. Fast-forward main to 0896b34, then merge the stack after review.
3. Parallel track: minimal-effort upstream fixes as clean patches on a
   branch off 0896b34 (dead long_path branch, dead is_req_active
   mutex, README.AI rot, ...) intended for upstream submission — NEVER
   merged into this measurement line; ai/baseline stays frozen across
   any future rebase.
4. Next epic after merge: W25 tuning harness — three Phase-1 epics are
   implemented-but-inert on ~15 deep_tuning placeholders; one training
   pass + holdout report converts them and re-baselines the plan-time
   acceptance percentages.
5. Then Tier B purity items, then Phase 2 (W13 campaigns also create
   the dynamic W11's credit is pre-positioned to count).
