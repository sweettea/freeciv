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
- [x] first real conversion: the defensive diplomat. Stock writes a
      self-described 'totally arbitrary' 16000 (daidiplomat.c:112,134)
      that tier-jumps even military emergencies; deep evaluates it on
      a fresh choice and merges it as emergency tier with a calibrated
      value (diplomat_defence_value 300, corpus-derived). Measured
      (20 contested + 10 smoke paired seeds, pinned binaries): the
      16000 population is gone (diplomat native max 16000 -> 5223 =
      the offensive diplomat's honest expected-value wants, untouched
      by design), pick rates stable (~296 -> 314 per seed), score dead
      neutral (contested 1.0004 t=+0.02, smoke 0.9842 t=-0.49).
- **Conversion-gating map** (why W01 stops here; each remaining
      conversion lands WITH the workstream that fixes its counterpart
      scale):
      - founder (median 311 vs anchor ~17): gated on W03 — the
        settler-vs-building rigging partly compensates the missing
        building horizon; deflating onto today's undervalued anchor
        would collapse expansion.
      - attacker spikes (Trireme max 63505 = kill_something_with
        wants): gated on W10 combat model.
      - explorer re-pricing (W06 auction): follows founder deflation,
        not self-inflation.
      - workers/buildings/caravans: near-anchor already; identity.

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

---

## Cloud handoff state (2026-07-25, pre-AWS)

Branch `epic/w03-horizon` carries W02+W03+W07-lite in flight:
- W02 v1 regressed (contested 0.879 t=-4.75: catchment MEAN replaced
  the best-other anchor and deflated every founder want; diagnosis in
  the anchor-fix commit). v3 restored the stock anchor + additive
  depth term; +proximity overlap weight, +claim bonus, +pressure-
  compressed potential (review directions). Score at v3 (pre-
  proximity/claim): contested 0.948 t=-2.02. ~~DECOMPOSED by the
  contested ablation just before handoff: site terms alone are
  NEUTRAL (0.9972 t=-0.07); the gap was the W03 horizon credits~~
  **RETRACTED 2026-07-26** — see "Tuning loader gap" below: the
  ablation inis were silently ignored (deep arms byte-identical);
  the 0.997-vs-0.948 spread was missing baseline rows in a partial
  metrics.csv. The urgency gate (no credits for threatened cities)
  remains in the code as a plausible war-map rule but its evidential
  basis is void; the campaign's post-fix no-horizon_* ablations are
  now the real test. Credit magnitudes go to the campaign.
- W03 credits (capacity/growth/bundle) landed; measurement confounded
  with W02 so far. Supermarket/Sewer acceptance needs long.conf
  (late techs unreachable by T150) — never yet run.
- PRIMARY REGIME (human decision): 400 land tiles/player, FRACTAL,
  T300 (primary.conf, TILESPERPLAYER runner support). All headline
  judgments happen there; contested/isolated/smoke are probes.
- Tuning deliberately deferred to the cloud campaign: ~24 deep_tuning
  placeholders; campaign.sh stages gates -> per-weight ablations
  (contested+primary) -> tune-A (scale-independent, contested) ->
  tune-B (site/horizon, primary+isolated blend) -> holdout -> long +
  primary acceptance -> report.md. See benchmark/AWS.md.
- Analysis niceties for the box: losses.py defence.py hutstats.py
  firstbuild.py terraform.py; DEEP_QUIET=1 for campaign runs.

---

## W29 — Transform chains
- [x] implemented (7 commits on `epic/w29-transform-chains`, atop
      w03-horizon): chain walk API (advtools, cycle-safe),
      adv_calc_transform valuing the best reachable terrain WITH its
      prospective improvement (the mine trap: bare-destination vs
      improved-current comparison lost by construction — measured as
      a byte-identical no-op before the fix), by-origin and
      from->to-pair terraform metrics, garden/garden-classic/cramped
      measurement regimes (solo scarce island = pure development
      comparison; EXTRA_SERV runner support).
- [x] **headline finding: upstream bug 28** — the AI (and human
      auto-workers) NEVER cultivated or planted in ANY ruleset:
      valuations cached under legacy ACTIVITY_MINE/IRRIGATE keys,
      consumed under ACTIVITY_CULTIVATE/PLANT (preset -1, gate always
      false). Found by gate decomposition: 418k transform candidates,
      prob=0 for all incl. 75k Engineer checks -> which exposed that
      civ2civ3 locks Transform behind FUSION POWER (endgame, by
      design; the workstream premise 'transforms with Engineers' is
      classic-rules thinking — stale-plan note), and cultivate was
      dead separately. Fix: store under the consumed keys.
