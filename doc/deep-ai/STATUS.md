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
