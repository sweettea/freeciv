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

### The separate-game metric is 8x tighter and was available all along
    (2026-08-04)

First A/B run on `--arms deep` (every seat deep, no opponent), 20
paired seeds, `isolated-150`, per-ocean idle damping:

    damping OFF   score 194.51   cities 14.35
    damping ON    score 195.29   cities 14.65
    diff          +0.78   se 2.22   t=+0.35   up 2/20

The result is neutral. **The standard error is the finding:** 2.22,
against 15-20 for every head-to-head margin taken tonight. Averaging
four deep seats per game and removing opponent variance makes this
metric roughly EIGHT TIMES more precise at identical cost.

`00-context` section 6 names separate-game score as the primary metric
and explicitly refuses head-to-head as the headline. Roughly fifteen
comparisons were run tonight on the noisy one anyway, because
`liftcmp.py` computed it and the tooling shaped the question. Several
of those were called "neutral" or "negative" at |t| < 1 when the same
games on the right metric would have been near-decisive.

**Every score verdict from this session should be re-taken this way
before it is trusted**, and it is cheap: 20 paired seeds is one batch.

### Boat numbers: the fleet has THREE sources, not one (2026-08-04)

    e73207790e / e3a626aad3  price every ferry proposal
    bca82de809               count a docked hull as on its water

Every attempt to shrink the fleet failed, and the reason is structural.
`colony_lift_weight` is one of THREE paths that build boats:

    daidomestic.c:595    "founder's boat"  -- a settler wants passage
    daimilitary.c:1687   "attacker ferry"  -- an attacker needs one
    deepdefence.c        deep's colony want

Neither stock path looks at idle hulls at all. Tuning one of three
inputs cannot set a total, which is why:

    quartering colony_lift_weight   fleet 5.32 -> 4.57, idle 75% both
    per-ocean idle damping          fleet 6.14 -> 5.78, idle 80% -> 79%

and both cost deliveries while leaving ~12 hulls per waiting passenger.

**Two bugs found on the way, both of the same family** -- a position
test standing in for a relationship:

- The per-ocean idle count matched the boat's OWN tile against an ocean
  id. A docked hull sits on the city's LAND tile, so every idle boat
  matched no ocean, the surplus was always 0 and the damping never
  fired: the logged want was identical on and off (307.0 vs 309.7).
- The pricing was then hooked on `choose_building`, which never sees a
  ferry -- advbuilding.c:311 passes a CT_BUILDING choice with need_boat
  cleared. Zero discounts logged in both arms.

**Current state: the pricing sits at the end of deep's per-city choice
loop where every path has spoken, and it EXECUTES -- 96 discounts in a
2-game check. Its effect on fleet size and score is UNMEASURED.**

That is the next thing to run, on `--arms deep`, checking `deep-hull`
before the score.

### Idle share is INVARIANT to fleet size (2026-08-04)

The hull pricing works and the target does not move. 20 paired seeds,
`--arms deep`, `isolated-150`, execution confirmed (deep-hull 0 in OFF,
6940 in ON):

    arm    boats/turn  idle/turn  idle share  arrived  score
    OFF       6.14       4.94        80%       348    196.25
    ON        5.26       4.03        77%       328    198.68

    score +2.42 (se 3.14, t=+0.77, 12/20)   unitsbuilt -6.29 (t=-2.78)

Fleet -14%, idle share 80% -> 77%, arrivals -6%.

**Across every fleet size produced tonight, by five different
interventions, idle share sits at 75-80%:**

    fleet 4.57 -> 75%     fleet 5.78 -> 79%     fleet 6.24 -> 80%
    fleet 5.26 -> 77%     fleet 6.14 -> 80%

Cutting hulls does not make the survivors busier; it removes busy and
idle hulls in the same proportion. **Idleness is not a quantity
problem and no pricing of the COUNT will fix it** (human standard,
2026-08-04: an idle boat means we are doing it wrong, and a fall in
crossings when idle hulls are cut indicts the idle PREDICATE rather
than justifying the surplus).

Arrivals falling 6% while idle share held is exactly that indictment:
the hulls removed were about to be useful. A per-turn snapshot cannot
tell "idle" from "idle now, sailing next turn".

### What to measure instead

Per-HULL idleness over its lifetime, not per-turn fleet aggregates:
how many turns each boat spends with no passenger, and what ENDS that
idleness -- a crossing, or nothing, ever. That separates three causes
a count cannot:

    too many boats        -> fixable by building fewer
    wrong harbour         -> fixable by WHERE they are built
    demand evaporated     -> the settler founded elsewhere, was
                             re-tasked, or died, and its hull is idle
                             forever regardless of fleet size

Only the first is what the pricing addresses, and the invariant says
it is probably not the dominant one.

## Why any individual hull is idle: measured per hull (2026-08-04)

The invariant said fleet COUNT is the wrong lever; this asks the
question a count cannot. `deep_hull_census()` emits one
`deep-hull-turn` line per hull per player-turn (birth, cargo, assigned
passenger, task/ssa, op claim, the water it TOUCHES, the demand on
that water and the distance to its nearest waiting unit);
`benchmark/hullcensus.py` reconstructs lifetimes offline. Logging
only. 20 paired seeds, `isolated-150`, `--arms deep`, pinned binary,
20/20 DONE, 0 pairing violations.

    1244 hulls, 48397 hull-turns (38.9 turns of life each)

    carrying             23.4%      never carried, ever   47.4% of hulls
    assigned, empty       9.5%      birth -> first cargo  median 7 turns
    op-claimed, empty     0.7%
    idle                 66.4%

**The answer: idle hulls are idle because nothing is there.** Of the
32155 idle hull-turns, 84.0% have NO unit waiting to cross that hull's
water at all; 96.8% have none within five tiles. A matching defect
would put idle hulls beside waiting passengers, and that population is
3.2% of idle time.

    wrong harbour   20.5% of hulls   demand NEVER appeared on its
                                     water (15.1% of idle turns)
    missed it       28.9%            idle while demand stood SOMEWHERE
                                     on its water -- but one ocean can
                                     be the whole world sea; only
                                     23.4% ever had it within 10 tiles
    worked          52.6%            carried at least once

Idle spells end: crossing 19.5%, assigned 41.2%, claimed 4.3%, death
5.8%, **never (game end) 29.2%**. 109 hulls (8.8%) died having never
carried anything.

**What it decides.** Fleet count is confirmed wrong from the hull
side: the fleet sits idle where and when there is no demand, so
cutting it removes hulls from the burst as readily as the lull --
which is why five interventions moved the share not at all and cost
arrivals. The cleanly recoverable slice is the wrong-harbour class:
one hull in five is built on a water nothing ever wants to cross, and
those hulls own 15.1% of idle time. That is exactly the useful-oceans
gap already spec'd -- the want is gated on "am I coastal"
(`is_terrain_class_near_tile(TC_OCEAN)`) and never asks whether that
water touches the demand. The remainder is a TIMING problem: bursty
demand against standing inventory, and pricing it needs the wait a
passenger would face without the hull, not the idle count.

Second, smaller waste the old predicate hid: 9.5% of hull-turns are
"assigned, empty" -- a passenger named, nobody aboard -- which the
idle predicate counts as busy. One such turn for every 2.5 spent
carrying.

Caveat recorded in `findings/hull-idleness.md`: demand counts only
units with a `goto_tile` on another continent and no ferry assigned,
so latent demand is undercounted and the wrong-harbour class is if
anything slightly overstated.

Commits: `c6210a10d9` census, `21780b1183` demand distance,
`aafcd3633d` hullcensus.py. `meson test -C build` 2/2 OK;
`tune-audit.py` CLEAN (106/106/106/106).

## The useful-water gate: the first fleet change that RAISES arrivals (2026-08-04)

The boat want was gated on `is_terrain_class_near_tile(TC_OCEAN)` --
"am I coastal" -- and never asked whether that water leads anywhere.
`boat_water_gate` refuses a ferry choice when every sea the city
touches closes on the city's own continent. Pure geometry: the map has
kept the answer since generation in `get_lake_surrounder()`, which
`assign_continent_numbers()` refreshes on terrain change, and no AI has
ever read it (00-context section 3).

Deliberately NOT a demand test. 84% of idle hull-turns have no waiting
passenger because demand is bursty, so "nobody wants it here now"
suppresses the hulls that serve the next burst -- the mistake the five
count-based dampings made. A single-surrounder sea does not become
useful when demand arrives.

**Sized before building** (4 games, 217 hulls): closed-water hulls were
6.9% of hulls but owned 11.2% of all idle time -- more than the entire
demand-blind wrong-harbour class (664 turns against 639), because a
hull on a closed sea can sit beside a passenger it could never serve.
13 of 15 never carried anything.

**Measured**: 20 paired seeds, `isolated-150`, `--arms deep`, one
pinned binary, tuning-file switch, 20/20 DONE both arms, 0 pairing
violations. Deep's four seats averaged per game.

    metric              gate OFF   gate ON    delta      t
    score                 195.00    199.65    +4.65   +1.12   9/20
    cities                 13.95     14.70    +0.75   +1.23
    unitsbuilt             89.04     96.67    +7.64   +3.14
    lift-arrived           17.90     21.80    +3.90   +2.67
    gate refusals           0.00     17.20   +17.20   +7.20
    closed hull-turns     211.30     55.80  -155.50   -5.55

    closed-sea hulls, per hullcensus:   69 (6.2%)  ->  0 (0.0%)

**The mechanism is exact, not approximate: the class it targets goes
to zero.** The residue of `bridge=0` hull-turns in the ON arm is
river-borne Triremes from coastal cities (civ2civ3 lets them navigate
rivers), which is a census artefact and not a hull on a closed sea --
108 of the 109 remaining, and zero hulls whose whole life was dry.

**Arrivals went UP, and that has not happened before.** Every previous
fleet intervention bought a smaller fleet with fewer deliveries; the
hull pricing cut the fleet 14% and lost 6% of arrivals. This is not a
cut at all: the fleet got BIGGER (hull-turns +18.6%, unitsbuilt +7.64,
t=3.14) because shields refused at closed-water cities bought other
things, the empire is slightly larger (+0.75 cities), and the hulls
that do get built are on water that can deliver. +22% arrivals.

**The score is directional, not a claim.** +4.65 at t=1.12 on 20 seeds
is what the brief calls direction; 40 seeds is its bar for a claim, and
this cell has flipped two estimates under power before (rendezvous
+29.90 -> +11.28, slots +22.30 -> -13.50). Do not bank the number.

Honest counter-reading on the demand-defined class: wrong harbour FELL
as a share of idle time (9.8% -> 6.8%) but ROSE in hull count (127 ->
226, 11.4% -> 15.7%), because the fleet is 29% larger. The geometry
class is gone; the demand class is a bigger fleet's problem and is
still the timing question nobody has priced.

**A 40-game A/B was wasted first, and the guard is now in the harness.**
Both arms came back byte-identical: the tuning files had no `[tuning]`
section, so every key was ignored and both arms ran the compiled
default. The tell was not the equal scores -- it was the OFF arm
logging 17.2 executions per game of the feature it was meant to
disable. `run.sh` now refuses a tuning file with no section header, or
with a key absent from `tuning-params.ini`, before any game starts
(`70acda4798`). The off-switch was then PROVEN, not assumed: zero
`deep-watergate` lines in 20 games.

Side result from joining the existing census: hulls are born into zero
demand 68.2% of the time against a 70.0% baseline over all hull-turns,
so a hull's arrival is no likelier to meet a passenger than a random
turn is. Consistent with "the order outlived its reason" but NOT
evidence for it -- the honest test needs the production-start turn,
which nothing logs. Hulls born into demand carried something 68% of
the time against 50% for the rest.

Commits: `159b914815` geometry read, `73bd3ef440` the gate,
`9ff747c634` sizing, `70acda4798` tuning-file guard, `97daac1087`
river/closed-sea split. `meson test -C build` 2/2 OK; `tune-audit.py`
CLEAN (107/107/107/107).

## The gate confirmed at power: mechanism yes, score no (2026-08-04, evening)

Three cells, one pinned binary per mechanism, tuning-file switches,
every arm's execution counter read before its score
(`benchmark/paircmp.py`, committed this session).

**Cell 1 -- isolated-150, 40 paired seeds, gate off -> on.** 40/40 both
arms, 0 pairing violations.

    metric              OFF       ON      delta     se      t    wins
    score            201.70   203.52     +1.82   2.71   +0.67   18/40
    cities            14.25    15.01     +0.76   0.33   +2.26   21/40
    unitsbuilt        87.33    93.15     +5.82   2.00   +2.91   28/40
    lift-arrived      18.82    23.25     +4.42   0.97   +4.58   30/40
    lift-formed       26.48    31.85     +5.38   1.33   +4.03   28/40
    gate-refusals      0.00    15.75    +15.75   1.81   +8.69   40/40

**The 20-seed +4.65 (t=1.12) is RETIRED. Do not quote it.** At double
the power it is +1.82 (t=0.67) with 18/40 wins -- under half, and the
mean halved as n doubled, which is the shape of noise. The mechanism
went the other way: every proxy is STRONGER at 40 seeds than at 20.
The off-switch was proven at scale, not assumed: zero deep-watergate
lines in 40 OFF games.

**Cell 2 -- lategame-80, 20 paired seeds. The gate is worth nothing
there.** score -0.07 (t=-0.16), arrivals -0.35, everything else flat,
while the gate fires 10.65/game (t=+4.34, 17/20). Not harmful, inert.
A verdict inherits its venue, again: this gate's value lives on the
island cell or nowhere, and averaging the two cells would have hidden
that.

**Cell 3 -- the refused shields, isolated-150, 20 seeds, gate ON in
both arms.** The first recovery mechanism (re-ask the domestic advisor
with founder_want zeroed, `d95aedf67a`) executes 17.15/game in 20/20
pairs with refusals unchanged (17.25 -> 17.15, a clean isolation):
score +2.24 (t=0.58, 12/20), landarea +12025 (t=+2.78), cities +0.81,
arrivals +2.05 (t=+2.05), techs -0.62 (t=-1.61).

**And then the mechanism was replaced, because a probe showed it
choosing wrong.** The refused ferry at the traced city came from
deep's own colony-lift want, not the domestic founder's boat, so the
re-ask zeroed a POSITIVE founder want and handed a Temple (97.6) to a
city whose best real candidate was Settlers (750.1). `a81d509b4a`
withdraws the bid where it is FORMED instead -- domestic founder's
boat via founder_want, deep's colony want via the WHICH-water test
section 1c named -- leaving the post-hoc gate as a backstop whose
counter should read zero. Same city, same turns:

    ladder    Settlers 750.1 | Workers  1.0 | Workers  1.0
    re-ask    Settlers 750.1 | Temple  97.6 | Temple 142.9
    at_bid    Settlers 750.1 | Settlers750.1| Settlers750.1

`boat_gate_at_bid` replaces `boat_gate_second_best`. Its own A/B on
isolated-150 is the number cell 3 should be re-read against, since the
re-ask arm carries a known tech-want double-count that the bid-time
form does not.

**Standing reading.** Five delivery mechanisms have now worked and the
aggregate has not followed any of them. This one at least stopped
costing arrivals -- it is the only fleet change that raised them --
and the channel that does move is shields refused at the wrong city
buying other things. That is an argument for placing hulls better, not
for building more of them (section 2 stands).

### Cell 4: bid-time withdrawal, and the third producer it exposed

`a81d509b4a` measured against the same OFF arm as cell 3 (gate ON,
post-hoc veto), isolated-150, 20 paired seeds, 20/20 both arms.

    metric              OFF       ON      delta     se      t    wins
    score            199.65   203.21     +3.56   4.22   +0.85   12/20
    landarea         186825   198950    +12125   4203   +2.89   15/20
    cities            14.70    15.51     +0.81   0.45   +1.80   14/20
    settlers           3.52     2.50     -1.02   0.43   -2.36    4/20
    techs             30.02    29.51     -0.51   0.45   -1.15    5/20
    lift-arrived      23.50    25.15     +1.65   1.10   +1.50   11/20
    gate-refusals     17.25     1.40    -15.85   2.09   -7.59    0/20

Score is direction, not a claim, as ever. Two things it does settle:

**The refactor is behaviour-exact where it should be.** Cell 3's and
cell 4's OFF arms come from different binaries and agree to the digit
on every metric (score 199.65, cities 14.70, refusals 17.25), so
`boat_gate_at_bid = 0` restores the post-hoc path exactly.

**The tech dip is the build mix, not the re-ask.** techs fell 0.62 in
cell 3 and 0.51 here, and only cell 3's arm carried the tech-want
double-count. The double-count hypothesis is not needed to explain it;
refusing hulls at closed-water cities buys land and citizens
(landarea +6.5%, pop +3.98) and slightly less science. Fewer settlers
(-1.02) with MORE cities and land is the same story: settlers stop
standing around waiting for a hull that could never come.

**The backstop is NOT zero, and that is the finding.** 28 vetoes over
20 games (1.4/game) survive bid-time withdrawal, against 345 (17.25)
in the veto arm -- so 92% of ferry bids now withdraw at formation and
a third producer accounts for the rest. It is named in deep's own
comment already: **stock's attacker ferry, `daimilitary.c:1687`**, a
transport for a needy attacker set inside
`military_advisor_choose_build`. Same shape as the domestic founder's
boat and suppressible the same way, at its input rather than after it
wins. Domestic withdrawals ran 113 over 20 games (5.65/game); deep's
own colony bids fell 4827 -> 4448.

### Cell 5: the attacker ferry closed, and a score line that means nothing

`eb6ac2e4aa` against cell 4's ON arm, same 20 seeds, isolated-150,
differing only in the attacker path.

    backstop vetoes        28 -> 0     attacker withdrawals  0 -> 18
    domestic withdrawals  113 -> 101
    score              203.21 -> 202.35   -0.86 (t=-0.78)

**The mechanism is complete**: the backstop reads zero over 20 games,
so all three producers of a ferry bid -- domestic founder's boat,
deep's colony want, stock's attacker transport -- now withdraw at
formation. W36's award step can assume one path.

**The score line is not a result and must not be quoted as one.** The
change fires on 6 of 20 seeds (18 withdrawals, 11 of them in one
seed); 15 seeds are bit-identical. Among the 5 seeds whose score moved
at all: mean -3.45, se 4.55, 2 of 5 up. The tight-looking paired se
(1.10) is an artifact of fifteen zeros, not precision. **The attacker
path is RARE on this cell and its effect is unmeasured, not
measured-flat.** If it matters it is on a war-heavy cell --
lategame-80 or contested -- where attackers actually want transports,
and that is where to measure it if anyone wants a verdict.

Method note this earns: when a change fires on a minority of seeds,
report the fired-on subset and the tie count, or the paired statistic
will quietly claim a precision the experiment does not have.

## W36 step 0: the sizing retires the commission (2026-08-04, night)

The handoff's job was to size W36 step 0 before writing any
commissioning code, on the explicit understanding that it might retire
half the file. It retires the spine. Numbers, caveats and definitions:
`findings/commission-sizing.md`; the verdict block is now at the top of
`workstreams/W36-build-commissioning.md`.

**What was built to answer it.** Two censuses, both instrumentation
only. `deep-hull-order` fires where the build choice COMMITS (not where
production stands: a bought hull is ordered and delivered inside one
turn, and the production vantage saw only 229 of 1359 hulls), carrying
the ordering city's completion ETA against the best a rival on the same
water could offer. `deep-wait` fires once per waiting cross-continent
passenger per turn. `benchmark/commission.py` reconstructs orders and
waiting spells offline as consecutive-turn runs, so no state is stored
and nothing goes stale across a save/load.

**Behaviour-neutrality was proven, not assumed.** The wait census is
bit-identical across the two binaries built this session -- 3291 lines,
1031 spells, every quantile equal -- and the second added the
choice-site log.

**Cells.** isolated-150 and lategame-80, 20 seeds each, `--arms deep`,
one pinned binary, 20/20 DONE both, 0 pairing violations.

### 1. The award buys banked shields, not a better shipyard

                                       isolated-150   lategame-80
    hull orders                        1563 (78/gm)   1074 (54/gm)
    rival advantage, switch penalty: median   2             3
      ... >= 2 turns                       56.0%         60.4%
      ... >= 5 turns                       30.4%         35.1%
    rival advantage, standing start: median  -1            -1
      ... >= 2 turns                       27.3%         31.1%
    ordering city best-or-tied on capability 65.4%        61.7%

The first cut of this number was WRONG and its own output said so: it
credited a rival with its entire shield stock, so a city holding 200
shields toward a wonder scored as a 0-turn producer of a 20-shield
hull, and the median "advantage" was 4 turns of that. Rivals are now
charged `city_change_production_penalty()`, and `bestfresh` -- cost
over surplus, no stock at all -- is logged beside it as the floor.

Read the two together: ranked on production capability alone, the city
that placed the order is already best or equal-best about two-thirds of
the time and the median rival is a turn SLOWER. The 2-3 turn median win
is the rival's SAVINGS, an accounting transfer whose cost the ETA does
not show and W36 did not price.

### 2. The fall-through cascade handles an empty population

A commission with no candidate producer at all: 12 waiting unit-turns
in 20 games on the island cell, ZERO in 20 on lategame. `lift-noboat`
is an order of magnitude bigger than the brief carried (7.30
spells/game island, 2.10 lategame, against "7-10 per 20 games") and
matters less, not more: those spells end WITHIN A TURN and never enrol.
A passenger with no boat stops wanting one rather than waiting for a
producer to appear.

Not the same as a commission REFUSED by the city it lands on, which is
what the cascade is really for -- that one cannot be measured without
building it. What can be said is that its stated justification is gone.

### 3. The wait, finally measured (section 1d asked three times)

                                   isolated-150      lategame-80
    waiting spells                  1031 (52/gm)     166 (8/gm)
    enrolled as a lift core            42.6%            35.5%
    censored (never enrolled)          57.4%            64.5%
      ... still waiting at game end       36               19
    wait to enrolment: median            0                0
                       mean             2.28             0.98
                       p75 / p90       3 / 7            2 / 3
    spells >= 5 turns                  19.8%             5.4%
      ... share of all waiting turns    61.4%            29.4%

**Half of all passengers enrol the same turn they first want a boat**,
and the censored majority stops wanting one rather than waiting
forever. So hitch-versus-wait does not bind in the median case at all
-- and all of its room is in a fifth of spells holding three-fifths of
the waiting. Anything built there is built for the tail, where an ETA
estimate is least reliable, so it must be measured rather than assumed.

### What this leaves

Retired: raise, award, fall-through, cancel. Cancellation goes too --
an order lives a median of 2 turns (island) / 1 turn (lategame) and 85%
of production runs already end without producing a hull, so build churn
is de facto cancellation and there is barely a window for a demand to
evaporate between order and delivery.

Surviving: the passenger's own ETA for the tail, and the two censuses.

Method note this earns: **size the machinery against the population it
would act on, and check that the population is not already handling
itself.** Three of W36's four steps target populations that are empty
(no-producer), self-resolving (noboat spells end in a turn), or already
handled by an existing mechanism (churn cancels orders).

Commits: `d575264247` order census, `874083ecd7` wait census,
`5c21b26813` commit-time vantage + switch-penalty ETA, `619a77b77a`
findings, `9afdb8a8d6` W36 verdict, plus `benchmark/commission.py`.
`meson test -C build` 2/2 OK; `tune-audit.py` CLEAN (108/108/108/108).

**Next**: nothing in W36. The open leads are unchanged and now
unblocked of it -- TUNING is still owed (NEXT-SESSION section 4: the
lift cluster, on training seeds, reported on holdout), and the two
ownerless leads are rates (sci/tax/lux from amortized tech value,
`deephorizon.c:439` reads them and nothing sets them) and trade routes
on merit.

## W34 designed: the objective layer, and a victory plan deferred (2026-08-04)

`workstreams/W34-victory-plan.md`. The bundle's highest-value undesigned
gap now has a file, staged into a phase that is buildable today and a
phase that is deliberately not scheduled.

**Phase A — the objective layer. BUILD THIS.** It absorbs the human
proposal of 2026-08-01 (`findings/objective-layer.md`), which was
accepted, never built, and had no owner. Objective enum in
`struct deep_plr`, forceable from the tuning file, gating four existing
seams as MULTIPLIERS on existing wants (fear conversion, operation
formation, growth ladder, expansion) plus an objective bias on tech
wants. `DEEP_OBJ_BALANCED` at identity weights must be byte-identical
to today's binary -- that is both the A/B switch and the acceptance
gate. Ten new tunables, all placeholders, all owed a tuning pass.

Its real purpose is separability: 108 constants against a diagonal ES
at pop ~6 must be tuned in blocks, but economy and military bid for one
shield budget, so an economy optimum found under wrong fear knobs is an
optimum for spending a starved budget. Pinning the objective CREATES
the separability instead of assuming it. The acceptance criterion is
written to be falsifiable in both directions: if per-candidate variance
does not fall when pinned, the premise is wrong and the workstream says
so instead of proceeding.

**Phase B — the victory plan. DESIGNED, DEFERRED until W04** (human
call). Two independent reasons, either sufficient. Every input is
omniscient -- a rival's spaceship state, exact culture, and the city
counts that make `civil_war_triggered()` computable -- and 00-context
section 2 says the project is unfalsifiable while being fed the
answers; a victory planner is the most answer-fed component we could
build. And it cannot be measured: no victory has ever fired in any cell
in this project's history, and `get_spaceship_score()` (`score.c:367`)
pays only at SSHIP_ARRIVED. A victory-enabled cell must be built WITH
phase B, after W04, so it measures an AI that had to look.

**Found while checking references: `enum winning_strategy` is worse
than vestigial.** Four values, three assignments
(`daidata.c:88`, `daidiplomacy.c:880`, `:1125`/`:1128`), and exactly
ONE read (`daidiplomacy.c:1630`, `!= WIN_SPACE`). `WIN_CAPITAL` -- "we
cannot win unless we take war_target's capital", set when the target
leads the space race, which is precisely the decapitation capstone's
trigger -- is computed and read by nothing at all. A correct inference,
thrown away. Belongs in 00-context section 3's table of infrastructure
built and abandoned in place.

Hard ordering added to 00-context section 7: **W04 before W34 phase B.**

Also recorded: under score truncation "maximise score" is nearly
"pursue an economic objective" (`get_civ_score()`, `score.c:352`), so
phase A is aligned with the metric we have and phase B is not. That is
the honest reason phase A is schedulable and phase B is not.

### W34 phase A corrected the same day: axes, not a menu (human review)

The first draft made the objective a three-valued enum. Wrong twice:
it contradicted `findings/doctrine-architecture.md`, which had already
established these are **not values of one variable** but simultaneous
axes, and it threw away the property that matters -- each player having
its OWN preference rather than picking from a menu of three.

Phase A is now `struct deep_stance { econ, mil, cult }`, a weight per
axis per player, and the enum survives only as named presets for
logging. Gates read a RATIO, so a gate is one gain constant rather than
one per named objective: **eight constants instead of ten, and adding
an axis or a corner adds none.**

**The per-player preference is READ, not invented.** The game already
randomises a personality vector -- TRAIT_EXPANSIONIST, TRAIT_TRADER,
TRAIT_AGGRESSIVE, TRAIT_BUILDER (`common/traits.h:21-29`) -- and the
stock AI already multiplies wants by it in seven places
(`daidomestic.c:417`, `:558`, `:620`, `daicity.c:2042`,
`daimilitary.c:1778`, `:1914`, `daidiplomacy.c:1703`). Seeding the axes
from traits gives four deep seats that behave like four different
players, deterministically, without a second personality system to
disagree with the first.

**Culture is DECLARED and INERT**, with a stated un-defer condition,
because the reasons are measurement facts rather than taste:
`VC_CULTURE` is off in every cell we run (`game.h:589` defaults to
spacerace + allied; the check is `srv_main.c:592-612`), culture is
`culture / 50` in `get_civ_score()` (`score.c:361`), and the lever is
already priced by stock (`daieffects.c:632-644`). An active culture
axis today would re-scale an existing term chasing points our metric
barely counts. It goes live when a victory-enabled cell exists -- which
is phase B's condition anyway.

Ordering consequence inside phase A: the axes stay at IDENTITY through
E34.1 and E34.2, so every gate is proven by PINNING first. E34.3 is
where players become different, and it is the first commit in the
workstream that changes an unpinned game -- deliberately after every
gate has an execution counter, so a behaviour change is attributable to
the trait mapping rather than to a gate nobody exercised.

Two modes of one switch, and a run must log which it was in: FREE
(traits; ships, and what self-play should exercise) and PINNED (all
seats one corner; what makes a tuning block separable). A tuning result
taken free and reported as pinned is a wasted campaign.

## W34 E34.1: the stance surface, and identity proved inert (2026-08-04)

The control surface phase A rests on is built and measured. Four
commits: `f5e6300352` the axes and the pin, `00da5c415a` the log line,
`2d6bd815b4` save/load, `b2090a62b6` gate 1.

**What landed.** `struct deep_stance { econ, mil, cult }` in
`struct deep_plr` (`ai/deep/deepstance.[ch]`), refreshed at phase
begin and saved with the rest of deep state. `stance_forced` in the
tuning file clamps every deep seat to a named corner -- balanced
(identity), economic `(1+span, 1-span)`, military `(1-span, 1+span)`
-- and -1, the shipping default, leaves every player at identity. The
corner depth is `stance_trait_span`, deliberately the SAME constant
the trait step will use in E34.3, so a pinned corner is exactly the
extreme a trait-driven player will be able to reach and a constant
tuned pinned means the same thing free. One `deep-stance` line per
player per turn carries the three weights, the source, FREE vs
PINNED, the corner name and gate 1's cumulative counter. Gate 1
multiplies the ANTICIPATORY fear terms -- overseas danger, boat
shadow, density fear, the pre-contact prior, barbarian onset -- by
`(mil/econ)^stance_fear_gain` at both injection sites, and scales
nothing that prices an observed threat: the survival deficit and the
garrison floor keep full weight, which is what makes "minimise
military spend SUBJECT TO SURVIVAL" true rather than decreed.

### Acceptance

**Byte-identity at identity axes: 20/20, two cells.** Pre-E34.1
binary (`364f9719b8`, built in the bench worktree) against this one,
both pinned to scratch dirs, `--arms deep`, tuning=none, 10 paired
seeds each:

    smoke        scorelogs IDENTICAL 10/10
    lategame-80  scorelogs IDENTICAL 10/10

Not "no significant difference" -- identical sha256 per seed. The
gate's arithmetic is `x * 1.0` at identity and IEEE 754 makes that
exact; the fear terms were hoisted into locals so the counter could
exist without changing evaluation order, and none of those functions
reads `city_data->danger` or the RNG.

**The pin executes, and the counter says so.**

    cell         seeds diverging   gate applications / game   FREE arm
    smoke              9/10          636 .. 2313 (mean 1362)      0
    lategame-80       10/10         2660 .. 4842 (mean 4050)      0

with the pinned arm reading, on every line of every seat,

    deep-stance turn=80 player="P2" econ=1.500 mil=0.500 cult=1.000
      src=identity mode=PINNED preset=economic identity=0 fear_gate=1075

against the free arm's `econ=1.000 mil=1.000 ... mode=FREE preset=-
identity=1 fear_gate=0`. `sort -u` over a pinned game's stance lines
returns exactly ONE distinct stance across all seats, which is the
A.6 criterion for a pinned arm.

**Smoke seed 10 is the honest exception and worth keeping.** 1440
gate applications and a byte-identical scorelog: the danger moved and
no build decision crossed a boundary all game. Executed is not
effective, and the counter is the only thing that tells those apart.

**Save/load round trip.** T8 pinned game, saves kept:
`deep_state.stance_econ=1.500000 / _mil=0.500000 / _cult=1.000000` on
all four deep seats; on reload `deep-stance-load plr=0..3 econ=1.500
mil=0.500`, and plr=4 (no deep state written) defaults to identity
rather than to a zeroed stance. Reloaded WITHOUT the tuning file the
first phase re-resolves to FREE identity -- a pin belongs to the run,
not to the position.

**The regime, reported and not promised.** Gate 1 alone, pinned
economic at the placeholder gain, moves nothing measurable:

    lategame-80, 10 paired seeds, deep arm, fleet totals
    metric        FREE    PIN-ECON     delta     se      t    wins
    score      1049.40     1043.80     -5.60  16.94  -0.33    3/10
    munits      100.20      100.90     +0.70   5.05  +0.14    5/10
    unitsbuilt  252.70      252.50     -0.20   6.03  -0.03    5/10
    unitslost    98.90       92.20     -6.70   7.63  -0.88    3/10
    cities       46.70       47.70     +1.00   1.56  +0.64    6/10
    (smoke, 4 seats: score +3.30 t=+0.66, munits -1.80 t=-0.35)

This is expected and is NOT the A.6 regime-reality check failing: that
check ("military spend must fall toward the survival floor, and if it
does not, a gate is missing") is written against ALL FOUR gates, and
three of them are E34.2. What the numbers do say is that scaling
anticipatory fear by itself does not move the shield split -- the
fears it scales are already discounted by satisfaction and upkeep, and
the survival floor it must not cross is doing most of the work. E34.2
inherits a sharpened version of the claim: if gates 2-4 also fail to
move military spend under a pinned corner, the missing mechanism is
not a gate but the floor's height, and that is a finding to report
rather than a licence to raise the gain.

**Housekeeping.** `meson test -C build` 2/2 OK. `tune-audit.py` CLEAN,
**112/112/112/112** (was 108).

### Owed to the tuning pile

Four new tunables, all PLACEHOLDER, none hand-tuned:

    stance_forced      -1   mode switch; degenerate range on purpose,
                            a campaign pins it by hand per pass
    stance_source       0   A.4's ladder; only 0 (identity) is built,
                            1/2 log an error and fall back
    stance_trait_span 0.5   how different players are allowed to be.
                            Pinned range until E34.3 makes it live in
                            FREE mode -- if the optimiser could move it
                            during a pinned pass, the pin would not be
                            a pin
    stance_fear_gain  1.0   gate 1's sensitivity. Registered in S and
                            MIL, searchable; identity axes give
                            identity behaviour at ANY value of it

`stance_fear_gain` and `stance_trait_span` are the two that want a
pass, and neither can be tuned meaningfully until E34.2 gives the
pinned regime something to be measured by.

### What E34.2 needs to know

- **The shape is fixed: `want *= f(ratio)`, identity at econ==mil.**
  Gates 2-4 copy gate 1's form -- read `deep_stance_fear_mult()`'s
  guard clause. One gain constant per gate, no per-corner constants;
  that is the axis form paying for itself.
- **Every gate needs its own counter on the stance line.** `fear_gate`
  is cumulative per player and zero in the free arm; add
  `op_gate`/`growth_gate`/`expand_gate` beside it rather than a new
  log line, so one grep answers "did anything execute".
- **Do not raise the gains to make the regime appear.** If four gates
  still leave military spend flat under a pinned economic corner, say
  so; `objective-layer.md` section 2's premise is falsifiable and this
  is where it gets tested.
- **The byte-identity gate must be re-run per epic**, against the
  previous epic's binary, with the same recipe: build the pre-change
  commit in `.worktrees/bench`, pin both binaries to scratch dirs,
  `--server`, `--arms deep`, compare scorelog sha256 per seed.
- **Axes stay at identity in FREE mode through E34.2.** E34.3 is the
  first commit in the workstream that changes an unpinned game, and it
  only earns that after every gate has been proven by pinning.

## W34 E34.2: the remaining gates, and the regime that did not appear (2026-08-05)

Gates 2, 3 and 4 are built, proven firing, and proven inert at
identity. The falsifiable claim the epic existed to test -- "pin every
deep seat to the econ corner and military spend must fall toward the
survival floor; if it does not, a gate is missing" -- **fails, and the
missing gate is now named.** Five commits: `7f72f5c8ba` the gains,
`1303e0f6c7` the multipliers, counters and log fields, `236e0fe813`
gate 2, `7a06200351` gate 3, `88aacc3cbc` gate 4.

**What landed.** All three copy gate 1's shape exactly -- `want *=
f(ratio)`, 1.0 at econ == mil at any gain -- but read `(econ/mil)`
where gate 1 reads `(mil/econ)`.

- **Gate 2, assault formation** (`deep_op_form`, `deepops.c`). The
  odds bar the smallest strongest-first prefix of the free pool must
  clear is scaled by `(econ/mil)^stance_op_gain`, capped at 1.0. An
  economy-weighted player commits blades only where the take is
  near-certain; a force-weighted one forms freely. Two floors, neither
  scalable: the cap, so a certain take always forms, and an exemption
  for targets that THREATEN us -- barbarian holdings and cities on a
  landmass where we hold cities, which is `deep_garrison_menace()`'s
  own reachability law read against a war we are already in.
- **Gate 3, the growth ladder** (`deep_adjust_building_wants`,
  `deephorizon.c`). The horizon credits are scaled before the ruleset's
  gold upkeep is charged against them. In a THREATENED city, where the
  only surviving credit is the structural cap-lifter, the gate may
  raise but never lower it: declining growth is a stance, accepting a
  permanent size cap is not. The tech-wall branch is deliberately
  untouched -- that is tech DIRECTION, A.3's gate and E34.4's to build.
- **Gate 4, expansion** (`deepdefence.c`). `founder_want` on the FRESH
  path only, so a cached want never compounds, and the COLONY half of
  the lift demand only -- the amphibious muster's berth shortfall shares
  that want site but is military lift.

### Acceptance

**1. Byte-identity still holds, 20/20, two cells.** E34.1 tip
(`66e0a57517`, built in `.worktrees/bench`) against this tip, both
pinned to scratch dirs, `--server`, `--arms deep`, tuning=none:

    smoke         scorelogs IDENTICAL 10/10
    isolated-150  scorelogs IDENTICAL 10/10

Identical sha256 per seed, not "no significant difference".

**2. Every gate fires under the pin and none fires free.** Pinned
economic corner (`stance_forced = 1`, span 0.5, so econ 1.500 /
mil 0.500), isolated-150, 20 seeds, gate applications per game summed
over the four deep seats:

    gate            mean      range      seeds firing    FREE arm
    fear_gate      3536   1957 .. 4583       20/20           0
    op_gate          57      1 ..  134       20/20           0
    growth_gate   19957  12995 .. 27383      20/20           0
    expand_gate     395    252 ..  568       20/20           0

Every `deep-stance` line in the pinned arm reads `econ=1.500 mil=0.500
cult=1.000 src=identity mode=PINNED preset=economic identity=0`, one
distinct stance across all seats; every line in the free arm reads
identity with all four counters at zero, on all 20 games.

**3. The regime-reality check FAILS, and cleanly.** 20 paired seeds,
isolated-150, same binary, the pin as the only difference. Build
decisions are classified from the `deep-want` line's pick against the
ruleset's NonMil flag and attack-vs-defence values:

    metric              FREE   PIN-ECON     delta     se      t   wins
    military share     0.204      0.211    +0.007  0.020  +0.37   9/20
      offence share    0.069      0.089    +0.019  0.015  +1.27  13/20
      defence share    0.134      0.122    -0.012  0.010  -1.19   7/20
    civilian share     0.351      0.308    -0.042  0.010  -4.31   4/20
    improvement share  0.445      0.481    +0.035  0.018  +1.98  13/20
    score            201.83     194.73     -7.10   4.58   -1.55   8/20
    pop               92.46      85.50     -6.96   2.17   -3.21   5/20
    mean_city_size     7.08       6.45     -0.62   0.15   -4.30   4/20
    munits            29.00      32.18     +3.18   2.66   +1.19  12/20
    unitsbuilt        96.98      87.11     -9.86   3.76   -2.62   6/20
    unitslost         10.95       7.08     -3.88   2.39   -1.62   4/20

**The military share of production does not move.** The pin moves the
CIVILIAN/IMPROVEMENT split instead, and the standing army gets
BIGGER -- because gate 2 stops spending it (unitslost -3.88,
unitskilled -2.38, unitsused -2.10), not because more is bought.

### The missing gate, named

Splitting the pinned corner into its halves (two more 20-seed arms,
same binary, gains zeroed per half) says which gate owns which effect:

    vs FREE            gates 1+2 only        gates 3+4 only
    score                -0.50  t -0.21        -7.33  t -1.68
    military share      +0.011  t +1.04       +0.003  t +0.16
      offence share     +0.006  t +0.95       +0.021  t +1.20
      defence share     +0.005  t +0.82       -0.017  t -1.91
    civilian share      -0.008  t -1.34       -0.040  t -4.57
    improvement share   -0.003  t -0.31       +0.037  t +2.05
    munits              +3.275  t +2.24       +1.875  t +0.73
    mean_city_size      -0.226  t -1.63       -0.580  t -4.01

Three things follow, and none of them is "raise the gains".

- **No gate in W34 A.2 prices the OFFENSIVE BUILD.**
  `deep_assault_want()` (`deepdefence.c:2924`, `attack_value_weight`)
  turns a target's worth into attacker demand and nothing in the
  stance layer touches it: gate 2 governs whether existing blades are
  COMMITTED, not whether new ones are BOUGHT. Measured, offence share
  *rises* under an economic pin (+0.019 all gates, +0.021 gates 3+4
  alone) because the defenders gate 1 discouraged stop competing for
  the slot. That is the missing gate, and it is a gate rather than a
  floor.
- **What is left of DEFENSIVE spend is floor, not anticipation.**
  73% of military city-turns happen where stock's own pre-injection
  danger is exactly ZERO (720.8 of 990.3 per game free, 779.3 of
  1014.2 pinned, delta +58.5 t +0.81; 77% pinned). Gate 1 scales anticipation and refuses
  to scale the survival deficit or the garrison floor, so the pin can
  only reach the quarter of military production that anticipation
  drives. Lowering the floor is a `survival_*`/`garrison_*` tuning
  question, not a stance question -- exactly the sharpened claim E34.1
  handed forward.
- **Gates 3 and 4 fight each other at the placeholder gains.** Gate 4
  raised `founder_want` per city-turn from 320.7 to 1009.7 (+689,
  t +17.07, 20/20) -- a 3.15x on the want -- and expansion city-turns
  FELL by 239 (t -5.27), because gate 3's 3x landed on a credit
  measured on a different scale and improvements outbid settlers. Two
  gates with the SAME gain on two incommensurable wants do not
  preserve their ratio. That is RC4 caught in the act, it is the whole
  of the -7.33 score cost, and it is an argument for tuning these two
  together rather than for either gain's current value.

### Owed to the tuning pile

Three new tunables, all PLACEHOLDER at 1.0, none hand-tuned:

    stance_op_gain      1.0   gate 2. Registered in M and MIL.
                              Cheap: gates 1+2 pinned cost -0.50
                              score (t -0.21) and are the half that
                              can be turned on early.
    stance_growth_gain  1.0   gate 3. Registered in G and ECON.
    stance_expand_gain  1.0   gate 4. Registered in X and ECON.
                              These two MUST be tuned as a pair -- at
                              1.0 each they cost -7.33 score by
                              fighting over the civilian budget.

`tune-audit.py` CLEAN, **115/115/115/115** (was 112).
`meson test -C build` 2/2 OK.

### What E34.3 needs to know

- **The gates are proven inert and proven live; the trait step is now
  attributable.** Every behaviour change E34.3 measures in FREE mode
  belongs to the trait mapping, because the four gates have each been
  shown firing under a pin and shown silent at identity.
- **Do not turn on all four gains at once in FREE mode.** The pinned
  split says gates 1+2 are nearly free and gates 3+4 carry the entire
  cost at their placeholder values. A trait span that reaches the same
  corner reaches the same collision.
- **The regime premise (`objective-layer.md` section 2) is not proven
  by phase A as designed.** An economic stance cannot lower military
  spend while the offensive build want has no gate and three quarters
  of defensive spend sits on an unscaled floor. Either A.2 grows a
  fifth gate on `deep_assault_want` -- one gain, same shape, and the
  survival-floor precedent says it must not scale the reconquest of
  our own cities -- or the claim gets rewritten to be about the
  civilian split, which is the part the axes demonstrably do control.
- **Byte-identity is re-run per epic** against the previous epic's
  binary, same recipe: build the pre-change commit in
  `.worktrees/bench`, pin both binaries to scratch dirs, `--server`,
  `--arms deep`, compare scorelog sha256 per seed.

## D2: credit integrity -- the sawtooth is real and measured (2026-08-05)

Ten commits. The three read-verified defects that made every horizon
credit measurement unsound are fixed and the fixes are proven firing by
census, not by sighting: `9dd9cbc886` the delivery trace, `c1c7d9aa72`
the sawtooth, `9193cc4ffc` + `938cbe07c9` the counterfactual
time-to-grow, `e410d99995` one survival verdict, `972648329a` bundle
reachability, `319c4da6cf` the upkeep dimension, `015c3f81cb` the
caravan legality test, `3a6dc24cdc` the tuning log, `e4d5bd07ce` the
ECON block.

**The headline: the audit's 1-9x sawtooth is confirmed at 4.35x mean
delivery, and the credit the build queue reads is now 1x.** Deep's
happiness column drops from 6.4x baseline to 1.6x with no weight
touched, and the capacity credit now pays in 96% of capped city-turns
against 71%. Score moves on no cell -- -7.50 (t -1.44), -25.07
(t -0.73), +1.77 (t +1.19), -0.62 (t -0.42) -- which is the expected
result of removing an unpriced multiplier from weights that were never
tuned with it. **The tuning pass is now both possible and owed.**

### 1. The sawtooth, before and after (D-B)

`deep_adjust_building_wants()` added into stock's `building_want[]`
every turn; stock clears that array only every `fc_rand(5)+5` turns per
city. Pooled over 20 isolated-150 games, per credited city-turn, with
`post` = what the build decision reads and `add` = what deep intended:

    arm       city-turns   post/add mean  median   p90     max
    BEFORE        92 719           4.35    2.98   6.95    4325
    AFTER         91 886           1.50    1.28   2.06     471

The residue deep found in the array before adding anything (`pre`) fell
905.5 -> 146.5, and in the AFTER arm that residue is entirely stock's
own want: deep contributes exactly its intent, once.

One city across a recalc window, same seed, same cell:

    BEFORE, city 151            AFTER, city 152
    turn   pre     add          turn   pre     add
      60   182.2   214.0          60   124.0   189.2
      61   245.2   233.4          61   124.0   780.3
      62   311.0   274.7          62   124.0   247.7
      63   379.9   254.9          63   124.0   295.7
      64   443.0   663.3          64   124.0   387.4
      65    87.0   199.1  <-reset  65   124.0   652.7
      ...                          66   124.0   171.6
      69   606.8   533.7          67   183.7   172.1  <-stock recalc
      70   975.8   630.3          68   183.7   185.7
      71  1414.3   888.3          69   183.7   186.8
      72  2040.4   136.7          70   183.7   188.0
      74  2235.7   147.8          71   183.7   189.2
      75   208.0   252.1  <-reset  72   141.0   780.3  <-stock recalc

`pre` climbing to 2235 and resetting is the sawtooth; `pre` flat at
124.0 / 183.7 / 141.0 is stock's own want, moving only when stock
recalculates. (Different city ids because the games diverge after the
first behaviour change; the pooled table above is the comparison that
does not depend on that.)

**How it was fixed.** The credit is applied for exactly the span of the
decisions it exists to influence and taken back out at the end of the
build pass (`deep_revert_building_wants()`), so stock's array is stock's
own again between passes -- no cross-turn state, and no stale credit
sitting in the array after the city stopped earning it. The two
alternatives the audit named (credit only on stock's recalc turns;
subtract deep's prior contribution) both leave one. Stock's recalc
schedule is untouched.

Execution census, 20 games: **544.8 revert entries per game, 20/20
seeds, and `stale=0`** -- not one entry was ever clobbered between
apply and revert, so the wall hack and deep's credit never collided.

### 2. The capacity credit now pays in the capped city (D-C)

`deep_capacity_credit()` triggered on `city_turns_to_grow()`, which in a
capped city measures the cap rather than growth. It now triggers on a
counterfactual: the city's citizens on its best food tiles, less its
live food usage, against the granary box -- computed from the CATCHMENT
and not from the arrangement, which is what breaks the deadlock with the
CM without touching the CM. The capped state goes on shedding food the
city genuinely cannot use.

Census of every capped city-turn, 20 games, isolated-150:

    capped city-turns                      3641
    the old trigger would have paid        2588   (71%)
    the new trigger pays                   3499   (96%)
    old ZERO, new pays                      921
      of which the CM had shed the food     586   (obsttg == FC_INFINITY)
      of which the food box was pinned      180   (obsttg == 0)

    cities capped >= 20 turns                59
      old trigger paid on ... of their capped turns   69%
      new trigger pays on                             96%

Sample line: `city=147 size=8 obsttg=1000000000 obssurplus=0 cfttg=10
cfsurplus=4 credit=111.1` -- the CM had shed the food to surplus 0, the
old code read that as "the cap is not the binding constraint", and the
city would have grown every ten turns if the gate lifted.

**A correction to the audit, from the data.** D-C predicted the credit
would be *permanently* zero after the first cap. On this cell it is
*intermittent* instead -- zero in 29% of capped city-turns, not in all
of them -- because migration and famine keep resetting the food box, so
`ttg > 0` recurs. The permanently-pinned case (`obsttg == 0`) is 180
city-turns here. The mechanism is exactly as described; its frequency
on a migration cell is lower than the Europe evidence implied.

**The tech-wall route fired zero times in both arms** on this cell: the
cap-lifting tech is always already known by the time cities cap, so the
branch that routes the credit into `tech_want` has no population here.
It is not evidence about the branch.

**A defect found while measuring, and fixed** (`938cbe07c9`): the first
version of the counterfactual excluded specialists, and in a capped city
the specialists are made BY the cap -- the capped state weights food at
`cm_cap_food` against shield and trade at `cm_cap_*`, so a specialist
beats a food tile and citizens are parked off the land because the city
cannot grow. That read the credit back to zero in 29 of the 34 deadlock
city-turns in the first probe. Every citizen is counted now; the
happiness ceiling is the SOFT cap and `deep_content_credit()` prices it.

### 3. What the mechanism did to the ladder

The happiness column was the sawtooth's headline symptom. Re-taken on
**20 games** of lategame-80 -- the cell `findings/happiness-ladder.md`
measured on -- buildings per 100 cities at end of game:

    column        baseline   deep BEFORE   deep AFTER
    CAPACITY           4.5          31.3          7.7
    CONTENT            4.6          29.3          7.4
    MULTIPLIER         1.4           0.4          0.3
      Temple           0.5           8.1          0.5
      Colosseum        3.6          15.6          6.1
      Aqueduct         0.5           1.6          0.3
      Granary          0.2           2.1          1.8
    mean city size    4.79          5.64         5.11
    cities            1098           917          995

Deep's happiness column was **6.4x baseline** and is now **1.6x**. The
capacity column was 7.0x and is now 1.7x. (The finding's published
figures were 39.6 against 4.0 on six games with an older binary; this
is the same story at 20 games and today's tip.) No weight was touched:
the entire overshoot that the ladder finding attributed to a hot
placeholder was the delivery multiplier.

On isolated-150, the same census over 20 games:

    column / building      BEFORE   AFTER
    CAPACITY                 53.0    47.7
      Aqueduct               11.6    12.4
      Granary                21.4    16.8
    CONTENT                 100.6    77.6
      Temple                 41.3    28.2
      Colosseum              37.7    32.7
    MULTIPLIER               20.4    20.1
      Marketplace             5.4     7.3

-- the happiness column down 23%, and here the cap-lifter RISES against
a shrinking total. It does NOT on garden2b, where every column falls
(CAPACITY 70.9 -> 39.0, CONTENT 109.2 -> 79.0, MULTIPLIER 28.5 -> 20.0,
Aqueduct 9.8 -> 4.3 per 100 cities) while city count rises 31.5 -> 34.2
per player. Both are the same story told by cells with different
constraints, and neither is a claim about whether the new level is
right: that is what the weights are for.

**The clearest single mechanism result is on garden2b's first-build
table.** Supermarket -- the bundle building the README bug was about --
was *picked* 12 813 times across 20 games and actually built in 9 of
them; after, it is picked 1 659 times and built in **13 of 20**. A want
pinned at DAI_WANT_DOMESTIC_MAX by accumulation makes a city re-pick
the same building every turn without finishing it; the same want
delivered once finishes more of them from a seventh of the picks.
Aqueduct's median first turn moves 69 -> 81 on that cell and 75 -> 74
on isolated-150, built in 20/20 games in every arm.

### 4. The A/B: four cells, no cell beyond noise

20 paired seeds per cell (10 on smoke), `deep` arm, pinned binaries
(`--server`), tuning=none. BEFORE is `9dd9cbc886` (the trace commit,
behaviour-identical to the E34.2 tip); AFTER is `938cbe07c9`.

    cell            score BEFORE   AFTER    delta     se      t   wins
    isolated-150         201.82   194.32    -7.50   5.22  -1.44   8/20
    lategame-80          172.38   174.15    +1.77   1.48  +1.19  13/20
    smoke (10)            82.62    82.00    -0.62   1.50  -0.42   4/10
    garden2b             640.23   615.15   -25.07  34.24  -0.73   8/20

**Every execution counter first.** Same seeds, per game, all seats:

    counter                isolated-150     garden2b       lategame-80
    revert entries        0 -> 544.8 20/20  0 -> 598.6 20/20  0 -> 447.2 20/20
    revert stale          0 -> 0            0 -> 7.3   3/20   0 -> 0
    capped city-turns     0 -> 182.1 20/20  0 -> 207.1 20/20  0 -> 90.4  20/20
    capacity credit paid  0 -> 175.0 20/20  0 -> 182.5 20/20  0 -> 85.7  20/20
    tech-wall routes      0 -> 0            0 -> 0            0 -> 0
    credited city-turns   4636 -> 4595      8781 -> 9665      2430 -> 2395

(smoke, 10 seeds: revert 0 -> 323.9 10/10, capped 0 -> 3.7, paid
0 -> 3.5, stale 0.)

Both new mechanisms fire on every seed of every 20-seed cell. The
credit reaches the same cities; it is simply no longer multiplied.

**The one non-zero stale count is worth naming.** On garden2b -- 300
turns, the longest cell -- 7.3 ledger entries per game (1.2% of 598.6),
on 3 of 20 seeds, were found changed between apply and revert and were
left alone rather than subtracted. That is the wall hack writing to an
improvement deep had credited. The consequence is bounded and
self-correcting: one turn of over-credit on one improvement, cleared at
stock's next recalculation. It is zero on every other cell.

**What moved instead of score.** The same shift appears on every cell:
the civilian budget buys fewer buildings and more of everything else.

    metric            isolated-150       garden2b         lategame-80
    unitsbuilt      +1.50  t +0.45   +0.40   t +0.02   +5.58  t +5.81
    settlers        +0.64  t +1.21   +0.05   t +0.16   +0.22  t +1.97
    cities          -0.07  t -0.12   +2.65   t +1.53   +0.65  t +3.41
    techs           -0.60  t -0.80   +2.33   t +1.47   +0.22  t +1.47
    landarea        -3563  t -0.56   +23425  t +1.25   +5817  t +3.36
    pop             -2.79  t -0.87   +5.88   t +0.55   -0.67  t -0.83
    mean_city_size  -0.14  t -0.64   -0.05   t -0.29   -0.49  t -5.21

(smoke: unitsbuilt +4.40 t +3.00, settlers +0.38, cities +0.12.)

That is the expected consequence and not a surprise: the horizon
weights are PLACEHOLDERS that have never been tuned, and whatever
informal calibration they had was done while the delivery multiplied
them by ~4.35. Delivering 1x makes them, by construction, several times
colder than they were behaving, so buildings lose auctions they were
winning and the shields go to settlers and units. On lategame-80 that
trade is worth +0.65 cities and a slightly higher score; on
isolated-150 it costs mean city size and a slightly lower one. Neither
is outside noise, and no cell explains the other -- which is exactly
why the tuning pass this round unblocks has to run before any of these
weights is called right or wrong.

### 5. The small edges (D-H)

- **Upkeep dimension.** The credits sum a per-turn value across MORT
  turns; the upkeep correction subtracted ONE turn of the ruleset's gold
  bill from that number, so a building's recurring cost entered ~24x too
  cheap. Charged over the same horizon now. `upkeep_gold_weight`'s
  default and range are divided by MORT in the same commit (3.0 ->
  0.125, 0..30 -> 0..1.25) so the landed charge is unchanged and this
  round's A/B stays attributable; the knob owes the tuning pile a value,
  and before this no value of it could have been right.
- **Bundle reachability.** A tile counted as viable if it merely had no
  conflicting extra. Now a worker of ours must be able to put the
  partner there: caused by a worker action, terrain accepting it, and
  its requirement vector (tech included) satisfiable. Supermarket is
  0/20 on isolated-150 and lategame-80, so those cells say nothing; on
  garden2b it is built in 9/20 games before and 13/20 after, with the
  sawtooth fix and this one landing together and not separable.
- **Caravan legality.** `caravan_parameter_is_legal()` is called on the
  real parameter instead of a re-derivation that admitted exactly 1.0,
  at which `perpetuity()` divides by zero and every destination scores
  inf. `tuning-params.ini` endpoint 1.0 -> 0.99.
- **Tuning log.** The 115-argument hand-written `deep-tuning` line is
  generated from the loader's own list now, so a knob cannot be loadable
  and unloggable. The five that were --
  `horizon_multiplier_weight`, `caravan_discount`,
  `caravan_foreign_trade`, `site_travel_discount`,
  `site_travel_crowding` -- print. Verified by running the server with a
  tuning file and counting 115 name=value pairs with the override
  visible.
- **Blocks.** `upkeep_gold_weight` and `survival_target` join ECON,
  which they tax; they keep their single home in the S/X/G/M partition.

### 6. One survival verdict per city per phase

`deep_city_survival_verdict()` computes it on the first question of the
phase and remembers it. The CM asked at the top of the turn and the
growth ladder at the bottom, against a map that had moved in between,
so the same city could be arranged for a siege and credited for growth
in the same turn. The two THRESHOLDS are untouched -- `cm_war_survival`
and `survival_target` price the same probability differently on purpose,
and deepcm.c's argument for that is about the price, not the
measurement. The four presence-count consumers the audit lists (melt,
triage sort, veteran guard, severe-danger cutoff) are military and stay
for D1.

### 7. WHICH PAST RESULTS THIS INVALIDATES

Everything in this list measured a credit multiplied by a
phase-randomised 1-9x (mean 4.35, p90 6.95). **Do not quote any of it
unqualified again.**

- **The happiness ladder** (`findings/happiness-ladder.md` section 5;
  STATUS 2026-08-01 "the happiness column"): 39.6 happiness buildings
  per 100 cities against baseline's 4.0, and the conclusion that
  "the placeholder weight of 1.0 is very probably too hot". The weight
  was never delivered at 1.0. Re-taken on the same cell at 20 games,
  the column is 29.3 per 100 cities against baseline's 4.6 before and
  7.4 after -- most of the overshoot was the delivery, not the weight.
- **The multiplier column** (`findings/multiplier-column.md`; STATUS
  "the multiplier column, and three credits that do not score"): the
  three credits whose mechanism landed and whose score did not were
  each measured against an inflated, variance-inflated delivery. The
  *diagnosis* may still stand; the numbers do not.
- **The horizon ablation** (STATUS ~line 2020: zeroing the seven
  horizon credits raised the Settlers share ~18% -> ~50%). That
  contrast is between "no credit" and "4.35x credit", not between
  "no credit" and "the credit".
- **W03's acceptance numbers** (capacity buildings 8.9 vs baseline 3.4
  per 100 cities, and every per-100-cities ladder count taken before
  today): same reason.
- **E34.2's gate 3 numbers** (growth_gate 19957 applications per game;
  the -7.33 score cost of gates 3+4 together; "gates 3 and 4 fight each
  other at the placeholder gains"). Gate 3 scaled a credit that stock's
  array then multiplied by a random 1-9x, so the gain's effective
  magnitude was not the gain. The QUALITATIVE finding -- two gates with
  one gain on two incommensurable wants do not preserve their ratio --
  survives; the magnitude is not a measurement.
- **The Europe T106 attribution** (zero Aqueducts across 85 cities laid
  entirely at the war gate's door): the census above gives the other two
  causes their share.
- Any tuning of `horizon_*` or `upkeep_gold_weight`. None has run;
  R3's corpus and the cloud campaign were correctly blocked on this
  session.

### 8. What the next session must know

- **The tuning pass is now the binding constraint, and it is owed the
  whole ECON block.** The credits' placeholder weights were set when
  delivery was 4.35x; they now deliver 1x and are, by construction,
  4.35x too cold relative to whatever informal calibration they had.
  That is the most likely explanation of the score direction in
  section 4 and it is a tuning question, not a mechanism one. Deep's
  horizon weights, `upkeep_gold_weight` (new scale), `stance_growth_gain`
  and `stance_expand_gain` should be tuned together.
- **The tech-wall branch has no population on any cell run** (zero
  routes in 70 games): by the time cities cap, the cap-lifting tech is
  known. Anyone claiming that branch works or does not needs an
  early/contested cell where the tech is still unresearched. The bundle
  credit has population only on garden2b.
- **E34.2b's fifth gate** (`deep_assault_want`) is unaffected by this
  round: nothing here touches the military want. Its baseline for
  comparison should be taken on the post-D2 binary, since the civilian
  side of the auction just changed magnitude.
- **The harness reaps background batches at roughly 70 minutes.** Three
  launches died mid-cell today: a detached `( ... ) &` inside a tool
  call (killed within ten minutes, at turn 36 of 150), a `setsid nohup`
  launch (killed immediately), and two tracked background runs at ~58
  and ~69 minutes. Only `run_in_background` survives at all, and only
  for about an hour, so a cell longer than that -- garden2b is ~26 min
  per game and took three launches -- must be resumed by relaunching
  the same run.sh, which the DONE-marker cache makes free. Never trust
  `pgrep` here (empty PID namespace); the liveness signals that work
  are DONE counts, game.log mtimes and load average.
- **`deep-capacity`, `deep-credit` and `deep-credit-revert` are the
  three censuses this round added.** Read them before any claim about
  the growth ladder; `stale=0` on the revert line is the invariant that
  says the ledger is sound.

`meson test -C build` 2/2 OK. `python3 benchmark/tune-audit.py` CLEAN,
**115/115/115/115**, before and after.

## W34 E34.2b: the fifth gate, and the floor underneath it (2026-08-05)

The gate E34.2's regime check named is built, fires on 20/20 seeds and
does exactly what it was added to do -- and the regime STILL does not
appear, because what is left is not a gate. Four commits:
`54d71c9545` the constant, `241c9a039f` the multiplier and its
counter, `80a01a8fb9` the gate, `9ee6dff9ea` the W34 amendment.

**What landed.** `deep_assault_want()`'s amortized attacker want --
the one that turns a target's worth into demand for blades -- is
scaled by `(mil/econ)^stance_assault_gain`. It reads the ratio gate
1's way round, not gates 2-4's, because it prices a MILITARY want: an
economy weight must lower the offensive build, not raise it. Applied
after the snap-up sweep, where a positive scalar cannot move an
argmax, so the type the sweep picks is unchanged and only the want it
carries into the auction moves. The exemption is gate 2's own
`deep_op_target_threatens()` -- now non-static -- rather than a second
test: a barbarian holding, or a hostile city on a landmass where we
hold cities, keeps the unscaled want, because reconquest of what
menaces us is survival and not ambition.

### Acceptance

**1. Byte-identity holds at identity axes, 20/20, two cells.** The D2
tip (this session's HEAD `a78b075477`, doc-only above `bf44341570`)
against `80a01a8fb9`, both binaries pinned to scratch dirs with
`libfreeciv.so` beside them, `--server`, `--arms deep`, tuning=none:

    smoke         scorelogs IDENTICAL 10/10
    isolated-150  scorelogs IDENTICAL 10/10

Identical sha256 per seed, and `assault_gate=0` on every `deep-stance`
line of every free game.

**2. All five gates fire under the pin; none fires free.** Pinned
economic corner (`stance_forced = 1`, span 0.5, econ 1.500 / mil
0.500), isolated-150, 20 seeds, applications per game summed over the
four deep seats:

    gate            mean       range        seeds firing   FREE arm
    fear_gate      3858.0   2511 .. 4908       20/20           0
    op_gate          48.8      3 ..  138       20/20           0
    growth_gate   21956.0  15345 .. 30138      20/20           0
    expand_gate     379.6    193 ..  592       20/20           0
    assault_gate    964.8     82 .. 2270       20/20           0

One distinct stance across all seats in the pinned arm (`econ=1.500
mil=0.500 cult=1.000 src=identity mode=PINNED preset=economic
identity=0`), identity with five zero counters in the free arm, on all
20 games.

**How much of the offensive build gate 5 actually reaches: 53%.**
1812.5 `deep-kill` lines per game, 964.8 of them scaled; the other 47%
are exempt as threatening. That is the exemption doing its job on an
island cell where barbarian holdings and shared-continent neighbours
are most of what is at war with us, and it is worth knowing before
anyone reads the gain's effect size.

**3. The regime re-check, on the post-D2 binary. Military share still
does not fall.** 20 paired seeds, isolated-150, same binary, the pin
as the only difference (FREE runs tuning=none, which is identity).
Build decisions classified from the `deep-want` pick against
`units.ruleset`: NonMil or attack 0 is CIVILIAN, attack > defense is
OFFENCE, other units DEFENCE, everything else IMPROVEMENT. Shares are
of build decisions; the rest are per-seat means.

    metric              FREE   PIN-ECON     delta     se      t   wins
    military share     0.201      0.205    +0.004  0.018  +0.21   9/20
      offence share    0.065      0.067    +0.002  0.014  +0.14   9/20
      defence share    0.137      0.138    +0.002  0.010  +0.18  10/20
    civilian share     0.375      0.331    -0.044  0.012  -3.55   5/20
    improvement share  0.424      0.464    +0.040  0.020  +2.00  16/20
    score             193.79     196.16    +2.370  5.395  +0.44   9/20
    pop                89.41      88.96    -0.450  2.984  -0.15   9/20
    mean_city_size      6.914      6.490   -0.424  0.255  -1.66   7/20
    munits             29.04      29.62    +0.582  2.321  +0.25   9/20
    unitsbuilt         97.82      92.40    -5.417  3.068  -1.77   5/20
    unitslost           8.92       9.39    +0.467  1.883  +0.25   8/20

Twenty seeds are direction, not a claim. The direction is: **the pin
moves the civilian/improvement split and leaves the
military/civilian split alone**, which is what E34.2 measured with
four gates and what E34.2b measures with five.

**The one thing the fifth gate did change is the one it was built
for.** The offence-share RISE is gone:

    offence share vs FREE      four gates        five gates
    E34.2 (pre-D2 binary)    +0.019  t +1.27          --
    E34.2b (post-D2)         +0.009  t +0.62    +0.002  t +0.14

and isolated properly -- PIN5 against the same pin with
`stance_assault_gain = 0`, so gate 5 is the only difference and the
pairing is tight:

    metric            gates 1-4      +gate 5     delta     se      t   wins
    offence share         0.073        0.067    -0.007  0.003  -1.87   4/20
    defence share         0.133        0.138    +0.006  0.005  +1.07  11/20
    military share        0.206        0.205    -0.001  0.007  -0.15   7/20
    munits                31.00        29.62    -1.375  1.163  -1.18   5/20
    unitsbuilt            94.24        92.40    -1.837  1.411  -1.30   4/20
    unitslost             10.99         9.39    -1.600  0.885  -1.81   3/20
    score                194.82       196.16    +1.337  1.383  +0.97   9/20

Sixteen of twenty seeds bought a smaller share of attackers. **And the
shields did not leave the army -- they moved from offence to
defence.** Military share is unchanged to three decimals.

### 4. The remaining mass is the garrison floor, and it is a REPORT

Where the freed shields go is where E34.2 said the rest of the army
already was:

    military city-turns at stock's RAW danger == 0
      FREE          690.2 of 974.1   (0.726)
      PIN-ECON      751.1 of 1019.0  (0.744)   delta +0.017  t +0.57
      gates 1-4     751.4 of 1028.9  (0.736)

73-74% of military production happens in cities where stock's own
pre-injection danger is exactly zero. **No stance gate can reach it**:
gate 1 refuses to scale the survival deficit or the garrison floor by
construction, and gate 5 only prices a want that needs a target. The
constants that DO reach it -- `garrison_floor`,
`garrison_floor_danger`, `garrison_menace_slope`,
`menace_overseas_weight` -- are untuned placeholders in the S/MIL
blocks.

**That is a tuning-pile item, not a sixth gate.** A gate on the floor
would be a stance talking a player out of garrisoning cities it cannot
see a threat to, which is the exact line "minimise military spend
SUBJECT TO SURVIVAL" draws, and it would be tuning by mechanism.
W34 A.6 is amended accordingly: the regime claim as written is false
(measured twice, at four gates and at five), and the separability the
workstream is FOR is restated on the split the axes demonstrably
control -- the civilian budget.

### 5. Gates 3+4 re-measured: D2's prediction holds

Same pin, `stance_fear_gain = stance_op_gain = stance_assault_gain =
0`, 20 paired seeds against FREE. E34.2 found these two fighting hard
enough to cost the whole of the pinned score delta; with honest credit
delivery they no longer do:

    vs FREE                E34.2 (pre-D2)        E34.2b (post-D2)
    score                 -7.33  t -1.68        -0.38  t -0.07
    founder want/city-turn +689  t +17.07 20/20  +650  t +12.37 20/20
    expansion city-turns   -239  t -5.27         -27.8 t -0.46   8/20
    civilian share        -0.040 t -4.57        -0.038 t -2.99
    improvement share     +0.037 t +2.05        +0.038 t +1.72

**The founder want still triples and expansion city-turns now barely
move.** The collision was real and it was mostly the sawtooth: gate 3
was scaling a credit that stock's array then multiplied by a random
1-9x, so the growth side of the fight was several times larger than
the gain said. The qualitative warning survives -- two gates with one
gain on two incommensurable wants do not preserve their ratio -- but
the pair no longer costs score at the placeholder values, and
`stance_growth_gain`/`stance_expand_gain` should be tuned together for
their own sake rather than to defuse a collision.

### Owed to the tuning pile

    stance_assault_gain  1.0  gate 5. PLACEHOLDER, registered in M,
                              MIL and ALL. Its measured effect at 1.0
                              under a pinned corner is -0.007 offence
                              share (t -1.87) for +1.34 score
                              (t +0.97) -- cheap, and the direction
                              the axis intends.

    garrison_floor, garrison_floor_danger, garrison_menace_slope,
    menace_overseas_weight -- NOT new, but newly implicated: they own
    73% of military production and no gate can reach them. The MIL
    block's most valuable unexplored dimensions.

`python3 benchmark/tune-audit.py` CLEAN, **116/116/116/116** (was
115), before and after. `meson test -C build` 2/2 OK.

### What E34.3 needs to know

- **All five gates are proven inert at identity and live under a pin**,
  so every behaviour change the trait step measures in FREE mode
  belongs to the trait mapping. That property is now worth more than
  it was: gate 5 is the first gate whose OWN effect has been isolated
  against an otherwise identical pinned arm, and the technique (pin
  everything, zero one gain, pair on seeds) has se an order of
  magnitude smaller than pinning against FREE. Use it.
- **A trait-driven econ player will buy fewer attackers and about as
  many defenders.** That is what the axes do today. If E34.3 expects
  trait differentiation to show up in military SHARE, it will measure
  nothing; seat-level variance should be looked for in the
  civilian/improvement split and in offence-vs-defence composition.
- **Do not turn on all five gains at once in FREE mode.** Unchanged
  from E34.2, and gates 3+4 remain the pair that moves the most.
- **Byte-identity is re-run per epic** against the previous epic's
  binary: build the pre-change commit, pin both binaries (server AND
  `libfreeciv.so`) to scratch dirs, `--server`, `--arms deep`,
  tuning=none, compare scorelog sha256 per seed.
- **The isolated-150 metric definitions are reproducible**: this
  session's classifier reproduced E34.2's FREE arm to within noise on
  the pre-injection-danger census (mil city-turns 974 vs 990, danger-
  zero share 0.726 vs 0.728, founder want per city-turn 337 vs 321) on
  a DIFFERENT binary, which is why the two sessions' tables can be
  compared in shape even though D2 moved the magnitudes.

## W34 E34.3: the players become different, and the cells could not see it (2026-08-05)

The trait step is built and lands with two findings that matter more
than the mechanism. Four commits: `ad94fbdb30` the trait seeding,
`f1bbf7b543` the trait values on the stance line, `fc1f107cc3` and
`9bca4b86df` the two cells that randomise traits at all.

**What landed.** `stance_source = 1` seeds each deep player's axes from
its own traits -- `econ <- EXPANSIONIST/BUILDER/TRADER`,
`mil <- AGGRESSIVE`, `cult` inert (A.1c) -- read through
`ai_trait_get_value()`, the accessor stock uses at all seven of its
trait sites. Each trait is normalised against the range ITS RULESET
allows it (`nation->server.traits[tr].min/max`, mapped to [-1, +1]),
not against `TRAIT_DEFAULT_VALUE`, because E34.1 chose
`stance_trait_span` so that a pinned corner is exactly the extreme a
trait player can reach; only a ruleset-relative index has that
property. The two axes are independent, so an aggressive builder gets
both weights raised and the gates -- which read a ratio -- leave it
playing today's game. The compiled default stays `stance_source = 0`.

### FINDING 1: our cells had no traits to read

`traitdistribution` defaults to **FIXED**
(`GAME_DEFAULT_TRAIT_DIST_MODE`, `common/game.h:517`), no shipping
nation overrides its trait limits (575 files under `data/nation/`,
none with a `trait_` key), and civ2civ3's `[default_traits]` set all
four defaults to 50. **Every AI player in every cell this project has
ever run drew 50/50/50/50** -- confirmed in a savegame's trait block
(`trait={"val","mod"} 50,0 x4`, `"traitdistribution","FIXED"`) and on
the `deep-stance` line.

So A.1a's source is the right source and it is constant here. The
mapping is written to be exactly inert in that case: the military index
is summed three times rather than used directly, because 2x is exact in
IEEE 754 and `(x + x) + x` and `3 * x` round identically, so four
traits at the same relative position give EXACTLY equal axes. Measured:
on the FIXED smoke cell, `stance_source = 1` against `stance_source = 0`
is **20/20 scorelogs byte-identical**, with every line reading
`econ=0.833 mil=0.833 ... identity=1` and all five gates at zero.

The two new cells (`isolated-150-traits.conf`, `smoke-traits.conf`) are
their parents plus `set traitdistribution EVEN`, which draws each trait
uniformly from the nation's own [min, max] -- the distribution the
ruleset ships. EVEN calls `fc_rand()` per trait per player at init, so
these cells pair arm-to-arm and do NOT compare to their parents'
absolute levels.

### Acceptance

**1. Determinism guard: 20/20, two cells.** HEAD's binary
(`136f1882e3`, doc-only above the E34.2b tip `80a01a8fb9`) against this
tip, both pinned to scratch dirs with `libfreeciv.so` beside them,
`--server`, `--arms deep`, tuning=none:

    smoke         scorelogs IDENTICAL 10/10
    isolated-150  scorelogs IDENTICAL 10/10

Plus the stronger claim above: at `stance_source = 1` on a FIXED cell,
20/20 identical.

**2. Four distinct stances free, one under identity.** Both cells,
20 games each, from the `deep-stance` line:

    arm                        distinct stances/game   identity=1 lines
    isolated-150-traits src=1        4 (one game 5)        0 / 12005
    isolated-150-traits src=0        1                 12019 / 12019
    smoke-traits        src=1        4                     0 / 8000
    smoke-traits        src=0        1                  8000 / 8000

(the seed with five is a civil-war splinter seated on deep). A sample
line, unpinned:

    deep-stance turn=100 player="P4" econ=1.128 mil=1.167 cult=1.000
      src=traits mode=FREE preset=- identity=0 trait_exp=80
      trait_bld=86 trait_trd=37 trait_agg=70 fear_gate=158 op_gate=19
      growth_gate=1060 expand_gate=61 assault_gate=239

against the identity arm's `econ=1.000 mil=1.000 ... src=identity
identity=1` with five zero counters. **All five gates fire in FREE
mode for the first time**, isolated-150-traits, applications per game
over four seats:

    gate            mean       range        games firing
    fear_gate      3880.4   2781 .. 5227       20/20
    op_gate          54.4      0 ..  145       18/20
    growth_gate   22637.5  14752 .. 30629      20/20
    expand_gate     424.8    285 ..  610       20/20
    assault_gate    981.0      0 .. 2170       19/20

which is pin-like volume (E34.2b pinned: 3858 / 48.8 / 21956 / 379.6 /
964.8) reached with nobody pinned.

**3. Differentiation: the axes move, the SEATS do not separate, and
the FLEET does.** 20 paired seeds, isolated-150-traits, same binary,
both arms on a tuning file (see finding 2), `stance_source` the only
difference. Shares are of build decisions, classified as E34.2b did;
`sd` is the population SD across the four seats of one game.

    metric               IDENT   TRAITS    delta      se      t   wins
    axis econ sd        0.0000   0.1467  +0.1467  0.0128 +11.47  20/20
    axis mil  sd        0.0000   0.2134  +0.2134  0.0139 +15.38  20/20
    civ_share  sd       0.0748   0.0872  +0.0124  0.0075  +1.66  13/20
    civ_share  mean     0.3736   0.3815  +0.0080  0.0084  +0.94  11/20
    off_share  sd       0.0925   0.0403  -0.0522  0.0105  -4.98   3/20
    off_share  mean     0.0801   0.0402  -0.0400  0.0081  -4.95   4/20
    def_share  mean     0.1319   0.1266  -0.0053  0.0083  -0.63   6/20
    mil_share  mean     0.2120   0.1668  -0.0452  0.0085  -5.32   3/20
    impr_share mean     0.4144   0.4517  +0.0373  0.0139  +2.67  15/20

The civilian criterion is met weakly and in the predicted direction:
seat dispersion rises, fleet mean does not move. **The offence
criterion fails in an informative way**: its dispersion falls because
its MEAN falls -- a share bounded below by zero carries its spread
with its level, and the relative dispersion is flat (sd/mean 1.15 ->
1.00).

**Why offence falls is a GAME effect, not a seat effect, and the
evidence says so.** Sorting all 80 seats by their own `mil/econ` and
taking the paired delta by quartile:

    quartile   ratio    d_off     d_def    d_civ   d_impr  assault_gate
    Q1          0.59  -0.0296  +0.0247  +0.0008  +0.0041      256
    Q2          0.87  -0.0406  -0.0247  -0.0009  +0.0662      283
    Q3          1.18  -0.0847  -0.0214  +0.0374  +0.0686      264
    Q4          1.43  -0.0050  +0.0003  -0.0054  +0.0101      178

    corr(mil/econ, delta off_share) = +0.028

Every quartile buys fewer attackers, including the most militarised
one. A within-game correlation between a seat's trait tilt (econ index
minus mil index) and its build shares is null in BOTH arms -- and note
that the identity arm already contains stock's own trait reads:

    within-game corr, trait tilt vs share    IDENTITY   TRAITS
      civ_share                               -0.080    -0.023
      off_share                               -0.021    -0.000
      mil_share                               -0.081    +0.009
      impr_share                              +0.110    +0.009   (n=80)

At 80 seats the 95% band is about +-0.22, so **a seat's own stance does
not predict its own build mix at this power, in either arm.** What
does move is the whole cell: fewer attackers bought, fewer units built
and lost, more improvements. The mechanism most consistent with that
is gate 2's odds bar -- raising a bar toward certainty removes
operations outright while lowering it only admits marginally worse
odds, so a symmetric spread of stances produces asymmetrically less
war, and war is mutual: the seats that did not change their own
posture still face fewer assaults.

**4. Score: reported, not promised.** Same 20 paired seeds.

    isolated-150-traits            IDENT    TRAITS    delta     se      t   wins
    score                         190.21    195.95   +5.74   4.67  +1.23  13/20
    mean_city_size                  6.41      6.73   +0.32   0.15  +2.18  12/20
    techs                          28.52     29.49   +0.96   0.71  +1.36  13/20
    unitsbuilt                    103.40     95.78   -7.62   2.38  -3.20   4/20
    munits                         32.21     27.95   -4.26   1.81  -2.36   5/20
    unitslost                       9.84      6.06   -3.77   1.50  -2.52   7/20
    unitskilled                     3.69      2.66   -1.02   0.64  -1.60   8/20

    smoke-traits (guard cell)
    score                          79.22     78.78   -0.45   1.33  -0.34  10/20
    munits                         17.71     19.10   +1.39   0.74  +1.87  14/20
    off_share mean                 0.0896    0.1110 +0.0214 0.0077  +2.79  14/20
    off_share sd                   0.0593    0.0778 +0.0185 0.0100  +1.84  13/20

**The two cells disagree on the sign of the offence effect** (+0.021 on
smoke, -0.040 on isolated-150). A verdict inherits its venue, again.
Twenty seeds are direction; forty are a claim. Nothing here is a claim.

**5. Housekeeping and the save.** `meson test -C build` 2/2 OK.
`tune-audit.py` CLEAN **116/116/116/116**, before and after -- this
epic adds no tunable. Save/load: a T101 EVEN game saved four distinct
stances (`deep_state.stance_econ=1.094444 / _mil=0.600000`, etc.),
reloaded as `deep-stance-load plr=0..3` with the same values and
plr=4 (no deep state) at identity, and the first phase re-resolved the
same axes from the traits. Reloaded WITHOUT the tuning file it
re-resolves to identity: a source belongs to the run, not the position,
exactly as the pin does.

### FINDING 2: a tuning file at all changes the game

Discovered while the smoke inertness check came back 12/20 DIFFERENT
with an identity stance and every gate at zero. The cause is not the
stance layer:

    float secfile_lookup_float_default(const struct section_file *,
                                       float def, const char *path, ...)
    (utility/registry_ini.h:457)

`deeptuning.c`'s `TUNE_DOUBLE` passes a **double** compiled default
through that **float** parameter, so loading ANY tuning file quantises
every double tunable to float precision -- including every key the file
does not mention. `site_travel_discount` 0.9 becomes
0.8999999761581421, `site_boat_cost_weight` 0.333333 becomes
0.33333298563957214, and so on for every non-dyadic default.

Measured, two identity arms differing ONLY in whether a tuning file was
loaded (smoke-traits, 20 paired seeds): **12/20 games diverge**, score
79.74 -> 79.22 (-0.51, se 0.43, t -1.20, 4/20). Small, but it is a
second uncontrolled treatment.

**What this means for method: `tuning=none` is not the control for a
one-key tuning file.** Every A/B in this project that compared
tuning=none against a file -- which includes E34.1/E34.2/E34.2b's
pinned arms, all of which read "FREE runs tuning=none, which is
identity" -- carried this perturbation alongside the pin. The pin's
effects are an order of magnitude larger than 0.5 score, so the
qualitative findings stand, but the numbers are not clean. **Both arms
of this epic's comparisons use a tuning FILE** (`stance_source = 0`
versus `= 1`), which is why they are trustworthy. The fix -- a
double-precision lookup for the tuning loader -- is a one-commit change
that this session deliberately did not make, because it changes the
meaning of every existing tuning file and belongs in its own epic with
its own guard.

### The fold decision, for the human

`stance_source` stays **0** in the compiled defaults, as the brief
requires. The numbers a decision would rest on:

- On the only cell where traits vary and where score is measured,
  turning it on is **+5.74 score (se 4.67, t +1.23, 13/20)** with a
  markedly less militarised, more improvement-heavy game.
- On the guard cell it is **-0.45 (se 1.33, t -0.34, 10/20)**.
- On every cell as currently configured (`traitdistribution FIXED`) it
  is **exactly nothing** -- byte-identical, 20/20. Folding
  `stance_source = 1` therefore changes no existing benchmark unless
  `traitdistribution EVEN` is also folded into the cells, which is the
  larger half of the decision.
- Nothing here is significant at n=20. A 40-seed pass on
  isolated-150-traits would make it a claim.

### Owed to the tuning pile

    stance_trait_span  0.5  PLACEHOLDER, and now MEASURED against what
                            it buys: over 80 seats the realised axes
                            span econ 0.656..1.356, mil 0.500..1.483,
                            ratio 0.47..2.03 -- a mean |log ratio| that
                            is 27% of the pinned corner's, best seat
                            68%, only 9/80 seats past half the corner.
                            A uniform trait draw rarely puts all four
                            traits at their extremes, so traits reach
                            about a quarter of the pin. If more
                            differentiation is wanted this is the knob;
                            it is a tuning number and this session did
                            not touch it (rule 7).

Unchanged and still owed: `stance_fear_gain`, `stance_op_gain`,
`stance_growth_gain` + `stance_expand_gain` (as a pair),
`stance_assault_gain`, and the `garrison_*` floor constants that own
73% of military production.

### What E34.4 needs to know

- **Position modulation (A.4 step 3) has the same problem traits had,
  and worse.** Traits at least vary once `traitdistribution EVEN` is
  set; before building position, check what the cell actually offers
  and whether it varies across seats on an ISLAND generator where every
  seat gets a similar island by construction.
- **Measure at the fleet, not the seat, until power improves.** 20
  games is 80 seats, and 80 seats cannot resolve a per-seat correlation
  below about 0.22. The fleet-level differences (military share
  -0.045, t -5.32; unitsbuilt -7.62, t -3.20) were resolvable on the
  same data.
- **Use a tuning FILE in both arms.** Finding 2. `stance_source = 0` in
  a `[tuning]` file is the correct identity control.
- **The trait step is attributable.** All five gates were proven inert
  at identity and live under a pin before this epic, and this epic
  proved them live in FREE mode; so the fleet-level shift measured here
  belongs to the trait mapping.
- **Byte-identity is re-run per epic** against the previous epic's
  binary: build the pre-change commit, pin both binaries (server AND
  `libfreeciv.so`) to scratch dirs, `--server`, `--arms deep`,
  tuning=none, compare scorelog sha256 per seed.

## H1: the tuning loader keeps its doubles, and a guard says so (2026-08-05)

E34.3's FINDING 2 -- loading ANY tuning file quantises every double
tunable to float, including the keys the file never mentions -- is
fixed, guarded in the unit suite, and its blast radius on the archived
files is measured. Two code commits and this one:

    288cc3bfce  ai/deep: keep the compiled double for a tuning key the
                file omits
    c66eafca68  test: guard that a zero-key tuning file loads as no
                tuning file

### The fix

`secfile_lookup_float_default()` takes its default as a `float`
(`utility/registry_ini.h:457`), so `TUNE_DOUBLE` handed it a double and
got back a float-rounded value *even when the file had no such key*.
`deep_tuning_init()` now asks whether the entry exists before reading
it:

    if (secfile_entry_lookup(sf, "tuning." #field) != NULL) {
      deep_tuning_active.field = secfile_lookup_float_default(...);
    }

`utility/` is untouched, so no other caller of the registry changes.
Keys the file DOES set still go through the float lookup, which is what
keeps every archived ini meaning what it meant.

### The demonstration, before and after

Smoke cell, 10 paired seeds, `--arms deep`, pinned server +
`libfreeciv.so`, `--server`; the treatment is
`benchmark/tuning/identity.ini` (a `[tuning]` header and zero keys)
versus `tuning=none`. Scorelog sha256 per seed:

    pre-fix binary dc4803b8 (63169f041c)
      seed  1 file=049c418203d2 none=1dd20000f0d8 DIFFER
      seed  2 file=d41abd7e9bdd none=eb8019912f64 DIFFER
      seed  3 file=624c62cce147 none=624c62cce147 SAME
      seed  4 file=727bbf617388 none=727bbf617388 SAME
      seed  5 file=e55f2b3288be none=24f768bc20c9 DIFFER
      seed  6 file=2a737017ce87 none=435241a98f26 DIFFER
      seed  7 file=5ecad53c072e none=5ecad53c072e SAME
      seed  8 file=c08b4a727226 none=c08b4a727226 SAME
      seed  9 file=c220108bf842 none=2d08dec74ab2 DIFFER
      seed 10 file=64344ee8f0e8 none=64344ee8f0e8 SAME
      => 5/10 games diverge on a file that sets NOTHING

    post-fix binary 48944ee2 (288cc3bfce)
      seeds 1..10 file == none, 10/10 byte-identical

**No behaviour change under `tuning=none`**: the same 10 seeds run on
the pre-fix and post-fix binaries with no tuning file give identical
scorelogs 10/10 (1dd20000f0d8, eb8019912f64, 624c62cce147,
727bbf617388, 24f768bc20c9, 435241a98f26, 5ecad53c072e, c08b4a727226,
2d08dec74ab2, 64344ee8f0e8 -- and note these are exactly the post-fix
file-arm hashes). The fix moves the file path onto the no-file path,
not the other way round.

### The guard

`tests/deeptuning_test.c` (`meson test -C build`, third test) loads
three ways -- no file, zero-key file, one-key file -- and requires the
first two to be byte-identical structs and the third to differ in
exactly its own key. It also asserts the witness default
(`site_travel_discount`) does not survive a float round trip, so the
check cannot go quietly vacuous if that value is ever tuned to
something dyadic. Against the pre-fix loader, same test:

    FAIL: a tuning file with zero keys changed the active tuning;
          site_travel_discount 0.90000000000000002 -> 0.89999997615814209
    FAIL: a one-key tuning file moved something besides its own key

`benchmark/tuning/identity.ini` is the benchmark-side twin: the correct
control arm for a one-key A/B, now that it is provably identity.

### What the fix changes in the archived tuning files: nothing they set

Every ini under `benchmark/tuning/` (20 files) was loaded under both
loaders and all 116 fields dumped at 17 significant digits. **In all 20
files the set of fields whose value changed is disjoint from the set of
keys the file mentions** -- the change is entirely in unmentioned keys
falling back to their compiled double. The archived values were written
as decimal text and re-parse through the same float lookup as before,
verified rather than assumed.

Exactly **17** double defaults are non-dyadic and were therefore being
perturbed by any file load:

    founder_pool_per_site      0.059999998658895493 -> 0.06
    incoming_defence_survival  0.89999997615814209  -> 0.9
    explore_discovery_value    0.05000000074505806  -> 0.05
    site_pressure_factor       0.30000001192092896  -> 0.3
    site_travel_discount       0.89999997615814209  -> 0.9
    site_boat_cost_weight      0.33333298563957214  -> 0.333333
    site_rival_crowding        0.89999997615814209  -> 0.9
    expansion_war_survival     0.89999997615814209  -> 0.9
    caravan_discount           0.94999998807907104  -> 0.95
    cm_war_survival            0.89999997615814209  -> 0.9
    overseas_decay             0.20000000298023224  -> 0.2
    wall_fear_weight           0.30000001192092896  -> 0.3
    tired_attack_floor         0.89999997615814209  -> 0.9
    sortie_win_floor           0.85000002384185791  -> 0.85
    op_take_target             0.69999998807907104  -> 0.7
    invasion_reinforce_rate    0.34000000357627869  -> 0.34
    op_abort_floor             0.34999999403953552  -> 0.35

A file that sets many of them sees few: `a6-best.ini` (93 keys) moves 2
fields, the `liftsweep` files 1, and the one-key sweep files all 17.

Two consequences for old numbers, both benign:

- The sweeps that compared a one-key file against
  `sitesweep/base.ini` -- which is itself a zero-key `[tuning]` file,
  i.e. the same broken control this session names -- were **file vs
  file with the same quantisation in both arms**, so their deltas are
  clean. That was luck, not design.
- Comparisons that ran `tuning=none` as the control against a file
  (E34.1/E34.2/E34.2b pinned arms) carried this perturbation alongside
  the treatment. Their byte-identity results stand; their
  pinned-vs-free deltas keep a small quantisation component that is
  now unmeasurable going forward because it no longer exists.

### Housekeeping

`meson test -C build` **3/3 OK** before and after (the third test is
the new one; 2/2 before it existed). `python3 benchmark/tune-audit.py`
**CLEAN 116/116/116/116** before and after -- this session adds no
tunable and changes no value. `NEXT-SESSION.md` §5's interim rule
("compare file-vs-file, never file-vs-none") is retired and replaced
with the fix and its evidence.

### What this unblocks

- `tuning=none` is a valid control again, so an A/B may use it.
  `identity.ini` remains the better habit when the treatment arm is a
  file: it keeps both arms on the same code path.
- The cloud tuning campaign inherits a loader whose defaults are the
  defaults. Any tuned value it reports is now a value the compiled
  default can be replaced with directly, with no float round trip
  hiding in between.

## R1: the rates leave stock's ladder, and the price that is not there (2026-08-05)

Four code commits and this one. `ai/deep` chooses its own
science/tax/luxury split by comparing the amortized value of what each
point buys, behind `rates_enabled` -- compiled default 0, stock's
ladder untouched, off arm byte-identical to the pre-session binary.

    9fa647545a  ai/deep: expose the marginal citizen value and the
                pacification load
    2bd394990c  ai/deep: price a bulb, a gold and a luxury point in
                want currency
    626de48689  ai/deep: let deep re-choose the rate split behind
                rates_enabled
    b9b77dca51  ai/deep: correct the rate model where its first probe
                showed it degenerate

**Headline: the mechanism executes on 100% of player-turns, moves
every quantity it targets in the intended direction, and loses 49.81
score points (t -8.39, 1/20).** It is a calibration result, not a
mechanism result: the one thing it could not price is a tech, because
the conversion between stock's want currency and deep's does not
exist. `rates_enabled` stays 0 and goes to the cloud pile with its two
gains and a starting range read out of the logged terms.

### 1. The current path, established before anything was built

The brief deliberately did not assert what stock does. Read out of
`daihand.c:211-699`:

**`dai_manage_taxes` is a solvency ladder with a celebration branch,
and it prices no tech anywhere.** It treats the whole empire as one
city (`dai_calc_data` + `distribute`), scans upward in steps of 10 for
the minimum tax rate whose projected balance is non-negative -- or
merely "not worse than a tenth of the treasury per turn" -- then, only
if the ruleset charges tech upkeep, the minimum science rate that
avoids losing a tech. It then tests celebration, and finally picks by a
FIXED preference order: celebrate > balanced treasury and research >
balanced one of them > "risk of tech loss" (max tax) or "risk of empty
treasury" (max science), the last chosen by `adv_wants_science()` and
`dai_on_war_footing()`. Whatever is left over is dumped into science,
tax and luxury in a fixed order. **Science is a residual. No advance,
anywhere in the function, is ever given a value.**

Deep forwarded it verbatim (`deepdefence.c:3945`) and read the result
when valuing multiplier buildings (`deephorizon.c:541-551`) -- a
solvency artefact read as a statement of strategic value, exactly as
the economy audit says.

**Two venue facts that were not in the plan and change the sizing:**

- The celebration branch is gated on `get_player_bonus(pplayer,
  EFT_RAPTURE_GROW) > 0`. `Rapture_Grow` **does not exist in the
  civ2civ3 ruleset at all** (grep: `data/classic/effects.ruleset:681,
  689` only), and every economy cell this project runs uses the
  default ruleset. So on isolated-150, garden2b-150 and lategame-80,
  stock's entire luxury section is dead code: `dai_manage_taxes` never
  considers a luxury rate, and the observed stock splits confirm it --
  **luxury is 0 in 100% of stock's player-turns on the cell measured
  here.** Happiness is left entirely to the city manager, which pays
  for it in entertainers.
- Even on the classic cells, `Rapture_Grow` requires Republic or
  Democracy (`effects.ruleset:685,693`), so celebration is unavailable
  for the whole early game there too.

### 2. What deep does instead

`deeprates.c`. Deep runs stock's function first and then overrides its
choice, which keeps the two parts of stock's policy that are load-
bearing:

- **Solvency stays a constraint**, not a preference. A triple is
  feasible when the projected balance is non-negative, or when the
  treasury absorbs the deficit and still holds `dai_gold_reserve()`.
  A research-solvency mirror was added for rulesets with tech upkeep:
  a bulb deficit is allowed only while the bank covers it. Losing a
  tech to buy a luxury point is not a trade the comparison may make.
- **Celebration stays stock's.** See section 5.

Between those, every legal triple at stock's granularity of 10 (36 of
them at a 60% government maximum, 66 at 100) is scored and the best
feasible one wins.

**The three prices, and the one that had to be abandoned.** All three
terms have to be in ONE currency or the comparison is theatre:

| term | price | source |
|---|---|---|
| gold | `net gold/turn x adv->gold_priority x MORT` | the advisor's own price of a trade point taken as gold; the same number `deep_multiplier_credit` already values a Marketplace's gold half with |
| science | `amortize(bulbs x adv->science_priority, turns-until-the-advance) x MORT x rates_science_gain` | same family; the AMORTIZATION is the part stock never does |
| luxury | per city, `min(luxury/happy_cost, entertainers) x deep_unworked_tile_value(city)` summed, `x MORT x rates_luxury_gain` | the pair `deep_content_credit()` prices a Temple with |

The yields are linear extrapolations of what the empire really
produced last turn -- `bulbs_last_turn / science rate`, gross gold /
tax rate, per-city luxury / luxury rate -- so corruption, waste,
tithes and every Output_Bonus multiplier are read THROUGH rather than
modelled. A rate that was zero has no measurement to divide and falls
back to stock's own trade-share estimate.

**The tech want could not be used as the science scale, and that is
the session's finding.** `plr_data->tech_want[]` is the only place in
the codebase a TECH is priced, and deep snapshots it before
`dai_clear_tech_wants()` wipes it (the array is refilled by the city
loop that runs BELOW the rates, so the live array at rate time is
empty). Spread over prerequisite steps the way `dai_select_tech` does,
it is a real number -- 200 to 16000 on the cell measured -- and it
gates the override and is logged every turn. It cannot SCALE the
science term, because it lives in stock's want currency and everything
else here lives in the advisor's tile-output currency, and the
conversion between them is **W01's `want_t`, which was declared and
never built** (economy audit section 2.1: `want_t` appears in exactly
one place in `ai/deep` and both maps are the identity). Guessing the
ratio would be rule 7. **Naming the missing price is the finding the
brief's stop condition asked for.**

### 3. Three defects the first probe found, before any benchmark

The first cut ran two games and was measured degenerate: every empire
at 0 science and maximum luxury from the moment any city went unhappy.
Three linked causes, all fixed in `b9b77dca51`:

1. **The currencies disagreed by 170x.** Gold was priced at deep's own
   `upkeep_gold_weight` (3 want per gold/turn) against a freed citizen
   at 1300. Gold could never win an argument. Both are now the
   advisor's priority family.
2. **The yield slopes divided by the wrong rate.** Last turn's bulbs
   were divided by THIS turn's science rate -- the one stock had just
   re-chosen -- so one turn at science 0 gave a bulb slope of 0, which
   made science worthless, which set science to 0 again. A trap, not a
   rounding error. Deep now remembers the split it actually left in
   place.
3. **Luxury was pooled and over-counted.** It counted every unhappy
   citizen as reclaimable -- but an unhappy citizen is still on its
   tile and still producing; only entertainer-equivalent specialists
   are off the land. And it compared the empire's total luxury against
   the empire's total entertainers, which credits a content city's
   luxury to its unhappy neighbour. The term is now per city, each
   saturating at its own entertainer count.

**Method note earned here: a two-game probe of the LOG is worth more
than a twenty-game probe of the score.** All three defects are visible
in the `deep-rates` line and none of them would have been diagnosable
from an aggregate.

### 4. The A/B: the mechanism works exactly as designed, and the design loses

Cell isolated-150 (civ2civ3, migration, 4 seats, 150 turns), **20
paired seeds**, `--arms deep`, binary pinned (`638e167d2d0a`, server +
`libfreeciv.so`, `--server`). Both arms load a tuning FILE: control is
`benchmark/tuning/identity.ini` (zero keys, provably identity since
H1), treatment is the same file plus `rates_enabled = 1`. Twenty seeds
are direction.

    metric                       OFF        ON     delta      se      t   wins
    score                     194.32    144.51    -49.81    5.94  -8.39   1/20
    techs                      28.75     13.21    -15.54    0.72 -21.56   0/20
    cities                     14.50     13.25     -1.25    0.75  -1.67   7/20
    pop                        89.67     82.69     -6.99    3.50  -2.00   7/20
    mean_city_size              6.94      7.49     +0.55    0.22  +2.51  12/20
    settlers                    3.50      2.00     -1.50    0.43  -3.46   4/20
    landarea               185212.50 166437.50 -18775.00 8477.89  -2.21   7/20
    unitsbuilt                 98.47     90.16     -8.31    3.35  -2.48   5/20

Proxy metrics, from the per-turn scorelog (mean over the game, and at
the end):

    metric                     OFF        ON     delta      se      t   wins
    scirate_mean             58.63     24.23    -34.41    1.11 -31.08   0/20
    luxrate_mean              0.00     32.62    +32.62    1.06 +30.69  20/20
    taxrate_mean             41.37     43.16     +1.79    1.20  +1.49  13/20
    gold_mean                99.56    144.41    +44.85   11.31  +3.97  19/20
    gold_end                130.24    353.48   +223.24   72.39  +3.08  18/20
    techout_mean             28.44      5.84    -22.59    1.32 -17.18   0/20
    specialists_mean          4.51      1.03     -3.48    0.28 -12.52   0/20
    specialists_end          10.38      3.30     -7.08    0.87  -8.18   1/20
    unhappypop_mean           2.81      3.44     +0.63    0.24  +2.61  12/20
    riot_turns (disorder)     2.04      6.61     +4.58    1.74  +2.64  12/20

**Every one of those is the mechanism doing precisely what it was
built to do.** Luxury goes from a rate stock never touches to a third
of the empire's trade; the entertainer population it was aimed at
drops by 77%; the treasury it protects grows by 45%; cities get
bigger. And the empire finishes with **less than half the techs**, and
the score falls by 50 points with one seed out of twenty in front.

Execution census (per game, 20/20 seeds, ~600 player-turns each):

    mode=set        476.30 per game   deep overrode stock's split
    mode=agree       91.30             deep re-derived stock's split
    mode=unpriced    14.05             no tech want yet (turns 1-2)
    mode=celebrate    0.00             civ2civ3 has no rapture at all

    turns    set  agree  unpri | mean chosen sci/lux/tax
     1-30   1246    713    280 |  47.0/  0.9/ 52.2
    31-60   1690    638      0 |  35.1/ 14.9/ 50.0
    61-90   1971    357      0 |  19.8/ 42.5/ 37.7
    91-120  2280     85      0 |   8.9/ 53.5/ 37.7
   121-150  2339     33      1 |   6.7/ 53.1/ 40.2

Deep starts near stock's split, and from turn 60 -- when the first
cities go unhappy and the CM starts making entertainers -- walks
science down to almost nothing and luxury up to the government maximum.

**The diagnosis is a scale, and it is measurable without another
game.** Over the 9526 overriding player-turns, the winning triple's
own terms average `vlux = 26318` against `vsci = 1969` and
`vgold = 3460`; restricted to the 6337 turns where both the science
and luxury terms are non-zero, `vlux/vsci` averages **4.33**. The
luxury term is roughly half an order of magnitude too hot relative to
the science term, which is exactly the gap the missing tech price
would have closed. `rates_luxury_gain` around 0.1-0.25, or
`rates_science_gain` around 4-13, is where a tuning pass should START.
**That is a handoff number read out of the logged terms, not a tuned
result, and it is not a claim about score.**

**Verdict: `rates_enabled` stays 0 and must not be turned on until the
two gains have been through a tuning pass.** The alternative reading --
"the policy is wrong, revert it" -- is not supported: the policy
executed on 100% of player-turns, moved every quantity it targets in
the intended direction, and lost on the one quantity whose price it
was structurally unable to obtain. That is a calibration result, not a
mechanism result, and the project has been wrong in this exact way
before (the happiness column's "hot weight" that turned out to be the
sawtooth).

### 4a. The off switch, proven

Smoke cell, 10 paired seeds, both arms loading the SAME
`identity.ini`-shaped file; the only difference is the binary.
Scorelog sha256 per seed:

    seed  1 new=1dd20000f0d8 pre=1dd20000f0d8 SAME
    seed  2 new=eb8019912f64 pre=eb8019912f64 SAME
    seed  3 new=624c62cce147 pre=624c62cce147 SAME
    seed  4 new=727bbf617388 pre=727bbf617388 SAME
    seed  5 new=24f768bc20c9 pre=24f768bc20c9 SAME
    seed  6 new=435241a98f26 pre=435241a98f26 SAME
    seed  7 new=5ecad53c072e pre=5ecad53c072e SAME
    seed  8 new=c08b4a727226 pre=c08b4a727226 SAME
    seed  9 new=2d08dec74ab2 pre=2d08dec74ab2 SAME
    seed 10 new=64344ee8f0e8 pre=64344ee8f0e8 SAME
    => 10/10 byte-identical

(`new` = this session's tip, `638e167d2d0a`; `pre` = the H1 tip
`bf6c1bcc81` built in the bench worktree, `1dcd3c7573b7`. Those ten
hashes are the SAME ten H1 published, which is a second, independent
check that neither session moved the no-tuning path.)

Execution counter, same ten seeds: **0 `deep-rates` lines in the off
arm, 3828 in the on arm.** Non-execution and byte-identity are
different claims and both are made.

Smoke guard, 10 paired seeds, off vs on (100 turns, so a weaker cell):

    metric               OFF        ON     delta      se      t   wins
    score              82.00     75.03     -6.97    4.17  -1.67   4/10
    techs              16.02     12.18     -3.85    0.90  -4.28   1/10
    cities              8.50      8.72     +0.23    0.53  +0.43   6/10

Same direction, same cause, less power. Nothing crashed; no game
timed out; `meson test -C build` 3/3 and `tune-audit.py` CLEAN
119/119/119/119 before and after.

### 5. Celebration: the design, deferred rather than half-landed

The human call (2026-08-04) was "AI cities should grow by celebrating
under suitable govt", as one coupled mechanism with the rates. It is
NOT in this epic, and the reason is a sizing fact found while reading:
**celebration is three mechanisms, not one, and its payoff is
venue-and-government-dependent in a way the rates model structurally
cannot express.**

**What celebration pays, per venue.**

- **classic**: `Rapture_Grow` (value 3) under **Republic or
  Democracy** only (`data/classic/effects.ruleset:681-694`). A
  celebrating city with food surplus > 0 grows by one EVERY turn,
  ignoring the food box (`cityturn.c:1127`, `city_rapture_grow`).
  Enormous, and unavailable before Republic.
- **civ2civ3 (every economy cell)**: `Rapture_Grow` does not exist.
  Celebration instead SUPPRESSES `Output_Penalty_Tile` for Despotism,
  Tribal and Monarchy (`effects.ruleset:1480,1494,1508`) -- the -1 on
  every tile producing more than 2 -- plus a civil-war bonus. Broad,
  bounded, and worth nothing once the empire leaves the early
  governments.

**Why it cannot be a term in `deep_rates_pick`.** The luxury term
there is a SLOPE that saturates: each `happy_cost` of luxury buys one
citizen back off the tiles, until the city is content. Celebration is
a THRESHOLD: below the luxury rate at which the CM can find a happy
arrangement it pays exactly zero, and above it the whole payoff
appears at once. A threshold cannot be an argmax term over a linear
model; it has to be a separate CANDIDATE -- "celebrate at luxury L"
against "the best non-celebrating triple" -- scored in the same
currency and compared once.

**The three legs, and why all three must land together.**

1. **Rates.** Find the smallest luxury rate at which the target cities
   can be arranged happy. Stock already does exactly this test
   (`daihand.c:255-260`: a `cm_parameter` with `require_happy = TRUE`,
   `allow_specialists = TRUE`, `factor[O_FOOD] = 20`, queried per
   city), and it is the right test; what is missing is comparing its
   RESULT against an alternative use of the same trade.
2. **City manager.** `pcity->cm_parameter.require_happy` must be TRUE
   for the celebrating cities and must STAY true, or the next
   `first_activities` arrangement drops them straight back out.
   `deepcm.c`'s state machine has no celebrate state; it needs one,
   ranked below `threatened` and above `grow-hard`, whose factors
   match the payoff's shape (food, for rapture on classic; shield and
   trade, for the penalty lift on civ2civ3). **This is the leg that
   makes celebration a CM change and not a rates change**, and the
   reason the rates half alone would set a luxury rate that nothing
   holds.
3. **Government.** The payoff is a government question on both
   rulesets -- classic needs Republic/Democracy for rapture at all,
   civ2civ3 pays only under the early governments. Deep forwards the
   `gov_value` hook to `dai_gov_value`, which temporarily sets each
   candidate government and calls `auto_arrange_workers` on every
   city -- which honours `pcity->cm_parameter`, which deep now sets.
   So a celebrate CM state would silently enter stock's government
   evaluation. That interaction (seam map section 2.C, "not a bug; not
   designed either") has to be decided deliberately before a
   celebrate state exists, not discovered afterwards.

**Pricing it with quantities that are already priced.** Per candidate
city, per turn of celebration:

- classic: one extra citizen per turn = `deep_unworked_tile_value()`,
  bounded by `city_can_grow_to()` -- which is the capacity credit's
  own arithmetic with `ttg = 1`, so no new price is needed.
- civ2civ3: the summed tile-output delta from lifting the penalty,
  from the same catchment walk and the same `adv->*_priority`
  weights.

Cost: `deep_rates_pick`'s value at the celebrating triple minus its
value at the best triple -- already computed, already in the same
currency. So the comparison needs **no new constant** beyond a gain.

**Measurement.** Venue is `*-classic` (rapture pays there and only
there); guard on isolated-150 that nothing regresses. Census:
`deep-celebrate` per turn with the candidate value, the chosen luxury
rate, cities MARKED, and cities that actually celebrated the following
turn -- "marked" and "celebrating" are different claims and only the
second one says the CM held them.

**Sizing warning for whoever builds it.** On a 150-turn civ2civ3 cell
the payoff window closes when the empire leaves Despotism/Monarchy,
and on a classic cell it does not OPEN until Republic. Before writing
any of it, count the player-turns in each cell where a celebration
would have been legal AND paid -- W36's own lesson, three times over:
size the machinery against the population it would act on.
### 6. What the next session must know

- **`rates_enabled` ships at 0.** Nothing in a default build changes.
  The knob, `rates_science_gain` and `rates_luxury_gain` are all
  registered PLACEHOLDERs in the ALL / ECON / G blocks and all three
  go on the cloud pile. The two gains are not decoration: they are
  the entire bridge between a bulb, a gold and a freed citizen, and
  the compiled 1.0/1.0 is the untuned corner of a two-dimensional
  space that has never been searched.
- **The missing conversion is now named and located.** Pricing
  research by the tech want -- the brief's headline, and the
  economy audit's gap 3 -- needs `want_t`, the W01 currency that
  section 2.1 of the audit records as declared and never
  implemented. Any future work that wants to compare a TECH against a
  BUILDING against a GOLD is blocked on the same thing. That is a
  bigger prize than the rates.
- **Stock never uses luxury on the default ruleset**, because its
  whole luxury section hangs off `EFT_RAPTURE_GROW` and civ2civ3 has
  no such effect. Every happiness cost on every economy cell is being
  paid in entertainers by the city manager. Whatever the score says
  about this A/B, that asymmetry is real and is the reason the
  luxury term dominates whenever entertainers exist.
- **`deep_rates_pick()` is a pure function** over a gathered struct
  (`deeprates.h`), so a unit test of the enumeration and the
  feasibility rules costs one test file and no game state. It was not
  written this session (the meson deep tests link only `ai/deep`
  sources, and `amortize()` lives in `server/advisors`); the cheapest
  fix is to add `advtools.c` to that test target.
- **Do not re-open the currency question by guessing.** The 170x gap
  between `upkeep_gold_weight` and the tile-output priorities is a
  measurement of RC4, not a bug to patch. Either both prices are
  right for their own purpose, or one of them is wrong -- and the way
  to find out is a tuning pass, not an argument.

## R2: the caravan gets a price, and the venue turns out to be the finding (2026-08-05)

Five code commits and this one. `ai/deep` prices a caravan by the
route it would actually walk -- caravan.c's own NPV against the real
destinations reachable from the city -- behind `trade_merit`, compiled
default 0, stock's bid untouched, off arm byte-identical to the
pre-session binary.

    3efa5ba78c  ai/default: expose dai_choose_trade_route for deep to
                read its bid
    62e0548987  ai/deep: price a caravan by the route it would
                actually walk
    b741702c2f  ai/deep: stop the fallback ladder injecting an
                unpriced caravan
    8c5c573d29  ai/deep: load the virtual caravan before asking
                whether it may trade
    0a1f73494f  ai/deep: discount the build delay instead of gating
                on it

**Headline: the mechanism executes 3682 times per game, doubles the
trade routes the empire actually establishes (10.0 -> 19.6 endpoints,
t +4.58, 17/20) at unchanged caravan cost, and moves the score by
-1.06 +/- 1.10 (t -0.96, 10/20).** A sixth mechanism that provably
works and is worth zero. The calibration answer is a NUMBER this time
rather than a shrug: deep's amortized want is a median **0.114** of
stock's bid, and a gain of 7 -- which puts it at 0.770 of stock -- is
where the routes appear. `trade_merit` stays 0.

### 1. The venue is the first finding, and it retired the brief's cell

The brief named garden2b-150 or isolated-150. Both are **civ2civ3**,
and civ2civ3 sets

    "National",   0,   "Cancel", "None"
    "NationalIC", 0,   "Cancel", "None"

(`data/civ2civ3/game.ruleset:612-613`). A pct of 0 means the route
**cannot be established at all**, so on every economy cell this
project runs, a domestic trade route is illegal by ruleset and the
only legal partner is a foreign city -- which needs contact, peace
(`Enemy` is also 0) and reachability.

Measured before anything was benchmarked, 2 games per cell:

    cell                 seam lines   with a legal destination
    isolated-150 (c2c3)         290                          0
    primary-120  (c2c3)         134                          0
    smoke        (c2c3)           0                          0
    primary-120-classic        6726                       6458

**Zero.** Not "small" -- zero legal trade destinations in six
civ2civ3 games, deep's price and stock's alike. `classic` pays 100%
national / 200% intercontinental with a `Both` bonus and the
population is alive. The A/B therefore ran on **primary-120-classic**,
and the civ2civ3 cells became the guard.

`findings/trade-routes.md` section 7 flagged the ruleset spread as a
caution ("any distance weight is ruleset-scoped until shown
otherwise"). It is not a caution. It is the binding fact: **the entire
trade-route subsystem is dead code on this project's default
ruleset**, and every conclusion the project has drawn about caravans
from a civ2civ3 cell was drawn from an empty population.

### 2. Where the want forms, and how deep intercepted it

`dai_choose_trade_route()` (`daidomestic.c:172`), one of five producers
inside `domestic_advisor_choose_build()`, whose result deep receives
at `deepdefence.c:3458` and merges through `deep_want_cmp`. That is
deep-owned, so the brief's stop condition does not fire -- but the
merge is a MAX over producers, so intercepting only the returned
winner cannot tell a trade caravan from a **wonder** caravan (same
unit type, and `ADV_CHOICE_TRACK`'s producer tag is compiled out).

So `dai_choose_trade_route` became non-static (rule 2's exception,
`3efa5ba78c`) and deep probes it directly with `plr_data->tech_want[]`
snapshotted and restored. That is the only side effect the function
has, and restoring it is what makes the probe inert -- re-running
`domestic_advisor_choose_build()` instead would have doubled the tech
want of every producer in it.

Three outcomes, all counted:

- **replace** -- stock's bid is the returned winner; its want is
  overwritten. When that LOWERS the bid the runner-up must come back
  or the city builds a caravan deep just said is not worth it;
  `building_advisor_choose()` is provably side-effect-free (it reads
  `building_want[]` and calls the AI's `choose_building` hook, which
  is `dai_build_adv_override`, also pure), so the improvement
  runner-up is recovered exactly. Worker, founder and wonder runners-up
  are not recoverable without tech-want side effects; that censoring is
  logged as `recover=partial` and measured at **1.65 of 242 replaces
  per game (0.7%)**.
- **promote** -- stock's bid lost but deep's price beats the winner.
- **keep** -- neither, logged anyway because that is most of the
  calibration corpus.

### 3. The price, and the one constant it needed

`caravan_find_best_destination()` on a **virtual** caravan homed in the
city: same evaluator, same `caravan_discount`, same
`deep_caravan_reach()` the unit will get the moment it exists, so the
price and the plan cannot disagree (RC5). Three deliberate departures:

- `horizon = MORT` instead of `FC_INFINITY` -- every other deep credit
  is summed over MORT, an infinite horizon turns the route into
  `perpetuity()`'s 20x, and it makes the pathfinder walk the whole map
  instead of stopping at the planning horizon.
- `consider_wonders = FALSE` -- the wonder caravan is a separate
  producer this epic does not touch.
- `consider_trade = FALSE` when every route slot this city holds is
  already claimed by a caravan in the field. That is the honest
  marginal value of the Nth caravan and it replaces stock's
  `if (unassigned_caravans * 10 > want) want = 0.1`. It fired on 97 of
  71166 seam lines at gain 1 and **1722 of 71807 at gain 7**, i.e. it
  does real work exactly when more caravans exist.

Two conversions, no new constant: divide the NPV by **MORT** (which is
exactly `amortize()`'s own annuity factor -- its discount is
1 - 1/MORT, so a perpetual per-turn flow of x is worth x*MORT and this
is the inverse) to turn a stock into the per-turn flow build wants are
denominated in; and price an output point at `adv->science_priority`,
what `deep_unworked_tile_value()` already pays for tile trade. Then
`amortize()` over the turns to BUILD, the way the horizon credits
amortize over time-to-grow -- the walk is already discounted inside
the NPV and is not charged twice.

`trade_merit_gain` (PLACEHOLDER 1.0) is the honest residue and is
section 6's whole subject.

**One upstream bug was found by building this** (upstream-bugs.md #34):
`action_speculate_unit_on_city(ACTION_TRADE_ROUTE)` -- the documented
way to price a unit before building it -- **segfaults on a virtual
unit**, because `actres.c:927` reads `unit_current_goods()`, which
returns `punit->carrying` unexamined on any ruleset with
`goods_selection = "Leaving"` (civ2civ3's), and dereferences it one
line later. Measured: isolated-150 seed 1, turn 127. Deep gives the
ghost the goods its city would load it with, without
`goods_from_city_to_unit()`'s `fc_rand()` tie-break -- a planner must
not consume the game's random state.

### 4. The A/B: routes double, score does not move

Cell **primary-120-classic** (6 players, FRACTAL, 400 tiles/player,
classic ruleset, 120 turns), **20 paired seeds**, `--arms deep`,
binary pinned (`ac74b9367d3a`, server + `libfreeciv.so`, `--server`).
All three arms load a tuning FILE; control is
`benchmark/tuning/identity.ini` (zero keys, provably identity since
H1). Twenty seeds are direction.

Execution census first (per game, 20/20 seeds, off arm in every case
exactly 0):

    metric                    OFF    ON g=1    ON g=7
    seam evaluations         0.00   3682.35   3717.35
    mode=replace             0.00    242.50    234.05
    mode=promote             0.00      0.45    165.95
    mode=keep                0.00   3439.40   3317.35
    recover=yes              0.00    240.45    145.75
    recover=partial          0.00      1.65      7.00
    ladder rung refused      0.00      2.15      1.70

Mechanism metrics:

    metric                    OFF    ON g=1  delta      t   wins
    caravans built          354.00   217.35 -136.65  -7.51   2/20
    trade dispatches          6.10     1.45   -4.65  -6.37   2/20
    wonder dispatches         9.20     8.15   -1.05  -1.09   8/20
    ROUTE ENDPOINTS          10.00     3.50   -6.50  -6.54   1/20
    ladder injections         3.50     0.00   -3.50    --    0/20

    metric                    OFF    ON g=7  delta      t   wins
    caravans built          354.00   363.15   +9.15  +0.38  12/20
    trade dispatches          6.10    21.95  +15.85  +3.59  19/20
    wonder dispatches         9.20    10.30   +1.10  +0.94  11/20
    ROUTE ENDPOINTS          10.00    19.60   +9.60  +4.58  17/20
    ladder injections         3.50     0.00   -3.50    --    0/20

Route endpoints are counted from the FINAL SAVE (`traderouteN` columns
of each player's city table), not from an intention log: an endpoint is
a route that exists, and each domestic route contributes two.

Score:

    metric                    OFF    ON g=1     delta      se      t   wins
    score                  138.40   137.78     -0.63    1.31  -0.48   9/20
    cities                  20.97    20.58     -0.38    0.22  -1.71   6/20
    techs                   29.16    28.97     -0.19    0.30  -0.63   7/20
    unitsbuilt              94.73    92.22     -2.52    1.25  -2.02   4/20
    landarea            265550.00 259775.00 -5775.00 2600.47  -2.22   5/20

    metric                    OFF    ON g=7     delta      se      t   wins
    score                  138.40   137.34     -1.06    1.10  -0.96  10/20
    cities                  20.97    20.76     -0.21    0.39  -0.53   9/20
    techs                   29.16    28.82     -0.34    0.27  -1.25   6/20
    unitsbuilt              94.73    95.28     +0.54    1.20  +0.45  13/20
    munits                  34.12    31.73     -2.40    0.88  -2.72   4/20

**Both arms are flat on score and both are unambiguous on mechanism.**
At gain 1 the price halves route formation; at gain 7 it doubles it,
at the same caravan count -- so the gain is not buying MORE caravans,
it is buying caravans that go and open routes instead of helping
wonders or idling. And 20 games of that is worth about a point.

### 4a. The off switch, proven

Smoke cell, 10 paired seeds, both arms loading the same identity-shaped
file; the only difference is the binary. Scorelog sha256 per seed:

    seed  1 new=1dd20000f0d8 pre=1dd20000f0d8 SAME
    seed  2 new=eb8019912f64 pre=eb8019912f64 SAME
    seed  3 new=624c62cce147 pre=624c62cce147 SAME
    seed  4 new=727bbf617388 pre=727bbf617388 SAME
    seed  5 new=24f768bc20c9 pre=24f768bc20c9 SAME
    seed  6 new=435241a98f26 pre=435241a98f26 SAME
    seed  7 new=5ecad53c072e pre=5ecad53c072e SAME
    seed  8 new=c08b4a727226 pre=c08b4a727226 SAME
    seed  9 new=2d08dec74ab2 pre=2d08dec74ab2 SAME
    seed 10 new=64344ee8f0e8 pre=64344ee8f0e8 SAME
    => 10/10 byte-identical

(`new` = this session's tip `ac74b9367d3a`; `pre` = the R1 tip
`3cf68458da` built in the bench worktree, `305e3eed09b8`. Those are the
SAME ten hashes H1 and R1 published, a third independent check that no
session has moved the no-tuning path -- and, since `3efa5ba78c` touched
`ai/default`, the demonstration rule 2 requires for the visibility
change.)

Execution counter, same ten seeds: **0 `deep-trade-merit` lines and 0
`deep-trade-fallback` lines in the off arm.** Non-execution and
byte-identity are different claims and both are made.

**Smoke guard, 10 paired seeds, off vs on** -- deliberately a civ2civ3
cell, i.e. the population section 1 measured as empty:

    metric              OFF       ON    delta     se      t   wins
    score             82.00    81.97    -0.03   0.06  -0.43   1/10
    cities             8.50     8.50    +0.00   0.00    nan   0/10
    techs             16.02    16.00    -0.03   0.02  -1.00   0/10

    seam evaluations   0.00     1.10   (all at merit 0 AND stock 0)
    replaces           0.00     0.00
    promotes           0.00     0.00
    ladder refusals    0.00     0.00

The seam fires and finds nothing, exactly as the ruleset says it must.
Nothing crashed; no game timed out; `meson test -C build` 3/3 and
`tune-audit.py` CLEAN 121/121/121/121 before and after.

### 5. The fallback ladder, and where caravans actually come from

The ladder's first rung built a caravan at want 1 whenever a city
wanted nothing else. Measured on the off arm: **3.50 per game**, and 0
on both on-arms -- the rung is unreachable under `trade_merit` and the
refusal is counted (2.15 and 1.70 per game respectively, i.e. the
ladder DID reach it and was turned away).

The `deep-want` line answers a question the project had not asked. Of
354 caravan build decisions per game on the off arm, the winning want
distribution is **p10 68.5, p50 197.3, p90 609.7** -- while deep's
trade price is a median 8.3 and stock's trade bid a median 70.9. **The
caravan build queue is dominated by the WONDER producer, not the trade
producer**, by roughly an order of magnitude in want. That reframes
the epic: pricing trade routes properly moves a minority of caravan
builds, which is a large part of why doubling the routes is worth a
point.

### 6. The calibration verdict: the amortized want needs an ~8x deflator

The corpus, over 71166 (gain 1) and 71807 (gain 7) seam evaluations
with a legal destination -- 96.6% of all seam lines on this cell:

    term                mean     p10     p50     p90     max
    npv (output pts)   28.02   12.80   27.90   41.70  101.20
    eta (turns)         9.59    6.00    8.00   17.00   24.00
    build (turns)      36.21    7.00   19.00   49.00      --
    stock's bid        77.80   66.40   70.90   79.30 1654.30
    merit, gain 1.0     9.66    0.20    8.30   20.70   63.40
    merit, gain 7.0    65.96    1.30   54.90  142.50  531.00

    merit / stock, gain 1.0:  p50 = 0.114   mean = 0.195
    merit / stock, gain 7.0:  p50 = 0.770   mean = 8.75

**Answer to the brief's question: no, the amortized want does not land
on the building scale at gain 1.0 -- it lands about an eighth of the
way up it, and R3's deflator is ~7-9x** (7 / 0.770 = 9.1 to sit
exactly on stock's median; 7 is where the route count doubles). Unlike
R1 this is not a guess read off a log: both ends were run and the
mechanism responded monotonically in the right direction at both.

Where the eighth goes is decomposable and is not a mystery:

- **the build delay is the largest single term.** Median build 19
  turns, `amortize(.,19)` = `(23/24)^19` = 0.446. Stock charges
  nothing for it at all.
- the walk costs another `0.95^8` = 0.66 inside the NPV.
- dividing by MORT converts a 24-turn stock into a per-turn flow.

And stock's bid is, on inspection, **mostly not a valuation**: its p10
and p50 are 66.4 and 70.9 against an interquartile spread of ~13,
because `want += trader_trait` adds a flat 50 whenever the city has no
route. The two valuations proper -- stock's income term and deep's
amortized NPV -- are within a small factor of each other; the gap is
the constant.

**Verdict: `trade_merit` ships 0.** Not because the mechanism is
wrong -- it moves the quantity it targets, in the right direction,
monotonically in its own gain -- but because the only cell where it
can act at all is a `-classic` cell that is not in the tuning
campaign, and one point of score is not worth turning on an untuned
knob. It goes to the cloud pile with the deflator range above.

### 7. What the next session must know

- **`trade_merit` and `trade_merit_gain` ship at 0 and 1.0.** Both are
  registered PLACEHOLDERs in ALL / G / ECON. The gain's useful range
  is **5-10**, read off two measured arms, not one log.
- **civ2civ3 makes domestic trade routes illegal** (`National` pct 0).
  Before any further caravan, trade or `caravan_*` work: the economy
  cells cannot see it. Either the work is scoped to `-classic` cells,
  or a civ2civ3 cell with real foreign contact has to be built and its
  population counted FIRST. Section 1's table is the template; it cost
  six games.
- **`action_speculate_unit_on_city` segfaults on virtual units for
  trade actions** (upstream bug 34). Anyone pricing an unbuilt unit
  through the action system will hit it. The workaround is three lines
  in `deeptrade.c`.
- **The caravan build queue belongs to the wonder producer.** p50 want
  197 against the trade producer's 71. `dai_choose_help_wonder`
  (`daidomestic.c:66`) is untouched by this epic and is the larger
  claimant; whether IT should be amortized is now the open question,
  and the seam and the census are already built.
- **What R3's corpus must include from here.** The `deep-trade-merit`
  line is the first place in this project where deep's price and
  stock's price for the SAME decision are logged side by side, per
  city, per turn: 145k paired evaluations over 40 games. That is the
  W01 conversion table's first real data. It says that for the trade
  producer the ratio is 0.114 with the build delay explaining 0.45 of
  it, and it says stock's number is 70% a constant. Any `want_t` work
  should start from this file rather than from an argument, and should
  add the same two-price logging to the producers that do not have it
  -- the wonder caravan first, since section 5 shows it is the one
  that decides most caravan builds.

## D1: military discipline -- one hold revived, two with no population (2026-08-05)

Seven code commits. The six read-verified defects of
`findings/deep-dive-2026-08-04/military.md` (C1-C5, C7) are fixed, each
with its own execution counter: `3dafe7fd48` the phase-scoped holds,
`ae7aff55e0` sortie claim discipline, `fb4654afed` the melt's survival
question, `321861bd4a` triage on probability, `94f12010fa` the build
side's barbarian exemption, `9e528aea4f` the op-aware attacker pool,
`513086bef2` the empire-centroid target anchor.

**The headline is a correction to the audit.** C1 is real and the fix
works -- proven unit by unit, same seed, same turn -- but of the "three
landed mechanisms" the done-flag reset was said to disable, only ONE has
any population at all. **The tired-attack hold and the convoy HOLD have
never fired in a single benchmark game this project has ever run** --
zero `deep-tired` and zero `deep-convoy` lines across all 206 result
directories under `benchmark/results/` -- and the reason sits UPSTREAM
of the done flag in both cases. No done-flag fix could have revived
them. Score is flat on both cells and three of the six fixes are
isolated against their own probe binaries; none of them is the cause of
anything.

### 1. The three binaries the evidence needed

    BEFORE   6ed0814108   aca9e506d839   the pre-session tip
    AFTER    513086bef2   3852a5004c83   this session's tip
    PROBE    (uncommitted) 8d36ca4734c4  AFTER with hold preservation
                                         disabled and the census kept
    PROBE2   (uncommitted) 37a9d863dfae  AFTER with the two sortie
                                         claim checks disabled
    PROBE3   (uncommitted) 11e1e1f57960  AFTER with the list-head anchor

All pinned to scratch dirs with `libfreeciv.so` beside them, `--server`,
`--arms deep`, tuning=none, 20 paired seeds per arm. PROBE exists
because the BEFORE binary has no census: "the counter fired and the
behaviour was absent" is a claim about individual units, and only a
binary that keeps the census while dropping the fix can make it.

### 2. Dead, then alive: the worker flee, unit by unit

`deep-hold` is emitted from `last_activities`, i.e. after BOTH drivers
have had their chance at the unit -- the manage loop and
`auto_workers_player()`. lategame-80, 20 games each:

    arm      why     holds   alive   same tile   spent no moves
    PROBE    flee    16821   16545    6295 (37.4%)   6533 (38.8%)
    AFTER    flee    14298   14168   14156 (99.0%)  14287 (99.9%)
    AFTER    (contested)  9091  9035   9009 (99.1%)   9068 (99.7%)

Six of every ten fled workers were moved again in the same turn they
were logged as fleeing. The same unit, the same seed, the same turn:

    seed 3, P2 unit 140, turn 18
      PROBE   deep-hold ... why=flee alive=1 same_tile=0 moved=1 spent=10
      AFTER   deep-hold ... why=flee alive=1 same_tile=1 moved=0 spent=0

The second, independent driver mattered: `auto_workers_player()` reaches
every worker of an AI player at end of phase and never reads the flag,
so the fix needed a consumer in `deep_auto_settler_run()` as well as in
the manage loop.

**The flee mark count falls when the hold holds** -- 785.5 -> 636.9 per
game (t -4.26), and 841.2 -> 636.9 against PROBE (t -4.72). A worker
that stays fled does not have to decide to flee again next turn.

**The consolidate garrison is real but rare**: 0.10 holds per game on
lategame-80 (2 events in 20 games), 0 on contested. It is not evidence
either way at that volume, and it is not a reason to doubt the fix.

### 3. The two mechanisms with no population, and why

**The tired-attack hold cannot fire at the seam it runs in.**
`deep_hold_tired_attackers()` requires `moves_left < SINGLE_MOVE`, and
it runs in `first_activities` -- where `update_unit_activities()`
(`server/srv_main.c:1336`, which calls `unit_restore_movepoints`) has
just given every unit its full movement back. A unit becomes tired
DURING the manage loop, not before it. Zero firings in 206 result
directories is what that predicts. `tired_attack = TRUE` in civ2civ3, so
the ruleset gate is not the cause.

**The convoy HOLD cannot fire either**, for the reason STATUS already
published on 2026-08-01 about the port-scan invade gate: stock's ferry
system boards and sails within one manage pass, so a loaded transport
resting in port at phase time does not exist. The escort half of the
same pass has never logged a line either.

Both fixes are still correct and both stay: they are the behaviour the
code says it wants, and they cost nothing. But **the audit's C1 prize
-- "three landed mechanisms go from zero to live for ~5 lines" -- is one
mechanism, not three.**

### 4. The A/B: flat on both cells, and no fix owns a regression

20 paired seeds, `--arms deep`, pinned, tuning=none.

    cell            score BEFORE   AFTER    delta     se      t   wins
    lategame-80          174.15   172.93    -1.23   1.36  -0.90   6/20
    contested             80.88    79.54    -1.34   1.52  -0.89   8/20

Neither is outside noise, and both point the same small way, so three
fixes were isolated against a probe binary that differs from the tip in
exactly one of them (the E34.2b technique: pin everything, disable one
thing, pair on seeds -- the se comes down by half or more):

    isolation                     cell         delta     se      t   wins
    fix 1  phase holds            lategame-80  -0.32   1.19  -0.27   8/20
    fix 2  sortie claim checks    contested    +0.72   0.63  +1.14   9/20
    fix 6  centroid anchor        lategame-80  +0.36   1.32  +0.27  12/20

**The two largest behaviour changes are the two that look mildly
positive.** Fix 2 on contested also moves what it should:

    metric              no claim check   with it    delta     se      t
    sorties                     62.00      56.40    -5.60   2.78  -2.02
    unitskilled                 20.08      18.79    -1.29   0.70  -1.83
    unitslost                   29.06      27.84    -1.22   0.85  -1.44
    unitsbuilt                  61.86      60.96    -0.90   0.67  -1.34

and fix 6 forms more operations (11.95 -> 14.65 against BEFORE, t +1.65;
12.15 -> 14.65 against PROBE3, t +1.86, 14/20). The bundle's -1.2 is not
attributable to any of the three isolated fixes, and the three
un-isolated ones (melt, triage, barbarian gate) have populations one to
three orders of magnitude smaller. **Twenty seeds are direction.**

### 5. Every fix's own counter, per game, all seats

    counter                lategame-80 (B -> A)     contested (B -> A)
    hold lines             0 -> 715.00  20/20       0 -> 454.55  20/20
      of which flee        0 -> 714.90              0 -> 454.55
      of which garrison    0 -> 0.10    2/20        0 -> 0
    claimskip pass=kill    0 -> 7.45   17/20        0 -> 120.05  20/20
    claimskip pass=cover   0 -> 2.70   12/20        0 -> 12.95   20/20
    claimskip pass=melt    0 -> 11.20  10/20        0 -> 20.75   13/20
    meltblock              0 -> 0.05    1/20        0 -> 0.10     2/20
    anchor decisions       0 -> 358.40 20/20        0 -> 801.35  20/20
      target changed       0 -> 181.60 (50.7%)      0 -> 212.00 (26.5%)
    deep-kill barb=1       0 -> 0                   0 -> 4.65     1/20

**The anchor is the biggest single decision change in the round.** The
medoid differs from the list head on 89.7% of formations (lategame-80) /
55.9% (contested), and the target it picks differs from what the list
head would have picked on **50.7% / 26.5%** of them. Every military
measurement this project has ever taken was taken with target scoring
anchored at an arbitrary city.

**The op-aware pool is wider than it looks**: over 46 182 build-side
assault evaluations on lategame-80, 16.3% now exclude at least one
attacker committed elsewhere, mean 3.39 excluded when any are, max 14.

**The barbarian gate has almost no population on our cells**: zero
barbarian-held cities in 20 lategame-80 games, and 93 want evaluations
on 1 of 20 contested seeds (`deep-kill turn=127 ... target=442 barb=1
... p1=0.737 type="Legion" want=82.58` -- a target the build side could
not see before). The T160 Europe state (47 empty barbarian cities)
remains the motivating case and it is not in either cell.

**The melt's survival gate fires rarely and correctly** when it does:
`deep-meltblock turn=57 player="P6" city=240 unit="Musketeers" p=0.392
target=0.500` -- a disband that would have left the city below its own
survival bar. The claim check is the melt's bigger fix: 11-21 claimed
units per game were meltable garrison fodder.

### 6. Which past claims this voids

- **`findings/mechanism-map.md` D-A and military.md C1, "three landed
  mechanisms go from zero to live".** One does. The tired hold and the
  convoy hold have zero population in every cell this project runs, for
  reasons upstream of the flag.
- **D-A's "the convoy HOLD (boat sails regardless -- STATUS's 'verified
  inert' has this second, independent cause)".** There is no second
  cause. The hold never executes. STATUS 2026-07-31's "convoy hold+
  escort (verified inert)" stands, and its cause is the one STATUS
  2026-08-01 already published for the port-scan invade gate.
- **military.md stale-plan note 4** (`deepdefence.c:1410-1414`
  "describes a hold that does not hold") is right about the symptom and
  wrong about the cause: it is the seam, not the done flag.
- **Anything about deep's military behaviour measured before this
  round** carries a list-head target anchor that moved half the
  formations' targets on lategame-80. That does not invalidate a score,
  but no claim about WHICH targets deep picks survives.

### 7. What M3 and G1 must know

- **Do not count the tired-attack hold or the convoy hold as live
  mechanisms.** Force sizing that assumes deep declines tired attacks is
  assuming something that has never happened. Re-seating the tired hold
  means putting it where the unit becomes tired -- inside the attack
  path -- exactly the fix shape #16's invade gate needed. That is a
  combat-E2 design item, not a defect.
- **The two remaining presence-count gates are G1's**, and both are
  marked in-file: the severe-danger attacker cutoff
  (`deepdefence.c`, `w.tier == DWT_EMERGENCY && grave_danger > 0`) and
  the veteran-building guard (`grave_danger == 0`). Both sit on the
  garrison BUILD ladder, so swapping them for a survival threshold moves
  how much army a quiet city buys, which is the floor sizing itself.
- **The build side's assault want changed shape**: `deep-kill` now
  carries `barb=` and `elsewhere=`, and the marginal attacker is priced
  against the force that will actually fight for that target. Any floor
  or want A/B must take its baseline on a post-D1 binary.
- **Respecting op claims in the sortie passes costs 5.6 sorties per game
  on contested and kills 1.3 fewer enemy units** -- the first measured
  trade between the sortie pass and the operation layer. If M1 wants
  those blades back, that is a priority question, not a bug.
- **The hold registry is the place to put any future "this unit stands
  still" decision.** `deep_hold_unit(ait, punit, "why")` is phase-scoped,
  survives the manage loop, is honoured by the settler driver, and
  reports itself in `deep-hold`. Writing `unit_ai.done` directly is the
  bug this round fixed.
- **73% of military production still happens where stock's raw danger is
  zero** (E34.2b). Nothing here touches it.

`meson test -C build` **3/3 OK** and `python3 benchmark/tune-audit.py`
**CLEAN 121/121/121/121** before and after; this round adds no tunable
and changes no value.

## M3: the small-mechanism round -- four built, four closed on evidence (2026-08-05)

Seven code commits plus this one. `ai/deep` and one meson option; no
`ai/default`, `ai/baseline` or `advisors` behaviour touched.

    791c0a3a15  ai/deep: census the cross-water discount on density fear
    7f709fd90b  ai/deep: size the founder pool by land a founder can reach
    5720d60fd4  ai/deep: correct two comments that claim honesty the code lacks
    5841c3d285  ai/deep: price the health credit in citizens like every other credit
    6affe4e1cc  build: allow adv_choice producer tags without a debug build
    026c2360cf  ai/deep: price the marginal explorer, not the average one
    a9793209ac  ai/deep: correct horizon_health_weight's documented dimension
    af14ebcf12  ai/deep: salvage the explorers the prize can no longer employ

Binaries, all pinned to scratch with `libfreeciv.so` beside them,
`--server`, `--arms deep`, tuning=none:

    BEFORE   4a9f2afb06   7e3aee3001d3   the pre-session tip
    AFTER1   6affe4e1cc   d90dce3ecc95   items 1, 5, 6, 7, 8
    AFTER2   026c2360cf   c3cae66773d9   AFTER1 + item 4's build shape
    AFTER3   af14ebcf12   63532b6f75d2   AFTER2 + item 4's salvage leg

`meson test -C build` **3/3 OK** before and after. `tune-audit.py`
**CLEAN 121/121/121/121 -> 124/124/124/124**: three new PLACEHOLDERs
(`explore_explorer_claim`, `explore_salvage_gap`,
`explore_retain_floor`), each registered in all four audit places and
in the G/ECON/ALL blocks; the `deep-tuning` log line is generated from
`DEEP_TUNING_FIELDS` so it picked them up unaided.

### 1. The verdict table

    #   item                            verdict            where the number is
    1   density-fear reachability gate  ALREADY BUILT;     4a. Brief was stale:
                                        census built       gate is 92bd4a961a
    2   hut-risk awareness              CLOSED -- the      2. Premise inverted
                                        premise is wrong      vs server source
    3   drop-pick cosmetics             WONTFIX, measured  3. 1.2/game vs 633
    4   explorer census creep           BUILT (2 commits)  5. Hypothesis was
                                        + hypothesis dead     falsified first
    5   founder-pool reachability       BUILT              6. 12.5x tile cut
    6   producer tags for R3            BUILT              7. Byte-identical
    7   health-credit currency          BUILT              8. 1146 lines/game
    8   two dishonest comments          BUILT              -- no code change
    9   civil-war splinter purity       CLOSED -- defect   9. Splinters inherit
                                        is not real           the parent aitype
    10  phantom switch-penalty dock     SUPERSEDED         brief item 10; the
                                        (human, 08-05)        HEDGE session owns it

### 2. Item 2: the hut-risk discount has the sign backwards

The brief asked whether `hut_risk_radius`/`hut_risk_discount` covers
the hut->barbarian lottery. **It covers the opposite of it.**

`_deflua_hut_get_barbarians` (`data/default/default.lua:106-129`)
refuses to unleash anything when
`unit.tile:city_exists_within_max_city_map(true)` -- i.e. when ANY
player's city lies within `CITY_MAP_MAX_RADIUS` (5,
`common/fc_types.h:87`) of the hut. The AI-handicap path
`hut_get_limited()` (`server/unittools.c:3320`) carries the identical
guard. **A hut near a city cannot spawn barbarians at all**; the huts
that can are the ones far from every city.

That governs both cells this project runs: neither `data/classic/` nor
`data/civ2civ3/` ships a `default.lua`, their `script.lua` files
contain zero occurrences of "hut", and `ruleload.c:9758` ->
`valid_ruleset_filename()` (`:541`) falls back to
`data/default/default.lua`.

`deepexplore.c` discounts huts within `hut_risk_radius` of an OWN
city -- exactly the risk-free ones -- and pays full value for the ones
that unleash. Two lesser mismatches on top of the sign: the server
tests EVERY player's cities, deep tests only its own; and the server's
predicate is a squared-radius city-map iterate where deep uses
`real_map_distance`.

The barbarian-rate/onset gate deep applies IS right -- it is
`unleash_barbarians()`'s own gate (`server/barbarian.c:283`).

**Not fixed here.** The correction is small (use the server's own
predicate, invert the branch) but it changes an explore-value term and
needs its own A/B on a hut venue, and this session's huts cell was
already carrying three arms for item 4. The v3b tuner's habit of
zeroing hut value on classic is now explicable as pricing around a
term with the wrong sign, not merely an unpriced one.

### 3. Item 3: the drop-pick population is gone

The event is `deep-lift-op ... nextstop` (`deepops.c:1606`) -- the
operation re-picking its beachhead at the next passenger's goto tile
after a drop. Cycling is an op that does this repeatedly.

    dir                        games  nextstop/game  games>0  ops re-picking 2+
    isolated-150 BEFORE           20           1.50    13/20                  6
    isolated-150 AFTER1           20           1.20    12/20                  2
    isolated-150 AFTER3           20           1.25    12/20                  3
    cramped AFTER1                10           0.00     0/10                  0

Against the seed-205 watch item's **633**. There is no cost to
measure because there are 1.2 events per game and 2-6 operations in
20 games that ever re-pick twice. **WONTFIX**, and the watch item is
retired rather than carried.

### 4. Item 1: the gate was built a week ago; what was missing was the counter

The brief lists this as unbuilt. It is not: `92bd4a961a` landed it and
STATUS 2026-07-31 records "#18 CLOSED -- cross-water density fear
scales by the neighbour's actual lift". The spec at `0112a65112` and
the code agree; **the brief is the stale half**.

What was genuinely missing is what `findings/knob-coverage.md` 4a
names as the project's largest instrument hole. `deep-density` now
reports, per city per assessment, rivals in radius, how many are
across water, how many were discounted for owning no lift, and the
fear both with and without the gate.

    counter (per game)              isolated-150 (20)   cramped (10)
    deep-density lines                       2 543.8       12 802.9
    rivals in radius                         5 531.2       43 734.6
      of which across water                  3 825.1       21 381.0
      of which DISCOUNTED                    3 825.1       21 381.0
    raw fear (no gate)                       4 739.4       38 883.2
    gated fear                               2 400.8       25 313.6
    share of raw fear shaved                   51.2%          32.4%

Every cross-water rival in range was discounted in both cells: not one
of them owned a sea transport at the moment it was priced. And the
gate is not cosmetic at the integer where the danger is actually
injected -- on isolated-150 it **moves the injected integer on 48.6%
of evaluations (24 702 of 50 876)** and more than halves the cities
that see any density fear at all (1 724.8 -> 763.0 per game).

**The brief's cramped prediction is falsified.** It asked for a
counter of ~0 on cramped "where everyone is reachable". Cramped is
30x30 with three players and plenty of water: 21 381 cross-water
contributions per game, a third of the raw fear shaved. Cramped is
not a reachability no-op cell and should not be used as one again.

**No A/B isolates this gate**, and none can from the arms run here:
it is present in BEFORE and AFTER alike. The owed measurement is a
probe binary with the gate disabled, the E34.2b technique. Recorded
as owed rather than claimed.

### 5. Item 4: the hypothesis was wrong, the defect was real, and the fix is upstream of the fleet

The brief's hypothesis was that the fleet supply count includes
explorers on continents that cannot serve the local unknown.
**Falsified before any code was written**: the continent filter is
already in force (`deepexplore.c:79-130`), the supply set and the
demand set span the same continents by construction, and `unknown`
has read 0 in 100% of logged lines since the omniscience gate landed.

The actual pump is the want's SHAPE. Dividing the whole visible prize
by the fleet is a hyperbola: it approaches zero and never arrives, and
`explore_want > city_data->choice.want` accepts any positive number
from a city whose own best want is 0. So the fleet grows until
something else stops it, which nothing does.

Fixed by pricing the MARGINAL explorer -- each one already fielded or
on order is deemed to work `explore_explorer_claim` huts' worth of the
prize, and only the remainder is divided by the fleet. The want now
reaches exactly zero.

    counter (deep-explore)          AFTER1      AFTER3
    isolated-150, peak explorers        12           1
    isolated-150, mean explorers      0.98        0.94
    isolated-150, want-positive lines 2 630         479   (-82%)
    huts cell, peak explorers            2           2
    huts cell, want-positive lines    1 401         702   (-50%)

Score, isolated-150, AFTER1 vs AFTER3, 20 paired seeds:
**+0.01 +/- 0.34 (t +0.04)** -- and 18 of 20 seeds are bit-identical,
because the runaway fleet is a tail event, not the median game.

**The huts cell has no decision population for this at all.** All
three arms produced identical games on all 20 seeds and every metric
(score 67.50, cities 9.31, unitsbuilt 34.39, ... delta 0.00 on all).
The want changed on 699 lines per cell and never once flipped a build
argmax: peak fleet there is 2, and 80 turns is too short to reach the
equilibrium. The huts cell measures hut RACING, not fleet sizing;
isolated-150 is the venue for this mechanism.

**The salvage leg is built and has never fired.** Zero `deep-salvage`
lines in every cell. That is the loop closing from the front: with the
build shape fixed the fleet never grows past the retention floor, so
there is nothing to salvage. It stays because it is the disposal half
of the same quantity and it costs nothing, but **do not count it as a
live mechanism** -- same status as the tired-attack hold and the
convoy hold from D1, and for a healthier reason.

### 6. Item 5: the founder pool was holding options on other people's islands

`deep_remaining_sites()` fed the Join City gate from a whole-map count
of every unowned land tile on the planet. Now: land on a landmass we
occupy always; the rest of the world only while we own a sea transport
that could carry a founder to it. `deep-pool` gained `freetiles=` and
`reachtiles=`.

    counter (deep-pool, per game)   isolated-150        cramped
    lines with reachtiles<freetiles   75.8 / 75.8    35.7 / 35.7
    mean freetiles                         427.4           74.6
    mean reachtiles                         34.3           21.6

**Every single line.** On isolated-150 the census was counting 12.5x
more land than a founder could stand on.

    metric        BEFORE   AFTER   delta     se       t     cell
    sites          47.25    3.55  -43.70   1.74  -25.07   iso-150
    pool target     2.83    0.21   -2.62   0.10  -25.27   iso-150
    sites           8.00    2.80   -5.20   0.72   -7.22   cramped
    pool target     0.48    0.17   -0.30   0.05   -6.65   cramped

A/B, BEFORE vs AFTER1 (this arm also carries items 6, 7 and 8; 6 and 8
are provably behaviour-neutral, so the movement is items 5 and 7):

    cell            score BEFORE   AFTER    delta      se      t   wins
    isolated-150         194.66   196.12    +1.46    4.38  +0.33  12/20
    cramped              313.43   321.37    +7.93   17.37  +0.46   5/10

Flat and slightly positive on both. The mechanism metrics move the way
the change predicts -- isolated-150 settlers 3.51 -> 3.20,
mean_city_size +0.11 -- and two cramped metrics move more than noise
in a direction worth watching: **cities 11.27 -> 10.87 (t -1.07) and
munits 35.60 -> 30.73 (t -2.78)**.

**Recalibration debt, and it is the important line here.**
`founder_pool_per_site` was tuned against the UNFILTERED sites number.
The pool target is now 0.17-0.21 where it was 0.48-2.83, so the gate
dissolves nearly every surplus founder. The knob has not changed but
its meaning has, by roughly 13x. **It owes the tuning pile a value
before any expansion claim rests on this gate.**

### 7. Item 6: producer tags, byte-identically neutral

`-Dadvchoicetrack=true` defines `ADV_CHOICE_TRACK` as a global
compiler argument -- global rather than an `fc_config.h` entry because
the macro changes the LAYOUT of `struct adv_choice` and every
translation unit that sees the header must see the macro whatever its
include order. **No C source file is touched.**

Neutrality proved two ways. Byte identity across the change at the
default: `freeciv-server` `d0e1ad42481cf3b6...` and `libfreeciv.so`
`5b7284355f8f0fc1...`, `sha256sum -c` OK on both after a full meson
reconfigure. And the switch demonstrably does something when thrown:
`adv_choice_set_use` and `adv_choice_get_use` are defined symbols in a
`-Dadvchoicetrack=true` build and absent from the default one. The 25
existing `adv_choice_set_use()` call sites in `ai/default`, `ai/deep`
and `server/advisors` supply the names; nothing in the tree calls
`ADV_CHOICE_LOG()` or `adv_choice_mark()`, so the switch adds no log
output of its own. **R3 gets its producer column by building with the
flag.**

### 8. Item 7: the third currency is gone

`deephorizon.c` paid raw `EFT_HEALTH_PCT` percent points times a
weight. Now: expected citizens the building saves over the horizon,
valued at `deep_unworked_tile_value()` -- the same per-citizen price
the capacity and content credits pay. The conversion is the ruleset's
own arithmetic (`city_illness_calc()` reports base illness per turn in
tenths of a percent; `city_illness_strike()`, `citytools.c:2984`,
costs exactly one citizen), so no constant was invented.

`deep-credit` gained `health=`, the per-term instrument
knob-coverage 4e records as missing:

    cell            credit lines/game   with health>0   mean when >0
    isolated-150              4 490.9         1 146.4           9.25
    cramped                   4 971.5         1 842.0           9.58

Live in 20/20 and 10/10 games. Under the old form a civ2civ3 Aqueduct
contributed a flat 30 in every city, including cities whose plague
risk was zero. **`horizon_health_weight`'s scale moved with its
dimension** -- there was no exact factor to divide by the way the D2
upkeep fix had MORT -- so it is flagged for the tuning pile and its
header comment was corrected to name the new unit (`a9793209ac`).

### 9. Item 9: splinters inherit the parent's aitype; the defect is not real

`split_player()` (`server/plrhand.c:2777`) calls
`server_create_player(-1, ai_name(pplayer->ai), ...)` -- **the
parent's** aitype, not the server default. The 2026-07-31 bug-round
item 5 is false against this tree and needs no fix.

What does take `default_ai_type_name()` is the BARBARIAN player
(`server/barbarian.c:131`, resolving to `AI_MOD_DEFAULT`), and
barbarians are excluded from scoring twice over -- by `GOOD_PLAYER` in
`historian_generic()` and by `paircmp.py`'s `P\d+` seat filter.

Population, this session's dirs: **6 splinter seats in 10 cramped
games, 1 in 20 isolated-150, 0 in 20 huts** -- all already filtered
out of every comparison by that same rule, which `paircmp.py`'s
docstring documents. No scoring contamination exists to fix.

### 10. What G1 must know

- **`founder_pool_per_site` is stale by ~13x in meaning.** G1's floor
  work sits next to the expansion gate; any settler or founder claim
  taken against the current value is taken against a knob calibrated
  for a quantity that no longer exists.
- **The density-fear gate has never been A/B'd**, in either direction,
  and it is in every binary this project has benchmarked since
  `92bd4a961a`. Its counter is now live and says it shaves a third to
  a half of the raw fear. If a floor A/B moves, that term is in the
  baseline.
- **Cramped is not a reachability no-op cell.** 21 381 cross-water
  density contributions per game. Any "everyone is reachable here"
  reasoning about cramped is wrong.
- **`deep-density`, `deep-credit health=` and `deep-pool
  freetiles=/reachtiles=` are new.** Between them they convert part of
  knob-coverage 4a and 4e from NO-INSTRUMENT; the fear lattice's other
  ten knobs are still dark, because the post-injection `deep-def` line
  is still not written.
- **The explorer salvage leg has zero population.** Do not size
  anything on the assumption that deep disbands explorers.
- **Do not use the huts cell to measure explorer fleet sizing.** Three
  binaries, 20 seeds, byte-identical games. Fleet sizing lives on
  isolated-150.
- **The hut-risk discount is inverted** and is still shipping that
  way. Any hut-value tuning done before it is fixed is tuning around a
  sign error, which is what v3b already did by hand.

## G1: the garrison floor sized, then class-gated (2026-08-05)

**The floor is a TAX on both cells, and it buys no reduction in city
losses on either.** That is the sizing the session was told to take
first, it is the first time the mechanism has been measured on any
binary, and it reverses the assumption the floor has shipped on since
the naval-blindness fix. Ten commits: `f740582fcf` the census,
`f09e4375ed` the class facts, `92e289e0a2` the gate, `1637b6aa51` and
`988f284760` two class-fact defects the counters caught, `d8c436217a`
the escalation cap, `7c05d859e5` gate 6, plus `d652496196`,
`9dcee7ad95` and this section. That is over the 5-8 fence; three of
the commits are defect fixes the counters forced and one is the
human's mid-session addition.

### 1. The sizing (step 1), both cells

20 paired seeds each, binary PINNED (`536df20ee44c`), `--server`,
`--arms deep`, file-vs-file (`identity.ini` against `floor-off.ini`,
which sets `garrison_floor_danger = 0`). The off-switch is
`garrison_floor_danger`, not `garrison_floor`: the latter is also read
by `deep_fear_satisfaction()` and by the melt, so zeroing it would
move three mechanisms. **The off arm is proven by counter, not
assumed** -- `floor-applied` is 0.00 on every game of both off arms
while eligible city-turns stay in the thousands.

    isolated-150         floor OFF   floor ON     delta      se      t   ON>OFF
    score                  202.26     196.14     -6.12    3.18  -1.93     7/20
    cities                  16.15      15.06     -1.09    0.39  -2.77     5/20
    pop                     94.88      90.62     -4.25    1.40  -3.04     3/20
    landarea               197475     190000     -7475    3829  -1.95     7/20
    unitsbuilt              97.14     101.55     +4.41    2.30  +1.92    12/20
    munits                  25.29      26.57     +1.29    0.97  +1.33    14/20
    CITY LOSSES             24.85      25.65     +0.80    1.07  +0.75     9/20
    floor-applied/game       0.00     180.65

    lategame-80          floor OFF   floor ON     delta      se      t   ON>OFF
    score                  177.16     174.64     -2.52    1.71  -1.47     5/20
    cities                   8.82       8.17     -0.66    0.16  -4.16     4/20
    pop                     45.58      42.17     -3.42    0.66  -5.17     3/20
    landarea                99625      95142     -4483    1258  -3.56     5/20
    unitsbuilt              48.25      46.45     -1.80    0.75  -2.41     4/20
    munits                  17.79      16.77     -1.02    0.37  -2.76     6/20
    CITY LOSSES              8.05       9.45     +1.40    0.82  +1.70    10/20
    floor-applied/game       0.00     264.85

**The loss guard does not merely fail to justify the floor; it points
the other way on both cells.** The naval-blindness history that
motivated the floor is now covered by the fear lattice itself (F1
embarked cargo, F2 boat shadow, F6 survival deficit), and the floor's
residual contribution is a standing purchase of defenders in cities
where every one of those terms returned zero.

**Where the tax falls.** On isolated-150 the first 40 turns are
BYTE-IDENTICAL between the two arms, and the reason is not the floor's
design: `deep_garrison_menace()` returns 0.0 before first contact, so
the ungated floor already injects nothing there. The human's
acceptance sentence was already true on that cell, for a reason nobody
had written down. Divergence starts with contact:

    isolated-150, deep-want picks     floor OFF   floor ON     delta      t
    T1-40   settler picks               312.75     312.75      0.00     ---
    T41-80  settler picks               510.80     487.55    -23.25   -3.74
            DEFENCE share                0.011      0.038    +0.027   +5.12
    T81-150 settler picks               659.80     505.95   -153.85   -4.75

On lategame-80, where six players are in contact from the first turns,
the tax lands in the early window exactly as the direction predicted:

    lategame-80, T1-30                floor OFF   floor ON     delta      t
    settler picks                       179.05     122.15    -56.90   -6.29
    CIVILIAN share                       0.345      0.289    -0.056   -5.03
    IMPROVEMENT share                    0.122      0.098    -0.025   -3.75
    OFFENCE share                        0.428      0.513    +0.085   +7.99

Fifty-seven settler decisions per game displaced in thirty turns, on
19 of 20 seeds. That is the acceptance sentence's own evidence, taken
against the arm that has no floor at all.

### 2. The class-gated floor, and what it does and does not fix

Built as directed: the floor carries the weight of the strongest LIVE
blind-spot class for this city (MAX, not sum -- a second blind spot
does not buy a second token garrison), zero when no class is live.
Barbarian gate = the server's own. Sea = a met non-allied rival floats
a landing hull, weighted per city by whether the city's water leads
anywhere (`get_lake_surrounder`, so a closed lake is interior) and
escalated by attack-from-hull capability. Air = a met rival's known
tech can build a paradrop- or helicopter-class unit, reaching within
air range of a rival city, or of any open water once a carrier is
buildable. Border = proximity to the nearest met non-allied city ON
THIS CITY'S LANDMASS, shaped `R/(R+d)` with R = `density_fear_radius`.

    vs the UNGATED floor         iso-150             late-80
    score                  -0.59  t -0.18      -1.24  t -1.10
    CITY LOSSES            -0.10  t -0.06      -0.90  t -1.06
    floor firings/game    180.65 -> 150.95    264.85 -> 257.65
    injected danger/game    4057 -> 4247        10219 -> 7575
    GATED TO ZERO/game            123.2                 8.8

**The gate is correct and it is not the instrument that fixes the
tax.** On both cells the classes it gates on are genuinely live most
of the time -- 94.4% of eligible lategame city-turns have a land
neighbour on the same landmass, 50% have a rival hull afloat -- so
gating removes 16% of the firings on the island cell and 3% on the
crowded one, and score moves by noise. The tax lives in the floor's
MAGNITUDE, and every constant that sets it (`garrison_floor_danger`,
`garrison_menace_slope`, and now the six class weights) is an untuned
placeholder. E34.2b called these "the MIL block's most valuable
unexplored dimensions"; this session says the same thing with a sign
on it.

The loss guard is addressed and no attribution was needed: losses did
not rise on either cell against either baseline.

### 3. The class gates, proven by counter

Every gate is a server or ruleset fact and therefore free. All figures
are over 20 games per cell, from the `deep-floor` census line.

- **Barbarian, pre-onset zero.** civ2civ3 sets `onsetbarbs = 120`.
  Isolated-150 runs to T150: **0 of 2891 `barb=1` lines fall below
  T120**. Lategame-80 ends at T80, before the onset: **0 `barb=1`
  lines in 20 games**.
- **Sea, zero with no transport.** First `sea=1` at turn 62 / 66 / 81
  on the first three isolated seeds -- zero before a met rival floats
  a landing hull. Live share 11.8% (isolated) / 50.0% (lategame).
- **Air, zero pre-flight.** **0 `air=1` lines in all 40 games, both
  cells**, and 0 `carrier=1`. Neither cell reaches flight, so the
  airborne class and the carrier escalation ship UNEXERCISED: their
  code is proven inert, not proven correct.
- **Interior cities barely carry the sea class.** `wsea` takes exactly
  the four values the geometry defines and nothing else:

        1.000  exterior, marines-capable rival   2806 lines
        0.667  exterior, no marines yet            93
        0.250  interior, no marines yet            24
        0.167  interior, marines-capable          358

  An interior city carries one sixth of what a port carries once the
  escalation is live.
- **Gate 6 is inert at identity.** `floor_gate=0` on every
  `deep-stance` line of every game, and `fmult=1.000` on all 27 828
  floor census lines. The uniform-floor baseline is intact and the
  byte-identity property the W34 layer rests on is unchanged.

### 4. One owner per class -- and it is STRUCTURAL, not a convention

The floor is a RESIDUAL. Every anticipatory fear term is added before
it, and `deep_fear_satisfaction()` returns 1.0 while the garrison is
below the floor -- which is exactly when the floor is eligible -- so
the fear terms are at FULL strength whenever the floor could fire.
Therefore whenever a fear term prices its class, danger is nonzero and
the floor is silent in that city. No double charge is constructible.
The three ownership questions the brief asked to settle explicitly:

- **F5 vs the barb class.** F5 owns the barbarian era outright: it is
  flat and unconditional from the onset turn, so post-onset the floor
  is eligible only where incoming-defence credits cancelled the
  danger. The floor's barb class owns that remainder and nothing else.
- **F2 vs the sea class.** F2 owns the AT-WAR boat owner. The floor's
  sea class owns the met, non-allied, not-yet-at-war one -- the case
  F2 refuses by construction.
- **F3 vs the border class.** F3 owns proximity inside
  `density_fear_radius`; when it fires the floor is silent. The
  border class owns only the far field where F3's integer truncation
  has taken it to zero. It reuses F3's radius as its shape constant
  rather than introducing a distance scale of its own.

### 5. Two defects the counters caught, and one still shipping

**Counters before scores earned its place twice in one session.**

1. **Barbarians and animals polluted every capability class.**
   civ2civ3 runs an Animal Kingdom player, met from turn 1, that can
   "build" anything; it reported the marines escalation live from turn
   4 of every game. Excluded (`1637b6aa51`). The same commit fixed the
   attack-off-hull test, which asked only about `USP_NATIVE_TILE`
   while the two shipped rulesets key their conquest enabler on
   `USP_LIVABLE_TILE`.

2. **`utype_move_type()` cannot answer "does it float".** The sea
   class measured as NEVER LIVE across 20 island games on a cell where
   every seat demonstrably builds Triremes. A debug line printed on
   the transports themselves says why:

        DBG-cap rival=P1 type=Trireme mt=0        (UMT_LAND)

   The class is `native_to` lake and ocean in BOTH shipped rulesets,
   but `set_unit_move_type()` derives UMT_LAND for it. The fix asks
   terrain nativity instead (`988f284760`) and the class then goes
   live from turn 62.

3. **THE SAME BLINDNESS IS STILL SHIPPING IN THE FEAR LATTICE.**
   `deep_boat_shadow_fear` (F2), `deep_density_fear`'s float gate
   (F3), `deep_reachable_free_land` and the ferry census are all
   written as `transport_capacity > 0 && utype_move_type(ut) ==
   UMT_SEA`, so **none of them can see a Trireme** -- which is to say
   the entire early- and mid-game landing threat in the default
   ruleset of every cell this project benchmarks. The fear lattice was
   fenced this session, so they are UNCHANGED and owed. Each needs its
   own A/B; F2's is the most interesting, because the boat shadow's
   whole purpose is to fund a garrison before the boats load and it
   has never once seen an ancient boat. Some part of what this session
   measured as a floor tax may be the floor standing in for an F2
   that never fires.

### 6. Rider: the density reachability discount, finally isolated

`menace_overseas_weight` 0.25 -> 1.0 on isolated-150, 20 paired seeds,
same pin. One knob governs both reachability discounts
(`deep_density_fear`'s cross-water weight and `deep_garrison_menace`'s
overseas weight), so this arm isolates the DISCOUNT, not the density
term alone -- the honest scope of the result.

    score           197.70 vs 196.14   +1.56   se 0.76   t +2.05
    city losses      25.60 vs  25.65   -0.05   se 0.45   t -0.11
    floor firings   164.30 vs 180.65  -16.35   se 5.14   t -3.18

**It fired on 6 of 20 seeds and 14 games were byte-identical.** On the
fired-on subset every single seed improved: +0.5, +1.75, +2.25, +5.75,
+8.5, +12.5, mean **+5.21**. The gate that has been in every
benchmarked binary since `92bd4a961a` is, on this cell, a small net
cost. Twenty seeds are direction; this one is worth forty.

### 7. Owed to the tuning pile

    floor_barb_weight    1.0   PLACEHOLDER. Registered in S, MIL, ALL.
    floor_sea_weight     1.0   At 1.0 each, a live class reproduces
    floor_air_weight     1.0   the ungated floor exactly, so the
    floor_border_weight  1.0   change ships as a pure gate.
    floor_interior_weight 0.25 Same magnitude as menace_overseas_weight,
                               this project's existing reachability
                               discount, rather than invented separately.
    floor_marines_mult   1.5   Moves BOTH sea weights DOWN from full,
                               never up past it, so no live class can
                               buy more floor than the ungated version.
    stance_floor_gain    1.0   Gate 6. Identity at equal axes.

    Tuning INVARIANT to preserve: floor_border_weight must not fall
    below floor_sea_weight, or an overseas-only rival would outweigh a
    land neighbour for a border city.

    And the two that matter more than any of the above:
    garrison_floor_danger (40) and garrison_menace_slope (1.0) are what
    actually size the tax this session measured.

### 8. What the next session must know

- **The floor's SIZE is the open question, not its gating.** Sizing
  says -6.12 / -2.52 score with no loss benefit; gating recovers none
  of it because the classes are honestly live. The next move on this
  mechanism is a magnitude sweep of `garrison_floor_danger` and
  `garrison_menace_slope` (and now `floor_sea_weight`, which is the
  binding class on both cells), not another structural change.
- **Do not read the T1-40 identity on isolated-150 as the floor being
  harmless early.** It is `deep_garrison_menace()` returning 0.0
  before contact. On a cell with early contact (lategame-80) the floor
  costs 57 settler decisions in the first 30 turns.
- **The airborne class and the carrier escalation are unexercised.**
  Zero firings in 40 games across both cells. To measure them at all a
  cell must reach flight; neither of the standing tuning cells does.
- **The Trireme blindness in F2/F3 is the highest-value item this
  session uncovered and did not fix.** Section 5.3.
- **The reachability discount is worth re-running at forty seeds.**
  Section 6: six seeds moved, all six the same way.
- **`benchmark/paircmp.py` now takes `--pat NAME=REGEX`**, repeatable,
  so an epic pairs its own counters without editing the standing dict.
  `benchmark/floor.py` reports the floor census, the per-class live
  shares and GATED TO ZERO.

`meson test -C build` 3/3 OK and `python3 benchmark/tune-audit.py`
CLEAN **131/131/131/131** (was 124), before and after.

## D3: site truth -- deep had the inverted defence bonus, and its SIZE was the cost (2026-08-05)

Six code commits, one doc commit, plus this section. `ai/deep`,
`benchmark/` and doc only; no `ai/default`, `ai/baseline` or
`advisors` behaviour touched.

    ed98f06afb  ai/deep: read the enough-result threshold from the tuner
    7d645af4bd  ai/deep: value a site's defensible ground upwards
    4a0d05dd74  ai/deep: refuse the naval premium to water that leads nowhere
    0d6440e194  ai/deep: re-validate a lift passenger's mission mid-crossing
    263691b1ff  doc: correct the 00-context claim about the defence bonus
    051bc68483  ai/deep: make the maritime premium a tunable
    96e7f6d1dd  ai/deep: default the site defence weight to a measured zero

Binaries, pinned to scratch with `libfreeciv.so` beside them,
`--server`, `--arms deep`:

    BEFORE  b2161301d2  d1160fff868e   the G1 tip
    WIRED   ed98f06afb  1c3fba40772d   BEFORE + the enough-result wiring only
    AFTER   263691b1ff  240ae22fb680   all four defects, defence at stock's 20
    FINAL   96e7f6d1dd  750a0c988c80   AFTER + naval knob + defence default 0

`meson test -C build` 3/3 before and after. `tune-audit.py` CLEAN
**131 -> 134** (`site_defence_weight`, `site_naval_gate`,
`site_naval_weight`): each in all four audit places, in the X
(expansion) and ALL blocks -- X is where every other `site_*` knob
lives, there is no SITE block -- and in the generated `deep-tuning`
line, checked in a probe (2976 of 8192 chars, nothing truncated).
`benchmark/sitecensus.py` reports the two new censuses.

### 1. The defence bonus, and the position proof mechanism-map D-D asked for

`deep_city_desirability()` called stock's `result_defense_bonus()`
from the day the deep site scorer landed until this session --
upstream bug #1, where the defence factor sits in a DENOMINATOR, so
better ground scores less. **`00-context` section 5 offered exactly
this function as the bug ai/deep "simply doesn't have"** (corrected in
`263691b1ff`; the correction is in the context doc, where the claim
was).

The proof is a real position, not arithmetic. Turn-1 save of
isolated-150 seed 1, replayed three turns on the AFTER binary at
`-d 3`; the new `deep-sitedef` line is LOG_VERBOSE (free in benchmark
runs) and prints stock's term beside deep's, so one probe shows both
rankings of the same candidate. 331 candidates scored:

    turn 1, player P3 -- both candidates, identical base total 277.83
      Hills     (43,56)  defence factor 18   stock 313.69   deep 322.28
      Grassland (41,54)  defence factor 10   stock 349.56   deep 277.83
      => STOCK PREFERS THE GRASSLAND. Deep prefers the hills.

    6 exact-tie pairs in the probe, all inverted before, all correct now.
    92 near-tie pairs (|d total| <= 2, different defence factors):
      high-defence site ranked ABOVE:  stock 0/92,  deep 92/92.

Per-terrain means from the same probe make the second half of the bug
visible -- the stock term also falls with site VALUE, so it pays the
worst sites the most:

    terrain     factor    n   mean total   deep term   stock term
    Plains          10   89        326.6        0.00        64.33
    Grassland       10   80        322.7        0.00        64.75
    Hills           15   44        323.9       32.39        38.49
    Desert          10   21        258.4        0.00        83.46
    Hills           18    5        275.5       44.08        38.01

### 2. The sign was wrong AND the size was too hot -- and the size is what cost score

contested (8 players, 32x32, T150), 20 paired seeds, `--arms deep`:

    arm                                    score    delta     se      t   wins
    BEFORE (G1 tip)                        81.29       --     --     --     --
    AFTER, defence at stock's 20           76.94    -4.36   1.69  -2.57   4/20
    AFTER, same binary, defence OFF        79.37    -1.93   1.75  -1.10   7/20
    FINAL (default 0.0), vs BEFORE         79.37    -1.93   1.75  -1.10   7/20

The term ON vs OFF inside one binary is **+2.43 for OFF (se 1.87,
t +1.30, 13/20)**: more than half the session's regression is the
corrected term at stock's own DEFENSE_EMPHASIS. That agrees with the
human prior recorded the same day -- *"defense bonuses for cities
basically don't matter long-term"* -- so `site_defence_weight`
DEFAULTS TO 0.0, measured rather than assumed, with its 0-80 tuner
range registered for the pile. The sign fix ships unconditionally; the
magnitude is now a knob where stock had a literal, and the same commit
gives `site_naval_weight` the same treatment for NAVAL_EMPHASIS
(default 20.0 = stock, never measured by anyone).

**FINAL is byte-identical to AFTER+`site-defence-off.ini` on all 20
contested seeds** (scorelog sha256, 20/20 SAME). That proves two
things at once: the naval-weight refactor is inert at 20.0, and the
"skip the term's per-candidate virtual city when the weight is 0"
short-circuit changes no decision.

### 3. The closed-water premium: counted, and zeroed where the gate fires

`naval_bonus()` pays NAVAL_EMPHASIS percent of a site's whole value
for adjacency to ANY ocean-class tile, including a lake or a sea that
closes on its own continent -- at a city where the build side's
`boat_water_gate` then refuses to lay a keel. Gated on the same free
geometry (`deep_city_water_bridges` -> `get_lake_surrounder`).

    per game (deep-sitenaval)          contested (20)   isolated-150 (20)
    candidates PAID the premium              2 634.6            10 959.9
    candidates REFUSED it                       95.0               614.6
    games with >0 refusals                     20/20               20/20
    site score withheld                      7 312.1            54 511.1

**The closed-water site premium is exactly zero in the shipped
configuration** -- the refused count IS the population that used to be
paid it, and the withheld sum is what it used to be worth. Off-switch
proven by counter, not assumed: `site_naval_gate = 0` gives
`refused=0` while the gated arm refuses 22 in the same two probe turns.

### 4. D-E's frequency, finally a number

The op-hold early return sits above `city_can_be_built_here()`, so a
held passenger's mission was never re-examined; the lift delivered
onto an unfoundable tile and the metric called it `lift-arrived`. The
audit could only say "frequency unmeasured -- nothing logs it".

    per game (deep-lift-invalid)       contested (20)   isolated-150 (20)
    invalidations                                2.20                6.85
    games with >0                               13/20               16/20
    of which killed the BEACHHEAD                2.15                6.60
    operations aborted                           2.10                6.40
    re-aimed to another passenger                0.00                0.20
    lift-arrived                                 5.15               17.25

    when it fires: the hull is 23.5 tiles from its beachhead, 8.8 turns
    after the lift formed (n=137, isolated-150).

So on the island cell **roughly one crossing in four was sailing to a
tile that had stopped being foundable**, and it was being counted as a
delivery. `lift-arrived` falls 6.90 -> 5.15 on contested and
21.00 -> 15.00 on isolated-150 (10 paired seeds) -- that drop is the
metric getting honest, not deliveries being lost: the aborted
operations release their passengers to re-search instead of shipping
them to a dead tile. The re-aim path is real but rare (0.20/game):
almost every lift on these cells carries ONE passenger, so when its
target dies there is nobody else aboard to sail on for.

### 5. `site_enough_result`, byte-identical at default

The early break in `deep_settler_map_iterate()` compared the
compile-time `RESULT_IS_ENOUGH` while its own caller already read the
tunable for the same decision. Wired; default 250.0 = the constant.

    isolated-150, 10 seeds, BEFORE vs WIRED, scorelog sha256:
    seeds 1-10 all SAME  =>  10/10 byte-identical.

### 6. What the next session must know

- **The corrected defence term is written, correct, and OFF.** Turning
  it on at stock's magnitude costs 2.43 points on contested. A tuning
  pass over `site_defence_weight` (0-80) is the way to find out
  whether any positive value pays; it is the cheapest untried knob in
  site scoring and the term is waiting for it.
- **`site_naval_weight` is the bigger unmeasured magnitude.** A fifth
  of a site's whole value for touching water, inherited from stock and
  never tested by anyone, and it fires on thousands of candidates a
  game. It is now tunable. Nobody has moved it yet.
- **The naval GATE has no isolation arm.** Its counters prove it fires
  (95 / 615 refusals per game) and the off switch is proven inert, but
  the `site_naval_gate = 0` A/B was queued and dropped when the cores
  were needed for the defence attribution. `benchmark/tuning/
  site-naval-gate-off.ini` is committed and ready; one 20-game
  isolated-150 arm closes it.
- **The remaining -1.93 (t -1.10) is unattributed.** It is the naval
  gate plus the lift re-validation plus noise, on 20 seeds. If it
  matters, forty seeds and the gate-off arm will say which.
- **`deep-sitedef` is a LOG_VERBOSE probe line**, not a benchmark
  counter: it costs a `result_defense_bonus()` call per candidate and
  only exists under `-d 3`. That is how a per-candidate quantity gets
  instrumented without drowning a game log -- reusable pattern.
- **Nothing in this session touched the lift's SUPPLY side.** The
  invalidation abort is a correctness fix on the demand side; the
  wait-tail lead from W36 is still open and still unowned.

### 7. Addendum, same session: the naval GATE got its isolation, and lost it

The gate arm this section recorded as owed finished after the section
was written (a stale waiter relaunched it). It is a clean isolation --
one binary, `site_naval_gate` the only difference -- and it says the
gate does not pay:

    isolated-150, 20 paired seeds, AFTER binary   gate OFF   gate ON
    score                                           198.79    187.11
    delta                                                     -11.68  (se 8.02, t -1.46, 7/20)
    lift-arrived                                     21.55     17.25
    settlers                                          2.26      3.09

Direction, not a claim, at twenty seeds -- but it is the only evidence
there is, it points the same way as the contested residual, and
nothing anywhere measures the gate positive. **`site_naval_gate` now
defaults to 0** (`f657986d1a`), with the mechanism and its census kept
and tunable to 1.

**Why the gate was wrong, and it is the interesting part.** The design
took the premium for an option on SHIPPING and gated it on
navigability -- the same test the build side uses to refuse a keel.
But `naval_bonus()` is not a shipping term, it is COASTAL YIELD: a
Harbour feeds ocean tiles whether or not the sea leads anywhere, lakes
carry food specials, and a closed sea is still tiles a city works.
Gating a yield term on navigability prices out real food. The build
side's water gate is right about hulls and wrong about harvests, and
"the same geometry answers both questions" is the assumption that
failed. **Deep now prices the premium (`site_naval_weight`, stock's
20) and no longer refuses it.**

Shipped defaults measured end to end against the pre-session tip
(`f4a5786723c9`, defence 0, gate 0, naval weight 20, lift
re-validation live, enough-result wired):

    cell            BEFORE    SHIPPED    delta      se      t   wins
    contested        81.29      79.47    -1.82    1.47  -1.23   7/20
    isolated-150    195.55     198.99    +3.44    4.09  +0.84  12/20

Both within noise, opposite signs, and the -4.36 the corrected defence
term cost at stock's magnitude is gone. Counters on the shipped
binary, proving what is and is not running:

    counter (per game)              contested   isolated-150
    naval premium refused                0.00           0.00   (gate off)
    lift-invalid                         3.15           7.55
    games with any lift-invalid         15/20          19/20
    lift re-aims                         0.00           0.30
    lift-arrived                    6.90 -> 7.05   21.85 -> 22.45

**And that corrects section 4's reading.** With the gate off,
`lift-arrived` does NOT fall when the re-validation lands -- it rises
slightly on both cells. The 6.90 -> 5.15 drop reported there was the
naval gate suppressing coastal sites, not the re-validation losing
deliveries. The re-validation aborts 3-7 doomed crossings a game and
costs no arrivals at all.

Owed, unchanged: forty seeds on the gate before anyone calls -11.68 a
number, and a tuning pass on `site_defence_weight` and
`site_naval_weight`, the two magnitudes this session turned from
inherited constants into knobs.

## R2b: the wonder caravan gets a price, and it buys back a third of a fleet (2026-08-05)

Five code/benchmark commits and this one. `ai/deep` prices the OTHER
caravan -- the one that helps build a wonder, and the one R2 measured
as the LARGER claimant on the caravan queue -- by what it delivers and
when. Behind `wonder_merit`, compiled default 0, stock's bid untouched,
off arm byte-identical to the pre-session binary on 10/10 seeds.

    9ea76baff6  ai/default: expose dai_choose_help_wonder for deep to
                read its bid
    b21ac26028  ai/deep: share the caravan runner-up recovery between
                both producers
    17707e715a  ai/deep: price a wonder caravan by the shields it
                delivers
    052688c1d9  benchmark: census the wonder-caravan producer and its
                outcome
    9d2cb32ebd  ai/deep: recognise a wonder bid the domestic clamp
                truncated
    d4d1d411ca  benchmark: separate the wonder lines that were priced
                from the refused

**Headline: the mechanism executes 3620 times a game, and at the gain
that puts it on stock's own scale it builds 7% FEWER caravans on
classic while sending 39% MORE of them to a wonder (7.80 -> 10.85
dispatches, t +2.64) and completing slightly more wonders (2.75 ->
3.10 per game). Score does not move on either cell at either gain
(|t| <= 0.7). On civ2civ3 it buys the same wonders with 38% fewer
caravans.** A seventh mechanism that provably works and is worth about
zero in score. `wonder_merit` ships 0, with a measured gain of 5 and
the calibration table below going to the pile.

### 1. Wonder caravans ARE legal on civ2civ3 -- verified, not assumed

R2's binding fact was that civ2civ3 kills the trade ROUTE: `National`
and `NationalIC` pct 0 (`data/civ2civ3/game.ruleset:612-613`), so a
domestic route cannot be established and no foreign one ever became
legal in six games. That is the `[trade]` block and it has nothing to
do with the other errand. `enabler_help_build_wonder`
(`data/civ2civ3/actions.ruleset:647-662`) is present and identical in
shape to classic's: HelpWonder-flagged actor, no diplomatic relation
with the target (i.e. domestic only), target genus GreatWonder,
`help_wonder_max_range = 1`. Caravan and Freight carry the flag
(`units.ruleset:2638/2691`).

Measured on the cells, not read off the file. lategame-80 (civ2civ3),
20 games, off arm:

    dispatches per game        wonder errand   trade errand
    lategame-80 (civ2civ3)              6.55           0.95
    primary-120-classic                 7.80           8.25

**On civ2civ3 the caravan that gets dispatched is the wonder caravan**,
7 to 1, and the on arms find 6757 eligible seam lines in 20 games there.
The R2 caution stands for routes and is retired for wonders.

### 2. What stock prices, and what deep prices instead

`dai_choose_help_wonder()` (`daidomestic.c:71`, made non-static here by
rule 2's exception) prices a wonder caravan as

    want = wonder_city->building_want[wonder] / MAX(walk_turns, 1)

behind a boolean count gate -- build another while
`build_points_left > one caravan's shields * caravans on this
continent`. The wonder's want is not the problem; D2 made that honest.
Two other things are:

- **the whole wonder's want is credited to a caravan that pays for a
  sixth of it.** Deep re-denominates per shield delivered:
  `building_want * usable_shields / impr_build_shield_cost`, where
  usable_shields deducts the shields already pledged by help-wonder
  units fielded or in production -- stock's count gate as a continuous
  quantity. The share is a measured p50 of **0.167** (a 50-shield
  Caravan into a 300-shield wonder), so this term alone is a 6x
  deflator.
- **the delay is divided by, or not charged at all.** Stock charges
  nothing for the turns to BUILD the caravan (p50 **18** on classic --
  the same omission R2 measured as the largest single term on the trade
  side) and divides by the walk, which values a wonder 20 turns away at
  5% of one next door where `amortize()` gives it 43%. Deep amortizes
  over build + walk with the same `amortize()` every horizon credit
  uses. The walk is stock's own pathfinder move cost over the unit's
  move rate (p50 4 turns), not a crow-flight distance.

Recognition at the seam is R2's probe: `dai_choose_help_wonder` called
directly with `plr_data->tech_want[]` snapshotted and restored, because
the domestic advisor's MAX cannot tell a wonder caravan from a trade
one. Replace / promote / keep, all counted; `deep_caravan_replace()` is
now shared with the trade path and brings the improvement runner-up
back when the reprice is a reduction.

**One recognition defect found by its own counter** (`9d2cb32ebd`): the
advisor clamps its winner to `DAI_WANT_DOMESTIC_MAX` (750.075) before
returning it, so comparing against the producer's RAW bid silently
declines to act on the cities that bid hardest -- 6 of 84 eligible seam
lines in the probe game, all classified "keep". Fixed for the wonder
path; **the trade path has the identical gap and was deliberately left
alone** (it ships at 0, it is off in every arm here, and moving it
would take R2's published numbers out from under R2).

### 3. The off switch, proven twice

Smoke cell, 10 paired seeds, both arms loading `identity.ini`; the only
difference is the binary. Pre-session binary is `f657986d1a`
(`28d2e246ddac`); the session binary is `9d2cb32ebd` (`cced46192216`).

    seed  1 base=a6f3efd43d48 new=a6f3efd43d48 SAME
    seed  2 base=2a4d816b561d new=2a4d816b561d SAME
    seed  3 base=253dd4e45ad6 new=253dd4e45ad6 SAME
    seed  4 base=dfc060079036 new=dfc060079036 SAME
    seed  5 base=175aab030ede new=175aab030ede SAME
    seed  6 base=c96854550231 new=c96854550231 SAME
    seed  7 base=e3280a3541d2 new=e3280a3541d2 SAME
    seed  8 base=eae0f81b8e88 new=eae0f81b8e88 SAME
    seed  9 base=4f67f36c5f92 new=4f67f36c5f92 SAME
    seed 10 base=4fab860a4858 new=4fab860a4858 SAME
    => 10/10 byte-identical, and 0 deep-wonder-merit lines

Run twice: once on the pricing commit (`17707e715a`) and again on the
clamp fix (`cced46192216`), because the clamp fix landed after the
classic off arm had already been run on the earlier binary. Both are
10/10 against the same pre-session hashes, which is what makes the one
off arm valid for both -- non-execution and byte-identity are different
claims and both are made, twice.

The execution counter carries the same weight in the live cells: **0.00
`deep-wonder-merit` lines per game in every off arm, 20/20 games on
both cells**, against 3620 and 2210 in the on arms.

### 4. The A/B: primary-120-classic, 20 paired seeds

Binary PINNED (`cced46192216`, server + `libfreeciv.so`, `--server`),
`--arms deep`, all arms load a tuning FILE, control is
`benchmark/tuning/identity.ini`. Twenty seeds are direction.

    metric              OFF     g=1    delta     se      t   wins
    score            140.25  140.99    +0.74   1.08  +0.69  11/20
    wonders (seat)     0.45    0.44    -0.01   0.03  -0.29   3/20
    cities            21.65   21.30    -0.35   0.29  -1.21   6/20
    techs             28.81   28.63    -0.17   0.32  -0.55   6/20
    mean_city_size     3.33    3.41    +0.09   0.03  +2.63  13/20

    metric              OFF     g=5    delta     se      t   wins
    score            140.25  139.82    -0.42   1.14  -0.37  10/20
    wonders (seat)     0.45    0.50    +0.05   0.05  +1.06   8/20
    cities            21.65   21.15    -0.50   0.30  -1.67   8/20
    techs             28.81   28.45    -0.36   0.25  -1.42   7/20
    mean_city_size     3.33    3.42    +0.10   0.04  +2.23  13/20
    landarea         284042  276933    -7108   2737  -2.60   7/20

Mechanism, same seeds:

    per game                  OFF     g=1     g=5
    seam evaluations         0.00 3742.95 3619.55
    mode=replace             0.00  253.85  181.40
    mode=promote             0.00    0.00   50.15
    recover=yes              0.00  241.80  124.25
    recover=partial          0.00   12.00   15.35
    caravan build picks    335.20  225.95  312.40
    wonder DISPATCHES        7.80    3.95   10.85
    route dispatches         8.25    8.00    8.70
    wonders completed        2.75    2.70    3.10
    mean completion turn    85.93   86.20   86.71

    caravan picks  g=1 -109.25 (se 17.09, t -6.39,  0/20)
                   g=5  -22.80 (se 10.62, t -2.15,  5/20)
    wonder disp.   g=1   -3.85 (se  1.09, t -3.53,  3/20)
                   g=5   +3.05 (se  1.16, t +2.64, 13/20)

**The two gains do different things and both do what the price says
they should.** At gain 1 the reformed want is a fifth of stock's, so
the producer mostly withdraws: a third fewer caravans and half the
wonder errands, with wonders completed flat. At gain 5 -- the gain that
puts deep's number ON stock's median -- the fleet shrinks 7% while the
errands rise 39%, i.e. the price is not buying fewer caravans, it is
buying caravans that go to the wonder instead of idling or opening a
route. Wonders completed move +0.35 per game (2.75 -> 3.10) on a
population of ~2.7, which twenty seeds cannot resolve (t +1.06 on the
per-seat metric).

### 5. The A/B: lategame-80, the civ2civ3 cell, 20 paired seeds

    metric              OFF     g=1    delta     se      t   wins
    score            170.43  169.90    -0.52   1.16  -0.45   8/20
    wonders (seat)     0.15    0.09    -0.06   0.03  -1.93   2/20
    unitsbuilt        45.16   44.19    -0.97   0.24  -4.02   3/20
    techs             53.03   52.79    -0.24   0.11  -2.20   3/20

    metric              OFF     g=5    delta     se      t   wins
    score            170.43  171.17    +0.74   1.07  +0.69  10/20
    wonders (seat)     0.15    0.15    +0.00   0.04  +0.00   4/20
    cities             7.98    8.00    +0.02   0.10  +0.16   9/20
    techs             53.03   52.79    -0.24   0.14  -1.74   5/20

    per game                  OFF     g=1     g=5
    seam evaluations         0.00 2215.60 2210.00
    mode=replace             0.00  164.05  134.20
    mode=promote             0.00    0.00   22.45
    caravan build picks     50.85    1.15   31.35
    wonder DISPATCHES        6.55    0.30    6.30
    wonders completed        1.00    0.55    0.90
    mean completion turn    65.60   66.55   67.56

**This is the cell where gain 1 is visibly too cold.** It does not
merely trim the fleet, it abolishes it -- 50.85 caravan picks a game
become 1.15 (t -8.96, 0/20 seeds) and wonder dispatches 6.55 become
0.30 -- and the wonders follow it down, 1.00 completed per game to
0.55, the only statistically interesting wonder movement in the
session (per-seat t -1.93, 2/20). At gain 5 the same empire builds
**38% fewer caravans and completes the same wonders at the same time**
(0.90 vs 1.00 per game, completion turn 67.56 vs 65.60): a third of
stock's wonder caravans were surplus to the wonders that got built,
and the price says so before the shields are spent.

### 6. Calibration, for R3's corpus and for WRACE

The seam logs deep's price beside stock's for the SAME decision, per
city per turn, with the whole decomposition (`wwant`, `cost`, `left`,
`pledged`, `deliver`, `usable`, `walk`, `build`). 27 422 eligible
evaluations on classic and 6 757 on lategame-80, over 20 games each.

    population per game            classic g5    lategame g5
    seam lines                       3 619.55       2 210.00
    eligible (a wonder want)         1 371.10         337.85
    PRICED (the amortize ran)        1 058.55         320.20
    refused: already funded            312.55          17.65
    refused: no shield surplus         107.75           0.35

    over the priced population      p10     p50     p90
    classic  merit (gain 5)       22.400  87.900 294.400
    classic  stock's bid          33.800 102.100 333.100
    classic  merit/stock           0.239   0.956   2.424
    classic  merit (gain 1)        6.500  23.700  66.300
    classic  merit/stock (g1)      0.068   0.233   0.542
    classic  share of wonder       0.063   0.167   0.250
    classic  walk (turns)          2.000   4.000   7.000
    classic  build (turns)         7.000  18.000  45.000

    lategame merit (gain 5)       82.900 174.500 277.900
    lategame stock's bid          66.400 191.200 427.500
    lategame merit/stock           0.440   0.765   2.274
    lategame merit/stock (g1)      0.104   0.180   0.475

**The deflator this producer needs is ~5, and it is the same answer on
both cells** (1/0.233 = 4.3 on classic, 1/0.180 = 5.6 on lategame),
which is a stronger result than R2's single-cell ~8: the two rulesets
disagree about almost everything else and agree about this.

Where the fifth goes is decomposable and none of it is mysterious: the
share term is p50 0.167 (a 6x), the build+walk discount is
`(23/24)^22` = 0.39 at the medians, and the two multiply to 0.065 --
against a measured 0.233 because stock's own `/walk` division already
takes its bid down by a factor of ~4 at p50 walk 4. **Stock and deep
are charging for the same walk twice over in opposite directions**, and
that is the whole gap.

The anchors the brief asked for, on the same cells and the same games:

    winning want when the pick is...      classic OFF   lategame OFF
    a caravan (any producer)  p50               165.4          309.4
    a BUILDING (non-wonder)   p50               191.8          258.3
    a wonder improvement      p50               349.6          114.0

    the same, on the g=5 arm
    a caravan                 p50               154.0          229.0
    a BUILDING (non-wonder)   p50               192.0          288.5

R2 published p50 197.3 for the winning caravan want on this cell; this
session measures 165.4 on its own off arm, on a binary six sessions
later. **The reformed wonder want lands at p50 87.9 against stock's
102.1 at the same seam and against a building anchor of 191.8** -- so
the wonder caravan is priced at roughly HALF a typical winning
improvement, by both valuations, and the reform does not move the
building anchor at all (191.8 -> 192.0). That last number is the one
R3 should take: **repricing one producer did not shift the scale the
other producers live on**, which is what makes a per-producer deflator
table meaningful rather than circular.

### 7. What the next session must know

- **`wonder_merit` and `wonder_merit_gain` ship at 0 and 1.0**, both
  registered PLACEHOLDERs in ALL / G / ECON and in `knobcensus.py`.
  **The useful gain is ~5** and 1.0 is measurably too cold -- it
  abolishes the wonder caravan on civ2civ3 and costs wonders there.
  Anyone turning this on at the compiled corner will measure the
  withdrawal, not the reform.
- **WRACE's delivery arm is `deep_wonder_merit_want()`
  (`deeptrade.c`).** The race-urgency multiplier the brief wants goes
  on the `usable/cost` share term, not on the want: the want is the
  wonder city's own `building_want[]` and multiplying it here would
  double-count whatever WRACE does to that array. Everything WRACE
  needs is already logged per decision.
- **The wonder caravan is the caravan errand that survives on
  civ2civ3.** 6.55 wonder dispatches against 0.95 trade ones. Any
  future caravan work should test on lategame-80 as well as a
  `-classic` cell; the two cells give different answers about the
  fleet's size and the same answer about its price.
- **The trade path's clamp gap is real and unfixed** (section 2). It
  censors the top of the trade producer's own bid distribution the same
  way, and it should be fixed the next time `trade_merit` is touched --
  with the arm re-run, because R2's tables were taken without it.
- **`benchmark/wondercensus.py` reports three nested populations** --
  eligible, priced, finite -- and the distinction is load-bearing: a
  third of eligible classic lines are REFUSALS (the wonder is already
  funded by caravans in the field), and averaging their zeros into the
  price reads as deep pricing wonders at nothing.
- **Nothing logs a Help Wonder ARRIVAL.** The action executes inside
  `ai/default`'s `dai_manage_caravan()`, which deep must not modify, so
  the session reports dispatches and completed wonders and states the
  gap rather than inventing a delivery number.
- **A concurrent session committed into this branch mid-edit.**
  `f657986d1a` (D3's naval-gate default) carries R2b's two knob
  registrations, because it was made at 21:28 while this session was
  editing `deeptuning.*`. Nothing is wrong with either change and the
  audit is CLEAN at both commits -- but a bisect over the knob's
  introduction will land on a commit whose message is about something
  else.

`meson test -C build` **3/3** before and after. `tune-audit.py` CLEAN
**134 -> 136** (`wonder_merit`, `wonder_merit_gain`), each in all four
audit places, in the G / ECON / ALL blocks and in the generated
`deep-tuning` line.

## HEDGE: the AI's free production switch, sized and then priced (2026-08-06)

Four code/benchmark commits and this one. The session was briefed to
size a liquidity hedge -- bank surplus shields in a caravan when a
military need MAY arrive -- and, on the human's redirect, to stop deep
exempting itself from the change-production penalty. The sizing retired
the hedge and found something an order of magnitude larger underneath
it.

    cbbda6c933  ai/deep: census the production switch and the shields
                it would forfeit
    75ea5bbb07  ai/deep: let a deep seat pay the change-production
                penalty
    3385b40e4c  benchmark: report the production-switch penalty census
    1934ac4df8  ai/deep: price the production switch in the build
                auction

**Headline: deep was throwing away a quarter to two fifths of its own
shield output and nothing in the codebase knew.** The exempted mass is
4045 shields a game on contested (25.2% of everything the empire
produces) and 7967 on primary-120-classic (39.5%). Imposing the penalty
without pricing it costs 2.69 score and changes the churn by -3%;
pricing the switch in the auction cuts class churn 40%, the forfeit
66%, and brings score back to the exempt arm's level (-0.36, t -0.24)
while finishing three times as many wonders. The hedge itself is
retired on its own census: its instrument is a UNIT, so hedging pays
the very forfeit it hedges against.

### 0. The two human calls this section implements, and the asymmetry

2026-08-05: do not exploit the AI exemption -- deep self-imposes
`H_PRODCHGPEN` behind `pay_switch_penalty`, default off, "and if score
drops the verdict is that churn is expensive, not that the honesty
should come out."

2026-08-06, on the first numbers: "honest but expensive, with
switching-penalty in the auction to make the cost real." Compiled
default flips ON and the auction debits candidates by the real forfeit.

**Read this into every number from here on: `ai/default` and
`ai/baseline` stay exempt.** Deep is now the only seat in any benchmark
paying a cost its opponents do not, and every head-to-head from today
measures deep-with-a-handicap against stock-without-one. That is the
chosen posture, not an oversight. `pay_switch_penalty = 0` restores
comparability with everything published before today.

### 1. The mechanism nobody was paying (ruleset facts, stated)

`city_change_production_penalty()` (common/city.c:1943) halves
`before_change_shields` when production changes CLASS -- unit,
improvement and wonder are the three classes, and a change inside one
is free -- unless the city built last turn, in which case up to last
turn's shield surplus is exempt. `disbanded_shields` are never
penalized; `caravan_shields` are penalized unless the new target is a
wonder.

`change_build_target()` (server/citytools.c:3221) applies it
`if (!is_ai(pplayer) || has_handicap(pplayer, H_PRODCHGPEN))`, and
`handicap_of_skill_level()` (ai/difficulty.c:60-91) sets H_PRODCHGPEN
at **AWAY and NOVICE/RESTRICTED only** -- not at easy, and not at
`hard`, which every benchmark cell runs. The penalty is a rule for
humans and for the two weakest AI levels, and free for everyone else.

Conversion rates the hedge's legs needed, off the rulesets, identical
in both:

    Disband Unit Recover      50%  (Unit_Shield_Value_Pct -50, range 1)
    Help Wonder              100%  (no Unit_Shield_Value_Pct effect)
    Caravan build cost        50 shields (classic and civ2civ3)
    Caravan shield upkeep      0 classic / 1 per turn civ2civ3
    Settlers build cost       40 classic / 30 civ2civ3

### 2. Step 0, the sizing: how big the exempted mass is

Binary PINNED (`f2e87b9dfe87`, server + `libfreeciv.so`, `--server`),
`--arms deep`, 20 seeds per cell, `identity.ini`. The seam is deep's
own commit point (`deepdefence.c`, immediately before
`change_build_target()`) -- W36's rule about logging where the decision
commits.

    per game                    contested (civ2civ3)  primary-120-classic
    city-turns                              3 807.8              8 251.5
    production CHANGES                      1 209.8              1 965.0
    ... class-changing                        629.0              1 025.5
    ... penalty > 0                           452.0                747.8
    shields produced                       16 067.5             20 149.2
    PENALTY MASS forfeited                  4 045.3              7 967.5
    ... as share of output                    25.2%                39.5%
    ... in settlers (40 shields)              101.1                199.2

The class-pair matrix says where it sits: `I->U` and `U->I` together
hold 82% of the mass in both cells; the wonder pairs are rare and
expensive (`W->U` on classic, 19.9 events a game at a mean of 37.5
shields, p90 93). Why it is zero when it is zero, on the class-changing
population of contested: 406.3 events a game are a standing stock
charged 50% (3748 shields), 156.6 built last turn (297), 66.2 have an
empty box (0).

**The counterfactual was an honest upper bound and it is now checked
against the bill**: 4045 estimated, 3470 actually taken when the
penalty was imposed on the same cell and seeds -- 86% -- the gap being
exactly the compounding the census could not model.

### 3. The hedge's own population: retired on the arithmetic

    per game                        contested   classic
    switch INTO a unit                  309.4     545.5
    ... at a caravan-capable city        16.5     359.7
    ... mass there                      161.7   3 192.2
    ... share of the cell's mass          4.0%     40.1%
    ... and danger > 0                    9.2     180.9
    ... mass there                       87.5   1 469.5

The venue owns the population again -- 4% of the mass on civ2civ3
against 40% on classic, because a HelpWonder-capable unit is buildable
at 5% of the relevant city-turns there and 66% here. But the hedge dies
in both cells for a reason the design did not anticipate:

**a caravan is a UNIT.** Banking shields in one is itself a class
change, charged the same forfeit, on the turn the hedge fires. The
hedge pays the penalty now to avoid paying it later, and the "later" it
avoids is a mean 8.9 shields on the addressable population, bought with
a 50-shield instrument carrying 1 shield/turn upkeep on civ2civ3. Worse,
`caravan_shields` are themselves penalized on any switch that is not
into a wonder (common/city.c:2006), so the bank is not a safe deposit
either.

That is the brief's stop condition -- the penalty function differs from
the design's assumption in a way that guts the arbitrage -- and it IS
the finding. **WONTFIX: no option-value hedge on the caravan.** Section
5 is what the hedge was reaching for, and it needs no instrument at all.

Also worth recording against the design: `danger` does not discriminate.
On contested 38.5% of the mass is forfeited at cities with danger
exactly 0 and 57.6% at danger 1-99, and `menace > 0` covers 610 of 629
class-changing events. A hedge gated on elevated fear would have fired
almost everywhere or nowhere.

### 4. The other two legs, sized and closed

**Coinage.** `deep-fallback` -- the city-turns where every producer bid
zero, which the brief named as the sink comparison's only population --
fires **0.00 times a game on contested and 0.50 on classic**, against
3808 and 8252 city-turns. The fallback ladder whose caravan rung R2
killed is not a live population at all: deep's want machinery
essentially never bottoms out. Coinage is picked 8.4 (contested) / 2.05
(classic) times a game by the ordinary domestic advisor at wants under
1. **WONTFIX: there is nothing to compare three sinks on.**

**Salvage / the convertible class**, per game:

    deep-convertible city-turns   contested 2.75   classic 10.50
    ... disband-into-build             1.85            4.90
    ... shields recovered              18.5            38.2
    ... Help Wonder ARRIVALS            0.55            5.65
    ... shields delivered              27.5           292.5

Disband-into-build happens five times a game at best and recovers under
40 shields. A salvage option-value term on workers would be priced
against a 1-in-2000-city-turn event. **WONTFIX on the sizing.**

This does close an R2b gap for free: **the Help Wonder arrival is
instrumented now** (`carsh` on `deep-convertible`). Deep manages cities
in last-activities, after every unit has moved and before
`nullify_prechange_production()` wipes the field, which is the one
vantage that still sees it.

### 5. The three arms: exempt, honest, honest-and-priced

Binary PINNED (`52e293e7aba5`), `--server`, `--arms deep`, contested,
20 paired seeds, every arm loading a tuning FILE.

    A  exempt            pay_switch_penalty=0  switch_cost_price=0
    B  paid, unpriced    pay_switch_penalty=1  switch_cost_price=0
    C  paid and priced   compiled defaults

    metric              A       B       C     B-A (t)      C-A (t)      C-B (t)
    score           79.47   76.79   79.12   -2.69 -2.26  -0.36 -0.24  +2.33 +1.87
    unitsbuilt      62.31   50.73   54.52  -11.58 -8.84  -7.79 -6.45  +3.79 +2.86
    munits          16.27   14.24   14.49   -2.03 -3.21  -1.78 -2.76  +0.25 +0.37
    wonders          0.09    0.06    0.28   -0.03 -0.77  +0.19 +2.78  +0.22 +3.68
    cities           4.50    4.40    4.51   -0.10 -0.75  +0.01 +0.06  +0.11 +0.88
    pop             23.36   23.38   24.13   +0.03 +0.04  +0.78 +1.32  +0.75 +1.29
    techs           17.11   17.50   17.13   +0.39 +1.48  +0.02 +0.05  -0.37 -0.92

The mechanism, same seeds:

    per game                        A         B         C
    production CHANGES         1209.8    1169.5     970.3
    ... class-changing          629.0     607.9     377.6
    penalty mass                4045.3*   3469.7    1195.7
    ... share of output          25.2%*    21.8%      7.5%
    switch debits applied         0.00      0.00    1788.1
    ... debited to ZERO           0.00      0.00    1551.5  (86.8%)
    winner flips                  0.00      0.00     365.4
    switches REFUSED              0.00      0.00     236.5
                                          (* counterfactual, not paid)

**B is the finding the brief predicted and it is worth stating plainly:
paying without pricing changed nothing about the behaviour that caused
the bill.** Churn moved -3%, the seat forfeited 3470 shields a game --
22% of its output, 87 settlers -- and bought 11.58 fewer units and 2.69
less score with them. Nothing in the auction knew the penalty existed,
so nothing responded to it.

**C is what pricing it does.** Class-changing switches fall 40% (629.0
-> 377.6), the forfeit falls 66% against B and 70% against A's
counterfactual, and score comes back to the exempt arm within noise
(-0.36, t -0.24, 11/20). The one metric that moves clearly in C's
favour is **wonders, 0.09 -> 0.28 per seat (t +2.78 against A, +3.68
against B)**: a wonder is the longest illiquid build in the game, and
it is what an unpriced switch was mostly abandoning.

Units built stay 7.79 below A. That is the honest half of the trade:
deep really is poorer per turn than an exempt seat, and it buys the
same score with fewer, better-committed builds.

### 6. Both off-switches proven, twice each

Smoke cell, 10 paired seeds. Arm A on the PRICED binary
(`52e293e7aba5`) is byte-identical to the pre-session binary
(`bb6ee6d8d6c2`, HEAD `ebadf7dae9`) on all ten scorelogs, and the
hashes are the ones R2b published:

    a6f3efd43d48  2a4d816b561d  253dd4e45ad6  dfc060079036  175aab030ede
    c96854550231  e3280a3541d2  eae0f81b8e88  4f67f36c5f92  4fab860a4858

The census binary (`f2e87b9dfe87`, the three read-only seam lines
alone) was proved the same way before it took section 2's numbers --
10/10 identical, same hashes. Execution counters both ways:
`deep-payswitch` 0.00 lines and `deep-prodswitch pen=1` 0 events in
every off arm, against 8 seats a game (one line each, turn 1) and
970.3 paid events in C;
`deep-switchcost` 0.00 in A and B against 1788.1 in C.

**Note what byte-identity now describes: the tuning-file OFF arm, not
the compiled defaults.** From `1934ac4df8` the defaults are
honest-and-priced and are deliberately NOT identical to anything
published before today.

### 7. What the next session must know

- **`pay_switch_penalty` and `switch_cost_price` ship at 1**, both
  registered PLACEHOLDERs, with `switch_cost_gain` (1.0, untuned) as
  the magnitude. `benchmark/switchcensus.py` reads all of it.
- **`switch_cost_gain` is the hottest untuned knob in the tree.** The
  debit is the real forfeit at SHIELD_WEIGHTING (17) per shield, and it
  zeroes **86.8%** of the candidates it touches (debit p50 136 against
  a candidate want p50 41). It works, and it works as a near-veto on
  class changes; the tuning pile should find out whether a gentler
  conversion keeps the wonders and gives back some of the 7.79 units.
- **Deep is handicapped relative to stock from now on.** Any A/B whose
  arms differ in `pay_switch_penalty` is measuring the handicap, not
  the mechanism under test. Pin it in both arms.
- **Every number published before 2026-08-06 was taken on an exempt
  seat.** They remain valid on their own terms and remain reproducible
  with `pay_switch_penalty = 0`, but a re-measurement at the new
  defaults is a different game.
- **The commitment-discipline generalisation is queued, not built**
  (human call 2026-08-06: a written note, not tonight's code). Three
  pieces, in the order the evidence supports them:
  1. **Worklists.** Deep never writes one. A worklist is the ruleset's
     own commitment device -- production advances to the next entry
     without a class change being re-litigated -- and the census says
     405 of contested's 970 remaining changes are `U->U`, which a
     worklist would express directly.
  2. **Deeper hysteresis.** The debit is symmetric today: it prices
     leaving the current class but nothing prices ABANDONING a build
     that is nearly finished. `city_turns_to_build()` is already
     charged for elsewhere and the completion fraction is free to read.
  3. **The emergency exemption's cost.** Emergency-tier choices bypass
     the debit by design. Nobody has measured what share of the
     remaining 1196 shields they forfeit, and the counter to do it is
     already in the log (`deep-prodswitch` with `urgency > 0`).
- **The R2b trade-path clamp gap is still unfixed** and is unaffected
  by this session.

## R3: one scale, measured -- the gate is open and the room is empty (2026-08-06)

Three code commits, one findings commit and this one. The session was
briefed to put building, settling, trade-routing, worker-making and
military-making "at least vaguely in the same scale", from a fresh
corpus, behind per-class deflators. The corpus was taken, the
mechanism was built, the deflators were derived and measured, and the
answer is that **the rescale is safe and worth nothing.**

    4787233332  ai/deep: census every class's bid before the auction,
                not just the winner
    21a7dc4e16  ai/deep: give each producer class a scale, with the
                building as anchor
    3a86052ba3  ai/deep: cap the value inside the emergency tier, not
                the tier itself
    a303604434  doc: re-take the want calibration corpus, and find the
                tier is not a tier

**Headline: W01's founder gate IS open -- deflating the founder 5x no
longer craters score the way W02-v1 did (0.879, t -4.75) -- and behind
it there is nothing. At 40 paired seeds on isolated-150 the founder
deflator moves score by -0.05 (t -0.02, 20/40) while costing 13% of
the empire's cities.** Everything ships inert at compiled defaults; the
corpus (`findings/want-corpus-2026-08-06.md`) is the deliverable.

### 1. The corpus, and the two things it says that July's did not

20 games each on contested and isolated-150, census binary
`d2274419b6ee` (`-Dadvchoicetrack=true`), proved byte-identical to the
shipped build on 10/10 smoke scorelogs. Full tables in the findings
file; the two headlines:

**The disease is milder and has swapped classes.** The horizon credits
moved the building anchor 2-4x (July median 10-48 -> bid p50 91.6
contested / 191.3 isolated), so the founder's gap fell from 10-20x to
**6.5x / 3.7x**. The WORKER, at parity in July, is now **0.40 / 0.27**
of the anchor -- nothing rescaled it, the anchor grew out from under
it. And the founder SATURATES: 32.8% (contested) and 51.9%
(isolated-150) of winning founder wants sit exactly on
`DAI_WANT_DOMESTIC_MAX` (750.075). On isolated-150 its p50, p90 and max
are all the clamp.

**W01's tier did not abolish priority-by-magnitude; it moved it up one
level.** Emergency-tier winning VALUES, contested: first defender p50
100 (max 353), W01's calibrated defensive diplomat 300 flat, defence
building max 398 -- against attacker ferry p50 2705, attacker p50 6485
**max 259 976**, and a veterancy building at p50 4382 that no
workstream has ever mentioned. The honest emergency population's **p99
is 300.0 in both cells**; 15.5% / 16.4% of all emergency wins exceed
400. July read the "Trireme 63505" as a normal-tier spike. It is not.
It rides the tier that was built to stop exactly this.

The new instrument is `deep-bid` (`4787233332`): the founder, the
worker, the building anchor and deep's military want for the same
city-turn, **win or lose**. R2b asked for this; a winner-conditioned
sample cannot price a class against a class it rarely beats, and
founder-versus-building is that pair.

### 2. What the corpus does and does not support

    class        contested   isolated-150   shipped   basis
    settler          0.154          0.273      1.0    measured, then refused
    worker           2.48           3.75       1.0    measured, then refused
    building          1.0            1.0       1.0    ANCHOR, no key
    defender         0.29           2.99       1.0    NO DERIVATION
    attacker         1.10           1.96       1.0    NO DERIVATION
    ferry            0.65           3.22       1.0    NO DERIVATION

**The three military classes have no derivable scale and that is a
result, not an omission.** Their ratios to the anchor INVERT between
cells -- the defender wants 0.29 on contested and 2.99 on isolated-150.
A number that changes direction with the venue is not a calibration.
The caravan classes are deliberately absent: R2's ~8 and R2b's ~5 are
already implemented at the same seam as `trade_merit_gain` /
`wonder_merit_gain`, and a `scale_caravan` would double-count them.

### 3. The A/B, guard metrics before score

Binary PINNED (`1044a6e97ff4`), `--server`, `--arms deep`, every arm
loading a tuning file that **pins `pay_switch_penalty = 1` and
`switch_cost_price = 1`** so no arm differs in HEDGE's handicap.
Arms: S = `scale_settler 0.20`; W = `scale_worker 3.0`; CAP =
`emergency_value_cap 400`; ALL = all three.

    contested, 20 paired seeds      OFF        S          W        CAP        ALL
    cities                         4.51    -0.12      -0.09      +0.00      -0.71
                                          (-1.28)    (-0.92)              (-4.50)
    landarea                     50 188     -813       -631        -13     -3 988
                                          (-1.53)    (-0.83)              (-2.99)
    settler picks                 544.3    -85.3      -15.1       -0.9     -154.0
                                          (-5.27)    (-1.00)              (-6.21)
    worker picks                   75.3     +0.9     +120.3       -0.1     +133.5
                                          (+0.13)   (+12.11)              (+9.46)
    mean_city_size                 5.29    +0.13      +0.01      -0.01      +0.47
    unitsbuilt                    54.52    -1.09      -0.34      +0.19      -6.08
    score                         79.12    +1.67      -1.52      -0.21      -3.79
                                          (+0.80)    (-1.17)    (-1.00)    (-1.52)

    isolated-150                  OFF        S (40 seeds)      ALL (20)
    cities                      15.22     13.29  -1.94 (-4.34)  -4.46 (-8.04)
    landarea                   190 506   176 456 -14 050 (-3.20) -39 225 (-6.13)
    settler picks              1 480.9   1 152.0  -328.9 (-8.35)  -673.5 (-10.47)
    mean_city_size               6.80      7.23   +0.44 (+3.50)   +0.95 (+3.67)
    techs                       28.76     29.12   +0.36 (+0.78)   -3.04 (-5.75)
    unitsbuilt                  88.58     80.44   -8.13 (-3.02)  -11.18 (-3.00)
    score                      190.30    190.25   -0.05 (-0.02)  -24.30 (-6.28)

**S: the gate is open, and empty.** At 20 seeds isolated-150 read
+6.39 (t +1.62, 14/20) and it looked like the session's result. At 40
it is **-0.05, t -0.02, 20/40.** The project's own method note, for the
fourth time: twenty seeds are direction, forty are a claim. What the
deflator reliably buys is the trade it was designed to make -- 22%
fewer settler build-decisions, 13% fewer cities, +0.44 mean city size
(t +3.50) -- and that trade is worth exactly zero score. Contested
agrees within its own noise (+1.67, t +0.80).

**W: the median-match premise is falsified for the worker.** Inflating
the worker 3x does precisely what it was told -- worker picks 75 ->
196, t +12.11 -- and costs 1.52 score (t -1.17). Equalising medians
assumes each class is equally valuable at ITS median. For a want whose
distribution is bounded and small by construction (`worker_want` p90
69-72, max 148-150 in both cells) that assumption is simply wrong, and
the corpus could not have told the difference.

**ALL is worse than the sum of its parts, and that is the mechanism
story.** On contested S+W+CAP predicts -0.06 and ALL measures -3.79;
on isolated-150 S alone is -0.05 and ALL is **-24.30 (t -6.28), cities
-4.46 (t -8.04)** -- the W02-v1 tell, reproduced. The interaction is
visible in one counter: with the founder deflated AND the worker
inflated, settler picks fall to 856.8 against 1152.0 under the founder
deflator alone. The worker becomes the dominant civilian bid and
crowds out the settler a second time. **Two rescales that are each
survivable are not jointly survivable**, which is the argument against
ever folding a scale table one key at a time.

**CAP executes and does not matter, for a reason worth writing down.**
22.1 clamps a game, 20/20 seeds, and 19 of 20 games come back
identical: score -0.21, cities +0.00, landarea -12.5. Clamping the
spikes to 400 leaves them ABOVE the honest emergency population (p50
100-300), so the ORDERING the cap was built to fix survives the cap.
The real question is not the magnitude, it is whether a
`kill_something_with` attacker belongs above a grave-danger defender at
all -- and nothing measured here answers that. `emergency_value_cap`
stays registered at 0.

### 4. Fold / no-fold, per class -- the human owns this

**Recommendation: fold nothing.** All six keys ship at their compiled
defaults and are inert.

    scale_settler         NO FOLD   safe (gate open) but score-neutral
                                    at 40 seeds; costs 13% of cities
    scale_worker          NO FOLD   measured harmful, -1.52 alone and
                                    the poison in the bundle
    scale_defender        NO FOLD   no derivation; ratios invert
    scale_attacker        NO FOLD   no derivation; ratios invert
    scale_ferry           NO FOLD   no derivation; ratios invert
    emergency_value_cap   NO FOLD   executes, provably inert at 400

The keys stay because the mechanism is built, proven and free when
off, and because the cloud pile can now search a space the project
could not previously express. Every one is a registered PLACEHOLDER.

### 5. Off-switches proven, twice each

10 paired smoke seeds per binary, scorelogs bit-identical along the
whole chain `52e293e7aba5` (HEDGE's tip) -> `41b127880cc1` (bid
census) -> `f8b1bf9a9b26` (five scales) -> `1044a6e97ff4` (the cap),
same ten hashes each time:

    bc553467c513  6e3885799d7c  d39705b3f05b  88aad2316372  61e2bf59ba57
    190d8c2520ff  ca4b939713f9  ffadfe351823  c5ea7fcdd388  d28ccfcfd0d6

and the census build `d2274419b6ee` (`-Dadvchoicetrack=true`) matches
them too. Execution counters: **0.00 `deep-scale` lines per game at
compiled defaults** in every one of those forty games, against 1065.0
(S), 1291.9 (W), 2090.4 (ALL, contested) and 3704.1 (ALL,
isolated-150); `scale=CAP` 0.00 at defaults against 22.1-29.2.

`meson test -C build` **3/3**. `tune-audit.py` CLEAN **139 -> 145**
(`scale_settler`, `scale_worker`, `scale_defender`, `scale_attacker`,
`scale_ferry`, `emergency_value_cap`), each in all four audit places,
in the G / ECON / ALL blocks, in the generated `deep-tuning` line and
in `knobcensus.py`.

### 6. What the next session must know

- **`deep-bid` is now the project's conversion table** and it is cheap.
  One line per city-turn, four prices, read-only, byte-neutral. Any
  `want_t` work should start from `benchmark/wantcensus.py` output
  rather than from an argument. The gap it still has is the MILITARY
  split: deep forms one merged military choice, so defender, attacker
  and ferry cannot be separated at bid time. The per-producer line
  inside `deep_military_choose_build()` is the instrument combat E2
  wants anyway, and it is the one thing that would let the three
  military scales be derived.
- **The explorer is NOT unblocked.** W01's map said explorer re-pricing
  follows founder deflation. The corpus says the explore want wins
  0.1-0.3 times a game at 7.5-30 against an anchor of 61.5-104.6 --
  10-20x below, which no founder deflation reaches. It needs its own
  valuation, not a rescale of its rivals.
- **The veterancy building is an unowned spike class.** 9.5-10.4
  emergency-tier wins a game at p50 ~4000, max 129 488, filed as
  CT_BUILDING. It is in no workstream and nobody has priced it.
- **The founder want has no resolution left.** Half its winning values
  on isolated-150 are the clamp. Anything that wants the founder to
  DISCRIMINATE -- between sites, under rival pressure, against a
  building -- has to deal with the saturation first, and the scale key
  is the wrong tool because it moves the whole distribution rather
  than the ceiling.
- **The emergency tier's ORDERING is the open design question.** The
  cap proves the magnitudes can be flattened without effect; whether
  an offensive want should ever outrank a grave-danger defender is a
  doctrine call for W34/combat, not a calibration.
- **Median-matching is not a safe default.** It worked for R2 and R2b
  because both were pricing the SAME decision two ways. It failed here
  for the worker, where two classes' medians were matched on the
  assumption that they are equally valuable at their medians.
- **Every arm this session pinned `pay_switch_penalty = 1` and
  `switch_cost_price = 1`.** The archived inis are
  `benchmark/tuning/r3-{off,settler,worker,cap,all}.ini`.

## M1: combat E2 -- the attack learns what it costs (2026-08-06)

Ten code/benchmark commits plus this one. `ai/deep`, `tests/`,
`benchmark/` and doc only; no `ai/default`, `ai/baseline` or
`advisors` behaviour touched.

    6f7858abc8  ai/deep: report expected attacker losses from the
                survival walk
    7d5f928892  ai/deep: charge an assault want the losses it expects
                to take
    cce4f82b1e  ai/deep: price the marginal attacker against the force
                that can arrive
    6625a568cd  ai/deep: launch a mustered wave on its odds, not on a
                head count
    21dcc5cdf9  ai/deep: decide hold-vs-press where a unit is actually
                tired
    4e6edebce5  ai/deep: let the survival wave see enemy air
    def008f38c  benchmark: report the combat E2 census and register its
                four knobs
    b4bde75f6d  ai/deep: charge the assault loss only when there is one
    b3e6c2723b  ai/deep: keep deep_op_form's doc comment on deep_op_form
    d6e8c26ff2  benchmark: refuse a float tunable written without a
                decimal point

**Headline: deep loses half the units it used to lose, and the score
moves the right way on both cells without clearing twenty seeds.** On
contested the whole epic takes unitslost 23.99 -> 12.98 (t -9.47,
worse on ZERO of 20 seeds) and unitsbuilt 54.52 -> 42.72 (t -9.22) for
+1.75 score (t +1.27); the cost term ALONE is -4.41 unitslost
(t -4.40) for +1.27 score. The freed shields show up where the brief
said to look: mean_city_size +0.36 (t +3.07), wonders +0.13 (t +2.43),
techs +0.57. Full tables in
`findings/combat-e2-attrition-2026-08-06.md`.

### 1. What the substrate now reports, and what it costs to report it

`deep_stack_survival_losses()` fills `att_death[j] = P(attacker j
dies)` from the walk that was already happening: at every state the
duel has two outcomes, and the defender's survival probability IS the
attacker's death probability. Mass in the annihilated-stack absorber
is not live, so an attacker with nothing left to fight is charged
nothing -- which makes `n_att - sum(att_death)` an expected FORCE SIZE
and lets "can I hold what I take" be asked for the first time.

Performance, the brief's stop condition: 32 attackers x 16 defenders
at 40 HP, 200 calls, -O2 -- **2.406 ms/call before, 2.394 after, ratio
0.995.** No measurable cost.

`meson test -C build` is **4/4**: `tests/deepattrition_test.c` is a new
L0 target beside `deepodds`, all closed forms, including the
hand-checkable four-coin-flip example (P(attacker j dies) = 0.5^(j+1)
exactly, expected survivors 3.0625) and the accounting identity that
every duel kills exactly one unit.

### 2. The clamp, which is the epic's one real design decision

The first census of `loss=` said the MARGINAL loss is negative on
48.1% of evaluations (lategame-80, 63 375 lines, mean -5.40 shields).
That is real: the stream strikes strongest-first, so a strong blade at
the head often kills the defender in duel one and the weaker pool
members never fight. Paid unclamped it would be a SUBSIDY on half the
population, and it would credit the build with the same causal event
`Dp` already credits it with. Clamped at zero the term is a cost and
only a cost -- it can make deep more selective than the pre-E2 want
and never more aggressive. **"Massing a strong blade at the head saves
the weak ones behind it" is recorded, not spent**; it belongs to
whichever epic prices force COMPOSITION.

### 3. The three A/B arms, both cells, 20 paired seeds each

Binary PINNED (`8ce4a7617a7c`, server + `libfreeciv.so`, `--server`),
`--arms deep`, every arm loading a tuning FILE that pins
`pay_switch_penalty = 1` and `switch_cost_price = 1` so no arm differs
in HEDGE's handicap. Arms: OFF (every E2 mechanism off), LOSS
(`attack_loss_gain 1.0` alone), ALL (compiled defaults).

    contested                    OFF      LOSS       ALL   L-O (t)   A-O (t)
    score                      79.12     80.39     80.87  +1.27 1.18 +1.75 1.27
    unitsbuilt                 54.52     50.31     42.72  -4.21 -6.17 -11.80 -9.22
    UNITSLOST                  23.99     19.58     12.98  -4.41 -4.40 -11.01 -9.47
    unitskilled                17.93     14.64      9.82  -3.29 -3.58  -8.11 -8.21
    mean_city_size              5.29      5.41      5.65  +0.12 +1.23  +0.36 +3.07
    wonders                     0.28      0.31      0.41  +0.03 +0.74  +0.13 +2.43
    munits                     14.49     14.17     13.07  -0.33 -0.60  -1.43 -2.50

    per game                     OFF      LOSS       ALL
    declines (cost)          1668.00   1757.65   2431.90   55.1% -> 81.9%
    marginal loss NEGATIVE    782.30    717.60    144.20   25.8% ->  4.9%
    net want, summed       165334.48  89291.96  26700.21
    pool members, summed    12043.95  12739.25   3203.00
    ... OUT of reach             0.00      0.00  10702.05
    operations formed           6.30      5.15      2.90
    operations LAUNCHED         5.25      4.45      2.60

    lategame-80                  OFF      LOSS       ALL   L-O (t)   A-O (t)
    score                     169.54    170.68    168.05  +1.14 0.78 -1.49 -0.84
    cities                      7.78      7.85      8.08  +0.07 0.52 +0.30 +1.97
    unitsbuilt                 38.98     39.18     39.76  +0.20 0.35 +0.77 +1.21
    UNITSLOST                  13.07     13.01     13.39  -0.06 -0.13 +0.33 +0.59
    mean_city_size              5.47      5.43      5.32  -0.04 -0.71 -0.15 -2.31
    operations LAUNCHED         4.65      4.85      6.35  +0.20      +1.70 +1.99
    declines (cost)           682.60    668.45   1392.65   33.1% ->   65.0%
    pool members, summed     8982.00   9459.05   2144.10
    ... OUT of reach             0.00      0.00   7097.15

**The cost term points the same way on both cells** (+1.27 contested,
+1.14 lategame-80) and clears twenty seeds on neither. **The ops pair
does not**: it is part of contested's +1.75 and it costs 2.63 on
lategame-80. The halving of unit losses is a CONTESTED result --
lategame-80's unitslost is flat -- because contested is where deep
fights a lot and loses a lot.

The reach test is the biggest single behaviour change in the round:
**it removes three quarters of the pool** (lategame-80: 9459 -> 2144
member-slots a game, binding on 75% of evaluations) and by doing so
turns the marginal blade from a shield into a fighter, which is why
the decline rate climbs from 55% to 82% when the two are combined.

**The launch bar's sign is venue-dependent.** Launches fall 4.45 ->
2.60 on contested and RISE 4.85 -> 6.35 on lategame-80. Same code,
same knobs: contested's waves rarely clear an odds bar the head count
would have passed, lategame's often clear it before the last body
arrives. A verdict inherits its venue.

**The four-marginal-campaigns pattern did NOT appear.** Force size per
launch is essentially unchanged (lategame-80: 4.51 -> 4.27 members);
what moved is the NUMBER of launches, in opposite directions by cell.
G2's "four marginal campaigns where a human runs one decisive one" is
not addressed by this epic and remains open.

### 4. Two mechanisms with no population, and one with none by design

- **The re-seated tired-attack hold fires ZERO times in 120 games,
  both cells.** The seat is now correct -- D1 proved the old one could
  not fire, and this one sits inside the op's own battle where a unit
  IS tired -- but the window (a member adjacent, `0 < moves_left <
  SINGLE_MOVE`, tired odds below the floor AND fresh odds above it) is
  empty on both standing cells. Marching members arrive with either
  full moves or none. **Do not count it as a live mechanism**; same
  status as the convoy hold and the explorer salvage leg.
- **`deep-air` is zero in 120 games**, which is the answer G1 already
  predicted: neither standing cell reaches flight. The air term ships
  proven-inert-here rather than proven-correct, and the cell that
  would exercise it is the era-reachability item (ladder row 14).
- The negative-loss branch is inert BY DESIGN (section 2).

### 5. A harness defect that cost a 40-game pair, now guarded

The first lategame pair came back **bit-identical on all 20 seeds**
with `attack_loss_gain = 0` in one arm and `1.0` in the other.
`secfile` stores a token with no decimal point as an INT entry, and
`TUNE_DOUBLE`'s `secfile_lookup_float_default()` then falls back to
the COMPILED DEFAULT. The key name validates, the entry exists, the
result key differs -- nothing downstream notices. The `deep-tuning`
line is what caught it (both arms printed `attack_loss_gain=1
tired_attack_floor=0.9`).

`run.sh` now reads each key's declared type from `tuning-params.ini`
and refuses an int-formatted value for a float key, proven both ways.
**All 20 archived inis under `benchmark/tuning/` were scanned and are
CLEAN**, so no published result is affected.

The wasted pair is not wasted evidence: both its arms charged the cost
and neither ran the ops-side changes, so it isolates the ops pair on
lategame-80 exactly (score -2.63, t -1.54; launches +1.50, t +1.76).
Both lategame arms were then RE-RUN with the corrected file, and every
lategame number in section 3 is from the corrected pair.

### 5b. Provenance of the shipped tip

The benchmarked binary is `8ce4a7617a7c`. The tip carries one further
commit, `b3e6c2723b`, which moves the extracted assault-bar helper
above the doc comment it had landed inside -- pure code motion, no
statement changed, but the compiler reorders and the hash differs
(`7301ac1a3176`). Proven behaviour-neutral the project's usual way:
10 smoke seeds, `m1-all.ini`, both binaries, **all ten scorelogs
bit-identical**:

    59aeddeb566f  82fb0c66c073  e363f4dedec9  327b1c3a007d  83e497e71bb7
    5e4c2bbd2be9  f954506e5f87  a8b2d7a81a5a  e7daec9dddb7  6e372be6425c

### 6. Owed to the tuning pile

    attack_loss_gain      1.0   PLACEHOLDER, and the epic's hottest
                                knob. The conversion is
                                SHIELD_WEIGHTING -- the codebase's own
                                shields-to-want rate and stock's own
                                choice in kill_desire() -- so nothing
                                is invented; the gain is how much of it
                                to charge. At 1.0 it refuses 55-82% of
                                positively priced attacks. 0 = the
                                pre-E2 want exactly.
    op_hold_survivors     1.0   PLACEHOLDER, expected units. "One blade
                                expected to survive and stand in the
                                city." Never exercised against an
                                alternative value.
    assault_pool_reach      1   Geometry, not a magnitude, like
                                boat_water_gate. 0 restores the
                                whole-continent pool.
    op_launch_odds          1   The bar's CURRENCY, not a magnitude.
                                0 restores the head count.

`tune-audit.py` CLEAN **145/145/145/145 -> 149/149/149/149**, each new
key in all four audit places, in the G/MIL/ALL blocks, in the generated
`deep-tuning` line and in `knobcensus.py`.

### 7. What the next session must know

- **The cost of victory is landed and it is the first mechanism this
  project has that makes deep DECLINE.** Any military measurement from
  here is taken against a seat that refuses most of the attacks it
  prices. Take new baselines.
- **`attack_loss_gain` wants a magnitude sweep before anything else in
  MIL is tuned.** It moves the offensive want mass by 84% on contested
  at its placeholder value; every other MIL knob is being tuned around
  whatever it is set to.
- **The tired-attack hold now has a CORRECT seat and no population.**
  Two sessions have now paid for this mechanism. If a third wants it,
  the honest next step is to measure how often any deep unit is
  adjacent-and-tired at all, not to re-seat it again.
- **The negative marginal loss is an unowned finding.** 48% of
  evaluations price the marginal blade as REDUCING the wave's losses,
  because strongest-first ordering lets a strong unit absorb the duel
  the weak ones would have lost. Nothing in deep prices force
  composition, and this is the number that says it should.
- **`deep_take_odds_cost()` is the substrate's new public face**
  (expected losses in shields, expected survivors). W14's war ledger
  and E8's raid trade metric both wanted exactly this and can now be
  built on it.
- **The deep-bid MILITARY split that R3 asked for is NOT built.** The
  military ladder is a running MAX over producers, so a per-producer
  line at that seam reports "who raised the max", not "what each class
  bid". A true split needs each producer to report its own want before
  the max -- free for deep's own producers (the assault want already
  logs itself in `deep-kill`), impossible for stock's
  `kill_something_with()` without touching `ai/default`. Written up
  rather than half-landed.
- **The two recovered posture notes are DEFERRED, not dropped.**
  Sortie pricing as survival-DELTAS and escort demand scaled from
  observed sea threat are both pre-registered designs
  (`civ2civ3-adjustments.md`, "Combat workstream epic 1"), and the
  escort one is explicitly filed to naval epic #16 with its own rule
  ("empty seas justify unescorted caravans"). Neither fits inside an
  epic that already changed the pool definition, the launch bar and
  the want; landing them badly would contaminate the attribution this
  session's tables rest on.
- **An underdetermined choice the human owns** (lost-posture warning,
  flagged rather than guessed): whether the cost of victory should be
  the MARGINAL loss (what this blade adds, shipped) or the assault's
  TOTAL expected loss discounting the prize (`Dp x (worth - cost)`).
  The brief's wording supports either; the marginal form was chosen
  because it matches the benefit's own form, and the clamp exists
  because the marginal form has a sign problem the total form does
  not. If the human wants the total form, it is a small change to the
  same call site and the census already logs what it needs.

## PEACE: the cease-fire trap halves, and the trade never turns negative (2026-08-06)

Nine commits plus this one. `ai/deep`, `benchmark/` and doc only; **zero
`ai/default`, `ai/baseline` or `server/advisors` changes** (`git diff
--stat ac153b34a0..HEAD -- ai/default ai/baseline server/advisors` is
empty).

    3adcb9cd94  benchmark: census the diplomatic state every pair ever
                reached
    89a64dba74  ai/deep: propose peace on the turn the cease-fire window
                opens
    820cd03564  ai/deep: read the three game-state signals nobody has
                ever read
    c2fc2ec2cc  ai/deep: price an embassy by the research discount it
                actually is
    d4f20135fd  ai/deep: report the window the peace decision saw, not
                the one after it
    c95783641f  ai/deep: leave the allied embassy to ai/default and keep
                peace level
    e57e29f8cc  benchmark: archive the PEACE arms and read a treaty's
                decision, not a clause's
    c7bb0350e6  ai/deep: give the leader bias an execution counter of
                its own
    f45cd25364  doc: report PEACE -- the trap halves, and the trade
                never turns negative

**Headline: 20-42% of pacified pairs were stuck at cease-fire forever;
the pre-pass halves that on contested and cuts it a third on
lategame-80, and the peace-vs-conquest trade the brief asked me to
report honestly DID NOT MATERIALISE AS A SCORE COST on either cell.**
Full tables in `findings/peace-diplomacy-2026-08-06.md`.

### 1. The seam the brief asked about does not exist

`dai_diplomacy_suggest()` is static, but it is a three-line wrapper over
`could_meet_with_player()` plus two handlers that are already public in
`build/hand_gen.h`. Deep reaches the proposal machinery by calling the
same three functions. Two eleven-line stateless statics
(`get_allied_with_enemy_player`, `clear_old_treaty`) are copied under
rule 3's last-resort clause and the commit says so: deep must apply
exactly the guard the counterparty's valuation applies, or it proposes
into a `-BIG_NUMBER` refusal on every turn of the window.

Both handlers call `call_treaty_evaluate()` on both sides
synchronously, so a proposal is judged, accepted and concluded inside
the suggest call. That is what let the census be taken with no stored
state -- and it is what made the first measurement wrong (§4).

### 2. Off-switch proof

Pre-session tip `ac153b34a0` (pinned `ed0a5ec1b9e355`, `tuning=none`)
against the session binary `cc43f00c3f7a36` loading `peace-off.ini`:
**10/10 smoke seeds byte-identical scorelogs, identical
`metrics.csv`.** The counter-only commit `c7bb0350e6` is proved the
same way on the real cell: its OFF arm's `metrics.csv` is identical to
the earlier binary's OFF arm and all 20 scorelogs match.

### 3. The A/B, guard metrics before score

Binary PINNED, `--server`, `--arms deep`, 20 paired seeds, every arm
loading a tuning FILE that pins `pay_switch_penalty = 1` and
`switch_cost_price = 1`. Archived inis
`benchmark/tuning/peace-{off,seek,embassy,lead,all}.ini`.

    contested                       OFF     SEEK  EMBASSY      ALL
    PEAKED AT CEASE-FIRE          29.7%    15.9%    28.6%    17.2%
    ended peace-or-better         24.8%    34.4%    23.7%    33.5%
    embassy (ordered pairs)       31.6%    35.8%    35.5%    41.7%
    peace windows/game                0     53.5        0     53.2
    ... PROPOSED / SIGNED           0/0  30.4/19.8      0/0  28.6/18.8
    embassy treaties judged/CLEARED 0/0      0/0  48.7/26.9 45.0/25.5
    score                         80.87    84.47    80.94    85.51
                                          (+1.48)  (+0.04)  (+2.14)
    techs                         17.71    18.94    17.98    19.03
                                          (+1.96)  (+0.51)  (+2.61)
    unitskilled                    9.82     6.82     9.59     8.81
                                          (-3.11)  (-0.32)  (-1.07)

    lategame-80                     OFF     SEEK  EMBASSY      ALL
    PEAKED AT CEASE-FIRE          41.9%    28.6%    42.4%    28.5%
    ended peace-or-better         19.9%    26.7%    19.9%    25.6%
    embassy (ordered pairs)       26.3%    32.0%    28.6%    34.8%
    ... PROPOSED / SIGNED           0/0   19.7/8.3     0/0  20.9/8.7
    embassy treaties judged/CLEARED 0/0      0/0  21.3/10.9 23.9/12.4
    score                        168.05   169.97   169.49   170.28
                                          (+1.47)  (+2.18)  (+1.76)
    techs                         52.75    53.12    52.87    53.08
                                          (+2.36)  (+2.48)  (+3.27)

    contested, second pin           OFF     LEAD
    score                         80.87    82.31   (+1.04, 11/20)
    unitsbuilt                    42.72    45.23   (+2.21)
    unitskilled                    9.82    11.34   (+2.24)
    unitslost                     12.98    15.04   (+2.12)
    mean_city_size                 5.65     5.52   (-1.61)
    target-bias applications       0        2216 (max per seat)

### 4. A measurement that was wrong in its own output, again

`ds` points at LIVE diplomatic state, and the treaty concludes inside
`deep_treaty_suggest()`. The first probe therefore logged `left=16` --
the armistice's counter -- on every proposal that worked, when the
pre-pass only ever runs at `turns_left <= 4`. **The tell was that the
number was impossible.** Fixed by snapshotting before, and the line
gained `now=` read deliberately after, so one field separates "asked"
from "got it". The same class of error was found and fixed in the
census: `accept=` on a `deep-embassy` line is the RUNNING balance's
sign, so a treaty's decision is its group's LAST line, and clauses
priced / treaties judged / treaties cleared are three numbers that were
being conflated into one.

### 5. What the next session must know

- **Every knob ships at 0.0, and that is a refusal, not caution.**
  `peace_seek_gain = 1.0` was the batch's maximum-population setting,
  not a tuned value; shipping it would be picking a number to chase a
  benchmark (rule 7). All four knobs are registered in
  `tuning-params.ini` and in the ALL/ECON/MIL blocks and are ready for
  a pass. `tune-audit.py` CLEAN at 153/153/153/153; `meson test` 4/4.
- **Lever 1 is the strongest thing in this session and is the one that
  deserves 40 seeds.** Guard metrics are far outside noise on both
  cells; score is +3.60 (t +1.48) and +1.93 (t +1.47). Nothing here
  clears twenty seeds on score alone -- contested ALL at +4.64
  (t +2.14, 15/20) is the closest anything comes.
- **THE TIMING/CHARACTER DECOMPOSITION WAS NEVER RUN, and it is the
  first thing a follow-up should measure.** `peace_seek_gain` bundles
  two different changes: the pre-pass RUNS AT ALL (fixing a pure timing
  collision -- stock would have proposed if it had been asked on the
  right turn), and the love bar is WAIVED (deep proposes to players
  stock never would). An arm at a small gain separates them. That is
  the difference between fixing a bug and changing the AI's diplomatic
  character, and this session cannot tell you which one earned the
  +3.60.
- **Lever 2 works, is nearly free, and is worth about nothing alone**
  -- the seventh mechanism in this project to prove out and measure
  flat (contested +0.07). It compounds with lever 1 because lever 1
  supplies its population: peace-level pairs are 3-5% of the OFF world
  and 25-34% once the pre-pass runs.
- **Lever 3 is a read, not a policy.** It executes hard (2216 biases a
  seat) and does exactly what it says -- more built, more killed, more
  lost, growth down, score +1.44 at t +1.04. The statecraft that would
  make the read pay is BALANCE's.
- **`WIN_CAPITAL` is unreachable in every cell we run: 0 of 24 000
  player-turns.** It needs `player_leading_spacerace()` to name
  somebody, which needs a STARTED spaceship, which never happens inside
  150-200 turns. One of the finding's three "free signals" is not a
  signal here at all. The other two are live (a distinct tech leader on
  92% of player-turns).
- **Five of the pre-pass's ten guards never fire once**, including the
  op-commitment guard that replaced stock's no-mercy rule: in 1070
  windows deep never had a live operation aimed at a cease-fire
  partner. It costs nothing and buys nothing in this population.
- Upstream bugs #43-#46 all held up under measurement and none was
  fixed in place. Two now have numbers (see the findings §6).

## M1 addendum: the op-level go/no-go takes the total form (2026-08-06)

One code commit (`50eef808a7`) and this section, on the human call that
resolved the design question M1 flagged: keep the MARGINAL clamped form
for the build want, and adopt the TOTAL form -- `Dp x worth - E[total
losses]` -- for the operation's own launch decision, where "is the
whole fight net-positive" is the actual question. Same substrate
outputs, different call site.

**Verdict: the theory that the launch bar's FORM caused lategame-80's
ops-pair cost is supported in mechanism and unresolved in score.** The
gate removes exactly the launches the odds bar had added, and it gives
back +1.16 of the 1.49 the ops pair cost -- at t +1.47.

### 1. The pair

Binary PINNED (`20a685e8e655`), `--server`, `--arms deep`,
lategame-80, 20 paired seeds, both arms loading a file that pins
`pay_switch_penalty = 1` and `switch_cost_price = 1`. Arms differ in
`op_net_value` alone (`benchmark/tuning/m1b-{baronly,netvalue}.ini`).

    metric                  bar only   + net value   delta      se      t  wins
    score                     168.05      169.21    +1.16    0.79  +1.47  8/20
    mean_city_size              5.32        5.39    +0.06    0.03  +1.95  9/20
    techs                      52.75       52.94    +0.19    0.13  +1.47  5/20
    unitslost                  13.39       13.43    +0.03    0.37  +0.09  7/20
    unitsbuilt                 39.76       39.64    -0.12    0.39  -0.30  5/20
    cities                      8.08        8.01    -0.07    0.07  -1.00  3/20

    deep-netval refusals        0.00        7.65    +7.65    1.48  +5.16 13/20
    operations LAUNCHED         6.35        5.10    -1.25    0.48  -2.63  1/20
    operations formed          11.50       10.60    -0.90    0.71  -1.26  4/20

**The mechanism is exactly the predicted one.** M1's ops pair raised
launches on this cell from 4.65 to 6.35 (+1.70); the net-value test
takes 1.25 of that back, and it is lower or equal on 19 of 20 seeds.
The gate is not undoing the odds bar -- it is removing the subset of
what the bar released that does not pay.

**Reported on the fired-on subset, as the method note requires**: 7 of
20 games are identical (the gate never fired), 13 saw it fire at 11.77
refusals a game. On those 13 the mean is **+1.78, se 1.19, 8/13**, and
the spread is wide (-5.3 to +8.2). Twenty seeds are direction.

### 2. What a refused launch looks like

153 refusals across the arm:

    mean odds           0.922      (27% of them at p >= 0.99)
    mean target worth     633
    mean expected loss  111.8 shields = 1901 want at SHIELD_WEIGHTING
    mean value            583  against mean cost 1901

**These are near-certain TAKES that cost three times what the city is
worth.** The odds bar cannot see that by construction -- it asks
whether the wave wins, and the answer was yes. This is the first gate
in the project that can refuse a fight it expects to win.

### 3. Why the two seats take different forms, in one line each

- **Build want, MARGINAL and clamped.** It prices one more blade among
  many; the marginal loss is negative half the time (a strong unit at
  the head of a strongest-first stream saves the weak ones), and paying
  for that would double-count the odds it is already paid for.
- **Op launch, TOTAL and unclamped.** It prices the whole fight against
  not fighting, so not launching is the zero, `odds_now` IS the
  probability difference, and the whole expected loss is the whole
  cost. Nothing here can go negative in the subsidy sense.

Both convert shields at SHIELD_WEIGHTING through `attack_loss_gain`, so
the two seats cannot disagree about what a shield is worth.

### 4. Still owed

- **`op_net_value` ships at 1** (human call), registered, audit-clean,
  off-switch proven by counter: 0.00 `deep-netval` lines in the
  baronly arm against 7.65 in the netvalue arm.
- **The residual is not resolved.** +1.16 at t +1.47 against a
  -1.49 the ops pair cost: consistent with a full recovery and equally
  consistent with half of one. This pair is the fortieth-seed
  candidate of the round.
- **Contested has NOT been re-measured with the total form.** There the
  ops pair was part of a POSITIVE result (+1.75), and a gate that
  removes launches could subtract from it. Do not assume the sign
  carries.

## TRADE: the annuity was inverted, and the slot had no population (2026-08-06)

Nine code/benchmark commits plus this one. `ai/deep`, `benchmark/` and
doc only; **zero `ai/default`, `ai/baseline` or `server/advisors`
changes** (`git diff --stat e18c3dd60d..HEAD -- ai/default ai/baseline
server/advisors` is empty). Full tables in
`findings/trade-portfolio-2026-08-06.md`.

    ed1d8ed3c0  doc: file caravan.c's inverted annuity as an upstream
                bug
    50891abf43  ai/deep: assemble the trade route's NPV instead of
                caravan.c's
    77a698d47c  ai/deep: let live trade routes reach the war and the
                peace
    90c1b549d9  benchmark: census the route portfolio and the partners
                we cannot reach
    0d9618b5ba  ai/deep: report the risk factor of the route that set
                the want
    69ea3e3d28  benchmark: archive the TRADE arms
    2d067c6a02  ai/deep: correct the cease-fire life function's own doc
                comment
    c8c94270f6  benchmark: read route endpoints out of the save's city
                table
    1783550091  ai/deep: remove the love bar and let commerce anchor the
                peace

**Headline: the caravan build want R2 shipped was priced with an
inverted annuity, so for two sessions deep has valued the destinations
where a trade route could actually be established BELOW the ones where
it could not. `common/aicore/caravan.c:446` computes
`perpetuity * (1 - 1/rate^term)` where the closed form is
`(1 - rate^term)`; every other caller in the tree uses an infinite
horizon and takes the correct `perpetuity()` branch, and R2's
`horizon = MORT` is the only thing that reaches the broken one.**

### 1. Measured per decision, not argued from the source

Deep now assembles its own NPV and prints caravan.c's beside it for the
same pair. On primary-120-classic, 78 272 priced evaluations over 20
games:

    quantity            p10      p50      p90
    deep's npv        28.10    49.40    67.90
    caravan.c cnpv   -16.50     4.40    25.40
    merit / stock      0.01     0.21     0.47

    an ongoing route is available on 95.5% of priced destinations
    caravan.c's NPV is NEGATIVE on 37.1% of them

**The decomposition is proved rather than assumed:** on the rows where
no ongoing route is available -- the marketplace-only destinations,
where the annuity term vanishes -- deep's number equals caravan.c's
exactly, 350 of 350 on the probe corpus. The entire difference is the
annuity.

It also explains a number R2 could not decompose. R2 reported the merit
at a median **0.114** of stock's bid and asked for a ~8x deflator; on
the corrected price the same ratio is **0.21**. Half of R2's deflator
was this bug. **R2's "useful gain 5-10" does not carry over** -- ~4.8
is where the corrected price sits on stock's median, and that gain was
not run.

Not fixed in place (`ai/baseline` and `ai/default` both link
`common/aicore`); filed in `findings/upstream-bugs.md` with a second,
smaller defect in the same function's neighbourhood (an unguarded
`city_owner(game_city_by_number(...))` in the broken-route ledger).

### 2. Two of the brief's premises did not survive the code

- **`city_trade_removable()` is not uncalled.** `caravan.c`'s
  `one_city_trade_benefit()` calls it on BOTH endpoints for every
  candidate, and `account_for_broken_routes` defaults TRUE, so the
  "one-ledger rule" the brief asked for is the behaviour the game
  already has. What deep was doing wrong was throwing it away: R2 set
  `consider_trade = FALSE` once the slots were committed, which on
  classic deletes the candidate outright because
  `enter_marketplace_blocked_by = "Establish Trade Route"` makes the
  windfall it fell back on illegal exactly where a route is legal.
- **Trade caravans are already lift-eligible.**
  `deep_lift_wants_boat()` tests three things and has no unit-class
  filter, so a caravan with a cross-continent goto would be carried
  today. The block is one step upstream: the destination search is a
  LAND pathfinder, in deep's enumeration and caravan.c's alike, so an
  overseas city is never a candidate and that goto is never set. Stock
  reaches them only by setting `ignore_transit_time` -- pricing the
  crossing as free, the exact defect R2 removed. **`lift_carry_caravans`
  was not added: it would gate a thing that is not gated.**

### 3. What was built, and what it is worth where it landed

    mechanism                     executes            binds
    corrected annuity             3 946 seam/game     95.5% of prices
    slot rank (Nth-best dest)     3 946 seam/game     0.5% of decisions
    cease-fire truncation         691/game            3.6% of seam rows
    transit risk discount         knob at 0.0         not exercised
    trade_route_love              64 lines/6 games    classic only
    trade_partner_target_discount knob at 0.0         see section 6

**The slot opportunity cost -- the brief's first job -- is correct,
structural and almost without population.** The rank the marginal
caravan is sold: rank 0 on 77 858 decisions, rank 1 on 365, rank 2 on
37, rank 3 on 10, rank 4 on 2. 99.5% of cities have no trade caravan
already in the field, so the comparison never gets to bite. It replaces
a boolean that was wrong in a way that mattered; it does not, on this
cell, change many decisions.

### 4. The A/Bs

**primary-120-classic, 20 paired seeds**, binary PINNED
(`b9321020ec2b`), `--arms deep`, both arms loading a tuning FILE with
the standing `pay_switch_penalty` / `switch_cost_price` pins, both at
the PRE-FOLD diplomacy defaults so the pair measures the price alone:

    metric              OFF        ON     delta      se      t   wins
    score            137.60    139.57     +1.97    2.05  +0.96  11/20
    pop               68.08     69.81     +1.73    1.66  +1.04  12/20
    cities            21.48     21.95     +0.47    0.55  +0.85  10/20
    techs             28.20     28.19     -0.01    0.44  -0.02   8/20
    unitsbuilt        83.92     82.42     -1.50    1.06  -1.41   6/20

    per game            OFF        ON
    seam evaluations   0.00   3946.05
    mode=replace       0.00    230.20
    truncations        0.00    691.20
    trade dispatches   5.45      1.10
    ROUTE ENDPOINTS    8.90      3.00

**At the untuned corner the corrected price still WITHDRAWS** -- route
endpoints 8.90 -> 3.00, the same direction R2 measured at gain 1 on the
inverted price. Correcting the arithmetic doubled the number without
moving it onto stock's scale. Score +1.97 at t +0.96 is the right sign
and twenty seeds cannot resolve it.

**lategame-80 (civ2civ3), `trade_merit = 0`, `peace_seek_gain` pinned
1.0**: **0.30 route endpoints per game.** R2's finding that the
trade-route subsystem is effectively dead on this project's default
ruleset **survives PEACE**. There is no route population on civ2civ3
for the price to move, which is why no civ2civ3 price arm was run.

**isolated-150, 4 games**: 6 priced land destinations in four games,
zero routes, the whole fleet on the wonder errand -- and offshore legal
partners on **8.1% of city-turns, one or two at a time, at `offpct=200`
every time**. The INIC premium design call 3 was aiming at is real and
is exactly the 2x it expected; the population it would serve is one or
two partners on 8% of the city-turns of one cell, on top of a land
population that is empty. **Sized rather than built.**

### 5. The switch tree, and each level proved

    trade_merit = 0 (compiled default)   the whole price path skipped
      trade_merit = 1
        corrected annuity                always on inside (a bug fix)
        slot rank                        always on inside (a comparison)
        trade_ceasefire_peace_odds 0.0   pure truncation pricing
        trade_route_risk_gain      0.0   survival factor exactly 1.0
    trade_partner_target_discount 0.0    own off-switch, skipped
    trade_route_love              5.0    own off-switch -- NOW ON, §6

**Level 1, everything off, proved before the fold:** smoke cell, 10
paired seeds, both arms loading `trade-off.ini`, pre-session
`e18c3dd60d` (pinned `923fc3512696`) against the session binary --
**10/10 byte-identical**, and the same ten hashes M1 published on
2026-08-06. Execution counters on those seeds: 0 `deep-trade-merit`
lines, 0 `deep-trade-love` lines, `spared=0 loved=0` throughout.

### 6. THE FOLD -- and the thing the human should see

**Human call 2026-08-06: "remove the love bar, trade want anchors
love".** Two compiled defaults flip together:
`peace_seek_gain 0.0 -> 1.0` and `trade_route_love 0.0 -> 5.0`. The
argument is the pairing: waiving stock's `req_love_for_peace` alone is
indiscriminate forgiveness; waiving it while commerce pays love means
the affection is EARNED per turn by a route that exists, and decays at
stock's own 4% when it stops. Every structural guard PEACE refused to
waive is still refused. This overrides rule 7 on values grounds and is
recorded as such; both stay registered PLACEHOLDERs and 0.0 restores
the old behaviour on either side.

The flip is proved to be exactly those two values: the post-flip binary
(`2d0d5143f974`) with only the standing pins equals the pre-flip binary
with both values written out, **10/10 smoke seeds identical** -- and 8
of those 10 differ from the pre-session hashes, so it does change play.

**Confirm pair, lategame-80, 20 paired seeds:**

    metric              OFF        ON     delta      se      t   wins
    score            168.05    169.97     +1.93    1.31  +1.47  13/20
    techs             52.75     53.12     +0.37    0.16  +2.36  13/20
    pop               41.67     42.38     +0.71    0.69  +1.02  13/20
    munits            15.09     14.28     -0.82    0.44  -1.84   6/20

**And the decomposition says the whole +1.93 is the bar removal.** A
third arm at `peace_seek_gain = 1.0, trade_route_love = 0.0` is **byte
-identical to the fold-on arm on 20 of 20 scorelogs**, and there are
**0 `deep-trade-love` lines in all three arms across 60 games** --
because the cell holds 0.30 route endpoints a game, so
`deep_routes_with_players()` returns zero and the function returns
before it can add anything.

**The anchor is inert on the ruleset this project measures its economy
on.** It is live on classic: 64 firings over 6 games, 3 of 6 seeds,
+5 love per partner per turn, structural cap observed at 249
(`req_love_for_alliance - 1`), 7.67 route endpoints a game. So the
mechanism works and the pairing is real -- on a ruleset that is not the
default one. **On civ2civ3 the bar removal ships alone.** That is not a
reason to withdraw it (it measures positive), but the human's safety
argument does not apply on the cells the campaign runs, and that should
be a deliberate decision rather than an inherited assumption.

### 7. What the next session must know

- **Take no caravan number from R2 or R2b at face value again.** R2's
  entire calibration table was taken on the inverted price. R2b's
  wonder path is unaffected (it never touches `annuity()`), and its
  numbers stand.
- **The corrected price has never been run at a gain where it acts.**
  ~4.8 puts it on stock's median. One arm, twenty seeds, classic.
- **The fold's two halves have never been measured together where both
  act.** `trade-love-probe.ini` at twenty seeds against
  `trade-fold-off.ini` on primary-120-classic is the arm.
- **`trade_partner_target_discount` ships at 0.0 with an execution
  probe only**: 44.67 applications per game on primary-120-classic at
  0.5, concentrated on the seeds where deep both trades and fights
  (140 / 126 / 2 / 0 / 0 / 0 over six games). It executes; it has no
  scored arm, and the twenty-seed question -- does declining profitable
  conquests against partners cost or pay -- is open.
- **PEACE's timing/character decomposition is still not run and is now
  more urgent**, because `peace_seek_gain = 1.0` is shipping.
- **civ2civ3 kills the trade route and PEACE does not revive it.**
  0.30 endpoints a game with the pre-pass pinned on. Any future trade
  work is a `-classic` project until somebody builds a cell where
  foreign routes actually form.
- **`lift_carry_caravans` was not created**, and the reason is in the
  code rather than in a measurement: the lift already accepts caravans.
  Cross-water trade needs an overseas candidate enumeration with a
  modelled ETA, and its population is one or two partners on 8.1% of
  island-cell city-turns.

`meson test -C build` **4/4**; `python3 benchmark/tune-audit.py` CLEAN
**158/158/158/158** (four new keys: `trade_ceasefire_peace_odds`,
`trade_route_risk_gain`, `trade_route_love`,
`trade_partner_target_discount`).

## M1 rider 2: conquest is worth what it denies, too (2026-08-06)

One code commit (`cede5b9dbd`) and this section, on the human call
that a conquest has been under-counted: taking a city both gains us
its amortized value and deprives the opponent of it, and every
conquest want in this project has priced only the first half.

`conquest_worth_mult`, default **2.0** in a registered range 1.0-3.0
(the human's prior band is ~1.9-2.2), multiplies the city worth at
BOTH conquest seats -- `deep_assault_want`'s build side and the
operation's net-value launch gate -- once at each, never compounded,
same referee in both. `deep-kill` and `deep-netval` now carry `worth=`
(stock's read, meaning unchanged so older numbers stay comparable)
beside `eworth=` (after the multiplier), so the knob is visible on
every evaluation win or lose.

**Verdict: the refusal prediction is confirmed, the launch prediction
is falsified, and score is flat with a fat tail.**

### 1. The pair

Binary PINNED (`d3b70a215b7c`), `--server`, `--arms deep`,
lategame-80, 20 paired seeds, arms differing in `conquest_worth_mult`
alone (1.0 vs 2.0) with everything else at compiled defaults
(`op_net_value` on) and HEDGE's two keys pinned in both.
`benchmark/tuning/m1c-mult{10,20}.ini`.

    metric                    mult 1.0    mult 2.0    delta      se      t  wins
    score                       170.28      169.47    -0.81    1.67  -0.48 11/20
    unitslost                    12.85       12.33    -0.52    0.51  -1.01 10/20
    unitskilled                  11.46       11.03    -0.43    0.49  -0.89  9/20
    munits                       14.97       14.49    -0.48    0.40  -1.20  7/20
    unitsbuilt                   40.08       39.67    -0.42    0.54  -0.77  8/20
    settlers                      0.43        0.64    +0.21    0.14  +1.51 12/20
    pop                          42.77       42.07    -0.71    0.55  -1.29  5/20
    cities                        8.18        8.15    -0.03    0.13  -0.19  7/20

    deep-netval refusals          8.00        3.45    -4.55    1.90  -2.40  4/20
    operations formed             7.75        8.40    +0.65    1.20  +0.54  9/20
    operations LAUNCHED           4.05        4.15    +0.10    0.62  +0.16  9/20
    operations at the walls       1.40        1.15    -0.25    0.49  -0.51  7/20

**Zero identical games.** All twenty seeds moved, which is what a
multiplier on the quantity every conquest want reads should do. The
score spread is wide -- -15.2 to +10.7 -- and the mean is -0.81 at
t -0.48, so twenty seeds say flat, not negative.

### 2. The two predictions, judged

**CONFIRMED: the net-value gate refuses less.** 8.00 -> 3.45 refusals
a game (t -2.40), a 57% fall, because doubling the prize is exactly
what clears a value-versus-cost test.

**FALSIFIED: it does not buy more launches.** +0.10 a game (t +0.16),
flat, even though the gate stopped refusing 4.55 times a game. The
reason is in what the counter counts: `deep-netval` is a per-TURN
refusal event, not a distinct operation denied. A refused muster stays
mustered and re-asks next turn, so removing refusals makes a wave
launch EARLIER, not more often. Every previous reading of this counter
-- including the M1 addendum's -1.25 launches for +7.65 refusals --
should be read that way.

### 3. What is still refused, which is the more interesting half

    per refusal            mult 1.0   mult 2.0
    refusals (total)            160         69
    mean odds                 0.933      0.888
    mean RAW worth              632        598
    ... under 300                0%         0%
    mean expected loss    112.6 sh   149.0 sh
    mean value vs cost   589 / 1914  1067 / 2534

**The prediction that the survivors would be "genuinely cheap prizes
only" is not what happened.** The raw worth of a refused target barely
moves (632 -> 598) and none of them is a cheap city. What the
multiplier filters out is the band of fights that cost between one and
two cities; what survives is the fights that cost more than TWO --
still 2.4x under water on average, and BIGGER (expected loss 112.6 ->
149.0 shields). The gate did not become lenient, it became
better-targeted.

### 4. Two things parked for the human, explicitly

1. **The defensive symmetry is not implemented.** Keeping our own city
   is worth about 2x by the identical argument -- we keep the value
   and deny the taker -- and multiplying only the conquest side tilts
   the offence/defence budget by construction. `deep_op_defend_bid`'s
   `worth x odds` and the whole fear lattice are untouched. This is a
   design call, not an oversight, and it is the first thing to settle
   before `conquest_worth_mult` is tuned.
2. **The base is still omniscient.** The multiplier scales stock's
   `def_ai_city_data(acity)->worth`, which reads an enemy city's true
   state (census finding #4, the self-valuation channel). The rider
   does not touch that channel and does not worsen it; W04's estimator
   replaces the base, and the multiplier survives that swap unchanged
   because it multiplies whatever the base reports.

### 5. Owed

- `conquest_worth_mult` ships at **2.0**, registered in all four audit
  places, in the M/MIL/ALL blocks, in the generated `deep-tuning` line
  and in `knobcensus.py`; off-switch is 1.0 and is proven by counter
  (`worth == eworth` on every line of the 1.0 arm).
- **A 2.0-vs-1.0 verdict at forty seeds is worth having**: every seed
  moved and the spread is +-10, which is the profile of a knob that
  matters and a sample that cannot see it.
- **Contested is NOT re-measured here** (BALANCE owns that cell's
  re-measure). The multiplier is live in both cells' defaults from
  this commit, so any contested number taken before it is stale.

## M1 rider 3: the offence/defence stakes become a tilt (2026-08-06)

One code commit (`558841bf07`) and this section. The conquest rider
left an asymmetry its own argument condemns -- keeping our city keeps
its value AND denies it to the taker -- so `defence_worth_mult` is
that half, default **2.0**, range 1.0-3.0, sibling of
`conquest_worth_mult`.

**Verdict: doubling the defence stakes does NOT regress. Score -0.13
(t -0.29) on the pair the rider required, so the SYMMETRIC default
ships as landed (2.0/2.0) and the symmetric case does not go to the
pile.** The mechanism moves exactly as designed and the tilt is now a
knob rather than a decision taken in code.

### 1. The tilt is the ratio, and 1:1 is invariant

Both sides of the one comparison that reads both -- the defence
counter-claim in `deep_op_poach_defender` -- now carry their own
multiplier: our risk by `defence_worth_mult`, the op's expected take
by `conquest_worth_mult`, once each. That is what makes a common
factor cancel, so 1:1 is algebraically the pre-rider behaviour at any
level and only the RATIO means anything:

    conquest / defence  > 1   conquest-hungry
                        = 1   pre-rider behaviour, any level
                        < 1   hearth-guarding

Multiplying only the defence side would have made "symmetric stakes"
mean "double the defence" -- a tilt wearing a symmetry's clothes.
Documented in that language in `tuning-params.ini`, in the header, and
in `knobcensus.py` where the entry is marked **MAP**: a sweep must
move the pair together or explicitly hold one.

### 2. The other worth readers, exempt with reasons

- **Emergency triage comparator** (`deepdefence.c`): sorts our OWN
  cities, so a common positive factor cannot change any comparison --
  and post-D1 it is lexicographic (surv, urgency, capital, worth, id)
  with worth only the fourth key. **Exempt by construction**, as the
  brief predicted.
- **Target scoring** (`worth / (1 + distance)`, both in
  `deep_op_pick_target` and `deep_assault_want`): picks an argmax that
  a positive scalar cannot move -- the same reason gate 5 is applied
  after the snap-up sweep. **Exempt.**
- **The overseas lift-demand magnitude** (`deepdefence.c`, `worth =
  MAX(worth, target_worth)` feeding a ferry build want): reads a
  target's worth but SIZES A BUILD rather than comparing stakes, so
  scaling it would move naval wants against the domestic ladder for
  reasons this rider is not about. **Untouched and flagged**, not
  silently skipped -- if the human wants stakes to propagate into
  build magnitudes, that is a separate call with its own pair.

### 3. The pair

Binary PINNED (`6051a0e8bbbe`), lategame-80, 20 paired seeds,
`defence_worth_mult` 1.0 vs 2.0 against a fixed
`conquest_worth_mult = 2.0` -- so the arms are tilt ratio **2.0
(conquest-hungry) vs 1.0 (symmetric)**. Standing pins in both.

    metric                    tilt 2.0    tilt 1.0    delta      se      t
    score                       169.60      169.47    -0.13    0.47  -0.29
    unitsbuilt                   39.39       39.67    +0.27    0.16  +1.70
    unitslost                    12.04       12.33    +0.29    0.32  +0.92
    unitskilled                  10.82       11.03    +0.20    0.29  +0.70
    munits                       14.70       14.49    -0.21    0.18  -1.15
    cities                        8.11        8.15    +0.04    0.08  +0.53
    mean_city_size                5.40        5.38    -0.03    0.02  -1.08

    poach events                 12.90       15.25    +2.35    2.83  +0.83
    netval refusals               5.05        3.45    -1.60    1.12  -1.43
    operations launched           3.90        4.15    +0.25    0.41  +0.61

Eight of twenty games are identical; on the twelve that moved the mean
is -0.22 (se 0.79, 4/12). **No regression beyond noise, so the
default stays 2.0.**

### 4. What the defence doubling actually buys

    per arm                      tilt 2.0    tilt 1.0
    poaches (total)                   258         305
    mean own-city worth               582         601
    ... as dworth (after mult)        582        1201
    mean risk the city bid            550        1149
    mean ODDS of the op outbid       0.03        0.16

**That last row is the mechanism in one number.** Under a
conquest-hungry tilt a sinking city can only outbid operations that
were nearly hopeless anyway (mean take odds 0.03); at symmetric stakes
it outbids operations with real prospects (0.16), 18% more often. The
knob does what it says, the counter proves it, and the score cannot
tell the difference at twenty seeds -- which is the honest reason to
ship the symmetric default rather than a tilt nobody measured.

### 5. Owed

- `defence_worth_mult` ships at **2.0**, registered in all four audit
  places, M/MIL/ALL blocks, generated `deep-tuning` line, and
  `knobcensus.py` (MAP). Off-switch is 1.0, proven by counter
  (`worth == dworth` on every poach line of the 1.0 arm).
- **The RATIO is the tuning object, not either level.** Both knobs at
  2.0 is one point on a line; the pile should sweep the ratio.
- The defence-side stakes still reach only the counter-claim. The fear
  lattice, the garrison floor and the relief machinery price danger,
  not stakes, and nothing in this rider changed them.

## ENDGAME: the AI learns the game ends, and it is worth half a point (2026-08-06)

Four code/benchmark commits plus this one. `ai/deep`, `benchmark/` and
doc only; zero `ai/default`, `ai/baseline` or `server/advisors`
changes. Full tables in `findings/endgame-horizon-2026-08-06.md`.

    4ba239a8b2  ai/deep: clamp the growth ladder's horizon to the turns
                that remain
    e9926b1094  ai/deep: play the last turns for the scoreboard, not
                for the empire
    c12565d6ea  ai/deep: deflate the unit the bell will not let a city
                finish
    b39daf69bd  benchmark: report the endgame horizon and the score
                mode apart

**Headline: nothing in `ai/`, `ai/default` or `server/advisors` had
ever read `game.server.end_turn` -- plain readable state, zero readers
-- so every AI in this tree plans over a fixed 24-turn horizon on the
last turn of a game that 199 of 199 benchmark runs end on the turn
limit. Deep now reads the bell. On two cells at 20 paired seeds the
whole family measures positive and none of it resolves: +0.68 / +0.67
/ +0.45 on lategame-80 and +0.25 / +1.55 on isolated-150, every |t|
below 1.7. The one column with a t above 2 is the one the score metric
pays five points for -- wonders +0.14 (t +2.60) on isolated-150.**

### 1. Three mechanisms, not two, and the third one is a finding

The brief asked for the horizon clamp to be measured apart from the
score mode. Building it forced a third distinction that the counters
now carry:

- **the horizon clamp** -- `MORT` becomes `MIN(MORT, end_turn - turn)`
  in the four horizon credits, in the plague-insurance term and in the
  building upkeep bill. Both sides, deliberately: clamping only the
  benefit would bias against every building the ruleset charges rent
  for.
- **the arrival test** -- `MAX(1, MORT/ttg)` floors at one growth step,
  which is right with 200 turns to run and wrong with five, so a credit
  whose first instalment lands after the last turn is refused outright.
- **the score mode** -- inside the final `endgame_window` turns, a want
  whose build cannot be COMPLETED is deflated and a great wonder that
  still can be is paid a premium.

**The clamp is provably inert before the window and the counter says so
in absolute numbers: 0 clamps before T-24 against 162 865 (lategame)
and 252 143 (isolated) after, across 40 games.** That is the brief's
stop condition, met exactly.

**The arrival test is NOT inert, and cannot be.** 31% / 27% of its
refusals happen more than MORT turns from the end, the earliest on
turn 8 of an 80-turn game -- because a city whose time-to-grow exceeds
the whole remaining game has no growth to credit whatever the turn
number is. It is a mid-game behaviour change, it is the arithmetically
correct one, and neither cell regressed under it. Recorded rather than
buried.

**The brief's own formula would have been wrong-signed.**
`amortize(value, MIN(delay, MORT, turns_left))` clamps the DELAY, and
`amortize()` discounts for a wait -- so clamping the delay down makes a
late benefit worth MORE. The quantity that needed clamping is the
HORIZON: the `steps = MORT / ttg` family and the upkeep bill, not
`amortize()`'s second argument. Flagged per the campaign rule about
questioning a stale plan rather than bending code to it.

### 2. The two cells

Binary PINNED (`cc1fab22cea8`), `--server`, `--arms deep`, 20 paired
seeds, every arm loading a tuning file with `pay_switch_penalty`,
`switch_cost_price`, `peace_seek_gain` and `trade_route_love` all
written out. `benchmark/tuning/eg-{off,clamp,deflate,mode}.ini`.

    lategame-80        OFF    CLAMP-OFF    DEFLATE-OFF     MODE-OFF
    score           169.47  +0.68 +1.37   +0.67 +1.61   +0.45 +0.86
    pop              42.07  +0.24 +2.40   -0.17 -0.88   -0.11 -0.46
    mean_city_size    5.38  +0.04 +2.69   +0.03 +0.90   +0.04 +1.13
    unitsbuilt       39.67  -0.11 -0.73   +0.52 +2.69   +0.31 +1.92
    wonders           0.22  -0.02 -0.81   -0.04 -2.03   -0.02 -1.00

    isolated-150       OFF    CLAMP-OFF      MODE-OFF
    score           184.06  +0.25 +0.19   +1.55 +0.90
    unitsbuilt       89.15  +1.52 +2.38   +3.94 +5.47   (19/20)
    wonders           1.48  +0.05 +1.29   +0.14 +2.60
    munits           22.89  +0.62 +2.74   +0.03 +0.04

    wonders COMPLETED in the last 15 turns, per deep seat
      lategame-80    OFF 0.14  CLAMP 0.12  DEFLATE 0.10  MODE 0.12
      isolated-150   OFF 0.39  CLAMP 0.44                MODE 0.50

**The wonder premium works where wonders exist and nowhere else.** On
isolated-150 (1.48 wonders per seat per game) the mode lifts in-window
completions 28% and final wonders by t +2.60. On lategame-80 (0.22 per
seat) the deflator *costs* wonders at t -2.03 -- a wonder is the
longest build in the game and therefore the first thing an
unfinishability test refuses -- and the premium buys back exactly what
the deflator took, 0.17 -> 0.20, and no more. A verdict inherits its
venue, for the sixth time.

**What the deflator otherwise does is "build more, cheaper things":**
unitsbuilt +3.94 at t +5.47 on 19 of 20 isolated seeds. Shields that
were going into something the bell would interrupt go into something
that finishes, and `built/10` is a score term. The in-window build
distribution shows it directly: Musketeers 21.7% and Barracks II 13.0%
against Settlers at 7.3%, where Settlers are 1453 of the same cell's
rest-of-game picks.

**What the clamp alone does is grow better.** On lategame-80 it is the
only arm that moves population (pop t +2.40, mean city size t +2.69);
on isolated-150 it buys units and cities instead (unitsbuilt t +2.38,
munits t +2.74).

### 3. Fold / no-fold, and the human owns both calls

**Recommendation: fold nothing yet.** All four knobs ship at their
compiled defaults, inert, registered PLACEHOLDERs.

`endgame_horizon` is the fold CANDIDATE: it never measured negative on
either cell, both guard columns are in its favour, and its correctness
argument stands on its own. It is not folded because +0.68 (t +1.37)
and +0.25 (t +0.19) is exactly the profile the project's own method
note names -- twenty seeds are direction, forty are a claim -- and
because of the fairness call below, which is not a session's to make.

`endgame_window`, `endgame_unfinished_gain` and `endgame_wonder_gain`
go to the pile as a COUPLED triple: the premium's population is
precisely the wonders the deflator would otherwise refuse, so sweeping
them one at a time measures the wrong thing (R3's own lesson about two
rescales that are each survivable and not jointly survivable). The
shipped probe values were 15 / 0.0 / 3.0 and 0.0 is the most aggressive
setting the deflator's range allows.

**FAIRNESS, and it is a human call: stock does not know the game ends
either.** `ai/default` and `ai/baseline` are untouched and keep
planning over a fixed 24 turns on the last turn of every game. With
`endgame_horizon = 1` deep would be the only seat in any benchmark that
reads the bell. That is defensible as benchmark realism (a human plays
the bell in a scored game; 199/199 games end on the limit) and as a
real-game feature wherever `endturn` is set -- but it is the OPPOSITE
asymmetry to HEDGE's, where deep took on a cost stock does not pay.
`endgame_horizon = 0` restores comparability with everything published
before today.

### 4. Off-switches proven

Smoke cell, 10 paired seeds, `eg-off.ini` on both binaries. The
pre-session binary `6051a0e8bbbe` (HEAD `1a3c2b8689`, rebuilt
bit-for-bit and the same binary the concurrent defence-tilt run pinned)
against the session binary `cc1fab22cea8`: **10/10 scorelogs
bit-identical.**

    70f2c13136a4  ed23c9303eb5  6bfeb8bbf7cf  2314c27e8c5e  7f08e0eb79de
    ca1fe91c60b1  912773e6b97e  825c66af8f8b  4bd40ba5c2f7  7b74bb209f89

Execution counters: **0.00 `deep-endgame` lines per game in the OFF arm
of both cells** (40 games), against 243.7 / 189.1 player-turns a game
with the clamp on; `deep-endgame-unit` 0.00 in OFF and CLAMP against
191.3 / 212.5 in the mode arms.

`meson test -C build` **4/4**; `python3 benchmark/tune-audit.py` CLEAN
**164/164/164/164** (`endgame_horizon`, `endgame_window`,
`endgame_unfinished_gain`, `endgame_wonder_gain`).

### 5. What the next session must know

- **A 40-seed `endgame_horizon` arm is the single most valuable
  follow-up in this section**, and it is cheap: one knob, two cells,
  the DONE cache already holds the 20.
- **The deflator has never been run at a gentler setting than 0.0.** It
  zeroes a 750-want founder sitting on `DAI_WANT_DOMESTIC_MAX` as
  readily as it zeroes a Trireme.
- **`city_turns_to_build()` does not model a caravan arriving to Help
  Wonder**, so a wonder R2b's fleet would finish reads as unfinishable
  and is deflated. Conservative -- it can only decline to pay a wonder,
  never overpay one -- but it under-counts exactly the population R2b
  built.
- **Three consumers were deliberately NOT clamped because they ship
  off**: `deeptrade.c`'s route horizon and `deeprates.c`'s three MORT
  terms are dead code at `trade_merit = 0` / `wonder_merit = 0` /
  `rates_enabled = 0`. The rates case is worth its own note -- MORT
  appears symmetrically in all three of its terms, so a uniform clamp
  there is a NO-OP on the pick. The endgame content in the rates is the
  science term's `wait` test (a research that cannot finish before the
  bell is worth nothing), which is a different change and was not
  built.
- **Culture is DARKFX's job.** `S-ENDGAME-horizon.md` §3 deprioritised
  culture/History as metric-gaming; the derby forensics re-sized it as
  the plurality of the metric at long horizons. That §3 line now reads
  "leave culture to DARKFX", not "culture does not matter". The score
  mode built here is horizon-blind about culture: it pays great wonders
  through the score term only and prices no compounding stock at all.
- **The window is the least examined number in the session.** 15 turns
  is 19% of a T80 game and 10% of a T150 one; nothing measured whether
  a fixed count, a fraction, or a per-city function of turns-to-build
  is the right shape.

## DARKFX: the dark effects priced, and one credit that was wrong (2026-08-06)

Five code commits plus this one. `ai/deep`, `benchmark/` and doc only;
zero `ai/default`, `ai/baseline` or `server/advisors` changes. Full
tables in `findings/darkfx-2026-08-06.md`.

    dfc4d62271  ai/deep: credit a government centre with the waste it
                saves other cities
    4c4480ad21  ai/deep: price a Player-range free-upkeep effect at the
                empire it acts on
    24e5c0be4f  ai/deep: pay a building for the culture it accumulates
                before the bell
    937d763190  ai/deep: rank the rush-buy loop by want per gold
    a7a1cf68ce  ai/deep: price a government centre as a move, not as a
                second copy

**Headline: three of the effects audit's four ranked opportunities are
built, all behind PLACEHOLDER weights at 0 = off = byte-identical, and
the session's most useful output is not a score. The gov-centre
credit's first formulation left the OLD Palace standing while crediting
a new one -- and both gov-centre buildings are one per player, so every
city in the empire became a permanent Palace candidate: 3454 firings a
game at a mean credit of 2827 against a whole-ladder mean of 85, and
the arm lost 49.40 score (t -10.67) on 0 of 20 seeds. Pricing the same
term as a MOVE -- today's centres against the centres that survive it,
with the losing city charged the waste it starts paying -- turned the
identical arm into +2.15 (t +0.83).**

### 1. The numbers

Binaries pinned to scratch with `libfreeciv.so` beside them,
`--server`, `--arms deep`, 20 paired seeds, `benchmark/tuning/dark-
{off,struct,cult,buy}.ini`, every arm writing out the standing pins
(`pay_switch_penalty`, `switch_cost_price`, `peace_seek_gain`,
`trade_route_love`) and all four ENDGAME knobs off. **Every probe
weight is 1.0 -- the term's own arithmetic at face value; no number in
this session was chosen for looking reasonable.**

    isolated-150 (T150, 4p)      OFF     delta      t    wins
    STRUCT before the move fix  184.06   -49.40  -10.67   0/20
    STRUCT after the move fix   184.06    +2.15   +0.83  10/20
    CULT   (culture = 1.0)      184.06   -19.43   -5.55   3/20
    BUY    (buy_per_gold = 1)   184.06    +5.58   +1.64  13/20

    contested (T150, 8p)         OFF     delta      t    wins
    BUY                          82.44    -0.12   -0.08  10/20
    STRUCT                       82.44    -1.89   -0.88   9/20

Off-switches proven: smoke cell, 10 paired seeds, `dark-off.ini` on
both binaries. Pre-session `e0e1c20ba166` (HEAD `78f80af132`) against
session `333eb32e3846`: **10/10 scorelogs bit-identical**, and the same
ten hashes ENDGAME published. **0.00 `deep-govcenter`, 0.00
`deep-upkeepfree`, 0.00 `deep-culture` lines a game in the OFF arm of
isolated-150** (20 games). The move fix changed the binary and not the
OFF arm: 20/20 isolated-150 scorelogs bit-identical across the two
pins, so both share one baseline.

`meson test -C build` **4/4**; `python3 benchmark/tune-audit.py` CLEAN
**168/168/168/168** (`horizon_govcenter_weight`,
`horizon_upkeepfree_weight`, `horizon_culture_weight`,
`buy_per_gold`).

### 2. Culture: the arithmetic is right, the venue is wrong

The culture credit is NOT oversized -- mean 131 want against the
ladder's 85. What it does is double how many things get credit at all
(414 586 credited improvements to 830 533), because civ2civ3 puts
History on thirty-eight effects and a Temple is buildable everywhere
from turn one. The cell paid in the exact column the derby said deep
was already short of: cities -1.18, settlers -0.91, pop -12.46.

That is the derby's own caveat, measured. Culture is **14.5% of world
score at T150** and 29.5% at T300; at the tuning cells' horizon,
paying culture at the score's exchange rate buys 14.5% of the metric
with shields that were buying the other 85.5%. **The lever to test
first is the venue, not the weight** -- the credit already scales with
the turns that remain.

**And the venue could not be run.** garden2b is `ENDTURN=300`, a single
game exceeds the harness's ten-minute foreground block, and the DONE
cache is per-game so a killed block loses every in-flight game (0/20
after two blocks). `run_in_background` killed `run.sh`'s children
exactly as the ENDGAME note says: 0 servers alive, 0 DONE. **Every
long-horizon question in this project is now blocked on the harness
and not on the code.**

Two arithmetic corrections to the evidence base, both checkable:

- **`history_interest_pml = 0` in BOTH rulesets** (civ2civ3
  `game.ruleset:383`, classic `:378`). `city_history_gain()` carries an
  interest term and it is multiplied by zero in every cell we run;
  culture accumulates LINEARLY. The derby doc's "the history stock
  earns interest ... compounds" is true of the formula and inert in the
  venue.
- **The derby was civ2civ3, not classic** (its own §0(a) records the
  civ2civ3 `mapsize=PLAYER` on that map). This matters because classic
  has **zero** History effects, so a classic derby could not have
  produced the Temple and History readings at all.

### 3. Buy per gold: the mechanism barely moves

    shields bought per gold spent     OFF      ON     change
    isolated-150                     0.3881  0.3994   +2.9%
    contested                        0.4088  0.4087    0.0%
    emergency-tier share of buys      31.6% / 33.2%   untouched

The audit's "top-ups are ~4x cheaper per shield" is a fact about the
cost curve, not about the buy list: a third of purchases are in the
triage tier this change deliberately leaves alone, and in the rest the
want spread still dominates the price spread. So the +5.58 on
isolated-150 is **not attributable to the mechanism that was meant to
cause it**. Read the execution counter before the score.

### 4. Two items closed on evidence, no code

- **Disasters.** All six civ2civ3 disasters have `frequency = 10` and
  the setting is `disasters = 100`, so every one of them is
  `100 x 10 / 1 000 000 = 0.001 per city per turn` -- against illness
  at 10-30 per mille, one to two orders of magnitude smaller. Mfg./
  Nuclear Plant's pop-loss expectation over a MORT horizon is 0.024
  citizens, about 0.4 want. Below the resolution of every credit in the
  file. Reported and skipped, per the brief's own instruction.
- **Border push.** Nothing dark to price: neither ruleset carries a
  single building- or wonder-gated border effect. The only
  border-family type present is `Tile_Claimable` (four instances each,
  terrain-gated, no improvement requirement). `site_claim_bonus` stays
  deep's only border credit.

### 5. Deferred with written designs (findings §7)

Items 4 (specialist type as a decision -- two CM factors derived from
the treasury guard and the tech-upkeep bill) and 5 (`MAX_RATES` as a
closed-form ceiling term in deep's `gov_value` wrapper, priced at R1's
marginal value of the BINDING constraint). Both have their knob names,
their default-off shape and their trap written down.

### 6. What the next session must know

- **The parity weights are the obvious first entries in the tuning
  pile and were not run.** The move fix cut the gov credit's FIRING
  RATE, not its size: 2350 and 2734 mean want against the ladder's 85,
  so parity is roughly 0.036 and 0.031.
- **The structural pair is venue-split and neither side resolves:**
  +2.15 (t +0.83) isolated, -1.89 (t -0.88) contested.
- **The culture credit's wartime exemption owes its own arm.** 0.65 of
  its credits land in cities the survival model calls threatened -- the
  gate the derby blamed for zero Temples in six hundred turns -- and
  whether that exemption or the credit itself costs the expansion
  column is one A/B nobody has run.
- **All three new credits are civ2civ3-only** and structurally inert on
  every `*-classic` cell: classic has no Ecclesiastical Palace, no
  History effect of any kind, and no KRC/SoZ free upkeep.
- **The MinSize free-upkeep ladder is City-range Food upkeep**, not the
  Player-range empire term the brief grouped it with; it belongs with
  the capacity credit and this session's credit correctly skips it.
- **Pricing an effect requires knowing whether its building is additive
  or unique, and nothing in the effects tables says which.** That is
  what cost 49 points and it will recur.

## BALANCE: the AI can buy affection, and the buyer loses (2026-08-06)

Five commits plus this one. `ai/deep`, `benchmark/` and doc only; **zero
`ai/default`, `ai/baseline` or `server/advisors` changes** (`git diff
--stat 6eff59f822..HEAD -- ai/default ai/baseline server/advisors` is
empty). Full tables in `findings/balance-statecraft-2026-08-06.md`.

    91ec07258d  ai/deep: price the culture stock a conquest carries
                across
    31be0df640  ai/deep: value the peace a partner's routes and border
                give us
    a0bdbd41bc  ai/deep: buy a partner's love with the currency deep
                actually has
    f957dcf28e  ai/deep: arm a distant power against the player
                menacing us
    382518defc  benchmark: archive the BALANCE arms

**Headline: the love-buying gift is the biggest single-lever score move
this project has measured in self-play -- +8.28 on contested and +8.92
on lategame-80, both past t = 3.6 -- and the head-to-head says it is a
TRANSFER, not an advantage. Seated against `ai/default` in one game,
deep gains +1.93 (t +0.72) and the counterparty gains +5.26 (t +2.15);
deep's margin moves -3.34.** Everything ships at its compiled default,
which is off.

### 1. The census came first and it retired half the brief

The brief asked for "small gold/tech gift clauses". The gold half was
dead before any gift code existed. Over eight games, deep's treasury
above ai/default's own gold reserve has a **median of 17 gold on
contested and 0 on lategame-80** (max 164 / 100), while the gold that
buys the love shortfall has a p90 of 1085 / 1729 and the gold that
clears a refused peace clause -- `greed()`, the square of the shortfall
over ten -- has a p90 of 5904 / 6812. Affordable on 6.1% and 1.3% of
the rows with a shortfall. **Deep is not a player with a diplomacy
budget; it is a player that spends its gold the turn it arrives.**

Also measured there, and it matters for the peace valuation this
session built: **trade at stake was exactly ZERO on every row of both
civ2civ3 cells.** TRADE's 0.30 route endpoints a game is what that
looks like from the diplomacy side, so `deep_peace_worth` on these
cells is entirely its border term.

So the instrument became the tech clause -- ai/default prices one at
three times the receiver's remaining bulbs, out of no treasury at all.

### 2. The love gift executes, is accepted, and gives the game away

32.2 offers a game on contested at 96% accepted (median price 45 gold,
expected +40 love); 22.6 a game on lategame-80 at **100%** accepted
(median 1575 gold, expected **+300** love). Recipients do not
structurally refuse -- the brief's first stop condition is cleared.

    contested (T150, 8p)          OFF       ON    delta      t   wins
    score                       82.44    90.72    +8.28  +3.65  15/20
    techs                       18.56    21.36    +2.80  +4.11  15/20
    unitsbuilt                  49.71    44.79    -4.92  -3.63   5/20
    unitslost                   14.78    12.04    -2.74  -1.50   7/20

    lategame-80 (T80, 6p)         OFF       ON    delta      t   wins
    score                      169.47   178.39    +8.92  +4.57  17/20
    techs                       53.00    55.98    +2.98 +11.26  20/20
    pop                         42.07    44.32    +2.25  +2.87  13/20
    unitskilled                 11.03     9.16    -1.87  -2.64   6/20

    contested, mixed_dc (deep odd seats, ai/default even)
    metric        side          OFF       ON    delta      t   wins
    score         deep        86.15    88.08    +1.93  +0.72  13/20
    score         classic     89.85    95.11    +5.26  +2.15  13/20
    score         GAP d-c     -3.70    -7.04    -3.34  -0.86  10/20
    techs         classic     20.21    22.06    +1.85  +3.23  15/20

Score pays two points a tech, so +2.80 and +2.98 techs are +5.60 and
+5.96 of the two self-play totals. In an all-deep world every seat
gifts every other seat and everyone's tech count rises; the metric
reads that as a win because the metric is the arm's own mean score.
**Seated against a player who does not reciprocate, the only
significant score move belongs to the receiver.**

### 3. Proxy arming: the targeting is right and the clause is illegal

97 structurally eligible candidate-turns in two games of contested with
the willingness bound made non-binding, and **zero offers**. Two
independent walls, both ai/default's:

- A tech clause is refused outright while `ds_after == DS_WAR`, and the
  cease-fire that would change `ds_after` is priced at `greed(their
  love)`. Distant powers we have never dealt with sit at a median love
  of **-308**, so the cease-fire costs them a median **9486 gold**.
- `tech_trade_allow_holes = FALSE` caps a legal gift at one step of the
  receiver's tree, and a proxy worth arming is backward by
  construction, so the best safe advance that unlocks a better attacker
  for such a power is worth **45 gold**.

Nothing between 45 and 9486 can be tuned. On lategame-80 the candidate
count is **zero** -- nobody on that map is distant. The lever ships
with a census that says one line per eligible candidate whether or not
an offer follows, carrying `need` beside `reach`, because four changes
in this project have measured as "no effect" while never executing.

### 4. Culture in conquest worth: priced, executes, flat

+0.76 (t +0.90, 9/20) on contested and +0.15 (t +0.31, 4/20) on
lategame-80, on 37 663 and 12 076 conquest valuations that carried a
culture component (mean 159 and 82 want). `pcity->history` transfers
whole -- `transfer_city()` changes the owner on the same struct -- and
the exchange is `history / 50` score points at the citizen price that
scores exactly one point, the identical chain DARKFX's culture FLOW
credit uses. The flow at the same face-value weight cost -19.43; the
stock is flat, because it only fires where an assault is being valued.
The three conquest seats now read one `deep_conquest_worth()`.

### 5. Off-switch proof

Pre-session `6eff59f822` (pinned `d1e99bdd547c96`, built in a throwaway
worktree) against the session binary `20294f6a010ad8`, smoke cell, 10
paired seeds, both loading the same two-key ini: **10/10 scorelogs
byte-identical**, and the ten hashes are the same ten DARKFX and
ENDGAME published. Zero `deep-conqculture` / `deep-gift` /
`deep-lovegift` / `deep-proxy` lines in the OFF arms of both A/B cells.

### 6. Exploitability and ethics of play, said plainly

With `gift_love_gain` above zero, deep buys friendship with technology,
and a human can farm it: hold deep's love just under its own peace bar
and collect an advance. Section 2 says the farming works. With
`proxy_arm_gain` above zero, deep arms your rivals to keep you busy --
never someone allied to you and never someone you hold an embassy with,
because both hand the advance back, but the intent is what it looks
like. Both ship at zero.

### 7. What the next session must know

- **The primary metric cannot see a positive-sum mechanism, and this is
  the first lever whose self-play and head-to-head readings disagree
  about whether it helps.** `mixed_dc` already existed and cost one
  extra pair of arms. Any lever that GIVES another player something
  must go through it. 00-context §6 guarded the mirror-image failure
  (an AI tuned to beat ai/default specifically); this is the other one.
- **The mixed pair was run unmirrored** (`mixed_dc` only). Seat bias is
  identical in both arms so the paired delta stands, but a mirrored
  pair is the 40-seed version of the claim.
- **`gift_love_gain` needs the alliance clamp `trade_route_love`
  already has.** The lategame-80 median gift is expected to add +300
  love against a 250 alliance bar, and pairs ending allied go from
  27.1% to 33.2%. It was NOT added after the fact because every number
  above was taken without it.
- **Nothing here is tuned.** 100 x 100 was chosen to make the
  willingness bound non-binding, so a refusal would be the
  counterparty's rather than the budget's. `gift_border_value` x
  `gift_love_gain` is a product and must be swept as one; the same for
  `gift_border_value` x `proxy_arm_gain`.
- **The gold instrument is structurally dead, not mis-tuned.** A
  gold-denominated diplomacy lever needs a treasury policy first.
- **Proxy arming cannot be revived by tuning.** It could be revived by
  `tech_trade_allow_holes = TRUE` or by a cell where distant powers are
  met before love has bled; neither is in the suite.
- **`deep_peace_worth`'s trade half has never been exercised** -- zero
  routes on both civ2civ3 cells. It is live on classic and no classic
  arm was run.
- **The culture stock still has not been asked its real question.** Both
  cells here are short-horizon and culture is 14.5% of world score at
  T150 against 29.5% at T300. The long-horizon cell remains unreachable
  from this harness, exactly as DARKFX left it.

`meson test -C build` **4/4**; `python3 benchmark/tune-audit.py` CLEAN
**172/172/172/172** (four new keys: `conquest_culture_gain`,
`gift_border_value`, `gift_love_gain`, `proxy_arm_gain`).
## GROUND: the terrain-value round -- and the prize is the hold (2026-08-06)

**Four mechanisms briefed, three built, one retired on its own census;
plus the barbarian re-shape. The bundle is +4.03 on civ2civ3 contested
and -1.52 on classic, and the isolation says the entire civ2civ3 move
is the cheapest thing in the round -- the fortify hold, briefed as a
"small consumer" of the fortress want.** Nine code and doc commits on
`epic/ground-terrain` (shas after the rebase onto `e675c35251`):
`6b70834776` the capture census, `3bec4d64be` the barbarian re-shape,
`fdf45b29bb` the widened advisor hook, `341582f8e7` the choke export,
`43f021db71` the base want, `d4d6cb73b7` its ruleset fix,
`cf66426b00` the fortify hold, `116104c3ef` pillage, `8899c268e0` the
findings, plus this section. The A/B numbers below were taken on the
PRE-rebase binary `dfde58a6691b`; the rebase touched only append
seams (`deeptuning`, `tuning-blocks.ini`) and one doc-comment
adjacency in `deepdefence.c`, and a 2-seed re-smoke on the rebased
binary reproduces every mechanism counter exactly (deep-fort 701,
deep-ground 564, deep-pillage 359 lines, unchanged).
Detail in `findings/ground-terrain-2026-08-06.md`.

### 1. The A/B, both cells, 20 paired seeds

Binary PINNED (`dfde58a6691b`), `--server`, `--arms deep`,
file-vs-file (`identity.ini` against `ground-off.ini`, which sets
`base_fort_gain = 0.0`, `fortify_ground = 0`,
`pillage_denial_gain = 0.0`). **Every off-switch is proven by
counter**, and by the right counter: `offers` -- base candidates the
advisor put on the table -- stays at ~88 000 (civ2civ3) / ~105 000
(classic) per game in BOTH arms, while `evals` goes 0 -> 78 751 /
0 -> 101 893.

    contested (civ2civ3)     OFF        ON     delta      se      t   ON>OFF
    score                  81.07     85.10     +4.03    1.98  +2.03    13/20
    cities                  4.60      4.59     -0.01    0.13  -0.09     5/20
    unitsbuilt             49.59     46.46     -3.12    1.56  -2.00     7/20
    techs                  18.25     19.17     +0.92    0.50  +1.83    10/20
    pop                    25.93     26.82     +0.89    0.79  +1.13    10/20
    munits                 14.09     13.39     -0.70    0.48  -1.47     8/20
    unitslost              15.07     13.24     -1.84    1.66  -1.11     9/20
    unitskilled             9.26      8.47     -0.78    1.42  -0.55     6/20
    CITY LOSSES             1.05      1.60     +0.55    0.42  +1.31     8/20
      fort worker-turns     0.00    314.40
      fortify holds         0.00    440.25
      pillages started      0.00      7.80

    contested-classic        OFF        ON     delta      se      t   ON>OFF
    score                  83.23     81.71     -1.52    1.14  -1.33     7/20
    cities                  6.15      6.17     +0.03    0.15  +0.17    10/20
    unitsbuilt             50.76     48.26     -2.50    0.76  -3.29     6/20
    pop                    25.19     24.60     -0.59    0.62  -0.96     6/20
    mean_city_size          4.09      3.98     -0.11    0.07  -1.55     7/20
    munits                 14.53     15.19     +0.67    0.56  +1.20    12/20
    unitslost              14.24     11.84     -2.41    0.48  -5.06     2/20
    unitskilled            13.42     10.95     -2.47    0.47  -5.26     2/20
    landarea            46337.50  47493.75  +1156.25 1005.65  +1.15    10/20
    CITY LOSSES             6.35      5.45     -0.90    1.61  -0.56     9/20
      fort worker-turns     0.00     47.60
      fortify holds         0.00    691.60
      pillages started      0.00      9.90

**The loss guard passes on both cells.** Units lost fall on both
(-1.84 / -2.41), city losses move +0.55 (t +1.31) and -0.90
(t -0.56). Pillage does not cannibalise defence at this size.

**The venue split is real and it is the OPPOSITE of the one the brief
predicted.** Classic was expected to pay extra -- flat +100% defence,
`border_sq = 5`, NoStackDeath. It is the cell that does not pay. What
classic does show is the round's biggest single behaviour change:
units lost AND units killed both fall by ~2.4 at t > 5, on 18 of 20
seeds each. **Fewer battles happen in either direction**, which is the
fortify hold keeping blades off the field, and on classic that trade
is neutral-to-negative rather than positive.

### 2. The attribution: the producer is a wash, the consumer is the prize

One isolation arm on civ2civ3 contested, same pin, `fortify_ground = 0`
with the two want mechanisms left on:

    base want + pillage, no hold   -0.17   se 0.89   t -0.19   12/20
    ... plus the fortify hold      +4.20   se 2.14   t +1.96   13/20

**Teaching deep to BUILD fortresses is worth nothing on its own.** It
also costs a little expansion: -0.06 cities (t -2.03, 2/20) and -619
landarea (t -1.94), which is the worker-turns going into bases instead
of tiles. Teaching a blade already standing on good ground to STAY
there is worth the whole +4.

The caveat that keeps this honest, and it is not small: **the hold
fires on chokepoints as well as on bases, and a chokepoint exists
whether or not anyone built anything.** So the +4.20 is not
attributable to the fortresses it was designed to consume. The 2x2
(`base_fort_gain` x `fortify_ground`) is owed and is the first thing
a tuning pass on this family should run.

### 3. Capture Units: the gate is open and the population is empty

`deep-capture-venue inuse=1` on all 20 civ2civ3 games -- the enabler is
live, the census is running, the silence is real. Against that, over
**40 civ2civ3 games of 150 turns**:

    exposed tiles (ours, an enemy capturer could take this instant)   0
    offered tiles (theirs, ours could take this instant)              0

Neither half was built. The reason is in the enabler: the actor needs
the Capturer flag, WAR and a foreign target; the target must be
Capturable, not transporting and not a city centre -- and **only three
civ2civ3 unit types carry Capturable** (Migrants, Caravan, Freight,
`units.ruleset:560, 2638, 2691`). Nineteen capturer types against
three capturable ones is not the surface the ranked opportunity list
implied. **`action-surface.md`'s opportunity #2 is retired on census.**

### 4. Pillage ships small and sizes its own sequel

The shipped pass acts only on a blade ALREADY STANDING on an
at-war-owned improved tile -- no movement logic, cannot send anyone
anywhere. Per game, 20 seeds:

    counter                        civ2civ3    classic
    standing (unit-turns)             54.05     125.10
    pillages started                   7.80       9.90
    nearby   (the raid population)   837.70    1420.10

**The raid is 15x / 11x the population of what shipped.** That number
was taken before building the raid, not after, and it is the only
argument for or against building it.

### 5. The barbarian re-shape: mechanism yes, score no

Isolated against a PROBE binary that keeps everything else and
restores the pre-session step function (`010767f0abc3`, uncommitted,
built and pinned before the tip was rebuilt and re-verified by sha).
Venue: `raging-120` -- contested geometry, `barbarians HORDES`,
`onsetbarbs 40`, so the uprising machinery actually runs inside the
horizon. 20 paired seeds.

    raging-120              STEP       TIP     delta      se      t   TIP>STEP
    score                  75.10     75.28     +0.17    1.36  +0.13    10/20
    CITY LOSSES            16.55     16.80     +0.25    2.48  +0.10     9/20
    cities conquered       47.10     47.00     -0.10    1.84  -0.05     9/20
    pre-onset ramp turns    0.00     67.95   +67.95          +55.83    20/20
    summed injected fear   36 255    72 562  +36 307          +9.85    20/20

**The re-shape fires, doubles the terror it injects, opens 68
city-turns of anticipation per game that did not exist -- and moves
neither score nor city losses.** Twenty seeds are direction. What the
counters do establish is that the shape is now the server's: at HORDES
with four cities the scale reaches 2.52 (fear 75 against the old flat
30) and pre-onset lines carry `pre=1` from turn 31 against an onset of
40.

**Byte-identity in the no-barb cell: PROVED.** `nobarb-120`
(`barbarians DISABLED`), TIP against STEP, 10 seeds: **10/10 scorelogs
identical, 80/80 metric rows identical, 0 `deep-barb` lines.** The
whole class drops out free where the server cannot raise a horde.

Note for anyone reading the contested numbers: on that cell deep holds
~4.6 cities, so the willingness probability is ~0.017 and the
re-shaped barbarian term truncates to ZERO where the old one was a
flat 30. The re-shape is in both arms of section 1's A/B, so it is
controlled there -- but it means contested is no longer a cell where
the barbarian term does anything.

### 6. The channel asymmetry, and the counter that caught it

The first cut of the base want read `extra->defense_bonus` and
measured **zero evaluations against 625 candidate offers in two
civ2civ3 games**. Cause:

    classic    defense_bonus = 100 on the Fortress extra -- a FIELD
    civ2civ3   no defense_bonus field at all; +50% Fort and +50%
               Fortress are Defend_Bonus EFFECTS

**A ruleset fact that is a field in one ruleset can be an effect in
another, and only the ruleset's own evaluator sees both.** The fix is
`get_virtual_defense_power()` on a virtual tile. This is a new entry
for the ruleset-asymmetry list beside Rapture_Grow and Enter
Marketplace, and it is a trap for anything that starts reasoning about
tile defence.

It was caught because `deep-fort` counts `offers` BEFORE any gate and
`evals` after them. "The advisor offered the candidate" and "we priced
it" are different questions and a zero want has to answer the first
one first.

### 7. The seam in `server/advisors/`, declared

Rule 4 permits an extension point that is inert until a consumer
registers. `aw_task_value_fn` already existed and was already
registered by deep; this round WIDENED it to carry the candidate
activity and its target extra, because a hook that sees a tile and a
number cannot raise a fortress's value without raising irrigation's on
the same tile. With no hook registered not one line of `autoworkers.c`
behaves differently, and the frozen baseline never registers one. The
brief's stop condition was "the hook can't express a base want without
advisor changes BEYOND the inert-extension rule" -- widening an
already-inert hook is inside the rule, not beyond it, and it is
recorded here so the call can be reversed if the human disagrees.

### 8. Owed to the tuning pile

    base_fort_gain        40.0  PLACEHOLDER. Registered in ALL.
    fortify_ground           1  PLACEHOLDER (0/1 off-switch).
    pillage_denial_gain    1.0  PLACEHOLDER. Threshold AND scale:
                                the pass acts when gain x denied
                                output reaches 1.
    barb_anticipation_turns 10  PLACEHOLDER. The LEAD, not the shape.
    barb_size_ref_cities    10  PLACEHOLDER. Pure normalisation: the
                                empire size at which the shared
                                barbarian scaling is 1.0. It must be
                                tuned WITH barbarian_fear and
                                floor_barb_weight -- alone it just
                                rescales them.

    First run, and it is not a sweep: the 2x2 of base_fort_gain x
    fortify_ground on both cells. Section 2 cannot tell the fortress
    from the chokepoint and that is the question.

### 9. What the next session must know

- **The prize was the consumer, not the producer.** Item 4 was briefed
  as a "small consumer of (1)" and it is the entire measured move.
  Item 1, the structural hole the whole round was designed around, is
  -0.17 on its own. Be suspicious of ranking opportunities by how
  structural the hole looks.
- **The classic cell is not a smaller version of the civ2civ3 one.**
  Same code, opposite sign, and the mechanism counters differ by 6x in
  one direction (fort worker-turns 314 vs 48) and 1.6x in the other
  (fortify holds 440 vs 692).
- **Do not read the fortress want as measured.** It is measured as a
  BUNDLE component and as an isolated wash; it has never been measured
  against a cell where its want could not be confounded with the
  chokepoint hold.
- **The capture family is closed on population, not on design.** If a
  future cell routes many caravans through contested ground the census
  is already in the tree and costs nothing.
- **The pillage RAID is sized and unbuilt.** 15x the standing
  population. It needs movement logic, which is the part the shipped
  pass deliberately does not have.
- **The barbarian demand-side twin is still #39/#43** -- the expansion
  want debit for uprising pressure. Not built here. The supply side is
  now shaped and the seam is `deep_barb_empire_scale()`, which is
  exactly the quantity an expansion debit would multiply.
- **`deep-fort`, `deep-ground`, `deep-pillage`, `deep-barb`,
  `deep-capture` and `deep-capture-venue` are the new census lines.**
  `benchmark/paircmp.py --pat NAME=REGEX` pairs any of them.

`meson test -C build` **4/4 OK** and `python3 benchmark/tune-audit.py`
**CLEAN 173/173/173/173** (was 168), before and after.

## CM-COMPLETION: the posture change gets a price (2026-08-07)

Six commits on `epic/cm-completion` off `85c7dfbad4`, run in the bench
worktree in parallel with CONSEQ+WRACE. Full detail:
`findings/cm-completion-2026-08-07.md`. Progress log:
`sessions/progress-CM.log`.

**The centerpiece works and is not zero.** Putting a city on war
footing has been free in this codebase since W09-lite: the state
machine swapped one factor set for another and the food it cost
appeared in no want, no danger field and no counter. The CM solver
already answers the question -- `cm_query_result()` does not touch the
city -- so `deep_cm_counterfactual()` now runs the arrangement under
BOTH candidate postures and remembers the two surplus vectors.

18 904 flip candidates priced across 40 games on two cells, and the
two cells agree on every ratio:

                                          lategame-80   garden2b-150
    flip candidates priced                     14 307         4 597
      both arrangements feasible                 60%            51%
    of those:
      surplus vector differs                     31%            35%
      growth cost > 0                            22%            25%
      flips revoked at gain 1.0                 5.9%           6.8%

    mean surplus delta, war minus alternative:
      lategame-80    food -0.31   shield +0.67   trade -0.44
      garden2b-150   food -0.30   shield +0.59   trade -0.23

That trade -- roughly 0.6 shields bought with 0.3 food and 0.2-0.4
trade per priced city-turn -- is the first time the cost of a war
footing has been a number in this project rather than an assumption.
Agreement across two cells is worth stating given how many verdicts
here have inherited their venue.

**The solver bill is ~1.1% of game time**: 1 434 extra
`cm_query_result()` calls per game on lategame-80 (498 on garden2b),
5.05 s CPU against a ~450 s game, worst single turn 0.292 s. It is that cheap by construction, not by
luck: the counterfactual is evaluated INSIDE the free-flip test, so
only cities the gate was about to flip get priced. A charge can revoke
a flip but cannot create one, so nothing else needs the price. No bail
was required.

**`set_default_city_manager()` was made non-static** (`server/cityturn.h`,
visibility only) and it was load-bearing: 78% (lategame-80) and 59%
(garden2b) of the priced population has no deep posture at all, so its
alternative IS stock's default. Without it the counterfactual would
have been blind on most of the cities that actually go to war footing.

### The census caught a wrong design item

Item 2 was briefed to price the grow-hard entry against "the auction's
own top want at the city". **That want does not exist at this seam.**
`city_data->choice.want` is reset to -1 by the buy pass at the end of
the turn (`deepdefence.c:4660`) and this test runs in
`first_activities` at the top of the next. First probe: `build=-1.0`
on **554 of 554** evaluations, `fires=1` on all 554. The test was
vacuous.

The correction reads `pcity->server.adv->building_want[]`, the
standing building auction -- live here, what the build decision
actually reads, and made honest by D2. It is also better dimensioned:
`infracache.h` states `building_want[]` is "output multiplied by a
priority", which is exactly what `deep_unworked_tile_value()` returns,
so both sides are in ONE currency and `cm_grow_price_blend` is a
trade-off weight rather than the unit conversion W01's unbuilt
`want_t` was going to have to be. After the fix, 89 of 545 fire.

The gap it leaves, recorded rather than papered over: `building_want[]`
covers buildings only, so a city whose best build is a UNIT shows a
smaller build side and leans toward growth. Fixing that needs a
snapshot of `choice` before the buy pass clears it, in a file this
session did not own.

### Specialist factors: the gold half works, the science half is inert

DARKFX item 4, absorbed with its design. 9 328 player-turns:

                          lategame-80 (9 328)   garden2b-150 (5 933)
    gold factor  = 2      4 606   49%           3 718   63%
                 = 3      3 019   32%             943   16%
                 = 4      1 703   18%           1 272   21%
    science moved off 2     646  6.9%               0    0%

The science half is inert because civ2civ3 charges almost no tech
upkeep at these tech levels (`upkeep=0` and `upkeep=1` dominate the
census), and on garden2b-150 it moved the factor on ZERO of 5 933
player-turns. Same shape as DARKFX's "civ2civ3-only" finding in
reverse: a property to state, not a defect to fix.

Stated honestly and not sold: these are the CM's OUTPUT factors, so
they weight trade-derived gold and science as well as the specialists.
That is what the design asked for and it is defensible, but it is
broader than the name suggests.

### Off switch and score

Four new knobs, all PLACEHOLDER at 0. Smoke cell, 10 paired seeds,
both arms loading the same `cm-off.ini`, differing only in the binary
(`85c7dfbad4` vs this tip): **10/10 byte-identical**. That covers the
refactor and the visibility change too. Execution counters on the
all-on arm: 14 307 `deep-cmpost`, 19 183 `deep-cmgrow`, 9 328
`deep-cmspec`.

lategame-80, 20 paired seeds, file-vs-file, all three mechanisms at
gain 1.0:

    metric                  OFF        ON     delta      se      t   wins
    score                168.20    167.43     -0.77    1.51  -0.51   10/20
    pop                   41.55     42.64     +1.09    0.73  +1.49   12/20
    mean_city_size         5.37      5.50     +0.14    0.09  +1.51   13/20
    settlers               0.53      0.30     -0.23    0.11  -1.96    4/20
    techs                 53.08     52.91     -0.18    0.11  -1.55    6/20
    cities                 8.01      8.02     +0.01    0.17  +0.05    9/20

garden2b-150, same two files, same binary:

    metric                  OFF        ON     delta      se      t   wins
    score                252.45    256.38     +3.92    4.22  +0.93   11/20
    pop                  113.15    116.28     +3.12    2.79  +1.12   10/20
    mean_city_size         4.37      4.48     +0.11    0.12  +0.93   12/20
    unitsbuilt           150.97    158.03     +7.05    5.45  +1.29   12/20
    techs                 32.85     33.52     +0.68    0.58  +1.16   10/20
    cities                28.07     27.65     -0.42    0.68  -0.63    7/20

Score is inside noise on both. **This is not the venue split that has
bitten this project seven times**: the growth column points the same
way on both cells (`pop` +1.09 / +3.12, `mean_city_size` +0.14 /
+0.11). Where they differ is what the growth is spent on -- lategame
loses settlers (t -1.96), garden2b builds more units (t +1.29) -- a
downstream allocation difference, not a disagreement about the
mechanism. Twenty seeds are direction; three untuned placeholders at
1.0 are not a calibration.

### What the next session must know

- **The 40% infeasible rate is the biggest open item.** `valid=0` on
  5 715 of 14 307 priced city-turns: one of the two solves found no
  arrangement meeting `deep_cm_base()`'s minimal surpluses. The live
  `auto_arrange_workers()` relaxes through three fallbacks; the
  counterfactual does not, and declares no opinion instead. Correct
  and conservative, but it halves the reach. Mirroring the relaxation
  ladder is a small, well-defined increment.
- **`deep_cm_posture_delta()` is exposed and has no consumer.** The
  fear machinery's `satisfied_fear_discount` is the intended one; it
  lives in `deepdefence.c`, which a parallel session owned. FALSE from
  the accessor means "no opinion", not "costs nothing", and it answers
  only for cities the war gate priced this turn.
- **garden2b-150 IS reachable, and here is the recipe.** DARKFX §8
  recorded it as unreachable; that is true only of foreground blocks,
  which are hard-capped at ten minutes, and a killed block loses every
  in-flight game because the DONE cache is per-game. A 20-game arm
  makes 4 games in the first foreground block and 2 in the second.
  Both arms here completed from a *tracked background command* running
  `run.sh` in a relaunch-until-20-DONE loop -- about 60 minutes per arm
  under a machine load of ~23 on 12 cores (the parallel session's
  batches). isolated-150 was attempted the same way and abandoned for
  time.
- **Four PLACEHOLDERs for the cloud pile**, none searched:
  `cm_war_growth_charge` (0-1; it is subtracted from a probability, so
  it cannot usefully exceed `cm_war_survival`), `cm_grow_price_blend`
  (0-4, a same-currency trade-off weight), `cm_specialist_gold_gain`
  and `cm_specialist_sci_gain` (0-4).
- **Read the two execution counters before the score at any gain.**
  `deep-cmpost kept=0` and `deep-cmgrow fires=1` both move at gain
  1.0 (503 and 1 066); at smaller gains they may be zero, and a "no
  effect" reading would then be a non-execution reading. Fifth time
  this note has earned its place.

**Rebased onto `epic/w03-horizon-content` tip `4110b0eedd`.** Three
append conflicts in `benchmark/tuning-blocks.ini` (ALL, G, ECON),
resolved keeping both sides. Identity re-proved after the rebase
against the parent tip alone: **10/10 byte-identical** at all-zeros --
and those are the SAME ten hashes the pre-epic run published, a third
independent check that neither session moved the no-tuning path.

`meson test -C build` **4/4 OK** and `python3 benchmark/tune-audit.py`
**CLEAN 196/196/196/196** post-rebase onto `74f85b4082` (181 on the
branch alone, from 177), before and after.

## CONSEQ: the write-off is an alarm, and the birth guard was paying 100 for nothing (2026-08-07)

Eight code commits plus the shared doc pair, paired in one session with
WRACE by standing human call. `ai/deep`, `benchmark/` and doc only;
**zero `ai/default`, `ai/baseline` or `server/advisors` changes**. Full
tables in `findings/conseq-wrace-2026-08-07.md`.

    a28474416c  ai/deep: clamp a bought advance at the alliance bar
    b80ee0b42d  ai/deep: price the first defender instead of pinning
                it at 100
    ae2cd644f5  ai/deep: adjudicate every write-off against what
                happened
    3e2a4ba86e  ai/deep: route a survival deficit at a tech wall into
                the research want
    a1c73eb334  ai/deep: damp two pumps by what their output never does
    4e8e25bbea  ai/deep: stop paying full price for things about to
                expire
    edc01351af  ai/deep: record what the write-off had already tried
    4110b0eedd  ai/deep: sell a building to buy the defender, and spend
                late gold late
    37127fa779  ai/deep: walk the civilians out of a city that is
                falling

**Headline: the write-off signal, shipping since W14 and never
measured, has a precision of 0.027 and a recall of 0.969 on contested
-- 2 326 city-turns of alarm per 20 games to catch 31 of 32 losses.
Conditioning it on "no reinforcement can reach it AND the enemy is
adjacent" lifts precision to 0.202, and ADJACENCY carries the entire
lift while the treasury test carries none. Even that slice holds two
times in three, so the brief's fire sale is retired as a gate and
replaced by a priced comparison with the census as its prior.** The
session's second result is that the inherited first-defender rule pays
a flat, emergency-tier 100 on 14 000 decisions a game where P(walk-in)
is exactly zero.

### 1. The write-off census, and the conditional table

    20 seeds, one arm, pin 9619996eb4818e
    cell          writeoff     first    lost   PRECISION   recall
                  city-turns   calls
    contested        2 326       964      27     0.028     0.969
    raging-120       3 279     1 430     195     0.136     0.679

    contested slices:  p == 0  0.044   wave >= 8  0.015   wave >= 12  0.007

A bigger wave is LESS likely to take the city: the signal's own
certainty axis runs backwards.

The raw number is the wrong one, because the write-off fires BEFORE the
defence machinery answers and most flagged cities survive because the
answer works. The enriched ledger records `relief` (best survival the
whole reachable pool reaches), `pool`, `adj` and `buy` at the moment of
the call:

    contested, 40 games, 1 966 adjudications
    slice                                       n     lost   precision
    RAW                                      1 966      53     0.027
    p < 0.01                                 1 143      45     0.039
    p < 0.01 & relief <= p                     841      41     0.049
    ... & enemy ADJACENT                       109      22     0.202
    EXHAUSTED (... & rush unaffordable)         71      14     0.197
    adj >= 2 & p < 0.01 & relief <= p           38      12     0.316

### 2. The birth guard: 88.6% of first defenders were priced at zero

    contested, 20 paired seeds, pin 724e885dc2ab61, birth_guard_gain 1.0
    metric                  OFF        ON     delta      se      t   wins
    score                 85.10     84.99     -0.11    2.49  -0.05   8/20
    cities                 4.59      4.74     +0.16    0.12  +1.29  15/20
    cities gained         38.25     39.75     +1.50    1.01  +1.48  15/20
    CITY LOSSES            1.60      1.80     +0.20    0.47  +0.42  11/20
      priced firsts        0.00    747.75  +747.75   56.94 +13.13  20/20

Over 14 955 priced decisions: **P(walk-in) == 0 exactly on 88.6%**,
mean priced want 13.11 against the inherited flat 100, 96.0% below it,
mean city worth 339.65. That is derby 3's turn-2 Warriors cohort
explained. Correcting it is score-neutral on this cell and buys a small
consistent expansion move with the loss guard passing -- the brief
demanded both numbers before trading safety for tempo and both are
here.

### 3. The last-ditch ladder: one rung works, one has no population

    contested, 20 paired seeds, pin bdd23bb77f8b04, conseq-lastditch.ini
    metric                  OFF        ON     delta      se      t   wins
    score                 85.10     84.46     -0.64    0.53  -1.22   4/20
    mean_city_size         5.94      5.77     -0.17    0.09  -2.00   5/20
    CITY LOSSES            1.60      1.55     -0.05    0.34  -0.15   4/20
      shortfalls seen      0.00     41.05    +41.05    6.68  +6.14  18/20
      buildings sold       0.00      2.65     +2.65    0.66  +4.03  15/20
      ... that bought      0.00      2.65     +2.65    0.66  +4.03  15/20
      late buys            0.00      0.00     +0.00    0.00    n/a   0/20

Rung 1 (sell to defend) converts **perfectly** -- every sale completed
its buy -- and costs city size, which is the buildings themselves.
Sales: 44 Barracks, 3 Temple, 2 Aqueduct, 1 Colosseum over 20 games, so
the bar is mostly selecting the cheapest militarily-idle building. The
sign is negative at face value and that is a bar question, not a design
one.

**Rung 3 (the final defence buy) has ZERO population**: not one
`deep-latebuy` line and not one `deep-latebuy-sum`, meaning the budget
was never positive. In today's phase ordering nothing credits gold
between the main buy pass and the end of deep's phase. It stays, off,
as the seam for a future reordering, and its counter is what will say
so.

Rung 2a (civilian evacuation) is built with its arm archived and **was
not A/B'd** -- the host restart took `/tmp` and the pins.

### 4. Items 1, 4, 5: execution proven, score unmeasured

Probe: contested, 10 seeds, every new knob at face value 1.0.

    deep-defwall (tech routings)    226.5 /game
    deep-damper pump=attacker       163.2 /game
    deep-damper pump=diplomat       184.2 /game
    deep-expiry                    1545.8 /game  (15 398 impr : 60 unit)

The 250:1 building-to-unit split on expiry is the tier discipline
working: the unit deferral needs a successor whose whole requirement
conjunction lands inside the current build AND a city with no danger
and no urgency.

Obsolescence semantics verified and written down: an obsolete
improvement is AUTO-SOLD (`server/cityturn.c:236`); an obsolete wonder
is not sellable at all (`common/improvement.c:1122`) so it stands and
pays nothing; either way the effects lapse
(`common/requirements.c:2360`); and the trigger is the OWNER's research
(`improvement_obsolete()` carries `.player` and nothing wider), with all
16 civ2civ3 `obsolete_by` tech requirements at `REQ_RANGE_PLAYER`.

### 5. Not built

**Item 3 (home-city upkeep redistribution) was never reached** -- no
census, no code, no claim, and its stop condition is untested.

**Item 2's fire sale is retired as a gate** and replaced by a written
design (findings §2b): a priced comparison of stand-and-fight against
denial, whose `P(hold)` is `1 - precision(slice)` and whose slice is
nameable from the ledger this session shipped. Two mechanics verified
for its "what resisting costs the taker" term: capture costs exactly
one size point, fixed (`server/citytools.c:2141`), but each successful
attack costs another under `killcitizen` + `UCF_KILLCITIZEN`
(`server/unithand.c:4601`), and `razechance` destroys a fraction of the
buildings a denial sale would sell, so denial must not double-count
them.

### 6. Off-switch proof

Smoke cell, 10 paired seeds, `identity.ini` on both binaries, pin
`9619996eb4818e` against pin `724e885dc2ab61`: **10/10 scorelogs
byte-identical.** The three commits before the first pin are inert by
construction and by counter (the clamp is unreachable at
`gift_love_gain = 0`, the birth guard returns FALSE before reading
anything at 0, the ledger only reads and logs); the ladder and the
evacuation are gated the same way and their off-arm counters are 0.00
in the A/B above.

`meson test -C build` **4/4 OK**; `python3 benchmark/tune-audit.py`
**CLEAN 192/192/192/192** (was 177).

### 7. What the next session must know

- **Condition on ADJACENCY, never on the treasury.** 0.049 -> 0.202 is
  all adjacency; adding the affordability test moves it to 0.197.
- **The prior for any last-ditch act is "the city holds."** Two in
  three, in the tightest slice the ledger can name.
- **Rung 3's seam is empty.** Do not build on it.
- **`sell_defend_gain` is the first tuning entry the session earned**:
  perfect conversion, visible cost, and 83% of sales are Barracks.
- **`/tmp` and the pins do not survive a host restart.** Every table
  above is analysis output, not a re-derivable artefact.

## WRACE: the race can be read, and its default answer is wrong (2026-08-07)

Four code commits, paired with CONSEQ. `ai/deep`, `benchmark/` and doc
only. Full tables in `findings/conseq-wrace-2026-08-07.md`.

    e0ab700087  ai/deep: read the wonder race, and adjudicate it
                afterwards
    c294d596b2  ai/deep: act on the race verdict, on both sides of the
                line
    4e8e25bbea  ai/deep: stop paying full price for things about to
                expire   (shared with CONSEQ item 4)

**Headline: the race is readable and the composed valuation is now one
expression, but the estimator's DEFAULT is its bug -- it counts only
rivals who have already started, so its median P(win) is 1.000 against
an observed win rate of 0.371 over 35 resolved races. It therefore
UNDER-declines, which is the safe direction for a mechanism that ships
off. And the race-to-win multiplier has no population at all in the one
venue this harness can reach.**

### 1. The module

`ai/deep/deepwonder.c`: `deep_tech_eta()` (own research only -- stock's
obsolescence estimator takes a MIN over every player's bulb rate, which
is a rival model wearing a minimum sign), `deep_impr_obsolete_eta()`
(MINIMUM over the disjunctive `obsolete_by`),
`deep_utype_successor_eta()` (MAXIMUM over the successor's conjunctive
build requirements), `deep_wonder_rival_eta()` -- **the one function
W04 substitutes, and the reason the read is one function rather than
four inline loops** -- `deep_wonder_p_win()` and the race ledger.

The human's 2026-08-05 formula is now one valuation: `P(win) x want x
completion-ETA discount x obsolescence truncation`. The two truncations
this session owed are built; `P(win)` is a linear ramp on the margin in
turns scaled by `wrace_margin_turns`, so a dead heat is 0.5 and losing
by the whole margin is 0.0 -- which IS the go/no-go bar, needing no
second knob. No rival at all is 1.0 and not 0.999, so the race term
cannot tax an uncontested wonder.

### 2. The calibration, which is the stop condition's question

    contested probe, 10 seeds: 4 902 evaluations, 261 declined (5.3%)
    estimated P(win)      mean 0.938   median 1.000
    composition factor    mean 0.292   median 0.256  (all gains at 1.0)
    races resolved 35:    won 13   lost 22   observed rate 0.371
    losses at estimated p >= 0.75:  2 of 22

The brief's stop condition is "the completion estimator disagrees with
observed completions by more than its own margin". In the form stated
it does not trip -- only 2 of 22 losses were recorded at p >= 0.75, so
the estimator's CONFIDENT predictions are not the failures. What fails
is its default of "nobody is racing me". The fix is a prior on
unstarted rivals, not a different margin. (The resolved set includes
wonders deep evaluated and never started, which inflates the loss
count; the honest reading is "it says uncontested far too often".)

The face-value composition factor of 0.292 means the three truncations
together cut a wonder want by 70%. That is a large move and it is
entirely untuned.

### 3. Race-to-win: zero population, and the venue is unreachable

`deep-wrace-feed` is **0.0 a game**. In order:

1. The first probe did not set `wonder_merit`, and the help-wonder
   caravan pass is gated on it. The probe's fault; fixed and re-run.
2. With `wonder_merit = 1`: 659 caravan evaluations over 10 games with
   a real wonder city, still zero feeds. The multiplier returns 1.0
   when no rival is building the same wonder, and on contested there
   never is -- the same fact the median `P(win) = 1.000` reports from
   the other side.
3. **primary-120, the brief's own venue, could not be run**: 0 DONE
   after two full foreground blocks, and the DONE cache is per-game so
   a killed block loses everything in flight. This is DARKFX's and
   ENDGAME's standing finding for the third time: **every long-horizon
   question in this project is blocked on the harness, not the code.**
   Made worse here by a parallel session driving the box to load
   average 34 on 12 cores.

### 4. Counters and off-switches

`deep-wrace` (490.2 evaluations a game, logged with every gain OFF --
the census arm and the acting arm are one binary), `deep-wrace-sum`,
`deep-wrace-end` (3.5), `deep-wrace-abandon` (0.3), `deep-wrace-feed`
(0.0). Byte-identity across the module: 10/10 smoke scorelogs, §6 of
the CONSEQ section.

### 5. What the next session must know

- **Fix the estimator's default before tuning any of its gains.** A
  prior on rivals who have not started is the whole calibration gap.
- **Do not measure the wonder round on contested.** Its races are
  uncontested by construction, which makes three of the five WRACE
  knobs unobservable there.
- **The five `wrace_*` knobs sweep as a BLOCK**, and `wrace_margin_turns`
  is a scale rather than a bar, so it moves P(win) everywhere at once.
- **Item 4's building truncation is the largest new population in the
  pair** (15 398 a game) and has never been scored.

## M4b: the deterrence seat -- five mechanisms live, both cells null (2026-08-07)

Seven code commits on `epic/w03-horizon-content` off `6fdcd6cfd9`,
final `9275e9bb6d`. `ai/deep`, `tests/`, `benchmark/` and doc only --
zero `ai/default`, `ai/baseline` or `server/advisors` changes. Full
tables in `findings/m4b-deterrence-2026-08-07.md`. Progress log:
`sessions/progress-M4b.log`.

    5a7694ea85  ai/deep: report the defender's side of the chained-duel
                ledger
    41055ee314  ai/deep: value the stack the sortie is killing
    163b7509ed  ai/deep: pay a defender for the losses it imposes, not
                only for holding
    228bddb309  ai/deep: charge a unit for the life it is going to live
    09202e3416  ai/deep: price walls as insurance against the citizens
                an assault kills
    03620395b4  ai/deep: price the barracks as the two streams it
                actually is
    9275e9bb6d  ai/deep: charge the food half of a unit's upkeep too

**One idea: deep priced a defender by whether it HOLDS and never by
what it COSTS the attacker.** The chained-duel DP already computed the
whole attrition distribution; the city read threw the attacker-loss
half away and never had the defender-loss half at all. Both are now
one call (`deep_stack_survival_full`, `deep_city_defence_read`), and
`E[defenders killed] == E[successful enemy attacks]` is the quantity
`EFT_UNIT_NO_LOSE_POP` insures and that stock prices as `units x 2`.

### 1. The A/B: two arms, one shared OFF arm, both null on score

    contested, 20 paired seeds per arm, pin cff0da39e9dd3d (= final
    9275e9bb6d) on all three arms, 0 pairing violations.
    OFF = identity.ini, SHARED by both comparisons.

    ITEM 5 ALONE (unit_upkeep_charge 1.0)
    metric                  OFF        ON     delta      se      t   wins
    score                 85.10     83.29     -1.81    2.79  -0.65   6/20
    pop                   26.82     24.91     -1.91    0.76  -2.51   6/20
    cities                 4.59      4.37     -0.22    0.18  -1.24   7/20
    unitsbuilt            46.46     45.17     -1.29    1.57  -0.82   9/20
      upcharge             0.00   1551.00  +1551.00  103.11 +15.04  20/20
      upchargesum          0.00    715.75   +715.75   28.98 +24.70  20/20

    BUNDLE (all six gains at 1.0)
    metric                  OFF        ON     delta      se      t   wins
    score                 85.10     83.39     -1.71    2.49  -0.68   6/20
    unitsbuilt            46.46     43.41     -3.06    1.60  -1.91   6/20
    unitslost             13.24     11.24     -2.00    1.44  -1.39   6/20
    pop                   26.82     25.71     -1.12    0.85  -1.32   9/20
      deter                0.00    273.50   +273.50   42.65  +6.41  18/20
      detersum             0.00    192.30   +192.30   29.96  +6.42  18/20
      popins               0.00     93.80    +93.80   16.31  +5.75  17/20
      sortieno             0.00      8.95     +8.95    2.04  +4.40  17/20
      sortie              36.70     39.10     +2.40    3.99  +0.60  12/20
      barracks             0.00      0.00     +0.00    0.00   +nan   0/20
      upcharge             0.00   1583.35  +1583.35  102.07 +15.51  20/20

**Score is null in both cells** and that is the expected reading of six
untuned gains at face value 1.0. What the cells DO establish is
population: five of the six mechanisms fire on 17-20 of 20 seeds with
`t` between 4.4 and 24.7, so the arithmetic is exercised and the
counters can carry a tuning pass.

Per-line census, bundle arm, 20 games: deterrence dividend n=5461
(median 7.40, mean 15.81 expected attacker shields, menace 1.000 almost
everywhere); pop insurance n=1876 (median credit 7.85, mean 44.11);
sortie 36 kills taken (mean prize 29.72) against 179 DECLINED (mean
33.85, max 150) -- the pass sees big stacks and its bar turns them
down, which is a bar question with data under it rather than a guess.

The one flag: `pop -1.91` (`t = -2.51`) in the item-5 arm is the only
|t| above 2 in either table. One metric of twelve at 20 seeds, so a
flag and not a finding -- but 281 of ~299 charges a game land on
Settlers, and a settler tax is a population tax. Watch it first when
that knob is opened.

### 2. Item 4 ships built, off, and with NO POPULATION in civ2civ3

`deep-barracks` is a literal `0.00` A/B row. Not a probe-length
artifact: civ2civ3's Barracks has both effects and is buildable from
turn 1, and the credit functions are reached 363 times a game -- but
the effects are gated on a building FLAG (`BuildingFlag "Barracks"`),
and **`get_potential_improvement_bonus()` is blind to every effect a
ruleset gates on a building flag rather than a building** (it resolves
through `get_req_source_effects()` on a `VUT_IMPROVEMENT` source;
`VUT_IMPR_FLAG` is indexed elsewhere and never returned; `RPT_POSSIBLE`
does not help). Deep uses that accessor in the growth, health and
content credits too, so **any flag-gated effect in any ruleset is
priced at zero by all of them** -- very likely why stock WAGs
`HP_Regen` and `Veteran_Build` at all, and `daieffects.c:479` carries
the matching FIXME. Not fixed here: it needs the effect evaluated
against a context that knows the candidate's flags (a virtual city, or
the `struct req_context` field), which is a slice and not an
end-of-session patch to shared requirement code.

**`heal_stream_gain` and `veteran_stream_gain` are therefore untuned
AND untested against live data. Do not tune them until the accessor can
see a flag-gated effect.**

### 3. Item 5's census forced a seventh commit, and found RC4 again

The first item-5 arm measured nothing -- zero `deep-upcharge` lines.
Of 616 evaluations reaching the charge, 5 were charged: **under
Despotism/Tribal/Anarchy, civ2civ3 charges almost every unit no gold
and no shields; the only nonzero upkeep in the venue is FOOD**, which
the first commit had deliberately excluded as "the safe direction". It
was the empty direction. Fixed by summing gold + food under
`upkeep_gold_weight` -- the equivalence
`deep_defender_upkeep_discount()` already makes forty lines up, so
reuse and not an invented constant. After: 299 charges a game, 281 of
them Settlers.

Second finding, bigger: `debit=3.0` against `want=750.1` is **0.4% of a
settler bid**, and at the top of the knob's registered range (4.0) it
is 1.6%. A charge calibrated on the BUILDING side is invisible on the
UNIT side. That is **RC4 measured with a ratio attached**; it is W01's
problem, and the range was NOT widened to paper over it.

### 4. Off-switch proof

Smoke, 10 paired seeds, `identity.ini` on both binaries, base
`6fdcd6cfd9` vs final `9275e9bb6d`: **10/10 scorelogs byte-identical.**
The same proof passed **10/10** at the intermediate head `03620395b4`
(all six commits before the food fix), so identity is established twice
across the session rather than once at the end. Every mechanism is also
inert by construction at its default and its counter is absent from the
off arm.

`meson test -C build` **4/4 OK**; `python3 benchmark/tune-audit.py`
**CLEAN 202/202/202/202** (was 196).

### 5. Items 6 and 7 parked, with evidence rather than "no time"

- **Item 6 (Defend_Bonus sweep).** Defect confirmed, seam named: 13
  `Defend_Bonus` effects across four carriers answering four threat
  classes, and both stock and deep collapse them to ONE building via
  `dai_find_source_building()` under stock's own "HACK" comment
  (`daimilitary.c:1896`), inherited at `deepdefence.c:4210` where
  `wall_fear_weight` routes boat/air/barbarian/density fear into
  whichever building that call returns. Parked because **no combat
  venue in the portfolio reaches Flight** (M4a's era table), so SAM and
  SDI could never be measured. Coastal Defense is pre-Flight and is the
  member a sweep should carry first.
- **Item 7 (incite resistance).** Parked with a pointer:
  `epic/m4a:ai/deep/deepspy.c` (`633bcca985`) already prices the
  attacker half. Consuming it means merging an in-flight epic branch --
  refused. Whoever picks up `EFT_INCITE_COST_PCT` after M4a merges must
  reuse that model and must not build a second one.

### 6. What the next session must know

- **No M4b knob may be opened on this evidence.** Both cells are null
  at face value; the session bought population and dimensional
  correctness, which is the precondition for tuning, not tuning.
- **MIL tuning is DEFERRED to the block-freeze finale.** The six new
  knobs join the MIL block rather than getting a one-off sweep, and the
  finale must sweep `deterrence_gain` against `defence_worth_mult` and
  `survival_fear_value` -- three knobs funding the same body from three
  sides, so sweeping any one alone measures the wrong thing.
- **The finale CANNOT run only on early-era venues.** Per
  `findings/tech50-collapse-2026-08-10.md`, deep's head-to-head edge
  INVERTS at tech 50 (early contested holdout: deep +55.3/seed;
  tech50war-200 mirrored: deep -115.7/seed, 1-5 in games).
  tech50war-200 or a sharpened variant must be in the finale blend, or
  the finale tunes a fighter that collapses at muskets.
- **An ERA DIAGNOSIS epic now sits AHEAD of NETTHREAT in the queue.**
  Same finding: the mechanism-level forensics (why deep stops founding
  after t10; fear terms priced against early-era unit power) must
  locate which subsystem inverts before more deterrence-family
  refinement lands on top of it. NETTHREAT
  (`sessions/S-NETTHREAT-offense-as-defense.md`) stays briefed and
  queued behind it.
- **`get_potential_improvement_bonus()`'s flag blindness (§2) is a
  project-wide credit bug, not an item-4 detail.** It silently zeroes
  the growth, health and content credits for any flag-gated effect.
  **FIXED, 2026-08-12, `epic/credit-integrity`** — and the mechanism
  named in §2 is corrected there: the effect IS returned by the index,
  and it is the requirement loop that drops it. Item 4 has population
  now (0 -> 220 `deep-barracks` lines a game) and its first honest A/B
  costs 7.8 score at face value 1.0. See the CREDIT entry.
- **`/tmp` does not survive.** The M4b result directories are gone;
  every table above is analysis output, not a re-derivable artefact.
---

## M4a: the stance reads the map, spies get prices, and only one cell reaches Flight (2026-08-07)

Three items, no shared files with the other in-flight sessions. Five
commits on `epic/m4a`.

### W34 E34.4 — source 2: traits + position + research lean

`stance_source = 2` keeps E34.3's trait prior and adds, on the same
INDEX scale and inside the same span clamp:

    land    deep_free_land_share()      -> econ
    war     rivals at war / rivals      -> mil
    threat  deep_border_pressure_from() summed over rivals, PER CITY WE
            HOLD, saturated p / (p + stance_pressure_ref) -> mil
    lean    (mil_unlocking - econ_unlocking) / sum over KNOWN advances,
            classified from the ruleset (attack_strength > 0 in a unit's
            build_reqs vs an improvement's reqs) -> +mil / -econ

Adding on the index rather than on the weight is what keeps a pinned
corner exactly the extreme a free player can reach. With both gains at
zero, source 2 is EXACTLY source 1 (x + 0.0*y == x, and a trait index is
already inside the clamp), which is the per-term off-switch.

**Compiled default stays `stance_source = 0`.** The 2026-08-05 standing
call holds: not folded into any cell or default.

**Recency was NOT built, deliberately.** A.4 asks for the share of
RECENT bulbs. `struct research` records counts, a target and a goal --
no history -- so recency needs saved per-turn state plus a decay
constant to weight it. The known-advance stock is the same signal
integrated; `tech_mil` and `tech_econ` are raw on the stance line so the
question "does it discriminate" is measured rather than assumed.

**Measured**, smoke, 10 games, 40 deep seats, `stance_source = 2`
(`benchmark/tuning/m4a-probe.ini`), turn-100 lines:

    signal      min      mean      max
    econ      0.645     0.733    0.812
    mil       0.503     0.704    0.867
    mil/econ  0.654     0.963    1.273
    land      0.080     0.165    0.274
    war       0.000     0.467    1.000
    threat    0.047     0.284    0.664
    press     0.200     1.929    7.890   (raw, per city we hold)
    lean     -0.385    -0.267   -0.067
    tech_mil      3       5.4        8
    tech_econ     6       9.4       13
    pos_moved    96      99.3      100   (of 100 turns)

- **4 distinct stances in 10/10 games** on a `traitdistribution FIXED`
  cell. Source 1 could not do that: E34.3 measured 1 distinct stance per
  game there, because every player draws 50/50/50/50. **Source 2 is the
  first source that differentiates seats on the cells we actually run.**
- The tech lean **does** discriminate (range 0.32 across seats) but is
  **uniformly economic** (-0.39..-0.07): early civ2civ3 unlocks more
  buildings than attackers, so the term is a small consistent econ tilt
  plus a seat-level spread, not a two-sided signal. Honest, and on the
  line.
- **`stance_pressure_ref = 4.0` is too high for the observed
  distribution** (mean raw pressure 1.93, median well under 4), so
  `threat` sits under 0.33 most of the time. Flagged for the tuning pile
  rather than adjusted here (rule 7).

### W35 E35.1 — spy actions priced in the want currency

`ai/deep/deepspy.c`. Stock's ladder (embassy 10000, steal 9000, incite
7000, sabotage 6000 — its own FIXME calls it arbitrary) is replaced, for
the units deep claims, by prices in deep's currency:

    embassy   embassy_science_value, the want deep ALREADY pays per
              missing embassy on the BUILD side
    steal     bulbs x 3 (stock's own dai_goldequiv_tech rate) x
              spy_gold_value; targeted variants price the named tech,
              untargeted ones our own next research step
    incite    deep_conquest_worth() - incite cost x spy_gold_value
    sabotage  shields destroyed x spy_sabotage_gain
    net of    the actor's shield cost when the action consumes it

Where deep has no price (steal gold, maps, poison, plague, nuke) it
ABSTAINS and stock's ladder answers.

**The seam, and the number that judges the design.** Stock's
`dai_manage_diplomat()` bribes, defends, targets, walks and acts in ONE
call, so the action choice can only be intercepted upstream of it. Deep
runs before `deep_do_first_activities()`, takes the action for a
diplomat already standing where an action is legal, holds it, and leaves
every other diplomat-turn to stock. **No `ai/default` change was needed:
everything required was already public.**

Census, smoke, 10 games, 40 deep seats, `spy_enabled = 1`:

    seen 4304   at_seam 76   abstained 25   refused 0
    acted: steal 51, embassy 0, incite 0, sabotage 0

- **The seam is 1.8% of diplomat-turns.** Deep prices the action for a
  diplomat that starts its phase adjacent to a target; the rest are
  approaching, bribing or garrisoning. Owning more would mean owning the
  target choice and the walk, which is W35's next slice, not this one.
- **The first probe reported 107 thefts, and none of them happened.**
  `action_prob_possible()` answers "might be possible"; the server's
  handler applies `is_action_enabled_unit_on_city()`. Adding the strict
  test plus stock's already-robbed guard, and counting what the server
  DID (a real diplomat action charges movement or eats the unit), took
  it to 51 real thefts and refused 0. A refused action is no longer
  held, so stock still gets its turn with the unit.
- **embassy 0 is a finding, not a bug.** `embassy_science_value` (8.0)
  and a steal (15..735 in currency) are placeholders on scales that have
  never been calibrated against each other, so steal wins every contest
  and deep never establishes an embassy under civ2civ3 tech leakage. The
  two placeholders owe the tuning pass a joint pass, and that is the
  first question W35's next slice must answer.
- incite 0 and sabotage 0 on smoke: 100 turns is too short to afford an
  incite, and sabotage needs a war and an adjacent diplomat.

### Era reachability — only garden2b reaches Flight

`benchmark/era-check.py` + `findings/era-reachability-2026-08-07.md`.
Reads archived final savegames, parses the per-research bitstring, maps
it through the ruleset's advance order. `metrics.csv` has always had a
tech COUNT and never a name.

    cell           T   games  techs  max  highest landmark   games
    smoke        101      40   15.1   24  Gunpowder           2/10
    primary-120  121       6   17.3   22  Gunpowder           1/1
    contested    151     160   17.9   29  Sanitation          1/20
    isolated-150 151      82   29.2   43  Sanitation         16/20
    lategame-80   81     120   53.0   57  Espionage           1/20
    garden2b     301      51   53.3   83  Space Flight        2/24

Classic-ruleset attic cells separate ruleset from horizon:
`primary-120-classic` (T121) 28.8 techs mean, Flight 0/20;
`cramped-classic` (T301) Flight 3/30; `garden2-classic` (T301) Flight
6/30. **Every cell that reaches Flight is T300, in either ruleset, and
no cell below T300 reaches it at all** -- the gate is the HORIZON.
`isolated-classic` (canonical, T300 classic, no archive) is still owed a
row: a fresh 5-game run was still in the T20s after 40 minutes and the
background batch was reaped.

**Flight exists in ONE canonical cell: garden2b, 11/24 games** — two players,
separate islands, barbarians off, i.e. the cell with the least combat
in the portfolio. **W32 (air doctrine) has no venue and is parked with
evidence**; a cell built for it is the prerequisite, not the doctrine.
**Espionage (Diplomat -> Spy) is 7/24 garden2b and 1/20 lategame-80**,
so every W35 slice that assumes a Spy is priced for a unit the cells
almost never field — E35.1's Diplomat-era scope is the only scope the
portfolio can measure. Four of six cells end between Gunpowder and
Sanitation; that band is where mechanism work pays.

### Identity and hygiene

- **Byte-identity 10/10, twice.** Smoke, `--arms deep`, tuning=none,
  binaries pinned with their own `libfreeciv.so`, `--server`: first
  against the base this branch was cut from (`7c019ac9de`), then again
  after rebasing onto the moved epic tip (`03620395b4`) so the proof is
  taken where the branch will actually merge. Both new mechanisms
  default OFF (`stance_source = 0`, `spy_enabled = 0`).
- `meson test -C build` **4/4**, `python3 benchmark/tune-audit.py`
  **CLEAN 197/197/197/197** (was 194 mid-session, 191 at the epic tip).
- New tunables, all PLACEHOLDER, all for the pile: `stance_position_gain`
  0.5, `stance_techlean_gain` 0.5, `stance_pressure_ref` 4.0 (measured
  too high, see above), `spy_enabled` 0, `spy_gold_value` 0.5,
  `spy_sabotage_gain` 0.5.
- New census lines: `deep-spy`, `deep-spy-act`, and eight new fields on
  `deep-stance`.
- `benchmark/tuning/m4a-probe.ini` is a probe, not a cell and not a
  candidate default.

### What the next session should know

- **Source 2 is the first stance source that does anything on our
  cells.** If the stance layer is ever to be measured free rather than
  pinned, it is source 2 that must be A/B'd — source 1 is provably inert
  under `traitdistribution FIXED`.
- **The spy seam is small by construction.** Growing it means deep
  taking the diplomat TARGET choice (and with it incite-vs-assault as
  competing plans for one city, which is where W35's value actually is).
- **The embassy/steal placeholder mismatch is the live defect** in the
  gated pricer: deep will never make an embassy while a theft is legal.
---

## ERA instrumentation: three counters landed, the probe is written and unrun (2026-08-11)

Branch `epic/era-instrumentation`, cut from `063a86bdf7`. Phase 1 of the
ERA epic: the top three counters of
`findings/era-tech50-forensics-2026-08-11.md` §4, observation only. **No
game was run — a tuning batch owned the machine — so nothing here is a
result.**

### 1. The counters

- **`deep-expgate`** (`deepdefence.c`, `deep_expansion_blocked()`): one
  line per gate evaluation with `surv`/`thresh`/`wave`/`blocked`, plus
  `mil` (the call site's second conjunct, so the effective firing rate
  is derivable) and **`stock_blocked`** — `dai_on_war_footing()` alone,
  which is exactly the verdict deep's gate replaced. That is the
  stock-side counterfactual the forensics report calls the only way to
  learn whether deep's gate is more or less permissive than stock's,
  and it costs nothing: the value was already computed, `ai/baseline`
  is untouched, no second run is needed.
- **`deep-opodds`** (`deepops.c`, formation seat): one line per scored
  target, `verdict=refused|formed`, with the best take-probability any
  prefix of the pool reached against the stance-scaled `bar` and the
  raw `take_target`, plus a target-tile defence census behind the
  log-level guard. This closes the report's **highest-value unknown**:
  today "odds sat at 0.60 against a 0.65 bar" and "odds never rose
  above 0.1" produce identical logs and call for opposite fixes.
- **`deep-wave`** (`deepcombat.c`, `deep_city_survival_core()`): wave
  composition by the four qualification routes
  (`by_land`/`by_sea`/`by_air`/`by_transport`), `max_dist`,
  `max_turns`, `def_best`, `surv`, and `capped` for the
  `DEEP_WAVE_MAX` displacement that was silent. Aimed at the report's
  own open question: the `UMT_SEA && coastal` arm has no distance bound
  beyond `horizon × move_rate`, which for anything moving 6 exceeds a
  32×32 torus's diameter.

All three follow the existing metric style, are quieted by
`FREECIV_DEEP_QUIET`, and log only values the watched decision had
already computed. `dai_on_war_footing()` was hoisted, not added; the
wave's distance is the one the turns computation already takes.

### 2. Zero knobs

`python3 benchmark/tune-audit.py` **CLEAN 208/208/208/208**, unchanged —
counters are not knobs. `meson test -C build` **4/4** (incl.
`baseline-frozen`) after every commit.

`knobcensus.py`'s map entry for `expansion_war_survival` moves from
NO-INSTRUMENT (its "Needs:" was literally "a blocked-expansion
counter") to direct on `deep-expgate wave>=0`. Note the map has been
out of sync with `tuning-params.ini` since long before this branch —
**70 knobs are unmapped and the script's key self-check exits non-zero
on the base commit too**. That is its own pass.

### 3. The probe, written and NOT run

`benchmark/era-probe.sh` (`bash -n` clean, resumable, safe from this
worktree's root):

- **Stage 1** — the identity proof the counters owe: 10 paired smoke
  seeds, identity tuning both sides, base `063a86bdf7` binary against
  this branch's, asserting **byte-identical scorelogs 10/10**. It
  builds the base binary itself from a `git archive` export into
  `.era-probe/` (no worktree registered, the shared checkout is never
  touched) and **aborts stage 2 if identity fails** — a counter that
  moved the game would make stage 2 measure the counter.
- **Stage 2** — the decisive A/B: `tech50war-200`, mixed
  seat-mirrored, 8 seeds, `benchmark/tuning/era-a-pv1.ini`
  (principled-v1 verbatim, `expansion_war_survival` at its compiled
  0.9 placeholder) against `era-b-noexpgate.ini` (the same plus
  `expansion_war_survival = 0.0`, which `deepdefence.c:3657` documents
  as restoring stock's gate exactly — an off switch, not a tuned
  value).

Run it with the machine idle:

    nohup ./benchmark/era-probe.sh > era-probe.log 2>&1 &

### 4. What the next session must know

- **The counters are the deliverable; the 8-seed A/B is not a
  magnitude claim.** The forensics report is explicit that n=3 supports
  none, and 8 mirrored seeds barely improves on that. Read the
  `deep-expgate` / `deep-opodds` / `deep-wave` streams the probe leaves
  in every `game.log` even if the score difference is null.
- **The first question `deep-opodds` should be asked** is the shape of
  the refused-odds distribution against `bar`. Clustered just under it
  means re-derive the threshold; near zero means deep is not building
  an army that can take a tech-50 city, and no threshold change will
  help.
- **`stock_blocked` may embarrass the gate in the other direction.** If
  deep's survival gate fires *less* often than stock's flag would have,
  the expansion story in `findings/tech50-collapse-2026-08-10.md`
  needs revising again, not fixing.

---

## S5: the omniscience flag exists, and the census says it moves 1 read in 8 (2026-08-11)

Two commits on `epic/s5-omniscience-flag`, cut from
`epic/w03-horizon-content` @ `063a86bdf7`. Flag and census only, per the
human's standing scope guard: no inference layer, no knowledge structs,
no recon wants, no staleness projection, nothing fixed.

### The flag

`honest_info` (knob 209). `!= 0` ORs `H_MAP`, `H_FOG` and `H_TARGETS`
onto a deep seat's own player, every phase, from
`deep_impose_honest_info()` (`ai/deep/deepai.c:554-589`). Same shape as
`pay_switch_penalty` with `H_PRODCHGPEN`: deep issues itself the
handicap in its own hook, so **nothing in `ai/default`, `ai/baseline` or
the server changes and no exemption is asked for**. Never CLEARS a bit,
so 0 means "deep adds nothing", not "deep guarantees omniscience".

Imposed BEFORE the forwarded `dai_data_phase_begin()`, because that call
refreshes advisor data and `adv_data_phase_init()` gates its census on
`H_TARGETS` and `map_is_known` (`advdata.c:426-446`) — imposing after
would leave that census omniscient for the phase it was built in.

`deep-honest` is edge-triggered on the phase that first imposes each bit
on each seat. That is the execution evidence, and it exists because the
40-game batch this project lost to an unverified switch is the reason
the rule exists.

**Compiled default 0.** A STRUCTURAL EXPERIMENT FLAG, not a tuning
target — `1` is expected to make deep worse, and the census says in
which direction.

### The census — `findings/omniscience-census-2026-08-11.md`

Re-taken at this HEAD. The 2026-08-04 census was read at `36bac91163`
when `deepdefence.c` was 3.8k lines and is now 7.2k; **every line number
in it had moved, and its findings had not**. Both recorded (§8) rather
than bending code to the older document.

    148   distinct UNGATED omniscient reads, 13 of deep's 19 .c files
     19   information-handicap gates in ai/deep (18 H_MAP, 1 H_FOG, 0 H_TARGETS)
    ~42   in the forwarded ai/default code (33 H_MAP, 8 H_FOG, 3 H_TARGETS)
      9   in server/advisors/ (+1 in server/unittools.c)
      6   visibility gates in deep that fire UNCONDITIONALLY today

**The 70 gates and the 148 reads are disjoint sets. `honest_info` moves
none of the 148.** That ratio is the headline and the reason the flag is
an instrument rather than a fix.

**The biggest omniscience surface is not `dai_war_desire`.** It is
`deep_city_survival_core()` (`deepcombat.c:349-394`), which walks every
at-war player's entire unit list with no visibility test of any kind and
has **43 external call sites**. It gates ~14 subsystems on/off rather
than scaling them. Consequence for the roadmap:
honest-information §3's "read the fog-of-war model that has been sitting
there the whole time" **undercounts — that model has no units in it**, so
`known_force` is new PERSISTED deep state and §7's "save nothing and
rebuild on load" needs a carve-out.

**Prediction scorecard, statically checked:**

- *"Settling stops"* — **half right, and the real shape is worse.**
  `deepsite.c:589-598` zeroes an unknown catchment tile to `sum = -1` and
  DROPS it, so `deep_growth_arrivals()` truncates the site's target size
  as well as its yield. A monotone deflation strongest at the frontier,
  feeding back into fewer settlers built. `deepexplore.c:176-178`'s
  discovery term wakes in the same instant but the coupling is one-way:
  exploration gets bought, and a partly-explored site still never scores
  as if the unknown part might be good. **§5.4 priors are the missing
  half and `deepsite.c` has no place to put them.**
- *"Danger amnesia"* — **confirmed both ways.** `deepsettler.c:144-148`
  is a verbatim port of `daiunit.c:3250-3254`: "Cannot see it: assume no
  danger", with no memory, so the settler walks back into the tile it fled.
- *"Targeting starvation"* — **falsified.** Deep's menus are already
  gated on existence (`deepops.c:2612`, `:3475`, `deepdefence.c:5059`).
  It is the PRICES behind them that are omniscient — the garrison
  (`deepcombat.c:597`), the enemy's own `->worth` (`deepops.c:2620`), and
  a per-turn re-read of the target garrison for the whole march
  (`deepops.c:4258`).
- *"Active self-harm, not stalls"* (2026-08-04 §0.3) — **confirmed, with
  line numbers.** An unseen wave is `wave == 0` and 14 subsystems read 0
  as safety: fear satisfaction discounts everything (`:1856`), the melt
  disbands on `p_without == 1.0` (`:2198`), the entire counter-claim
  relief system `continue`s (`:3228`), the deterrence dividend returns 0
  (`:4150`).

**Six comments claim a read is "observed" or "visible" where no
visibility test exists** (§5), and `deepdefence.c:1901-1902` is
load-bearing on a design decision — it cites the "OBSERVED wave" as the
reason for skipping a stance multiplier. Listed so a future session does
not believe them. One prior overclaim WAS retired correctly
(`deepdefence.c:1164-1173` on `score.units` and the historian), which is
the model.

### Validation

- `meson test -C build` **4/4** (incl. `baseline-frozen`).
- `python3 benchmark/tune-audit.py` **CLEAN 209/209/209/209** (was 208).
- **NO GAMES WERE RUN.** A tuning batch owned the machine.

### Identity gate: TAKEN AND PASSED (2026-08-12)

`honest_info = 0` proven byte-identical: 10/10 smoke scorelogs
identical, base `063a86bdf7` build vs branch build, identity.ini both
sides. Same morning, the ERA counters passed their own gate 10/10
(era-probe stage 1) and the tech-50 A/B ran: pv1 **-255.5**/seed over
8 mirrored seeds (seeds 1-3 replicate calib2 nearly exactly — the
collapse is worse than the 3-seed estimate), arm B **-334.4**/seed.
CORRECTION (same day, evidence mine): arm B was NOT "gate off" —
expansion_war_survival<=0 routes to stock's war-footing gate, which
measured 3.6x STRICTER (22.6% vs 6.25% of city-turns blocked). Read
correctly, deep's replacement gate is worth ~+149/seed over stock's
(t=-1.83, n=8). The forensics' 83-88% firing claim is dead (6.25%),
and the odds BAR is not the lever either: 84.5% of consulted
refusals sit at odds exactly 0.000 — deep masses pools of 0.69-2.69
units against defenses needing 8-15. Phase 3 targets the MASSING
layer (marginal garrison release, MUSTER state), per
findings/era-probe-evidence-2026-08-12.md. Both branches merged; pv2 adopted (bec743c69f).

### The identity gate as originally recorded (now historical)

`honest_info = 0` must be byte-identical to the pre-change binary. Not
proven. The next session with the machine runs, from the bench worktree
with pinned binaries:

```sh
./benchmark/run.sh --quick --arms deep --config smoke \
    --server <build-of-6fd2e6edc5>/freeciv-server
./benchmark/run.sh --quick --arms deep --config isolated-150 \
    --server <build-of-6fd2e6edc5>/freeciv-server
# then the same two against a build of 063a86bdf7, and diff the scorelogs
```

10 smoke + 10 isolated-150 paired seeds, tuning=none, both arms deep.
Expect 20/20 identical. The flag touches one `if` on a knob that
defaults to 0, so a divergence means the phase-hook ordering change
(imposition now precedes `dai_data_phase_begin`) is not inert and the
commit needs revisiting.

### QUEUED, and deliberately not started

- **The runtime breakage census** — brief item 3 and its deliverable
  `findings/omniscience-flag-breakage.md`. Everything in the scorecard
  above is what the CODE says will happen, not what was observed. The run
  wants `honest_info = 1` in isolated-150 plus one land cell, 10-20 paired
  seeds each, `--arms deep` A/B on the tuning file, one pinned binary, and
  instrumentation for melt events, assault formations vs actual garrisons,
  foundings, explore fraction and losses. **Prove the ON state by
  execution evidence: the `deep-honest` line must appear.** A crash is a
  finding, not a session failure.
- Everything in honest-information §5 (knowledge layer, staleness,
  recon, priors, honest `dai_war_desire`). §7 of the census orders it by
  what the evidence says is load-bearing: `known_force` first and largest,
  `known_city` cheapest, §5.4 priors before any flag flip in site eval.
- Stock's Tier-3 layer stays omniscient and stays untouched
  (`dai_war_desire`, `daitech.c`, `daidata.c` — still 0 handicap checks,
  re-verified). Already upstream bug candidates 11-14. **This means any
  breakage the flag run measures is a LOWER BOUND** on what a genuinely
  honest AI would suffer.

### For the next session

- **The flag's value is diagnostic and its cost is a score drop.** Do not
  fold `honest_info = 1` into any cell, default or tuning blend. It is in
  the ALL block only because `tune-audit.py` requires it there.
- **`deepsite.c` never calls `map_get_player_tile()`** — not once, in
  1306 lines. Even with the flag ON, every yield, resource and owner it
  reads is the live value of a possibly-fogged tile. The (b)-category
  substitutions in census §3.4 are mechanical and independent of
  everything else in W04.
- **`deepwonder.c:183-186` already built the seam** for a `known_city`
  substitution and says so. It is the pattern the other consumers want —
  but note the census correction: a remembered `vision_site` carries no
  production and no shield stock, so that seam is category (c) and the
  seam alone will not be enough.

---

## CREDIT: the flag-gated effect was invisible, and the barracks credit it uncovers is far too big (2026-08-12)

Four commits on `epic/credit-integrity`, cut from `046c662b5d`.
`ai/deep`, `tests/`, `benchmark/` and doc only — zero `ai/default`,
`ai/baseline`, `server/advisors` or `common/` changes.

    65e3447552  ai/deep: evaluate a candidate building against its own
                flags                        (ai/deep/deepeffects.[ch],
                                              tests/deepeffects_test.c)
    93e5342123  ai/deep: price a candidate's flag-gated effects at the
                credit seams
    b64afdbc17  ai/deep: ask the barracks streams what ONE UNIT would
                gain
    3c19c8f3af  benchmark: add the barracks population probe

### 1. The defect, and M4b's diagnosis of it corrected

M4b §5a found that deep's credits price every flag-gated effect at
zero, and it was right. Its explanation of *why* was wrong in a way
that mattered, so it is corrected in place in
`findings/m4b-deterrence-2026-08-07.md`:

- The effect **is** in the index. `effect_req_append()`
  (`common/effects.c:276`, in tree since 2022) files an effect with a
  `VUT_IMPR_FLAG` requirement under **every improvement carrying that
  flag**, so the Barracks lookup returns it.
- What drops it is `get_potential_improvement_bonus()`'s requirement
  loop. It short-circuits a requirement naming the improvement itself —
  the city does not have the candidate yet — and evaluates every other
  requirement live, so `"BuildingFlag", "Barracks", "City"` is asked of
  a city with no barracks and comes back **definitively FALSE**. That
  is also why `RPT_POSSIBLE` did not rescue it: the requirement is
  false, not unknown.

`deep_potential_improvement_bonus()` is that accessor with the
short-circuit widened to any requirement the candidate itself would
satisfy — the improvement, or a building flag it carries. Copied rather
than reused because the seam is four lines inside a shared `common/`
function with three non-deep callers (`common/improvement.c:614`,
`rssanity.c:832`, `get_current_construction_bonus()`), and moving them
is a behaviour change in shared code to fix an ai/deep credit.

### 2. The building flag was only half of it — found by measuring

With the flag fix in, the smoke game still logged **zero**
`deep-barracks` lines. civ2civ3 gates the pair on the unit as well:
`"UnitClassFlag", "Barracks", "Local"` (`effects.ruleset:1835`), which
with no unit in the context is UNKNOWN and `RPT_CERTAIN` discards. Both
credits already know which unit they mean, so they now say so through
`deep_potential_improvement_bonus_for()` (same accessor, `utype` in the
`req_context`), and read today's rate the way the server does:
`get_unit_bonus()` — the read `hp_gain_coord()` itself makes — and
`city_production_unit_veteran_level()`. Contributions ADD to today's
rate rather than replacing it, which is what the effect system does.

### 3. Identity at default, 10/10

    smoke, 10 paired seeds, identity.ini on BOTH binaries
    base 046c662b5d  vs  final 3c19c8f3af
    10/10 scorelogs BYTE-IDENTICAL, 0 pairing violations

Not an off-switch — this fix has no knob, as the D2 credit-integrity
round's fixes had none. It is identical at default because the two
consumers that a flag-gated effect actually reaches in civ2civ3 are
`heal_stream_gain` and `veteran_stream_gain`, both compiled off, and
**no in-tree ruleset gates Growth_Food, Make_Content, Health_Pct,
History or Output_Bonus on a building flag** (15 `BuildingFlag`
requirements across 7 rulesets, all of them the Barracks pair). The
growth/health/content credits are therefore fixed against the rulesets
that would, and unmoved on ours.

### 4. Item 4 has population now — and at face value it COSTS 7.8 score

    smoke, 10 paired seeds, pin 3c19c8f3af on both arms, jobs 2,
    portbase 7500, 0 pairing violations
    OFF = identity.ini
    ON  = benchmark/tuning/credit-barracks.ini
          (heal_stream_gain 1.0, veteran_stream_gain 1.0)

    metric                       OFF        ON     delta      se      t   wins
    score                      80.45     72.65     -7.80    3.31  -2.36   2/10
    cities                      8.12      7.17     -0.95    0.53  -1.81   2/10
    unitsbuilt                 39.30     30.15     -9.15    2.52  -3.63   1/10
    techs                      15.93     15.12     -0.80    0.73  -1.10   4/10
    pop                        38.12     33.02     -5.10    1.79  -2.85   2/10
    mean_city_size              4.94      4.74     -0.20    0.15  -1.34   5/10
    settlers                    0.57      0.75     +0.17    0.31  +0.57   4/10
    landarea                90375.00  84200.00  -6175.00 4224.44  -1.46   1/10
    munits                     14.35     12.85     -1.50    1.32  -1.14   3/10
    wonders                     0.17      0.00     -0.17    0.07  -2.69   0/10
    unitslost                   3.27      3.58     +0.30    0.91  +0.33   6/10
    unitskilled                 2.80      2.88     +0.07    0.80  +0.09   7/10

Population, same 10 games: `deep-barracks` **0 lines in 10/10 OFF games
-> 220.1 lines a game in 10/10 ON games**, 6300 credited veteran
candidates and 21 credited heal candidates. M4b's literal `0.00` A/B
row is answered.

Per-line credit: **vet mean 3095.5, median 2452.9, max 16787.5** over
2201 lines (2.86 candidates a line, so ~850 want per candidate); heal
n=21, mean 9495.7. Against building credits of tens.

**That is the finding, and it is not a win.** The claim this epic makes
is credit correctness; what correctness bought is the first honest
measurement of item 4, and the measurement says the term as written is
one to two orders of magnitude too large for the auction it enters —
`unitsbuilt -9.15` (`t -3.63`) and `wonders` to literally zero are a
city queue that stops doing anything but build barracks. Same shape as
M4b §6b's RC4 finding with the sign reversed: a stream priced in
horizon-of-shields is incommensurable with credits priced in citizens.

### 5. What the next session must know

- **Both barracks knobs stay at 0.0.** The default is byte-identical
  (§3) and the only level ever tried costs 7.8 score. A sweep should
  start well below 1.0, and it should ask first whether
  `deep_veteran_stream_credit()`'s `horizon/ttb × margin × shields ×
  SHIELD_WEIGHTING` is the right dimension at all — `per_horizon` is a
  unit COUNT and the product is total shield-power over the horizon,
  while the credits beside it price citizens. That is a W01 question.
- **The heal stream is real but rare on smoke** (21 credited lines in
  10 games): it wants damaged units standing in a city that has no
  barracks yet, so a combat venue (contested, tech50war-200) is where
  it should be measured, not here.
- **The accessor is the reusable part.** Any future deep credit that
  asks "what would this building give me" should call
  `deep_potential_improvement_bonus[_for]()` rather than the stock
  accessor; the stock reading is asserted in
  `tests/deepeffects_test.c`, so the day `common/` fixes this the
  duplication fails loudly instead of going quietly redundant.
- **`meson test -C build` is now 5/5** — `deepeffects` joins
  `baseline-frozen`, `deepodds`, `deepattrition`, `deeptuning`. It
  fails on the old behaviour (verified by reverting the flag case in
  place). `python3 benchmark/tune-audit.py` **CLEAN 209/209/209/209**,
  unchanged: no knob was added.

## EXPGATE: the never-block arm becomes askable (2026-08-12)

One commit on `epic/expgate-never-block`, cut from
`epic/w03-horizon-content` @ `1e201a3ffa`. A knob and its proof — **no
A/B was run, and nothing here is a result about expansion.**

### What was missing

`deep_expansion_blocked()` (`ai/deep/deepdefence.c`) has had two regimes
since it landed: `expansion_war_survival > 0` runs deep's survival gate,
`<= 0` routes to stock's `dai_on_war_footing()`. No value meant *no gate
at all*, so the question `findings/era-probe-evidence-2026-08-12.md` §4
closed on — deep's gate is **+149/seed** better than stock's, but what
does a world with **no** gate score? — had no code path.

### The knob (210)

**`expansion_gate_enabled`**, `int`, compiled default **1**.

- `1` — both existing regimes, byte-for-byte as they were.
- `0` — the gate is `FALSE` on every city-turn *whatever*
  `expansion_war_survival` says; war never stops a city planning
  settlers and workers.

A separate switch rather than a third sentinel on the same `double`,
because that double's two regimes **are** the meaning of every archived
tuning file (`era-a-pv1.ini` omits it deliberately; `era-b-noexpgate.ini`
pins it to `0.0`). None of them mention the new key, so none of them
change meaning.

Range in `tuning-params.ini` is degenerate (`1,1,1`), the `stance_forced`
convention: an experiment-arm selector the tuner can never spend a
sample on. Blocks G/ECON/ALL, where its neighbour
`expansion_war_survival` already sits. `benchmark/tuning/expgate-off.ini`
is the arm file.

The off path still logs `deep-expgate`, with **`wave=-2`** extending the
counter's negative-wave sentinel ("the survival model was not asked") to
"and never will be". So the off arm keeps `stock_blocked` and both wants
priced on the same city-turns, and `knobcensus.py` gets a single-field
probe for the branch.

### Proof

- **Off-switch inert at default: 10/10 scorelogs BYTE-IDENTICAL**, 0
  differ, 0 missing — 10 paired smoke seeds, identity tuning on both
  sides, jobs 2, this worktree's binary against a `git archive` build of
  `1e201a3ffa` (`benchmark/results/expgate-neverblock/{base,head}`).
- **The branch is live, not dead code:** 2 smoke seeds with
  `expgate-off.ini` give **1693 `deep-expgate wave=-2` lines/game**
  against **0** at the default, same 1693 evaluations either way.
- On smoke the two arms then play the *same game* (scorelogs identical,
  both seeds) for the reason the ERA probe found: deep's gate fires on
  ` blocked=1` **0** city-turns there, while `stock_blocked=1` on 24.
  **The arm can only be priced on a war venue.**
- `meson test -C build` **5/5 OK**; `python3 benchmark/tune-audit.py`
  **CLEAN 210/210/210/210** (base 209).

### What it contributes

The third arm of the gate comparison: **deep's gate / stock's gate / no
gate**. It is deliberately unrun here — per CLAUDE.md's *parallelise
scheduling, not attribution*, the never-block corner belongs to the
tech-50 factorial the orchestrator schedules, where it is the
`expansion_gate_enabled = 0` cell against arm A (deep's gate) and arm B
(`expansion_war_survival = 0.0`, stock's gate).
## ERA phase 3a: the marginal garrison draft is FALSIFIED and score-null (2026-08-12)

Branch `epic/era-3a-massing`, based at `046c662b5d`. Four commits.
**Verdict: `op_draft_marginal` stays 0.** The mechanism does not do
what the evidence document predicted, and the score says nothing.

### What landed

- `a4e0854d0d` — **expgate counter varargs fix.** `choice.want` is an
  `adv_want` (float) and was passed to `%d`, so every conversion after
  it shifted: `mil=` was carrying `city_size_get()` and `size=` was
  uninitialised stack. Fields ahead of `mil` were sound, which is why
  the probe's gate numbers stand; what was lost was the gate's second
  conjunct. Format-only, no behavioural effect.
- `0a168de0da` — **the 3c instrument** (evidence doc §6 item 3c, which
  says instrument before touching). `deep-wave` now carries
  `far_dist` / `far_turns` / `pf_turns`: the pathfinder's turn estimate
  for the furthest qualifying attacker, beside that same unit's
  straight-line verdict. One `pf_map_new()` per survival evaluation,
  metric-log gated. `pf_turns = -1` means unreachable.
- `35afc4c2b5` — **the 3a mechanism** behind `op_draft_marginal`
  (default 0 = the old absolute floor). Registered in all four audit
  places; `tune-audit.py` CLEAN at 210 fields. Arm A' file is
  `benchmark/tuning/era-a1-draftmarginal.ini`.
- `3fc1b18e40` — **CLAUDE.md hard-rules change, authorised by the
  human this session.** The epic budget is now "at most one epic **per
  subagent session**", and "never rebuild while a benchmark is in
  flight" became "**always pin the binary** for a benchmark in
  flight". The old wording was stricter than the invariant it
  protected: what taints a run is later games execing a different
  binary at the pinned path, so a build that cannot reach that path
  cannot taint anything. This is what makes concurrent epics legal —
  each worker needs its own worktree, `build/`, `--server` path and
  port block. Read it before writing the next brief.

### Off-switch proof

10 paired smoke seeds, identity tuning, this build against a
`git archive` build of `046c662b5d`: **10/10 scorelogs BYTE-IDENTICAL**,
0 differ, 0 missing. All three code commits are inert at knob 0.

Second, independent confirmation: arm A's seeds 1–8 reproduce the
2026-08-11 probe's arm A **exactly** (−4, −204, −151, −204, +189,
−398, −398, −468).

### The A/B — pre-registered, tech50war-200 mixed, 16 seeds each arm

Harness objective (`tune.py` `mixed_sides`: final turn, seats by
`P<n>` parity, civil-war splinters excluded):

| arm | knob | mean paired diff |
|---|---|---|
| A (control) | `op_draft_marginal = 0` | **−293.4**/seed |
| A′ | `op_draft_marginal = 1` | **−241.1**/seed |

Paired over all 16 common seeds: **A′ − A = +52.3, sd 296, se 74,
t = +0.71, A′ wins 5/16.** **Score-null.** (An interim read on the 15
complete seeds gave +56.9, sd 306, se 79, t +0.72, 5/15 — same
verdict.) Both arms remain far below baseline; nothing here closes the
tech-50 gap.

### The pre-registered scorecard — 4 of 6 fail, and the proximate output moves BACKWARDS

| counter | prediction | arm A | arm A′ | verdict |
|---|---|---:|---:|---|
| `pool = 0` share of refusals | falls from 44% | 44.9% | 43.4% | flat |
| mean pool, t>130 | rises toward 8 | 2.74 | **2.61** | **FELL** |
| odds histogram, exactly 0 | mass leaves | 83.5% | **84.7%** | **did not move** |
| `deep-op form` | rises | 396 | 495 | rose (+25%) |
| `deep-op strike` | rises | 87 | 128 | rose (+47%) |
| scorelog `units_killed` | rises toward 5721 | 4338 | 4364 | flat |
| **mean `drafted`/attempt** | (the thing changed) | **0.30** | **0.22** | **FELL** |

The forms/strikes rise cannot be credited to the draft: `drafted` fell,
the pool fell, and the odds histogram did not move, so by the time the
arms' games have diverged those counts are measuring different worlds,
not a bigger wave. **The evidence doc's own falsification condition —
"if the pool rises and the odds histogram does not move, 3a is wrong
and 3b is the whole story" — is met in a sharper form: the pool did not
even rise.**

### WHY it fails, and why this was derivable before the arm was run

The release test is `gain > cost`, where `gain` is the increment in
whole-pool take-odds from appending one blade. Arm A, refusals with a
non-empty pool:

| pool | refusals | P(odds > 0) | mean odds | P(>0 \| def_n ≤ 1) |
|---:|---:|---:|---:|---:|
| 1 | 3391 | **5.4%** | 0.003 | 12.5% |
| 2 | 2813 | 9.3% | 0.010 | 25.9% |
| 3 | 1549 | 11.6% | 0.012 | 36.0% |
| 4 | 1275 | 21.5% | 0.020 | 57.1% |
| 6 | 636 | 30.0% | 0.030 | 91.4% |
| 8 | 342 | 46.8% | 0.065 | 97.3% |

**At pool = 1 the blade's take-probability contribution is exactly zero
94.6% of the time.** So `gain = 0`, `cost ≥ 0`, and the marginal test
refuses — *harder* than the absolute floor it replaced, which is why
`drafted` fell 0.30 → 0.22. A per-blade marginal criterion cannot
bootstrap a pool whose first four blades are individually worth
nothing; the gradient only switches on around pool 6–8, and mean odds
even at pool 8 is 0.065 against a 0.65 bar.

This is a **threshold nonlinearity, not a mis-set comparison**. Only a
criterion that evaluates a *set* of blades, or accumulates across
turns, can cross it. That is `DEEP_OP_MUSTER` — see
`sessions/S-ERA-3b-muster.md`, which now carries the brief and the
human's Gunpowder doctrine. **Note its success metrics are exchange
ratio / cities / score, NOT formation counts** — this epic is the
reason: forms rose 25% here while nothing real improved.

Nuance worth keeping: against weak targets (`def_n ≤ 1`) the gradient
does exist (12.5% → 91.4% from pool 1 to 6). A marginal test is not
wrong in general; it is null against the tech-50 garrisons that
dominate this venue.

### 3c — the instrument fired, and the reading is large

`deep-wave` on tech50war-200 (arm A, 4 games, 69 336 evaluations):

| phase | rows | pf unreachable | pf slower | mean gap | d ≥ 12 unreachable |
|---|---:|---:|---:|---:|---:|
| all | 69 336 | **19.1%** | **25.6%** | +0.70 | 16.8% |
| t<60 | 8 495 | 21.6% | 31.3% | +0.79 | 22.1% |
| t60–130 | 29 820 | 16.7% | 27.8% | +0.80 | 14.5% |
| t>130 | 31 021 | 20.6% | 21.9% | +0.57 | 18.3% |

The pathfinder **never** disagrees in the other direction (0.0%
faster). So ~45% of survival evaluations are shaped by a furthest
attacker that either cannot arrive at all or arrives later than the
straight-line test assumed. Nothing was changed — this is the
observation the evidence doc asked for before anyone touches
`deepcombat.c:443`.

### Next experiment: the 3a × 3c factorial

3a and 3c are on **one causal chain** — 3c inflates `n_att`, which
depresses survival, which pins blades at `AIUNIT_DEFEND_HOME`, which
starves the pool 3a was trying to fill. Two independent A/Bs cannot
attribute them. The scheduled experiment is a **2×2 over
`op_draft_marginal` × the 3c reach knob**, 4 arms × 32 games on
tech50war-200. The live hypothesis is that 3a is null *because* 3c is
upstream of it: a marginal test may have a gradient once survival stops
being depressed by attackers that cannot arrive. A sequential pair
would read that as "3a is dead" and drop it — this epic is the first
half of exactly that mistake, and the factorial is what prevents the
second half. `epic/era-3c-reach` is in flight in its own worktree.

Cross-reference: `sessions/S-NETTHREAT-offense-as-defense.md` was
updated with a promoted attacker-garrison-credit item and a new
hole-covering rearrangement item that **shares the 3a cost-side seam**
(`deep_city_stack_survival()` minus `_without()`). That seam is landed
and inert here; it does not need rebuilding there.

### For the next session

- **`op_draft_marginal` stays 0.** The code is landed, proven inert,
  and is one arm of the factorial. Do not enable it standalone.
- **Do not schedule `op_take_target`.** No gradient exists while 84.5%
  of consulted refusals sit at 0.000; it becomes tunable only after
  massing moves, which is sequencing, not backlog.
- **The expansion gate's "never block" mode still does not exist**
  (`<= 0` means "use stock's"). §4's open question remains unaskable
  until someone adds the setting. Small epic, unassigned.
- **The sea clause is untested, not harmless.** `by_sea = 0` is a fact
  about 32×32 tech50war-200, not about the clause, which has no
  distance bound beyond `horizon × move_rate`. It needs a **naval
  venue** before it can be asked at all. Unassigned.
## ERA phase 3c — honest land reach landed behind a knob (2026-08-12)

Branch `epic/era-3c-reach` (based on `epic/era-3a-massing`, which
carries the 3c instrument `0a168de0da`). One code commit:
`fc533432f8` `ai/deep: confirm the survival wave's land reach with the
pathfinder`.

### What the instrument measured (the license for the mechanism)

tech50war-200, 4 games, 69 336 wave evaluations, furthest qualifying
attacker per evaluation, pathfinder (pft_fill_unit_attack_param, the
same parameter find_something_to_kill uses) vs the straight-line
`real_map_distance / move_rate` test at deepcombat.c:443:

| phase | rows | pf-unreachable | pf-slower | mean gap | d>=12 unreach |
|---|---:|---:|---:|---:|---:|
| all | 69 336 | 19.1% | 25.6% | +0.70 | 16.8% |
| t<60 | 8 495 | 21.6% | 31.3% | +0.79 | 22.1% |
| t60-130 | 29 820 | 16.7% | 27.8% | +0.80 | 14.5% |
| t>130 | 31 021 | 20.6% | 21.9% | +0.57 | 18.3% |

The pathfinder is NEVER faster (0.0%). ~45% of survival verdicts are
shaped by a furthest attacker that cannot arrive as assumed.

### The mechanism

New tunable `survival_pf_reach` (int 0/1, **default 0 = old behaviour
exactly**). At 1, a land-arm attacker that passes the straight-line
test must also be pathfinder-confirmed within the same
`survival_horizon` bound; unreachable or too-slow attackers are
dropped from the wave. Land arm only — the sea arm admitted 0 units in
437 440 probe evaluations (inert), the air arm's reach is the point of
flight, a passenger's approach belongs to its hull. Because pf was
never faster on the measured venue, the straight-line test stays as a
free prefilter and the clause only removes attackers.

Cost design (the central problem — survival_core is deep's hottest
routine): ONE pf map per attacker per phase, lazily expanded, keyed
(unit, origin tile), shared across every city and every _without/_with
counterfactual; redrawn if the attacker moved; freed in
deep_survival_cache_clear() (same phase lifecycle as the verdict
memo). No per-evaluation pf_map_new().

### Validation

- `meson test -C build` 4/4; `tune-audit.py` CLEAN (211 fields;
  registered in deeptuning.h, TUNE_INT, tuning-params.ini, ALL + MIL
  blocks).
- **Off-switch proof: 10/10 smoke scorelogs byte-identical** at knob 0
  vs a `git archive` build of base `3fc1b18e40` (era-probe stage-1
  pattern, identity.ini both sides, ports 7200/7300).
- Wall-clock cost at knob 1, tech50war-200, same binary
  (fc533432f8), same 2 seeds, jobs 2 both arms, full T201 all four
  games. By kernel-stamped file mtimes (earliest game file -> DONE
  marker, same clock source both arms): OFF 230 s / 312 s, ON
  **167 s / 217 s** — the ON arm was ~30% FASTER per game. Mechanism:
  honest reach empties many waves outright (n_att = 0 early-returns
  skip the duel chains and the metric instrument's own pf map);
  nonzero-wave evaluations fell from 45 034 to 31 802 rows across the
  pair. The lazy one-map-per-attacker-per-phase memo is more than
  paid for. TWO CAVEATS, stated plainly: (1) the arms play different
  games once the knob changes decisions, so this is the practical
  price of turning it on, not a microbenchmark; (2) the measurement
  box's clock sources disagreed during the session (sandbox
  sleep/notification time vs filesystem mtimes, an order-of-magnitude
  apart); the mtime deltas are internally consistent and identically
  sourced for both arms, but a human-timed `time run.sh` outside the
  sandbox should confirm the absolute numbers before they are cited
  as load-bearing. The RELATIVE claim (ON is not slower than OFF on
  this venue) is supported on the only self-consistent clock
  available.
- ON-state execution evidence, from the same 2-seed cost games:
  deep-wave pf_unreach on the far attacker collapses 50.7%/20.0% ->
  0.7%/1.1%, pf_past_horizon 15.0%/15.9% -> 0.6%/0.3% (residue =
  non-land far units, exempt by design); mean n_att falls 5.19 ->
  3.50 and 2.44 -> 4.43* (*seed-2's ON game fought a bigger war —
  different trajectory, directional only); scorelogs differ from the
  OFF arm on the same seed (the knob is live).

### QUEUED — the 3a x 3c factorial (needs an explicit human go)

3a (`op_draft_marginal`) was FALSIFIED standalone on this venue: the
marginal release test frees FEWER blades than the floor it replaced
(0.18/attempt vs 0.30), because at pool<=1 the blade's take-probability
contribution is exactly zero 94.6% of the time — no gradient. But 3a
and 3c sit on ONE causal chain: the dishonest reach inflates n_att ->
depresses survival everywhere -> pins blades at AIUNIT_DEFEND_HOME ->
starves the ops pool 3a drafts from. If 3c-on unpins blades and grows
the pool past 1, the marginal draft regains the gradient it lacked.
Two independent arms cannot see that; the design is a **2x2 factorial
over op_draft_marginal x survival_pf_reach**, era-a-pv1.ini base (file
copies per cell, the era-a1-draftmarginal.ini convention), mirrored
mixed seats, >=16 seeds/cell (n=8 gave SEM 81 on a paired contrast;
the interaction contrast carries ~2x that variance), one pinned
binary, tech50war-200.

Pre-registered predictions, all readable from landed counters:

| counter | if 3c is the mechanism |
|---|---|
| deep-wave mean n_att (t>130) | falls from 6.08 |
| deep-wave mean surv | rises from 0.477/0.515 |
| deep-wave pf_turns=-1 on land far units | ~0 in ON cells (execution evidence the knob is live) |
| survival reads clearing 0.85 | rise from 39.9% |
| deep-opodds pool=0 share | falls from 44% |
| deep-opodds mean pool (t>130) | rises from 2.69 |
| draft adds/attempt | above 0.28 ONLY in the (1,1) cell |
| deep-op form / strike | rise from 199 / 36 per 16 games |

If the (0,1) cell moves score but (1,1) adds nothing over it, 3a stays
falsified even with its pool restored — that is the clean second
falsification and it costs one cell.

### Not addressed, deliberately

- The sea arm's unbounded coastal clause: inert on this venue (0 of
  437 440), bounding it would change nothing measurable — per the
  probe's section 6 "do not bound the sea clause".
- Arrival-time weighting: a pf-slower attacker still inside the
  horizon keeps full weight; the knob makes admission honest, not the
  within-horizon schedule.

## S6: deep honours its skill level by inheritance, and three paths escape (2026-08-12)

Branch `epic/s6-difficulty`, base `9ccbf8426e`. Audit + harness only; **no
`ai/deep` behaviour changed**, which was the mandate. Two commits:
`b3eab5faef` `benchmark: run any config at another AI skill level`,
`436cabca5b` `doc: audit how a difficulty level reaches ai/deep`.

Full audit: `findings/difficulty-audit-2026-08-12.md`.

### Does deep honour skill levels today?

**Yes — but almost entirely by inheritance, not by asking.** Where deep
forwards to `ai/default`, the handicap binds at stock's own consumer.
Where deep replaced a stock decision with its own, it usually stopped
applying. Deep's own code reads handicaps at 36 sites, **33 of them
`H_MAP`** for pathfinder omniscience; the rest are `H_RATES`
(`deeprates.c:374`), `H_PRODCHGPEN` (which deep *imposes on itself*,
`deepai.c:505-524`), `H_AWAY`, and a gated `H_TARGETS` that ships off.
`ai_fuzzy()` is called at 3 sites, all inherited from stock's defence code.

`science_cost` (common-side) and `H_RATES` bind unconditionally. `expand`
and `H_EXPANSION` bind through exactly one door.

### The three gaps for S7

1. **H_TARGETS on deep's ops path — the only place deep is STRONGER than
   its level intends.** `deep_op_pick_target()` (`deepops.c:2611-2613`),
   retargeting (`:3597-3599`) and the build-side target menu
   (`deepdefence.c:5147`) filter on `map_is_known` — *ever* known. Stock at
   novice/easy/normal demands `map_is_known_and_seen(..., V_MAIN)` —
   *currently visible* (`daiunit.c:621-623`). Deep already wrote the fix
   (`deep_impose_honest_info()` sets H_TARGETS, `deepai.c:575-577`) and
   ships it at `honest_info = 0`. S7 needs the bit set per level, not new
   mechanism.
2. **Two expansion producers bypass the damping.** `H_EXPANSION` reaches
   deep only via `domestic_advisor_choose_build()` (`deepdefence.c:6607`),
   and deep has since built the **colony lift** (`deepdefence.c:4957-4984`,
   a `CT_ATTACKER` bid formed on the military side) and the **founder
   pool** (`deepsettler.c:1384-1390`, sized by map emptiness). Deep reads
   `adv->max_num_cities` **nowhere**. Net at novice: settler *production*
   damped 100×, settler *stock* and *transport* undamped. The
   `settler_rush_weight`, stance and `scale_settler` multipliers are all
   clean — they multiply upstream of the advisor, so the divisor still
   lands on top.
3. **A mechanism gap, recorded per rule 3.** `H_DANGER` forces
   `city_data->danger = 1` (`daimilitary.c:976`); deep's garrison floor
   early-returns on `danger != 0` (`deepdefence.c:1738-1740`). A handicap
   meant to make novice *more* fearful silently disables deep's own
   minimum-garrison insurance, which then only ever fires at hard. Needs no
   `ai/default` change: the gate should test the real threat, not the
   handicapped proxy.

Smaller: hut valuation ignores H_HUTS (`deepexplore.c:146`) — a
wasted-production bug, not a cheat, since the server still refuses the
walk and caps the prize. And **fuzzy is thin**: stock spreads 400/1000
across `ai/default`, deep inherits 3 defence sites, so deep at novice is
*less erratic* than stock at novice by construction. That is the gap the
build-choice sampler is meant to fill.

### The harness

`benchmark/run.sh --skill LEVEL` runs any existing config at another level.
It derives a config differing only in the `SKILL=` line and runs that, so
the level enters the key through `confhash` — an env var would not enter
the key at all and a novice cell would cache-hit the hard results in the
same directory. `run-game.sh` is untouched, so `runhash` is unchanged and
**every existing result dir keeps its name and its DONE markers**. Without
`--skill`, the key computation is byte-identical: the pre-session
4-component formula recomputed by hand for `smoke.conf` against this binary
gives `88a3bfd9057b`, which is the dir the new `run.sh` used with no flag.

Both arms share the level, including both modules of a `mixed_XY` game
(`run-game.sh:113` emits the bare level command after all `create` lines;
the argument-less form rewrites every AI player, `stdinhand.c:2036-2050`).
That shared level is what goal (c) compares.

### Validation — 4 smoke games, mixed_db, 2 seeds, jobs 2, portbase 7450

| cell | dir | result |
|---|---|---|
| hard (no flag) | `smoke-88a3bfd9057b` | 2/2, 0 pairing violations |
| `--skill novice` | `smoke@novice-3c49c509b367` | 2/2, 0 pairing violations |
| `--skill novice` rerun | same dir | 2 already done, 0 to run |

Mechanism evidence that the handicaps reach **deep's own player**:

| counter | novice | hard |
|---|---|---|
| `deep-bid expand=` | 450 lines, all `expand=10` | 830 lines, all `expand=100` |
| `deep-payswitch` | **0 lines** | 2 lines (one per deep seat) |

`deep-bid` is emitted only from `deep_city_choose_build()`, so both are deep
seats reading their own vector. The zero is the sharper of the two: at
novice the level itself sets `H_PRODCHGPEN` (`ai/difficulty.c:91`), so
`deep_impose_switch_penalty()` early-returns at `deepai.c:512` and imposes
nothing — the handicap *bitvector*, not just the `expand` scalar, arrived.

`meson test -C build` 5/5. `tune-audit.py` **CLEAN 212/212/212/212**,
unchanged — this session adds no tunable. No C file was touched, so no
byte-identity run was owed; the hard path is untouched by construction.

### Direction only, n=2, NOT a result

Deep-vs-baseline seat scores in those same 4 games (deep = P1/P3):
hard 101/88 vs 54/63 and 116/96 vs 69/93; novice 65/**4** vs 72/50 and
72/67 vs 63/68. Two games decide nothing, but the `4` is a deep seat that
finished with **0 cities and 1 tech** and is worth a look before the ladder
measurement — as is the tech collapse both modules show at novice
(15-20 → 7-11), which is `science_cost = 250` binding common-side on
everyone, as designed. The ladder session owns the real numbers.

### Feasibility read on the two randomizer ideas (S7 design input)

- **City-choice sampler**: hooks the two `deep_want_cmp()` calls in
  `deep_city_choose_build()` (`deepdefence.c:6650,6679`), where every
  producer's bid is simultaneously in scope. Cheap. Three constraints are
  load-bearing: temperature 0 at hard must be an argmax *fallback branch*
  (not a limit of the arithmetic) or the corpus dies; tiers are not
  samplable (`DWT_EMERGENCY` outranks value, and a grave-danger defender
  losing a coin flip is a lost city); and the candidate set at that seam is
  only 2-3 wide, because `building_advisor_choose()` collapses
  `building_want[]` to a single maximum before deep sees it. **A second
  hook inside `building_want[]` is needed for it to read as "picked a
  plausible-but-worse building".**
- **Tuning jitter**: `deep_tuning_get()` returns a file-scope global read
  at **210 call sites across 16 files**, with no player parameter anywhere
  in the API. Per-player values need either that refactor, or a
  phase-boundary swap in `dpai_data_phase_begin()` whose safety depends on
  first inventorying every read reachable from a shared-context callback
  (the `choose_building` vtable hook, treaty evaluation, anything under
  `advisors/`). That inventory is a prerequisite, not a detail. It should
  also follow the tuning pass owed in `NEXT-SESSION.md` §0 — jitter around
  placeholders is noise around arbitrary points. **Build the sampler
  first.**

### Where the plan was stale

`sessions/S-difficulty-audit.md:33-37` records deep reading only `H_MAP`
(17 sites), `H_FOG`, `H_AWAY` and calling `ai_fuzzy()` twice, as of
2026-08-04. Eight days on it is 33 `H_MAP` sites, `H_RATES` honoured,
`H_PRODCHGPEN` self-imposed, `H_TARGETS` implemented behind `honest_info`,
and 3 `ai_fuzzy()` sites — the S5 omniscience epic. The stock half of the
brief is exact; nothing in `ai/difficulty.c` has moved since `0896b34`.
## NETTHREAT — offense as defense, three seams landed inert (2026-08-12)

Branch `epic/netthreat`, based on `epic/w03-horizon-content` at
`9ccbf8426e`. Three code commits, brief
`sessions/S-NETTHREAT-offense-as-defense.md`:

- `f7d8f01f1b` `ai/deep: credit an attacker's build want with its
  garrison contribution` (item 3, the human's PROMOTED LEAD)
- `1620a2b4f0` `ai/deep: price a defender's removal after the cheapest
  covering move` (item 4, the shared cost-side seam)
- `850f047d1e` `ai/deep: measure the wave mass our own offense is about
  to engage` (item 1, **as an instrument, not a discount** — see below)

### Item 3 — the attacker's garrison credit (`attacker_garrison_gain`)

The defect, at the one comparison that decides what a city builds: the
defender want carries survival plus the deterrence dividend, and
`deep_assault_want()` carried take-odds and nothing else — so a
home-based attacker's defensive worth was exactly zero at the build
seam, while `deep_city_survival_core()` counts it and the sortie pass
spends it.

Credit = the same marginal-attrition term the dividend pays (the
shared helper `deep_marginal_attrition()`, extracted from
`deep_deterrence_dividend()` with its behaviour unchanged), times
`deep_garrison_menace()`, times the **observed** cadence.

The cadence is measured, not assumed: `deep_cadence_sample()` runs once
per new phase in EVERY arm, one pass over `pplayer->units`, and
accumulates attack-rated unit-turns spent standing in own cities.
Saved with the player (it drives a want, not a log line). First
reading, 10 smoke seeds: **mean share 0.891 over 3 409 player-turns,
0.821 at T100** — attackers on this venue are home about five nights in
six, which is why the credit was worth writing.

The no-double-spend rule is stated in full in the code at the credit's
own site. Summary: the dividend pays the DEFENDER body and this pays
the ATTACKER body (at most one of two competing bids is built);
`sortie_prize_gain` never enters a build want at all, and the sortie
half is deliberately left UNCREDITED; and `home_share < 1` is what
stops one body being paid for marching and garrisoning at once.
Applied after gate 5 and the never-launches damper, because both price
offensive appetite and a stalled op's garrison value is exactly what
it leaves behind.

### Item 4 — holes get covered (`cover_removal`)

One shared helper, `deep_cover_removal_cost()`, three consumers by
design. Wired: the kill sortie's survival gate and the ops layer's
garrison draft (BOTH release tests — the absolute floor and ERA 3a's
marginal one). Noted in `deepdefence.h` as the third consumer:
MUSTER's home-defence abandon trigger.

Priced empire-wide, which is the point:

    cost = [surv(A) - surv(A - blade + cover)] + [surv(B) - surv(B - cover)]

minimised over covers. The lender's loss is charged in FULL rather
than waved through on the `must_stay_safe` gate — "still above the
floor" is not "free" — so a rearrangement dearer than the hole it
fills is rejected, `cover` comes back NULL and `cost` is the old local
hole exactly.

No new optimizer, per the brief: candidates are own cities on the
continent, the lendable blade is `deep_city_sparable_blade(...,
must_stay_safe)`, the arrival bound is
`deep_defend_counter_claims()`'s own eta test on the cover's move
rate, and the ordered move runs through `deep_relay_execute()` as a
one-edge brigade, so the priced move and the ordered move are the same
act. `deep_relay_chain()` is the multi-hop generalisation and is
deliberately NOT called.

Ordered only where the removal is committed in the same breath (the
sortie). The draft prices without ordering — its formation can still
refuse, and opening a march for a hole that never opens is the trickle
the muster discipline exists to prevent.

Execution evidence, 2 smoke seeds with the knobs on: **175 removals
priced, 2 covered.** Example, turn 96, P2 city 170: a Warriors leaves,
a Chariot backfills from city 125, naive cost **0.422** against a
post-rearrangement cost of **−0.125**, and the city's own survival
**0.026 → 0.574**. The covering blade was STRONGER than the departing
one, which is the case the local-hole price could never see. Rare on
smoke (17 and 9 sorties per game, none of which coincided with an
available cover), so the ordering path is landed but not yet observed
firing — that wants a venue with denser cities.

### Item 1 — measured, not implemented, and the measurement is why

`deep-wave` gains `net_pinned` / `net_marching` / `launch`: qualifying
attackers standing in a city one of OUR live operations points at
(split by whether that op reached MARCH/STRIKE), and the observed
`ops_launched / ops_formed` rate. Two accessors in `deepops.c`
(`deep_op_live_targets()`, `deep_op_launch_rate()`); lifts excluded;
the per-attacker test is skipped entirely for a player with no live
op, so the hottest routine pays nothing in the common case.

First reading, 10 smoke seeds, 10 309 wave evaluations, 22 753
admitted attacker-slots:

| quantity | reading |
|---|---:|
| `net_pinned` slots | **489 (2.15% of the wave)** |
| evaluations with any pinned member | 286 (2.77%) |
| `net_marching` slots | **17 (0.07%)** |
| mean observed launch rate | **0.026** |

**No knob was added, deliberately.** The discount item 1 asks for
would net at most ~2% of the wave, of which 3.5% belongs to an
operation that is actually moving, weighted by a launch rate of 0.026.
A mechanism gated on a quantity measured this close to zero is a knob
with nothing behind it, and the brief's own guard ("weighted by
OBSERVED launch/engage rates, not by intents") is what rules it out.
Same precedent as ERA 3c: instrument first, and let the instrument
decide whether the mechanism is worth writing. **This is smoke, not
tech50war-200** — the venue where wars conclude is where the number
must be re-read before item 1 is finally abandoned or built.

### Item 2 — NOT REACHED

The pin discount (rivals' garrisons are unlikely wave members) got
neither a mechanism nor its cheaper counter-only probe. It is the
remaining NETTHREAT item.

### Validation

- `meson test -C build` **5/5 OK**; `tune-audit.py` **CLEAN at 214
  fields** (212 at base + `attacker_garrison_gain` +
  `cover_removal`), each registered in all four places
  (`deeptuning.h`, `TUNE_*` macro + compiled default,
  `tuning-params.ini`, and the ALL / M / MIL blocks).
- **Off-switch proof: 10/10 smoke scorelogs BYTE-IDENTICAL** at
  default knobs, `identity.ini` both sides, this build against a
  `git archive` build of the base `9ccbf8426e`; 0 differ, 0 missing,
  maphashes equal on all 10. Ports 7100, jobs 2. Results under
  `benchmark/results/netthreat-identity/` (`smoke-30578cb4de0d` =
  this build, `smoke-c25f7c802c11` = base).
- The identity arm confirms the split the design intends: **zero
  `deep-agc` and zero `deep-cover` lines** (both mechanisms inert at
  0), while `deep-cadence` (3 409 lines) and the `net_pinned` fields
  run in every arm and do not perturb play — byte-identity is the
  proof of that.

### For the next session

- **Both knobs stay 0.** They are unmeasured on a venue that can see
  them; smoke cannot (2 covers in 2 games, 0 eliminations).
- **`cover_removal` moves the cost side of the 3a×3c factorial**, as
  the brief's attribution note warns. It is landed and inert, so the
  queued 2×2 is unaffected — but if it is ever enabled, it must enter
  that design as an arm, never concurrently with it.
- **Item 1's discount is parked on evidence, not on effort.** Re-read
  `net_pinned` / `net_marching` / `launch` on tech50war-200 before
  deciding; if the mass is ~2% there too, record item 1 as measured-
  dead rather than deferred.
- **Item 2 is untouched** and its cheap probe (do garrisoning units
  actually attack us within the horizon?) is still the right first
  move.
- The A/B for these knobs needs `tech50war-200` — the brief's venue
  note stands: contested@150 produced 0 eliminations in 16/16 games
  and cannot see any of this.
## GOV: government choice gets the amortized grammar, and ships off (2026-08-12)

Branch `epic/gov-valuation`, based on `epic/w03-horizon-content` at
`9ccbf8426e`. Six code commits plus this section; `ai/deep`,
`benchmark/` and doc only. No `ai/default`, `ai/baseline` or
`advisors` change of any kind.

    d99d3f333e  ai/deep: own the government valuation hook
    543de9b96c  ai/deep: price a government as an output stream over the horizon
    4cb5fca771  ai/deep: charge a revolution the anarchy it costs
    daa748835e  ai/deep: charge a government the army it has to pay for
    484b301a88  ai/deep: credit a government the rate ceiling it unlocks
    491e4f23ef  ai/deep: leave the rates where it found them after its extra passes

### The scope this recovers, and where it came from

D3 (site truth) fenced itself with "no government valuation
(separately queued)". The queue row it pointed at is
`sessions/README.md` row 6a -- government valuation via the
`gov_value` hook, the seam map's item 4: *"unplanned, moderate value,
cheapest seam in the tree"*, zero `ai/default` change because the hook
is a vtable entry deep already forwards, ranked 4th only because
governments change a handful of times per game. The deferral was the
one-epic rule, not a doubt about the work.

The SHAPE came from two other documents and this session followed
both. `findings/amortization-audit-2026-08-07.md` item 1d: *"a
revolution is an upfront cost (anarchy turns x forgone surplus)
against a per-turn stream (the new government's deltas) amortized over
the remaining game. The gov-value session (ladder 6a) should be
written in exactly this form; noted here so it inherits the grammar
rather than reinventing a WAG."* And `findings/darkfx-2026-08-06.md`
item 5, the MAX_RATES ceiling term: *"one PLACEHOLDER weight,
`gov_rate_ceiling_gain`, 0 = off"*.

**Nothing in the recovered scope contradicted the code.**
`dpai_gov_value` still forwarded to `dai_gov_value` untouched;
`dai_gov_value` (`daidata.c:524`) still reads exactly as the plan
describes it; `adv_best_government` (`advdata.c:993`) still calls the
hook once per candidate inside its `govt_reeval` throttle. No stale
line references, nothing to question.

### The mechanism, in the currency everything else uses

    value(gov) = (output(gov) - output(incumbent)) * deep_horizon_turns()
                 - anarchy_turns * (output(incumbent) - output(anarchy))
                 + ceiling credit
    *val       = amortize(value, techs still missing)

`output()` is the empire's arranged per-turn output in the advisor's
tile-output currency (`dai_city_want` summed over cities, stock's two
percentage bonuses applied to that level), MINUS the unit upkeep bill
under that government. Five things changed from stock:

1. **A delta, not a level.** Staying put scores exactly 0, so a
   government has to be better than the incumbent rather than merely
   large. Consequence, flagged rather than corrected blind:
   `adv->goal.govt.val` -- which feeds the government tech want in
   `dai_manage_government` -- now rides a delta scale instead of
   stock's absolute-level scale, so that injection is smaller and more
   often floored at its `MAX(val, 100)`. Readable in an A/B.
2. **A horizon.** The per-turn delta is counted over
   `deep_horizon_turns()` (MORT, endgame-clamped), which is what makes
   a one-off cost comparable with an ongoing gain at all.
3. **The revolution is charged**: anarchy turns x the output those
   turns lose, both measured, clamped at zero from below.
4. **The army is charged.** `dai_city_want` says in its own source
   "Unit upkeep isn't handled here", and that is the half governments
   differ on most.
5. **The rate ceiling is credited** -- shipped OFF, see below.

**Deliberately dropped: stock's per-government effect sum** at
`turns = 9999` ("TODO: Set to correct value" in its own source). The
arrangement already prices what a government does to output and, via
the CM's happiness constraint, to order; pricing the same effects
again in a second currency at a fake horizon is RC4 inside one
function. **The blind spot that leaves, named rather than hidden:**
government effects with no output shadow -- veteran chance, HP regen,
bribe cost. Unit upkeep and the rate ceiling came back as their own
terms; the military ones did not, because pricing them honestly is a
combat-currency question and not a government one. First thing to
look at if a gov A/B comes back negative on a war venue.

**The CM coupling is now deliberate.** `auto_arrange_workers()`
honours `pcity->cm_parameter`, which deep sets and stock does not, so
every candidate government has been evaluated through deep's own city
manager since `deepcm.c` landed (seam map 2.C, "not a bug; not
designed either"). It stays that way, now on purpose: deep's CM is how
deep's cities will actually be run, so it is the right arrangement to
value a government under.

### Two terms are structurally zero, and the counters say which

- **Anarchy at `hard` is zero, by the handicap table.** `H_REVOLUTION`
  is set at AWAY, NOVICE/RESTRICTED and EASY only (`ai/difficulty.c`),
  and `handle_player_change_government()` gives an AI without it a
  0-turn revolution. Every benchmark cell runs `hard`. Measured rather
  than assumed: smoke seed 1 at `hard`, `anturns=0.0` on 527/527
  candidate valuations; the SAME seed at `easy`, `anturns=3.0`
  (REVOLEN_RANDOM at revolen 5, mean (5+1)/2) with a charge on 467 of
  533 -- e.g. Tribal at `delta=0.0` scoring `val=-177.0`, an equal
  government now correctly not worth three turns of anarchy. The
  diplomacy-gamestate finding said "revolution timing: non-issue at
  hard"; this is that claim with a counter under it. The term is
  written anyway, because it is what makes the comparison correct
  where anarchy is real, and because only a term that READS the game
  has a zero worth trusting.
- **The rate ceiling ships off.** `gov_rate_ceiling_gain` is
  PLACEHOLDER 0.0: it converts a rates-enumeration total into a
  government want and nothing has measured that ratio (rule 7). At 1.0
  it is live and ordered by ceiling -- smoke seed 1, turn 10, under
  Despotism: Monarchy +18.0, Communism +82.5, Federation +127.2, with
  `ceil` non-zero on 339 of 528 lines. At the shipped 0.0 it is
  `ceil=0.0` on 518/518 in the same arm.

### The unit upkeep spread, which is why term 4 exists

smoke seed 1, turn 100, P1 under Despotism, the SAME army priced under
seven governments (`upk=`, want per turn):

    Communism 34   Monarchy 36   Tribal 68   Republic 102
    Federation 144   Fundamentalism 144   Democracy 216

6x between Communism and Democracy on identical units, and the
valuation was blind to every bit of it one commit earlier.

### Validation

- Build `-Daimodules=classic,tex,baseline,deep`; `meson test -C build`
  **5/5 OK** at every commit.
- `tune-audit.py` **CLEAN, 212 -> 215**: `gov_valuation`,
  `gov_anarchy_weight`, `gov_rate_ceiling_gain`, each in all four
  audit places (struct field, `TUNE_*` macro, compiled default,
  `tuning-params.ini`) and in the G and ALL blocks -- G is where the
  rates knobs live, and this is the same decision seam. All three are
  also registered in `benchmark/knobcensus.py`'s instrument map
  (`deep-gov` plus the field each knob scales).
- **Off-switch proof: 10/10 smoke seeds byte-identical.** Deep arm,
  compiled defaults both sides, under `benchmark/results/gov-identity/`:
  `smoke-8fb20fa644e8` is a `git archive` build of base `9ccbf8426e`,
  `smoke-6dee67816e29` is `491e4f23ef`. scorelog sha256 SAME on 10/10
  seeds, 0 pairing violations, and 0 `deep-gov` lines anywhere in the
  default arm -- the hook is owned and inert at the same time.
- **Liveness, 2 games knob-on** (`tuning/gov-on.ini`,
  `smoke-187bec478774`): 518 and 474 `deep-gov` lines across 32 and 29
  evaluation bursts, all 4 players, `delta > 0` on 278 and 284 of
  them; `anturns=0` and `ceil=0` throughout, exactly as the two
  structural zeros above predict. Both scorelogs DIFFER from the off
  arm on the same seed, so the valuation is deciding and not just
  logging. What it decides, at T101:

        seed  stock's valuation             deep's valuation
        1     Monarchy x3 + TRIBAL          Monarchy x4 (one mid-revolution)
        2     Monarchy x2 + TRIBAL x2       Monarchy x4

  Stock leaves players parked in Tribal; deep moves them to Monarchy.
  Directional only -- 2 games, and no score claim is made from them.

### NOT done, deliberately: no A/B

This session was scoped to build the mechanism and prove the switch,
on a box already running a factorial (jobs 2, smoke/identity games
only). **The valuation is therefore unmeasured for score.** What it
needs next, in order:

1. A paired A/B of `gov_valuation = 1` against the default on
   contested and isolated-150 (>= 20 seeds), reading the government
   end-state census above as the mechanism check.
2. Only if (1) is not negative: a tuning pass on
   `gov_rate_ceiling_gain` (0-2 registered) and `gov_anarchy_weight`
   (0-3) -- the latter only where anarchy exists at all, which means a
   difficulty below `hard`, so it belongs to the difficulty-ladder
   session (row 17) rather than to a normal cell.
3. If the A/B is negative on a war venue, the dropped military effect
   sum is the first suspect.

### Two things found on the way

- **Upstream bug #48** (`findings/upstream-bugs.md`):
  `revolentype_length()` divides by zero at `revolen = 1` with
  revolentype QUICKENING/RANDQUICK (`server/plrhand.c:530`; the
  setting's own minimum is 1). Not reachable in any benchmark cell,
  reported as a courtesy; deep's own copy of the arithmetic guards it.
- **`knobcensus.py`'s map was already out of sync before this
  session**: `survival_pf_reach` and `op_draft_marginal` (both from
  the ERA rounds) are in `tuning-params.ini` and not in the map, so
  the census exits rather than running. This session's three knobs are
  mapped; those two are left for their owner, and named here so the
  next census run knows why it fails.
## W36 expected wait: the overseas search prices the queue (2026-08-12)

Branch `epic/w36-expected-wait`, based on `epic/w03-horizon-content` at
`9ccbf8426e`. Five commits; what ships is two knobs, both default-off,
plus one instrument.

- `99de8de163` `ai/deep: keep lift waiting spells live and estimate the tail`
- `8414577fb9` `ai/deep: discount overseas candidates by the wait their water is holding`
- `43a3f3400d` `ai/deep: drop overseas sites no hull of ours can land at`
- `6ac0ba54a4` `ai/deep: count sites taxed for cities our own operations are taking`
- `c2931f0fcc` `ai/deep: withdraw the beachhead reach filter, which cannot bind`

### The defect (expansion-logistics 3.1)

`deep_settler_map_iterate()`'s overseas leg ranked every candidate with
NO delivery term; the champion alone was amortized afterwards by
`ferry_wait`, the turns to the nearest free boat. A sea with a hull
idling on it and a sea whose queue has been holding settlers for twenty
turns ranked identically, and the 1:1-cap probe that publishes
`colony_want` priced no wait at all.

### What ships

**Live spells.** `deep_wait_census()` writes one line per waiting
passenger per turn and the spell is reconstructed OFFLINE, so no
decision could read it. `deep_wait_spells_update()` keeps the same
spell in memory (32 slots, not saved, unconditional so the estimate is
readable in BOTH arms), once per player per turn in
`deep_manage_units()` — before the settler pass that follows
`first_activities` in the same turn. A spell ends on an op's claim (the
offline definition), on no longer wanting a crossing, or on death.

**The estimate.** `deep_wait_expected(ocean)`: is an idle hull standing
free BEYOND the units already queued on that water
(`deep_idle_ferries_on` > `deep_ferry_demand_on`, the pair the colony
want is damped by, asked of the newcomer rather than the shipyard)? If
so the wait is 0 and the champion's `ferry_wait` fetch term already
covers it. Otherwise the tail of that water's live spells at
`lift_wait_tail_pct`.

**Why the tail, and why 80.** `findings/commission-sizing.md` §3:
median wait 0, mean 2.28, p75/p90 = 3/7, and spells ≥ 5 turns are
19.8% of spells holding 61.4% of all waiting (isolated-150). The mean
describes nobody. 80 is the boundary of that measured fifth. The live
sample is right-censored and length-biased — a running spell is
over-represented in a snapshot of running spells — and that bias points
the same way as the quantity being estimated.

**Consumption.** Every overseas candidate (`boat_cost > 0`) is
amortized by `site_wait_discount * wait`, the same discounter and the
same currency as the travel term: a wait is a delay like a walk is a
delay. The shortest adjacent queue is charged, not the longest — any of
those waters can carry the hull that lands the passenger. Because the
term sits inside the candidate loop, the probe path gets it for free;
both paths go through that function. Memoised per water per search
(`deep_wait_expected()` walks the unit list twice per water).

### Instrument only: §3.4, the tax on ground we are taking

`deep_cityresult_fill()`'s rival-pressure clause counts every
non-allied city within `4·radius_sq`, including one an assault of ours
is three turns from capturing. `deep_op_targeted()` is now declared in
`deepops.h`; the clause's sum is split and `deep-sitetarget` reports
candidates carrying it and the want it costs at today's weights. No
want changes, no knob; the split is computed only when the metric log
level is on. The correction waits on the number.

### The reach filter was built, measured, and withdrawn

expansion-logistics §3.1 clause (i) asks the search to discount a
beachhead the hull cannot path to. Built (`43a3f3400d`), measured, and
withdrawn (`c2931f0fcc`) in the same session, because **the search
cannot ask that question of itself**:

- MEASURED, isolated-150, 2 seeds, knob on: 37 searches raised the
  counter and 24 of them refused every candidate and returned NO site
  (2888 candidates refused). 36 of 37 were `unit=0` — the build side's
  founder-want probe, which searches with a VIRTUAL boat precisely
  because it is deciding whether to build one.
- STRUCTURAL: the overseas leg searches an overlap pf map rooted at the
  boat it was handed. A land tile is enterable there only from a tile
  the hull can occupy (`overlap_move` returns `move_rate` into a
  non-native tile; the iterate refuses to expand out of a `PF_MS_NONE`
  scope), so every candidate the search VISITS already has a reachable
  meeting tile beside it. Asking the same hull is vacuous; asking a
  different hull — which is what the filter did — answers a question
  nobody asked.

**So §3.1's clause (i) does not hold as written**, and the lift's 21%
of crossings that never close distance (59/275) is not the site search
failing to test reachability. The divergence must be between the boat
the search assumed and the boat the lift forms with, or a beachhead
that stops being reachable in between. That needs its own instrument
and is not this epic's.

### Validation

- `meson test -C build` 5/5; `tune-audit.py` CLEAN at **214 fields**
  (from 212; `lift_wait_tail_pct` and `site_wait_discount` registered in
  `deeptuning.h`, a `TUNE_*` macro, `tuning-params.ini`, and the ALL +
  X + ECON blocks).
- **Off-switch proof: 10/10 smoke scorelogs byte-identical** at both
  knobs 0, against a `git archive` build of base `9ccbf8426e`
  (era-probe stage-1 pattern, `identity.ini` both sides, ports
  7250/7265, jobs 2, results under `benchmark/results/w36-identity/`).
  Re-run against the shipped tip `c2931f0fcc` after the withdrawal:
  **10/10 again**. The spell table and the §3.4 split are therefore
  proven inert, which matters because both run unconditionally.
- **Liveness, isolated-150, 2 seeds, jobs 2, `site_wait_discount = 1.0`**
  (`benchmark/tuning/w36-wait.ini`, results under
  `benchmark/results/w36-live/`). Every counter fires:

      deep-waitspell   75 player-turns with a live spell, mean 1.41
                       spells, longest live spell 7 turns
      deep-sitewait    13 searches priced a candidate; 568 candidates
                       discounted, 1089 wait-turns charged,
                       mean 1.92 turns per candidate
      deep-sitetarget  4 searches, 1 site each, 1.1 want per site

  The mean 1.92 turns sits where `commission-sizing.md` §3 says it
  should (median 0, mean 2.28) — the term is small in the middle by
  construction, and it is the tail it exists for.
- **NO A/B.** The isolated-150 20-seed A/B is scheduled separately.
  Nothing here is a claim that the discount pays.

### For whoever runs the A/B

- **The knob to move is `site_wait_discount`**, one arm, on
  isolated-150. `lift_wait_tail_pct` is a shape parameter and should
  stay at 80 for that arm — moving both is a factorial nobody has
  designed.
- **Watch `colony_want` for a demand spiral.** The probe path takes
  the same discount by construction (§3.1 asks for it), and
  `colony_want` is the signal that ASKS for the hull whose absence
  creates the wait. At the measured magnitudes (mean 1.92 turns, p90
  of completed spells 7) the discount is a few percent of a site's
  score and a spiral is unlikely — but `deep-colony` and
  `deep-sitewait` are in the same log and the check is one grep.
- **`deep-sitetarget` is nearly empty on isolated-150** (4 searches, 2
  games): islands produce few assaults, so the §3.4 population is
  thin there by construction. Read that counter on a war venue
  (tech50war-200 or contested) before concluding anything about the
  size of the tax.

### Corrections to the plan, recorded

1. **expansion-logistics §3.1 clause (i) is wrong as written** — see
   the withdrawal above. Reachability of the beachhead is already
   enforced by the overlap pf map the search runs on.
2. **The "57.4% censored" figure means "never enrolled", not "waited
   forever".** `commission-sizing.md` §3: only 36 of 592 island spells
   were still waiting at game end; the rest stop wanting a boat. The
   population worth pricing is the 19.8% of spells ≥ 5 turns that hold
   61.4% of all waiting, which is what the tail estimator targets.
3. **§3.2 (the naval bonus gate) was already built, measured, and
   defaulted OFF** before this epic (`4a0d05dd74`, then `f657986d1a`),
   with the reason recorded above: `naval_bonus()` is a coastal YIELD
   term, not an option on shipping, so gating it on navigability
   prices out real food. Nothing to do; the rider is closed.

## The 3a×3c factorial: honest reach is the first confirmed tech-50 fix (2026-08-12)

Four cells, 16 mirrored seeds each, tech50war-200, one pinned
post-merge binary (cells c00/c10 reproduced the era-3a arms exactly —
the day's eleven merged epics are byte-inert at defaults).

    cell means   draft off   draft on
    reach off      -293.4     -241.1
    reach on       -236.2     -154.4

    reach main   +71.9  se 28  t +2.58  13/16 seeds   <- CONFIRMED
    draft main   +67.1  se 59  t +1.13   7/16
    interaction  +14.8  se 23  t +0.65   8/16
    c11 vs c00  +138.9  se 77  t +1.81   9/16

Verdicts: survival_pf_reach is the first statistically solid
mechanism win against the collapse and is fold-ready pending an
econ-venue guard (its effect on peaceful venues is presumed ~0 but
unmeasured). op_draft_marginal's revival under honest reach is
directionally supported (+81.8, t +1.60, vs +52 without) but the
interaction term is noise; the draft stays 0 and MUSTER (3b,
S-ERA-3b-muster.md) builds against the c11 baseline. Even both-on
loses -154/seed: the remaining gap belongs to 3b, the sortie lean,
MOVETYPE's honest naval pricing, and the finale's fear recalibration.
---

## MOVETYPE — the dead accessor (2026-08-12)

Branch `epic/movetype`, cut from `27cee96378`. **UNMERGED, PENDING
MEASUREMENT** — see "Merge gate" below before doing anything with it.

- [x] M.1 correct the mis-scoped move-type comment (1 commit, `8797b6016d`)
- [x] M.2 ask terrain nativity for the move type (1 commit, `073b50341f`)
      — 13 call sites, unconditional
- [x] M.3 count how far the wave's sea arm reaches (1 commit, `c969e5c347`)
      — `sea_far` + `sea_max_dist` in the deep-wave census
- [x] M.4 record the classification and the measurement (doc + config)

### What was wrong

`utype_move_type()` is `UMT_LAND` for **every unit class on the
server**: `set_unit_move_type()` is called only from
`client/packhand.c:3688`, so `unit_class::move_type` keeps its
zero-initialised value forever. Thirteen `ai/deep` call sites asked it
a question and received a constant — eight sea branches that had never
executed, one convoy loop that rejected every hull (so
`deep_manage_convoys()` was dead entirely), and four land guards that
had never rejected a warship. **None was benign.** Full site table:
`findings/venues-2026-08-12.md` §5.3.

### No knob, deliberately

The first cut put all thirteen behind `movetype_honest`, defaulting to
the status quo; that was overruled by the human and the reasoning is
recorded in §5.1. A knob selects between two behaviours, and here one
side is not a behaviour — it is an uninitialised field. Precedent: the
credit-integrity fix earlier the same week. `tune-audit.py` therefore
stays **CLEAN at 219**, unchanged.

### Validation

- Build `-Daimodules=classic,tex,baseline,deep` clean (pre-existing
  warnings only); `meson test -C build` **5/5 OK**;
  `tune-audit.py` **CLEAN 219/219/219/219**, before and after.
- **Scope proof, 10 paired smoke seeds, `--arms deep,baseline`, jobs 2,
  ports 7700/7740, 40 games, 0 failures, 0 pairing violations**
  (`benchmark/results/movetype-identity/`), this binary against a
  `git archive` build of `27cee96378`: the **`baseline` arm is 10/10
  byte-identical in score**, so the change is inside `ai/deep` and
  provably nowhere else. The `deep` arm differs on 5/10, aggregate
  +0.6% — directional only, not a result.
- **Liveness, `tech65sea-150`, 2 seeds, `--arms deep`, jobs 2**
  (`benchmark/results/movetype-liveness/`), same maps both binaries:

      deep-wave evaluations   4 676  ->  49 215
      by_sea admissions           0  ->  309 594   (98.2% of all arms)
      mean n_att               2.98  ->  6.40
      mean surv               0.726  ->  0.398
      deep-convoy lines           0  ->  1
      wall clock, 2 games   124/171s ->  145/191s

  `by_sea` was the counter this defect was found through, and it is
  the acceptance bar `tech65sea-150` was declared unable to meet. It
  is met.
- **NO A/B.** Two seeds is not a result (rule 6). Score moved +13.8%
  and +9.5% on those two seeds; that is recorded only as evidence that
  a large regression would have been visible and was not.

### Merge gate — READ THIS FIRST

**The branch merges only on the overnight A/B**, this binary against
the `27cee96378` archive build: `tech65sea-150`, `tech50war-200`, and
a `contested` guard. The base build is left in the worktree at

    .movetype-base/build/freeciv-server     (git archive of 27cee96378)

so the comparison needs no rebuild. Nothing about this branch may be
cited until that verdict exists.

### What the A/B must be read against

- **Fear-calibration debt.** Every fear constant in `struct tuning` was
  tuned against a wave that could not see a warship and a menace term
  that always applied `menace_overseas_weight`. They are now measured
  against a different quantity. **That re-tune belongs to the MIL
  finale**, not to this branch, and a flat or negative A/B on a naval
  cell is at least as likely to be stale constants as a bad fix.
- **The sea arm's distance bound is now the map.** `sea_max_dist`
  pegged at 18 = `survival_horizon` 3 × a destroyer's 6, on a 32×32
  torus 16 tiles across; `sea_far` was 288 699 of 309 594 admissions
  (**93%**). The deferred-not-dead worry at the sea clause is live and
  large. No bound was invented (rule 7) — the counters were added so
  the population a bound would remove is a measured number first.

### Downstream consumers, both to be re-read after the A/B

1. **The `tech50war-200` NETTHREAT item-1 reading** was taken against a
   warship-blind wave.
2. **The sea-clause verdict** was deferred because the clause was dead.
   It is not dead now, and `sea_far` is what the verdict should be
   taken against.

### One more thing the next session should know

`benchmark/configs/tech65sea-150.conf`'s provenance comment said
`by_sea` "IS STILL 0 HERE, AND ALWAYS WILL BE". That is corrected in
place; every naval number in its calibration table is flagged as a
**pre-fix** reading. The table is kept because what it argues for is
the cell's geometry, which the fix does not touch. The edit changes
the config's sha256 and therefore its result-cache key — which costs
nothing, since a rebuilt binary already misses that cache.

## MOVETYPE merged on its A/B (2026-08-12 evening)

Merge gate read out: tech50war fix −250.6 vs base −250.2 (neutral,
naval pricing doesn't bind there); contested guard +36.9 vs +43.1
known (−6, noise); naval tech65sea fix−base −34.3/seed paired,
t −1.25, ns — read as FEAR-CALIBRATION DEBT (survival 0.73→0.40 under
honest sea threat, constants tuned warship-blind), owned by the MIL
finale's naval-fear family sweep on tech65sea-150. Human call: merge.
pv2 on tech50war measured −250 vs the pv1 family's −293 on the same
seeds — ECON tuning travels to the war venue (~+43, pairing pending
G5). by_sea live: 0 → 309,594.

## NAVSEE + hut-flip: two corrections landed inert, three venue findings that redirect their A/Bs (2026-08-12 night)

Branch `epic/navsee-hutflip`, based at `ccf9e10e55`. Six commits.
Machine budget was smoke/identity/liveness only — a night batch owned
the box — so **neither mechanism ships on and no A/B was run**. What
this session bought is two corrections behind default-preserving
knobs, the instruments that size them, and three measured facts that
change where their A/Bs should be spent.

### 0. S-NAVSEE part A is already built — by MOVETYPE, this afternoon

The brief's headline job was to generalize the terrain-nativity test
to four blind sites: `deep_boat_shadow_fear` (F2), `deep_density_fear`'s
floats gate (F3), `deep_reachable_free_land`, and the ferry census.
**All four were converted six hours earlier** by `073b50341f` on
`epic/movetype` (deepdefence.c:447, 794, 610, 5398 respectively), as
part of the thirteen-site sweep, and merged at `7ad93151b8`. The brief
predates that work and its evidence is stale in deep's favour: it
asked for one shared helper and there is one
(`deep_utype_is_sea`/`_is_land`), it asked for the class-fact riders
and they are folded in, and it asked for an A/B which MOVETYPE ran and
the human merged on.

The one acceptance item MOVETYPE did not deliver is the nativity
helper's **unit test in the deepodds pattern**. That pattern compiles a
FREESTANDING module against no freeciv tree (`tests/deepodds_test.c`
header comment); `deep_utype_is_sea` lives in deepdefence.c and reads
`struct unit_type` and terrain nativity, so testing it that way means
extracting it first. **Not done, and flagged rather than faked** —
MOVETYPE's runtime evidence (by_sea 0 → 309,594 admissions on
tech65sea-150) is stronger than the unit test would have been.

So this session built part B and the hut-flip.

### 1. The hut-risk sign, corrected behind `hut_risk_honest` (default 0)

M3's item 2 established from the server source that the discount runs
backwards. `_deflua_hut_get_barbarians` (`data/default/default.lua:112`)
and the handicap path `hut_get_limited` (`server/unittools.c:3321`)
both refuse to unleash when ANY player's city lies within
CITY_MAP_MAX_RADIUS, so the huts that CAN unleash are the far ones,
and `deepexplore.c` discounts the near ones.

`hut_risk_honest = 1` charges `hut_risk_discount` against the server's
own set, fixing all three mismatches at once (the sign; our cities vs
every player's; `real_map_distance` vs the squared-radius city map).
`hut_risk_radius` is then unread — the radius is the server's.

**Default 0.** Not doubt: the flip moves an explore-value term on
every hut map, and v3b's tuner has been zeroing `explore_hut_value` on
classic *in response to this term's wrong sign*, so sign and magnitude
are one factorial question.

### 2. The maritime premium, split behind `site_naval_connect_weight` (0.0)

Human design 2026-08-05. `site_naval_weight` (20.0, unchanged) is now
documented and implemented as coastal YIELD — any workable water, dead
lake included. `site_naval_connect_weight` is a NEW additive term paid
only where `deep_city_water_bridges()` says the water leaves the
continent. Both read the SAME `cr->total` before either is added.

The evidence for splitting is D3's own negative result: `site_naval_gate`
tested the VETO form of this distinction and measured **−11.68 (se 8.02,
t −1.46, 7/20)** on isolated-150, because a veto withholds yield from
water that genuinely yields. Additive is the shape that can price "a
useful lake beats a dead lake" without pricing "a dead lake is worth
nothing".

`benchmark/tuning/navsee-split.ini` carries the human's 17 + 3 cut as
the A/B arm, zero-sum at a connected site by construction, with the
value's provenance stated and labelled PLACEHOLDER.

### 3. Instrument first: `deep-explore` and `deep-sitenaval` grew four fields

`huts_disc` (what the term in force charged), `huts_unleash` (what the
SERVER would let unleash, from its own predicate, counted whether or
not any gate reads it), `barbs_armed` (the rate+onset gate), and on
the site line `connect` / `connectwant`. `findings/knob-coverage.md`
had named the first of these exactly — "Needs: huts_discounted= on
deep-explore" — and both `hut_risk_*` knobs move from NO-INSTRUMENT to
direct in the census MAP.

### 4. THE VENUE FINDINGS. All three redirect an A/B the brief specified

**(a) No existing cell can exercise the hut-risk term.** Its hut-DENSE
cells and its barbarian-ARMED cells are disjoint sets:

    cell         huts         armed?                 huts_vis observed
    huts         150          NO (civ2civ3 sets      -- T80 < 120
                                 onsetbarbs 120)
    smoke        ~15 default  NO (T100 < 120)        max 1; armed=0 on
                                                     all 400 lines
    storm-150    ~24 default  YES (onsetbarbs 30)    max 1; 968 armed
                                                     lines, 7 with any
                                                     hut, 0 with both
    tech65sea    ~15 default  YES                    2 lines with a hut

That is why both knobs sat NO-INSTRUMENT: there was nothing to
instrument. `benchmark/configs/hutrisk-80.conf` fixes it — huts.conf's
geometry and density with storm-150's onset, every value copied from
an existing canonical cell.

**(b) On that venue the shipped discount is spent ENTIRELY on
risk-free huts.** One paired game, seed 1, T80, both arms:

                              honest=1     honest=0 (shipped)
    deep-explore lines             338            333
    armed lines                    222            217
    lines with a hut in view       105            100
    lines with huts_disc > 0         0             33
    lines with huts_unleash > 0     51             51
    armed AND huts_unleash > 0       0              0

Every hut deep sees during the armed window stands inside some city's
max city map. The shipped form discounts on 33 lines; the corrected
form discounts on none, because there is nothing dangerous to discount.
The arms diverge (338 vs 333 lines), so the knob is live. **One game
is liveness, not a result** — but it says the flip's realised effect
is "stop paying a discount that never bought anything", which is a
smaller and cheaper claim than the A/B was being sized for.

**(c) The split's A/B must NOT be run on isolated-150.** The connect
term's discriminating population is `paid − connect` — coastal
candidates whose water goes nowhere — and on an island map there
essentially are none:

    venue (seed 1, split probe)   paid     connect   discriminating
    tech65sea-150 (ISLAND)      16 725      16 722         3  (0.02%)
    smoke (continental)          2 766       2 700        66  (2.4%)

The brief names isolated-150 because that is where the VETO was
measured. But on island geometry every coastal site is on bridging
water, so the split is inert there by construction and n=40 would buy
a zero. The discrimination lives on continental maps with lakes and
closed seas. **Measure `paid − connect` on a candidate cell before
spending seeds on it.**

### 5. Validation

- Build `-Daimodules=classic,tex,baseline,deep`: clean.
- `meson test -C build`: **5/5 OK** (baseline-frozen, deepodds,
  deepattrition, deepeffects, deeptuning).
- `tune-audit.py`: **CLEAN 219 → 221**, both new knobs in all four
  audit places and in ALL plus their domain blocks (M/MIL for
  `hut_risk_honest`, X/ECON for `site_naval_connect_weight`).
- `knobcensus.py` MAP: four entries changed, sync check passes,
  predicates in the parseable `field>=N` form.
- **IDENTITY: 10/10.** smoke, 10 paired train seeds, arm `deep`,
  tuning=none, this tree's binary vs a `git archive ccf9e10e55` build
  (`benchmark/results/navsee-identity/`, keys
  `smoke-1a3940ba642d` = before, `smoke-1c0c0f710012` = after):
  **scorelogs byte-identical 10/10, maphash equal 10/10, metrics.csv
  identical.** Both mechanisms are off at their compiled defaults and
  the new counters cost nothing observable.
- LIVENESS: 4 games. tech65sea-150 with the split probe — connect
  fired on 587/587 searches, 16 722 candidates, 194 627 summed
  connectwant; by_sea 89 079 (part A live in the same game).
  hutrisk-80 ON/OFF as in §4b. storm-150 as the negative control.

### 6. Owed

- The two A/Bs, on the venues §4 identifies rather than the ones the
  brief named: `hut_risk_honest` on hutrisk-80 (and the
  `explore_hut_value` factorial), `site_naval_connect_weight` on a
  continental cell chosen by its `paid − connect` gap.
- `site_naval_weight` 20.0 and `site_naval_connect_weight` 0.0 are
  PLACEHOLDERs for the pile; the 17 + 3 cut lives only in the probe.
- The nativity helper's unit test (§0), which needs the helper made
  freestanding first.
## M2 naval invasion slice 3: the scope had already landed (2026-08-12, night)

Branch `epic/m2-invasion`, based at `ccf9e10e55`. The session opened to
build "invasion wants + defence-at-arrival go/no-go". **Both halves were
already in the tree, and had been for twelve days.** This is the stale-plan
stop condition (CLAUDE.md), taken as a stop rather than as a rewrite.

### What the charter said, and where it went

`findings/civ2civ3-adjustments.md` "Naval convoy design" (2026-07-30) names
three slices: (1) full-load gate, (2) escort pairing, (3) **massed
landings — hold the convoy until the assembled force clears a bar against
the target's DEFENCE AT ARRIVAL, current garrison plus eta x a
reinforcement rate**. STATUS's mechanism-epoch queue added the build side:
"invasion wants + defence-at-arrival go/no-go".

- Slices 1-2 landed **2026-07-30** (convoy hold + aiguard escort pairing).
- Slice 3 landed **2026-07-31**: `a255144c34` "target across water and
  price arrival on both terrains" is the invasion want, `f4eb702e65` /
  `5a7c70988a` / `d67ae21055` the arrival pricing, the convoy hold and the
  ledger tripwires.
- Task **#16 CLOSED 2026-08-01** — STATUS "#16 closed: the invade gate at
  the right seam", 11 go / 57 hold on lategame where every prior run had
  zero.

So the assignment's three named deliverables all exist:

| Asked for | In tree since | Where |
|---|---|---|
| invasion wants (target worth, amortized) | 2026-07-31 | `deep_assault_want()` overseas branch, `deepdefence.c` |
| defence-at-arrival go/no-go | 2026-07-31 | `deep_take_odds_at_arrival()` + the MARCH crossing re-gate, `deepops.c` |
| price the SET, not the margin | 2026-07-31 | the campaign-pricing block: smallest force clearing `op_take_target`, each member valued at `(pk-p0)/made` |
| CONSEQ N3 never-launches damper | later | `attacker_hold_damper` |

**Nothing was re-implemented.** No knob was added, because no mechanism
was; `tune-audit.py` stays CLEAN at **219/219/219/219**.

### What did land: the invasion want had no counter

The one real gap was evidential. `deep-kill` carried `target=`, `pool=`,
`p0/p1`, `want=` — and nothing saying the target was across water or how
long the defender had to prepare. So "does deep ever want to INVADE" was
unanswerable from any archived corpus, which matters because two live
campaign documents rest on the answer:

- `findings/knob-coverage.md` excluded the convoy family as
  "inert pending invasion wants" — a receipt that was already four days
  stale when written, and whose stated mechanism (the 1:1 ferry cap) was
  not the binding one: `deep_manage_convoys()` was dead **entirely**,
  because its hull loop asked `utype_move_type()`. Corrected in place.
- `CAMPAIGN-v4.md` open question 2 asked the human "is M2 in or out of the
  gates?". Marked **MOOT**; the surviving question is narrower and is now
  a measurement, not a call.

Two commits of mechanism-free instrument, one of doc:
- `bad46de04b ai/deep: log whether an assault want is an invasion`
  (`sea=`, `eta=` on `deep-kill`; the two duplicate inner `overseas`
  copies collapse into one function-scope fact, candidate predicate
  renamed `cand_overseas`).
- `9d7a911ea6 benchmark: probe the reinforcement rate where the build side
  applies it` (`invasion_reinforce_rate` gains `deep-kill eta>=1`
  alongside `deep-invade`; the old probe only fires when a fleet is
  literally mid-sail, so the knob read DEAD while its arithmetic ran
  thousands of times a game).

### Validation

- Build `-Daimodules=classic,tex,baseline,deep` clean (pre-existing
  warnings only); `meson test -C build` **5/5 OK**; `tune-audit.py`
  **CLEAN 219/219/219/219**, before and after.
- **Identity, 10 paired smoke seeds, `--arms deep,baseline`, jobs 2,
  ports 7100/7140, 40 games, 0 failures, 0 pairing violations**
  (`benchmark/results/m2-identity/`), this binary against a `git archive`
  build of `ccf9e10e55`: `scorelog.txt` **byte-identical on 20/20 game
  pairs — 10/10 deep, 10/10 baseline**. A log field cannot move a game
  and did not.
- **Liveness, `tech65sea-150`, 2 seeds, `--arms deep`, jobs 2, port 7180,
  compiled defaults** (`benchmark/results/m2-liveness/`):

      deep-kill (assault want)      8 253 lines
        sea=1 (INVASION want)       8 011   (97.1%)
        sea=0 (land want)             242
        want>0 among sea=1          1 879   (23.5%)
      eta on sea=1 lines            3:3193 4:3946 5:741 6:112 7:13 8:6
      deep-invade go                   29
      deep-invade hold                 25
      deep-op retarget                 46
      deep-convoy                       1

  On the naval war cell **the assault want is essentially all invasion
  want**, the go/no-go shows **both verdicts** (29/25 — a coin-flip gate,
  not a rubber stamp), and 76% of invasion wants price at zero: the odds
  machinery refuses most crossings, which is the intended shape.

### What this buys the campaign

`invasion_reinforce_rate` reads **LIVE, population 8 307** on
`tech65sea-150` through the new probe. `convoy_min_load` reads **LIVE,
population 1** — so the family is no longer provably dead, but one firing
in two games is not a searchable dimension and **EXCLUDE stands, now on a
measured population instead of a false mechanism claim**. Re-read at
freeze on a larger naval sample.

### Open, and explicitly NOT done here

- **No A/B.** Two seeds is not a result (rule 6), and there is nothing to
  A/B: the binary is behaviourally identical to its base by proof.
- **`invasion_reinforce_rate` is still a PLACEHOLDER 0.34 flat rate.** The
  charter said "a tunable slope on the defender's public production /
  military signals"; what landed is a constant times eta. Replacing it
  with an observed rate is a real, unstarted mechanism — and it is a
  *different* epic, not this one.
- The `+2` embark/disembark turns in the overseas eta
  (`deepdefence.c`) is an unregistered constant. Flagged, not touched.
- `benchmark/cities.py` matches a `sail-anyway` invade verdict that no
  code emits (harmless; the counter is a Counter). Cosmetic.
- The next honest naval question is **not** more invasion machinery. It is
  the fear-calibration debt MOVETYPE's merge note already owns: survival
  0.73 -> 0.40 under honest sea threat, constants tuned warship-blind.

## Night-batch guard verdicts (2026-08-12 late)

- G1 reach econ guard: pv2+survival_pf_reach on lategame-80 holdout
  -17.5 vs pv2 -10.1 — paired -7.4, t -0.59, NULL. Reach is free on
  econ venues; pv3 fold awaits only G4/G5.
- G2 W36 wait discount: pv2+site_wait_discount=1.0 on isolated-150
  holdout +0.5 vs pv2 +55.0 — paired **-54.6, t -4.00, 16/20 worse.
  DECISIVELY HARMFUL at the placeholder weight.** The knob stays 0.
  A ~2-turn/candidate discount cannot honestly cost 55 points at
  weight 1.0: suspect DIMENSIONAL mismatch (the CREDIT lesson), not a
  scalar to sweep. Inspect the term's currency at the site-want seam
  before any tuning; the probe path (colony_want) taking the same
  discount is the first suspect.
  **WITHDRAWN 2026-08-12 (W36-DIM, end of file): this comparison's two
  arms ran different binaries — the control's games finished the
  previous night, ≥26 ai/deep commits earlier. Three of its 20 seeds
  never applied the discount once and still "lost" 13–33 points, and
  on two of them tonight's knob-0 game is byte-identical to the G2
  arm's knob-1.0 game. The −54.6 is not a measurement of the knob.
  G1 and G3 below were run the same way.**
---

## EXPANSION slice 1: the founding decision is instrumented, and the slice's own premise is falsified (2026-08-12)

Branch `epic/expansion-slice1`, cut from `epic/w03-horizon-content` at
`ccf9e10e55`. Seven commits: five counters, one default-off knob, this
entry. **No A/B** — a night batch owned the machine, the budget was
jobs 2 and ports 7250+, so everything here is a mechanism read.

- `fe74035d61` `ai/deep: census why a site search comes home empty`
- `65888b0067` `ai/deep: report the founding decision each city-turn reaches`
- `cb7173c399` `ai/deep: price what ai/default takes off a founder want before the bid`
- `983f9fee79` `ai/deep: split the search's largest refusal into its four causes`
- `a9b71efc30` `ai/deep: record whether the food a founder bid needs is deep's to arrange`
- `6dd2c889ff` `ai/deep: let a city that wants to found ask for the food the bid needs`

### Why this slice, out of the epoch's menu

The menu was settling order, planned lifts, corridors, colony net
value, city roles. **Colony net value + the demand side** was taken,
on one line of evidence: `findings/era-probe-evidence-2026-08-12.md`
§2 had already closed the expansion gate as the cause of the founding
flatline (it fires on 6.25% of city-turns and is worth +149/seed as it
stands) and named the replacement — `founder_want > 0` in only **5.0%
of *unblocked*** city-turns — and then said *"That decision is not
instrumented."*

Every other item on the menu is a structural build with nothing to aim
it. This one had a measured hole with a named function at the bottom
of it, and the instrument cost five short commits in code we own. That
ratio was the whole argument.

### What the counters said — and the first answer is NO

**`neg = 0`. In all 2138 site searches, across both seeds.**

Not one scored candidate ever went non-positive after the travel
discount, the expected-wait discount, the settler cost and the boat
cost came off it. The valuation refuses nothing. **Whatever stops deep
founding cities, it is not that colonies are priced above what they are
worth** — so net colony value, the thing this slice was commissioned to
build, is aimed at a mechanism that never fires. Withdrawn.

That is the case for instrument-before-fix, and it cost two games.

**The land search dies at t20, and it dies on citymindist.**
`deep-siteladder`, means per search, seed 1:

| phase | leg | seen | nofound | of which citymindist vs a real city | scored | found% |
|---|---|---:|---:|---:|---:|---:|
| t<20 | land | 71.5 | 37.7 | 30.1 (80%) | 8.9 | 53.0 |
| t20–49 | land | 69.1 | 63.0 | **56.2 (89%)** | **0.0** | **0.6** |
| t50+ | land | 77.4 | 50.7 | **45.0 (89%)** | **0.0** | **1.4** |
| t20–49 | sea | 1077.4 | 968.9 | 67.5 (7%) | 31.2 | 83.2 |

Seed 2 reproduces it: 0.3 and 0.1 scored on the two late land buckets,
6.9% and 3.1% found. **From t20 onward the home continent produces no
legal site at all**, and 89% of the refusals are the citymindist ring
around a real city. Not terrain (6–7 tiles a search), not fog
(`nomap` = 0 — H_MAP is unset, RC6), not legality (`nolegal` ≈ 1).

On the overseas leg the same bucket is 93% **water** (`noterr`) — the
overlap pf map walking the ocean, which means nothing. Splitting
`nofound` earned its own commit for exactly that reason: before the
split the two legs' largest number looked like one finding, and they
are opposite findings.

**So after t20 every founding want deep has is an OVERSEAS want**, the
sea leg finds a site 83% of the time worth `best_net` 280–491, and the
whole question becomes the colony/lift pipeline. That is DERBY-F1's
colony-demand hallucination seen from the demand side, and it is
structural, not a valuation error.

**The food veto, which nobody had ever counted.**
`domestic_advisor_choose_build()` (daidomestic.c:561) makes **no founder
bid at all** — not a reduced one — unless the city's food surplus covers
the founder's food upkeep. `deep-founderbid`, 6555 city-turns:

    nonzero founder want        1430 (seed 1)      1321 (seed 2)
    of those, food-vetoed        451  31.5%         314  23.8%
    at surplus 0 vs upkeep 1     83% of the vetoed
    with NO deep cm_parameter    418  92.7%         298  94.9%

One food stands between a want of several hundred and no settler, and
at ~93% of those city-turns **deep's own posture machine had returned
`DEEP_CM_NO_POSTURE` and handed the city to the stock default** — deep
wanted the settler and nothing in deep asked for the food it cost.

Two hypotheses the counters also closed: `capped` (the
`adv->max_num_cities` /100 divisor) fired **0 times** in 6555
city-turns (`max_num_cities` is 1600 here), and `role=0` — the silent
exit that leaves a stale want standing — fired **0 times**.

### What shipped as a fix

`cm_founder_food` (double, **default 0.0 = off**, the 220th tunable)
adds a last arm to `deep_set_city_manager_params()`'s posture machine,
**over `DEEP_CM_NO_POSTURE` only**: a city carrying a founder want it
cannot fund takes the grow posture and gets `minimal_surplus[O_FOOD]`
floored at `cm_founder_food ×` the founder's upkeep. It reuses the grow
factors rather than inventing any, and firing only where deep had
declined to govern bounds it to the measured population and leaves
every live posture untouched.

`pcity->cm_parameter` is precisely the abandoned mechanism
`00-context` §3 lists ("honoured by `auto_arrange_workers`, never set
by `ai/default` — `ai/deep` sets it"), and `deepcm.c` already owns the
write. This is a coupling, not a new subsystem.

**1.0 is a PLACEHOLDER** flagged for `benchmark/tune.py`: it is the
identity scaling of the upkeep the veto itself tests, which is the only
non-arbitrary value available, and it has never been tuned.

### Validation

- `meson test -C build` **5/5** after every commit; `tune-audit.py`
  **CLEAN at 220 fields** (219 + `cm_founder_food`, registered in
  `deeptuning.h`, the `TUNE_DOUBLE` list, `tuning-params.ini` and the
  ALL + G + ECON blocks); `knobcensus.py` MAP entry added, grade
  **direct** on `deep-cmfound floor>=1`.
- Built `-Daimodules=classic,tex,baseline,deep` in this epic's own
  worktree with its own `build/`, its own `--server` and ports 7250+ —
  the pinning rule, with a night batch on the box.
- **Liveness, 2 × lategame-80, jobs 2, counters at LOG_NORMAL**
  (`benchmark/results/expansion-live/`, run twice — once before the
  `nofound` split and the `cm` field, once after). Every counter fires:
  2138 `deep-siteladder`, 945 `deep-founder`, 6555 `deep-founderbid`.
  The ladder's buckets partition `seen` on every line by construction.
  The two runs' ladder aggregates are **identical to the digit**,
  incidental evidence that the split and the `cm` field are inert.
- **Knob liveness, 2 × lategame-80, `cm_founder_food = 1.0`**
  (`benchmark/tuning/expansion-cmfound.ini`,
  `benchmark/results/expansion-knob/`): `deep-cmfound` fires, first at
  T29 on a size-4 city with `want=1052 food=-1 upkeep=1 floor=1` —
  exactly the population the veto census described. 69 and 76 firings
  over 14 distinct cities per game, T29–T80, mean |want| 671. (The two
  arms' games diverge from T29, so **no rate comparison against the
  off arm is a result** — this shows the arm executing, nothing more.)
- **Off-switch proof: 10/10 paired smoke seeds BYTE-IDENTICAL**, 0
  differ, 0 missing, against a `git archive` build of base
  `ccf9e10e55`, `identity.ini` both sides, jobs 2, ports 7280/7300,
  `benchmark/results/expansion-identity/`. All five counters and the
  knob's dead branch are therefore proven inert, which matters because
  the ladder's increments run unconditionally.
- **NO A/B, and nothing here claims the fix pays.** Two liveness seeds
  price nothing.

Runner: `benchmark/expansion-slice1.sh {live|knob|identity}`.

### The epoch's menu, re-ranked by what the counters said

1. **Planned lifts (menu item 2) rises to first.** The overseas channel
   is now measured as the *only* channel after t20 — 0 legal land sites
   in both seeds, 83% overseas hit rate, `best_net` 280–491. Every
   remaining expansion question on a settled venue is a delivery
   question, and planned lifts is the one item that acts on the channel
   the map leaves open.
2. **The founder food veto** — the fix above, needing its A/B. Small,
   isolated, and the only demand-side item the counters indicted.
   Arm: `cm_founder_food = 1.0`, lategame-80, ≥16 mirrored seeds.
3. **Settling order (menu item 1)** was ranked first by
   `findings/deep-dive-2026-08-04/expansion-logistics.md` §4 on RC2
   purity. It **falls**: with the continent legally full from t20,
   ordering sites we cannot found coordinates nothing. It becomes
   interesting again only where the land search still scores — read
   `deep-siteladder scored` on `isolated-150` and `primary-120` before
   spending an epic on it.
4. **Colony net value (menu item 4)** — **withdrawn on `neg = 0`.** Do
   not build it without first showing a venue where a scored candidate
   goes non-positive. That check is one grep.
5. **Corridors and city roles** — untouched, unmeasured, rank
   unchanged.

### For whoever picks this up

- **`deep-siteladder` is venue-diagnostic and cheap.** `scored` and
  `nomin` on the land leg answer "is there anywhere left to settle
  here" in one grep, and no venue but lategame-80 has been asked.
- **The 5.0% ERA figure and this venue's 22–32% are not in conflict.**
  `tech50war-200` is 8 players on 32×32, lategame-80 is 6 on 40×40.
  What reproduces across both is the shape: the want is absent far more
  often than it is beaten.
- **`deep-founderbid` carries the ai/default haircut for free.** `want`
  minus `bid` is exactly what daidomestic takes off the top, and
  `wonder=1` fires on ~10% of city-turns; nobody has priced the
  wonder-city divisor on a founder want.
- **`expansion-logistics` §3.1 clause (i) and §4's ranking both took
  damage this session** — clause (i) already withdrawn by W36, §4's
  first-ranked item demoted here. That document's §4 is a ranking by
  argument; it now has counters to be re-ranked against.
## GIFT-REFOLD: the love gift learns who it is talking to (2026-08-12 night)

Branch `epic/gift-refold`, based at `ccf9e10e55`. `ai/deep` +
`benchmark` + doc only; zero `ai/default`, `ai/baseline` or
`server/advisors` changes.

CAMPAIGN-v4 **§11.4** re-opened BALANCE's love-buying gift as an
ALLY-TARGETED mechanism. The gift is the largest single-lever self-play
move this project has measured (+8.28 contested, +8.92 lategame-80) and
the mixed head-to-head said it was a transfer, not an advantage: deep
+1.93 (t 0.72), counterparty **+5.26** (t 2.15), deep's margin over
`ai/default` **−3.34** (`findings/balance-statecraft-2026-08-06.md` §0).
§11.4's fold criterion is not "no surplus" — it is that the surplus
lands inside the alliance.

### What landed

| # | Commit | What |
|---|---|---|
| 1 | `fca2570185` | `deep_gift_rel_of()` — the relationship classifier, measurement only |
| 2 | `fe8da34be7` | the class price: three knobs on the willingness bound |
| 3 | `7c8c281c1d` | the shared-vision give-leg SEAM, comment only |
| 4 | `330ac18af4` | `benchmark/tuning/gift-refold.ini`, the arm for the queued mixed run |

### How the target is chosen

Every criterion is state deep already reads for another decision; no
new information enters the AI.

- **ALLY** — the pact itself, a **shared enemy** (someone we are both
  at war with), or a **common ally**. Note which population this
  actually reaches: an existing ally never arrives at `deep_love_gift`
  at all (the pre-pass skips allied pairs, and bought love cannot raise
  a bar the alliance is already past), so "inside the alliance" here
  means the alliance's **candidates** — the players who would fight
  beside us, financed while financing them still buys something.
- **INSTRUMENT** — no friendship, but we are **already at war
  elsewhere** and this player **presses our border** by the same
  density lattice `deep_top_menacer()` ranks menacers with. The second
  front deep cannot afford, priced rather than assumed, behind its own
  knob.
- **RIVAL** — everyone else, and explicitly anyone a deep **operation
  is marching on** (the same veto `deep_peace_wanted()` applies) or who
  is **allied to a player we fight** (the same counterparty the
  tech-leak veto refuses).

Ordered and exclusive: op-target and enemy-ally disqualify before
anything can promote.

### The knobs (all default 1.0 = today's bound for every class)

    gift_ally_gain         the alliance and its candidates
    gift_instrument_gain   a second front we are already paying for
    gift_rival_gain        everyone else -- the +5.26 population

A multiplier and not a gate, because the classes differ in degree as
well as kind: a would-be ally is worth paying MORE for than the flat
bound, and a bool cannot say that. Registered in the four audit places
plus `knobcensus.py`'s MAP with parseable predicates
(`deep-lovegift rel=ally` / `rel=instr` / `rel=rival`).

**`ALL` block only, deliberately.** These are cross-border by
construction and §11.4's finding is that an all-deep cell cannot tell a
positive-sum mechanism from a competitive one — so putting them in a
block the ECON search optimises on paired score would tune them on the
reading that cannot see what they do.

### The seam that was marked and not taken

`findings/omniscience-flag-breakage.md` item (b): giving vision is an
intel concession and "belongs in the gift/love currency". That currency
is this bound, and the seam is one line wide — `tech_worth` is the
whole of what a gift costs us today, so a give-leg is a second term
added to it before the comparison, behind its own knob and its own
class price. **Comment only, no behaviour**: vision is worth nothing to
an omniscient player, so pricing it now would price a good nobody
consumes. Honest-info epoch, with items (a), (c) and (d).

### Validation

- Build `-Daimodules=classic,tex,baseline,deep` clean (pre-existing
  warnings only, none in `deepdiplomacy.c`); `meson test -C build`
  **5/5 OK**; `tune-audit.py` **CLEAN 222/222/222/222** (from 219 —
  `gift_ally_gain`, `gift_instrument_gain`, `gift_rival_gain`).
- **Identity, 10/10 byte-identical scorelogs.** 10 paired smoke seeds,
  `--arms deep`, `identity.ini` on both binaries, jobs 2, ports 7350,
  this build (`f9cf1a665d50`) against a `git archive` build of
  `ccf9e10e55` (`f0f3cd213159`), under
  `benchmark/results/gift-identity/`:

      86fa9cf8e9ea  20e77144ddce  516442070fa0  0c0e8db3ccd8  d26859d9f559
      7feae491265f  1947015da4da  a7c49b4535fd  a1339da3f752  93ae385c00a1

- **Identity WITH THE GIFT LIVE, 2/2 byte-identical.** `identity.ini`
  proves nothing about this change on its own — at `gift_love_gain = 0`
  the pre-pass returns before any of it runs. So the same pair was
  taken on `contested` under **`pv2.ini`**, where the gift fires 72
  times a pair of games: `0ccd771be1fb`, `e773d3605428`, same on both
  binaries (`benchmark/results/gift-live/`). The class multipliers at
  1.0 are inert on a path that is actually executing.
- **A trap worth recording.** The first archive build was wrong and the
  first identity proof was worthless: `meson setup <builddir>` with no
  source argument takes the CALLER'S CWD, which on this harness is the
  worktree root — so the "base" binary was the branch under test and
  the proof compared the branch with itself. It was caught by a census
  line: the base arm's log carried `rel=`, a field that does not exist
  in `ccf9e10e55`. **Always pass the source dir explicitly, and always
  grep the base arm for a field only the branch can emit.**
- **Liveness, `contested`, 2 seeds per arm, `--arms deep`, jobs 2**
  (`benchmark/results/gift-live/`). Counters only — 2 seeds is not a
  result (rule 6), and no score is read from these games.

      pv2.ini (untargeted, the adopted baseline)
      rel      census   priced   sent  refused   gold given
      ally        772       12      7        0         1454
      instr       368       30     30        0         4238
      rival      1914       35     35        0         2020

      gift-refold.ini (gift_rival_gain = 0.0)
      rel      census   priced   sent  refused   gold given
      ally        941       18     18        0         2427
      instr       542       42     42        0         3232
      rival      2498      133      0      133            0

  The mechanism does what §11.4 asked: the rival leg goes to zero, 133
  gifts are refused BY THE CLASS PRICE and counted as such, and the
  surplus that remains is ally- and instrument-directed. The two arms
  diverge in play, so the columns are populations and not a paired
  comparison.
- `knobcensus.py` on the same corpus: all three knobs **LIVE**
  (`gift_ally_gain` 30 firings, `gift_instrument_gain` 72,
  `gift_rival_gain` 168).

### The queued arm — MANDATORY before any default changes

**Nothing here has been measured for effect, and no default moved.**
Under §11.4 this mechanism folds only on a **MIXED head-to-head arm**
showing deep's margin over `ai/default` is a wash or better. That arm
is **queued and not run** — the box was owned by another batch tonight,
and an all-deep paired-score cell is precisely the reading that cannot
see a transfer.

The arm to run:

    mixed_dc, seat-mirrored, contested + lategame-80
    A: benchmark/tuning/pv2.ini          (untargeted -- today)
    B: benchmark/tuning/gift-refold.ini  (ally-targeted)
    read: deep's margin over ai/default, per §11.4; the counterparty
          column is the one that moved -3.34 last time

**pv2 ships `gift_love_gain = 46.59`**, so the mechanism BALANCE
measured as a transfer is live in the adopted composed baseline right
now. That is the reason this arm matters beyond the knob: if the
refold reads as a wash or better, it is a candidate for pv2's own
defaults; if the fold criterion fails, the honest reading may be that
pv2 should carry `gift_love_gain = 0` instead.

Not attempted, deliberately: no A/B of any kind tonight, no default
change, and no vision pricing (item (b)'s seam is a comment).

- G3 gov guard: pv2+gov_valuation=1 on lategame-80 holdout -36.5 vs
  pv2 -10.1 — paired **-26.4, t -2.92, 16/20 worse: significantly
  harmful as shipped.** Knob stays 0. The liveness read (all four
  players to Monarchy by T101 vs stock's Tribal) says the machinery
  works and the PRICING is wrong — suspects: the upkeep charge
  dominating (Communism 34 vs Democracy 216 on the same army pushes
  toward cheap-and-poor governments), the dropped effect-sum's blind
  spot (veteran/HP/bribe), or revolution timing. Diagnosis session
  owed before any sweep.

- G4/G5 — THE FOLD DIES, INSTRUCTIVELY: reach on the pv2 base is
  +2.8/seed, t +0.04, 8/16 (paired sd 259 vs 148 on the pv1 base).
  The +71.9 t+2.58 effect on pv1 is real and does NOT transfer to the
  tuned ECON baseline — a measured mechanism-x-block interaction,
  exactly what campaign gate 8 (separability) predicted we would
  find. Resolution per block-freeze doctrine: no pv3;
  survival_pf_reach stays 0 in the composed file and is swept
  JOINTLY with the fear lattice in the MIL finale on the tech-50
  blend. MUSTER's baseline question inherits this: build 3b against
  pv1+reach (where the war-venue evidence lives), and let the finale
  arbitrate the composed default.

Night batch complete (21:55). Ledger: MOVETYPE gates passed and
merged; reach free on econ but non-transferable to pv2 (no fold);
wait discount HARMFUL at placeholder (t -4.00, dimensionality task
#60); gov valuation HARMFUL as shipped (t -2.92, diagnosis task #61).
Three of four candidate folds stopped by measurement. The pipeline
is the product.

## GOV-DIAG (2026-08-12) — task #61 answered: the revolution was free

Full write-up: `findings/gov-anarchy-2026-08-12.md`. Re-derivable with
the new `benchmark/govcensus.py`. No holdout re-run tonight; liveness
and identity games only.

**Guilty suspect: #3, revolution timing — but the timing model is not
mistimed, the anarchy charge is structurally DEAD.** `ancost` is 0.0
on all 8712 candidate lines in the G3 corpus, because
`deep_gov_anarchy_turns()` returned 0.0 without the `H_REVOLUTION`
handicap on the reasoning (its own comment) that "at `hard` this term
is exactly zero". **The premise is false.** `H_REVOLUTION` decides how
many EXTRA turns are served; `handle_player_change_government()` sets
`government = game.government_during_revolution` unconditionally
(`plrhand.c:615`) and the only `update_revolution()` call is at BEGIN
phase (`srv_main.c:1381`), so the empire produces one full turn under
Anarchy at every difficulty.

The key number, from the savegame's own `changed_to_times` counter and
the scorelog's government tag, in the games already on disk:

| | deep gov=1 | stock |
|---|---|---|
| government changes per game | **26.8** | 17.8 |
| player-turns of Anarchy per game | **26.8** | 17.9 |
| Anarchy turns per change | **1.00 ± 0.00** | 1.01 ± 0.02 |

Paired change delta **+9.0/game, t +6.51, 20/20 seeds.** One turn of
anarchy per revolution in 20 seeds out of 20, priced at zero.

Cost of that turn, measured on bursts evaluated while in Anarchy:
**26.6% of a turn's empire output**, median 850 want, against a median
revolution margin of 2439 want — 19.6% of deep's revolutions have a
margin smaller than the charge. Replayed at the measured rate the
charge removes 17.3% of them.

Why zero mattered so much: `adv_best_government()` starts
`best_val = 0` (`advdata.c:996`), and deep's differencing design makes
"stay put" score exactly 0, so **the anarchy charge is the only
hysteresis in the system.** Re-scoring the government already held —
which must be 0 — comes out at mean **-16.8** output/turn (median
-14.5, sd 52.4, negative 67.1% of 1312 bursts), noise from
`auto_arrange_workers()` being path-dependent, amplified x24 by the
horizon. At zero cost that noise floor alone revolts. Deep revolves to
Democracy 132 times in the corpus and finishes there once; it holds
Federation 42.3% of turns vs stock's 75.0%, and pays +3.0 corruption
(t +4.08) and -74.5 gold (t -5.83) for it.

**Suspects 1 and 2 cleared as the cause.** The upkeep spread is real
(Democracy 215.4 vs Communism 29.6 charged per burst) and re-ranking
without it flips 12.9% of bursts, 106 Communism->Federation — a live
second-order bias, but it cannot explain deep spending 3x longer than
stock in Democracy, the priciest government on the board. It is a bias
in WHICH government, not in HOW OFTEN. Suspect 2's ceiling half was
never tested at all: `ceil` is 0.0 on all 8712 lines because
`gov_rate_ceiling_gain` defaults to 0.0 and `pv2-gov.ini` sets only
`gov_valuation = 1`. The military effects (veteran/HP/bribe) remain
the valuation's stated blind spot, neither indicted nor cleared.

**Landed:** `ai/deep: charge a revolution the turn of anarchy the
server always serves`. Floors the term at 1.0 turn. No new constant —
`gov_anarchy_weight` already exists at 1.0 ("charge the measured loss
at face value") and the loss is measured. Identity at knob 0 is by
construction (`deep_gov_value()` forwards to `dai_gov_value()` before
reaching the function) and demonstrated: smoke 10/10, `--arms deep`,
`TUNING=pv2.ini`, fixed binary vs `git archive` build of base
`c93649faf4`, `metrics.csv` **byte-identical over 40 player-rows**.
`meson test` 5/5 including `baseline-frozen`.

### QUEUED GATE — the G3 re-run, unchanged

    A: benchmark/tuning/pv2.ini      (gov_valuation = 0)
    B: benchmark/tuning/pv2-gov.ini  (gov_valuation = 1, fixed)
    venue: lategame-80, holdout seeds, 20 paired, jobs per budget
    read: B - A per seed; as shipped this was -26.4, t -2.92, 16/20

Secondary read via `govcensus.py`: changes/game should fall from 26.8
toward 17.8 and Federation occupancy should rise from 42.3%. If the
change count falls but the score does not recover, the residue is
suspect 1 and the next GOV session prices the upkeep bill properly
instead of netting it. **The knob stays 0 until that arm reads.**

## ERA 3b — MUSTER: the odds test becomes a launch gate (2026-08-12 night)

Branch `epic/era-3b-muster`, based at `c93649faf4`. `ai/deep` +
`benchmark` + doc only; zero `ai/default`, `ai/baseline` or
`server/advisors` changes. **Default OFF, and no A/B was run.**

3a's falsification is the design input, and the human's Gunpowder
doctrine (2026-08-12) is the frame. The take-odds curve is flat at
zero until pool 6-8, so nothing that asks each blade to justify
itself incrementally can bootstrap across it -- a SET of eight blades
is worth having while every increment in it is worth nothing -- and
formation is memoryless besides (0.88 attempts per player-turn, each
asking "can I win NOW"). Two independent reasons nothing masses.
MUSTER exists to make the RARE sane attack possible, not to attack
more often.

### What landed

| # | Commit | What |
|---|---|---|
| 1 | `0423c0aef5` | the two knobs: `op_muster`, `op_muster_stall` |
| 2 | `44e0a82b35` | muster state on the operation slot, saved and cleared |
| 3 | `67c086a2a4` | enter MUSTER on the worth gate when the odds refuse |
| 4 | `44ab3cbdb6` | accumulate the muster across turns |
| 5 | `b1b89fb64c` | abandon: stall, worth collapse, home-defence spike |
| 6 | `9554ef68b8` | the free blade -- 3a's release feeds the muster |
| 7 | `59c4182b85` | muster state on the `deep-opodds` counter |
| 8 | `9b9707bbfc` | `benchmark/era-3b-muster.{sh,ini}` -- identity, liveness, the arm |
| 9 | `f1677316ac` | `op_muster_home` -- the dominant trigger gets its own switch |

### The mechanism, and where each piece already existed

- **Formation** (`deep_op_form`). When no prefix of the pool clears
  the bar, the slot no longer dissolves: if the target clears a WORTH
  gate it enters `DEEP_OP_MUSTER` with `required = 0` and the WHOLE
  pool enrolled. The worth gate borrows the launch seat's own
  arithmetic -- `worth x P(take) - attack_loss_gain x loss x
  SHIELD_WEIGHTING` -- asked at the only odds a muster ever launches
  at, the bar. No threshold was invented; the gate is optimistic
  about the loss and says so at the seam.
- **Accumulation** (`deep_muster_enroll`). `deep_op_collect_pool()`
  unchanged, every turn: a muster claims exactly what a formation
  would have claimed and nothing more. Members walk to the muster
  point on the existing rotation/goto hold.
- **Launch** is the odds test that was already there
  (`op_launch_odds` + `op_net_value` + `op_hold_survivors`), untouched.
  A muster is never released by a head count and is not on the
  `op_muster_wait` clock -- forming one without `op_launch_odds` is
  refused outright.
- **Abandon** (`deep_muster_abandon`): stall (`op_muster_stall` turns
  without a new high-water count), worth collapse (the formation gate
  re-asked), home-defence spike (`deep_cover_removal_cost()` on the
  staging city -- the third consumer its NETTHREAT header has named
  since the day it landed). Abandon dissolves rather than retargets:
  laundering an abandoned muster into another one is how a unit sink
  hides from its own counters.
- **The free blade** (the draft). 3a's marginal comparison cannot
  bootstrap because in the flat region a blade's marginal gain is
  exactly 0. A muster does not need the blade to buy odds -- it needs
  the blade to be FREE, and free has an exact meaning that invents no
  epsilon: the lending city's survival is the same number with the
  blade and without it. Wired into BOTH release tests.

### The knobs

    op_muster        0/1, default 0 -- the mechanism
    op_muster_stall  turns without GROWTH before abandon
    op_muster_home   0/1, default 1 -- the home-defence abandon trigger

`op_muster_stall` is a **flagged PLACEHOLDER and needs a tuning
pass**. The brief asked for an observed cadence; there is none to be
had, because nothing in deep has ever accumulated a force across
turns, so there is no accumulation to read a stall off. It ships
equal to `op_muster_wait` (itself PLACEHOLDER) so a muster is not
granted more patience than a formed wave by fiat.

### Guards, each one checked rather than asserted

- **Default OFF**, with the byte-identity proof below.
- **Mustered units still garrison where they stand**: they hold at
  the staging city with `done = TRUE`, so `deep_city_stack_survival()`
  counts them as present, and `deep_muster_reserve_credit()` already
  credits nearby cities for en-route muster members (it predates this
  epic and needed no change).
- **No double-spend with deterrence/sortie prizes** holds by
  construction: muster members are op-claimed, and every claiming
  pool -- including both sortie passes since military audit C2 --
  checks `deep_op_claiming()` first. The defence counter-claim can
  still POACH a muster member, which is a priced transfer of one
  blade to one owner, not a second payment for the same body.
- **The never-launches damper is not fed by a working muster**: a
  muster enters `ops_formed` at the instant it stops being one
  (launch or retarget), never while it gathers. The damper's quantity
  is "formed but never marched", and a muster gathering is the
  mechanism working. `ops_launched` can never outrun `ops_formed`.
- **The unit-sink is visible**: `mform`/`mlaunch`/`mabandon` ride on
  every `deep-opodds` line as running totals, so CONSEQ N3's
  launch-rate reality check is readable off any late line.

### The A/B that is NOT run here, and what it must be

**Baseline is `pv1 + survival_pf_reach = 1` (`fact-c01.ini`), not
pv2** -- reach's +71.9/seed on pv1 does not transfer to the tuned ECON
baseline (G4/G5: +2.8/seed, t +0.04), so the war-venue evidence lives
on pv1+reach. The arm is `benchmark/tuning/era-3b-muster.ini` =
`fact-c01.ini` + `op_muster = 1`, nothing else.

    venue   tech50war-200, mixed, seat-mirrored, >= 16 seeds
    A       benchmark/tuning/fact-c01.ini        (pv1 + reach)
    B       benchmark/tuning/era-3b-muster.ini   (A + op_muster = 1)
    B'      B + op_muster_home = 0               (see the liveness read)

**B alone may not be worth the machine.** The liveness census says B
is mostly deep forming musters and abandoning them a turn later: 433
formed, 5 launched, 155 of 191 abandons from the home trigger. An arm
that measures a mechanism which is not gathering measures churn. B'
is the arm where the muster can actually accumulate, and the pair
B/B' is what separates "massing does not help on this venue" from
"the home trigger never let it mass". Whoever runs this should run
B' if only one extra arm fits.

**Attribution.** `cover_removal` stays **0 in both arms**: it moves
the muster's home-defence trigger AND the garrison draft's release
cost at the same time, so it cannot be read as a passenger on this
arm and enters the factorial as its own. `op_draft_marginal` stays
**0 in both** (3a falsified, score-null). `op_muster_stall` is
absent from both files, i.e. the compiled placeholder in both.

**Pre-registered predictions, in the doctrine's terms.** 3a's
"forms rise, strikes rise" scorecard is retired and is NOT used here.

| metric | prediction if MUSTER is the mechanism |
|---|---|
| kill-exchange ratio (killed/lost, deep seats, T200) | RISES from the control's; deep was out-killed ~2:1 (2289/3726 = 0.61 at pv1) while stock ran 1.13 |
| cities held at T200 | does NOT fall -- the disarmament lens (NETTHREAT). A muster that buys kills by emptying the walls is refused whatever it kills |
| score/seed, paired mirrored | the headline. Flat is a null and does not fold |
| `mlaunch`/`mform` | > 0. A muster→launch rate of ZERO with `mform` climbing is a unit sink and the mechanism is reverted whatever the score says |
| formation/strike counts | **no prediction**. They may legitimately fall; they are not evidence either way |

### Validation

- Build `-Daimodules=classic,tex,baseline,deep` clean (pre-existing
  warnings only, none in `deepops.c`); `meson test -C build` **5/5 OK**;
  `tune-audit.py` **CLEAN 228/228/228/228** (from 225 --
  `op_muster`, `op_muster_stall`, `op_muster_home`).
- `knobcensus.py` MAP entries for all three, predicates verified
  against the compiled predicate language: `muster>=1`,
  `why=muster-stall`, `why=muster-home`.
- **Identity, 10/10 byte-identical, taken on the SHIPPED tree**
  (`f1677316ac`, i.e. after the `op_muster_home` commit -- the first
  proof was taken at `9b9707bbfc` and stopped being a proof the moment
  the tree moved). 10 paired smoke seeds,
  `--arms deep`, `identity.ini` on both binaries, jobs 2, ports 7600,
  against a `git archive` build of the base sha `c93649faf4`, under
  `benchmark/results/muster-identity/`. Provenance checked as well as
  equality: the BASE arm carries **0** `mform=` lines and the head arm
  **277** in seed 1 alone, so the two binaries are demonstrably
  different ones (the gift epic's trap -- a proof that compared the
  branch with itself -- fails loudly here instead of passing).

  **A trap this run added to the ledger.** The re-run first reported
  `0/10 byte-identical, 0 differ, 10 MISSING`. Nothing was wrong with
  the games: the result key includes the server sha, so the rebuilt
  head landed in a SECOND `smoke-<key>` directory beside the first,
  and the assertion's `smoke-*` glob expanded to two paths, making
  every seed unreadable. A failure that reads as "missing" rather than
  "differs" is the shape to distrust -- the equality was 10/10 when
  the two cells were named explicitly. The script now resolves the
  newest cell per side, prints both names, and refuses to run if the
  two sides resolve to the same cell.
- **Liveness, 2 `tech50war-200` games, `era-3b-muster.ini`, jobs 2**
  (`benchmark/results/muster-live/`, read by
  `benchmark/mustercensus.py`). Counters only: 2 seeds price nothing
  (rule 6) and no score is read from these games.

      verdict        lines
      muster           433   entered on the worth gate
      muster-hold      457   held turns (one line per muster per turn)
      muster-abandon   191
      muster-launch      5
      formed            55   ordinary formations, unchanged path
      refused         2194

      mform = 433   mlaunch = 5 (1.2%)   mabandon = 191 (44.1%)
      abandon reasons: home 155, worth 34, stall 2
      held member counts: 1x337  2x54  3x27  4x20  5x12  6x1  7x5  8x1
      high-water maximum: 8 members
      odds > 0 on 18 of 457 held turns (3.9%)

  Every transition fires and every counter reads, which is what
  liveness had to show. **What it also shows is that the mechanism as
  parameterised does not yet do its job**, and that is the honest
  headline of this session: 337 of 457 held muster-turns hold ONE
  blade, the pool reaches 8 exactly once in two games, and 81% of
  abandons are the home-defence trigger. The one launch worth reading
  went at odds **1.000** against a city with `def_n = 0` -- the rare
  sane attack the doctrine describes, and the only kind this venue
  offers -- but five of those in two games is a mechanism that is
  gathering nothing.

  Cause, priced rather than guessed: at `survival_target = 0.85` a
  frontier staging city on a war venue essentially never spares its
  best blade, so the muster is abandoned the turn after it forms. The
  trigger is now behind `op_muster_home` **for exactly this reason**
  -- see the commit -- and whether that is deep correctly refusing to
  disarm a home front under real threat, or a cost side priced against
  the wrong counterfactual, is not decided here and must not be
  guessed at. It is the first thing the A/B has to separate.

### Not attempted, deliberately

No A/B of any kind (the box is owned by a derby and a batch tonight;
the tech-50 arm wants >= 16 mirrored seeds and is tomorrow's machine
work). No default moved. No tuning pass on `op_muster_stall`. No
change to the odds model, the bar, the expansion gate or the sea
clause -- all three priced dead by the probe.
---

## W36-DIM: the wait discount was never measured (2026-08-12, task #60)

Branch `epic/w36-dim`, cut from `epic/w03-horizon-content` at
`c93649faf4`. Machine budget: jobs 2, portbase 7700, liveness only —
the G2-style holdout re-run is queued machine work and was NOT run.

Task #60 asked which dimensional defect makes `site_wait_discount = 1.0`
cost −54.6/seed when the term's own liveness read was a 1.92-turn
discount, and named the CREDIT precedent (a term priced in the wrong
currency, 1–2 orders too large). **It is neither a currency defect nor
an honest term at a wrong weight. The G2 verdict is not a measurement
of the knob**: the arm and its control ran different binaries.

### The proof, from the G2 games themselves

Arm: `benchmark/results/nightbatch/g2-wait-isolated/holdout-report`
(pv2-wait.ini, `site_wait_discount = 1.0`, games run 2026-08-12 20:01–20:19).
Control: `benchmark/results/tuning/f2-d6-isolated/holdout-report` — the
run whose numbers `bec743c69f` published as pv2's isolated-150 holdout
(+55.0), games completed **2026-08-11 22:45 on binary 9275e9bd**.
`diff benchmark/tuning/pv2.ini benchmark/tuning/pv2-wait.ini` is one
line, `commands.serv` and `maphash.txt` are identical per seed — so the
tuning and the map were controlled and the binary was not.

Re-pairing those two dirs reproduces the published verdict exactly
(paired **−13.65 score/seat, se 3.41, t −4.00, 16/20 worse**; ×4 seats
= the −54.6 STATUS reported). Then:

1. **Three seeds never applied the discount once.** 105, 108 and 111
   have **zero** `deep-sitewait` lines in 150 turns. The census
   increments on the same branch that applies the amortize
   (`deepsite.c`, `if (wait > 0)`), and the metric log was plainly on
   in those games — 171/202/514 `deep-sitenaval` lines from the *same
   function*, 37/14/630 `deep-colony` lines — so the arm was
   behaviourally the knob-0 arm there. Those three seeds lost
   **−29.8, −32.8 and −13.0** score/seat.
2. **18 of 20 seeds diverge before the term could act.** First
   scorelog turn where arm and control differ, against the first turn
   any candidate was discounted:

       seed  first-diff  first-wait     seed  first-diff  first-wait
        101      53         103          111      54          none
        102      55          65          112      47           58
        103      57          53          113      56           65
        104      47         128          114      61           61
        105      59         none         115      50           65
        106      55          64          116      51           62
        107      56          68          117      42           83
        108      40         none         118      48           71
        109      44          90          119      47           80
        110      47          68          120      56           73

   Only seed 103 diverges after its first firing; 114 ties.
3. **No dose-response.** Pearson r between candidates priced and score
   delta = **−0.10** (n=20). The half of the seeds where the term fired
   *least* is hurt *more* (−19.0/seat, mean 227 candidates priced) than
   the half where it fired most (−8.3/seat, mean 1172 candidates).
4. **At least 26 non-merge `ai/deep` commits** landed between the
   control's games (Aug 11 22:45) and the earliest binary the night
   batch could have used (Aug 12 17:21) — 38 by the time the G2 cell
   ran. The published −54.6 is those commits plus one knob, and nothing
   on disk said so: the night-batch result dirs carry no `key.txt`,
   because the driver named its own arm directories.

### What the term actually does — 20 real games at 1.0

- **337 searches** priced a candidate; **13 995 candidates** discounted;
  45 225 wait-turns → **3.23 turns per candidate**, a **12.9% haircut**
  at amortize's 23/24 (the liveness read of 1.92 was a 2-game smoke;
  3.23 is the 20-game figure).
- **288 of 337 firings (85%) are `unit=0`** — `deep_contemplate_new_city()`'s
  virtual settler, i.e. the build-side *founder want*, not a settler
  choosing where to go. The term was built to rank destinations and
  five sixths of its firings land on a want.
- **10.2% of `deep-colony` publications** carry the discount, so the
  demand-spiral seam the W36 hand-off flagged is live but thin.
- Whether the discount ever moved an **answer** is not in the log at
  all: `priced`/`turns` count candidates, not decisions. Fixed below.

### What landed

- `ai/deep: report whether the wait discount moved the answer` —
  `deep-sitewait` now carries `bestwait` (the wait charged to the site
  that won) and `flip` (the site that would have won with the term off
  is a different tile). Observation only; inert at knob 0.
- `benchmark: record the binary and tuning each game ran on` —
  `run-game.sh` writes `provenance.txt` (sha256 of server, config,
  tuning, runner) into **every** game directory, whichever driver named
  it. This is the artefact whose absence made the confound invisible.

### The same-binary A/B nobody had run

Five isolated-150 seeds, **one binary** (`server sha 1600b9d7`, one
config, one runner — `key.txt` in both dirs proves it), `pv2.ini` vs
`pv2-wait.ini`, jobs 2, ports 7700/7740. Results under
`benchmark/results/w36-dim/`.

    metric        knob 0    knob 1.0    delta     se       t   wins
    score         183.20     181.55    -1.65    1.91   -0.86   1/5
    cities         14.25      13.75    -0.50    0.31   -1.63   0/5
    pop            81.85      79.50    -2.35    1.53   -1.54   0/5
    mfg            59.95      57.75    -2.20    1.35   -1.63   0/5
    settledarea    61950      60000    -1950    1226   -1.59   0/5

    seed  scorelogs   searches  cands  turns/cand   knob0   knob1  delta
     105  IDENTICAL          0      0        0.00   164.5   164.5   +0.0
     108  IDENTICAL          0      0        0.00   162.8   162.8   +0.0
     111  differ            17    724        1.62   181.2   181.2   +0.0
     115  differ            27   1463        5.46   202.0   192.8   -9.2
     116  differ            29   1585        2.10   205.5   206.5   +1.0

**And the closing proof.** On seeds 105 and 108 tonight's knob-0
scorelog is **byte-identical to the G2 arm's knob-1.0 scorelog** —
the same game, to the byte — while both differ from the control the
verdict paired them against by −29.7 and −32.7 score/seat. On those
seeds the measured "harm of the wait discount" is 100% binary and 0%
knob, and it is provable from files already on disk.

At n=5 the honest same-binary read is **−1.65/seat (t −0.86, 1/5)**,
i.e. at most about an eighth of the −13.65 that was published, and not
significant. Five seeds is not a verdict — that is the point of the
gate below.

### Liveness of the new counters (seeds 115/116 at 1.0)

    56 searches, 3048 candidates, 11 316 wait-turns -> 3.71 turns/cand
    bestwait   mean 0.89, median 0, ZERO for 32/56 winners, max 5
    flip=1     24/56 searches (42.9%)
    unit=0     48/56 searches (86%)

So the term does change decisions — two searches in five — and the
site it picks usually pays no wait itself (0.89 against 3.71 mean per
candidate), which is the steering it was built to do. Whether that
steering pays is exactly what was never measured.

### Verdict on task #60

**Not a dimensional defect.** The term's currency is turns and it goes
into the same `amortize()` as the travel delay at 1–5 turns per
candidate; the CREDIT failure was 1–2 orders of magnitude in the wrong
unit and there is nothing like that here. **Not an indicted weight
either** — nothing has yet measured 1.0 against anything. No knob was
moved and no formula was changed, because the evidence that would
justify either does not exist.

**The queued gate** (machine work, not this session's budget): the
G2-style 20-seed isolated-150 holdout at `site_wait_discount = 1.0`,
**both arms on one binary**, with `provenance.txt` on both sides. Do
not sweep a range first — one arm at 1.0 answers whether there is
anything to sweep. If it reads harmful, the next question is not a
smaller scalar but the seam: 86% of firings are `unit=0`, the
build-side founder want, where `deep_wait_expected() > 0` means "no
hull is free for me" and the discount damps the want that commissions
the hull. That is a where-it-lands question, and `flip`/`bestwait` are
now in the log to answer it.

### Collateral: G1 and G3 were run the same way

Both compared tonight's batch binary against controls whose games
finished 2026-08-11 (`f1-d6-econ` at 22:24, `f2-d6-isolated` at 22:45
— the pv2 adoption run, binary 9275e9bd). **G1's NULL and G3's
"gov valuation harmful as shipped, −26.4, t −2.92" carry the same
uncontrolled variable** and neither is attributable as it stands. G3
cannot be rescued by the divergence test used here — its knob acts
from turn 5 — so it needs a same-binary re-run before that verdict
stands. G4/G5 ran both arms that night and are unaffected.

**The rule this earns:** a cached control is only a control if its
binary is the arm's binary. From now the artefacts say so.

## Cross-binary control corrections (2026-08-12, very late)

W36-DIM proved G2 measured a BINARY, not the knob (knob-0 scorelogs
byte-identical to G2's knob-1.0 on seeds 105/108; zero-firing seeds
"lost" 30 points). MOVETYPE's unconditional merge broke the
inert-at-defaults chain; every comparison against cached pre-MOVETYPE
controls after 7ad93151b8 is void. Ledger:
- G2 wait-discount "harmful, t -4.00": WITHDRAWN. Same-binary 5-seed
  probe reads -1.65, t -0.86. Proper same-binary gate queued.
- G3 gov "harmful, t -2.92": score attribution VOID (same flaw). The
  anarchy-zero DIAGNOSIS stands - its evidence (26.8 changes/game vs
  stock 17.8, ancost=0 on 8712 lines) is within-arm, binary-neutral.
  The running g3rerun is same-binary and remains the gate.
- G1 reach econ null: direction unaffected (null), magnitude
  unreliable; re-take same-binary only if it ever matters.
- F1 food-veto (+17.9 t+1.48) used the same-binary H0 control: VALID.
- RULE, from tonight: an arm and its control run on ONE binary, or
  the control re-runs. Result dirs now record binary provenance
  (2e91446316).

- G3 same-binary re-run (anarchy floor in): gov_valuation=1 vs H0
  control, paired -5.5, t -0.55 — NULL. The floor fix moved gov from
  (spuriously) harmful to neutral; knob to the pile for the finale
  ECON sweep alongside gov_anarchy_weight and the untested ceiling
  credit.

- H1/H2 gift mixed arm (same binary, seat-mirrored, 20 holdout
  seeds): gift-refold +62.8 vs pv2 +36.9 — paired +25.9, t +0.74,
  10/20. Clears §11.4's wash-or-better bar with room; FOLD-ELIGIBLE.
  Human decision queued: adopt gift-refold.ini's class values into
  the composed baseline (rival leg to 0), or hold for a larger-n
  read. The old indiscriminate gift's -3.34 transfer is what this
  replaces.

## Derby-4 (2026-08-12 overnight): the era collapse, third confirmation

7 deep vs 7 classic, Europe-style generated map (startpos was
adjusted — NOT the true scenario map, a kit item for derby-5),
T300, tip binary, pv2 + trait-seeded stances. Result: deep LOSES
1761 v 2095 (-48/seat), classic sweeps the podium, deep's best is
Iceland (337, and the most cities at 20).

The trajectory is the finding: +70 at T50, crossover T100-150,
monotone divergence to -337 at T300 — the tech-50 collapse
reproduced in the flagship venue, with the crossover exactly where
the canonical venues' horizons end. The benchmark canon measured the
half of the game deep wins. Consequences: the ERA/massing arc and
the finale's late-era blend are the campaign's critical path, full
stop. Kit lessons banked: set timeout -1 for headless (memory), true
scenario map for derby-5, zero eliminations in 300 turns on this
geometry.

- E1/E2 hut-flip: +24.7 vs +24.7 — the honest arm is a practical
  no-op on its own venue (the misdirected discount it corrects was
  being spent on huts that changed no outcomes). Knob stays 0,
  correctness noted, priority floor. Batch 2 complete 00:11.

## pv3 minted (2026-08-13): the gift fold — 11.4's first execution

Human go. pv3 = pv2 + gift_ally_gain 1.0 / gift_instrument_gain 1.0 /
gift_rival_gain 0.0 (benchmark/tuning/pv3.ini). Evidence: mixed
head-to-head holdout +62.8 vs pv2's +36.9, paired +25.9, t +0.74 —
clears 11.4's wash-or-better criterion with room; liveness showed
rival-directed gold 2020 -> 0. pv3 is the composed baseline for all
future arms; pv2 remains the control lineage.
## PERPLAYER: one game, several tunings (2026-08-13, task #44)

Pre-v4 plumbing, gated on byte-identity. The constraint the derby-4 kit
work hit — `FREECIV_DEEP_TUNING` loads exactly ONE ini per server
process into one global `struct deep_tuning`, so candidate-vs-candidate
inside a single game was impossible — is gone. Each deep seat now has
its own tuning struct, and `benchmark/arena.py` seats different
candidate files at one table. Branch `epic/perplayer`, 5 commits.
No policy and NO new knobs: tune-audit CLEAN at the same 228 fields,
`meson test` 5/5.

This is the plumbing half of the arena tier
(`findings/tuning-venues-2026-08-06.md` §6a). The tier's *risk* half is
answered below; its *value* half (rank-based fitness over many tables,
the balance experiment) is now runnable and unrun.

### The loading scheme (full text at the top of `ai/deep/deeptuning.h`)

1. **Base, unchanged** — compiled defaults overlaid by
   `$FREECIV_DEEP_TUNING`'s `[tuning]` section. One file, every deep
   seat. With no arena this is the whole scheme.
2. **Arena, opt-in** — `$FREECIV_DEEP_ARENA` names a manifest:

       [arena]
       P1 = "benchmark/tuning/candidate-a.ini"
       P2 = "benchmark/tuning/candidate-b.ini"

   Seats resolve by player NAME first, then by `slot<n>`. Each named
   file loads exactly the way the base file does (defaults, then its
   keys) and is cached by path, so eight seats on one file share one
   struct. A seat named by neither keeps the base values.

The layers compose at **seat** level, not key level: a named seat runs
its own file from the compiled defaults, NOT the base plus a delta. One
semantics for every tuning file this project has written beats the
convenience of an overlay, and every archived ini is already a whole
file.

Assignment resolves on a seat's **first read**, not in the alloc hook:
`server_create_player()` runs before `server_player_set_name()`
(`server/stdinhand.c:908` vs `:939`), so at alloc time there is no name
to key on.

### Reading the values — and why the mechanical swap was NOT done

The brief said to swap `deep_tuning_get()`'s 229 call sites for a
per-player accessor. That was deliberately not done, and the reason is
the failure mode §6a gates this work on. 72 of the 229 sit in functions
with no player argument at all, and many of the rest carry TWO players
(the deciding seat and its target), so a mechanical swap *invents* a
seat binding at every site — and a wrong binding is invisible to the
identity gate, because under one file every seat holds the same values.
A gate that cannot see the defect it exists to catch is not a gate.

What landed instead is the design §6a itself points at: a phase-scoped
active pointer. AI phases are strictly sequential per player, so the
pointer has no concurrent reader by construction. `deepai.c`'s 49 hook
wrappers enter the scope of the seat they serve and RESTORE the previous
one on the way out — save/restore, not set/clear, because hooks nest
across seats (`city_lost` fires for the victim inside the attacker's
phase). The single path that reaches deep without passing a wrapper,
`deep_worker_task_value()` / `deep_base_want()` (registered directly
with the advisors' `aw_set_task_value_hook()`), names its seat
explicitly with `deep_tuning_for(pplayer)`. Entry-point inventory is
complete: the vtable, `fc_ai_deep_setup()`, and that one hook are the
only ways in (nothing outside `ai/deep` references a deep symbol
except `server/aiiface.c:217`).

Coverage is measured, not asserted. `deep_tuning_scopeless_reads()`
counts reads that found no scope while an arena was configured, and
`dpai_data_phase_finished()` logs it. It cannot fire in a single-file
run (every seat's pointer IS the base struct's address); in an arena a
nonzero count names a path no guard covers.

### Gate 1 — byte identity: 11/11, PASSED

smoke (4p/32×32/T100), `git archive` build of the base commit
`304e36af41` vs the branch build, same runner, same config, same seeds,
`DEEP_QUIET=1`. Scorelogs compared byte for byte:

| arm | seeds | identical |
|---|---|---|
| deep | 201–210 | **10 / 10** |
| mixed_dc (deep vs ai/default in one game) | 201 | **1 / 1** |

Map hashes matched on all 11. Base binary verified to be the base tree
(`strings | grep -c deep-arena` = 0 against 2 for the branch), per the
box quirk that `meson setup` with no source argument configures the
caller's cwd.

A 12th game re-ran seed 201 on the *rebuilt* binary after the one
comment-only commit that followed the tested build, and came back
identical too — so the gate covers the tree as it stands, not just the
binary it was taken on.

### Gate 2 — per-seat attribution: exact, and scopeless_reads = 0

Two arena smoke games, `benchmark/arena.py`, candidates dealt
round-robin and rotated by `seed % seats`:

- **demo-201**, `identity.ini` vs `arena-demo-traits.ini`
  (`stance_source = 1`). The game's own log settles attribution
  independently of the harness: P1/P3 print `src=traits econ=0.833
  mil=0.833`, P2/P4 print `src=identity econ=1.000 mil=1.000`, held
  from turn 1 through turn 50. Per-seat scores: P1 55, P2 112, P3 87,
  P4 85 — identical to the plain all-identity game on the same seed,
  because smoke's traits are FIXED at 50 and every stance gate reports
  0 executions. Attribution proven; the candidate is inert on this cell.
- **demo-202**, `identity.ini` vs a one-key `garrison_floor = 0` file,
  chosen because it must bite. Scores P1 81, P2 100, P3 53, P4 61
  against the plain game's 119/84/38/89 — the seats diverge from the
  single-file run, so the per-seat values reach decisions and not just
  the log.
- `scopeless_reads=0` on every phase of **both** games: the 49 guards
  covered every tunable read that happened.

### Harness

`benchmark/run-game.sh` takes `ARENA` (→ `FREECIV_DEEP_ARENA`) and
records the manifest *and* every candidate file it names in
`provenance.txt` — the manifest's own hash does not cover the values
played. `benchmark/arena.py` deals seats, writes the manifest, runs the
game, and reports per-seat score keyed by candidate plus per-candidate
mean rank over several games. It re-implements run.sh's three
tuning-file checks per candidate (no `[tuning]` header; unknown key;
float written without a decimal point), because an arena multiplies each
of those 40-game-A/B losses by the population.

Stated in the file, and worth repeating here: arena fitness is
population-dependent and can cycle, so the tier **triages, it never
claims**. Claims stay with run.sh's paired protocol on held-out seeds.

### Next

- `tune.py` has no `--arena` mode; `arena.py` is the callable unit and a
  campaign wrapper is the next step (rank-based stage 0 of §6e's racing
  ladder).
- The balance experiment (§6a: round-robin the personality corners
  within the shipped `stance_trait_span`, require **no perpetual
  winner**) is now runnable. Note demo-201's finding in advance: on
  smoke the traits are fixed, so that experiment needs a venue whose
  nations actually differ (`smoke-traits.conf`, or the derby map).
- Candidate-vs-incumbent tables when stock goes stale as a yardstick
  (§2a) are unblocked by the same plumbing.
## EXPANSION slice 2: the lift queue is not short of hulls (2026-08-13)

Branch `epic/expansion-lifts`, cut from `epic/w03-horizon-content` at
`304e36af41`. Seven commits: two counters, two knobs (both
default-off) and the correction the first knob arm earned, one
benchmark kit, plus this entry. Machine budget jobs 2, ports 7100+,
**no A/B** — the arms are queued for the daytime machine.

- `da2d8f84e0` `ai/deep: count the founders and the berths waiting on each water`
- `770517e991` `ai/deep: count the dead missions a lift muster is holding`
- `faf92ecfd7` `ai/deep: let overseas founder demand commission a hull`
- `968805ed31` `ai/deep: let the muster release a passenger whose target died`
- `73bbfefbd0` `benchmark: add the lift-commission arm and its gap reader`
- `85e19278fa` `ai/deep: charge the commission for the hulls it ordered this turn`
- `b0f0b022b1` `ai/deep: keep the idle-hull doc comment with its function`

### Why this slice

Three measurements pointed at one circle. Slice 1: from t20 the land
search scores **0.0** candidates per search on a settled venue (89% of
refusals the citymindist ring), so every founder want deep still forms
is an overseas want and the lift is the only expansion channel. W36:
57.4% of wait spells never enroll, and the fifth running ≥ 5 turns
holds 61.4% of all waiting. W36-DIM: **86%** of the wait discount's
firings land on the build-side founder want — the want whose
fulfilment would commission the hull whose absence is the wait.

Demand damped by the queue; the queue there because demand never
commissions supply. The instrument landed first (rule: two guessed
expansion fixes were reverted, the measured one won its cell).

### What the gap counter showed — and it is not a supply shortage

`deep-liftgap`, one line per water per player-turn: founders waiting
(ACTION_FOUND_CITY, unclaimed — the same population whose spells the
wait estimator reads) against berths idle and berths building on that
water, with `deep_wait_expected()` beside it. Two isolated-150 games
at defaults, 632 water-turns, turns 53–150 (the line is written only
when something is happening on that water, and **before t53 no water
this player touches holds a waiting founder, an idle berth or a hull
in production**):

    field       >0 of 632    max     mean
    founders          123      3     0.23
    waiting           183     15     0.45
    idle hulls        496     15     3.90
    idle berths       496     30     7.82
    building          188     20     1.20
    wait               67     12     0.44

    gap = founders - berths - building  > 0 on  67/632  (10.6%)
    ... of those, on water that BRIDGES to another continent:  7 (1.1%)
    ... wait > 0 with founders == 0:                           0/67

**The empire is hull-rich and founder-poor on the water that matters.**
Where founders wait on a bridging water, deep is already carrying ~3.9
idle hulls (7.8 berths) against a mean 0.23 waiting founders. Sixty of
the sixty-seven positive gaps sit on water that closes on its own
continent — a founder queued beside a dead-end sea, where
`boat_water_gate` refuses the keel and is right to. The long waits are
there too: the 12-turn spells are all on `bridge=0` water with zero
hulls and zero production, while the bridging water one tile away
holds 4–16 idle berths.

So on this venue the queue is a **distribution and geometry** problem,
not a supply problem — which is exactly what the fleet census said
from the other side (idle fraction pinned at 75% across five
interventions and every fleet size: "not overbuilt, badly
distributed"). The commission ships because the population it acts on
is real, but it is **7 water-turns in 632**, and nobody should expect
a large A/B from it on isolated-150.

Two more readings worth keeping:

- **Every wait > 0 has a founder behind it (67/67).** The wait
  estimator's population is settlement, which confirms W36-DIM's seam
  from the supply side rather than by inference.
- **4 of the 7 serviceable gaps carried no fresh `colony_want`.** The
  colony-lift want can only bid on turns a lift-limited settler search
  published a score; on those four turns the queue was dry and the
  shipyard silent. That is the hole the commission fills.

### What shipped

**`hull_commission_weight`** (double, default **0.0** = off, the 229th
tunable). In the coastal build decision, per water this city touches:

    want = amortize(w x n x (value - amortize(value, wait)), build)

`n` = MIN(gap, hull capacity), both ruleset facts; `value` =
`colony_want`, deep's own overseas site score, **aged by amortize()**
rather than cut off by a freshness window — a site is not worth nothing
because the probe ran three turns ago, and amortize is the discounter
every other delay in that file pays; `wait` = `deep_wait_expected()`
for the same water, so the term is zero exactly where that estimator
declines to claim a wait. The weight is the only constant. It honours
`boat_water_gate` **per water** (the census says that gate is
load-bearing here, not decorative) and pays the stance expansion gate
like the colony demand beside it.

**`lift_muster_release`** (int 0/1, default **0**). D-E's
re-validation has asked `city_can_be_built_here()` on every MARCH turn
since `0d6440e194`; the MUSTER is just as held, lasts
`lift_gather_turns`, and never asked. Counted first: **10 march, 2
muster** invalidations in the two default games, so the population is
about a fifth the size of the marching one. At 1 the gather drops the
dead mission, re-aims if it was the beachhead, and aborts with nobody
left, through the same `deep_lift_revalidate()` — the cheapest end to
do it at, since the hull has not sailed and the released founder still
has its own legs on its own continent.

**The multiplication the first knob arm caught.** On turn 88 of seed
2, **eight** coastal cities on ocean -2 priced the same gap of two
founders and **five** beat their incumbent choice: five hulls for two
passengers, because `building` is a once-a-turn photograph. The gap now
also subtracts berths this player has ordered this turn
(`commission_berths`), the same orders-not-deliveries accounting
extended to the current turn, and `ordered` is in the log.

### Validation

- `meson test -C build` **5/5** after every commit; `tune-audit.py`
  **CLEAN at 230 fields** (228 + `hull_commission_weight` +
  `lift_muster_release`, each in `deeptuning.h`, a `TUNE_*` macro,
  `tuning-params.ini`, and the ALL + X/ECON blocks); `knobcensus.py`
  MAP entries added for both, grade **direct** on `deep-commission
  gap>=1` and `deep-lift-invalid phase=muster`; the census reads
  `hull_commission_weight` **LIVE** (23 firings) and
  `lift_muster_release` MODE-with-population (2) on the knob cell.
- Built `-Daimodules=classic,tex,baseline,deep` in this epic's own
  worktree with its own `build/`, ports 7100+. The runner **pins the
  binary by copying it** (with `libfreeciv.so`, which the `$ORIGIN`
  runpath then finds) before a game starts — an epic that alternates
  edit, build and look is exactly where a run gets rebuilt under
  itself, and it happened once here before the pin existed; that run
  was discarded.
- **Liveness at defaults**, 2 × isolated-150 (`benchmark/results/
  explifts-live/`): 632 `deep-liftgap` water-turns, 10 + 2
  `deep-lift-invalid`, **0** `deep-commission` (knob off, and the
  block is not entered).
- **Knob liveness**, 2 × isolated-150 with `expansion-lifts.ini`
  (`hull_commission_weight = 1.0`, `lift_muster_release = 1`,
  `benchmark/results/explifts-knob/`): **23 `deep-commission` firings,
  4 of which beat the running military choice**, mean gap 2.00, mean
  wait 3.91 turns, mean want 32.8. Both arms of the muster counter
  fire (8 march, 2 muster).
- **...and BOTH knobs are inert on these two seeds. The knob arm's
  scorelogs are byte-identical to the default arm's, seed for seed.**
  Two separate reasons, both worth keeping:
  - The commission's want is **too small to win a build**: mean 32.8
    against the colony-lift want's mean 197 / median 146 / max 492 in
    the same games (240 bids). The four "beat" flags are the running
    military choice at that instant, not the city's final production.
    At weight 1.0 this term cannot act, so **an A/B at 1.0 on
    isolated-150 would measure nothing**. The registered range 0–5 does
    reach parity (5 × 32.8 = 164 against the 146 median); the sweep is
    the tuner's, and 1.0 is a PLACEHOLDER, not a recommendation.
  - The muster release changes **when inside the turn**, not what: at
    both firings the op sailed in the same drive pass, so the MARCH
    re-validation caught the dead mission on the same turn and
    dissolved the same operation. The eight `lift-invalid`
    dissolutions are identical line for line between the arms. It can
    only pay where a gather spans turns without boarding.
- **Off-switch proof: 10/10 paired smoke seeds BYTE-IDENTICAL**, 0
  differ, 0 missing, against a `git archive` build of base
  `304e36af41`, `identity.ini` both sides, jobs 2, ports 7120/7140
  (`benchmark/results/explifts-identity/`). Both counters run
  unconditionally, so this is what proves them inert.
- Runner: `benchmark/expansion-lifts.sh {live|knob|identity}`; reader:
  `benchmark/liftgap.py`.

### Corrections to the plan, recorded

1. **`expansion-logistics.md` §3.5 is half stale.** The MARCH-phase
   re-validation it proposes shipped on 2026-08-05 (`0d6440e194`,
   `deep_lift_revalidate`), unconditionally. Only the MUSTER phase was
   still open, and that is what this slice measured and gated. Read the
   code before scoping from that document's §3.
2. **"Demand never commissions supply" is too strong.** `colony_want`
   does reach the shipyard — but only on turns a lift-limited settler
   search published a score, and the census caught the queue dry and
   the shipyard silent together on 4 of the 7 serviceable gaps. The
   circular dependency is real; the break in it is narrower than the
   hand-off assumed.
3. **The size of the prize is 1.1% of water-turns on isolated-150**,
   and anyone planning further hull-supply work should read
   `deep-liftgap` on their own venue before spending an epic on it —
   that is one run and one grep (`liftgap.py`). Sixty of the
   sixty-seven positive gaps this venue produced are on water that
   bridges nowhere.
4. **`deep-liftgap`'s `colony` field prints the FRESH publication
   only** (`colony_turn >= turn-1`), while the mechanism uses the aged
   value. To see what the term actually priced, read `value` and `age`
   on `deep-commission`, not `colony` on the gap line.

### For whoever runs the A/B

- **Do not run the commission arm at 1.0.** Measured above: at 1.0 the
  term's mean want is 32.8 against a colony-lift want of 146–197 for
  the same hull, and two isolated-150 games came out byte-identical to
  the off arm. Whatever weight the arm runs at has to be one the term
  can act at; the registered range reaches parity around 5.
- **One knob at a time**, both arms on ONE binary with
  `provenance.txt` on both sides — the rule W36-DIM earned.
  `lift_muster_release` is a separate, much smaller arm (~1 firing per
  game, and inert on these seeds) and should not be folded into the
  same cell.
- **Pick the venue with the counter, not with intuition.** The arm can
  only pay where founders queue on water a hull may legally serve.
  isolated-150 offers 7 such water-turns per 2 games; run 2 liveness
  games on any candidate venue and count `gap>0 & bridge=1` before
  committing seeds to it.
- **Watch `ordered` and `beat` together.** The failure mode this
  mechanism has already shown once is multiplication across coastal
  cities in one turn; `ordered` is the correction and `beat` counts
  the orders it did not stop.
- **1.0 is a PLACEHOLDER** flagged for `benchmark/tune.py` — the
  identity scaling of a term whose other factors are measured. It has
  never been tuned and no A/B has been run on it.

### What the next slice should be, on this evidence

Not more hulls. Sixty of sixty-seven positive gaps are founders queued
beside a sea that closes on its own continent, while the bridging water
one tile away holds 4–16 idle berths — the fleet is badly distributed
and the passenger is standing in the wrong port. Nothing in deep moves
a waiting founder to a coast a hull can reach, and nothing prices that
walk against the wait it would end. That is a demand-side mechanism on
the same seam, it is measurable with the counter that now exists, and
it is where the mass is.
## W12 slice 1: trunk roads and worker locality, both behind zeros (2026-08-13)

Task #55 from the stray-asks audit
(`findings/stray-asks-audit-2026-08-12.md` ask 2): "workers bias
toward nearby jobs" was PARTIAL (fallback-only, measured inert) and
"trunk road value connecting cities should go up" was MISSING
(unbuilt W12). Both are now built on the primary path, both ship at
0.

Branch `epic/w12-roads`, 5 commits.

### 1. The hole each mechanism fills

**Locality.** `worker_evaluate_improvements()` amortizes travel
exactly once -- after every loop, on the champion
(`autoworkers.c:939-943`). Inside the loops, the branch that chooses
between tiles nobody works yet compares the resulting tile value and
nothing else (`autoworkers.c:457-474`); distance enters only as a
tie-break on exact equality. Two candidates worth the same to the
city rank the same whether the worker stands on one of them or
fifteen tiles away. That is the hole, and it is on the path every
worker takes -- unlike `deep_idle_fallback_work()`, which ran only
where nothing else was wanted and measured inert for a reason that
does not transfer (`findings/idle-workers.md` section 7: in
lategame-80 the declined improvements really were worth nothing,
cities average size 5.8 and never grow to work the outer tiles).
This term never adds a job; it re-ranks jobs the auction already
wants.

**Trunk roads.** The advisor prices a road by what it does for the
TILE. `adv_workers_road_bonus()` (`autoworkers.c:138-306`) is the
function that *claims* otherwise -- its own comment reads "this
calculates the overall benefit of connecting the civilization" -- and
what it computes is a 12-tile adjacency template around the candidate,
counting how many road neighbours it would join. That is a local
pattern, not a connectivity query: it cannot tell a tile between two
cities from a tile between two dead-end spurs. Nothing anywhere in the
auction knows a road here would join two cities.

### 2. What landed

`deep_worker_travel_price()` (`ai/deep/deepsettler.c:2190`)
discounts the IMPROVEMENT -- candidate value minus the same tile
without it -- by `amortize(gain, worker_travel_discount * arrival)`,
the shape the site search already uses (`deepsite.c:1487`). The
improvement and not the total, because the advisor also compares the
total against `old_tile_value` to decide whether a candidate improves
anything at all: discounting THAT makes distant candidates vanish
rather than rank lower.

`ai/deep/deeptrunk.c` spans the player's cities with a minimum
spanning tree on map distance, walks each edge into a tile corridor
(greedy, land only; a link that must cross water is dropped whole),
and gives every corridor tile still lacking a road the LINK's worth:

    saving = corridor terrain cost - corridor cost under this road
    want   = trunk_road_gain * saving/SINGLE_MOVE * amortize(1, gap)

The saving is movement points on one traversal once the link is
whole, from the same terrain and road move costs `autoworkers.c`
reads two lines away. The `amortize()` over the tiles still missing
is the standard discounter, so the nearly-finished link is closed
before the barely-started one is begun. An MST and not every pair:
n-1 links make every city reachable, while pricing all n(n-1)/2 would
pile the same want onto whatever tiles lie between the two densest
clusters.

This is steps 1, 2 and half of step 3 of the sketch in
`findings/better-ai-plan.md` section 2, with one deliberate
departure recorded in `workstreams/LATER-PHASES.md`: the sketch's
"finish what you started" term is `is_last_gap_in_segment ? 3 : 1`,
an invented constant. `amortize(1, gap)` is monotone in the same
direction and carries no number of its own (rule 7).

Both reach the auction through the task-value hook, beside the base
want and on the same footing: all three say what an extra is worth
for a reason the advisor's tile arithmetic structurally cannot hold,
and all are added after the danger discount rather than through it.

**Known ceiling, stated because the census will show it.** The
advisor only enumerates tiles inside the player's own city radii
(`autoworkers.c:586-588`), so corridor tiles in the gap between two
distant cities are never offered to the hook at all. This slice moves
which road tile a worker builds, not whether a worker is dispatched
into no-man's land to build one. `links>0 hits=0` on a `deep-trunk`
line is that ceiling speaking, not a dead knob.

### 3. The 11.5 widening record

`aw_task_value_fn` was widened a second time (`9682267cb6`), and this
is the record CAMPAIGN-v4 11.5 requires. Appended, nothing
repurposed:

    old_tile_value   the same tile valued WITHOUT the candidate
                     improvement. The hook's `value` is a resulting
                     TOTAL; a consumer that wants the delta cannot
                     recover it.
    arrival          turns before this worker could begin work there.
                     It exists only inside worker_evaluate_
                     improvements()'s own pf_map; a consumer can only
                     guess it from map distance, which is the
                     terrain-blind guess pathfinding replaces.

Against 11.5's ratchet guard -- "could this signature be offered
upstream as an AI-module hook without embarrassment" -- the question
the widened hook asks is "what is this candidate worth now, what was
it worth before, and how far away is it". That is not deep's private
bookkeeping. The unregistered path is untouched: the call is still
inside `if (aw_task_value_hook != NULL)`.

### 4. Knobs, both PLACEHOLDER, both zero

    worker_travel_discount  0.0  OFF. Turns of amortize() per turn of
                                 walking. 1.0 is the unit case.
    trunk_road_gain         0.0  OFF (the plan is not even built).
                                 Movement points -> advisor tile
                                 currency.

Four audit places each, plus the G/ECON/ALL blocks and the knobcensus
MAP (`deep-worklocal disc>0`, `deep-trunk hits>0`, both direct-grade).
`tune-audit` CLEAN at 230 fields (base 228 + 2). `meson test` 5/5.
Neither has ever been tuned and neither has a score reading -- this
epic bought mechanism and instruments, not a result.

### 5. Identity: both proofs, fresh, around this change

CAMPAIGN-v4 11.5's belt-and-braces clause landed on this session, and
this is the discharge. Base binary = git-archive of
`304e36af41` (`epic/w03-horizon-content` tip) built in a scratch tree,
sha `52a8f58667aba910`; branch binary sha `578320920139c6e0`. Ten
paired smoke seeds per arm, tuning=none, jobs 2, portbase 7200,
`benchmark/results/w12-identity/`.

**Stock arm, the one 11.5 asks for: 20/20 byte-identical.**

    classic   scorelogs IDENTICAL 10/10
    baseline  scorelogs IDENTICAL 10/10

Both stock arms, not just the frozen one, because the hook is
registered at module setup (`deepai.c:1294`) whenever the deep module
is compiled in -- so it is live in games with no deep player at all,
and "no consumer registers" is NOT the state a stock arm runs in. What
keeps those games identical is the consumer's own first line,
`pplayer->ai != deep_ai_get_self() -> return value`, and 20/20 is that
line proved rather than asserted.

**Deep arm at the new knobs' zeros: 10/10 byte-identical.**

    deep      scorelogs IDENTICAL 10/10

Not "no significant difference" -- identical sha256 per seed.

### 6. Liveness: both mechanisms execute, and one moves behaviour

Two lategame-80 games with `tuning/w12-on.ini`
(`worker_travel_discount` 1.0, `trunk_road_gain` 5.0) against the same
two seeds at the shipping zeros; `benchmark/results/w12-live/`.
knobcensus reads both knobs **LIVE, direct grade**
(`worker_travel_discount` 843 cells-lines, `trunk_road_gain` 724).

    deep-worklocal          knobs ON      knobs OFF
    candidates offered      2 064 236     2 531 610
    candidates discounted     593 139 (28.7%)     0
    want removed            1 761 814             0
    mean arrival of those      3.79 turns         -

    BEHAVIOUR (en-route workers at end of player-turn)
    walker unit-turns             926          1 033   -10.4%
    mean remaining walk          6.148          6.884  -10.7%

The bottom two rows are the claim the ask actually made. Workers under
the discount end fewer turns walking, and the walks they are on are
shorter -- the auction is choosing nearer jobs, not merely pricing
them differently. (The offered-candidate count differs between arms
because the games diverge from turn 1; it is not a bug and not
comparable.)

    deep-trunk (ON arm only; OFF emits no line at all -- the knob
    gates the plan, not just the want)
    player-turns with a plan            739
    link-turns planned                2 229
    corridor tiles missing a road     4 360
    candidates the want reached      68 267
    player-turns with hits>0        724 / 739
    largest want issued               44.98

The corridors close, which is the mechanism working rather than merely
firing. One player's own trajectory, seed 1:

    turn  4  links=2 gap=7  hits=32   maxwant=22.00
    turn 14  links=5 gap=12 hits=304  maxwant=26.02
    turn 24  links=4 gap=7  hits=150  maxwant=27.15
    turn 44  links=3 gap=5  hits=105  maxwant=27.15
    turn 64  links=2 gap=3  hits=13   maxwant=22.36

Links appear as cities are founded and then leave the plan as their
gaps reach zero, while `hits` falls because there is less left to
want. The 15 player-turns with `links>0 hits=0` are section 2's
radius ceiling, visible exactly where it was predicted.

### 7. What the next session must know

- **No score reading exists for either mechanism, and none is
  claimed.** This epic was budgeted for smoke, identity and liveness
  only. Both knobs are PLACEHOLDER at 0; the A/B is unbought.
- **The trunk probe magnitude is arbitrary.** `trunk_road_gain = 5.0`
  was chosen to put the want on the same order as the tile wants it
  competes with so the census could show it landing. `maxwant` around
  25-45 against tile values in the same range means the term is
  currently able to dominate, which is a tuning question and not
  evidence.
- **The locality term is the cheaper bet of the two.** It costs one
  amortize() per candidate, needs no plan, and already moves a
  behaviour metric by 10%. If only one gets an A/B, it should be that
  one -- and its arm is `worker_travel_discount` alone, since the two
  knobs are independent and both default off.
- **Slice 2 is a task SOURCE, not a want.** The radius ceiling cannot
  be lifted from inside `worker_evaluate_improvements()`; it needs
  deep to offer corridor tiles the advisor never enumerates, in the
  shape `deep_idle_fallback_work()` already uses to reach outside the
  auction. See `workstreams/LATER-PHASES.md`.

## 2026-08-13 — MUSTER A/B verdict: reject as offense, retention signal noted

Three arms on tech50war-200 mixed, 16 mirrored seeds each, all on the
one pinned `.muster-bin` binary (control re-run on it after the
MOVETYPE merge — one-binary rule). Baseline pv1+reach (fact-c01).

    arm                    score/seed  vs control        cities deep:stock
    A  control              -22.7                        0.850
    B  muster (as shipped)  -34.8      -12.1  t -1.98    0.768
    B' muster, home off     -29.0       -6.3  t -0.85    0.902

Lifecycle totals (32 games/arm, running mform/mlaunch/mabandon):

    B   formed 2788  launched 24  abandoned 1208
    B'  formed 2392  launched 22  abandoned  396

Reading:
- The home-defense trigger was the churn engine (abandonment falls 3x
  without it) and it is what made as-shipped muster actively harmful.
- It was NOT the launch blocker. Launches are ~0.9% of formations in
  BOTH arms: the take-odds launch gate almost never clears even at
  accumulated pools, exactly as 3a's flat odds curve predicted.
  MUSTER as designed accumulates forever and never attacks.
- B' is a score wash vs control but holds the best cities ratio of
  the three arms (0.902 v 0.850): parked pools garrison where they
  stand and stop the home-trigger bleeding. The mechanism's real,
  measurable effect is defensive massing, not offense.

Verdict: `op_muster` stays 0. No fold. The retention signal is
consistent with the derby-6 lean-lattice finding and belongs to the
MIL-finale fear/garrison sweep, where a parked-army sink is the wrong
implementation of it. The deferred odds-bar question is now formally
unblocked (pools exist, odds still flat) but per the Gunpowder
doctrine it is a design question for the finale, not a knob to force.

Ops note, banked to memory: freeciv scorelog resume validation
requires the existing file to end with EXACTLY the loaded save's turn
block (`last_turn + 1 == first logged turn after load`, scores write
at begin-turn, synchronized with autosave); anything else disables
scorelogging silently for the whole session. Both derbies hit this
after the power-loss resume and were repaired from backups.

## 2026-08-13 — Derby postmortem: the lean lattice flips a −809 into a +337

Paired exhibition on the real Europe scenario (177x100, 37 startpos),
same seed, same pinned binary (174fce8cd2), 5 deep + 9 stock + barbs,
endturn 300. ONE variable: derby-5 ran the standard personality tuning
(pv2 base + stances), derby-6 the "lean" file — the same plus four
fear/floor knobs hand-zeroed after the live-log diagnosis
(floor_interior_weight, garrison_floor_danger, boat_shadow_fear,
density_fear_value; the diagnosis: garrison_floor mandates defenders
OUTSIDE the want auction, and 86% of military production was bought at
danger=0).

    final            DEEP           CLASSIC        margin
    derby-5 std      1391 (T282)    2200           -809 and falling (killed at T282, out of value)
    derby-6 lean     1498 (T301)    1161           +337 WON
    derby-4 std      1761 (T300)    2095           -334 (generated map, prior run)

Mechanism, visible in the tables every 25 turns:
- Standard: deep's over-garrisoned economy leaves one stock seat
  (Norge) to snowball unopposed — 100 cities by T160, 213 by T282,
  with a 491-unit army and a tech runaway (78 techs / 553 output at
  T217 vs deep's whole-team 233).
- Lean: the same Norge is contained at 6-10 cities ALL GAME; no
  runaway ever forms; classic collapses to 40 cities total while deep
  holds 91.
- Per-seat mirror: Deep_Rus finishes FIRST overall in lean (294, 31
  cities) and dies in standard (93, 1 city). Same seat, same map.
- Tech is the same story restated: at T217 deep's own tech pace is
  IDENTICAL in both arms (mean 47.7 vs 47.4) — the lean arm wins the
  tech race by preventing the rival runaway, not by researching
  harder. Era collapse is an economy failure, not a research one.
- Tail risk is real: lean Deep_Anglia was ELIMINATED (T~190) and
  Osmanli ground to 1 city late. The lean posture trades individual
  seat survival for the team mean.

Caveats: one map, one seed, hand-picked zeros, exhibition format
(team-of-5 vs team-of-9 on a fixed scenario). This is evidence for
PRIORITY, not a tunable verdict.

Consequence, already queued: the MIL-finale fear/garrison sweep leads
with the floor family (multi-seed lean-lattice A/B on benchmark
venues, jointly with survival_pf_reach and survival_target — the
thrice-implicated floor family is now four-times-implicated). The
DECAY brief's measured-rate table is the same finding from the
pricing side: the world's discount rate collapses after T100 and
deep's does not.

Retention: verdict banked; saves/ and game.logs pruned per policy
(scorelogs kept, derby-6 final save kept for replay).

## 2026-08-14 — BERTH A/B on the naval venue: mechanism causal, population absent, score NULL

EXPANSION slice 3's open question — does routing stranded founders to
a serviceable berth pay where overseas founding has value — answered
on tech65sea-150, 16 mirrored seeds, pv3 vs pv3 + founder_berth_route=1
(one binary c063a63dcb, pinned .berth2-bin with lib).

- Score: mean paired diff +0.14/seat, sem 0.67, t +0.21 — NULL.
- Attribution is exact: 13/16 seed pairs are score-IDENTICAL, and the
  three divergent seeds are exactly the three where deep-berthroute
  fired (once each): +5.5, +4.4, −7.6 per seat. Fires → game moves;
  doesn't fire → byte-identical. Free in-vivo off-path identity proof.
- Why it barely fires: the deep-berth census logged 1,213 boat-needing
  founder events in the berth arm; only 7 (0.6%) were the stranded
  class (coast=1 serves=0) and only 3 had a reachable serviceable
  berth. Slice 2's 60/67-gap population is CLOSED-WATER geometry
  (founders beside idle bridging berths on lakes/inlets); tech65sea is
  open ocean, where water almost always serves the crossing.
- Verdict: knob stays 0 in pv3. The mechanism is live, causal, and
  costs nothing off-path, but its defect population does not exist on
  open-sea venues and n=3 firings (net ~0) cannot price it. The venue
  where it lives is the Europe derby map — candidate rider for
  derby-7 alongside the fence and the lean zeros (exhibition evidence
  only, not a tunable verdict).

Retention: verdict banked; berth-naval saves/ and game.logs pruned per
policy (scorelogs, DONE, provenance kept).

## 2026-08-14 — DECAY slice 1: the shape census — flips are DENSE at the measured rates; the knob is load-bearing

Brief: `sessions/S-DECAY-one-discount.md`. Branch `epic/decay-census`,
three commits: `f6edd45e1b` (deep-decay shape lines in deephorizon.c,
log-only), `a60735801a` (corpus driver + offline flip-curve analysis),
`225cb67633` (bell-aware exponential model + exact validation).

**Byte-neutrality proven at the play level**, not argued: tech50war-200
seed 1 run on the pre-census and census binaries (.pin-base
853f61cad06a vs .pin-census 0edb3997e8ad) — scorelogs byte-identical
including headers, maphash identical, and the T201 final saves hash
identical after normalizing only wall-clock fields (event timestamps,
phase_seconds, last_turn_change_time).

Corpus: 12 train seeds, mixed_db on tech50war-200, pv3 tuning —
305,579 census lines, 39,493 build-pass decisions with >=2 candidates,
132,611 credited improvements. The offline rectangle replay reproduces
the code's own credits EXACTLY (0 of 132,611 off by >2%), so the
flip shares below are model-vs-model, not reconstruction noise.

Flip-vs-rate: share of decisions whose domestic argmax differs from
the MORT-24 rectangle when the same logged shapes are priced as
`sum b(t) q^t` (delay = leading zeros, horizon-derived counts run to
the scored bell as a truncated geometric; structural caps kept; lumps
invariant; the upkeep bill re-priced as the same per-turn stream):

    rate     | T1-50   T51-100  T101+  |  all
    0.001    | 56.5%   27.3%    15.8%  | 29.3%
    0.002    | 53.7%   26.5%    15.4%  | 28.1%   <- measured late (T100+) rate
    0.005    | 45.9%   24.0%    14.5%  | 25.1%
    0.011    | 32.6%   19.7%    12.6%  | 19.7%   <- measured overall rate
    0.02     | 21.8%   14.2%     9.6%  | 14.0%
    0.037    | 10.2%    5.2%     5.4%  |  6.5%   <- measured T1-50 rate
    0.0417   |  8.4%    3.6%     4.9%  |  5.4%   <- 1/MORT, the anchor
    0.06     | 10.0%    5.5%     7.3%  |  7.4%
    0.08     | 14.9%    8.7%    10.5%  | 11.1%
    0.12     | 22.7%   13.8%    16.4%  | 17.2%
    (n = 9,653 / 12,098 / 17,742 decisions per phase)

Anchor comparison. The curve bottoms at 1/MORT, as it must — but the
floor is 5.4%, not zero: even at the rectangle's own timescale, one
decision in nineteen is decided by the WINDOW SHAPE rather than the
timescale (instalment streams discount as q^ttg/(1-q^ttg), the window
as steps*q^ttg — same rate, different curvature). At the measured
early rate (0.037) the rectangle is nearly right: 6.5%. At the
measured overall rate (0.011) one decision in five flips; at the
measured late-game rate (0.002), 28% overall — and 15.4% of the
T101+ decisions themselves, which is the era-collapse myopia priced
per-decision. The steepest gradient sits exactly in the plausible
band 0.011–0.037. Per the brief's rule: flips are dense near
plausible rates, NOT sparse-and-extreme — slice 2 is worth buying.

By family (dominant credit of the rect winner, at 0.002): content
53%, uncredited-winner 41% (a deep stream overtakes stock's own
winner), bundle 29%, popins 22%, health 22%, veteran 5%, capacity 1%.
At high rates the flippers invert to the perpetuity families
(upkeepfree 50%, veteran 27%, capacity 16% at 0.12). The modelling
claim localizes: patient pricing mostly reorders content-vs-arrivals
and stream-vs-stock decisions, not the cap-lifter ladder.

Sensitivity: WITHOUT bell truncation (pure perpetuities) the low-rate
flips inflate to 88/58/38% by phase at 0.002 (56.5% overall) — i.e.
at late-game rates, payments past the scored end dominate the ranking
unless the discount is bell-truncated. **Slice 4 (the closed-form
geometric tail) is therefore not an optional companion to slice 2 at
low rates; ship them together or the knob misprices the endgame.**

Census limitations (recorded, not hidden): the argmax is over the
domestic building auction only — the building-vs-unit seam and the
techwall routing into tech_want are not censused; pv3 zeroes
horizon_multiplier/govcenter/culture weights, so those families do
not appear in this corpus; and deepgov.c:536 (government payoff) and
deepdefence.c:7092 (unit lifetime) count the same MORT rectangle in
OTHER auctions, outside slice 1's scope but inside any eventual
one-discount unification.

What slice 2 should do differently given the data: (1) the knob's
byte-identity-at-0 contract stands, and the search range wants to be
per-phase from the start — a single global rate can only reproduce
the rectangle near 1/MORT, and the early game is the most
rate-sensitive phase (53.7% of T1-50 decisions flip under the late
rate), so the two-piece schedule (slice 2b) is not a refinement of
the flat knob, it is the first honest arm; (2) implement the
duration sum bell-truncated from day one (fold slice 4 in); (3) the
retune-invalidation guard in the brief is real — at plausible rates
this reorders a fifth of the ladder, so it lands only where the
brief schedules it (before the extensive-tuning finale).

Retention: verdict banked; raw shape census kept compressed per seed
(benchmark/results/decay-census/census-s*.decay.zst, ~250KB each,
analysis verified to reproduce from extracts exactly); census-s2..12
game.logs pruned per policy; seed-1 pair (base + census) kept whole
as the identity evidence; scorelogs, DONE, provenance kept.
## NETTHREAT item 2 — the flat garrison-pin discount, live and a wash (2026-08-14)

Branch `epic/netthreat2-pin`, based on `epic/w03-horizon-content` at
`1844e16efc`, per `sessions/S-NETTHREAT2-pin-discount.md`. Two code
commits:

- `e03ca158ad` `ai/deep: add presence-weighted attackers to the duel
  substrate` — `deep_stack_survival_wfull()`: att_w[j] = P(attacker j
  takes part), mixed as (1-w)*skip + w*fight per attacker. The DP
  transition is linear in the state, so this is the EXACT expectation
  over independent presence draws, not an approximation; w=1 is
  IEEE-bit-identical to the unweighted walk (unit-tested as `==`).
- `e6c765025b` `ai/deep: discount home-pinned enemy mass in the
  survival wave` — knob `threat_pin_discount` (double, default 0):
  an attacker the pin tracker currently classifies as home-pinned
  (live tag, never seen off its home tile) enters the wave at
  presence (1 - discount), at the single admission seam in
  `deep_city_survival_core()`. Deep's own cities only; the assault
  pricer's enemy-city reads keep full weight. Tracker classification
  untouched; its gates now also arm when the knob is on, because a
  FREECIV_DEEP_QUIET tuning run would otherwise sweep a knob reading
  an EMPTY tracker.

**Derivation of the on-arm value (measured, not chosen).** The probe
funnel (`cae6c6d5af`: 65 deep seats, 8 mirrored tech50war-200 seeds,
18,538 resolved windows) read 3.22% reach / 68.67% never-left /
28.11% sortied-but-never-arrived. The flat discount takes the
never-left share ONLY: 0.687, weight 0.313 = P(ever sorties). Not
1-0.0322 — the sortied-but-slow mass is survival_pf_reach's
population and must not be spent twice. `benchmark/tuning/
pv3-pindisc.ini` = pv3 + that one key.

**Doc regression found:** the pin-probe STATUS entry exists only in
`cae6c6d5af`'s version of this file; the `ee61b90394` merge's conflict
resolution dropped it. The funnel numbers above are re-quoted from
that commit.

### Byte-identity at default 0: 20/20, plus provenance both ways

git-archive build of `1844e16efc` (sha `446816f26742c688`, 0
`deep-pindisc` strings) vs branch build (sha `440e9aad426f5a82`, 1),
smoke, DEEP_QUIET=1, tuning none, jobs 5, ports 7705/7726:

    deep      scorelogs IDENTICAL 10/10
    mixed_dc  scorelogs IDENTICAL 10/10   (map hashes equal on all 20)

### The seam reaches the population (the brief's ship-gate)

Full T200 tech50war-200 game, knob 0.687, exit 0, no assertions.
Final `deep-pindisc` per deep seat: share of ALL admitted wave mass
discounted 0.242 / 0.243 / 0.251 / 0.469; e.g. P3 mass 2,326,266 of
wave 6,581,294 across 54,822 discounted admissions, 10,555 of 14,664
evaluations touched. Not ~0 by three orders of magnitude.

### A/B: tech50war-200 mixed, 16 mirrored seeds, one binary — WASH

Arms mixed_dc + mixed_cd per seed, ONE pinned binary
(`440e9aad426f5a82`, server + libfreeciv.so staged in `.pindisc-bin`),
control TUNING=pv3.ini vs on TUNING=pv3-pindisc.ini;
survival_pf_reach at its pv3 value (0) in BOTH arms. 64/64 games, 0
pairing violations; one control game (mixed_cd seed 11) ended T186 by
allied victory — verified genuine, kept. Deep seats, mirrored pair
averaged per seed:

    metric            OFF        ON     delta      se      t   wins
    score          195.38    194.34     -1.05    3.40  -0.31   8/16
    cities           4.94      4.89     -0.05    0.16  -0.30   7/16
    techs           54.82     54.66     -0.16    0.15  -1.05   4/16
    pop             29.62     29.01     -0.62    1.02  -0.61   7/16
    munits          17.79     17.71     -0.08    0.66  -0.12   8/16
    unitsbuilt      87.47     88.49     +1.02    1.83  +0.56   8/16
    unitslost       61.89     64.05     +2.16    1.64  +1.32   9/16
    unitskilled     40.19     41.14     +0.95    1.82  +0.52   8/16
    cities_lost      0.98      0.95     -0.03    0.13  -0.24   7/16
    garr_ndef        2.35      2.39     +0.04    0.10  +0.43   8/16
    pindisc share (ON): 0.357 of all wave mass; ~103,700
    discounted admissions per game

Classic seats: score -1.27, se 3.76, t -0.34 — the head-to-head
margin is unchanged too.

### Verdict, and what the wash teaches

- **Default stays 0 in pv3.** The knob is registered (struct tuning,
  params 0..1 float, blocks ALL+MIL, knobcensus predicate
  `deep-pindisc share>0`, tune-audit CLEAN at 237) and **flagged for
  the tuning pass** the brief mandates.
- The risk mode did NOT appear: cities_lost and garrisons are flat at
  a discount that removes 36% of the danger mass. Deep was not
  under-defending-fragile here.
- But neither did freed shields appear: garr_ndef unchanged says the
  danger-side discount barely moved the BUILD side on this venue —
  consistent with survival consumers acting only near their
  thresholds, and with the garrison floor binding below them. A
  smaller/complementary lever (or the tuner finding a non-0.687
  optimum) is where remaining value would be.
- Per the brief, the posture-conditional variant needed the flat one
  to move something. It did not: -1.05 +/- 3.40. That slice should
  not be built on this evidence.

Retention: both cells under `benchmark/results/netthreat2/`
(tech50war-200-86943cb3ce0f = pv3, -8846b69d19c1 = pv3-pindisc).

## Chores round: tune.py --arena + the owed nativity L0 (2026-08-14, tasks #44, #58)

Branch `epic/chores-arena-nativity`, 5 commits off `50b55eab00`. HYGIENE
round: no gameplay behaviour change anywhere; `meson test` 6/6 (new
suite `deepnativity`); tune-audit CLEAN and unchanged at 237.

### 1. tune.py --arena (PERPLAYER follow-on) — DONE

`tune.py --arena MANIFEST` drives the server's own FREECIV_DEEP_ARENA
layer: the manifest is read in `deeptuning.c`'s own format (no second
format defined), games run all-deep, and the report is per seat --
final score and in-game rank keyed by the file the seat played, plus
per-file means over all games. Cache dir is `arena-<key12>` where the
key hashes the WHOLE manifest content plus every named candidate
file's content: manifest edit moved the key `8c180b7c3c33 ->
6ba639b9b4a5` (miss, fresh games); an identical rerun completed from
cache in 0.6s. Smoke (hutrisk-80, seeds 1-2, ports 7900-7901, pinned
`stage-chores/` binary), pv3 vs identity on seats P1/P2:

    seat   score rank confirmed file        (seed 1 / seed 2)
    P1      53/46  4/4   yes    pv3.ini
    P2      69/73  3/3   yes    identity.ini
    per-file: identity mean 71.0 rank 3.00; pv3 mean 49.5 rank 4.00

2 games = mechanism proof, not a tuning claim (and hutrisk-80 is a
weird venue; do not read the pv3-vs-identity ordering as anything).

### 2. FOUND LIVE, needs human adjudication: derby-lean.ini and pv3-lean.ini have NEVER loaded

Both files (added 2026-08-13) set `floor_interior_weight` twice
(derby-lean lines 95 and 235). `secfile_load()` refuses the whole file
over a duplicate key, deep logs `cannot load tuning file ...; using
defaults`, and the seat plays the COMPILED DEFAULTS -- while the
deep-arena seat line still names the file. Sweep of all 96 archived
inis: exactly these two fail. **Any lean-arm games since 2026-08-13
(the lean-lattice A/B, the lean-army derby arm) actually ran the
defaults.** The archived inis were left untouched (they are artefacts);
guards added instead: `validate_tuning()` (arena.py, used by tune.py
--arena) refuses duplicate keys upfront, and the --arena report's
`confirmed` column requires the file's own deep-tuning log line,
reporting LOADFAIL when assignment happened but application did not.
NOTE: run.sh's TUNING pre-checks do NOT yet catch duplicate keys --
same defect class, not touched in this round.

### 3. Nativity helpers: moved to deepnativity.c + the owed L0 test — DONE

Pure move (verbatim) of deep_uclass_swims/walks +
deep_utype_is_sea/is_land/is_airborne/air_reach out of deepdefence.c
so they link against common/ alone. New suite `deepnativity` pins:
land/sea/both-native/lake-only classification from terrain CLASS, and
the UMT_SEA boundary from both sides -- the server-side move_type
field really is zero-init UMT_LAND for a floating class, and poisoning
it changes nothing. Mutation-verified: an is_sea that reads move_type
fails 5 checks. deep_utype_is_airborne left uncovered on purpose
(action-enabler subsystem has no L0 fixture); its nativity half is
pinned via the BOTH class.

### 4. Also found: game.log truncates the deep-tuning read-back line at 1024

`server/console.c` MAX_LEN_CONSOLE_LINE=1024 truncates every log line;
the deep-tuning line at 237 knobs is ~4.9KB, so only the first ~40
knobs are readable back from a game log. The hazard the line exists to
prevent (deeptuning.c's own comment) is only partly prevented. Flagged,
not fixed (out of scope for a chores round).

## 2026-08-14 — VOID: the lean arms never loaded; the lean evidence is defaults-vs-pv

The chores round's loader guard work surfaced it and the games' own
logs confirm it: `deep: cannot load tuning file pv3-lean.ini; using
defaults` (lean-ab/econ-lean seed-1 game.log; provenance sha pins the
same file for every seed). pv3-lean.ini, derby-lean.ini AND
derby6/tuning-derby6-lean.ini all defined the four floor keys twice
(append-over-existing), and secfile refuses a whole file over one
duplicate key.

Re-attributions:
- The banked lean-lattice verdict (war +8.2, econ +10.4, four
  analyses) measured COMPILED DEFAULTS vs pv3 — not the four zeros.
  The observations stand as descriptions of the defaults arm
  (era-slide halved, no retention cost, starve-the-rival); the
  four-zeros attribution is void.
- The derby-6 postmortem (+337 lean vs −809 std) measured compiled
  defaults vs pv2-personality on Europe. Norge's containment is real
  behavior of DEFAULTS on that map.
- The floor family's "fourth implication" is retracted (back to
  three); the MIL-finale fear sweep is otherwise STRENGTHENED — what
  these accidents actually measured is that the tuned pv line loses
  to compiled defaults on isolated-150, tech50war-mixed and Europe,
  implicating the tuned fear/floor values wholesale.

Fixes landed: the three inis deduped (intended overrides survive);
validate_tuning now refuses duplicate keys and tune.py --arena
reports LOADFAIL (chores merge). Open (human call, budget-gated): a
TRUE lean A/B — pv3 vs a loading pv3-lean — if the four-zeros
attribution is still wanted, and a defaults-vs-pv3 autopsy for which
tuned blocks are harmful.
## 2026-08-14 — DECAY slice 2: the scheduled discount at all three rectangles — late-game window moved, score lean +5.9 (t +1.7)

Brief: `sessions/S-DECAY2-schedule.md`. Branch `epic/decay2-schedule`
(base 9e485567be), six commits: `ab717c66c0` (register the five
schedule knobs; tune-audit CLEAN 242/242/242/242), `6b46448fa4`
(deep_decay_duration() + deep_decay_rate_now(), the ONE shared
helper), `1d0b7eff7c` (deephorizon's ten duration credits + the gold
upkeep bill through it), `881d6e0162` (deepgov.c's government
payoff), `01f9c21ec7` (deepdefence.c's unit-lifetime upkeep charge),
`1ed70ca608` (benchmark/tuning/pv3-decay.ini, the measured-anchor
arm).

Census claims verified by reading before wiring: deepgov.c:536
((cand-base) x deep_horizon_turns()) and deepdefence.c:7092 (upkeep x
deep_horizon_turns(), eta-truncated) are both genuine MORT rectangles
— no mismatch to report. The helper implements exactly the model
slice 1 validated offline (decay-census.py exp_value minus its
q^delay lead — delay stays with amortize(), so delay is discounted
once and duration once), bell-truncated from day one per slice 1's
56.5%-phantom-value finding; cap keeps the census convention
(structural caps survive, horizon-derived streams run to the bell;
the obsolescence ETA in deepdefence is now correctly a structural
cap). Rates are per-phase per the brief: early through break_start,
late from break_end, linear in turn between.

**Byte-identity at defaults: 20/20.** git-archive build of 9e485567be
(`a3ceb4ccb6…`, 0 `horizon_decay_gate` strings) vs branch build
(`1ef9ba732f…`, 3), smoke, DEEP_QUIET, tuning none, arms deep +
mixed_dc x 10 seeds each: scorelogs byte-identical 20/20, maphashes
equal, pairing violations 0.

**Per-site liveness knob-on (the brief's per-site gate).** Full T200
tech50war-200 games, exit 0, no new assertions (6 daisanity fires =
upstream bug #30, module-symmetric):
- deephorizon (pv3-decay game): 15,076 `deep-decay` shape lines +
  14,913 `deep-decay-sum` lines with rate>0; the rate histogram IS
  the schedule (10,920 at 0.03700, 11,811 at 0.00200, interpolated
  values between); durations arithmetically exact (veteran steps
  0.96 -> dur 1.637 = bell-truncated geometric at spacing 25).
- deepgov + deepdefence are DORMANT under pv3 (gov_valuation and
  unit_upkeep_charge are 0 there — parent mechanisms off, not a
  seam miss), so a diagnostic probe (pv3-decay + gov_valuation=1 +
  unit_upkeep_charge=1, the gov-on/m4b-upcharge probe values) proved
  them: deep-gov 924/924 lines rate>0 (dur 27.010 vs rectangle 24 at
  T5), deep-upcharge 2,323/2,323 rate>0 (dur 27.012, both eta-capped
  and horizon-derived paths observed).

### A/B: tech50war-200 mixed, 16 mirrored seeds, one binary — +5.9/seat, t +1.68, and the late window moved

Arms mixed_dc + mixed_cd per seed, ONE pinned binary (`1ef9ba732f`,
server + libfreeciv.so in `.decay2-bin`), control TUNING=pv3.ini vs
on TUNING=pv3-decay.ini (gate 1, early 0.037, late 0.002, break
50/100 — the measured anchors, documented in the arm file). 64/64
games, 0 pairing violations, all to T201. Deep seats, mirrored pair
averaged per seed:

    metric            OFF        ON     delta      se      t   wins
    score          189.88    195.79     +5.91    3.52  +1.68  11/16
    cities           4.77      4.89     +0.12    0.15  +0.82   9/16
    techs           54.41     54.62     +0.20    0.16  +1.23   9/16
    mfg             34.06     35.20     +1.13    1.65  +0.69  11/16
    pop             28.58     28.64     +0.06    1.04  +0.06   7/16
    munits          17.94     16.76     -1.18    0.83  -1.42   5/16
    unitsbuilt      89.49     86.37     -3.12    2.43  -1.29   8/16
    unitslost       65.44     63.94     -1.50    2.17  -0.69   7/16
    classic score  224.78    223.34     -1.45    3.54  -0.41   8/16

    per-seed deltas: +8.5 +11.6 +11.5 -9.8 -20.6 +9.1 -10.1 -5.9
                     -5.0 +22.5 +0.3 +9.4 +34.2 +20.0 +3.0 +15.9

Era-slide (deep share, mean over games — the knob's whole thesis is
this trajectory, not the final number):

    tag      arm    T50    T100   T150   T200
    techs    ctl   0.500  0.499  0.497  0.495
    techs    on    0.501  0.497  0.496  0.495
    mfg      ctl   0.489  0.456  0.456  0.444
    mfg      on    0.481  0.460  0.452  0.458
    cities   ctl   0.489  0.471  0.462  0.457
    cities   on    0.496  0.483  0.469  0.471

### Read

The measured-anchor schedule moved the late-game window in the
predicted direction: control's mfg share FALLS T150->T200
(0.456 -> 0.444) while the on-arm's RISES (0.452 -> 0.458); city
share ends +1.4pt (paired t +0.93); tech pace is identical — the
same "win by not collapsing" signature the lean-lattice derby found,
now produced by pricing alone. The composition shift is coherent:
fewer units built (-3.1), same tech, more cities and mfg — patient
late pricing redirects shields from military volume to economy, and
the classic seats do not profit from it (head-to-head margin flat).
At n=16 the +5.91 (t +1.68) is a LEAN, not a banked win — but it is
the right shape, on the venue the knob was predicted to need, from
anchors that were measured rather than tuned. Both rates and breaks
stay FLAGGED FOR THE TUNING PASS (the brief's placement: with the
extensive-tuning finale, since gate-on reprices a fifth of the
ladder at plausible rates). Default stays 0 everywhere; pv3
unchanged.

Retention: verdict banked; saves/ and game.logs pruned per policy
(scorelogs, DONE, provenance kept; the two instrument game.logs kept
compressed: on-arm seed-1 mixed_dc and the site-probe cell
tech50war-200-5db8b07a1040). Cells: -1d411042c95a = pv3,
-597936cbdd19 = pv3-decay, -5db8b07a1040 = site probe; identity pair
smoke-21323da53164 (base) / smoke-622c48474265 (branch).
## 2026-08-14 — TECHMIL slice 2: the techgoal seam lands, live and provably inert at stock's shape

Brief: `sessions/S-TECHMIL2-argmax.md`. 5 commits on
`epic/techmil2-argmax` (based at `1844e16efc`): e5c56d7694 knobs,
a397667d85 mechanism, 0c8c500c60 harness, 5ef8fe0ac8 counter honesty,
plus this entry.

### The seam, and what reading the code corrected in the brief

Deep reaches research management by calling stock's
`dai_manage_tech()` from `deep_do_last_activities()`; that call now
goes through `deep_manage_tech()` (new `ai/deep/deeptech.c`). Gate
`techgoal_deep` = 0 (default): the wrapper IS the stock call —
byte-identical. Gated on: stock runs first, unchanged (its
`dai_tech_effect_values()` writer and its committed picks, kept as the
counter's honesty fields), then deep re-selects BOTH the goal and the
researching advance from the same `tech_want[]` mass, in double
precision, with both of stock's distance divisions raised to
`techgoal_distance_power`. Selection copied per rule 3 (stated in
a397667d85): `dai_select_tech()` is static and the visibility
exception would only export the exponent frozen at 1.

The brief's formula sketch was incomplete in two load-bearing ways,
found by reading:

- Stock divides by distance **twice**: the per-prerequisite spread
  (`daitech.c:127`, want/steps) and the goal argmax (`:155`,
  path-mass/steps). Both carry the power.
- The goal is NOT "practically never used" (stale comment,
  `daitech.c:372`): `research_goal_step(tech_goal)` picks the next
  research at every completion (`techtools.c:505`, `:1176`). And the
  per-turn immediate `choose_tech()` can fight a goal-only override,
  so deep re-sets both, behind stock's own penalty-guard shape. Deep's
  goal write is LAST within each turn, so it is the goal in force in
  the inter-turn window where completions happen.
- **Upstream bug #49** (upstream-bugs.md): the newtech argmax admits
  `gettable(allow_holes=TRUE)` advances (root_reqs_known only) that
  `choose_tech()` then silently refuses (`techtools.c:1021`). Stock
  "changes research" into a no-op on **33% of player-turns** on this
  venue (measured via the liveness probe). Deep reproduces the attempt
  for shape fidelity; the counter's `switched=` reports the post-call
  state change, not the attempt (5ef8fe0ac8).

### Validation

- `meson test -C build` **5/5** at every commit; tune-audit **CLEAN**
  (238 fields in all four registries); knobcensus MAP entries, direct
  grade.
- **Identity: 12/12 byte-identical** (10 `deep` + 2 `mixed_dc` smoke
  seeds, compiled defaults) against a `git archive` build of
  `1844e16efc`, ports 7600-7615 — proven **on the final binary**
  (re-run after 5ef8fe0ac8). Base-is-really-base guard is binary-level
  (head contains the `deep-techgoal` format string, base must not),
  because at defaults neither side emits a discriminating line.
- Liveness: 2 tech50war-200 `mixed_dc` games, gate on — counter fires
  from turn 1, 1600/1600 gated player-turns.

### The A/B (gate at power = 1.0 vs off — the seam-isolation arm)

tech50war-200 mixed, 16 mirrored holdout seeds (101-116), 64 games,
`tune.py --mixed` (mixed_db/mixed_bd vs `ai/baseline`), ONE pinned
binary (sha256 `2833954…dafcb`, with libfreeciv.so), `identity.ini`
vs `techgoal-on.ini`. Both arms 32/32 DONE, every game at endturn.

    metric        OFF        ON       delta   se     wins
    margin      -259.50   -259.50    +0.00   0.00   0/16
    (deep 1539.94 / stock 1799.44, cityratio 0.80 — both arms)

    per-seed margin (identical in both arms), mean -259.5, sd 193.7,
    sem 48.4:
    101:+0  102:-399  103:-405  104:-27   105:-309  106:-286
    107:-544 108:-254 109:-249  110:-460  111:-290  112:+194
    113:-503 114:-241 115:-199  116:-180

**All 32 game pairs are BYTE-IDENTICAL between arms** (cmp on every
scorelog), while the on arm's logs carry **25,600** `deep-techgoal`
lines. That is the strongest possible reading of "isolate the seam":
gate-on at stock's shape is an in-vivo no-op, not merely
score-neutral.

Counter, on arm: **goals differ 0.6%** of gated turns (float-vs-int
drift of the double-precision copy against stock's int truncation),
**researching moved 0.0%**, switch attempts refused as in stock (#49).
None of the 0.6% goal differences ever met a completion, so no game
diverged.

Military-research share (deep seats, n=25,057 player-turns/arm):
**57.1% OFF vs 57.1% ON — 0.0pp shift**, exactly as the byte-identity
requires. (Not comparable 1:1 with slice 1's 47.5%: that corpus was
`mixed_dc/cd` vs classic; this venue seats `ai/baseline`, whose array
the census cannot read, so no in-corpus stock reference exists here.)

### Verdict and what is owed

The slice's deliverable was the SEAM, and the seam is proven: live
(25,600 firings), in control of the decision (last writer each turn),
and inert at the default and at stock's shape. **The knob stays 0.**
No claim is made that any power value helps — that measurement does
not exist yet.

**The power tune is owed**, exactly as the brief anticipated:
`techgoal_distance_power` sweep (registered bounds 0.0-3.0, S2/MIL
block) on training seeds, reported on holdout. It should sweep the
power alone at gate on; venues: tech50war-200 AND a from-scratch cell
(primary-120) — this venue starts at techlevel 50 and slice 1 already
flagged that the early tree weights everything differently. The
mechanism hypothesis to test is directional: power < 1 lengthens
deep's sight and should raise realised military share toward the
61.0%-share appetite; the exact-zero at 1.0 means any observed move
in that sweep is the power's own effect, with no seam artefact mixed
in.

Retention: techmil2-ab scorelogs, DONE, provenance and the on-arm
game.logs (the counter corpus the power tune will want) kept;
identity/live cells kept as proof artifacts. Nothing pruned yet.

## 2026-08-14 — TRUE lean A/B: the four zeros are a WASH; the lean legend is fully closed

With pv3-lean.ini actually loading (deduped; load confirmed absent of
the cannot-load line), pv3 vs pv3+four-zeros on the two voided venues,
16 holdout seeds each, one binary 1237ba9c6e (.leanab2-bin):

- isolated-150: lean-minus-pv3 +1.72/seat, sem 3.68, t +0.47 (6/16)
- tech50war-200: lean-minus-pv3 −3.57/seat, sem 4.27, t −0.84 (5/16)

Both null. The four floor zeros do approximately nothing on benchmark
venues. Combined with the voiding entry above: the original "lean
lattice" gains (+8.2/+10.4) belonged to compiled-defaults-vs-pv3, not
to the zeros. What remains true and actionable: DEFAULTS beat pv3 on
these venues in those runs — the tuned fear/floor values as a package
are implicated, and the honest attribution now runs through the
finale's conditional (interior-vs-frontier) sweep, not through
hand-picked zeros. derby-7's ini keeps the zeros as exhibition spice
only.

## 2026-08-14 — Derby-7 (spicy): deep still loses Europe, but the gap narrows ~3.5x

Full T300 exhibition on the europe.serv roster, binary 1237ba9c6e,
tuning derby5/tuning-derby7.ini (fence + lean zeros + berth + measured
decay anchors + the dormant-mechanism block, all confirmed loading).

Final per-seat means: deep 155 vs classic 250 (diff −95). Top board:
Italia 341, Polska 341, Norge 309, Iberia 277, then Osmanli 233 as
best deep. No runaway seat this time — the best classic is 341, not
derby-5's 213-city Norge; and the worst deep (Carthago 100) survived
to T300, unlike prior derbies' eliminations.

Context: derby-4 (pv2-personality) lost −334/seat at T300; derby-5
(std) was killed at −809. −95 with the spicy arm is the smallest
Europe deficit yet recorded, with containment of the runaway pattern.
Caveats stack high: one seed, exhibition format, and the spicy block
conflates ~20 changes — this ranks PRIORITY for the finale sweep, it
attributes nothing. The two knobs with independent positive evidence
in the block remain the decay schedule (+5.9 benchmark) and reach
(pv1 +71.9).

## W12 slice 2: the task source past the radius ceiling — live, consuming the hidden demand, score NULL (2026-08-14)

Branch `epic/w12b-task-source`, 3 commits, merged into
`epic/w03-horizon-content`. Slice 1 (merged `d075c4bfc4`) named the
gap and predicted its own census would show it: the worker task
SOURCE is capped by the advisor's radius ceiling
(`autoworkers.c:586-588`), so corridor tiles between distant cities
generate no task at all and slice 1's locality optimised a truncated
demand set. `links>0 hits=0` was that ceiling speaking. This slice
opens the source; it does not change what wins once the demand set is
complete.

### 1. What landed

`deep_trunk_beyond_tiles()` (`ai/deep/deeptrunk.c`) exposes the
corridor tiles the plan already computes but the advisor never
enumerates. `deep_trunk_source_task()` (`ai/deep/deepsettler.c`) runs
beside the auction inside deep's own `settler_run`, prices each with
the same `deep_trunk_price()` the task-value hook uses, times
`trunk_source_gain`, then applies slice 1's OWN walk discount —
`deep_worker_travel_amortized()`, extracted from
`deep_worker_travel_price()` so there is one discount with two
callers, not a second one invented here — and amortizes over
completion exactly as `autoworkers.c:941` amortizes its champion. A
corridor tile must survive both discounts to beat work at home:
locality stays the arbiter, the demand set is what changed.

No `server/advisors/` change. The source is deep-side only, so slice
1's 20/20 stock-arm hook proof is not re-owed.

### 2. Knob, PLACEHOLDER, zero

    trunk_source_gain  0.0  OFF = today's ceiling, byte-identically.
                            1.0 is the unit case: a beyond-ceiling
                            corridor tile is worth exactly what the
                            same corridor is worth inside a radius.
                            Needs trunk_road_gain > 0 for a plan.

Four audit places, the G/ECON/ALL blocks, knobcensus MAP
(`deep-trunksrc picked>0`, direct grade). `tune-audit` CLEAN at 238
fields (base 237 + this slice's 1). `meson test` 5/5.

### 3. Identity: 20/20 byte-identical at the shipping zero

Base binary = `git archive 50b55eab00` built in `.w12b-base/`
(sha `b43e520a82021a26`); branch binary sha `cad20ef467690787`. Ten
paired smoke seeds per arm, `benchmark/results/w12b-identity/`.

    deep      scorelogs IDENTICAL 10/10
    mixed_db  scorelogs IDENTICAL 10/10

The mixed cell is included because the source runs inside deep's own
`settler_run` and the mixed game is where a leak into shared advisor
state would show. The base arm emits no `deep-trunksrc` line, so the
proof cannot have passed by running HEAD twice.

### 4. Liveness, then the A/B

Liveness (`benchmark/results/w12b-live/`, 2 lategame-80 games with
the source on): `deep-trunksrc` reads `picked>0` on most player-turns
and `deep-trunkpick` shows real assignments out to `dist=8`. Not a
knob-on run with zero beyond-ceiling tasks, which the brief called a
miss to report rather than ship.

A/B: 16 mirrored lategame-80 seeds, one pinned binary
(`.w12b-pin/ab`, sha `cad20ef467690787`), ports 7830-7849, jobs 4.
ON = `tuning/w12-source-on.ini`, OFF = `tuning/w12-on.ini` — the two
files differ in exactly one key, so this reads the source alone
against slice 1's probe background. All 32 games reached T81; 0
pairing violations. `benchmark/results/w12b-ab/`.

    metric              OFF        ON     delta      se      t   wins
    score            167.38    166.05     -1.32    1.56  -0.85   6/16
    cities             7.76      8.19     +0.43    0.25  +1.71  10/16
    landarea       92458.33  95947.92  +3489.58 2120.31  +1.65  10/16
    pop               40.61     41.51     +0.90    0.93  +0.96  10/16
    techs             52.89     52.81     -0.07    0.11  -0.64   4/16
    mean_city_size     5.36      5.26     -0.11    0.10  -1.12   7/16

    trunksrc-active    0.00     17.81    +17.81    4.30  +4.14  15/16
    trunk-picks        0.00     48.50    +48.50   15.04  +3.22  15/16
    trunksrc-turns    67.38     39.38    -28.00   13.96  -2.01   6/16

Counters summed over each arm's 16 games:

                        OFF        ON
    beyond=            1729      1275   demand the ceiling hides
    offered=              0      1587   priced by the source
    picked=               0       776   assigned beyond the ceiling
    trunk links=      15981     16634
    trunk hits=      487957    524354

### 5. What the numbers say, and what they do not

**The mechanism is causal and gated.** `offered`/`picked` are exactly
0 in the OFF arm and 1587/776 in the ON arm, 15/16 seeds. 776
beyond-ceiling worker tasks were assigned across 16 games that could
not have existed before this slice. `beyond=` fires in BOTH arms
(1729 vs 1275) exactly as the knobcensus entry says it should: the
hidden demand is a property of the plan, not of the gate. That the ON
arm sees LESS of it, on fewer emitting turns (39.4 vs 67.4 per game,
t -2.01), is the demand being consumed — corridor tiles that get
roads stop being gap tiles.

**The score effect is null.** -1.32 +/- 1.56, t -0.85, 6/16. Not a
regression to explain away and not a win: it is a wash on this venue
at the unit case. `cities` +0.43 (t 1.71) and `landarea` +3490
(t 1.65) lean positive and are the only shape in the table, but
neither clears noise on 16 seeds and neither is being claimed.

**The commuting cost is real and small.** Slice 1's walking-distance
instrument, the regression watch this slice owed:

    walker unit-turns   8918 (off)  ->  9547 (on)   +7.1%
    mean remaining walk 3.206       ->  3.507       +9.4%

Workers do walk farther when farther jobs exist to be walked to. That
is the intended trade, not a bug — but it is a cost, it is measurable,
and at `trunk_source_gain = 1.0` it buys no score. Both discounts are
in the path; neither has ever been tuned.

**Therefore the knob ships at 0.0** and this epic bought mechanism,
instruments and one honest null — the same shape slice 1 shipped in.
The next legitimate move is a tuning pass over
`trunk_source_gain` WITH `worker_travel_discount` and
`trunk_road_gain` (they interact through one discount and one plan;
CLAUDE.md's "parallelise scheduling, not attribution" says they want
one factorial cell, not three arms), not another mechanism slice.

Retention: `benchmark/results/w12b-ab/{on,off}/lategame-80-*`,
identity under `benchmark/results/w12b-identity/`.

## Observed in live play: deep fortresses saturate the city catchment (2026-08-15)

Not benchmarked -- recorded from a europe.serv game (deep playing Deep_Osmanli)
so the next session does not rediscover it. **No code changed.**

Symptom: deep builds fortresses on essentially every tile of a city's work
radius.

Why this is structural rather than a tuning slip. The base want
(`ai/deep/deepsettler.c:2238-2260`) prices a fortress as defender survival times
how much the tile matters. Its own "bounded scope" note records that the advisor
only ever offers candidate tiles inside some city's work radius, framed there as
a limitation -- deep can never fortify open ground between empires. The flip
side went unstated: it means *every* fortress deep is able to want lands inside
its own catchment.

Two costs are absent from the want:

1. A fortress is territory-claiming. An enemy unit standing in one flips tile
   ownership, so the city loses that tile from its worked set while the enemy
   sits there.
2. It is a defensive anchor for whoever holds it. An attacker who takes the tile
   is harder to counterattack -- deep has prepaid the attacker's defence inside
   its own economy.

The incentive is inverted precisely where it matters: the want scales with how
much the tile matters, and so does the damage when an enemy occupies it. Deep
wants fortresses hardest on exactly the tiles that hurt most to lose.

Before touching the want, instrument (fortresses built per game; turns a tile is
denied to its city by enemy occupation of a deep-built fortress) rather than
guessing a penalty term -- two guessed fixes on this project were reverted and
the measured one won. Any fix is its own epic with its own A/B.

## DECAYTECH tune closed: three-way holdout — tuned edge is real but outlier-carried (2026-08-21)

Campaign close-out. The tune (benchmark/results/decaytech-tune, .leanab2-bin
binary 1237ba9c6e) died with the box's power on 2026-08-16 mid-round-5; rounds
1-4 had all closed flat at delta +0.0 (~20 generations never beat the round-0
incumbent), so it was cut rather than resumed. Incumbent, now staged at
benchmark/tuning/decaytech-best.ini:

    horizon_decay_early = 0.057058    horizon_decay_late = 0.037030
    horizon_decay_break_start = 22    horizon_decay_break_end = 163
    techgoal_distance_power = 0.940560

Three-way holdout: tech50war-200, holdout seeds 101-116, one arm per tuning,
same pinned binary, sequential (benchmark/results/decaytech-holdout/).
Objective per seed = candidate game total score minus baseline-arm total.

    seed    tuned   anchor      pv3  tun-pv3  anc-pv3
     101      -60     -128     -129       69        1
     102      -74     -122     -138       64       16
     103     -174     -157      -29     -145     -128
     104      787     -129      -20      807     -109
     105     -250     -194     -163      -87      -31
     106     -289     -138     -214      -75       76
     107     -266     -150     -195      -71       45
     108      -69      -45     -156       87      111
     109     -149     -130      -94      -55      -36
     110     -246     -304     -324       78       20
     111      -98      -62       27     -125      -89
     112     -139      -69     -156       17       87
     113      -33     -128     -179      146       51
     114     -154     -182     -214       60       32
     115      -63     -269     -123       60     -146
     116       -6     -167      -49       43     -118
    mean    -80.2   -148.4   -134.8     54.6    -13.6

(arm means reproduce tune.py's own HOLDOUT report lines exactly.)

Reading it honestly:

- **Tuned vs pv3: +54.6 mean, but seed 104 alone is +807** (a runaway win
  game; the other two arms sat at -129/-20 on the same seed). Drop that seed
  and the mean advantage is **+4.4 over 15 seeds** — noise. Sign test 10/16
  (one-sided p ~ 0.23), median +62. Verdict: suggestive, not conclusive.
  What CAN be said: the tuned schedule does not hurt, and it occasionally
  enables a runaway that pv3 does not reach.
- **Hand-picked anchors vs pv3: -13.6, 9/16** — a wash-to-slight-loss. The
  guessed decay constants (early=0.037/late=0.002/break 50-100) did NOT
  transfer; the optimizer's reshaping (higher late rate, much earlier break
  start, wider window) is what rescued the mechanism. Guessing constants
  loses again; rule 7 vindicated again.
- **Training -50.5 vs holdout -80.2**: ~30 points of optimizer optimism,
  the expected regression toward the mean.

Adoption: NOT crowning decaytech-best as the new pv default on this evidence.
The standing defaults-vs-pv3 autopsy (compiled defaults beat pv3 on three
venues, 2026-08-15 entry) is still the bigger question — any new default
should be decided after it. decaytech-best.ini stays staged as the best-known
DECAYTECH configuration and the natural donor for the Europe spicy ini's
decay block.

Retention: benchmark/results/decaytech-holdout/{tuned,anchor,pv3}/ scorelogs
+ ARM-DONE markers; per-seed script in session scratchpad (holdout3way.py,
logic recorded here: sum final tag-25 over all 8 seats minus baseline total).
decaytech-tune/ r0-r4 logs stay for the tune trace; r5 partials disposable.

## S-FORT: the catchment fortress carpet, instrumented and measured (2026-08-21)

Follow-up to the 2026-08-15 live-play entry (`d3d6ad6`), which recorded deep
carpeting its city catchments with fortresses and said to instrument before
touching the want. This epic adds counters ONLY. **No want and no behaviour
changed**, and that is proved below rather than asserted.

### What landed

Three log-only lines in `ai/deep`, all written under
`log_do_output_for_level(deep_metric_log_level())`, none read back into a
decision:

| line | answers |
|---|---|
| `deep-fortwant` | whole-game positive base wants (n, min/p25/p50/p75/max/mean from a histogram), base tasks `assigned`, base tasks `finished` |
| `deep-fortsat` | per turn: catchment `tiles`/`bases`, `worked`/`worked_bases`, `held_anybuilder`, `held_by_enemy`, `held_unworked`, cumulative `denial_tileturns` |
| `deep-workturn` | worker-turns by class, cumulative: base/road/irrig/mine/transform/clean/travel/idle/other |

Two limits are in the field names on purpose. Extras record no builder, so
`bases` counts every EC_BASE extra in the radius -- ours, an enemy's, an era's
airbase -- and `held_anybuilder` counts a foreign unit standing in ANY of
them. Both are upper bounds on "deep's own fortress". `finished` likewise
means "assignment whose tile later carried a base", not proof of authorship.

### The counters do not perturb play

`smoke` (4p 32x32 T100 civ2civ3), arm `mixed_dc`, 10 seeds, instrumented
binary vs base branch `31b947ffc7`: **scorelog.txt byte-identical on 10/10**
(`cmp -s`: identical=10 differ=0). The two result keys differ only because the
binary hash is part of the key (`smoke-6cbbbbc94b05` instrumented,
`smoke-3b82df250b61` base).

`meson test -C build` 6/6 OK. `tune-audit.py` CLEAN (245/245/245/245).

### Venue and cells

`contested` (8p 32x32 T150 civ2civ3), 16 seeds of `seeds/train.txt`. Chosen
because the symptom venue was `europe.serv` -- deep and stock seats at one
table on a crowded map with contact from the first turns -- and `contested` is
the canonical portfolio cell for exactly that.

- **Cell A, compiled defaults**: arms `mixed_dc` + `mixed_cd`, 32 games, 126
  deep player-games. No `deep-tuning` line in any game.log, which is the
  read-back proof the arm ran defaults.
- **Cell B, `derby5/tuning-derby7.ini`** (the ini the live game ran, via
  `play-europe.sh`): arm `mixed_dc`, 16 games, 64 deep player-games.
  Read-back confirmed in game.log: `base_fort_gain=15.604`,
  `worker_danger_discount=3.15354`, `site_choke_bonus=6`.

### Distributions

Per deep player-game, at game end (peak-over-game is within 0.002 of end on
both cells -- the carpet is monotonic, it never gets un-built):

| metric | cell A (defaults, n=126) | cell B (derby-7, n=64) |
|---|---|---|
| catchment base share: med [p75] p90 max | 0.067 [0.156] 0.262 **0.579** | 0.053 [0.167] 0.289 **0.526** |
| worked-tile base share: med [p75] p90 max | 0.093 [0.227] 0.398 **0.714** | 0.067 [0.239] 0.350 **0.591** |
| base share of *working* worker-turns | 0.080 [0.144] 0.211 0.290 | 0.073 [0.133] 0.194 0.258 |
| base tasks assigned | 8 [16] 33.5 71 | 6 [21] 34.5 74 |
| base tasks finished | 6 [12] 25.5 54 | 4 [13] 27.7 45 |
| denial tile-turns | 0 [0] 2 **20** | 0 [0] 0 **23** |
| positive base wants quoted | 5213 [12483] 25860 46702 | 2594 [9692] 21290 38791 |
| want p50 (median over player-games) | **6.50** | **1.50** |
| want max (median over player-games) | **13.58** | **5.30** |

Pooled: cell A `bases/tiles = 1046/10602 = 9.9%`,
`worked_bases/worked = 591/3811 = 15.5%`, base worker-turns
`4241/38931 = 10.9%` of working turns, `assigned=1557 finished=1141`,
`denial_tileturns=82`. Cell B `540/5054 = 10.7%`, `252/1703 = 14.8%`,
`2153/18779 = 11.5%`, `assigned=851 finished=578`, `denial_tileturns=58`.

Tail fractions (player-games at or above the threshold):

| threshold | cell A | cell B |
|---|---|---|
| catchment share >= 25% | 14/126 (11%) | 9/64 (14%) |
| worked-tile share >= 25% | 29/126 (23%) | 15/64 (23%) |
| base share of working worker-turns >= 15% | 30/126 (24%) | 15/64 (23%) |
| any denial at all (>0 tile-turns) | 22/126 (17%) | 6/64 (9%) |

### Verdict: it reproduces at benchmark tunings, and the spicy ini is not the cause

The symptom is real at compiled defaults on a contact venue. It is not the
median case -- half of deep's player-games put a base on under 7% of their
catchment -- but the upper quarter is the carpet the live game showed: **23%
of player-games have a base on a quarter or more of the tiles their citizens
actually work, the worst reaching 71%**, and 24% of player-games spend a sixth
or more of their productive worker-turns on it.

**The surprise is cell B.** The derby-7 ini prices a fortress at
`base_fort_gain = 15.6` against the compiled `40.0` -- 2.6x *cheaper*, not
spicier, which was the opposite of this session's expectation going in. The
want distribution moves exactly as the price says: median want p50 falls
6.50 -> 1.50, median want max 13.58 -> 5.30, roughly 4x down. **The behaviour
does not move at all.** Catchment share, worked share, worker-turn share and
the tail fractions are all within noise of cell A, and the pooled base
worker-turn share is if anything slightly *higher* (11.5% vs 10.9%).

That is the finding that should drive the fix epic: **the carpet is not
price-sensitive, so lowering `base_fort_gain` will not remove it.** The base
want does not clear a bar; it competes against whatever else the advisor
offers that worker on that turn. Once a city's improvement backlog is
exhausted, a want of 1.5 wins as easily as a want of 13.5, because the
alternative is zero. The single-player trace makes this concrete
(cell A, seed-9 P7, a 2-city empire): bases in catchment 0 at T77, 6 at T102,
14 at T127, 22 at T143; base worker-turns 0 / 10 / 37 / 107 over the same
points, so **50% of all working worker-turns from T127 to T150 went into
fortresses** (base +70 of +141 working turns) after irrigation had plateaued
(+6) .

A second mechanism compounds it and is worth stating because it is in the
source as a deliberate choice (`deepsettler.c`, `deep_worker_task_value`):
the base want is added AFTER the danger discount and is explicitly exempt
from it, while every ordinary improvement on an exposed tile is divided by
`1 + worker_danger_discount`. At the default 1.0 that halves the competition;
under derby-7's 3.15 it quarters it. So the ini that made fortresses 2.6x
cheaper simultaneously made everything they compete with ~2x cheaper still --
which is a plausible reading of why the price cut did not show up in the
behaviour. Cell B cannot separate those two, and this entry does not claim it
does.

The **denial cost is not the problem at this venue**. Median 0 tile-turns,
83% of cell-A player-games never see it at all, pooled 82 tile-turns across
126 player-games. The 2026-08-15 entry named two absent costs -- denial and
prepaid attacker defence -- and the measured one is neither: it is the plain
opportunity cost of the worker-turns. A fix designed around a denial penalty
would be tuning against a term worth ~0.65 tile-turns per player-game.

### What the fix epic has to beat

Price range a competing term must outbid, from the want histograms
(cell A, pooled shape): positive base wants run from ~0 to a hard ceiling of
13.58 with median 6.50 -- the ceiling is `base_fort_gain` x survival gain x
ground weight, and ground weight saturates near 1.0 on a 32x32 map because
`density_fear_radius = 8` puts an enemy city within reach of nearly every
tile. On roomier venues the same want should be much smaller; this entry
measures one crowded venue and says nothing about `primary-120`.

Design consequences, all falsifiable by the fix epic's own A/B:
1. A price cut is not a fix (cell B).
2. A denial penalty is not a fix (denial is ~0 at 150 turns).
3. The lever that matches the data is a **budget or a saturation cap** -- some
   term that knows how many of a city's tiles already carry a base -- or a
   competing want that keeps the backlog non-empty so the fortress stops
   winning by default.

Any of those is its own epic with its own A/B. Nothing was changed here.

### Retention

`benchmark/results/fort-identity/` (the byte-identity pair),
`benchmark/results/fort-measure/contested-1719c9b22012/` (cell A, 32 games),
`benchmark/results/fort-derby7/contested-c6d583ff575c/` (cell B, 16 games).
Analysis scripts were session-scratch; the counter lines are self-describing
key=value and the last line per (game, player) is that game's total.
## W12 slice 3: allied cities as trunk endpoints — live in a STANDARD venue, not only a scripted one (2026-08-21)

Branch `epic/w12c-ally-corridors`, 7 commits, based at `31b947ffc7`
(`epic/w03-horizon-content`). `ai/deep` + `benchmark` + one tuning
file; zero `ai/default`, `ai/baseline` or `server/advisors` changes.

Slice 1 (`d075c4bfc4`) built the corridor plan and slice 2
(`e0dc5f193b`) lifted the advisor's radius ceiling — but the node set
in `ai/deep/deeptrunk.c` was `pplayer->cities` and nothing else, so
the network could never leave the shape of deep's own empire. A road
to the ally next door was exactly as invisible after W12 as any
connecting road was before it. Live play had already shown the
consequence (STATUS 2026-08-15, fortress saturation): worker turns
into catchment fortresses while the road to the human ally went
unbuilt.

### 1. What landed

| # | Commit | What |
|---|---|---|
| 1 | `9648bce65d` | the gate: allied cities in the MST node set |
| 2 | `c79c82836e` | `deep-trunkally`, the ally corridors' own census line |
| 3 | `22aa581d0a` | `w12c-ally.sh` + `w12c-roadcheck.py`, the scripted-alliance venue |
| 4 | `8b04422173` | `trunk_ally_gate` in `knobcensus.py` |
| 5 | `a55ae73071` | the Europe rider in `derby5/tuning-derby7.ini` |
| 6 | `3c2d54f8b2` | the A/B stage the venue answer licensed |
| 7 | this entry | |

`deep_trunk_ally_nodes()` appends ONE node per allied player: that
ally's city nearest by map distance to any of our own, on a continent
we have a city on. One and not the whole allied city list — n allied
cities in the node set would let Prim spend edges inside the ally's
empire, which the ally's own workers already cover, and dilute the one
link that matters. Ally nodes are LEAVES: `deep_trunk_build()` skips
the Prim relaxation from an ally, so a foreign capital can never
become the hub between two of our own cities, and every ally link's
direction is fixed own -> ally, which is what the border clip below
needs in order to mean "as far toward the ally as a worker can go".

Locality stays the arbiter. Ally corridor tiles enter the existing
beyond-ceiling set and are priced by the same `deep_trunk_price()` and
discounted by the same `deep_worker_travel_amortized()` as every other
corridor tile. No second discount and no new magnitude — one gate.

### 2. The buildability law, checked before the design

The brief demanded this be established rather than assumed. It splits
cleanly in two, and the interesting half is not the one the design
started from.

**Building is not restricted by tile ownership** in the classic
ruleset. `enabler_road` (`data/classic/actions.ruleset:1066-1074`)
requires `UnitTypeFlag "Workers"` on the actor and `TerrainAlter
"CanRoad"` on the target tile — no `DiplRel`, no owner requirement.
The Road extra's own reqs (`terrain.ruleset:1603-1607`) are the
Workers flag and a Land tile. And `ACTRES_ROAD` sits in the "no hard
requirements" case list at all three places `common/actions.c`
enumerates them (2569, 2767, 4314). A worker may legally road an
unowned tile and an allied-owned tile.

**Reaching the tile is what binds.** Both worker pathfinders
`TB_IGNORE` every tile whose owner is not allied —
`autoworker_tile_behavior()` (`autoworkers.c:521-535`, comment: "Don't
enter in enemy territories") and deep's own
`deep_settler_tile_behavior()` (`deepsettler.c:99-114`). Separately
`players_non_invade()` (`common/player.c:1434`) bars movement into a
PEACE partner's borders — but only for non-`UTYF_CIVILIAN` units
(`common/movement.c:775`), and workers are civilians, so peace is not
what stops them; the pathfinder callback is.

Consequence for the design: unowned and allied ground is both legal
and reachable, a third party's ground is neither in practice, and
`deep_trunk_walk()` therefore clips an ally corridor at the first
non-allied-owned tile. The clip is passed only for ally links, so
slice 1's own-empire links are untouched and the gate-off graph is
unchanged by construction.

### 3. Knob, PLACEHOLDER, zero

    trunk_ally_gate  0  OFF = the slice-1/2 graph exactly, byte-
                        identically. 1 = allied cities join the node
                        set. Geometry, not a magnitude, so 0/1 like
                        boat_water_gate. Needs trunk_road_gain > 0 for
                        a plan to exist at all, and trunk_source_gain
                        > 0 for a worker to be dispatched down a
                        corridor that begins outside every own city
                        radius (which is nearly all of one).

Four audit places, the G/ECON/ALL blocks, `knobcensus.py` MAP entry
(`deep-trunkally links>0`, gate grade, direct — carrying the one
caveat no other entry needs: the predicate reads zero when deep simply
had no ALLY, so it evidences the venue as well as the knob).
`tune-audit` CLEAN at 246 fields (base 245 + this slice's 1).
`meson test -C build` 6/6.

### 4. Identity: 10/10 byte-identical at the shipping zero, both cells

Base = `git archive 31b947ffc7` built in `.w12c-base/`
(sha256 `4802958e0b7f93d7`); branch binary `7eeecc63dbdcade0`, pinned
with `libfreeciv.so` per stage. Ten paired smoke seeds per arm at
`tuning/identity.ini`, `benchmark/results/w12c-identity/`:

    deep      scorelogs IDENTICAL 10/10   (0 differ, 0 missing)
    mixed_db  scorelogs IDENTICAL 10/10   (0 differ, 0 missing)

The mixed cell is included because the plan is consumed inside deep's
own `settler_run` and a mixed game is where a leak into shared advisor
state would show. Provenance guard: the base arm's logs carry no
`deep-trunkally` and no `ally=` token, so the proof cannot have passed
by running HEAD twice.

### 5. The venue question answered the other way

The brief expected no benchmark venue to form alliances and told me to
fall back on a scripted one. **The standard `lategame-80` cell forms
them.** Four seeds at `tuning/w12-ally-on.ini`,
`benchmark/results/w12c-venue/`, 249 `deep-trunkally` lines:

    seed   allylines  maxlinks  gap(sum)  offered  picked
    1        105         2        223       41       29
    2          0         0          0        0        0
    3        116         2        198       13       12
    4         28         1         32        0        0

Three of four seeds plan a corridor to an allied city; two of four
assign tasks on one. So deep-deep alliances do form inside 80 turns
and the mechanism runs on the primary path without any scripting.

The scripted smoke was run anyway, because it is the venue that can be
relied on rather than hoped for:
`benchmark/w12c-ally.sh smoke` seats two deep players on one team
(`team P1 "Team 1"` — DS_TEAM from turn 0, which `pplayers_allied()`
answers TRUE for, `srv_main.c:3481-3492`) with `startpos ALL` so they
share a continent. Three seeds, 80 turns,
`benchmark/results/w12c-smoke/`:

    seed   deep-trunkally lines   ally=1 picks
    1              53                  10
    2              42                   0
    3             123                  13

### 6. The road on the ground

Counters say what deep planned and assigned. `w12c-roadcheck.py` reads
the FINAL SAVEGAME's extras and owner grids and asks what is standing
there. Two confirmed roads on ally-corridor task tiles OUTSIDE the
deep player's own borders:

    smoke seed-1  P2 tile (23,10)  road=yes  owner=p0  (its ALLY)
    venue seed-1  P1 tile (32,14)  road=yes  owner=p2  (its ALLY)

Both are tiles no own city radius covers and no pre-slice-3 corridor
could have reached. That is the liveness claim in the brief, met.

It is also honestly small. Across seven games the distinct ally
corridor tiles ever worked number in the low single digits per player
— 29 picks in venue seed-1 are the SAME two tiles re-picked turn after
turn — and most ally links are short (`gap=1` on many turns), because
an ally's border is usually close to ours and the corridor between two
adjacent empires is a few tiles long. The mechanism does what it says;
what it says is worth a handful of worker-turns per game in these
cells, not a strategy.

### 7. Score A/B: mechanism strongly live, score a WASH

16 mirrored `lategame-80` seeds, one binary, arms differing in exactly
one key (`tuning/w12-source-on.ini` vs the same plus
`trunk_ally_gate = 1`), `benchmark/results/w12c-ab/`,
`benchmark/paircmp.py`:

    paired seeds: 16  (off 16, on 16)
        metric                   OFF        ON      delta       se      t   wins
        score                 166.05    166.70      +0.65     0.98  +0.66   6/16
        cities                  8.19      7.98      -0.21     0.13  -1.60   3/16
        techs                  52.81     52.76      -0.05     0.10  -0.51   2/16
        pop                    41.51     41.05      -0.46     0.35  -1.30   3/16
        mean_city_size          5.26      5.28      +0.02     0.04  +0.52   7/16
        landarea            95947.92  94010.42   -1937.50  1138.94  -1.70   4/16
        unitsbuilt             40.21     39.69      -0.52     0.32  -1.61   3/16

        ally-turns              0.00     80.19     +80.19    16.45  +4.87  14/16
        ally-active             0.00      4.06      +4.06     1.26  +3.23   9/16
        ally-picks              0.00     10.69     +10.69     3.13  +3.42   9/16
        trunk-picks            48.50     56.81      +8.31     9.53  +0.87   8/16

Read it in that order. The execution counters are unambiguous and the
knob's own predicate is exactly zero in the OFF arm, which is the
off-switch proof restated on the A/B's own data: 80 player-turns per
game plan an ally corridor, 4 of them price one, 10.7 tasks per game
are assigned on one, and 14 of 16 seeds show the mechanism running.
The score does not move: +0.65 on 166, t +0.66, 6/16 wins. **NULL, and
reported as null.**

The three counters that lean negative — cities −0.21 (t −1.60),
landarea −1938 (t −1.70), unitsbuilt −0.52 (t −1.61) — are the shape
you would predict if worker turns were being spent outward instead of
at home, and none of them clears significance at n=16. They are worth
naming rather than burying because they point the same way and the
mechanism is the plausible cause; they are not worth acting on at this
power.

Honest limit on the design: `trunk-picks` (all beyond-ceiling picks,
both arms) rose +8.31 with se 9.53, so the ally picks are not purely
additional work — some of them displace own-empire corridor picks the
OFF arm would have made. That is locality doing its job, and it also
means the knob buys a REALLOCATION of worker turns, which is exactly
the kind of change 16 seeds cannot price.

### 8. For whoever picks this up

- **The venue finding generalises beyond this slice.** Anything gated
  on deep having an ally (the FENCE variant, gift-refold's ALLY class,
  W12.4 bridges) can be measured on `lategame-80` after all; the
  cheapest probe is one grep for `deep-trunkally links>0`, and it
  costs four games.
- **`trunk_ally_gate` is untuned and PLACEHOLDER, and the A/B is not
  a verdict on shipping it.** It is a gate, so the only tuning
  question is on/off; n=16 at this effect size answers "not
  measurable here", not "worthless". The next honest step is either
  more seeds in a cell where alliances are common, or a cell with
  longer land borders (Europe) where a corridor to an ally is more
  than a handful of tiles.
- **The Europe rider is half a rider.** `derby5/tuning-derby7.ini`
  carries `trunk_road_gain` but has never carried
  `trunk_source_gain`, so with the ally gate on, Europe exercises only
  the task-value hook leg — ally corridor tiles that happen to fall
  inside one of our own city radii. Adding slice 2's knob there is a
  separate decision and was left to whoever owns the derby file.
- **Do not edit a shell script while it is executing.** The smoke
  stage's trailing parse died with "unexpected EOF" because the A/B
  stage was appended to `w12c-ally.sh` mid-run; bash reads a script
  incrementally by byte offset. The games and their checks had already
  completed, but the stage exited 2 for a reason that had nothing to
  do with the AI.

## W12 slice 4: corridors priced by the trade they unlock — mechanism live and flipping picks, score NULL (2026-08-22)

Branch `epic/w12d-trade-dividend`, 9 commits, based at `e6d64d7345`
(`epic/w03-horizon-content`). `ai/deep` + `benchmark` + one tuning
file; zero `ai/default`, `ai/baseline` or `server/advisors` changes.

Slices 1-3 price a corridor as pure logistics — the movement points
the finished link saves. Slice 3's A/B was a score NULL with the
mechanism strongly live, and the reading it invites is that corridors
are cheap to build and buy nothing. This slice gives them something to
buy, and the answer is still null: they buy it, the picks move, the
score does not.

### 1. What landed

| # | Commit | What |
|---|---|---|
| 1 | `0feb01a600` | `deep_trade_pair_value()`, deeptrade's route value quoted at arrival |
| 2 | `6e45a37f55` | the dividend itself, behind `trunk_trade_gain` |
| 3 | `857a1f1ba0` | the source's dividend-free counterfactual: `flipped=` |
| 4 | `a9903ef55e` | the conversion corrected to the advisor's trade weighting |
| 5 | `3c21128657` | `trunk_trade_gain` in `knobcensus.py` |
| 6 | `ad26161537` | `w12d-dividend.sh` identity + marginal A/B stages |
| 7 | `28c2596ec9` | the Europe rider in `derby5/tuning-derby7.ini` |
| 8 | `aa8313ab72` | the on-path identity stage |
| 9 | this entry | |

Deep already amortizes a caravan route by travel time
(`caravan_discount` d = 0.95/turn). Route face value GROWS with
distance while d^ETA shrinks with it, so the biggest routes on the map
are exactly the ones slow travel suppresses. Behind the gain, each MST
link additionally carries

    sum over served pairs of  value * (d^ETA_with - d^ETA_without)

`value` is `deep_trade_pair_value()` — `deep_route_terms()` plus
`deep_route_annuity()`, deeptrade's own two primitives at deeptrade's
own discount, quoted at the moment of arrival instead of at now — and
the reach filter deciding which partners count is deeptrade's too. No
second route valuation and no second discount exist.

### 2. The bounded ETA approximation, and what it cost

The exact ETA-with-road is a second pathfind per pair per corridor.
Instead, three approximations, all stated in the commit body:

- **ETA is arithmetic.** The corridor walk already measured its terrain
  cost (`link->terr`) over its own tiles (`link->len`), so
  `terr / len` is a measured cost per tile on this ground; a
  destination `dist` tiles away is priced at that rate, the road
  subtracts the same `saving` the logistics term is built from, and
  both ETAs are that cost over the caravan's own `move_rate`. Every
  number is measured or from the ruleset — no constant minted.
- **The served set is a distance test.** The corridor serves
  (near endpoint → destination) when the far endpoint is strictly
  closer to the destination than the near one is. Same continent, and
  known to us. That includes the link's own two cities, the pair the
  corridor most obviously serves.
- **Route life is held at the full horizon** on both sides of the
  difference, so it factors out. This overstates a distant route's
  tail and leaves the ETA difference, which is the signal, untouched.

The whole computation runs once per link inside the plan's existing
`(turn, player)` memoisation — that IS the cache by (corridor,
partner); the pricing path reads a per-link number and never
recomputes it.

**Measured cost, over the 16 on-arm games:** 5999 player-turns priced,
304,053 route valuations in total.

    pairs per player-turn   mean 50.7   median 41   p90 101   max 305

And in wall time, paired against the off arm on the same 16 seeds,
same binary, same `jobs 3`:

    off mean 169.5 s/game   on mean 167.1 s/game
    delta -2.4 s (-1.4%), paired sd 18.2, t -0.52, on-slower 7/16

**Not measurable.** The mechanism's cost is below the noise floor of
game wall time, so the per-(corridor, partner) cache the brief allowed
for is not owed.

### 3. The units error the instrument caught

Worth recording as a method note, because guessing would have shipped
it. The first implementation converted the dividend with `/ MORT`
alone, turning deeptrade's stock of discounted trade into a per-turn
flow. Two liveness games (`results/w12d-live`) then said:

    756 deep-trunkdiv player-turns, 459 with links carrying a
    dividend, maxdiv up to 23 — and flipped=0 on EVERY line.

The dividend priced corridors and moved no decision. The reason is
that the advisor's tile currency is not output points:
`city_tile_value()` (`infracache.c`) pays `TRADE_WEIGHTING` = 18 per
point of trade, so a trade term quoted in raw points stood beside a
logistics term at `trunk_road_gain` = 5.0 and was roughly 2% of the
price it was added to. Multiplying by `TRADE_WEIGHTING` states the
flow in the currency every other trade point is stated in; both
factors are the advisor's own. Same two games afterwards
(`results/w12d-live2`): 782 lines, 21 with `picked>0`, 4 with
`flipped>0`, 7 flips, maxdiv up to 122.

### 4. Knob, PLACEHOLDER, zero

    trunk_trade_gain  0.0  OFF = slices 1-3 exactly, byte-identically,
                           and no route valuation is spent either. 1.0
                           is the unit case BY DERIVATION: the dividend
                           is in deeptrade's own route-value units,
                           / MORT to a per-turn flow (amortize()'s own
                           annuity factor, the conversion
                           deep_trade_merit_want() makes on the same
                           number), * TRADE_WEIGHTING into the
                           advisor's tile currency. Needs
                           trunk_road_gain > 0 for a plan to exist.

One knob and not the gate-plus-gain the brief sketched: a gain of 0.0
IS the gate, as `trunk_source_gain`'s is, and a separate boolean would
be a second name for the same zero. Four audit places, the G/ECON/ALL
blocks, `knobcensus.py` MAP entry (`deep-trunkdiv pairs>0`, mag grade,
direct — the pair COUNT and not the dividend, because the count
evidences the knob while a dividend additionally needs the map to
contain a route worth walking). `tune-audit` CLEAN at 247 fields
(base 246 + this slice's 1). `meson test -C build` 6/6.

### 5. Identity: 10/10 twice, off the path and ON it

Base = `git archive e6d64d7345` built in `.w12d-base/` (sha256
`9d2605744085e26a`); branch binary `02f03e6acbccabd1`, pinned with
`libfreeciv.so` per stage.

At compiled defaults (`tuning/identity.ini`),
`benchmark/results/w12d-identity/`:

    deep      scorelogs IDENTICAL 10/10   (0 differ, 0 missing)
    mixed_db  scorelogs IDENTICAL 10/10   (0 differ, 0 missing)

That proof is off-path: at defaults `trunk_road_gain` is 0 and no
corridor plan exists at all. So the stage was run a second time with
slices 1-3 LIVE (`tuning/w12-ally-on.ini`, which does not name
`trunk_trade_gain` and therefore leaves it at its compiled 0.0),
`benchmark/results/w12d-identity-live/`:

    on-path identity deep: 10/10 byte-identical, 0 differ
    deep-trunk turn=100 player="P3" links=2 gap=2 hits=51 maxwant=15.97

The last line is the stage asserting its own premise — corridors were
planned and priced in both arms, and only the dividend was absent.
Provenance guard on both: the base logs carry no `deep-trunkdiv` and
no `flip=` token, so neither proof can have passed by running HEAD
twice.

A third, unplanned corroboration: the A/B's OFF arm scored **166.70**
on 16 `lategame-80` seeds, which is slice 3's ON arm to the last digit
(STATUS 2026-08-21 §7, same tuning file, same venue, same seeds,
different binary).

### 6. Score A/B: the picks move, the score does not

16 mirrored `lategame-80` seeds, one pinned binary
(`.w12d-pin/ab`, sha `02f03e6acbccabd1`), arms differing in exactly one
key — `tuning/w12-ally-on.ini` (slice 3's ON arm, the baseline the
brief names) vs the same plus `trunk_trade_gain = 1.0`.
`benchmark/results/w12d-ab/`, `benchmark/paircmp.py`, 0 pairing
violations, all 32 games reached T81.

    paired seeds: 16  (off 16, on 16)
        metric                       OFF        ON     delta      se      t   wins
        score                     166.70    165.47     -1.23    1.77  -0.69   7/16
        cities                      7.98      7.56     -0.42    0.27  -1.53   8/16
        unitsbuilt                 39.69     38.16     -1.53    0.84  -1.81   4/16
        techs                      52.76     53.15     +0.39    0.16  +2.48  11/16
        pop                        41.05     39.85     -1.20    1.24  -0.96   8/16
        mean_city_size              5.28      5.44     +0.16    0.05  +2.99  12/16
        settlers                    0.50      0.49     -0.01    0.13  -0.08   7/16
        landarea                94010.42  91625.00  -2385.42 2381.53  -1.00   6/16
        munits                     16.88     16.29     -0.58    0.67  -0.87   8/16
        wonders                     0.18      0.20     +0.02    0.05  +0.44   5/16
        unitslost                  11.11     10.32     -0.79    0.87  -0.91   6/16
        unitskilled                 9.48      9.04     -0.44    0.80  -0.55   6/16

        div-turns                   0.00    374.94  +374.94   10.10 +37.12  16/16
        div-links                   0.00    285.19  +285.19   20.65 +13.81  16/16
        div-picks                   0.00     13.44   +13.44    2.79   +4.82  15/16
        div-flips                   0.00      9.06    +9.06    2.68   +3.38  13/16
        trunk-picks                56.81     46.69   -10.12    9.81   -1.03   6/16
        ally-picks                 10.69     10.69    +0.00    5.80   +0.00   4/16

Read the execution counters first. The knob's predicate is exactly
zero in the OFF arm — the off-switch proof restated on the A/B's own
data — and in the ON arm 375 player-turns per game price route pairs,
285 of them come out with a dividend, 13.4 beyond-ceiling tasks per
game are assigned on a dividend-priced corridor, and **9.1 picks per
game were FLIPPED by the term**, on 13 of 16 seeds, measured against
the source's own dividend-free counterfactual rather than inferred.
The brief's bar (dividend picks > 0 or it is a miss) is met with room.

**The score does not move: −1.23 on 166, t −0.69, 7/16 wins. NULL,
and reported as null.** The knob stays at its shipping 0.0.

Two honest observations that are not claims:

- `techs` +0.39 (t +2.48) and `mean_city_size` +0.16 (t +2.99) are the
  only cells that clear |t| = 2, and they are the two you would predict
  if trade routes were in fact being unlocked — trade buys beakers, and
  a road between cities is what migration walks along. They are also
  two nominal hits out of twelve metrics with no multiplicity
  correction, on the same 16 games, and `score` is the metric this
  project ranks by. Worth a look at higher power; not worth believing
  yet.
- `trunk-picks` −10.12 against `div-picks` +13.44 says the dividend
  did not ADD corridor work, it REDIRECTED it: total beyond-ceiling
  picks fell while the dividend-priced subset rose. The same
  reallocation slice 3 found, one level down — and the same reason 16
  seeds cannot price it.

### 7. For whoever picks this up

- **The Europe rider is now whole.** `derby5/tuning-derby7.ini` carries
  `trunk_road_gain`, `trunk_source_gain`, `trunk_ally_gate` and now
  `trunk_trade_gain`, all at the unit case. Europe is where the
  dividend should be largest for exactly the reason the corridors are
  worth anything there: long land distances are what
  `caravan_discount` buries hardest.
- **The instrument is the reusable part.** `flipped=` is a real
  counterfactual (the same auction re-run with one term removed), it
  cost one subtraction per candidate, and it is what turned "the
  dividend priced 459 corridors" into "the dividend changed nothing" in
  two games instead of sixteen. Any term added to an existing price
  should carry one.
- **Three W12 slices have now returned NULL on `lategame-80` with the
  mechanism live each time.** That is a pattern worth naming rather
  than re-testing a fourth way: in this venue, deep's cities are size
  ~5.3 and average 8 per empire on a 40x40 map, so corridors are short,
  routes are small, and worker-turns spent on roads are worker-turns
  not spent on tiles. The next honest question is not another corridor
  term — it is whether `lategame-80` can resolve any corridor effect at
  all, which a power calculation on `trunk-picks` (se 9.8 on a mean of
  ~50) answers more cheaply than another 32 games.
## AUTOPSY closed: the tuned package loses on the ECON venue only, and G — mostly the CM block — is why (2026-08-22)

Brief: `sessions/S-AUTOPSY-defaults-vs-pv3.md`. Branch
`epic/autopsy-defaults` (base 31b947ffc7), diagnosis only, no
behaviour code touched. One pinned binary for all 200 games
(`.worktrees/autopsy/.stage-bin/freeciv-server`,
sha256 `b65878a6ccd975cc…`, with its own `libfreeciv.so`
`2618460cc337db3c…`), ports 7300-7339, jobs 4, TRAINING seeds.

Arms are generated, never hand-typed: `benchmark/ablate-blocks.py`
copies the chosen block's lines verbatim out of `benchmark/tuning/pv3.ini`
and omits everything else, which the loader treats as an EXACT compiled
default (`deeptuning.c` `deep_tuning_load_file`, TUNE_DOUBLE existence
guard). Every arm passed `validate_tuning` and an awk duplicate-key
check, and every one of the 200 games was audited against its own
`deep-tuning file="…"` read-back line: **200/200 loaded the arm they
were supposed to**, and a separate check confirmed each arm's live
values equal pv3 for its own keys and the compiled default for all
others.

Objective is the all-deep game's total score (`tune.py`'s `final_score`,
sum of every seat at its last turn). Arms are compared to each other on
the same seed, so the classic baseline arm — a per-(config, seed)
constant that cancels exactly — was not run at all; that is where the
game budget for the third round came from.

### Round 1: which family, both venues, 8 seeds (112 games)

isolated-150 (4 seats, the ECON venue), paired vs the `defaults` arm:

    arm        mean    sem      t    wins
    full      -98.8   38.1  -2.59    1/8      <- pv3 entire
    G         -53.8   18.6  -2.90    1/8
    S         -27.2   18.6  -1.47    1/8
    M         -24.2   20.0  -1.21    2/8
    X         +22.6   17.0  +1.33    5/8
    rest       +1.9   21.7  +0.09    4/8

tech50war-200 (8 seats, the WAR venue), same treatment:

    arm        mean    sem      t    wins
    full      +28.9   35.3  +0.82    4/8      <- pv3 entire
    M         -58.6   36.6  -1.60    3/8
    S         -30.1   32.8  -0.92    3/8
    G         -28.9   36.2  -0.80    3/8
    X          -8.0   34.1  -0.23    4/8
    rest       -0.2   20.1  -0.01    4/8

**The war venue does not reproduce the finding at all**: pv3 is
nominally AHEAD of compiled defaults there and nothing separates. The
standing claim "defaults beat pv3 on three venues" survives only as
"defaults beat pv3 on the ECON venue". (`rest` is the 14 pv3 keys that
S/X/G/M do not reach — the naval lift/boat set and three gift knobs. It
is a clean zero on both venues, so the block-assignment gap costs
nothing measurable, but it is still a gap.)

### Round 2: split G, isolated-150, 8 seeds (48 games)

G is 109 knobs; `benchmark/tuning-blocks.ini` now carries an exact
six-way partition of it (commit `9960d99a28`). Paired vs `defaults`:

    arm         mean    sem      t    wins
    GCM        -33.1   13.5  -2.45    1/8     17 cm_* knobs
    GHORIZON   -29.4   37.9  -0.77    2/8
    GCIVIL     -19.2   28.8  -0.67    4/8
    GDIPLO      -3.0    2.0  -1.52    0/8
    GPROD       +4.9   29.4  +0.17    5/8
    GSITE      +14.5   15.9  +0.91    4/8

Sub-arms sum to -65.3 against G's own -53.8, so the families are
roughly additive at this resolution and no interaction term needs to be
invented to explain the total.

### Round 3: confirm the top two at 16 seeds (40 games)

Same venue, seeds 1-16, five arms:

    arm         mean    sem      t    wins   median   12+ neg   drop-extreme
    full       -46.5   25.5  -1.83   6/16    -33.5    10/16        -35.3
    G          -40.4   17.7  -2.28   4/16    -43.0    12/16        -53.3
    GCM        -24.2   13.4  -1.80   4/16    -19.5    12/16        -17.3
    GHORIZON    -6.3   21.9  -0.29   6/16    -13.5    10/16        -20.1

`drop-extreme` removes the single largest-|diff| seed, the sensitivity
check the DECAYTECH entry above wishes it had run earlier. G survives
it and gets worse; GCM survives it; **GHORIZON collapses** — its
round-2 -29.4 was one +201 seed away from nothing.

### Verdict

- **Guilty: block G, and inside it GCM.** G carries -40.4 of pv3's
  -46.5 on the ECON venue at n=16 (t -2.28, negative on 12 of 16
  seeds); GCM carries -24.2 of G's -40.4 (t -1.80, negative on 12 of
  16). Nothing else in G is resolved: GHORIZON is null once the
  outlier goes, GDIPLO is tiny (-3.0) though it lost all 8 seeds,
  GSITE/GPROD are nominally positive.
- **Acquitted for now: the fear/floor family.** S is -27.2 (t -1.47)
  on econ and -30.1 (t -0.92) on war — the same direction the lean
  legend predicted, but not separated at any n we ran, and less than
  half of G on the venue where anything separates. The pre-registered
  suspicion in the brief ("most plausibly the fear/floor family") is
  NOT what the ablation found.
- **X is the only family that helps** (+22.6 on econ, t +1.33), which
  is consistent with the settler/site work being the tuned line's real
  contribution.

The 17 GCM overrides, as ratios to compiled default:
`cm_grow_safety_margin` x2.46, `cm_grow_trade` x2.25, `cm_wonder_trade`
x1.75, `cm_cap_food` x1.50, `cm_grow_shield` x1.50, `cm_trade_quality_slope`
x1.35, `cm_grow_below` x1.17, `cm_cap_shield`/`cm_grow_food` x1.12,
`cm_cap_trade` x0.62, `cm_wonder_shield` x0.60, plus two knobs pushed off
a compiled zero (`cm_grow_price_blend` 0.154, `cm_war_growth_charge`
0.285). The shape is "value trade and safety margin far above stock,
value the wonder shield below it" — a citizen-manager posture that the
optimiser found on some other objective and that costs score here.

### Recommended follow-up

1. **A pv4 should drop the GCM block** — revert those 17 `cm_*` keys to
   compiled defaults — and keep everything else. That is one change
   with n=16 evidence behind it, worth about +24/game (~+6/seat) on the
   econ venue.
2. Second candidate, weaker: GDIPLO (0/8 seeds won, but only -3.0).
3. Do NOT drop S on this evidence. If the fear/floor family is to be
   judged, it needs a venue where it separates and an n we did not
   spend here.
4. Whatever a pv4 does, it has to be re-measured on BOTH venues: the
   war venue's answer to this whole question is "no difference", and a
   package tuned to the econ venue's answer can lose there.

### Caveats

- 8 (round 1-2) and 16 (round 3) training seeds. Effects shrank when
  seeds 9-16 arrived (G -53.8 -> -40.4, GCM -33.1 -> -24.2, GHORIZON
  -29.4 -> -6.3): treat every n=8 number above as direction only.
- Two venues, not three. The third (Europe) is a single-seed
  exhibition, not a paired venue, and buys no per-seed statistics.
- The `deep-tuning` read-back line is truncated at 8192 chars, so it
  shows ~170-185 of the 245 fields. The audit is exact for the fields
  it prints and blind past the cut.
- Trap found the hard way, now in the runner's own docstring: this
  worktree's `meson setup` did not carry the tree's
  `-Daimodules=classic,tex,baseline,deep`, so the first smoke run
  logged "There is no AI type deep" and returned an identical 336 for
  all seven arms — a perfectly plausible null. The read-back audit is
  what caught it.

Retention: `benchmark/results/autopsy-r{1,2,3}/` scorelogs, game.logs
(the read-back evidence) and provenance; saves pruned. Arm inis are
regenerable byte-for-byte from `ablate-blocks.py` + `pv3.ini` +
`tuning-blocks.ini`; the exact round-2 invocation is
`ablate-blocks.py --out … --arm G=G --arm GCM=GCM --arm GHORIZON=GHORIZON
--arm GSITE=GSITE --arm GDIPLO=GDIPLO --arm GCIVIL=GCIVIL --arm GPROD=GPROD`.

## VENUE: lategame-80 is under-DOSED, not under-powered — and buying dose buys variance (2026-08-22)

Brief: `sessions/S-VENUE-corridor-power.md`. Branch
`epic/venue-corridor-power`, 7 commits, based at `6ab31162b0`
(`epic/w03-horizon-content`, all four W12 slices merged). **Zero `ai/`
changes of any kind** — one config, one tuning file, three instruments,
one driver, this entry. Own worktree `.worktrees/venue`, own build
(`-Daimodules=classic,tex,baseline,deep`), one pinned binary staged
with its own `libfreeciv.so` (`.stage-bin/freeciv-server` sha256
`2b5271ffcbf6f367de3c…`, lib `dc388fb8648fa152a6a3…`), ports 7500-7549,
jobs 4.

Three W12 slices returned score NULL on `lategame-80` with the
mechanism strongly live each time, and slice 4's entry named the right
next question: *can `lategame-80` resolve a corridor effect at all?*
The answer is not the one that question expects.

### 1. The diagnosis: the venue's problem is DOSE, not variance

Two new instruments, both reading artefacts that were already on disk.
`benchmark/corridorcensus.py` aggregates the `deep-trunk*` census lines
in a game log into a per-seed dose table.
`benchmark/corridorgeom.py` reads the final savegames and reimplements
`deep_trunk_build()`'s Prim to measure the world the plan was made in.

Over the 16 retained `lategame-80` games of the slice-4 A/B
(`.worktrees/w12d/benchmark/results/w12d-ab`), ON arm:

    links 2.73   gap 4.79   gap/link 1.75   hits 79.28   beyond 1.50
    ally-lnk 0.88   ally-gap/l 1.14
    picks: mean 46.7  sd 41.0  cv 0.88  near-zero in 2/16 seeds

**A corridor there is 1.75 tiles of missing road.** An ally corridor is
1.14. The beyond-ceiling demand W12 slice 2 exists to serve is one and
a half tiles per player-turn. And from the savegames, 95 deep
player-games of the same batch's OFF arm — which runs slices 1-3 live,
because that A/B was a MARGINAL control for the dividend, so if
anything it carries more road than an untreated `lategame-80` would and
the pre-roading figure below is generous to the venue:

    ncity 8.05   MST edges 5.72   edge len 4.50 tiles
    gap 2.77 tiles/edge   unfinished 3.20   MST path roaded 48.3%
    home continent 260 tiles   land 519   biggest continent 336

519 land tiles on a 40x40 world is 32% land shared by six seats, ~86
land tiles each, and **50.8% of every land tile on the map carries a
road by T81** (0.0% at T1 — the ten workers per seat that
`startunits ccccccccxxwwwwwwwwww` hands out at `techlevel 50` lay them).
Total unroaded MST tiles per empire: **8.9**.

Now the part that overturns the standing reading. `benchmark/powern.py`
inverts the paired t on the two retained A/Bs:

    slice 4 (w12d-ab):  OFF 166.70  ON 165.47  d -1.23  sd 7.10  se 1.77
                        D=+3.0 (1.8%)  n >= 23
    slice 3 (w12c-ab):  OFF 166.05  ON 166.70  d +0.65  sd 3.93  se 0.98
                        D=+3.0 (1.8%)  n >= 7
        per-seed diffs: +0.8 +0.0 +10.7 -2.5 +0.2 +0.0 +8.7 +0.0
                        -2.2 +0.0 +0.0 -5.3 +0.5 -1.2 -1.7 +2.3

**`lategame-80` is not underpowered for +3 score.** Twenty-three paired
seeds resolve one; slice 3's own sd says seven would. Slice 3's sd is
small for the worst possible reason — **six of its sixteen paired
differences are exactly 0.000**, because the venue applied no treatment
at all in those games. Three slices were read as "the term does
nothing" when what the data says is "the venue could not hand the term
enough to do".

### 2. corridor-150, and every setting that differs

`benchmark/configs/corridor-150.conf`, derived from the CANONICAL
`primary-120.conf` and differing in exactly three places:

| setting | primary-120 | corridor-150 | why |
|---|---|---|---|
| `ENDTURN` | 120 | 150 | the brief's floor; a road finished at T50 repays over 100 turns instead of 30 (`lategame-80`'s median corridor pick is turn 29 of 80) |
| `EXTRA_SERV` | — | `+ set startpos ALL` | one continent: `deep_trunk_build()` refuses an MST edge across a `tile_continent` mismatch, and an ally's border on `lategame-80` was 1.14 tiles away |
| `TIMEOUT_S` | 7200 | 10800 | `longhaul-300`'s watchdog, for the horizon |

Inherited from `primary-120` and load-bearing, so stated rather than
assumed: `TILESPERPLAYER=400` against `lategame-80`'s measured ~86 land
tiles per seat; `GENERATOR=FRACTAL`; `PLAYERS=6`, `SKILL=hard`, classic
ruleset, `migration`/`mgr_distance 6` all unchanged; and **no
`startunits`/`techlevel` line**, so a seat builds its own workers
instead of being handed ten at Railroad tech.

Measured, not hoped for — the OFF arm's own 8 games:

| | lategame-80 | corridor-150 |
|---|---|---|
| map | 40x40 = 1600 | 64x128 = 8192 |
| land | 519 (32%) | 2603 (32%) |
| land per seat | 86 | 434 |
| biggest continent | 336 | 1620 |
| deep's home continent | 260 | 1345 |
| cities per deep empire | 8.05 | 24.98 |
| MST edges | 5.72 | 22.41 |
| unfinished edges | 3.20 | 20.33 |
| gap tiles per edge | 2.77 | 3.84 |
| **unroaded MST tiles per empire** | **8.9** | **78.1** |
| MST path already roaded | 48.3% | 9.6% |
| all land roaded at game end | 50.8% | 11.5% |

`startpos ALL` is honest rather than absolute: 4 to 5 of the 6 seats
end on the biggest continent, not 6 of 6.

### 3. The pilot: 8 mirrored seeds, one binary, family ON vs all-OFF

`benchmark/venue-corridor.sh pilot 8 4`, results in
`benchmark/results/venue-pilot`. Arms differ in exactly the five keys
of the W12 family: `tuning/w12-div-on.ini` (slices 1-4 at their probe
values) against the new `tuning/w12-family-off.ini` (the same five keys
at their compiled shipping zeros). All 16 games reached T151, 8/8
maphashes match, 0 pairing violations, and the `deep-tuning file="…"`
read-back audit is 8 and 8.

**The dose the venue delivers (ON arm; OFF arm is a clean zero on every
column, which is the off-switch proof restated on the A/B's own data):**

    seed      links        gap   gap/link       hits     beyond   ally-gap/l      picks
       1       9.22      28.60       3.10     536.32       9.18         7.74     136
       2      13.40      30.69       2.29    1357.56       3.09         2.77     577
       3      11.31      29.64       2.62     703.38       6.76         4.10     357
       4      12.04      30.30       2.52     884.95       4.40         3.42     270
       5      11.54      28.49       2.47     733.96       5.03         3.39     340
       6      13.09      31.24       2.39    1132.08       2.99         2.84     109
       7       9.00      24.16       2.68     343.71       4.90         3.55     147
       8       7.86      18.55       2.36     342.79       3.69         3.24     107
    MEAN      10.93      27.71       2.55     754.34       5.00         3.88     255.4
    SD         2.02       4.30       0.26     360.98       2.08         1.61     165.0

    picks: mean 255.4  sd 165.0  cv 0.65  near-zero in 0/8 seeds

Against `lategame-80`: links x4.0, total gap x5.8, gap/link x1.5,
candidates reached x9.5, ally corridor length x3.4, tasks assigned
x5.5 — and **no seed at zero dose**, where `lategame-80` had two of
sixteen at zero and six of sixteen at an exactly-zero paired score
difference. The venue does what it was designed to do.

**Paired A/B (`paircmp.py`):**

    paired seeds: 8  (off 8, on 8)
        metric                       OFF        ON     delta      se      t   wins
        score                     198.48    191.23     -7.25    5.64  -1.29   2/8
        cities                     25.31     24.44     -0.87    1.09  -0.80   3/8
        unitsbuilt                126.50    128.00     +1.50    3.49  +0.43   3/8
        techs                      31.60     30.31     -1.29    0.41  -3.18   1/8
        pop                        94.15     92.81     -1.33    2.50  -0.53   3/8
        mean_city_size              3.96      3.99     +0.03    0.14  +0.22   4/8
        settlers                    8.19      9.17     +0.98    1.15  +0.85   4/8
        landarea               343250.00 332354.17 -10895.83 9717.89  -1.12   2/8
        munits                     35.44     37.60     +2.17    2.17  +1.00   6/8
        wonders                     0.10      0.27     +0.17    0.11  +1.53   4/8
        unitslost                  16.98     19.83     +2.85    2.84  +1.01   4/8
        unitskilled                11.19     12.96     +1.77    2.68  +0.66   5/8

        trunk-turns                 0.00    688.75   +688.75    8.31 +82.88   8/8
        trunk-picks                 0.00    255.38   +255.38   58.34  +4.38   8/8
        ally-turns                  0.00    322.38   +322.38   41.86  +7.70   8/8
        div-turns                   0.00    111.38   +111.38   28.68  +3.88   8/8

**Power (`powern.py`), from the pilot's own paired differences:**

    paired sd 15.94   se 5.64   t -1.29   wins 2/8
    per-seed diffs: -15.3 -13.8 -13.3 -16.2 +27.3 -24.0 +0.5 -3.2

    implied paired n for |t| >= 2, n >= (2*sd/D)^2:
      D =  +3.0  (1.5% of the OFF mean)   n >= 113   -> 226 games
      D =  +4.0  (2.0%)                   n >=  64   -> 128 games
      D =  +6.0  (3.0%)                   n >=  29   ->  58 games
      D = +10.0  (5.0%)                   n >=  11   ->  22 games

At ~20 minutes a game and jobs 4, that is ~19 h for D=+3, ~11 h for
D=+4, **~5 h for D=+6**.

### 4. Verdict

**Powered at n=29 for a 3% effect (+6 score); NOT powered at any cheap
n for +3.** Stated in the brief's own two forms, because both halves
are true and they point opposite ways:

- *"This venue is powered at n=N"*: **N = 29 paired seeds for D = +6
  score (3.0%)**, 58 games, about five hours at jobs 4. At the
  project's own `--full` size of 100 paired seeds, corridor-150
  resolves **|D| >= 3.2 score (1.6%)**.
- *"Corridor effects are below X at any feasible n"*: at n = 8 the
  pilot bounds the family's effect at **|D| <= 11.3 (2 se)**, and
  resolving the brief's +3 needs **n >= 113**, which is past `--full`.

**The trade the venue makes is dose for variance, and it is close to
one-for-one.** corridor-150 multiplies the corridor dose by 4-6x and
eliminates zero-dose seeds — and its paired score sd is **15.94 against
`lategame-80`'s 7.10**, so on pure score resolution it is *worse*
(n>=113 for +3 against n>=23). That is not a flaw in the config: the
dose IS the perturbation. 255 redirected worker tasks per game move the
game more than 47 do, in both directions.

**And the pilot's point estimate is negative.** −7.25 score (−3.7%),
2/8 wins, 6 of 8 seeds down, t −1.29 — not significant, and reported as
not significant. But `techs` −1.29 with **t −3.18** is the only cell in
the table clearing |t| = 2, and it is exactly the cost you would
predict: worker turns spent on connecting roads are worker turns not
spent on the tiles a city works. The first venue that gives the W12
family enough dose to show a sign shows a negative one. **That is
information the queued fifth corridor term (a military dividend) should
have before it is written, not after.**

Three further facts from the pilot, none of them claims:

- **The roads barely appear.** Map-wide roaded land tiles at T151: OFF
  298, ON 312 — **+14 tiles per game across six empires**, against 255
  corridor tasks assigned. The reallocation slices 3 and 4 each found
  at 1/6 the dose is still the dominant effect at full dose: the
  mechanism moves which tile a worker is sent to far more than it
  changes how much road exists.
- **The trade dividend shrinks as the map grows.** `maxdiv` on
  `lategame-80` reached 122; here it averages 1.37 with a max of 8.99
  per player-turn, because `caravan_discount`'s d^ETA is near zero at
  both ends of a long-distance difference. Slice 4's own header
  predicted Europe would be where the dividend is largest; this venue
  says the opposite for the same reason, and the factorial should not
  assume the dividend arm is live here without checking `div-flips`.
- **The cost is not in wall time.** `pairs` per player-turn reaches
  2400 here against slice 4's measured mean of 50.7, and the arms still
  ran at 1198.6 s (OFF) and ~1099 s (ON) per game.

### 5. For whoever picks this up

- **The factorial was NOT run and must not be inferred from this.** The
  pilot is one cell — family on against family off — at n=8. Reading
  four marginal effects out of it would be exactly the mistake the
  brief was written to prevent.
- **Size the factorial from `powern.py`, not from hope.** A four-factor
  read wants the marginal contrasts, whose sd is bounded above by this
  family contrast's 15.94 and is probably nearer slice 4's 7.10. Run
  the pilot's own arms first at the chosen n and re-read `powern.py`
  before committing the rest of the budget.
- **The sd is estimated from 8 differences**, so it is good to about
  ±25% and the implied n to about a factor of 1.5 either way. It sizes
  a decision; it is not a guarantee.
- **`corridor-150` is a PROBE cell, not canonical and not a tuning
  cell.** It has never been tuned on and its verdict here is its whole
  provenance. Promoting it into the portfolio is a separate decision
  with separate evidence.
- **Reuse the two instruments before designing any venue.**
  `corridorgeom.py` on eight retained savegames answered "why is this
  venue hostile" in under a minute and cost no games at all; the four
  most important numbers in this entry (land per seat, gap per edge,
  50.8% pre-roaded, unroaded MST tiles per empire) all came out of
  artefacts three earlier sessions had already paid for.

## W12 funnel: 255 corridor picks buy 21 work starts, and a fifth of them are overridden in the same call (2026-08-22)

Brief: `sessions/S-W12F-conversion-funnel.md`. Branch
`epic/w12f-funnel`, 7 commits, based at `75a9221054`
(`epic/w03-horizon-content`, all four W12 slices merged plus the
corridor-150 venue). **Counters only — no behaviour change anywhere,
proved twice below.** Own worktree `.worktrees/funnel`, own build
(`-Daimodules=classic,tex,baseline,deep`), one pinned binary staged
with its own `libfreeciv.so` (`.stage-bin/freeciv-server` sha256
`1b76af65bc3e1ab54dc93bc3…`, lib `6ea1358503e553ebbddf9e68…`), ports
7500-7549, jobs 4. `tune-audit` CLEAN at 247 fields, unchanged (no new
knobs — the ledger has none). `meson test` 6/6.

The corridor-150 pilot measured 255 corridor tasks assigned per game
buying +14 net roaded land tiles map-wide, and named the gap between
those two numbers without being able to look inside it. This session
put stages in the gap.

| commit | what |
|---|---|
| `a03d62b5ee` | export `deep_trunk_tile_roaded()` |
| `73d8ceed9e` | `deepfunnel.c`, the per-worker assignment ledger and its causes |
| `d2cf780a6c` | the mid-build task the safety pass idles |
| `2dd7dff31e` | `benchmark/funnelreport.py`, the stage table |
| `a4750dc6a6` | `benchmark/pickfunnel.py`, the funnel from artefacts that predate the counters |
| `02aa35c30a` | `benchmark/pickfate.py`, what took the worker |
| — | this entry |

### 1. How the ledger reads the real decision sites

The mechanic that shapes everything below was read before the counters
were written, and it is not the one the brief assumed.
`auto_workers_player()` sends a worker whose activity is `IDLE` to
`settler_run` and a worker that is mid-activity to `settler_cont`
(autoworkers.c:1399-1414), and `adv_follow_path()` restores `IDLE` after
every partial walk (advgoto.c:65-72). So a worker **walking** to a
corridor tile is re-decided from scratch every turn, with no memory of
its task beyond `punit->server.adv->task`; a worker **building** is not
re-decided at all, and the only decision that can take it off the job is
`dai_auto_settler_cont()` idling it for danger. Those are the two sites,
and the ledger sits on both.

`deep_funnel_open()` hands `deep_trunk_source_task()` the tile the
worker already held; the source raises a stage at each test that refuses
it (unsafe / taken / nopath / noact / price) and
`deep_funnel_close()` maps the final stage onto a cause only when the
tile was not re-picked. Absent from the plan entirely splits on the road
test into `roaded` and `replan`. A city work request, the flee pass, the
end-of-phase sweep and death each name their own.

### 2. Identity: the counters observe, twice

**Short config, family ON in both arms.** Base = `git archive
75a9221054` built in `.funnel-base/` (sha `73d97646216ac88d`), branch
`1b76af65bc3e1ab5`. Ten paired seeds, `configs/smoke.conf`, deep arm,
`tuning/w12-div-on.ini` read back in both:

    scorelogs IDENTICAL 10/10   maphash 10/10
    deep-trunkpick per seed: 40 3 11 20 20 8 69 33 59 6  — equal, arm for arm
    deep-trunkfun lines:     75 (branch)   0 (base)

On-path in every seed: 269 corridor picks across the ten games, the same
269 in both arms.

**And at full venue scale.** The measurement run below reuses the
pilot's exact seeds and config, so its games are comparable to the
pilot's ON arm game for game:

    corridor-150 scorelogs identical to the pilot's ON arm: 8/8

150 turns, six empires, 255 corridor picks a game, byte-identical
against a binary built by a different session from a different tree.

### 3. The funnel: 8 seeds, corridor-150, family ON

`benchmark/results/w12f-funnel`, `tuning/w12-div-on.ini`, read-back
audit 8 of 8, 45 deep player-games.

    seed      picks    tiles    start   wtiles   wturns     done    dsalv
    1           136       41       14       14       49       15        2
    2           577       53       31       31       91       34        3
    3           357       59       28       28       87       31        4
    4           270       61       22       22       60       23        2
    5           340       61       24       24       57       29        6
    6           109       35        9        9       27       12        3
    7           147       49       26       26       64       29        3
    8           107       25       12       12       27       13        2
    MEAN      255.4     48.0     20.8     20.8     57.8     23.2      3.1

**The stages, per game, with the attrition between them:**

| # | stage | per game | of the stage above |
|---|---|---|---|
| 1 | picks — assignments made | 255.4 | |
| 2 | tiles — DISTINCT tiles ever assigned | 48.0 | 18.8% of picks |
| 3 | walk — picks that did not begin work | 234.6 | 91.9% of picks |
| 4 | start — times work actually began | **20.8** | **8.1% of picks** |
| | wtiles — distinct tiles worked | 20.8 | 43.2% of tiles wanted |
| 5 | wturns — turns of work accumulated | 57.8 | |
| 6 | done — corridor tiles that got a road | 23.2 | 48.4% of tiles wanted |
| | dwork — of those, deep began work there | 20.1 | 86.6% of done |
| 7 | dsalv — of those, deep never worked them | 3.1 | 13.4% of done |
| | live — still assigned at game end | 3.2 | |

    picks per distinct tile      5.32
    picks per work start        12.31

**Where an assignment ended (148.8 drops a game):**

    norun     58.9   39.6%   the source never saw the worker again
    taken     28.2   19.0%   another own worker stands on the tile
    roaded    20.4   13.7%   the tile got its road (not a leak)
    outbid    19.5   13.1%   the advisor's own champion won
    replan     8.1    5.5%   the plan no longer carries the tile
    switch     6.8    4.5%   a different corridor tile won
    gone       5.2    3.5%   the worker died or stopped being a worker
    unsafe     1.4    0.9%
    nopath     0.1    0.1%
    flee       0.1    0.1%
    noact/price/request  0

**The salvage hypothesis is dead and the churn hypothesis is half
right.** `dsalv` is 3.1 tiles a game: essentially nobody finishes what
deep walks away from. And mid-build preemption — the brief's leading
suspect — is `flee` 0.1 and `stop` 0: once a worker actually starts a
corridor road it almost always finishes it (20.1 of 20.8 work starts end
roaded). The churn is entirely on the WALK.

### 4. What `norun` is, from the log rather than the ledger

`benchmark/pickfate.py`, same 8 games, artefacts only:

    SAME TURN: 458 of 2043 picks (22.4%) were overridden by the
               city-placement search in the same call
    NEXT TURN: 978 picks were not followed by a pick by the same unit
               city mission     426   43.6%
               silent but alive 411   42.0%
               never named again 141  14.4%

The same-turn number is the one that matters and it is exact.
`deep_auto_settler_run()` runs the corridor task source inside the
`UTYF_WORKERS` block (deepsettler.c:2167) and then, for a unit that is
**also** a cityfounder, runs `deep_find_best_city_placement()` and takes
the site whenever `result->result > best_impr` — where `best_impr` IS
the corridor pick that was just made and logged. In the classic ruleset
Settlers carry `UTYF_WORKERS`, so the corridor auction bids for, wins,
and logs a task on a unit that is one comparison away from being sent to
found a city. The task never survives the function it was made in, and
nothing charges it: the ledger only learns the next turn, when the unit
is in the `BUILD_CITY` branch and returns before `FIND_WORK`.

That also explains the ledger's `taken` at 19.0%, which is the second
cause and reads oddly at first: 69.5 distinct workers touch 48 corridor
tiles a game, so the tiles are crowded by units that keep being handed
the same corridor and keep being taken away from it.

### 5. Diagnosis

**The dominant leak is that the corridor task source bids for units it
does not get to keep, and 91.9% of its assignments are spent walking.**
Of 255.4 picks a game only 20.8 — one in twelve — ever begin work, and
those 20.8 are spread over 48.0 distinct tiles that are taken up 3.17
separate times each; 22.4% of every pick is overridden by the
city-placement search inside the same call before the worker takes a
single step, and 43.6% of the picks that are never renewed are followed
by that unit on a city mission. What is NOT the leak: mid-build
preemption (`flee` 0.1, `stop` 0 a game), the safety pass (`unsafe`
1.4), pathing (`nopath` 0.1), and salvage by others (`dsalv` 3.1 —
nobody finishes what deep drops). Once a worker starts a corridor road
it finishes it 96% of the time; the whole loss is between the
assignment and the first shovel. The 255 picks and the +14 roads are
therefore not a completion problem at all — they are 234.6 worker-turns
a game of walking, redirected by a term whose winner is discarded a few
lines later, which is exactly the reallocation-without-road the pilot
measured and the `techs` t −3.18 it cost.

### 6. For whoever picks this up

- **A fix was not in scope and is its own epic.** The obvious shape —
  do not let the corridor source bid for a unit the founder search is
  about to take, or do not re-run the source for a unit already walking
  to a corridor tile — is two different changes with two different
  risks, and the second one changes what the AI does with a worker
  every turn. Neither is a counter.
- **The two independent reads of stage 6 agree.** `deepfunnel`'s
  `done` is 23.2 a game; `pickfunnel.py` decoding the final savegames of
  the pilot's own ON games says 23.2. That agreement is the ledger's
  correctness check and it cost no games.
- **`pickfunnel.py` found a coordinate trap worth stating.** A savegame
  row is in NATIVE coordinates (`savegame3.c` calls
  `index_to_native_pos()` before writing every position) while `TILE_XY`
  — and so every `tile=` in a log line — is in MAP coordinates.
  corridor-150's own save says `"topology","ISO|HEX"`, and comparing the
  two directly put 22 of 41 picked tiles off the map and 5 in the ocean;
  `NATIVE_TO_MAP_POS` (common/map.h:170) fixes it, after which all 41
  are land. Any future instrument that joins a log tile to a savegame
  tile needs the conversion. `corridorgeom.py` is not affected — it
  works entirely inside the save, where everything is native — but its
  `real_map_distance` is computed in native coordinates on an isometric
  map, which is a separate question for whoever next reads its edge
  lengths.
- **Retention:** `benchmark/results/w12f-funnel` (8 corridor-150 ON
  games, saves pruned after reading), identity under
  `/tmp` scratch and reproducible from the two staged binaries named
  above.