- [x] measured (garden solo T300 regime + guards, pinned binaries):
      - civ2civ3 + key fix: terrain changes **0 -> 13.2/game**
        (f>g x116 of 132 — forest-clearing, the move humans make
        constantly), score 0.9927 t=-0.10 (neutral). Smoke guard
        0.9763 t=-0.47 (5.5 changes/game); contested guard 0.9498
        t=-1.68 with 25.9 changes/game — watch-item lean, within this
        config's recent band; per 00-context §8 recorded, not
        counter-tuned.
      - Chain valuation itself: dormant in civ2civ3 (Fusion-gated;
        correct but unreachable until endgame-scale games — same
        pre-positioned status as W11's credit awaiting W13) and
        redundant in classic (stock already transforms 72/game there;
        greedy chains fine when each step looks good alone).
      - Classic side-finding: the whole deep stack scores ~0.80 vs
        stock ON CLASSIC RULES (before W29 too) — our placeholders
        and design choices are civ2civ3-shaped; out of benchmark
        scope, recorded.
- Note: terraform.py's ONE_STEP pair table is classic-derived and
      mislabels some civ2civ3 one-steps as chain evidence; harmless
      for before/after deltas, fix when it matters.
- Note: the W28 stacking + newly-live cultivation compose: multi-
      worker forest clearing observed in garden games.

## Tuning loader gap (2026-07-26) — found, fixed, results voided

- **Bug**: `deep_tuning_init()` only had TUNE_ lines for the 14 fields
  that existed at E25.1. The 12 added since (site_*, horizon_*,
  diplomat_defence_value) were in the struct and used by the code, but
  any FREECIV_DEEP_TUNING value for them was silently ignored
  (secfile default-lookup). They were also missing from
  tuning-params.ini, so tune-B would have crashed at startup.
- **Detection**: every AWS ablation within one binary identical to 4
  decimals (contested all = 0.9275). Placebo confirmed locally: the
  W03-era siteonly/none ablation dirs' deep arms are byte-identical
  to the defaults dir (games are deterministic per seed).
- **Voided results**: all site/horizon ablations before fadb2a1f70
  (box and local); the W02/W03 "decomposition" (see retraction in
  Cloud handoff section — that spread was unpaired partial CSVs, an
  ad-hoc-analysis footgun now guarded by extract.py's seed-set
  warning, 13d393dab9).
- **Fix** (fadb2a1f70): 12 TUNE_DOUBLE lines; deep-tuning log line now
  prints all 26 fields so a gap is visible in any game log; fields
  registered in tuning-params.ini with defaults == compiled defaults.
  Verified live: zeroed file logs depth=0.000000 hcap=0.000000.
- **Campaign**: box redeployed at fadb2a1f70, rebuilt, placebo ablate
  logs archived (ablate.placebo-loader), stale result dirs deleted
  (1.9G -> 173M), service restarted 05:03 UTC 2026-07-26. Cache keys
  on the binary, so all ablations re-ran for real. Cap trim reverted
  per human instruction ("don't adjust the timing"): original
  TUNE_A_CAP=9h / TUNE_B_CAP=20h, window overrun accepted.

## Campaign complete (2026-07-27 22:06 UTC) — first trustworthy full-power numbers

Box self-stopped after writing report.md. Artifacts (report, logs,
best-tuning inis, holdout scorelogs, all metrics.csv) archived locally
in benchmark/results/campaign-2026-07-26/campaign-artifacts.tgz.
Ratios are paired per-seed deep/baseline total score.

- **Headline: primary acceptance 0.8400 +-0.044 (n=40, defaults
  tuning).** Deep trails baseline by 16% in THE target regime, and
  the CI is tight — this is the first well-powered measurement and it
  is definitive. Holdout primary with tuned params was no better
  (0.797 tune-A / 0.740 tune-B, n=12).
- **Parity in pure-development regimes**: garden acceptance 0.984
  +-0.100 (n=20); long.conf 1.023 +-0.095 (n=10); garden holdout
  ABOVE 1.0 under both tunings (1.019 / 1.017, n=12). Where war
  noise is absent, deep is at or slightly above stock. The gap is
  concentrated where military pressure decides games.
- **Ablations (12 contested / 6 primary seeds, post-fix, genuinely
  distinct)**: W03 horizon weights individually neutral on contested
  (<0.001 movement) — the urgency gate still has no evidential
  support. site_potential_discount and site_overlap_weight at
  placeholder values are HARMFUL in both regimes (removing them:
  +4pp/+8pp and +3pp/+10pp). site_special_cluster helps contested
  (+3.6pp), hurts primary. Package net: helps contested
  (all-off 0.926 vs defaults 0.955), hurts primary (0.776 vs 0.735).
- **tune-A overfit**: training contested obj -3.2 (near parity) but
  holdout contested -37.9/seed (0.959) — indistinguishable from
  untuned defaults. 6 training seeds was too few.
- **tune-B undersampled**: best (-148.2) was a GEN-0 RANDOM DRAW;
  6 later generations never beat it and drifted worse. Its values
  even contradict the ablation evidence (raised overlap/discount).
  Holdout -401.7/seed. ~56 evaluations for 9 dims is not a search.
  v2 plan: seed initial population from ablation evidence + defaults,
  population 16, cap in generations not hours, ideally on 24xlarge
  after quota bump; truncated-game objective now VALIDATED (below).
- **Truncation validation (2026-07-28, benchmark/turncorr.py)**: how
  well does the paired diff at turn T predict the final-turn diff,
  per seed, across every paired scorelog on disk (300 contested,
  40 garden, 24 primary-holdout, 30 isolated, 20 cramped pairs)?
  Primary: T200 r=0.77 / rho=0.76 / sign 83%; T150 r=0.46 (unusable).
  Garden: T200 r=0.95 local, 0.80 holdout. Isolated: T200 r=0.72,
  earlier useless. Cramped: decided late, T200 r=0.41 — never
  truncate cramped. Verdict for v2: SEARCH on a T200 objective
  (late turns are the expensive ones, so this buys roughly 2x more
  generations per hour), but confirm generation-bests and all
  holdout/acceptance numbers at full T300 — per-seed r=0.77 is good
  enough to rank candidates during search, not to report results.
  Caveat: this is seed-level fidelity; candidate-level bias (a
  tuning that looks good at T200 but fades, e.g. over-expansion)
  cannot be measured from existing data — the T300 gen-best
  confirmation runs are the guard against exactly that. (The only
  candidate-sweep data on disk, results/tuning/demo, is placebo-era:
  6 of its 8 candidates are byte-identical.)
- **campaign.sh flaws found**: (a) global DEEP_QUIET=1 suppresses the
  deep-want lines firstbuild.py parses, so the long.conf first-build
  acceptance table is all zeros — logging artifact, not behaviour;
  unset it for that stage. (b) report.md lists ablation logs but
  computes no ratios; fold the paired-ratio computation into
  report generation.
- **Next decisions**: fold which (if any) tuned values into defaults
  (tune-A's holdout showed no transfer — lean: none); tune-B v2
  design; whether the primary gap needs military-side workstreams
  before more economic tuning can show up in the headline number.

## Head-to-head grid + the two civ2civ3 defects (2026-07-28)

First HEAD-TO-HEAD measurements: mixed_db/mixed_bd arms seat deep and
stock in the SAME game (4e9fd90b73), mirrored so every seat sees both
modules per seed; seat-paired scoring (same seed+seat across mirrors)
is the honest metric — game-level ratio means are Jensen-inflated by
start-position luck and once showed a fake deep "advantage". Grid: 14
configs x {civ2civ3, classic}, ~900 games on the box, archived in
benchmark/results/mixed-grid-2026-07-28.tgz. tune.py --mixed
(f4f66ec6b6) optimises the within-game deep-minus-stock diff.

- **Seat-paired agg ratios (deep/stock, merged campaign tuning)**:
  contested 0.942/0.966 (civ2civ3/classic), cramped 0.778/0.992,
  garden2 0.793/0.903 (defaults 0.861), isolated 0.797/0.845, long
  0.844/0.982, primary-shared 0.663/0.952, primary-single
  0.736/0.871, primary-island 0.800/0.999.
- **The classic flip**: head-to-head deep is at or near parity on
  classic nearly everywhere, and loses 20-35% on civ2civ3. (Inverts
  the old same-world "deep ~0.80 on classic" note — that measured a
  different thing.) War-exposure gradient on civ2civ3 primary is
  monotonic: shared 0.663 < single 0.736 < island 0.800.
- **Defect 1, naval blindness (FIXED, constants untuned)**:
  assess_danger() (stock's, reused by deep) is blind to boat
  invasions; stock survives anyway by keeping token garrisons, deep
  trusted the assessment (danger=0, ndef=0, building Settlers the
  turn triremes landed) and lost 2.3x stock's cities on islands.
  Civil-war splinters of captured deep capitals ("extras" in
  metrics) keep running the deep module. Fixes: overseas fear from
  EMBARKED invaders within ~one transport move (1e6a118919 broad sum
  saturated danger and pinned deep at max military production —
  retest inverted city losses 63v94 but score 0.861->0.717;
  restricted in 81abefdd8c), garrison floor (6de9d40865), barbarian
  fear from the server's own onset gate (4993254d5c).
- **Defect 2, upkeep blindness (FIXED, constants untuned)**: deep
  bankrupts itself on civ2civ3 head-to-head — gold ratio 0.13-0.55
  vs stock, treasury peaks ~T100 then flatlines while stock's climbs
  (yet SAME-WORLD deep hoards 1.5x gold: the collapse needs war
  pressure). civ2civ3 charges gold for buildings, gold+food for
  units, 1-3 free units/city, tech upkeep/city (see ruleset survey
  in the 2026-07-28 session); every deep want ignored those bills.
  Fixes: horizon credits charge the building's ruleset gold upkeep
  (77081cf696); injected defence fear discounts by the cheapest
  defender's ruleset upkeep (01dae360d5).
- **Tuning**: defence-B campaign (local, 8h, tune.py --mixed on
  garden2+cramped train seeds) is sizing all seven new PLACEHOLDER
  knobs: overseas_fear/decay, garrison_floor(_danger),
  barbarian_fear, upkeep_gold_weight, upkeep_unit_weight. After it:
  holdout + garden2/isolated retest (pre-fix defaults benchmarks to
  beat: garden2 0.861, isolated 0.797, city losses 90v42).
- **Queued**: see findings/civ2civ3-adjustments.md for the full
  ranked list + the defender-rotation design (human-requested).
  Highlights: government-aware upkeep (cheap first step:
  government-adjusted utype_upkeep_cost in the defender discount);
  carrying-cost-aware W02 (distance waste + tech upkeep);
  health-credit in W03; rotation after defence-D lands. Still
  queued from before: war-unhappiness/martial-law (primary
  unhappypop 1.9), settler-factory economics, wonder prereqs.
  Edge-builders (symmetric): tired-attack discipline, embassy
  science, caravan policy, walls valuation.

## Head-to-head tuning campaign: final verdict (2026-07-30)

Three searches ran (G: garden2+cramped; H: cold on isolated+long; H2:
warm-started from G's optimum on isolated+long), a five-champion
bake-off, three 12-seed evaluation tiers, and finally the WIDE run:
60 fresh paired seeds x {defaults, G-final, H-final} x
{cramped, garden2b, isolated} (1080 games, us-east-1, artifacts in
benchmark/results/tuning-campaign-2026-07-30/).

**Wide verdict (per-seed paired, n=60, 95% CI):**
- defaults ratios: cramped 1.005, garden2b 0.880, isolated 0.765
- G-final vs defaults: cramped -0.13±0.15 (null), garden2b -0.18±0.20
  (null), isolated +0.085±0.056 (SIGNIFICANT, 39/60 seed wins)
- H-final vs defaults: all null.

**Ship decision: keep the compiled placeholder defaults.** The
mechanism commits (embarked-invader fear, menace-scaled garrison
floor, barbarian onset gate, ruleset upkeep charging, treasury guard,
health credit) carried the project to stock-parity on cramped and
0.88 on fair islands; no tuning fold is justified except noting
G-final as an isolated-specialist data point.

**METHODS FINDING (first-class): 12-seed batch means are unusable**
for score-ratio comparisons on head-to-head configs. Identical
binaries scored 0.57-0.85 on garden2 across three 12-seed batches;
every tier of the champion ladder read different weather; both the
holdout (+10.6pp) and confirmation (-8.7pp) verdicts on G-final
dissolved into nulls under per-seed pairing at n=60. Bar for all
future acceptance claims: paired per-seed differences, n>=50, or
mechanism-level metrics (city losses, treasury events) with large
effect sizes. The three-tier protocol (train / holdout-select /
virgin-confirm) caught both the overfit AND its own noise floor.

**Knob-diff covariate table** (G vs H vs H2 finals; agreements are
robust directions, divergences are missing covariates):
- AGREED across all three: raise upkeep_gold_weight (4.7/6.0/5.2 vs
  3.0), raise upkeep_unit_weight, raise settler_fearfulness_mult
  (2.7/4.4/5.8 vs 2.0), cut cm_war_food.
- ISLAND-vs-LAND divergences: site_special_cluster (G kills it at
  1.4; H and H2 both DOUBLE it to ~21 - tile-quality covariate
  confirmed from the optimiser side); BOTH island-native tunes
  disable the garrison floor (H: floor_danger 0; H2: floor 0) and
  ZERO overseas_fear - on island regimes the new fear terms read as
  pure tax, supporting reachability-gating and suggesting the
  mechanism wins came from the economics, not extra units.
- No consensus: barbarian_fear (44/79/13), menace_slope.

**Campaign cost**: ~$120 across three boxes. Real products: the
mechanism fixes (landed), the methods bar, and an evidence-backed
covariate queue (findings/civ2civ3-adjustments.md): reachability
menace, tile-quality CM/site weights, grow-state CM, tech-wall
credits, rotation. Next epic should come from that queue.

## Overnight pile (2026-07-30, human-directed)

Six mechanism commits from the findings queue, each build+test clean,
tripwires verified in live games:
- 9902402d59 reachability-gated menace (menace=0.00 across a
  pre-contact island opening; nonzero after contact on cramped)
- f39ab7fe96 grow-hard CM state (cm_grow_* PLACEHOLDERs)
- 5b766824d0 tech-wall: capacity credits route into tech wants
- d372458f70 / 9ee9089975 / 76b987f089 rotation moves 3/2/1 (melt,
  replace-first want, treasury-gated buys); deep-rotate observed
  live at T96 (Warriors melted, Phalanx buildable, count 2)
Deferred with notes: embassy science (boost point is diplomat want,
needs daidiplomat read), tile-quality CM scaling (formula needs
data). New PLACEHOLDER knobs: menace_overseas_weight, cm_grow_*,
horizon_tech_weight, rotation_power_ratio, rotation_want.
Acceptance per the methods bar: new-binary defaults vs the cached
old-binary wide baselines, 60 paired seeds x cramped/garden2b/
isolated + same-world guard, running overnight on us-east-1.

## Interaction round + ablation (2026-07-30, continued)

garden2b knob-ablation of the pile regression (-0.238): grow state
primary (~0.13 recovered), rotation small (~0.05), gate/tradescale
nil; stage-5 techwall pending. Fixes committed: danger-vetoed
grow-hard released by a PAID garrison (linear defence + sortie value
vs margined danger; margin neutral 1.0, range spans both priors),
boat-shadow standing coastal fear while enemies own transports,
sortie assets exempt from melting, worker flee (radius 3) + border
bodyguard requests (radius 6), convoy hold+escort (verified inert --
deep's navy is passive until the invasion-want epic).
NOTE commit hygiene: 65b54a57c9 also carries the convoy-discipline
code; its message covers only the sortie change. Single-branch
local history; splitting judged riskier than documenting.
Acceptance round 2 (all fixes, 3 configs x n=60 paired + guard)
launches when ablation stage 5 lands.

## Military & intelligence bundle imported (2026-07-30, human-supplied)

Five designs (W31 naval, W32 air, W33 operations layer, W35
espionage, decapitation capstone) landed in workstreams/ + findings/.
Every code claim spot-verified against source at 0896b34: invasion
census zeroed at daiunit.c:1302, anti-bombard "dummy support" FIXME
at daimilitary.c:606, spy-action constant ladder in daiactions.c
(10000/9000/.../2500 with its own FIXMEs), spaceship_lost() on
capital capture at citytools.c:2028, zero airbase references in
autoworkers/daidomestic. The central claim -- no object represents a
multi-turn operation; ai_invasion is a per-turn census of emergent
drift -- matches what the convoy epic found independently
(civ2civ3-adjustments.md "Naval convoy design": "ai_invasion mass is
bookkeeping no decision consumes").

Intake assessment:
- ACCEPTED as Phase 3 sequencing: W33 substrate first, doctrines as
  phase lists over it. The struct operation design (commitment /
  phase gates / deliberate abort banking survivors) is the right
  generalization of what the convoy scaffolding gropes toward.
- OVERLAP: W31 E31.1 (escort precondition) is partially landed as
  the opportunistic version -- convoy hold + aiguard escort pairing
  (2026-07-30, inert until invasion wants). Queued naval slice 3
  (defence-at-arrival go/no-go) is the un-substrated form of W31
  E31.3/E31.4. Keep harvesting these opportunistic slices FIRST;
  they are benchmarkable now, and W33 absorbs them as phase gates
  later. Amendment to the bundle's "build W33 first": true for the
  coordinated parts, but 12 commits of substrate before any
  measurable benefit violates our evidence rhythm -- the fallback
  slices de-risk the phase lists before the substrate exists.
- STALE REF: W31 says escort "reuses W13 campaign machinery" -- W13
  does not exist; our aiguard-based pairing is the actual landed
  mechanism and the likelier substrate until W33.
- MEASURABILITY CAVEAT (rule 6): W32/W35/capstone value concentrates
  in the flight+ era. Before scheduling either, verify our benchmark
  windows actually reach that era (check savegame tech at horizon on
  long configs); otherwise acceptance rests solely on L1 corpus
  tests, and headline claims still need paired n>=50. W35 E35.1
  (currency valuation of spy actions) is the exception: it bites as
  soon as diplomats exist and does not strictly need W04, though the
  bundle gates all of W35 on it.
- W34 (victory plan) flagged by the bundle as the highest-value
  undesigned gap; agreed -- it also subsumes tech direction
  (daitech.c has no push toward any goal). Needs a design pass
  before the capstone is schedulable.

Queue position unchanged: accept3/Z2 verdicts -> naval invasion
slice 3 -> tuning v3 -> combat workstream. The bundle slots in after
that as Phase 3, gated on W04/W10/W12 extraction.

## Acceptance round 3 verdict: density epic regresses at placeholders (2026-07-30)

Complete-manifest binary (density fear + density prior + wall fear
+ settler rush + choke bonus, all knobs at committed PLACEHOLDER
defaults) vs the pile+fixes binary, seat-paired on the same 60 wide
seeds (metrics archived: benchmark/results/accept3-2026-07-30/):

  config    n-seats  agg-ratio (was)   paired: new-worse  median-diff
  cramped     180    0.669  (0.935)         63%             -0.223
  garden2b    120    0.454  (0.537)         57%             -0.056
  isolated    240    0.658  (0.777)         63%             -0.130
  contested guard (n=10, game-level): 0.797 (was 0.902), 0 wins

VERDICT: do NOT fold the density-epic mechanisms into defaults at
placeholder values. The regression is density-proportional (cramped
hit hardest), consistent with density fear + wall funding + settler
rush all taxing the same economy at once when every map neighbour
triggers them. This is rule 7 working as intended: the placeholders
were never candidates for defaults, and the acceptance run priced
them. defence-Z2 (east-2, running) is tuning all 32 knobs including
the density set, warm-started from G-final; its gen-0 control
(-521.0 mixed objective) is the same package this round measured,
and gen 1 already found -360.1. Re-accept with Z2's best-tuning on
holdout seeds per the three-tier protocol before any fold decision.
east-1 stopped after harvest; results retained on its disk.

## Pivot: tuning halted for a mechanism-bug round (2026-07-31, human call)

Z2 stopped at gen 4 (bests: -521 / -360 / -242 / -136 / -104 --
still improving, but the seed-205 forensics showed the gradient is
partly pricing pathology, not design). Harvest banked: gen-4
best-tuning.ini + full candidate log (every candidate is
reconstructible from log lines). Both boxes stopped.

Bug round queue, from the forensics (findings/civ2civ3-adjustments
"Seed-205 save forensics"):
1. Explorer pump: deep_explore_want is a GLOBAL want applied
   per-city with no coordination -- 25 cities answer the same
   demand in one pass (Explorer x96 by T200 on seed 205), and the
   unknown-tiles term never decays on island maps (can't see across
   water). Fix: count explorers under construction in the divisor
   and/or gate unknown on same-continent reachability.
2. Embassy-diplomat want needs the same reachability gate.
3. Rotation churn: dozens of build-then-melt cycles per city;
   power-ratio guard needs hysteresis/cooldown.
4. Idle treasury: 2000+ gold unspent through the midgame stall;
   find what starves dai_spend_gold.
5. run-game.sh arm purity: civil-war splinter players inherit the
   server-default aitype (a splinter ran ai_type="deep" in one
   mixed arm). Pin splinter aitype per arm.
Settler stall (the top-line defect) is likely downstream of 1+3+4:
explorers and defender churn outbid settlers in choose_build
through exactly the T100-140 window where stock runs 15-27
concurrent settlers. Retune AFTER the fixes, warm-started from the
gen-4 harvest.

## The settler stall is dead (2026-07-31, seed-205 verification)

Root cause chain, found by instrumentation (deep-settler tripwires):
from T38 the placement search preferred nearby-islet overseas sites
(250+ vs infill ~190) that a 2-boat ferry pipeline could never
deliver; missions were never invalidated, so settlers parked at
docks while the island sat half-founded. Fixes, each verified by
distribution not sighting:
- 8d77e147bd ferry-wait pricing: overseas candidates amortized by
  the real pathfound wait to the nearest available boat; hard skip
  when every boat already has a passenger. (An intermediate
  drop-only version looped 1220 picks/1202 drops -- the drop
  cleared the ferry request that armed the cap; recorded here
  because it is the canonical two-fixes-defeating-each-other case.)
- 608b8b8a56 same-turn land-only fallback when a boat-dependent
  mission gets no boat.
- 8aefb1f8db embassy wants gated on deliverability (11 diplomats on
  an island = the explorer pump in a second organ).

Seed-205 full verification (all bug-round fixes + Z2-gen2 tuning):
city curve 25/30/37/44/51/62/66 -- the 24-27 plateau is GONE; deep
out-expanded stock (37 vs 26 at T160, winning 10 formerly-stock
islet sites), ran a real amphibious war (80 Marines, 20 Transports,
33 Cruisers), and finished 66 cities vs 3, score ratio 5.00 -- on
the seed that scored 0.20 pre-fix. One seed, one game: the paired
12-virgin-seed re-accept decides the real number. Watch items:
drop-pick cycling persists cosmetically (633 events), explorer
census crept to 47 as newly reached islands reopen explore want,
catchment overlap at the citymindist floor is priced by
site_overlap_weight (placeholder) and showed no specialist
starvation (22 specialists across 66 cities).

## Accept4 + Z3 + tuning v3 (2026-07-31, the fixed-binary verdicts)

ACCEPT4 (fixed binary, committed defaults, 60 wide seeds seat-paired,
metrics archived benchmark/results/accept4-2026-07-31/):
  garden2b 0.705 (was 0.454 accept3 / 0.537 pre-density) -- ALL-TIME
    defaults record, +0.25 over the broken binary
  isolated 0.861 (was 0.658 / 0.777) -- all-time record, beats even
    the old binary's tuned numbers
  cramped 0.657 (flat vs 0.669) and contested guard 0.794 (flat vs
    0.797): the war regimes still carry the density-epic regression
    AT PLACEHOLDER PRICES -- a tuning problem, not code (local
    paired: Z2 values beat placeholders +0.226 median on cramped).

Z3 (12h retune, garden2b+cramped, warm from Z2-gen4): objective SIGN
FLIP -- gen bests +149/+170/+140 vs -103.6 all-time best on the
broken binary; population median crossed zero in gen 1. Winner
shape vindicates the session's design calls: explore_discovery=0
(omniscient AI needs no reconnaissance), site_overlap_weight 0.18
(4x placeholder -- overlap priced once expansion works),
settler_fearfulness 1.59. Only 3 gens completed: fixed-binary games
run ~2x longer (conquests happen now). Box overran its shutdown
(timeout TERM insufficient; fixed in v3 with timeout -k).

TUNING V3 launched (east-1): cramped+isolated+contested training mix
-- the two laggards plus an economic regime; garden2b HELD OUT
(human call: too long and too idiosyncratic to train on; its lesson
is in the warm start). 36 params, warm from Z3's +170.4 candidate,
12h, self-stops. Local garden2b re-accept (tuned + defaults paired,
12 virgin seeds) still finishing after a laptop-suspend pause.

## Local re-accept verdict (2026-07-31 evening): the fixes carry it

12 virgin confirm seeds, garden2b, seat-paired: fixed binary medians
0.826 tuned / 0.784 defaults vs pre-fix 0.456 / 0.405. Code fixes
alone: better on 22/24 seats (92%, median +0.382, sign test ~3e-4)
-- past the methods bar. Z2-gen2 tuning on the fixed binary adds
only +0.044 median (54%): its values priced the broken machine and
are superseded by Z3/v3b. Consistent with accept4's wide-seed +0.25.
The bug round, not the tuning, was the week's yield; v3b prices the
remainder (cramped + contested regressions) overnight.

## Anticipation ramp (2026-07-31 late, human-directed)

density_fear_ramp_turns landed (PLACEHOLDER 0=off): anticipatory
fear (density + prior) scales min(1, turn/ramp); evidence terms
unramped. Live gradient verified at ramp 120: settlers-first T30,
3-4 Phalanx T60, full hedgehog T120. Flagged for the NEXT tuning
campaign (v3b, mid-flight, does not search it); fold candidate
alongside v3b's values under the three-tier protocol.

## Strategy pivot: mechanism epoch, tuning finale later (2026-07-31, human call)

v3c killed 30 min in (nothing banked beyond v3b gen 0); east-1
stopped; no boxes running. Rationale, from the week's ledger: code
fixes moved the local pairing +0.38 median, tuning on fixed code
+0.04 -- campaigns have functioned as expensive bug detectors (Z2
zeroed knobs -> worker-guard/choke questions; v3b gen 0 -> the
hut->barbarian blindness and the isolated-classic -1143 cell), and
every mechanism fix perishes the values just bought. New rhythm:
mechanisms until the bound-slam diagnostics come back quiet, then
ONE tuning finale (v4, big box) on the mature machine, folded under
the three-tier protocol. Warm-start lineage preserved: v3b cand-5
(+179.8 on six cells) is the current best-known point.

Mechanism epoch queue, roughly by size:
1. Naval invasion slice 3 (BIG): invasion wants + defence-at-arrival
   go/no-go; activates the inert convoy scaffolding; the contested
   cells are flat partly because deep never wants to invade.
2. Combat workstream (BIG): sequential-attrition odds,
   tired-attack discipline -- upgrades every danger/defence consumer.
3. Density-fear reachability gate (small, spec'd 0112a65112).
4. Hut-risk awareness (small): explore want must price classic's
   hut->barbarian lottery; v3b priced around it, code should know.
5. Watch items: drop-pick cosmetics, explorer census creep, W08
   doctrine coupling (the architecture bet, after the above).

## Combat workstream E1: survival substrate (2026-07-31 late, epic closed)

Resequenced BEFORE naval slice 3 (human call): the invasion go/no-go
is a combat-odds comparison and must be written in survival currency;
naval recon banked in task #16. Workstream then expanded twice on
human direction ("not big enough") into an E1-E8 map held in task
#17: arithmetic (E1, this epic) -> offensive force sizing (E2) ->
sorties (E3) -> ASSAULT OPERATIONS (E6: mass in a city/fortress,
launch a smash -- the first real W33 phase list; #16's convoy gates
are its naval instance) -> second-wave reserves (E7) -> raiding ops
(E8) -> aggregation swap (E4, deliberately pre-v4 only) -> sieges
(E5). Human posture notes banked in task metadata: cover/kill
sorties priced as survival-deltas; escort demand scales from zero
with observed sea threat (naked early caravans to allies are fine).

Landed (8 commits, ecaad878e0..3fdc3d3fa6): deepodds.c -- exact duel
math, win_chance()'s recurrence with roles swapped keeping the
defender-HP distribution, chained over an ordered attacker stream vs
a defender stack; freestanding, checked by tests/deepodds_test.c
against an independent forward-DP oracle (first L0 unit test in the
tree, meson 'deepodds'). deepcombat.c grounds it (wave = at-war
units reachable within survival_horizon, fresh-arrival power,
strongest-first vs best-defender-first; caps 32/16 computational).
Consumers: survival-deficit fear in both danger passes (deep-surv),
grow-release survival veto beside the linear margin test,
tired-attack holds (deep-tired; attack power scales by
moves_left/SINGLE_MOVE and stock never priced it). Measurement:
city ledger via previously-NULL city_got/lost hooks (deep-city;
teardown flood found by the first smoke run, guarded to
S_S_RUNNING) + benchmark/cities.py.

Numbers (smoke cell, 10 paired seeds, compiled defaults, T100):
deep/baseline median 0.891 with survival machinery ON and OFF alike
-- 8/10 seeds byte-identical (the gap is pre-existing
defaults-on-smoke posture, not this epic). The one seed with a real
war (seed 1, the only conquests in the set): ON 0.836 vs OFF 0.798.
Mechanism distributions: 1218 threatened city-turns, mean survival
0.881, 11.3% below even odds; tired holds 0 on smoke (no
adjacent-tired moments; civ2civ3 tired_attack=TRUE confirmed).
Smoke self-play barely fights -- E2 + contested cells are the real
proving ground.

New PLACEHOLDER knobs (all four edits + ini): survival_target 0.5,
survival_fear_value 20, survival_horizon 3, tired_attack_floor 0.9.
Model assumptions stated in findings (single-wave, no healing,
military-only garrison, observed units only).

Session mode (human 2026-07-31): epic-by-epic THROUGH the queue in
one sitting -- close each with code + numbers + STATUS before
opening the next. E2 next.

## The operations evening: E2/E3/E6, gates, colonies, naval slice 3 (2026-07-31 night)

Rolling epics on human direction ("multiple of these epics... might
as well do the naval epics too"; "stay active"). Binary chain, each
step contested-verified (10 paired seeds, placeholder defaults,
deep/baseline self-play score ratio):

- B2 (E2 marginal take-odds attacker wants): 0.758 -- CAUGHT A REAL
  BUG. Unmet players are nominally at war (initial diplstate), so
  turn-1 ungarrisoned "enemy" cities priced P(take)=1.0 and
  worth-scale wants outbid every settler. 29k deep-kill lines.
- B3 (+contact gates + E3 sorties): 0.819, 8/10 seeds improved.
  Contact required before targeting, fearing, or sortieing at a
  player (barbarians exempt: their units ARE the contact). E3: kill
  sortie (finish an adjacent weak besieger when duel odds clear
  sortie_win_floor AND the city clears survival_target without the
  blade -- stock only rampages above 5x bodyguard threshold in
  kill_desire currency) + cover sortie (lend the weakest sparable
  garrison blade to an unanswered worker bodyguard request). Fired
  34/448 times per 10 games.
- B4 (+E6 assault operations): 0.796 on / 0.731 all-off (median;
  the stack's net worth ~+0.065 at defaults). Ops alive: 1117
  formed, 1029 struck, 62 aborted per 10 games; required forces
  2-4 (proportionate to contested's thin garrisons), quorums in 0-1
  turns, wait-cap fires with half-quorum launches. Score-neutral vs
  B3: small waves, correct but not yet decisive.
- B5 (+satisfaction gate #19 + barracks guard): 0.812 on / 0.730
  all-off median (means tied 0.796); foundings 299->324, conquests
  13->7, losses 22->13 -- quieter, more expansive. Same-seed T151
  finals: Barracks 20->12, Temples 4->7 EVEN UNDER CONSTANT WAR.
  The gate's real payoff cell is quiet growth (isolated/smoke);
  contested keeps cities honestly unsatisfied.
- B6 (+naval slice 3 #16 + colony lift #20): smoke/contested/
  isolated chain running at session close; isolated (island starts,
  T300) is the amphibious + colony + growth-ladder proving ground.

Landed beyond the E-epics, all from live-game forensics (findings
"T153"): SATISFACTION GATE (#19) -- injected fears scale by
satisfied_fear_discount (default 0) once floor met + no urgency/
grave danger + survival at target; 22 deep cities had reached T153
with ZERO growth infrastructure because standing fears outbid the
granary in every auction forever. BARRACKS GUARD -- stock
adjust_ai_unit_choice swaps any unit choice for the veterancy
building with NO garrison check, even in an empty city (upstream
defect recorded); deep requires the floor met first. COLONY LIFT
(#20) -- the settler pipeline cap read 0>=0 with no boats and
silenced overseas demand forever (71% of the Europe map unowned);
a demand-only virtual-boat probe now records the best overseas
site score and coastal cities answer fresh demand by building the
boat. NAVAL SLICE 3 (#16) -- deep_take_odds_at_arrival adds
eta x invasion_reinforce_rate virtual best-buildable defenders
(travel time is the defender's budget, BOTH terrains, one law);
loaded convoys hold until the assembled landing clears
op_take_target at arrival, with the stateless nobody-else-coming
release; overseas at-war coastal cities join the want/op target
menus; amphibious ops stage at the nearest own port; muster berth
shortfalls drive the lift want (third consumer of the
lift-limited-means-build-the-boat rule).

New PLACEHOLDER knobs this stretch (all four edits + ini):
attack_value_weight, sortie_win_floor, op_take_target,
op_muster_wait, satisfied_fear_discount, colony_lift_weight,
invasion_reinforce_rate. Benchmark: lategame cell added (techlevel
50, colonial startunits, dispersion 3 -- human design; validated,
joins the v4 mix); cities.py pools all new tripwires (deep-city/
surv/tired/sortie/op/invade/colony/sat).

Caveats for the next session, stated plainly: every number above is
n=10 quick-run directional, not the n>=50 methods bar; the whole
stack runs at compiled placeholder defaults, untuned; op quality is
mechanical-not-strategic (small waves); multi-turn siege compounding
and healing remain unmodelled; W31's deeper layers (sea lanes, fleet
composition, bombardment) unstarted. Remaining E-map: E7 second
waves, E8 raids, E4 aggregation swap (pre-v4 only), E5 sieges; #18
small gates still pending.

## B6 isolated verdict + the small-hours epics (2026-07-31, very late)

B6 verification chain complete. Smoke median 0.891 == B1's exactly
(the whole evening added zero quiet-cell regression; mean up 0.913).
Contested holds 0.811 with foundings 324->401 and losses down to 11.
ISOLATED (T300 islands, the #19/#20 proving ground) is a different
species from the Europe T153 fossil: 7618 overseas settler picks
across 10 games, colonial empires of 29-36 cities, real navies
(Cruisers x35 / Destroyers x16 / Transports x17 in single players),
and THE FULL GROWTH LADDER -- Granary 7 / Temple 9 / Marketplace 7 /
Aqueduct 6 / Harbour 6 / Library 6 / Banks / Sewers in P1 where the
Europe game had zero of everything at T153. Score at untuned
defaults: isolated median 0.853 -- closing that gap is v4's job.
Tasks #19 (satisfaction gate) and #20 (colony expeditions) VERIFIED
AND CLOSED. #16 stays open on one honest gap: the deep-invade
assault gate fired zero times in every cell (self-play never
produced an amphibious assault); exercise it via the lategame cell
or mixed arms before closing. Watch items confirmed alive: explorer
census creep (18 Explorers in one player), diplomat count (20).

Landed after B6's launch (B8 binary, combat-first per human call --
"go further on the combat workstream before the tuning run"):
E7 SECOND WAVES -- wave one enrolls required x1.5, the rest stays a
designated reserve; for op_wave_window turns the struck target has
first claim on the next formation, re-priced against the garrison
wave one softened; the window closing is the formal withdrawal.
E8 RAIDS (first slice) -- with no operation standing, idle fast
attackers hunt exposed enemy civilians within raid_radius; pillage
missions deferred. #18 CLOSED -- cross-water density fear scales by
the neighbour's actual lift (menace_overseas_weight reused, no new
knob), and huts near own cities discount by hut_risk_discount
within hut_risk_radius while barbarian.c's exact gate (rate+onset)
says spawns are possible. New knobs: op_wave_window, raid_radius,
hut_risk_radius, hut_risk_discount. B8 contested verification pass
next; E5 sieges deliberately parked for a fresh session (needs
per-city streak state); E4 parked pre-v4.

B8 contested verdict (session close): E7 and E8 fire -- 211
second-wave formations, 534 raid missions per 10 games. Score
median 0.775 (B6 was 0.811; inside the evening's 0.78-0.82 noise
band) BUT losses doubled 11->23: smaller first waves + raiders
dying deep in enemy land. Whether raids are good trades is
UNMEASURABLE yet -- the ledger counts our deaths, not our victims;
a kills-side tripwire (enemy civilian deaths near our raiders, or a
server-side victim hook) is the missing metric. Per rule 7, raid
radius and wave sizing are v4's to referee, not tonight's to
hand-tweak. Session totals: 35 commits from the pivot, binaries
B1-B8 each contested-verified, tasks #18/#19/#20 closed, #16 open
on the unexercised landing gate, #17 umbrella at E1-E3+E6-E8 landed
/ E4-E5 parked. Next session: E5 sieges fresh, the raid exchange
metric, exercise the invasion gate on the lategame cell, then the
wide-n acceptance when the human calls the combat stream done.

## W33 first arc + the T160 reconquest stack (2026-07-31, small hours II)

Task #21 opened on human "march onward". Binary chain continues
(contested quick, 10 paired seeds, per-seed deep/baseline):

- B9 (multi-op substrate + MARCH phase): 0.826 median / 0.825 mean --
  top of the evening's 0.78-0.82 band. Substrate: deep_plr holds
  ops[4] slots, deep_op_claiming() keeps every pool honest, E7's
  second-wave claim moved to player level, berth-shortfall lift sums
  across overseas musters. MARCH: launch releases members to stock
  but the op SHADOWS them -- re-gate each turn at remaining eta
  against the CURRENT garrison (abort-odds banks everyone, tasks
  cleared), strike recorded at contact so the E7 window opens at the
  walls, march timeout reuses op_muster_wait. New knob
  op_abort_floor (PLACEHOLDER 0.35). Ledger: 114 forms -> 78
  marches -> 15 strikes / 72 odds-aborts: the re-gate is honest but
  2-4-unit waves lose the race against one reactively-bought
  defender (root cause = the cold start, fixed in the T160 stack).
- B10 (+raid op template): 0.784 median / 0.834 mean, three seeds
  above 1.0. E8 graduates into DEEP_OPK_RAID slots claiming idle
  fast attackers; 369 raid formations, 489 hunts per 10 games.
  WATCH: assault forms fell 114 -> 40 -- standing raids occupy
  slots and claim units; strike-control commit should referee this
  with data (raid yields to forming assault?) rather than a guess.
- B11 (+T160 reconquest stack): running at close. The stack, all
  from live-save forensics (findings "T160"): barb_reinforce_rate
  (barbarians price as non-reinforcing, PLACEHOLDER 0.0 = observed);
  singleton ops (required==1 forms -- stock never claims even empty
  cities, garrisons are DEFEND_HOME); garrison draft (formation
  borrows one sparable blade per city, survival-gated, when the free
  pool can't clear the bar); campaign pricing (deep-kill ck= --
  smallest clearing force priced at average rate when the marginal
  is cold-start zero); snap-up sweep (deep-kill type= -- cheapest
  land attacker that clears alone wins the amortization race).
  A walk-in adjacent sortie was prototyped and DROPPED: citymindist
  makes the geometry impossible.

NEW FIXTURE: benchmark/corpus/barb-reconquest -- the T160 Europe
autosave (human seat aitoggled by direct save edit), 52 barb cities
/ 47 empty. Asserts singleton formation + >=1 tile retaken + nobody
dies, 12 turns. Proof: 10 tiles retaken where the live game managed
0 in ~40; full corpus 7/7 green. Runner allowlists the pre-existing
recoverable deep-settler pf assertion -- task #22 (100 deep-arm
logs, 0 baseline; deep_settler_apply_risk parameter mix suspected).

Human doctrine notes banked this stretch: barbarians lean against
defending their cities (encoded as barb_reinforce_rate); defence
first, then quick snap-up units (defence-first already emerges from
the fear auction + satisfaction gate; snap-up = the sweep).

T160 forensics headline (findings file has the numbers): human
Irish #1 at 49 techs from 5 tall cities; Suleiman (deep) #2 at 48;
deep holds 4 of top-8 in a 22-player field. Pirates+Barbarians hold
the two biggest empires; Iberia (classic) collapsed 23 cities.
Every nation's army is defensive infantry; the only siege train in
Europe is barbarian.

Next: B11 verdict -> #21 phase 3 (strike control, THE RISKY PART)
with the raid/assault slot tension refereed by its ledger; then
save/load (#21 phase 4); E5 sieges still parked for a fresh
session; wide-n + v4 tuning wait for the human's call that the
combat stream is done.

B11 verdict (T160 reconquest stack, contested): median 0.812 / mean
0.839 -- band-stable, best mean and best single seed (1.116) of the
chain. Losses HALVED to 10 (B9/B10: 16/17), observed not attributed
(n=10). Mechanisms all alive in contested: campaign pricing on 75%
of deep-kill lines (25149/33308 ck>0), snap-up sweep won by Chariot
22045 / Horsemen 4857 (cheap-fast beats power-bias as designed), 27
singleton formations, 41 drafted formations. Still open: 54 marches
-> 4 strikes / 54 odds-aborts -- civilised targets reinforce faster
than small waves march; composition shifts need more than one game.
That dynamic is strike control's (phase 3) to referee, with the
raid/assault slot tension. Reconquest OUTCOME is proven by the
fixture (10 retakes / 12 turns), not by contested, which has no
barbarian city pool.

## W33 phase 3: strike control + the manage-loop hole (2026-08-01)

Task #21 phase 3, 8 commits. Both open questions refereed from the
B11 ledger (contested-e8cb148a4574) BEFORE code moved.

REFEREE 1 (54 marches -> 4 strikes): not the muster clock -- even
waited<=2 marches aborted 13/14, and 27/47 odds-aborts re-gated on
the launch turn itself. deep_op_eta priced the wave at the SLOWEST
enrolled member (reserve stragglers: present=2/required=2 waves
carrying eta=7), billing the quorum at the walls for its last
straggler's walk in virtual reinforcements; all four B11 strikes
launched at eta 1-2. Fixed: quorum eta (required-th smallest
arrival). Launch-turn abort share: 57% (B11) -> 34% (B13).

REFEREE 2 (raids crowding slots, forms 114 -> 40): confirmed,
slot-shaped -- 711 raid forms at mean size 1.3 held >=2 of 4 slots
on 56% of live player-turns; every one of the 750 fully-blocked
player-turns included a raid. Fixed: one pooled raid op per player
+ release-on-idle (below). B13: ZERO fully-blocked player-turns,
raids at >=2 slots on exactly 1 player-turn per 10 games.
Assault-poaches-raiders is UNNECESSARY -- released raiders rejoin
the pool by themselves. Closed.

THE RISKY PART WAS REAL. The new stall tripwire (deep-op-stall)
caught dai_manage_units clearing every done flag before managing:
every hold the op layer ever issued was erased the turn it was set.
The muster hold NEVER held in B9-B11, and a STRIKE member was
watched walking AWAY from its target under find_something_to_kill.
Fixed with deep_manage_units -- the stock loop verbatim (rule-2
visibility: dai_airlift + dai_set_defenders static -> extern, zero
behaviour change, neutrality covered by the B12/B13 runs) except
op-held units keep their done flag. Holds cover assault MUSTER/
STRIKE/CONSOLIDATE; marching members and raiders stay on stock's
leash by design.

STRIKE phase: at contact the op keeps its members and fights --
best duel first against the target's current best defender, focus
fire on the op target, walk-in conquest of emptied cities,
stragglers converge under op control, eta-0 regate vs
op_abort_floor before the first blow, op_muster_wait patience.
CONSOLIDATE: a fallen city (strike or march capture) garrisons
from the wave, best defender first, until survival_target clears
(the existing bar -- no new constant); dissolve reason "held".
cities.py grew op_consol/op_held/op_stall columns.

B12 contested (10 paired, per-seed sum-score deep/baseline): median
0.781 / mean 0.846 / max 1.116, 2 seeds >1. Ledger: 57 forms -> 38
marches; "15 strikes" decomposed to 13 AMPHIBIOUS LAUNCHES + 2 land
walls-strikes -- the real hold means musters now actually assemble,
so convoys finally sail (#16's landing gate should at last see
traffic). Stalls 10569, of which 9094 RAIDERS: the pooled raid op
kept idle raiders claimed while anyone hunted, benching cavalry out
of every pool. Fixed as commit 8: raid holds active hunters only.

B13 contested (full stack): median 0.811 / mean 0.853 / max 1.045,
2 seeds >1 -- median back at B11's level, best mean of the chain.
Ledger: 49 forms -> 37 marches -> 7 strikes / 36 aborts, 2
consolidate/held, conquests 5 (B11) -> 16, fewer forms with the
same marches = formations sticking instead of churning. WATCH
ITEMS, stated plainly: cities lost 10 (B11) -> 22 (B12) -> 26
(B13) -- B11 may be the outlier (B8-B10: 16-23) and conquest tripled
with score band-stable, but the trend is real and unexplained in
detail; and 6948 residual raider stalls are task-ATTACK hunters
that STOCK declines to move ("sit tight") while their claim
excludes them from assault pools -- refereeing that needs
per-member idle state, a next-session design question. All n=10
quick-run directional, untuned placeholders throughout.

Fixture: barb-reconquest green all session, retaken=14 best yet
(B11: 10); full corpus 7/7. Housekeeping: CLAUDE.md's
./build/tests/unit_tests path is stale (the suite is `meson test
-C build`); report.py does not exist (ratios computed from
metrics.csv, convention verified against B11's published numbers).
Task numbering #16-22 lives in the PREVIOUS session's task store
(~/.claude/tasks/90a0f237-*/); a fresh session starts a fresh
store, so TaskGet by old ids fails -- the #21 spec was recovered
from the old store's 21.json on disk.

Next: #21 phase 4 (save/load of op state); E5 sieges fresh session;
exercise #16's landing gate on the lategame cell now that musters
assemble; v4 tuning owns the march reinforcement race
(op_take_target/op_abort_floor spacing, invasion_reinforce_rate);
raider sit-tight idles need a design decision.

## W33 phase 4 + two referee runs (2026-08-01, continued)

OP SAVE/LOAD (2 commits, task #21 TERMINATES): ops persist under
deep_state (free slots one key each; last_strike rides along for
E7). Two traps found and coded around, verified on the T160 fossil:
(1) the player_load hook fires inside sg_load_player_main BEFORE
the player's units load, so member ids CANNOT be validated at load
-- an id lookup there empties every op (watched it happen: 4 saved
ops, reload, silent vanish, phantom re-form). Restored raw; the
per-turn validation pass owns the cleaning. (2) no savegame carries
unit_ai tasks, so loaded MARCH members are task-NONE: ops_reissue
re-points them once at the op target ("remarch"), verified via a
hand-edited MARCH save. Round-trip proven: save -> reload ->
continue (one op aborted for honest world reasons, dissolve logged)
-> next save still carries survivors.

#16 LANDING GATE, sharper diagnosis, stays OPEN: lategame cell
(deep vs baseline, 10 paired): median 0.867 / mean 0.883, ops at
scale (209 forms -> 151 marches -> 58 strikes of which 36
AMPHIBIOUS LAUNCHES, conquests 53). And STILL zero deep-invade
lines: the gate scans for a LOADED convoy resting IN PORT at phase
start, but stock's ferry system boards and sails within one manage
pass, so the precondition never materializes even with Transports
everywhere (convoy_min_load=2 active). The gate is hooked at the
wrong seam -- it needs to sit in the ferry/boarding path (or the
op layer's own future sea phases), not a port scan. Design item.

CITIES-LOST FORENSICS (B13's 26): the muster holds are EXONERATED
-- 1/26 was an op staging city. But 21/26 were seen threatened by
the survival tripwire before falling and 18/26 fell with last
recorded p < 0.5: the defence model watches them fall and the
response side does not answer. Offense now claims units (ops +
raids) while reactive defence has no counter-claim; whether a
below-bar city should outbid a forming op for blades is a priority
question FOR the ops layer, next session's referee. (Evacuation of
the doomed is W14's, far off.)

Fixture green throughout; meson tests green throughout. All n=10
directional, placeholders untuned.

Next now: E5 sieges (fresh session, per-city streak state) and the
raider sit-tight design share a shape -- both add per-entity memory
to ops; consider together. Defence counter-claim referee. #16 gate
re-seam. v4 tuning: march reinforcement race + the whole knob set.

## #16 closed: the invade gate at the right seam (2026-08-01, cont.)

Five commits re-seat the landing decision where landings happen.
The crossing SHADOW: an overseas launch enters MARCH like a land one
(sea-aware quorum eta: aboard = the boat's rate, ashore
off-continent = best buildable ferry); the ferry system still
carries the crossing -- transported members stay on stock's leash
in every phase, because holding a passenger freezes its boat -- and
the per-turn re-gate IS the invade gate: refusing to land into a
garrison that reinforced past the floor logs deep-invade hold,
reaching the beach logs go. The port-scan gate retired (measured
dead: its precondition, a loaded convoy resting in port at phase
start, cannot occur when stock ferries board and sail in one pass).

Human doctrine landed the same arc: ANY abort now asks whether the
gathered force, where it stands, can take something else -- the
full formation bar at the force's own arrival eta, worth over
quorum-eta scoring, no new constants -- and a retargeted land wave
rallies at an own city on the way (never backtracking) instead of
re-mustering on random squares. Fleets mid-crossing march direct;
their port call is the offshore-hold future work (task #3, with the
engineer-fortress-at-field-muster idea, which needs op claims on
civilians -- W28 territory).

VERDICT (final binary, 10 paired seeds each):
- lategame: deep-invade 11 go / 57 hold -- 68 real landing
  decisions per 10 games where every prior run had ZERO -- and 106
  retargets: refused fleets pick a second beach ~10x/game. Median
  0.875 / mean 0.872 (pre-fix 0.867/0.883, in-band); conquests 53
  -> 70, consolidates 3 -> 9, cities lost 60 -> 78 (aggression up
  both ways; WATCH).
- contested B14: median 0.798 / mean 0.830 (B13 0.811/0.853,
  in-band), 2 go / 1 hold even in a low-water config, losses 26 ->
  11 with conquests 16 -> 5: the honest gates refuse marginal
  attacks in both directions. Whether that trade leaves value on
  the table is op_take_target/op_abort_floor spacing -- v4's.
- Slot health: raids capped at one slot, zero blocked player-turns.

Also this arc: benchmark hygiene -- a mid-run ninja swaps the binary
under an in-flight benchmark and taints it (one lategame run killed
and its result dir deleted for exactly this). Adopted: a bench
worktree (.worktrees/bench, own build dir) pins the binary under
test; dev continues in the main tree. run.sh --server + the
sha-keyed result cache keep provenance exact.

In flight at close of this stretch (bench worktree validating):
task #22 settler-assertion fix (pf danger maps read past an empty
segment on a dangerous START tile -- recorded in upstream-bugs.md;
deep-side fix: the civilian danger callback exempts the start tile,
which is semantically right regardless), settler_risk_tolerance
(human doctrine: a hard any-threat veto is too absolute; knob at
1.0 = stock bar until v4), the defence counter-claim epic
(findings/defence-counter-claim.md: below-bar cities claim one
defender per turn -- free pool, then released raiders, then a
muster poach priced as expected worth vs expected take), and the
posture pass (mustering armies CREDIT nearby cities via the W11
reserve formula; committed_fear_mult scales fear up when every
launched blade is beyond the horizon, 1.0 until v4).

## The score gap is ECONOMIC, not military (2026-08-01, evidence)

Human, reading the chain: "it seems poooooor that we are below 1.0."
Correct, and worth stating plainly at the top of this file's next
read: **deep has been below baseline in every cell all session** --
contested 0.78-0.83, lategame 0.86-0.88, isolated 0.85, smoke 0.89.
The whole combat arc (B9-B16) moved the number INSIDE that band. It
never crossed 1.0. Nothing in the combat workstream is going to
cross it either, and here is the evidence.

COMPONENT SPLIT, B15 contested (totals, 10 paired games, deep vs
baseline as a ratio):

  score 0.847 | cities 0.920 | pop 0.773 | techs 0.851
  bnp 0.709 | mfg 0.738 | wonders 0.667 | landarea 0.945
  munits 1.572 | unitsbuilt 1.241 | unitslost 1.719 | unitskilled 1.687

Deep fields **1.57x the standing army**, builds 1.24x the units and
loses 1.72x of them, while its economy runs 25-30% behind. Score in
this ruleset is economic; the army is the cost, not the product.

BUILD CENSUS, same run, T151 finals -- this is the whole story:

  deep P1 (2 cities):     Barracks x2, City Walls x2, Palace
  deep P5 (2 cities):     Barracks x2, City Walls x2, Palace -- and
                          TEN Phalanx
  deep P6 (3 cities):     Barracks x2, City Walls x2, Palace
  baseline P5 (2 cities): Colosseum x2, Library x2, Temple x2,
                          Granary, Marketplace, Courthouse, 2 Aqueducts
  baseline P3 (4 cities): the full ladder + Marketplace

Whole deep empires reach T151 with barracks, walls and garrisons and
NO growth ladder. This is the T153 pathology (see "satisfaction
gate", task #19) still alive at scale: the injected fear terms keep
danger nonzero, the military advisor outbids the granary every turn
forever, and the economy that scores never gets built. #19 narrowed
the window; it did not close it.

Corroborating sign, and the direction of the fix: B15 posted the
best contested numbers of the entire chain (median 0.832 / mean
0.863, two seeds >1.0, best single seed 1.188) and the thing it
added was the counter-claim -- which satisfies danger by MOVING an
existing blade instead of BUYING a new one. Cheap defence is
economic policy. That is why "count claimed relief as incoming
defence" landed immediately after: a city that has summoned help
was still requisitioning a replacement for it.

WHAT SHOULD HAPPEN NEXT, in order:

1. **The v4 tuning campaign. It has never been run.** ~40 knobs
   ship at compiled PLACEHOLDER defaults, and the ones that set the
   military/economy split are exactly the untuned ones:
   survival_fear_value, survival_target, garrison_floor(_danger),
   barbarian_fear, boat_shadow_fear, density_fear_*, overseas_fear,
   wall_fear_weight, satisfied_fear_discount. Rule 7 says tuning is
   how these get set, and rule 6 says the claim needs the numbers.
   This is the single largest untried lever in the project.
2. **An economy audit against the same census method.** Ask of every
   deep player: which ladder buildings are missing versus the
   baseline player on the same seed, and which want lost the auction
   on the turn they were skipped. W01/W02/W03 are phase-2 work whose
   payoff the military layer has been sitting on top of.
3. Combat features only after those two. The layer works -- ops
   form, march, strike, consolidate, land, retarget, relieve -- and
   its remaining questions (doomed-city triage, E5 sieges, W31 sea
   phases) are refinements on a subsystem that is not what is losing
   the game.

Stated as a falsifiable claim for the next session to test: if the
fear knobs are tuned down to where deep's army approaches baseline's
size, the economy ratios should move toward 1.0 and the score with
them. If they do not, the deficit is in W02/W03 site and building
evaluation instead, and the census in step 2 will say so.

## Counter-claim epic verdict, and the tuning machinery (2026-08-01)

B15 (counter-claim as a per-turn drip) vs B16 (relief force sized to
the wave + claimed relief counted as incoming defence), contested,
10 paired seeds each:

  B15  median 0.832 / mean 0.863  (best of the whole chain)
  B16  median 0.806 / mean 0.857

Inside the noise band at n=10 (CLAUDE.md rule 6: a 3% move over 10
games is noise), so on SCORE the sizing neither paid nor cost. On
mechanism it is plainly better behaved:

  claims        2672 -> 2006 (and every one now sized)
  relief size   1050x1, 698x2, 156x3, 72x4, 16x5, 13x6
  the sizing works: p=0.000 -> relieved 0.988; 0.122 -> 0.983;
                    0.403 -> 0.924 (it takes the city OVER the bar)
  write-offs    1472, of which 1020 had pool=0 -- no reachable blade
                existed at all, so the triage is almost never
                discarding usable help
  re-claims     2238 same-city-within-3-turns (B15) -> 392 (B16)
  brigade       7 relays fired; rare because the direct pool usually
                answers first on these maps
  op_form       65 (B13 49, B15 66): offense is NOT strangled

Human doctrine folded in this stretch: attack posture inside the
defence ledger (mustering armies CREDIT nearby cities via the W11
formula; committed_fear_mult scales fear up when every launched
blade is beyond the horizon), settler_risk_tolerance (an absolute
overnight veto is wrong -- some loss chance is affordable en route
to a very good site), and the brigade.

TUNING MACHINERY (benchmark/README.md "Two-machine tuning split"):
tune.py grew --blocks/--rounds (alternating block coordinate descent
over tuning-blocks.ini: S = fear -> military want, E = civilian
allocation, M = war allocation), --start (pin a regime; the merge
mechanism), and --evaluate (score a tuning file without optimising).
benchmark/tuning/econ-regime.ini zeroes deep's military SURCHARGE
(not stock's defence underneath) so block E can be tuned against a
stable shield budget; garden2b-150 and lategame-80 are the truncated
economy cells. The alternation table tune.py prints is itself the
experiment: a block that still gains in a later round is coupled,
which is the empirical case for landing the objective layer first
(findings/objective-layer.md).

NOT DONE, and the honest state of the headline: deep is still below
1.0 everywhere. Nothing in this session changed that, and the
component split says nothing in the combat workstream will. The next
session's work is the objective layer and then the tuning campaign,
in that order, per findings/objective-layer.md.

## The growth trap, and the campaign launch (2026-08-01, evening)

Driven almost entirely by human forensics on a live Europe game.
Three fixes, all the same defect wearing different hats, plus the
tuning campaign now running on both cloud boxes.

THE DEFECT, three times: **presence used where a probability was
available.** Stock's `urgency` and `grave_danger` (daimilitary.c) are
COUNTS -- grave_danger increments for any capture-capable enemy
within one turn's move, urgency within three -- and neither asks
whether the garrison wins. A city with five Musketeers behind walls
scores what an empty city facing ten scores. In a war they are set
on most cities most of the time: Europe T106 measured 69% of deep's
cities, 86% of stock's. Deep gates behaviour on them in three
places, and deep already computes the honest quantity
(`deep_city_stack_survival`, the chained-duel probability) for its
whole defence layer.

  1. Horizon credits skipped entirely while "threatened", so a
     capped city never funded its cap-lifter.
  2. Hence, with the gate closed nearly always: deep held ZERO
     Aqueducts across 85 cities, NO city above size 8, 19 parked
     exactly at the ceiling -- while stock (86% urgency, no gate)
     built 9 Aqueducts + 6 Sewers and ran cities to 22. RC1's own
     headline example ("an aqueduct scores ~0") reintroduced by a
     gate added to protect a *contested-cell* score.
  3. `deep_fear_satisfaction` short-circuited on the same flags,
     pre-empting its own survival test three lines below. So every
     injected fear kept bidding at full strength for the rest of any
     war, the military choice kept outbidding founders in
     daidomestic's `founder_want > choice->want`, and deep stopped
     expanding permanently -- while stock queued eight settlers in
     the same Europe turn deep queued zero. This is the "flips to
     war mode and never expands again" failure.

VERIFIED on the human's own save (T106 +45 turns, deep and classic in
one game, so classic is the in-world control):

    deep            before (T106)   after (T151)
    cities                85             89
    population           489            555   (+13.5%)
    aqueducts              0              1   (first ever)
    cities over size 8     0              1
    max city size          8             10   (ceiling broken)
    ladder buildings       0              0   (still stuck)
    settlers               0              0   (still stuck)

Classic over the same window went 96 -> 81 cities and 632 -> 600
pop. An earlier 25-turn run WITHOUT the satisfaction fix had deep
going 85 -> 76. Caveat stated plainly: one game, no matched-length
control on the old binary, so magnitudes are indicative. The ladder
(Temple/Market/Library) and settler production are NOT fixed; those
credits remain gated and `upkeep_gold_weight` still charges triple
against every building.

FORENSICS THAT DIED ON CONTACT WITH DATA (recorded so nobody redoes
them): the T76 snapshot suggested deep under-values land. It does
not. Europe is 39.9% unowned with deep sitting next to the same free
land as classic (1230 vs 1213 tiles within 8 of a city) and packing
identically (2.62 vs 2.47 mean nearest-city distance). A 7-arm site
weight probe on isolated-150 (5 seeds, stock reference on the same
maps) settled it: deep wants 3.4x the land stock does (want mass
21.5k vs 6.4k) and founds 22% FEWER cities; no site-weight setting
moved city count more than 3%. Expansion is a CONVERSION problem --
want -> settler built -> settler arrives -> city founded -- not a
valuation one. The only knob that moved it was
`settler_fearfulness_mult`, which prices the JOURNEY (133 -> 150
cities, want prevalence flat at ~28%).

CAMPAIGN RUNNING (both boxes, branch `campaign` at d0e2cbfd7b):

  EAST2  machine B  --blocks ALL,ECON,MIL --rounds 3, pop 12
                    --start europe-priors.ini
  EAST1  machine A  --blocks ECON --rounds 3, pop 10
                    --start econ-priors.ini (military zeroed)

Both: cells lategame-80 + isolated-150 + primary-120 (all with
citizen migration, mgr_distance 6), 8 seeds, gens 3, jobs 22.
Verified at launch with the AWS.md bring-up check (no-AI-type 0,
deep-def >0) -- the trap that once cost 2.5h. primary-120 timed at
221s/game, so the blend is affordable. Watch `~/campaign-{a,b}.log`;
`best-tuning.ini` is rewritten every generation.

Read at the end: the ALTERNATION TABLE on EAST2 (a block still
gaining in a later round means the blocks are coupled, which is the
empirical case for the objective layer), and the merge check in
benchmark/README.md (does machine A's economy optimum compose with
machine B's?).

Not shut down automatically -- the boxes bill until stopped.
EAST2 also holds `stash@{0}` "pre-campaign-2026-08-01 box state":
uncommitted ai/deep edits from an earlier session, preserved rather
than discarded. EAST1 was rebuilt from a full bundle (its ~/freeciv
had no .git); the old tree is `~/freeciv-old-<ts>`, 15 GB free.

**Session handoff: `doc/deep-ai/HANDOFF-2026-08-01.md`** — what is
running on the two cloud boxes (and how to stop them), what landed,
what is still broken, the next four actions, and the six traps this
session paid for.

## Two crashes, and the happiness column (2026-08-01, late)

Session opened by resuming the handoff. The campaign harvest it asked
for was not available: **EAST2 was already dead.**

### The campaign did not survive gen 1

EAST2 crashed at 20:30, ~25 min into `r0-ALL` gen 1, and idled (still
billing) until found. Not OOM, not disk -- a real crash in ai/deep.
EAST1 survived and reached gen 2 (`-40.4 -> -22.8`, so the ECON block
does have headroom on the old code shape).

**Bug 1 -- dangling target city (`79b5efc9d0`).** `deep_op_launch()`
hoisted `game_city_by_number(op->target_city)` out of the loop over
members, then called `dai_gothere()` inside it. `dai_gothere()` moves
the member, and deep takes undefended cities by walking into them; an
emptied city is destroyed, freeing the struct. Every member after the
one that won then dereferenced freed memory. **The wave's own success
corrupted the rest of its own launch.** One cause, two assertions
depending on what reoccupied the block: `same_pos()` on a null tile
(what killed EAST2) and `dai_gothere_bodyguard()` on a tile with a null
unit list. Reproduced locally on the same candidate, config and seed,
dying at the same turn (primary-120 seed 5, T102); with the fix that
game runs to completion and logs the condition once, at T103. The same
hoist existed in the `ops_reissue` remarch path and was fixed too.

**Bug 2 -- NaN from a zero weight sum (`a8cbeeddec`).**
`deep_catchment_average()` returns `vsum / wsum`. Every tile contributes
`site_overlap_weight`, `1.0`, or `site_tail_weight`. With
`site_overlap_weight = 0` and a catchment that is entirely overlapped,
`wsum` is exactly 0 and the result is `0.0/0.0` = NaN. `MAX(0, NaN)`
returns NaN rather than clamping, so it sailed through
`deep_cityresult_fill()`'s clamp into
`fc_assert_ret_val(cr->total >= 0, NULL)` -- which failed, returned
NULL, leaked the cityresult and silently dropped the site. EAST1 logged
it **148 times in one generation**; all four sampled candidates had
`site_overlap_weight = 0.000000`, a value the tuner reaches because the
range starts at 0. This does not merely add noise: candidates with a
zero overlap weight were scored on FEWER surviving sites than
candidates without one, so **the optimiser was comparing unequal
things.** Verified on the campaign's own failing triples (cand3 tuning,
primary-120 seeds 1 and 6): zero assertions after.

**Bug 3 -- one bad game killed 22 hours (`50cc52b908`).** `tune.py`
re-raised on any single game failure, so 1 crash in 288 games aborted
the whole run. Now the seed is dropped, the loss is printed, a
candidate that loses every game ranks last via `-inf`, and the
per-generation tally is printed so a run shedding games cannot look
healthy.

### The happiness column of the horizon (`622f34ca76`)

Full write-up: `findings/happiness-ladder.md`.

The handoff predicted the ladder gap would need "a NEW credit term, not
tuning". Right conclusion, wrong test -- **no knob in any block can move
happiness buildings, because their want is structurally zero.**
`advdata.c:91-100` has the whole `Make_Content`/`Force_Content` family
inside an `#if 0` TODO, so a Temple keeps `ADV_IMPR_ESTIMATE` and
`base_want()` returns 0 on its first line (`daicity.c:1529`). Its entire
want is `35 x min(amount, citizens unhappy RIGHT NOW)` -- zero in
exactly the city about to need it. That is RC1's headline defect
("an aqueduct scores ~0") in the column W03 skipped.

Unhappiness is the SOFT size cap: past it the city still grows but each
new citizen must entertain instead of work. Deep priced the hard cap and
not the soft one.

Measured, 6 paired mixed lategame-80 games (current binary, post growth
fixes) -- the one-sided-inflation signature:

    per 100 cities        baseline   deep
    CAPACITY  (credited)      3.4     8.9    deep 2.6x
    HAPPINESS (no credit)     4.0     2.1    deep 0.5x
    MULTIPLIER(no credit)     2.7     0.0    deep 0x
    specialists per city     0.46    0.75    deep +63%

Deep buys the size with its capacity credits, then parks the citizens it
bought as entertainers. `deep_content_credit()` prices the citizens a
content building frees, over the same MORT horizon and in the same
tile-sum currency as the capacity credit. Same 6 games, only
`horizon_content_weight` differing 0.0 -> 1.0:

    happiness bldgs /100c   2.1 -> 39.6      (3 -> 55 buildings)
    specialists per city   0.747 -> 0.525    (-30%)
    max city size             9 -> 15
    head-to-head score     1.047 -> 1.063

**Mechanism confirmed and large; score NOT a result** -- 1.6pp over 3
paired seeds is inside the noise floor (rule 6), and 39.6/100c against
baseline's 4.0 says the placeholder 1.0 is too hot.
`horizon_content_weight` ships as a PLACEHOLDER, registered in
tuning-params.ini and the G/ALL/ECON blocks, and NEEDS A TUNING PASS.

### Known-next, with the defect already located

The Granary term is wired backwards: `deephorizon.c` gates the
`EFT_GROWTH_FOOD` credit on `cap_credit > 0`, and `cap_credit` is
nonzero only when the city is already jammed against the hard size gate.
So a growth accelerator earns horizon credit exactly when growth is
impossible, and nothing in the freely-growing city where it pays. Not
fixed here -- deliberately not bundled with an unvalidated content
credit. Census evidence for Granary specifically is thin (deep 0 vs
baseline 1 across 295 cities); the code defect is not.

## The Granary inversion, and a tunables audit (2026-08-01, later)

`fc7073184a` — the `EFT_GROWTH_FOOD` credit was gated on
`cap_credit > 0`, and `deep_capacity_credit()` is nonzero only when
`!city_can_grow_to(size + 1)`. So the growth accelerator was paid
**exactly when growth was impossible** and paid nothing in the small,
freely-growing city where a Granary earns. `deep_growth_credit()` now
values it directly — growths within MORT (`MORT/ttg`) × the share of
the food box handed back × what a new citizen earns — returning 0 when
capped and 0 with no food surplus.

MEASURED, 5 paired mixed lategame-80 games, only `horizon_growth_weight`
differing 0.0 -> 0.5:

    per 100 cities      OFF     ON
    Granary (count)       0     12      deep built NONE before
    CAPACITY            7.9   10.1
    mean city size      5.28   5.45
    head-to-head score  1.0367 1.0399

**The mechanism landed; the score did not move.** +0.3pp over 5 paired
seeds is smaller than the content credit's already-inside-noise 1.6pp,
one seed regressed (1.087 -> 1.054) and one was identical. The
justification for this change is the inverted condition — a defect
provable by reading — NOT a demonstrated gain. `Courthouse 0 -> 5` in
the same census is noise: Courthouse has no Growth_Food effect and
nothing here should touch it.

### Tunables audit (`6180544979`)

`benchmark/tune-audit.py` cross-checks the four places a tuning field
must appear — struct member, compiled default, `TUNE_` loader macro,
`tuning-params.ini` entry, block membership — in both directions,
including type agreement. Missing any one fails SILENTLY: no loader
macro means the ini value is read and discarded (this happened once
already, `fadb2a1f70`); no registry entry means a campaign quietly
optimises a smaller space than it reports.

Current: **82 fields, 82 loader macros, 82 registry entries, all 82 in
the ALL block, no mismatches or orphans in either direction.** All
three horizon knobs are in ALL, ECON and G.

### Campaigns relaunched a second time (campaign-a3 / campaign-b3)

Both boxes at `6180544979`. Fresh dirs again, for the same reason as
before. **Trap paid for here:** killing servers BEFORE the tuner lets
it respawn a batch in the gap — 22 orphans on EAST1 and 16 on EAST2
survived into the new run and were stealing half the CPU until found by
attributing each server to its campaign via `/proc/<pid>/cwd`. Kill the
tuner first, then the servers, then verify by cwd.

## The CM war gate, and the first clean win (2026-08-01, night)

`497e313579` — a FOURTH site of "presence used where a probability was
available", found by auditing for siblings of the three defects fixed
earlier today. `deepcm.c`'s state machine switched a city to war weights
on `urgency > 0 || grave_danger > 0`. Costliest of the four: it is the
FIRST branch, so growth-capped / wonder / grow-hard were unreachable for
any city with a neighbour, and T106 measured urgency on 69% of deep's
wartime cities — so `cm_cap_*`, `cm_grow_*`, `cm_wonder_*` governed a
minority of cities while the campaign optimised them.

The threshold is a NEW knob, `cm_war_survival`, not `survival_target`.
Rationale, and it generalises: `survival_target` arbitrates ~20 defence
and operations decisions where being wrong loses a CITY; being wrong in
the city manager loses some FOOD. One scalar cannot serve both. The
split also fixed a block-assignment error — the CM is ECON work, but
`survival_target` lives in ALL/MIL/S, so machine A could never have
calibrated the gate its own block depends on.

MEASURED, 10 paired mixed lategame-80 games, two binaries differing only
in this gate (old built in `.worktrees/bench`):

                       ratio     deep     base
    presence counts   1.0026    10554    10527
    cm_war_survival   1.0343    10670    10316
                      +0.0318     +116     -211    9/10 seeds improved

**The first change today that clears the bar.** Unlike the content and
granary credits, deep's ABSOLUTE score rises, and 9/10 seeds is p ~ 0.011
under a 50/50 null.

THE THRESHOLD MATTERS, and 0.5 is not it. An earlier 5-seed run at 0.5
moved the ratio while LOSING 85 points of deep's own score, 2/5 seeds.
Human called 0.9; at 0.9 the sign of deep's own score flips positive.
0.9 ships as a PLACEHOLDER, in the tuner's reach.

**Methods warning worth more than the result:** that same 5-seed run read
the presence baseline as 1.0399; ten seeds read 1.0026. Five paired seeds
cannot baseline this cell, and a 5-seed A/B here is capable of inverting
a verdict.

### What the campaigns said before being restarted

Cleanest comparison available: candidate 0 is the START POINT, not an RNG
draw, so it is identical across runs while everything else shifts (the
param list grew 42->43 and 81->82, moving the RNG stream — gen-0-BEST
figures are therefore NOT comparable and should not be quoted).

    machine A (ECON, econ-priors)     cand0  -57.3 -> -28.3   -49%
    machine B (ALL,  europe-priors)   cand0 -110.1 -> -87.8   -20%

24 games per candidate, same seeds and configs, only the code differing.
The deficit halved; it did not close — the objective is still negative,
so deep remains behind baseline on these blends at these priors. It also
bundles every 2026-08-01 change, so no single one can claim it.

### Start files reseeded (`458d66194f`)

`econ-priors.ini` / `europe-priors.ini` now carry each machine's best
gen-0 candidate, so restarting for the code change keeps the search
progress. Both headers state plainly that these are the best of TEN
RANDOM DRAWS in ONE generation, subject to winner's curse, and are not
to be cited. `fold-tuning.py` was deliberately NOT used — it stamps
defaults "tuned <date>", which would put a false claim in the source.

## The multiplier column, and three credits that do not score (2026-08-01, third session)

`c6d39b6081` — the last uncredited ladder column. Marketplace/Library/
Bank/Factory are `ADV_IMPR_CALCULATE` so `base_want()` runs, but
`dai_city_want()` prices them on `acity->citizen_base` — today's tiles —
while every other credit in `deephorizon.c` buys tomorrow's citizens.

**The cell had to change first.** `lategame-80`, where every ladder
decision of the day was made, gives baseline ONE Marketplace across 52
cities: the column is invisible there by construction. `long` (250
turns) gives it 114. `benchmark/ladder.py` (`3a0fa3b2d4`) does the
census by `ai_type` because `buildmix.py` only matches P1/P2 naming.

MEASURED, 20 paired `long` games per arm, train seeds 1-10, one binary
and two tuning files (verified an exact no-op at weight 0). Seed 3's
`mixed_db` segfaulted → 9 paired seeds:

    pooled ratio    OFF 0.8638   ON 0.9213   delta +0.0575
    mean per-seed +0.0727 (sd 0.1958)   seeds improved 5/9
    deep score  10858 -> 11014  +1.4%    base score 12570 -> 11955  -4.9%
    deep techs   1440 ->  1453  +0.9%    deep literacy 7035 -> 23595 +235%

    per 100 cities, deep    OFF     ON
    MULTIPLIER             10.7   23.3
      Library               4.2   16.0
      Marketplace           5.1    5.1   unchanged
      Bank                  0.8    0.8   unchanged

**No score claim.** 5/9 is a coin flip, t≈1.11 (p≈0.30), and the ratio
moved mostly because baseline FELL. The pre-registered falsification
test failed: literacy tripled, techs moved 0.9%. Landed on the defect,
as `fc7073184a` was.

The column closed entirely on the SCIENCE side — the `adv->*_priority`
routing declining to buy gold multipliers for an empire with no use for
gold. Marketplace is bit-identical between arms (26 buildings both).

**THE PATTERN IS NOW THE RESULT: three credits in a row (content,
granary, multiplier) whose mechanism lands decisively and whose score
does not.** Full argument in `findings/multiplier-column.md` §5 —
including a retracted claim that the per-column architecture is wrong
(it used baseline's build mix as the reference, which RC1 says is
exactly the broken thing).

### The T80 forensic: deep does not expand

`findings/europe-T80.md`. Deep_Carthago: 17 cities, room for ~18 more
within 15 tiles on uncontested land, **zero settlers for 8 straight
turns**, 5 Musketeers built in the same span. Every recent epic has
inflated what settlers compete against (defenders get +30 flat
`barbarian_fear` alone; buildings now have five credits) while settlers
gained one ≤2x multiplier. Cheap test specified: log `founder_want`
beside the chosen build — near-zero means a site-scoring bug, healthy
but losing means arbitration.

Also found there: `deep-def` logs PRE-injection danger (misleading for
diagnosis), and `rotation_power_ratio < 1.0` makes the rotation gate
trivially true — the registry range starts at 0.0 and campaign-a4 has
sampled 0.54–0.96.

### Crash: `deepops.c` MARCH block still unguarded

`d5ce7cecfb` fixed the post-launch target re-resolve and **overclaims**
— two core dumps show `deep_op_drive()`'s MARCH block dereferencing the
entry hoist, once as a freed pointer (17:41, pre-fix binary) and once
as NULL (18:55). Same function, same block. Entry guard still needed.

### Campaigns

`campaign-a4` / `campaign-b4` bumped 22 → 30 jobs at a generation
boundary; both resumed bit-exact (A replayed +21.6/+18.9, B replayed
−2.8/+10.2 from cache). Training-seed objectives only — not results.

## Expansion was switched off by a stale counter (2026-08-01, night 2)

**The session's real result, and it was not the assigned epic.**

`b2c8952693` — stock gates settler AND worker planning on
`dai_on_war_footing()`, TRUE if ANY player carries a war countdown >= 0.
That counter is set by `war_countdown()` and not reliably cleared. While
it holds, every city with any military want has `founder_want` and
`worker_want` forced to zero and SKIPS contemplation entirely.

Measured live (Europe T125-126, instrumented build): **16 of
Deep_Carthago's 18 cities never reached contemplation**, alone on a
624-tile island with 180 workable unclaimed tiles. After the fix,
`founder_want > 0` in **17 of 18** (411-733). The one still blocked is
danger 1083 / survival 0.598 — correct.

Two hypotheses were killed by experiment first, both the human's:
- **production change penalty** — zeroing `shield_stock` in all 218
  cities of the save and replaying the turn changed NOTHING.
- **`site_rival_pressure`** — contributes exactly 0 there; Carthage's
  continent has no non-allied city.

FIFTH instance of *presence count where a probability exists*. The gate
now asks whether THIS city is in danger. `expansion_war_survival`
PLACEHOLDER 0.9; 0 restores stock.

**The `dai_sanity_check` assertion recorded as harmless in
`upstream-bugs.md` #30 is the visible symptom of this same state.** It
was dismissed as unrelated earlier in the session; it was not.

### The horizon credits are SCORE-INERT (10 paired seeds)

All seven `horizon_*` weights at 0 vs shipped, primary-120, 20 games per
arm, 51,379 tripwire rows:

    arm          %Settlers  %other  cities/plr  pooled ratio  deep score
    credits ON      29%       40%      16.3        1.1383        8141
    credits OFF     42%       26%      18.8        1.1186        8121
    mean delta -0.0252 (SE 0.0473), 4/10 seeds

Build queue swings hard, score does not. **A 4-seed version read +0.13
and +9% and did NOT replicate — retracted.** Five commits of credit
machinery that reliably move mechanism and never move outcome.

### Also landed

- `c6d39b6081` multiplier credit (the assigned epic). Column closed
  10.7 -> 23.3 per 100 cities, ALL of it Library/University; Marketplace
  bit-identical. No score claim: 5/9 seeds, and literacy +235% with
  techs +0.9% failed the pre-registered falsification test.
- `d5ce7cecfb` + `8ccbc7ba34` third use-after-free, `deep_op_drive`'s
  MARCH block. Verified: the game that segfaulted at T156 now completes.
- `2674ac74b4` PERFECTION split into named tunables. It is documented as
  a SEARCH-EFFORT bound and was also a x3 travel discount and (via
  integer `PERFECTION/2`) a x1.5 yield multiplier. 3-seed sweep: d=1.0
  gave cities/player 18.3 -> 27.9. NOT a result at 3 seeds.
- `ecb5acc20a` trade-route destinations on merit — stock switched
  transit-time policy on a continent test and re-rolled `fc_rand(2)` per
  unit to decide whether foreign cities were candidates at all.
- `c1ebc4607d` ally briefings: op launches/cancellations, caravan
  departures (aggregated, one line per turn), war-countdown retraction.
- `88dfa15597` explore want no longer pays to reveal a map deep already
  plans through omnisciently (`huts_vis=0, unknown=6461` in the wild).

### FIRST HELD-OUT NUMBER

campaign-a4 best: **holdout mean paired diff +16.8 over 8 seeds**, three
cells, seeds 101-108. Training objective was +39.4 — the winner's curse
took ~57% and the sign survived. Quote +16.8, never +39.4.

### Campaigns

`campaign-a6` (ECON x3) and `campaign-b6` (ALL,ECON,MIL x2), both at
`b2c8952693`, 93 params, 30 jobs, auto-shutdown armed: each waits for
its tuner to exit, runs `--report-holdout` unattended, then stops the
instance. Shutdown behaviour verified `stop`, so EBS survives —
**never terminate these, DeleteOnTermination=True.**

Start files: `econ-priors-a5.ini` (a4 harvest + 9 new),
`europe-priors-b5.ini` (average of a4 and b4 harvests + 9 new).
`deep-best.ini` refreshed to 93 params for playing.

### Method lessons that cost real time

- **Read the map topology before computing on it.** Europe sets
  `wrap=""`. Five successive land measurements were wrong because the
  analysis scripts used `(x+dx) % xsize`. `ai/deep` was audited and is
  clean — no hand-rolled coordinates anywhere.
- **4-5 seeds manufacture effects the size we hunt.** Two separate
  4-seed results this session failed to replicate at 10.
- **Every benchmark gets a pinned binary via `--server`.** Rebuilding
  into a live run happened TWICE despite the rule being written down.
- Debug worktrees need `-Daimodules=classic,tex,baseline,deep`,
  `FREECIV_DATA_PATH`, and `set timeout -1` or the server idles forever.
- `Monitor` runs sandboxed with no network; ssh watchers must be
  background Bash with the sandbox disabled.

## The AI profile at scale — assess_danger is 80% of it (2026-08-02, night 3)

**The assigned epic was W20 threading. It is not threading, and this
says why.** `findings/scaling-profile.md` is the deliverable.

W20 was checked before anything else and is blocked. `00-context.md` §7
requires W26 first, and W26 is genuinely unstarted — no STATUS mention,
no workstream file, and all four blockers Part 22 names present verbatim
(`cm.c:915` comparator statics, `citymap.c:53` shared array,
`rand.c:85` global RNG, the requirements mutex). It is not procedural:
`ai/deep` delegates its building-want and danger paths to `ai/default`
(`deepai.c:330`, `deepdefence.c:1789`), so the impure functions are
exactly the ones threading would need to run concurrently. W20 has no
surface to attack. `workstreams/W20-threading.md` (`915d32ec87`) now
records this so it does not get re-derived.

`scaling.md` §1 also forbids threading before the algorithmic collapse.
So the epic became `threading-obstacles.md` §7 steps 1 and 2 — delete
the dead mutex, then profile — which is what those documents ask for
and what everything downstream is chosen by.

### −9.5% of all AI CPU for deleting dead code (`d678b53718`)

`is_req_active()` took a global mutex and a linear list scan on every
requirement evaluation to build a `trdata` it never reads. Part 22 §1
hedged that its cost was a hypothesis. Measured, `scale-20p-s6` seed 1,
one binary apart, each game run ALONE and sequentially:

    pair 1   mutex 280.5s   no mutex 254.8s   -9.2%
    pair 2   mutex 279.6s   no mutex 252.2s   -9.8%
    same-binary repeat spread 0.3% and 1.0%

Behaviour-neutral and checked, not argued: the final savegames agree on
all **53,033 lines** of state; only `phase_seconds`, the turn-change
time, and the timing rows themselves differ. Per bucket the saving lands
exactly where requirement evaluation lives — Tech −55%, Citizen arrange
−51%, Government −50%, Buildings −33% — while `AIT_DANGER`, which
dominates runtime but is geometry, moves −1.2%.

### The profile, and what it reverses

`scale-20p`, 20 players, 140x90, T120. AI CPU is ~88% of wall.

    turn  cities units  AI s/turn  DANGER   share  BUILDINGS
     20    226    367     5.50      3.05     55%     0.074
     60    243    937    13.53     10.64     79%     0.049
    120    256   1352    22.44     17.90     80%     0.057

`assess_danger` (`daimilitary.c:709`, not `:827` — tree has moved) is
**75–80% of all AI CPU, slope 2.49** against city count. Cities +13%
across the run while units 3.7x and danger cost 5.9x: `O(own_cities ×
all_units)` with the city term pinned.

`base_want` is **not a measurable cost at any cell tried** — 0.5% at
20 players, 2.0% at 4 players (~19 cities/player), every slope ≈1 not
≈2. `AIT_BUILDINGS` does cover it (`building_advisor()` runs inside it).
**Caveat stated plainly: no cell reached the 100 cities/player Part 17
argues from; the most any player owned was 19.** So this does not
disprove the analysis — but a quadratic does not hide by being small.

**Next epic is the spatial threat index, NOT the base_want collapse.**
The collapse is a modelling improvement (separating "is this wonder
worth building" from "where") and belongs to W01/W03 on merit; it is not
a performance workstream. Note the threat index needs neither purity nor
threading — it is a per-turn read-only structure.

### Harness added

- `-Daitiming=true` (`f7a967a98b`): AI timing buckets at full
  optimisation, without FREECIV_DEBUG's `-O0`. **Both build dirs in this
  tree were `optimization=0` — no performance number this project has
  taken was measured on an optimised build.**
- Per-turn bucket + empire-size dump (`ef150343ce`); `benchmark/profile.py`
  reports per-bucket share and log-log slope vs city count.
- `scale-20p`, `-s3`, `-s6` (`f1b4f0cd39`) and `scale-4p` (`668307296e`).
  Not scoring cells — nothing paired.

### Method lessons that cost real time

- **Never run profiling games concurrently.** Four at once inflated each
  other's CPU time +60% and inverted the mutex A/B. Everything here is
  sequential and exclusive.
- **`startunits` is capped at 21 chars.** Longer is rejected with one log
  line and the setting stays at the ruleset default `cwsx` — ONE settler.
  A 24-settler cell ran to completion looking healthy and gave 31 cities
  across four players. Check city counts, not exit status.
- **`LOG_TIMERS` is defined unconditionally in `utility/timing.h`.**
  Gating on it gates on nothing; a per-turn dump there would have changed
  the logs every existing benchmark reads.
- One run per arm cannot resolve a 9% effect. The same-binary repeats are
  what establish the noise floor and they are cheap.
- CLAUDE.md's `./build/tests/unit_tests` does not exist; the suite is
  `meson test -C build` (`deepodds`, `baseline-frozen`). Both pass.

## Site scoring cannot express spacing (2026-08-02, day)

**Two hypotheses died with measurements; the third is a design.**
`workstreams/W02b-catchment-stream.md` is the deliverable.

### Crowding scaling: null (`a79c85642e`, `6f33d8c1c4`)

`site_overlap_weight` and `site_rival_pressure` made crowding-aware
via a new non-saturating `deep_land_pressure()` (kept separate from
`deep_map_crowding()`, which four tuned consumers depend on). 2x2 on
lategame-80, 10 paired seeds, a6 and b6 tunings x crowding off/on:

    a6 ON-OFF  score +10.4 t=0.87 6/10 | cities +0.6 t=0.34 4/10
    b6 ON-OFF  score  -1.1 t=-.18 4/10 | cities +0.1 t=0.29 3/10

**The mechanism did not move.** Settlers sat at 26.1 / 25.6 / 25.3 /
25.8 across all four arms against baseline's 40.8 — a quantity
invariant to two tunings differing 7x on overlap and 3.6x on rival
pressure, plus a x12.4 intervention. Site scoring is not the
constraint; **arbitration is** (`europe-T80`).

### Why spacing is not tunable at all

`deep_catchment_average()` returns `vsum/wsum`, a weighted AVERAGE.
Overlapped tiles are down-weighted in numerator AND denominator, so at
low weights they drop OUT rather than counting against. Measured on
largepox-200 (380 land/player, 5 seeds):

    site_overlap_weight  0.05   0.01   0.002
    tiles/city           16.4   16.4    16.6

25x change, 0.2 tiles. **16.4 is a floor no weight can pass.** Site
score sees tile quality and is blind to tile quantity.

### Corrections to my own reasoning, recorded

- The largepox-200 probe that prompted "only smallpox is reachable"
  ran with `site_overlap_crowding=16` active, i.e. MY default making
  packing 6.5x more attractive. Conclusion retracted.
- Empire-size unhappiness costs OUTPUT, not score:
  `total_player_citizens()` sums unhappy and angry too. Going past
  empire size is usually right, and for deep especially — it runs 33
  temples per 100 cities against baseline's 11.

### Campaigns harvested; both watchdogs had died

    a6  training +76.1 -> holdout +62.2  (curse -18%)
    b6  training +94.5 -> holdout +58.8  (curse -38%)

A tie on the blend. Tiebreak by direct measurement on lategame-80, 10
paired seeds: **a6 0.9897, b6 1.0394** — a6 is BEHIND baseline on the
crowded cell. `deep-best.ini` rebuilt from b6 (`86cce1a729`), was
still the a4/b4 blend at 93 params.

**Both auto-shutdown watchdogs were dead** (bare background shells, no
systemd unit, no crontab). a6 idled 80 minutes after finishing. Both
holdouts were run by hand. Re-arm as the systemd oneshot AWS.md
already prescribes before the next campaign.

### The culture problem, now four cells

Deep's score advantage is majority-**culture** in every venue measured:

    isolated-150   62% of gain     lategame-80 (b6)  +2693 raw
    garden2b       +104 of +73     largepox-200      +76 of +62

Material position is flat or behind in all four (bnp -8.1% on
largepox-200, mfg -45% on garden2b). Culture is 11% of score; citizens
+ techs are 82%. **Deep is winning the small term.**

### Also landed

- `f7a967a98b`..`1049a5f869` the profiling epic — see the entry above.
- `86cce1a729` `primary-120-classic`, the RULESET GUARD. All three
  tuning cells were civ2civ3, so all 96 constants were conditioned on
  one ruleset. Settlers `pop_cost` is 2 there and 1 in classic.
- `0fa86752f5` upstream-bugs #31: no AI ever performs Join City,
  though `deepsite.c:610` already carries the hook comment.
- `581a124c7e` `mixed_XY` arms, so deep can face ai/default directly.
- garden2b RETIRED for head-to-head: 10 mirrored pairs, seat-averaged
  mean -138, sd 710. 6 of 14 games ended with one side holding >5x the
  other's cities — it is a conquest lottery, not a development race.

### Open, ranked

1. **W02b first epic** — the stream, `ttg(k)`, counterfactual overlap.
2. **Build arbitration** — settlers pinned at 26; `europe-T80`.
3. `site_overlap_crowding`/`site_rival_crowding` default 16.0/0.9 in
   code and measured flat. `deep-best.ini` zeroes them; the code
   defaults still need a decision.

## W02b landed and was falsified; spacing is citymindist (2026-08-02, evening)

**The epic's central claim is wrong, the falsification was
pre-registered, and the head-to-head found something bigger.**

### The stream landed and moved nothing

`deep_catchment_average()` (a weighted average) is replaced by
`deep_catchment_stream()`: each catchment tile enters at
`amortize(yield, arrival turn)`, arrival turns from a real growth
simulation (`city_granary_size()` foodbox, ruleset size gate, no
horizon constant -- `amortize` is the discount). Tiles a neighbouring
own city reaches FIRST are excluded outright rather than weighted.

    ee03cc4840  growth simulation replaces the target-size cutoff
    2a5f8bc438  the stream replaces the average
    e9de4f704f  counterfactual overlap exclusion
    84df35bf28  travel discount 0.9, range [0.75, 1.0], human-set
    c977682d44  play file: deleted keys removed, travel raised
    cab01f2614  REVERT of the drop-stuck fix (see below)

Four tuned constants deleted (`site_tail_weight`, `site_depth_weight`,
`site_overlap_weight`, `site_overlap_crowding`), one added
(`site_stream_weight`, placeholder 1.0). tune-audit CLEAN 93/93/93/93.

**Result: nothing.** deep pre vs post, 10 paired seeds, pinned binaries:

    cell            land/plr   land/city pre -> post   score diff (t)
    lategame-80           80     11.47 -> 11.54        -1.6  (-0.43)
    largepox-200         380     11.54 -> 11.61       -14.0  (-0.73)
    primary-120          400     13.89 -> 13.90        +2.2  (+0.32)

Pre-registered **F1 fires**: the spread between the crowded and roomy
cell was 0.07 tiles before and 0.07 after. P1 predicted >= 2.0.

### Why: spacing is set by the ruleset, not by the objective

Nearest own-city distance, SELF-FOUNDED cities only (captured excluded):

    pre-largepox-200    n=572   29.4% exactly at citymindist   86.7% <= 4   median 3
    post3-largepox-200  n=646   27.4% exactly at citymindist   84.5% <= 4   median 3
    pre-lategame-80     n=200   25.5% exactly at citymindist   80.0% <= 4   median 3

`citymindist` is 3 and forbids founding within chebyshev 2. **The
median founded city sits exactly on the floor**, at 80 land/player and
at 380 alike. Site scoring can only speak for the minority of cities
not on the constraint, which is why a 25x sweep of the old overlap
weight moved spacing 0.2 tiles and replacing the whole valuation moved
it 0.07.

### The drop-stuck fix was landed and reverted on measurement

52% of settler mission-turns on lategame-80 are a unit standing on the
tile it stood on last turn while holding a city mission (21% on
primary-120; 73% for the player at war with three neighbours, 3-6% for
its peaceful neighbours). Cause: deep sets `is_pos_dangerous` on the
settler pathfinder while `goto_is_sane()` knows nothing about danger,
so a mission whose every route is vetoed still looks healthy and
nothing calls it off (`findings/settler-missions.md`).

The fix -- drop it and re-search, mirroring `drop-boatless` -- FAILED:

                       mission-turns  stationary   drop-stuck   picks
    pre  (no fix)             10170   5275 (52%)            0    1978
    post (with fix)           14757   9145 (62%)         5070    7152

3958 of 4940 re-picks (80%) chose the tile just dropped, because the
search applies the SAME risk parameter as the mover. Reverted.

### The head-to-head, and why deep loses the crowded map

deep vs ai/default seated in the SAME game, mirrored seats, 10 seeds:

                   lategame-80 (80 land)   primary-120 (400 land)
    cities            -2.3  t=-3.89           +3.5  t= 2.00
    citizens          -4.4  t=-1.56          +17.6  t= 3.25
    bnp               -9.0  t=-2.52          +15.9  t= 2.99
    mfg              -11.3  t=-3.13          +11.0  t= 2.46
    culture/50        +6.4  t= 4.89           +9.9  t= 6.63
    score             -2.9  t=-0.78          +34.4  t= 4.53

Score margin by turn tells the real story:

    lategame-80   T20 +1.3  T40 +7.7  T60 +8.9  T80 -1.7
    primary-120   T20 +0.0  T40 +1.9  T60 +9.2  T80 +23.1  T120 +37.7

**Deep out-develops early and then STOPS EXPANDING.** Cities by turn on
lategame-80, deep/default: T60 7.7/8.6, T70 7.8/9.3, T80 7.8/9.9. Deep
adds 0.1 cities in the last 20 turns while default adds 1.3. It is not
being conquered -- city count never falls and unitslost is comparable
(17.9 vs 16.8). It simply stops founding, and each city default adds is
citizens and techs.

That is europe-T80's "17 cities, room for 18 more, zero settlers for
eight turns" reproduced under measurement, and the frozen settlers
above are its mechanism. **The stall, not site scoring, is what loses
the crowded map.**

### Corrections to my own claims, recorded

- "Deep wins the scoreboard but loses the map" was true of lategame-80
  and WRONG as a general claim: on primary-120 deep beats ai/default
  head-to-head on every material term. Culture is 9.9 of a +34.4
  margin there, not the whole of it. The culture puzzle is a
  crowded-cell artefact, not a project-wide one.
- Separate-game score and head-to-head AGREE in direction on both
  cells, so the primary metric is not obviously broken. The earlier
  suspicion that 93 constants were tuned against the wrong objective is
  not supported by these two cells.

### Also this session

- `findings/settler-missions.md`: the freeze, the 40-turn tail, and
  deep building 102 dedicated worker units where baseline builds 677
  (repurposed founders satisfy the worker demand test at pop_cost 2).
- Both campaign boxes STOPPED (verified idle, load 0.00, no freeciv
  processes, up 25h). Not terminated; EBS caches preserved.

### Open, ranked

1. **The expansion stall on contested maps.** Instrument why
   `gothere()` fails when search and mover share a veto function, then
   make the SEARCH stop proposing targets the mover refuses (RC5).
   This is the one that loses games.
2. **Is lategame-80 a good tuning cell?** 12 starting settlers,
   techlevel 50, 80 turns, 80 land/player. Three of the four campaign
   cells are crowded, and deep's stall is worst exactly there.
3. `site_stream_weight` is untuned at 1.0 and the site block of
   deep-best.ini is no longer tuned at all.
4. W02b's remaining commits (settler cost from blocked cities, Join
   City) are unaffected by the falsification -- they are about settler
   SUPPLY, not site ranking.

## The expansion stall was settlers walking to islands (2026-08-02, night)

**Instrumented first, then fixed. deep now beats `ai/default`
head-to-head on `lategame-80`, the cell it was losing.**

    87ecdc8f55  log why a stalled settler mission made no progress
    458cf07945  name the constraint that refuses the route
    0f126c33b7  decide a settler's boat leg with the mover's own knowledge
    405b78a596  upstream-bugs #32
    ca27cab372  findings/settler-missions.md section 7

### What it was

Every mission-turn now logs an outcome and, when the risk map has no
route, a ladder that relaxes one pf constraint at a time and names the
first relaxation that brings the route back. `lategame-80`, 10 train
seeds, deep arm, 11215 mission moves:

    reason     count   share        nopath, why    count   share
    nopath      6323   56.4%        vision          5659   89.5%
    moved       2810   25.1%        behaviour        647   10.2%
    arrived     1062    9.5%        danger            17    0.3%
    ferry       1017    9.1%
    blocked        3    0.0%

`wait` and `blocked` are ~0, so the mover was never refusing a step on
a route that existed -- the route did not exist. Joining each stall to
the pick that made the mission: **5656 of the 5659 `vision` cases are
missions the search itself marked `overseas=1`.**

`deep_settler_gothere()` asked `goto_is_sane()` whether the trip needed
a boat. That function pathfinds NON-omnisciently, so unexplored water
reads as passable and an island target reports a good land route. The
ferry branch was skipped, the omniscient risk map found no land path,
and the settler stood on the beach for the rest of the game holding a
valid mission. RC5 exactly: planning and execution asked different
pathfinders the same question. It also disabled the throttle built for
this -- a settler that never reaches `aiferry_gobyboat()` is never
counted as a passenger, so overseas picks were never capped.

The war-zone reading in `findings/settler-missions.md` §3 -- the one
the reverted `6ab78f8d98` was built on -- was wrong. Danger is 0.3%.

### What the fix moved

`deep_settler_can_walk_there()` is `goto_is_sane()`'s test at the
player's real handicap level, computed once per `gothere()` and used
for the boat gate, the walk gate and the bodyguard gate.

deep arm, same 10 seeds, pinned binaries either side:

    lategame-80             pre      post      diff        t
    score                183.82    189.22     +5.40     2.60
    cities                 8.27      8.68     +0.42     2.91
    pop                   45.05     48.87     +3.82     4.83
    landarea              95067     99917     +4850     3.37
    live settlers at end   4.47      2.73     -1.73    -3.97

    mission-turns 11527 -> 7718   picks 1964 -> 1574   nopath 6323 -> 529

    picks land      644 -> 655   arrived 460 -> 461   (71% -> 70%)
    picks overseas 1320 -> 919   arrived  91 -> 114   ( 7% -> 12%)

Land expansion is untouched; the cities come from overseas settlers
finally being delivered. The reverted attempt went the other way on
picks (1978 -> 7152).

### Head-to-head, mirrored `mixed_dc`/`mixed_cd`, 10 seeds

    turn      pre     post    delta        difference-in-differences
    T20     +1.30    +1.70    +0.40        at T80, paired over seeds:
    T40     +7.67   +10.87    +3.20        score  +11.22  t=2.09  7/10
    T60     +9.21   +16.12    +6.91        cities  +1.62  t=2.87  8/10
    T70     +5.25   +12.75    +7.50        pop     +7.27  t=2.59  9/10
    T80     -1.49    +9.73   +11.22        bnp    +10.11  t=3.69  9/10

The control reproduces last session's reference series to within
rounding, so the instrumentation is behaviour-neutral. The delta is ~0
through T30 and grows monotonically from T40 -- the window where the
stall lived.

### Cross-cell check: `primary-120`, deep arm, 10 paired seeds

    metric                pre       post      diff       t
    score              157.47     158.33     +0.87    0.29
    cities              21.70      22.65     +0.95    1.36
    landarea           299367     310150    +10783    1.26
    live settlers        12.55      10.33     -2.22   -2.06

    nopath 25.9% -> 7.1% of mission moves; same mechanism (vision 74.9%)

The roomy cell has the same defect at half the rate and gets the same
repair, with score flat rather than up. Directionally positive on every
expansion term, significant on none of them at 10 seeds -- so the fix
is worth its keep on the crowded cell and costs nothing on the roomy
one. **No cross-cell regression, which is what this run was for.**

### Pre-registration, scored

D1 held. **D2 passed its own criterion and was wrong about the
mechanism** -- it blamed territory, which is 10%; the ladder found the
cause only because it relaxed every constraint rather than the two I
suspected. D3 held. F1 held on the narrow stationary definition (56.4%
-> 7.2%) and **failed on the honest one**: counting settlers waiting on
a lift, no-movement mission-turns are 65.5% -> 39.6%. F2 held. F3
narrowly missed (+0.42 cities against a predicted +0.5). F4 held.

### Open, ranked

1. **Overseas delivery is 12%.** Nine of ten settlers that commit to a
   boat trip still never arrive, and 2398 of 7390 mission moves are a
   settler waiting on a lift. That is the ferry pipeline, and it is now
   the largest single loss in this subsystem.
2. **`behaviour` stalls, 523 turns** -- territory or a unit closing the
   corridor mid-walk, with no re-search. This is what §5.1 of the
   findings wanted, aimed at a case a tenth the size it was thought to
   be.
3. `largepox-200` and the head-to-head on `primary-120` are unmeasured
   for this fix. The two cells that were measured agree.
4. Unchanged from last session: is `lategame-80` a good tuning cell;
   `site_stream_weight` untuned at 1.0; W02b's remaining commits
   (settler cost from blocked cities, Join City).

## The ferry lift: diagnosed, two repairs refused (2026-08-02, late night)

**Continuation of the same session at the human's direction. Nothing
shipped but instrumentation and the measurements; both candidate fixes
were built, measured and reverted.**

    fae8a0f778  instrument the ferry rendezvous from both sides
    75664db56e  trim the probes to the line worth keeping
    e2171af6d2  findings section 8 + upstream-bugs #33
    16acdd9712  the 12% was mostly double-counting

### It is not boat supply, and it is not the settler

`lategame-80`, 10 train seeds. Of 2398 settler waiting-turns, **95% had
a boat that stock's own `is_boat_free()` says would carry that
settler**, and 92% of those were 2-8 tiles away -- outside the one-tile
radius `aiferry_find_boat_nearby()` searches, because the boat is meant
to come. Only 5% were "no boat anywhere".

### Why the boat does not come

Of 6076 idle boat-turns, 778 had a flagged passenger the boat's own
cargo-search map could reach. `aiferry_findcargo()`'s scan was
reproduced exactly from the deep side and **found the waiting settler
every time** -- so the scan is fine and is never run. A predicate
bitmask named the branch: `dai_manage_unit()` dispatches on
worker/cityfounder BEFORE ferry, and civ2civ3's **Transport** (role
Ferryboat, capacity 8, the colonisation boat) carries the `Workers`
flag, so it is handed to `dai_manage_settler()`, parked on
`SSA_AUTOWORKER` and never reaches the ferry code in its life.
Galleons are unaffected. `upstream-bugs.md` #33.

### Repair 1, refused: route those boats to the ferry code

    Transport boat-turns carrying   488 -> 196
    overseas delivery                12% ->  11%
    score  +1.20 (t=0.59)   cities  +0.00 (t=0.00)

Freed from auto-worker duty the Transport runs stock's ferry manager,
which makes it available, fails to pick a passenger-in-charge and parks
it -- where before it at least drifted and was boarded opportunistically.

### Repair 2, refused: stop deep destroying its own boat request

Tracing every mission pick to its terminal event: **66% of "failed"
overseas missions are the same settlers re-picking.** deep's
`drop-boatless` test is `unit_ai.ferryboat <= 0`, and `FERRY_WANTED` is
-1, so a settler that had just successfully asked for a boat had its
mission dropped that same turn -- and `dai_unit_new_task()` called
`aiferry_clear_boat()`, taking the flag off and decrementing
`stats.passengers`. The request never survived to the boat pass, which
is exactly why 77% of idle boat-turns saw nobody flagged.

    overseas picks   919 -> 406   arrivals 114 -> 115   (12% -> 28%)
    ferry-wait turns       2398 -> 3312
    score  -1.50 (t=-0.99)   cities  -0.13 (t=-0.77)

The delivery *rate* doubles because the denominator was inflated. The
**absolute overseas city count does not move**, 114 to 115. And it is
slightly negative overall: the churn was a crude escape hatch, and a
settler that waits honestly sits in a queue nothing serves.

### Corrections to my own earlier claims

- "12% of overseas settlers arrive, recover the other 88%" was wrong as
  a size estimate. Two thirds of the denominator was double-counted
  re-picks. The real overseas yield is ~11 cities per game and the
  headroom is smaller than the ratio suggested.
- The §7 epic's numbers are unaffected -- they were measured on cities
  and score, not on this ratio.

### Open, ranked

1. **The lift as an operation.** A named boat, a named settler, a named
   beachhead, held against re-tasking, with an abort condition, in
   `deepops.c` -- which already has holds that survive
   `dai_manage_units()` clearing `done`. Repair 2 is a prerequisite to
   land WITH it, not before it.
2. Size it first. ~11 overseas cities per game today; a perfect lift is
   worth some fraction of the ~1.3 cities/player gap that remains in
   the head-to-head, not all of it.
3. Unchanged: `behaviour` stalls (523 turns), is `lategame-80` a good
   tuning cell, `site_stream_weight` untuned, W02b's remaining commits.

## Idle units: both human calls built, one inert, one costs land (2026-08-03)

    eba2b77b04  74% of worker-capable units are parked, and no fallback
    df9309d5ad  give an idle worker the soonest legal improvement
    5933c431d0  hand a settler back to a city when there is nowhere to found

### What was found first

Human question: *do workers do anything if it isn't demanded?* No.
`worker_evaluate_improvements()` enumerates only the union of city work
radii; the want is a delta in tile value at today's city size floored
at zero; and when nothing scores, an AI player gets no fallback at all
-- humans reach `adv_seek_shelter()`, but `auto_workers_player()`
branches on `is_ai()` and calls the `settler_run` hook instead. Final
save census, `lategame-80`, 10 games: **475 of 645 worker-capable units
idle**, and 100% of idle dedicated workers had no goto destination, so
they were parked and not in transit.

### The worker fallback: shipped, inert

2267 assignments on `lategame-80` and `bnp -0.02`, `mfg +0.00`; 20
assignments on `primary-120` and everything inside noise. The declined
improvements really are worthless there -- cities average size 5.8 and
never work the outer tiles. Landed as doctrine on the human call,
recorded as neutral.

### The idle pool is mostly a `lategame-80` artefact

    cell           idle unit-turns/10g   cities/plr   idle per city-turn
    lategame-80              12273           8.68          0.2946
    primary-120                429          22.65          0.0026

113x. Third independent reason to question `lategame-80` as a tuning
cell, after twelve starting settlers and the stall being worst there.

### Join City: objective met, land lost

    settlers alive at end   2.73 ->  0.32   t=-8.87   joins 397
    cities                  8.68 ->  8.47   t=-1.57
    landarea               99917 -> 97117   t=-2.35
    score                 189.22 -> 187.77  t=-0.60

`landarea` is the only significant term and it is negative. Tightening
the trigger to "no site at all" was measured on a separate binary and
is byte-identical, so the cost is intrinsic: joining is irreversible
and a settler with no site today sometimes has one later.

**Open, and the next thing to do here:** join only after N consecutive
site-less turns. N is a tunable -- `struct tuning` with a placeholder
and a tuning pass, not a guess.

## Session close: one win, three refusals, and an unmeasured stack (2026-08-03)

### What actually improved the AI

**Settlers were walking to islands.** `goto_is_sane()` pathfinds
non-omnisciently, so unmapped water read as passable and the ferry
branch was skipped: 5656 of 6323 stalled settler mission-turns were
overseas missions being walked to. Fixed at `0f126c33b7`.

    lategame-80 head-to-head vs ai/default, T80 score margin
      -1.49 -> +9.73   difference-in-differences t=2.09, up 7/10
      cities +1.62 t=2.87, pop +7.27 t=2.59, bnp +10.11 t=3.69

That is the session's one real win and it is the reason deep now
finishes ahead of `ai/default` on `lategame-80`.

### What was built, measured and refused

- Routing ferries past the worker/cityfounder dispatch (upstream #33):
  Transport turns carrying 488 -> 196, delivery 12% -> 11%.
- Correcting `drop-boatless`'s `ferryboat <= 0` to `== FERRY_NONE`:
  overseas delivery 12% -> 28% but arrivals 114 -> 115, and score
  -1.50. The 12% was a denominator artefact -- two thirds of "failed"
  overseas missions were the same settlers re-picking.
- A founder pool sized per CITY: worse than joining freely on every
  count (`unitslost` +1.50, t=+2.30).

### What landed neutral-to-mildly-positive

- Idle worker fallback (`df9309d5ad`): inert on both cells, landed as
  doctrine on the human call.
- Join City (`5933c431d0`) plus a pool sized by SITES REMAINING
  (`182760afda`). On `primary-120`: pop +1.98 (t=4.14), mfg +3.05
  (t=2.11), land area back above zero, idle settlers still cleared
  (-2.45, t=-4.27). On `lategame-80` it moves the other way, all inside
  noise -- the 3.5x per-cell spread in the placeholder.
- Negative control: city density did not move on either cell. W02b
  holds; spacing is `citymindist`.

### The thing that is NOT known

**Every one of those landed after the last head-to-head.** The +9.73
margin was measured on `0f126c33b7`, before the fallback, Join City and
the pool. Their individual deltas are noise-to-mildly-positive, and
noise accumulates. **Nobody has measured deep vs `ai/default` on the
current tree.**

### Open, ranked

1. **Re-run the head-to-head.** Two 20-game batches. It is the metric
   the project is judged on, four behaviour changes have landed since
   it was last taken, and a stack of individually-insignificant changes
   is exactly how a regression hides. Do this before building anything.
2. **Tune `founder_pool_per_site`.** The only knowingly-guessed
   constant here, with a measured range (0.06 on `primary-120`, 0.207
   on `lategame-80`) and a documented reason to expect the cells to
   disagree. Report per-cell values; do not average the spread.
3. **The colony lift as an operation** (`deepops.c`). Biggest remaining
   mechanical loss, ~11 overseas cities/game, and an epic not a commit.
   `findings/settler-missions.md` section 8 has the full diagnosis and
   the two dead ends.
4. `lategame-80` as a tuning venue. Human call 2026-08-03: KEEP it --
   it is an island, tech-saturated regime no other cell tests, 60% of
   its settler targets are overseas, and it surfaced all three of this
   session's bugs. The guard is to report per-cell tuned values and
   treat sharp disagreement as overfit evidence, not to drop the cell.

## Addendum: the two ferry fixes work as a PAIR (2026-08-03, late)

Human call: keep both refused ferry fixes, they are important. Correct,
and the reason is one I should have seen while measuring them.

**Each was measured in the absence of the other, and they are
complementary halves of one loop:**

- `drop-boatless == FERRY_NONE` alone keeps the settler flagged, but
  Transports never reach `findcargo` (the dispatch bug), so nobody
  comes. The settler waits in a queue nothing serves: score -1.50.
- The dispatch routing alone sends boats to the ferry manager, but
  `stats.passengers` is ~0 because settlers un-flag themselves every
  turn via the bug the *other* fix repairs. `findcargo` early-outs and
  the boat parks: delivery 12% -> 11%.

That is not two failures. It is **one broken loop measured twice, from
each end, with the other end still broken.**

Both cherry-picked (`3050c59e6d`, `f88fa88a78`). Together, 10 paired
seeds:

    lategame-80                        without ->    with     diff      t
      score                            185.92 ->  190.28    +4.37   1.84
      pop                               47.48 ->   49.93    +2.45   2.30 *
      mfg                               55.27 ->   58.72    +3.45   1.58
      cities                             8.47 ->    8.70    +0.23   0.99
      landarea                          97367 ->   99533    +2167   0.98

    primary-120: score +0.30 (t=0.23), everything else inside noise

Every term positive on the island cell, `pop` significant. Also landed:
`site_enough_result` and `settler_guard_tolerance` (`190d47afc9`), both
inert placeholders reproducing stock.

**Method note worth carrying:** when two fixes repair opposite ends of
one broken protocol, measuring them separately measures neither. Both
were nearly discarded on individually-honest numbers.

### The head-to-head is now SIX behaviour changes stale

`+9.73` was taken on `0f126c33b7`. Since then: the idle-worker
fallback, Join City, the founder pool (twice), and both ferry fixes.
Re-running it is the first thing the next session should do.

## The stale head-to-head, re-taken: the stack is flat (2026-08-03, evening)

**First action of the session, before any new code, per the standing
instruction. The margin has NOT fallen -- and it has not grown either.**

Two pinned binaries on the same 10 train seeds, `lategame-80`, mirrored
`mixed_dc`/`mixed_cd`:

- **pre** = `0f126c33b7` (the commit the `+9.73` was measured on),
  rebuilt in `.worktrees/bench`
- **post** = `da57ed6206`, the epic branch tip: idle-worker fallback,
  Join City, the founder pool (twice), and both ferry fixes

`deep-best.ini` is byte-identical at both commits, so both arms ran the
same tuning file.

### By-turn score margin, deep - ai/default

    turn      pre     post    delta   t(DiD)     up
      20     1.70     2.27    +0.57     0.96   4/10
      40    10.87    11.85    +0.98     0.43   5/10
      60    16.12    14.88    -1.23    -0.47   5/10
      70    12.75    12.98    +0.23     0.09   7/10
      80     9.73     9.05    -0.68    -0.23   5/10

**The pre column reproduces the previous session's series exactly** at
every checkpoint, which is the control this run was for: the pin, the
seating and the analysis all agree with the measurement being compared
against.

### T80 difference-in-differences, paired over seeds

    metric        pre       post      diff        t     up
    score        9.73       9.05     -0.68    -0.23   5/10
    cities      -0.50      -0.35     +0.15     0.56   6/10
    pop          3.93       4.85     +0.92     0.54   6/10
    bnp          1.83       3.82     +1.98     1.10   6/10
    mfg          0.30       1.03     +0.73     0.17   6/10
    landarea   100.00     383.33   +283.33     0.07   7/10

### What this means

**No regression, and no demonstrated gain.** Score is flat to within a
quarter of a standard error; the other five terms are all mildly
positive and none is significant. The stack of individually-
insignificant changes did not accumulate into a regression, which was
the specific risk that made this run the first thing to do -- and it
did not accumulate into a win either.

Note this does not contradict the ferry pair's `+4.37` (t=1.84): that
was deep's absolute score in the deep arm, pre vs post, and this is
deep's MARGIN over `ai/default` seated in the same game. A change that
lifts both seats moves the first and not the second. It was not
significant on either measure.

**deep still finishes ahead of `ai/default` on `lategame-80` by ~9
score at T80, and the reason is still `0f126c33b7`.** Nothing since has
been shown to add to it.

## Workers can cross water now, and it barely matters (2026-08-03, evening)

    aa42dc099c  split the idle-work census by continent
    3e53b19a5a  findings/idle-workers.md section 11 -- the size of it
    d6dd6ce922  worker_cross_continent_discount, a tunable price
    3647d0fea4  let an idle worker take a boat to an unimproved colony

**The defect was real and is fixed. The fix is inert on the cell that
has the defect, and the reason is worth more than the fix.**

### What was measured first

The idle census now splits by continent, so a worker reporting "nothing
to do" says whether it had nothing to do or nothing it could WALK to.
`lategame-80`, 10 train seeds, 6890 idle worker-turns:

    mean per idle unit-turn      own continent   across water
      cities                             5.55           2.36
      OPEN work (road/irrig/mine)       17.55          12.69

    idle turns with no local open work    2397   34.8%
    ... and open work across the water    1043   15.1%
    ... and a boat available for it       1002   14.5%

**42% of the open terrain work these empires hold is on a landmass the
worker cannot reach.**

### The repair

The fallback's candidate scan is now shared by two passes: the land one,
and a second over cities on other continents, enumerated through a real
ferry's overlap map -- the machinery the site search already uses to see
an overseas site. The winner goes through `deep_settler_gothere()`, so
the worker books a lift, waits for it, and walks the far side.

Two guards keep it BACKUP behaviour: the site search's 1:1 ferry cap is
checked here too, and the trip pays `worker_cross_continent_discount`
(PLACEHOLDER 1.0) on its real travel turns.

### The result: neutral

deep arm, 10 paired seeds, pinned binaries either side:

    lategame-80              pre       post      diff       t
      score               190.28     190.18     -0.10   -0.10
      cities                8.70       8.68     -0.02   -0.36
      pop                  49.93      50.28     +0.35    0.68
      mfg                  58.72      59.07     +0.35    0.26
      landarea              99533      99833      +300    0.77
      mean_city_size         5.87       5.96     +0.08    1.34

Nothing moves. Not a regression, and not a win.

### Why -- and this is the finding

Over 10 games the cross-water pass was attempted 6834 times and fired
**70**:

    why=cap      3460   50.6%   a settler is already queued for every boat
    why=noboat   2988   43.7%   no free boat anywhere
    why=outbid    230    3.4%   overseas work existed and lost to land work
    why=nocand     86    1.3%   no reachable overseas work
    fired          70    1.0%

**94% of attempts die on lift supply, before the discount is ever
consulted.** The worker pipeline was not the binding constraint; the
boat supply is, and it is the SAME constraint the settler pipeline is
already starved by. The ferry fixes made it tighter, not looser: settlers
now stay correctly flagged, so `passengers >= boats` is almost always
true.

The 14.5% of idle turns that had "a boat available" used
`stats.available_boats`; the cap asks the stricter `passengers < boats`,
and that is where the population went.

### What NOT to do about it

Loosening the cap so workers bid against settlers for lift. That trades
a colony for a road, and the human call of 2026-08-03 already ruled on
it: keep 1:1, because a settler occupies a boat for a round trip.

The right answer is the one already specified and deferred: **the cap
should count TRIPS, not settlers.** A worker riding a boat that is
already crossing with a settler aboard costs nothing -- civ2civ3's
Transport has capacity 8 and carries one passenger. That needs the lift
to be an operation with a named boat, a named beachhead and a manifest
(`deepops.c`, `findings/settler-missions.md` section 8), which is the
next epic and now has **two** consumers waiting on it rather than one.

### Status of the epic's third piece

`worker_cross_continent_discount` is untuned at PLACEHOLDER 1.0 and
**tuning it now would be measuring noise**: only 316 of 6834 attempts
ever reached the comparison it governs, and only 230 of those were
decided by it. It should be tuned after the lift operation lands, not
before.

### Cross-cell check: `primary-120`, 10 paired seeds

    metric                pre       post       diff       t     up
    score              160.42     161.27      +0.85    1.00   1/10
    cities              22.68      22.83      +0.15    1.00   1/10
    landarea           312633     314050    +1416.67   1.00   1/10

**Nine of the ten seeds are identical.** The pass fired twice, both in
one game: 302 of 339 attempts declined on the ferry cap. The roomy
continental cell has almost no overseas cities to serve, which is the
expected shape, and the change costs nothing there.

No cross-cell regression. The fix is correct, guarded, and waiting on
lift supply on both cells.

## The lift epic's premise is falsified: there are no trips to share
   (2026-08-03, night)

    fc195bab2d  ai/deep: census the lift demand that could share a boat

**The first action of the lift epic was to size it, and the size is
1.6%. The design call the epic was built around does not survive.**

The specified epic was "the colony lift as an operation, and the cap
counts TRIPS" -- keep 1:1 for ordinary settlers, but let a unit ride a
boat that is already crossing, which needs a manifest, which needs the
lift to be an operation. `findings/settler-missions.md` section 9 has
the full ladder; the decisive rows, restricted to the player-turns where
the 1:1 cap actually binds (607 turns, 1160 waiting unit-turns,
`lategame-80`, 10 train seeds, 20 games):

    slot        1109   95.6%   a rideable boat with a free slot exists
    d5           329   28.4%   ... within 5 tiles
    d3           151   13.0%   ... within 3 tiles
    d0             7    0.6%   ... on the same tile
    loaded        18    1.6%   ... already carrying cargo (d<=5)

A trip-counting cap admits the last row and refuses nothing else:
**1.6%**. The manifest has nothing to share -- boats carry cargo on
11.1% of 8384 boat-turns, 0.19 loaded boats per player-turn.

### The finding is the rung above it

**95.6% of capped waiting unit-turns have a rideable boat with a free
slot, and 5017 free cargo slots sit idle on those turns.** The cap
refuses passengers while empty hulls ride along. But only **0.6% ever
stand on the same tile as a boat** -- and same-tile is where loading
happens.

The binding constraint is the **rendezvous**, not the cap's arithmetic
and not cargo capacity. Supply and demand coexist in the same empire, on
the same turn, a few tiles apart, and nothing closes the gap. Section 8
measured the identical defect from the boat's side (778 boat-turns with
a flagged, reachable settler, idle).

### What this means for the next session

- The lift-as-operation is **still the right epic**; its job changes
  from sharing crossings to CAUSING them: a named boat driven to a named
  passenger, loaded, crossed, landed, held against re-tasking.
- **Do not build the manifest as the win** (1.6%), and **do not loosen
  the cap** -- passengers it admits would not be collected either.
- The 1:1 cap can stay exactly as it is until the rendezvous works.

**This overrides an explicit human design call (2026-08-03), so it is
flagged rather than acted on.** Nothing was built on the falsified
premise.

## The lift owns the crossing now, and the score does not move
   (2026-08-03, night)

    41ea411542  ai/deep: declare the lift operation kind and its tunables
    28325fb6b9  ai/deep: own the colony lift -- boat, manifest, beachhead
    (+ the delivered/strayed metric split)

**The rendezvous defect is fixed and the fix is neutral on score.** Same
shape as the cross-water worker: the mechanism works, and the cell does
not reward it yet.

### What was built

A lift operation claims a named boat and a named manifest, remembers a
named beachhead (the passenger's own `goto_tile` -- the op does not
second-guess the settler's chosen site) and holds both ends against
re-tasking. MUSTER is the rendezvous the stock protocol never completes:
the boat goes to the manifest. MARCH is the crossing. The manifest is
released beside the beachhead, where the stock settler path disembarks
correctly on its own.

Passengers are ANY unit that must cross -- settler, worker, or a
defensive unit being redeployed overseas. Moving troops is not attacking
with them; ASSAULT remains the amphibious battle. Human call
2026-08-03: a garrison move must not masquerade as an attack to get a
hull.

### Mechanism: it works

`lategame-80`, 10 train seeds, mirrored, 20 games, lift ON:

    formed   165        sailed   110        arrived/delivered  92

    lift-landed(delivered)  77    lift-gather-timeout  31
    lift-nobody             24    lift-arrived         15
    lift-noboat              7    lift-unreachable      4
    lift-cross-timeout       2

Against a negative control of `lift_form_radius = 0` on the SAME pinned
binary, which logs **zero** lift events. Before this, the census found
0.6% of waiting unit-turns ever stood on a boat's tile; now 110
crossings happen per 20 games.

### Score: flat

Paired over 10 seeds, deep's margin over `ai/default`, ON minus OFF:

    metric            OFF       ON     diff       t     up
    score            8.37     7.02    -1.35   -0.36   8/20
    cities          -0.37    -0.43    -0.07   -0.16  11/20
    pop              4.77     4.35    -0.42   -0.19  11/20
    bnp              2.93     4.05    +1.12    0.35  11/20
    mfg             -0.12    -1.12    -1.00   -0.24   7/20
    landarea        33.33  3433.33 +3400.00    0.75  11/20
    mean_city_size   0.82     0.74    -0.08   -0.55   9/20

**Not a regression and not a win.** Nothing clears a standard error.
More units cross the water and the same number of cities exist at T80.

### What this rules out and what it leaves

Ruled out: that the rendezvous was the thing standing between deep and
more overseas cities. It was a real defect, it is fixed, and the cities
did not follow. Delivery was never the binding constraint on the
*count* -- section 8 already found the absolute overseas yield was ~11
cities either way while the delivery RATE tripled.

Left open, in order:

1. **The three tunables are placeholders and have never been tuned**
   (`lift_form_radius` 8, `lift_gather_turns` 8, `lift_cross_turns`
   20). 31 gather-timeouts and 24 hollowed ops out of 165 is a lot of
   waste, and gather_turns is the parameter that governs it.
2. **Hitch-a-ride** (human design 2026-08-03). Now that crossings
   actually happen, a unit wanting passage should inspect boats already
   servicing someone bound for that continent and join if the detour
   costs **less than one extra turn of boat waiting**. The census that
   killed the original trip-counting design measured a world with no
   trips in it (11.1% of boat-turns carried anything); this changes
   that denominator, which is why the order matters. `members[]` is
   already a manifest and the beachhead is already named.
3. Whether the colony that lands is worth founding at all -- site
   quality, not delivery.

## Hitch-a-ride is ruled out on its own census (2026-08-03, late)

    a1e7e7f774  ai/deep: census the demand that could join a lift
    4e7267e139  doc: hitch-a-ride is 0.4 admissible unit-turns a game

**The second specified design call in a row was falsified by sizing it
before building it.** Nothing was built on it.
`findings/settler-missions.md` section 10 has the full ladder.

The lift ON, 20 games, `lategame-80`, 10 train seeds, mirrored, one
pinned binary. Per unit-turn where a unit wants a boat, no operation
holds it, and its target is overseas:

    A LIFT IS RUNNING: 877 player-turns, 337 waiting unit-turns

    anyop  335  99.4%    cont    57  16.9%    muster  41  12.2%
    d5      26   7.7%    d3       8   2.4%    d0       0   0.0% [tripwire]

### 12.2% is not the 1.6% getting better

Section 9 measured 1.6% and killed trip-counting; this reads 12.2% and
still fails, because **the rate rose from the denominator collapsing**,
not from supply appearing: waiting unit-turns fell 1160 -> 337. The
lift already absorbs what used to queue.

    trip-counting cap (section 9, no lift)   18 unit-turns / 20 games
    hitch-a-ride, joinable                   41 unit-turns / 20 games
    hitch-a-ride, within a 1-turn detour      8 unit-turns / 20 games

`d3` is the rung the stated gate ("detour < one extra turn of boat
waiting") actually admits: **0.4 per game**, fewer than the 18 already
judged not worth a manifest, and unit-turns overcount the units behind
them.

### The risk runs the wrong way (human, 2026-08-03)

A hitched manifest concentrates N passengers in one hull, so one
sinking loses all of them: the downside scales with exactly the
quantity the feature raises, while the upside is one saved trip. That
makes it a do-not-build rather than a tune-it-carefully.

### What the same census found instead: the op array

`opfull` was carried so a storage ceiling could not masquerade as a
manifest ceiling. A lift only forms into a free slot
(`deepops.c:2420`) and `DEEP_OPS_MAX` is 4, shared with assaults:

    op array full                        1125 / 4823  23.3%
    ... AND an unclaimed waiter present   239 / 4823   5.0%  = 11.9/game

**Formation is blocked ~30x more often than hitch-a-ride would have
fired.** Unmeasured whether relieving it is worth anything -- the
army's claim winning is deliberate, and more crossings have twice now
been neutral. It is the next thing to SIZE, not to build.

### Provenance

The batch reproduced the previous session's lift tally exactly -- 165
form / 110 sail / 77 offboard / 15 arrive, 58 delivered + 19 strayed --
confirming the census is log-only and the pin is honest.

## The lift's parameter was never the lever; the rendezvous was
   (2026-08-03, late night)

    a4af2f21e4  ai/deep: make the lift rendezvous two-sided
    2aed19c427  ai/deep: let a second passenger join at the rendezvous
    8b94854e62  ai/deep: require a hitcher to be able to WALK there
    e35c0ba323  ai/deep: hold the quay in proportion to the crossing
    b338a7eeef  ai/deep: round the hitch wait up to a whole turn
    8c59eeac3a  ai/deep: give the lift its own operation slots
    dfde81faf1  ai/deep: measure slot pressure and hull capacity
    19792cb887  doc: scope section 10

**READ THIS FIRST: only the gather bracket and the censuses below are
MEASURED. Everything the five ai/deep commits above changed is BUILT
AND UNVALIDATED** -- `meson test` and `tune-audit` green, no benchmark
run on any of it. Rule 6 applies; the pending runs may come back flat
like every other lift change so far.

### MEASURED (AND NOW SUPERSEDED): `lift_gather_turns` in 4..16

20 games each, `lategame-80`, mirrored, one pinned binary, tuning-file
switch:

    gather   form  sail  delivered+arrived  timeout   score vs base
      4       177   107        74             55    -0.50 (t=-0.03, 5/10)
      8       165   110        73             31       --
     16       147   108        72             12    -4.90 (t=-0.20, 5/10)

A 4x range moves timeouts 55 -> 12 and moves deliveries by one colony
and score by nothing, coin-flip on seeds in both directions.

**DO NOT READ THIS AS "the parameter is inert" (human, 2026-08-03).**
It was measured against the BROKEN rendezvous -- 2.22-turn musters, 31
timeouts -- where it could only ever be a give-up timer on a commute
the passenger was not helping with. After the protocol fix musters run
0.70 turns and timeouts are ZERO, so the window does not bind at all
at 8; and with a manifest above 1 it stops being a give-up timer and
becomes a real GATHERING window, which is the first job it has ever
had. It is tunable and untuned, and this bracket does not speak to it.

Third instance this session of one error: a verdict sized against a
mechanism that then changed underneath it (see also hitch-a-ride's
census, and the two-sided rendezvous's own premise).

### MEASURED: why it could not have been a lever

`n_members` was fixed at 1, so "gather" gathered nothing -- the
parameter was a give-up timer on the boat's commute (human, 2026-08-03).
And the commute was one-sided: `deep_op_holds()` froze the passenger,
which only moved if ALREADY adjacent to the hull. Over the same 20
games:

    muster outcome     n    idle passenger-turns   mean
    sailed            110          244             2.2
    gather-timeout     31          279             9.0
    nobody             10           47             4.7

~570 held-idle passenger-turns per 20 games, 57% of them on lifts that
never sailed. **Every timeout burned the whole window (mean 9.0 against
a cap of 8, max 9)** -- not one was a boat that almost made it. The
boat was not arriving late, it was not arriving at all, which is what
a shorter window cannot rescue and a longer one cannot either.

### MEASURED: the island cell is a different world

`isolated-150`, 2 games, lift ON. Per game against `lategame-80`:

    unclaimed overseas waiters   16.9 -> 287
    joinable (cont)               2.9 -> 122
    within a 1-turn detour (d3)   0.4 -> 12.5
    waiters per lift-running turn 0.38 -> 4.04

`mapgenerator3` keeps making islands until the landmass runs out
(`mapgen.c:2401-2428`), so the unclaimed-island tail is the
colonisation target set. Section 10's verdict is scoped accordingly.

### BUILT, NOT VALIDATED

- **Two-sided rendezvous.** A tile named at formation, FIXED (a
  passenger homing on a hull that homes on the passenger orbits it),
  with both ends driving to it and formation refusing a meeting the
  passenger cannot reach.
- **Hitch-a-ride**, reopened because the fixed rendezvous moved the
  cost from the boat to the passenger. Contracts: a hitcher is drawn
  only from units already waiting for a boat, and a straggler is
  dropped rather than waited for unless the wait is priced.
- **Proportional patience.** `lift_hitch_wait_ratio` of the estimated
  crossing, rounded UP to a whole turn (truncation was refusing every
  short-trip wait, which is where the cheap wins are), never spending
  the gather window's last turn.
- **Dedicated lift slots.** The array is 4 army + 4 lift-only; the
  army's range is unchanged so assault placement cannot move.

### STATE: four new placeholders, all untuned, all A/B switchable

    lift_op_slots         4    (0 = old behaviour, proven identical)
    lift_manifest_max     3    (1 = old single-passenger lift)
    lift_hitch_wait_ratio 0.25 (0 = never wait)
    (plus lift_form_radius 0 = whole lift off)

The `lift_op_slots = 0` equivalence was PROVEN, not assumed: one seed
on the new binary reproduced the old binary's scorelog exactly across
80 turns and all players, with every metric line identical once
`opfull` was masked. That check is also what caught `opfull` silently
redefining itself when `DEEP_OPS_MAX` went 4 -> 8.

Measure in order -- rendezvous, then manifest, then wait -- since they
interact.

### Method notes earned here

- **A census sizes a MECHANISM, not a feature.** Section 10 killed
  hitch-a-ride correctly and could not speak to a version where the
  passenger walks instead of the boat detouring. When the cost model
  changes, the ladder has to be re-derived, not just re-run.
- **A verdict inherits the venue it was taken in.** 30x between cells.
- **Read the mechanism before spending the machine on its parameter.**
  60 games confirmed a knob that `n_members = 1` had already made
  inert. The measurement was sound; the hypothesis was never
  mechanically plausible.
- **`can_unit_exist_at_tile()` is not reachability.** It says a unit
  could stand somewhere, not that it could get there -- straight-line
  distance to such a tile enrolled units across open ocean, and would
  have been invisible because non-arriving hitchers are dropped by
  design. Same class as gating formation on straight-line distance.

### Stale reference

`CLAUDE.md` cites `./benchmark/report.py`; no such file exists.
`extract.py` writes `metrics.csv` and the per-cell comparisons are
ad-hoc. Either restore the script or correct the doc.

### MEASURED: the two-sided rendezvous, and the reason given for it
    (2026-08-03, late night)

20 games, `lategame-80`, mirrored, one pinned binary, everything else
switched off (`lift_op_slots = 0`, no manifest, no patience):

    variant      form  sail  delivered  arrived  strayed  unreachable
    base          165   110     58        15       19         4
    two-sided     173   114     69        13       16         0

    score, deep minus classic, paired: -3.70 (se 32.81, t=-0.11, 3/10)

**The mechanism the change was justified by does not operate.** The
commit said "a settler that can walk half the distance is half a
rendezvous the boat no longer has to sail". Measured:

    variant     sailed  idle-turns  mean muster  timeouts  timeout idle
    base          110      244         2.22        31         279
    two-sided     114      258         2.26        35         315

Muster duration is UNCHANGED, total idle time rose slightly, and the
timeouts still burn the entire window (315/35 = 9.0, exactly as
before). The boat is far faster than a settler, so the passenger
walking contributes almost nothing to closing the gap -- the arithmetic
was available before the run and was not done.

Nor is the delivery gain a result: 73 -> 82 delivered+arrived on counts
that size carries a difference SD near 12. The one unambiguous change
is `unreachable` 4 -> 0, which is the formation-time reachability check
working, and that is a small class.

**Not a regression, not a win, and -- unlike the lift and the
cross-water worker -- not even a proxy that moved.** Those two fixed a
measured defect and failed to convert it. This one did not move its own
proxy. The reachability gate is worth keeping on correctness grounds;
the "shortens the muster" reasoning is falsified and must not survive
into the next design as motivation.

Hitch-a-ride and the proportional patience are still UNMEASURED, and
they rest on the same fixed-rendezvous machinery -- but not on this
claim: they need the meeting point to be STABLE and NAMED (so a joiner
has something to walk to), not FAST. That premise is intact.

### MEASURED: the hull is the manifest's real cap, and it differs by cell

`isolated-150`, 2 games, one mirror -- small, but hull capacity is
structural rather than noisy. While a lift is running:

    boats/turn 14.81   capacity<=2  95.4%   carrying 1  25.0%
                                            carrying 2+  2.4%

**95.4% of hulls can carry at most two units** (human, 2026-08-03:
"not every boat type can carry more than 2 things"), and 2+ is carried
on 2.4% of boat-turns. So `lift_manifest_max = 3` is UNREACHABLE on
that cell: hitch-a-ride can add at most one passenger there, and only
onto an empty boat. `lategame-80` sets `techlevel 50` and gets
capacity-8 Transports, so the same constant means different things in
the two cells -- an argument for tuning it per cell, not globally.

**The operation array is full on 61.1% of lift-running turns** there,
against 36.0% on `lategame-80`, measured with the CORRECTED opfull
(slots a lift may actually use). Lifts are 3.26 of 3.44 live ops: they
saturate the array against themselves, not against the army. On the
island cell `lift_op_slots` is plausibly the largest of the four
pending changes, and it is currently ordered last -- reorder it first
if the island cell is the venue.

### MEASURED: the rendezvous protocol is fixed, and score still does not move
    (2026-08-03, late night)

    c5c7d27939  meet where the later arrival is soonest
    92d9c4009a  board and sail on the turn the two ends meet
    486f99030d  log whether the passenger actually walks

Three human observations (2026-08-03), bundled per this file's own
note that fixes to opposite ends of one protocol measure nothing
apart. 20 games, `lategame-80`, mirrored, one pinned binary:

    variant        form  sail  sail%  delivered+arrived  timeout  unreach
    base            165   110   67%          73            31       4
    two-sided only  173   114   66%          82            35       0
    ALL THREE       143   141   98.6%        97             0       0

    muster mean   2.22 -> 0.70 turns      idle 244 -> 99
    same-turn sails  0 -> 131 of 141 (93%)

**Gather-timeouts 31 -> 0. The rendezvous is solved.** The walk
instrumentation also settles the question the flat two-sided result
could not: the walk DOES fire -- 56 walks, 40 walk-then-board in the
same turn, 32 already at the shore.

Why the two-sided change alone did nothing is now clear:
`deep_lift_meeting_tile()` chose water at or beside the PASSENGER, so
the meeting point sat at its feet and there was nowhere to walk to.
Both ends drove to a point only one had to reach.

**Score: -1.80 (se 25.54, t=-0.07, up 6/10). Flat.**

### The mechanism IS the deliverable here (human call, 2026-08-03)

Cross-water worker, the lift, the two-sided rendezvous, and now a
protocol that tripled muster completion and delivered 33% more
colonies. None moved score on this cell, and **that does not make them
worth zero** -- 00-context section 8 says so in advance: record the
proxy AND the aggregate, and do not discard a correct fix because the
headline was flat. An AI that no longer freezes a settler for nine
turns against a boat that cannot arrive is a better AI at the thing it
was doing wrong. This epic is banked as a MECHANISM win, flat score
noted and not treated as a defect.

What the flat score does license is a narrower, still-useful claim
about where to look next -- not a verdict on the fixes. Section 8
measured
the absolute overseas yield at ~11 cities per game whether delivery
ran at 12% or 28%; delivery has now been repaired four times and the
count still does not follow. The constraint is not transport. It is
whether the colony that lands is worth founding -- site quality --
which is specified and unbuilt.

The island cell tests that reading where crossings ARE the game. Note
the asymmetry in how to read it: a score move there would be real
news, but another flat result does NOT retire the mechanism work --
it only says score is the wrong instrument for it, which is already
the conclusion here.

### The freed hull capacity is not harvested (2026-08-03, late night)

Human hypothesis: a faster rendezvous should mean fewer boats needed.
Measured on the same two 20-game batches, per player-turn:

    metric                        base    bundled
    standing fleet (boats)        1.67      1.63   -2.4%
    idle boats (avail)            1.24      1.19   -4.0%
    passengers waiting (psng)     0.57      0.53   -7.0%
    sails per 1000 boat-turns    13.68     18.02   +32%

**Each hull does 32% more crossings, and the fleet is the same size.**
The -0.04 boats/turn is inside noise. So the capacity was freed and
not harvested.

There is a mechanical reason, and it is the same gap section 1b of the
handoff names from the other side: **nothing in the build decision can
see fleet productivity.** Boat want is `dp->colony_want` plus a
berth-shortage test, and that test counts only MUSTER ops with a real
`target_city` -- which excludes every lift, since a lift sets
`target_city = 0`. The shipyard's entire demand signal is "is there
colony want, am I coastal". A fleet that doubled its throughput builds
at exactly the same rate, and one whose lifts are all starved of hulls
reports no shortage at all.

Two independent routes to the same conclusion: the supply side is
measured too crudely to respond to either surplus or scarcity. That is
a better-specified target than "bias boat building toward the
rendezvous", and it is sized by the numbers above rather than assumed.

### Too Many Boats (human, 2026-08-03)

Same 20-game batch, `lategame-80`, per player-turn:

    boats/turn             1.63
    idle (available)       1.19   = 73% of the fleet
    carrying anything      0.29   = 18% of the fleet
    passengers waiting     0.53   -> 3.1 boats per waiting passenger

    over the game:     boats/turn   carrying
      turns  0-19         0.19       15.0%
      turns 20-39         1.69       23.9%
      turns 40-59         2.51       18.3%
      turns 60-79         2.03       13.0%

**The carrying fraction FALLS as the fleet grows** -- 23.9% down to
13.0% while hull count peaks at 2.51, about five boats per waiting
passenger. That is the signature of overbuilding: more hulls chasing
demand that is not growing. (Shield cost is not measured here; the
counts are hulls, not shields.)

**CAVEAT on the 73% (human, 2026-08-03): `available_boats` is not
idle.** It counts every ferry whose `passenger` is FERRY_AVAILABLE --
"nobody is assigned to me" -- which includes boats out EXPLORING, and
includes every hull a lift has claimed, since the lift drives its boat
directly and never sets `passenger`. So 73% is an upper bound and the
true idle share is lower. The overbuilding conclusion does not rest on
it: the carrying share falling from 23.9% to 13.0% as the fleet grows
is independent of how idle is defined. Re-measure the headline with
deep_idle_ferries() before quoting it.

### What this does to the epic's premise

The delivery pipeline was repaired all session against a supply side
carrying a **3x surplus throughout**. Boats were never scarce --
`lift-noboat` was 7 of 165 before the fix and 1 of 143 after -- and
section 9's "95.6% of blocked passengers had a free slot somewhere"
was the same fact seen from the passenger's side. The rendezvous was
really broken and is really fixed, but "too few crossings" was never a
hull-supply problem.

Third independent route to the same defect: `colony_want` drives boat
production with **no term for idle hulls**, just as it has none for
fleet productivity (the -32% throughput case above) and none for
lifts going unserved (the berth test that excludes them). It cannot
see 73% of the fleet doing nothing, so it keeps building.

**The specified target is now the build signal, not the transport
mechanism.** It is wrong in three measured directions and it is
upstream of everything this epic touched.

### The stray diagnosis was WRONG, and the cause is still unknown
    (2026-08-04)

    261075c429  instrument the stray
    ac1310ccd4  take a lift's passengers off the auto-worker agent

**`ac1310ccd4` does not fix straying and its justification was
invalid.** With the agent provably suspended (`ssa=0` on every
offboard passenger), strays are 70 of 131 -- 53%, no better than the
38% before -- and they still leave with the boat a median 18 tiles
from the beachhead, 68 of 70 still at sea, 100% alive.

**The reasoning error, recorded because it is the instructive part:**
the fix was justified on "100% of strays carry SSA_AUTOWORKER". But
100% of DELIVERED passengers carried it too -- the same log line said
so, and it was read as confirmation. **A variable that is constant
across both populations cannot explain the difference between them.**
The only thing that ever separated the two groups was the boat's
distance, which says WHEN they leave (mid-ocean, median turn 4-6) and
never said WHO removes them.

Ruled out since: `deep_flee_workers()` skips transported units
(deepdefence.c:1469); `deep_manage_units()` skips own-transported
units entirely; the auto-worker agent is now off. Still open: the
stock ferry code acting on the claimed hull, transport-to-transport
transfer (3 of 70 were aboard something else), or a server-side unload
elsewhere.

**Next step is a trace, not another hypothesis.** Log every member's
transport id and tile each MARCH turn, then read the turn a stray
detaches, rather than correlating one snapshot at detach time.

The suspension itself is left in place but is UNVALIDATED and unjustified
by its stated evidence -- it may be neutral or harmful, and the paired
score against the fixed-rendezvous arm is what should decide whether it
stays.

### Straying: the hold leaked through a SECOND driver (2026-08-04)

    4cbe7618cd  trace every lift member through the crossing
    b699ddefa1  honour the operation hold in the settler_run hook

**Traced, not correlated.** The trace logs every member every MARCH
turn -- which hull it is on, both tiles, the hull's moves and hp -- and
the detach events are unambiguous:

    unit=219 turn 59->60: on 208->0  unit (42,55)->(46,57) boat (42,55)->(43,56)
    unit=219 turn 67->68: on 249->0  unit (42,53)->(41,54) boat (42,53)->(41,53)
    unit=219 turn 74->75: on 249->0  unit (41,53)->(42,54) boat (41,53)->(41,53)

The passenger steps onto adjacent land under its OWN power with the
hull mid-ocean at full hp -- and unit 219 does it on turns 59, 67, 74
and 84, which is section 8's pick-drop churn with a mechanism attached.

**The cause is a second, independent driver.** deep_op_holds() is
honoured in deep_manage_units(), but `auto_workers_player()` reaches
every worker and cityfounder of an AI player through the settler_run
vtable hook, gated on

    ssa_controller == SSA_AUTOWORKER || is_ai(pplayer)  (autoworkers.c:1372)

The `|| is_ai(pplayer)` means the agent flag is never consulted for an
AI player at all. deep_auto_settler_run() then re-tasks the held unit,
which disembarks it. The hook now returns early for a unit an
operation holds.

**Why the previous fix failed, kept as the lesson:** ac1310ccd4
suspended SSA_AUTOWORKER and changed nothing, because this gate
ignores it. Its evidence was a constant -- SSA_AUTOWORKER sat on 100%
of strays and 100% of deliveries, printed side by side and read as
confirmation. A variable identical in both populations cannot explain
the difference between them, and only a trace over time could show
what a detach-time snapshot never could.

That suspension is now redundant AND unvalidated. Leave it or revert
it on the paired numbers, not on argument.

### Metric trap: the arrival fix moved its own denominator (2026-08-04)

Reading the stray rate after `2485c95279` (release the manifest on the
turn the boat reaches the beach) gave **100% strayed**, which looks
like a catastrophic regression and is not one.

`lift-delivered` is logged from the `offboard` branch, which fires when
the manifest has already left the hull. The arrival fix dissolves the
op as `lift-arrived` while the passengers are still ABOARD, so
successful crossings stopped passing through `offboard` entirely and
the offboard population became pure failures by construction:

    binary        arrived  delivered  strayed   success/game  stray/game
    pin-board        52       88        115         7.0          5.8
    pin-trace(3g)    35        0         23        11.7          7.7
    pin-trace(2g)    20        0         13        10.0          6.5

Read as arrived+delivered against strayed, the success rate is 55% ->
~60% with throughput up from 7.0 to ~10-11.7 crossings per game.

**Second instance of this class tonight**, after `opfull` silently
redefining itself when DEEP_OPS_MAX went 4 -> 8. Both were caught only
because the number was implausible. The rule earned: when a change
alters WHICH code path an outcome exits through, every rate computed
over those paths changes meaning, and the ratio must be re-derived
before it is compared to an older one.

### The hold fix removes straying (2026-08-04)

`b699ddefa1`, quick check on `isolated-150`, success = arrived +
delivered (the denominator note above applies):

    binary                success  stray  rate   success/game
    without hold fix (2g)    20      13    60%       10.0
    without hold fix (3g)    35      23    60%       11.7
    WITH hold fix (3g)       31       1    96%       10.3

**23 strays in three games becomes 1.** Small sample, but not a subtle
shift, and it is the effect the trace predicted: the settler_run hook
was the driver, and it now returns early for a unit an operation
holds.

And the fix it replaces is confirmed a NO-OP: `ac1310ccd4` (suspending
SSA_AUTOWORKER) produced BYTE-IDENTICAL scorelogs over 20 games from a
different binary and a different tuning file -- paired score diff
exactly +0.00, se 0.00, every mechanism count matching to the digit.
Reverted in `31a8016bc7`. `auto_workers_player()` never reads the flag
for an AI player, so clearing it could not do anything.

### The full arc, worth keeping as method

1. Snapshot correlation at detach time said SSA_AUTOWORKER (100% of
   strays). It was also 100% of DELIVERIES -- a constant read as a
   cause. The fix built on it was provably inert.
2. A TRACE over time -- every member, every MARCH turn, hull id and
   both tiles -- showed a held settler stepping onto land with the hull
   mid-ocean at full hp, repeatedly.
3. That named a second driver the hold never covered, and the one-line
   check removed ~97% of the failure.

A snapshot can only show what correlates with an outcome. A trace shows
what happened before it. When a candidate cause is present in both
populations, only the second kind of evidence can discriminate.

### Hitch-a-ride measured at last: it does not pay (2026-08-04)

20 games, `isolated-150`, ONE pinned binary, one tuning switch
(`lift_manifest_max` 1 -> 3 with `lift_hitch_wait_ratio` 0 -> 0.25):

    variant  form  sail  arrived  strayed  cross-TO  enrol  manifests
    base      344   335    174       4       112       0     335x1
    hitch     328   322    177       3        90      85     289x1, 33x2

    score paired: hitch - base = -16.90 (se 19.35, t=-0.87, up 4/10)

It WORKS -- 85 enrolments, 33 crossings carrying two -- and on THIS
binary it buys nothing: deliveries 174 vs 177, score trending negative.

**PROVISIONAL, and for the fourth time this session for the same
reason (human, 2026-08-04): the mechanism changed underneath the
measurement.** That binary had SINGLE-DROP, so every joiner was landed
a median 7 tiles from its own site and walked the rest -- and it had NO
beachhead reachability gate, so 21% of crossings were stalled from
formation, on which a hitcher is a settler wasted twice. Both make
hitching maximally expensive, and both are now fixed (`75d108f119`,
`de5265b641`). Re-measure before concluding anything.

Note this ran with single-drop, so every joiner walked a median 7 tiles
from the core's beachhead. The multi-drop commit (`75d108f119`) lands
after this binary and could change it; that is the one remaining reason
to keep the feature switched on for another measurement rather than
defaulting it off.

### The hold fix, confirmed at scale

`strayed` 109 -> 4 over 20 games on the same cell (3 in the hitch arm).
The 3-game quick check said 23 -> 1 and it holds. Straying is solved.

### New bottleneck: the crossing itself

    cross-timeout   2 per 20 games (before)  ->  90-112 (now)

With the muster solved, the failures moved downstream. `lift_cross_turns`
= 20 was described in the last handoff as barely binding at 2 events;
it now dominates the failure mix. It is a PLACEHOLDER and has never
been tuned, and unlike `lift_gather_turns` it now has a real job.
Measure before assuming: a crossing that times out may be a boat that
cannot path to its beachhead at all, which is a routing bug rather than
a budget that is too small.

### The 40-seed verdict on the rendezvous fix (2026-08-04)

The 10-seed +29.90 (t=1.66, 8/10) does NOT survive. 40 paired seeds,
`isolated-150`, control = original lift, fixed = rendezvous protocol:

    variant   seeds    mean      se       t
    control     40    128.25   17.94    7.15
    fixed       40    139.53   17.09    8.17
    diff              +11.28   12.78    0.88   up 25/40

**+11.28, t=0.88. Not a score win.** The point estimate stays positive
and 62% of seeds are up, which is worth something as a prior, but
nothing here clears a standard error and it must not be reported as an
improvement.

The mechanism over the same 80 games is not in doubt:

    gather-timeout   545 -> 28
    delivered        427 -> 558   (+31%)
    unreachable       11 -> 0
    cross-timeout     98 -> 167   (the failures moved downstream)

Fifth instance of the project's recurring shape: transport mechanisms
improve dramatically and the aggregate does not follow. 00-context
section 8 anticipated exactly this, and the human's call stands -- the
mechanism is the deliverable, the flat score is not a defect.

**Method note that cost nothing and saved a false claim:** the 10-seed
estimate was inflated by 2.6x. The control arm's OWN margin also moved
86.3 -> 111.5 -> 128.25 as seeds went 10 -> 23 -> 40, which is the same
magnitude as the effect under test. On this cell, ten seeds cannot see
an effect of this size at all.

### The four switches, isolated on one binary (2026-08-04)

10 paired seeds each, `isolated-150`, `pin-hold`, one tuning switch per
arm. **All underpowered** -- the 40-seed run above showed 10 seeds
cannot resolve effects of this size -- so these are directions, not
results:

    arm                              mean    vs base      t     up
    base                            111.80      --
    hitch  (manifest 3 + patience)   94.90    -16.90    -0.87   4/10
    slots  (lift_op_slots = 4)      134.10    +22.30    +1.05   5/10
    damp   (boat_idle_damping 0.5)   91.80    -20.00    -1.17   3/10

**`damp` trending NEGATIVE is the one to take seriously**, because it
contradicts the "Too Many Boats" reading. If suppressing boat building
when hulls are idle costs score, the surplus is doing something the
idle count does not capture -- reserve capacity for bursts, or hulls
already positioned on the right ocean when demand appears. The
73%-idle figure was an upper bound anyway (it counted exploring and
op-claimed hulls). Do not treat fleet reduction as free.

`slots` is the most promising direction and is consistent with the
measured 61% array saturation.

**Attribution trap avoided, worth recording:** the first pass at this
table matched arms by tuning FILENAME, and several pins reuse
`base.ini`, so it compared across different binaries and produced a
plausible, wrong table (hitch -50.90 instead of -16.90). Group by the
server hash in `key.txt`, which is exactly what the content-addressed
cache records it for.

### Dead code that measured as "no effect" (2026-08-04)

    fc4262ff15  run the multi-drop routing on BOTH arrival paths

The nearest-first drop routing (`75d108f119`) sat inline in the slow
arrival branch, and the same-turn arrival (`2485c95279`) returned
before reaching it. Same-turn arrival is now the common case, so the
routing NEVER RAN: 20 games carried 16 two-passenger manifests and
logged zero next stops.

So the full-stack hitch re-measurement (-19.00, t=-0.76) did not test
what it was built to test either -- joiners were still landed at the
core's beachhead and still walked the median 7 tiles the routing
exists to remove. Both numbers are void; re-running.

**The rule this earns:** a feature that never executes and a feature
that does nothing are IDENTICAL in the score column. The only reason
this surfaced is that the routing logs a counter -- `nextstop=0` is
visible where a flat score is not. Every mechanism added here should
log an execution counter, and it should be read BEFORE the score.

This is the second such case tonight after hitch-a-ride went four
commits deep with `lift_manifest_max = 1` in every run, never taking a
branch.

### The combined configuration, 20 seeds (2026-08-04)

Human-specified bundle: quarter-priority boat building
(`colony_lift_weight` 0.779 -> 0.195), hitching with a >=2 turn wait
floor, two-sided rendezvous, nearest-first delivery.

Counters first, per the rule above: hitching FIRED (127 enrolments, 106
patience-waits, 48 two-passenger crossings) but `nextstop = 0` -- the
binary predates `fc4262ff15`, so multi-drop still did not run. The
bundle was missing the ingredient it was built around.

For the parts that did execute, 20 paired seeds:

    variant  arrived  cross-TO    mean      diff
    ctrl       430      113     164.95
    combo      394       97     150.30    -14.65 (se 19.05, t=-0.77, 12/20)

**Deliveries fell 8%** -- the boat cut doing exactly what it says --
and score did not come back.

### Two arms now agree: cutting boat production costs deliveries

    damp  (boat_idle_damping 0.5)   -20.00 (t=-1.17)
    combo (colony_lift_weight /4)   -14.65 (t=-0.77), deliveries -8%

Neither is significant, but they agree in direction and mechanism. The
"Too Many Boats" reading -- 73% idle, 3x surplus -- said the fleet was
overbuilt and the surplus was waste. **That inference is now doubtful.**
The idle count was an upper bound (it counted exploring and op-claimed
hulls), and a hull sitting idle on the right ocean when demand appears
is not waste, it is inventory. Removing it removes crossings.

Do not cut boat production further without measuring what the idle
hulls do when demand arrives.

### Hitch-a-ride, final: neutral even with routing live (2026-08-04)

20 paired seeds, `isolated-150`, `pin-drop` (multi-drop routing
verified firing: enrol=143, nextstop=23, 38 two-passenger crossings):

    variant  arrived    mean      diff
    base       430    164.95
    hitch      422    156.70   -8.25 (se 17.67, t=-0.47, up 10/20)

Ten of twenty seeds up, deliveries flat. **Neutral.**

The progression across three binaries is the useful part, and it
confirms the human's call that multi-drop was a precondition:

    pin-hold  multi-drop absent      -16.90 (t=-0.87)
    pin-full  multi-drop DEAD CODE   -19.00 (t=-0.76)
    pin-drop  multi-drop LIVE         -8.25 (t=-0.47)

Routing halved the penalty -- joiners no longer walk the median 7
tiles -- and did not get the feature past zero.

**Recommendation: default `lift_manifest_max = 1`** (off). The
mechanism works and the demand is real; it simply does not convert. The
switch stays so a future cell with capacity-8 hulls and denser demand
can re-test it cheaply, and section 10's verdict is upheld on its
third and fairest measurement.

### The full stack, three arms, counters verified (2026-08-04)

20 paired seeds, `isolated-150`, `pin-final`, all mechanisms firing
(ctrl 0/0/0; combo enrol 133, nextstop 31, wait 111; noboatcut enrol
133, nextstop 29, wait 144):

    variant                     arrived     mean      diff
    ctrl                          430     164.95
    combo (with boat cut)         361     147.80   -17.15 (t=-0.93,  9/20)
    noboatcut (transport only)    414     155.50    -9.45 (t=-0.58, 11/20)

**The boat cut is the damaging half, now isolated**: it costs ~8 points
AND 53 deliveries (414 -> 361, -13%). Removing it leaves -9.45, which
is indistinguishable from hitch-alone (-8.25) -- the bundle adds
nothing beyond hitching, and hitching adds nothing.

Deliveries fall slightly even without the boat cut (430 -> 414). That
is the patience cost: 144 waits means hulls holding the quay for
joiners, delaying crossings that would otherwise have sailed. Patience
is not free even when it is bounded.

### The session's shape, stated plainly

Every ECONOMIC knob tested measured neutral or negative:

    hitch-a-ride           -8.25  (t=-0.47)  -> defaulted OFF
    + patience             -9.45  (t=-0.58)
    + boat cut            -17.15  (t=-0.93)
    boat_idle_damping     -20.00  (t=-1.17)
    lift_gather_turns       flat  across 4..16 (pre-fix mechanism)

Every MECHANISM repair measured large and unambiguous:

    gather-timeouts     545 -> 28
    deliveries          +31%
    straying            109 -> 4
    unreachable          11 -> 0
    muster duration    2.22 -> 0.70 turns
    sail rate           67% -> 98.6%

**The value was in fixing what was broken, not in tuning what worked.**
Only `lift_op_slots` remains a live positive direction (+22.30 at 10
seeds, backed by 61% array saturation) and is being re-measured at 20.

### lift_op_slots: the ceiling was real, relieving it costs score
    (2026-08-04)

20 paired seeds, `isolated-150`, same control:

    variant  opfull  forms  arrived    mean      diff
    ctrl       8%     789     430    164.95
    slots      0%     997     526    151.45   -13.50 (t=-0.88, 9/20)

The mechanism did exactly what it was built to do -- saturation 8% ->
0%, formations +26%, **deliveries +22%** -- and score fell. Ninety-six
extra colonies delivered across 40 games bought -13.50 points.

Defaulted to 0. Second 10-seed estimate to FLIP SIGN under power
(+22.30 -> -13.50), after the rendezvous fix (+29.90 -> +11.28). Ten
seeds on this cell are not weak evidence, they are misleading
evidence.

### The conclusion this epic actually reached

Delivery is not the constraint, and this is the cleanest statement of
it the project has:

    more crossings   (+26% formations)
    more colonies    (+22% arrivals)
    LESS score       (-13.50)

Section 8 measured the overseas yield as ~11 cities a game whether
delivery ran at 12% or 28%. Five mechanisms later -- cross-water
worker, the lift, the rendezvous protocol, the hold fix, the slot
relief -- delivery has been repaired, tripled and un-capped, and the
aggregate has never followed. Something downstream of arrival consumes
the gain: the colonies land and are not worth what they cost, or they
displace something better.

**That is where the next epic goes.** Not transport. Site quality, or
what the settlers cost to make.

### The metric was wrong all night: deep's absolute score never moved
    (2026-08-04)

Prompted by asking whether the quarter-boat arm actually FREED shields.
It did not -- with the cut, `unitsbuilt` FELL 99.2 -> 94.5 and `mfg`
68.9 -> 66.4. Nothing was redirected; the AI just built less and lost a
city. But splitting the arms into absolute sides showed something
larger:

    arm          deep   classic   margin   deep cities  classic cities
    ctrl        208.9    167.7    +41.2       16.65        14.20
    combo(cut)  209.0    172.1    +36.9       15.61        14.65
    noboatcut   209.3    170.4    +38.9       16.32        14.47
    slots       207.2    169.3    +37.9       16.59        14.32
    hitch       209.8    170.7    +39.2       16.49        14.11

**Deep's own score is FLAT at ~209 across every intervention** (range
2.6 points, ~1.2%). The margins moved mostly because CLASSIC's score
moved, 167.7 -> 172.1 -- and it moved because deep took fewer cities
and left more for the opponent (deep 15.61 / classic 14.65 in the cut
arm against 16.65 / 14.20 in control).

So every score number reported tonight -- -8.25, -13.50, -17.15,
-20.00, +11.28 -- is a head-to-head margin substantially driven by the
opponent's side of a shared game. `00-context` section 6 refuses
head-to-head as the headline metric for exactly this reason, and it was
used anyway because `liftcmp.py` made it the easy thing to compute.

**What this does and does not overturn.** The MECHANISM results stand
untouched -- they are counts, not margins. The score verdicts become
much weaker: "hitch-a-ride is neutral" and "slots costs score" are now
"unjudged on a metric worth trusting". The one thing the absolute
column does say is that NONE of these interventions moved deep's own
score, which is consistent with the epic's conclusion and reached by a
better route.

**Owed before any of tonight's tuning defaults are trusted: a
separate-game measurement** (`--arms deep,classic` rather than
`mixed_*`), which is what section 6 specifies as primary and which was
never run.
