# Session brief — WRACE: wonder-race awareness and which-wonder economics (pre-v4)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md`
§5, the R2b STATUS tail (lands before this session — its help-wonder
pricing is this session's delivery arm), `findings/mechanism-map.md` §4
gap 6 (wonder economics), and stock's wonder machinery
(`adv->wonder_city` selection in `advbuilding.c:189-260`,
`dai_choose_help_wonder`, the wonders-in-wrong-city re-zeroing in
`daicity.c:2007-2009`).

## THE JOB

1. **Race go/no-go.** Rival wonder progress is omnisciently readable
   (their cities' current builds + shield stocks + surpluses — the same
   reads the W36 order census used). For every great wonder we are
   building or about to start: estimate our completion vs the best
   rival's; if we lose by more than `wrace_margin_turns` (PLACEHOLDER),
   don't start / switch out early (the salvage is the shield stock, and
   HEDGE's liquidity legs are the escape hatch). Counter: races
   declined, races abandoned, races LOST anyway (the calibration
   metric — losses at high estimated margin mean the estimator is
   wrong).
2. **Race to WIN.** Symmetric: when we lead a race that is close, the
   caravan-feeding path (R2b) gets a race-urgency multiplier
   (PLACEHOLDER) — finish lines are where help-wonder pays most.
3. **Which-wonder, minimally.** Do not build a full wonder chooser. One
   honest correction: the wonder-city's candidate wonders are ranked by
   their existing (post-DARKFX) effect wants; add the race term (P(win)
   × want, not want alone) so a contested wonder we would lose ranks
   below an uncontested lesser one. Stock's flat ×1.5 wonder emphasis
   stays.
4. **Under the honesty flag** (note for the roadmap, not this session):
   rival progress becomes an estimate; the go/no-go inherits
   known_city's staleness. Design the read behind one function so the
   W04 substitution is one seam.

Venue: wonders concentrate on garden2b/long/primary — race contention
needs multi-player cells; measure on contested + primary-120. Counters
before scores; knobs PLACEHOLDERs; byte-identity at zeroed gains;
`meson test` + audit CLEAN; STATUS final. 5-7 commits. Stop: the
completion estimator disagrees with observed completions by more than
its own margin (calibrate first, gate second).
