# Session brief — PEACE: the cease-fire trap, embassies, and free signals (pre-v4)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md` §5,
`findings/deep-dive-2026-08-05/diplomacy-gamestate.md` IN FULL (your
evidence base: the clause table, §2's machinery, §2a's 800-save census),
and `findings/unused-game-surface.md`'s PEACE paragraph.

## THE JOB — all in deep's own wrappers; zero `ai/default` changes

1. **The cease-fire trap fix.** Measured: 20-42% of pacified pairs stick
   at cease-fire forever (4-turn peace window at the END of the 16-turn
   ceasefire vs the 3-6-turn proposal spam gate, plus best_target love
   bleed −50/turn vs the 125-love peace bar). Deep's
   `dpai_diplomacy_actions` wrapper gains a pre-pass: propose peace on
   the exact turn the window opens (`turns_left <= 4` is readable state)
   toward partners deep wants durable (`peace_seek_gain` PLACEHOLDER,
   0 = off). Metric: the census script's peaked-at-ceasefire share, per
   cell, before/after — the 20-42% mass is the target population.
2. **Embassy clause pricing.** Override `treaty_evaluate` in deep's
   wrapper so peace-level embassy swaps clear: receive valued by actual
   tech-leakage worth (civ2civ3: embassies ARE the research discount),
   give by a tunable that is not `-5×turn` (`embassy_give_cost` family,
   PLACEHOLDERs; the invented unbounded cost is upstream bug #46). One
   owner: compose with deep's existing diplomat-embassy want
   (`embassy_science_value`) so deep stops paying units for what a free
   clause delivers — state which channel owns which case.
3. **Free third-party signals.** Wire the three computed-and-unread
   signals as INPUTS (no new policy this session): `WIN_CAPITAL`,
   `tech_leader`, `production_leader` become readable deep-side values
   with counters, consumed minimally (e.g. ops target bias toward the
   runaway leader's holdings behind `leader_check_gain`, 0 = off).
   BALANCE builds the real statecraft on top next.

## Acceptance

- Off-switches byte-identical (all gains 0 = today), counters per lever.
- The ceasefire-trap A/B: contested + lategame-80 (the 41-42% cells), 20
  paired seeds direction: peaked-at-ceasefire share, end-state peace
  share, score. Watch the interplay honestly: more peace may LOWER score
  on war-decided cells (conquest foregone) — report the trade, don't
  hide it; the trade-route dividend arrives with TRADE, not this
  session.
- Embassy swap: count of swaps cleared (was structurally 0), science
  delta on civ2civ3 cells.
- `meson test` + `tune-audit.py` CLEAN at counts; STATUS final; leave
  NEXT-SESSION.md and sessions/ alone.

## Scope fences

5-8 commits. No war/peace ownership beyond the pre-pass proposal, no
gifts/proxy-arming (BALANCE), no trade-route work (TRADE). The upstream
bugs this touches (#43, #44, #46) stay filed, not fixed in place.

## Stop conditions

Stock's treaty acceptance surprises the probe (measure a 2-game probe
first); the pre-pass can't reach the proposal machinery without touching
`ai/default` beyond visibility (report the seam).
