# Session brief — BALANCE: balance-of-power statecraft (pre-v4; after PEACE)

Branch `epic/w03-horizon-content`. Read `CLAUDE.md`, `NEXT-SESSION.md`
§5, `findings/deep-dive-2026-08-05/diplomacy-gamestate.md` (the treaty
machinery, love mechanics, third-party absences), the PEACE session's
STATUS tail (this session builds on its wrapper), and the TRADE brief's
love mechanism (one love ledger, no double-charging).

## THE JOB — two active levers, both deep-side wrappers, zero ai/default

1. **Love-buying gifts.** Deep proposes small gold/tech gift clauses to
   a partner whose PEACE/durability we want, priced against what the
   peace is worth (trade-route NPV at stake, border pressure relieved —
   both computable). Stock recipients accept anything their goldequiv
   values positive (a free tech/gold always is). Gain: `gift_love_gain`
   family, PLACEHOLDERs; hard cap on gift budget as a fraction of
   treasury above the guard floor (structural, reuses the treasury
   guard). Counter per gift with the intended target state.
2. **Proxy arming (human, 2026-08-05).** Give a tech to a DISTANT nation
   to menace the enemy menacing us. Recipient selection is the session's
   design core, all from readable state:
   - at war with, or bordering, our top menacer (density fear / menace
     already rank menacers);
   - distant from us (their strengthening must not raise OUR density
     fear — reuse the reachability law);
   - able to use it (tech within their reach: prereqs known);
   - leak-safe (their embassies/alliances don't hand it onward to the
     menacer — INVERT the existing `is_dangerous` tech-leak veto in
     `daidiplomacy.c:239-258` into a targeting criterion).
   The gift is a military-relevant tech chosen by what shifts THEIR
   balance vs the menacer (the defender/attacker unlock scan from
   CONSEQ's defensive tech-wall, run on their position). Gain:
   `proxy_arm_gain` (PLACEHOLDER, 0 = off). Counters: proxies armed,
   menacer's subsequent war engagements vs the proxy, our own border
   pressure delta — the mechanism's OWN metric is "did the menacer get
   busier elsewhere," measurable from diplstate + the fear terms.

3. **Culture stock in conquest worth (derby N2; corrected per DARKFX
   2026-08-06).** Conquest transfers the accumulated history stock
   (Carthago inherited Gdansk's 2445 and grew it to 6408 — by LINEAR
   accumulation: `history_interest_pml = 0` in both rulesets, so the
   stock earns no interest and the old "annuity/compounds" framing is
   dead). The stock is still a score asset that changes hands whole:
   `conquest_worth_mult` doubles city worth but nothing prices it. Add
   the captured city's culture stock x the score term's value as a
   component of conquest worth (tunable, PLACEHOLDER), at the same call
   sites as the multiplier. Read `findings/darkfx-2026-08-06.md` for
   the culture-channel facts before wiring.

## Discipline

Both levers default OFF (byte-identity), both are proposals through the
existing treaty machinery (recipient consent is stock's own valuation —
nothing is forced), one love ledger shared with TRADE's route-love (no
double-charge; state the composition). Measure on contested (8p — enough
third parties) + lategame-80, 20 paired seeds direction: score, wars
involving the menacer, our unitslost, gift spend. Exploitability and
ethics-of-play note for STATUS: this is the AI playing power politics
with real treaties; say what it does plainly so a human opponent reading
the changelog knows the AI may arm their rivals.

`meson test` + audit CLEAN at counts; STATUS final; leave NEXT-SESSION.md
and sessions/ alone. 6-8 commits. Stop: recipients structurally refuse
(stock's valuation surprises us — measure first with a probe); the love
ledger composition with TRADE can't avoid double-charging cleanly.
