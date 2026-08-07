# BALANCE: the AI can buy affection, and the buyer loses (2026-08-06)

Five `ai/deep` / `benchmark` commits plus this file. `ai/deep`,
`benchmark/` and doc only; zero `ai/default`, `ai/baseline` or
`server/advisors` changes.

Evidence base: `findings/deep-dive-2026-08-05/diplomacy-gamestate.md`
(the clause table, the love mechanics, the third-party absences), the
PEACE session's wrapper, TRADE's one-love-ledger rule, and
`findings/darkfx-2026-08-06.md` for the culture channel.

---

## 0. Headline

**The love-buying gift is the largest single-lever score move this
project has measured in self-play — +8.28 on contested and +8.92 on
lategame-80, both past t = 3.6 — and the head-to-head says it is a
transfer, not an advantage.** In a mixed game where deep gifts and
`ai/default` does not, deep gains +1.93 (t +0.72) and the counterparty
gains **+5.26 (t +2.15)**: the recipient banks more of it than the
giver, and deep's margin over `ai/default` moves −3.34.

That gap between the two readings is the finding. An all-deep cell
cannot tell a positive-sum mechanism from a competitive one, and the
project's primary metric lives in all-deep cells.

Everything ships at its compiled default, which is off.

---

## 1. What was built

| # | Commit | Knob | Default |
|---|---|---|---|
| 1 | `91ec07258d` culture stock a conquest carries across | `conquest_culture_gain` | 0.0 |
| 2 | `31be0df640` what a partner's routes and border are worth | `gift_border_value` | 0.0 |
| 3 | `a0bdbd41bc` buying love with the currency deep has | `gift_love_gain` | 0.0 |
| 4 | `f957dcf28e` arming a distant power against our menacer | `proxy_arm_gain` | 0.0 |
| 5 | `382518defc` the four arms archived | — | — |

`meson test -C build` **4/4**. `python3 benchmark/tune-audit.py`
**CLEAN 172/172/172/172**.

### Off-switch proof

Smoke cell, 10 paired seeds, both binaries loading the same two-key
ini (only keys the older binary knows). Pre-session `6eff59f822`
(pinned `d1e99bdd547c96`, built in a throwaway worktree) against the
session binary `20294f6a010ad8`: **10/10 scorelogs byte-identical**,
and the ten hashes are the same ten DARKFX and ENDGAME published:

    70f2c13136a4  ed23c9303eb5  6bfeb8bbf7cf  2314c27e8c5e  7f08e0eb79de
    ca1fe91c60b1  912773e6b97e  825c66af8f8b  4bd40ba5c2f7  7b74bb209f89

Execution counters in the OFF arms of both A/B cells: **0
`deep-conqculture`, 0 `deep-gift`, 0 `deep-lovegift`, 0 `deep-proxy`
lines.**

---

## 2. The measurement that chose the instrument

The brief asked for "small gold/tech gift clauses". The first thing
built was the census, and it retired the gold half before a line of
gift code existed.

    deep-gift census, 8 games (4 contested + 4 lategame-80)
                                     contested   lategame-80
    spare treasury above the guard    p50   17          0
      (gold - dai_gold_reserve)       p90   50         45
                                      max  164        100
    gold that buys the love shortfall p50    2          0
      (dai_treaty_accepted, inverted) p90 1085       1729
    gold that clears a refused peace  p90 5904       6812
      (greed(), the square over ten)  max 126562    61152
    affordable, of rows with a shortfall  6.1%       1.3%

**Deep is not a player with a diplomacy budget; it is a player that
spends its gold the turn it arrives.** Two prices, two products, and
neither is reachable from a median 17-gold treasury.

**Trade at stake was exactly ZERO on every row of both civ2civ3
cells.** TRADE measured 0.30 route endpoints a game on civ2civ3; the
peace valuation's trade half is therefore inert on the venue it was
built for, and the whole of `deep_peace_worth` on these cells is the
border term.

A tech is the instrument deep does hold: `ai/default` prices one at
three times the receiver's remaining bulbs, out of no treasury at all.
So the gift became a tech clause, and the design followed the
measurement rather than the brief.

---

## 3. The love gift: it executes, it is accepted, and it is a transfer

    deep-lovegift, 20 games per cell, willingness bound made non-binding
                        contested     lategame-80
    offers per game          32.2            22.6
    accepted                  96%            100%
    price to receiver p50   45 gold      1575 gold
    expected love     p50     +40            +300
    distinct techs given       30              22

The price gap between cells is the whole story of what is being given
away. On contested deep hands over Bronze Working; on lategame-80 it
hands over Industrialization and Tactics.

### 3a. Self-play, both cells, 20 paired seeds

    contested (T150, 8p)          OFF       ON    delta      t   wins
    score                       82.44    90.72    +8.28  +3.65  15/20
    techs                       18.56    21.36    +2.80  +4.11  15/20
    unitsbuilt                  49.71    44.79    -4.92  -3.63   5/20
    unitslost                   14.78    12.04    -2.74  -1.50   7/20
    munits                      14.18    13.12    -1.06  -1.91   6/20

    lategame-80 (T80, 6p)         OFF       ON    delta      t   wins
    score                      169.47   178.39    +8.92  +4.57  17/20
    techs                       53.00    55.98    +2.98 +11.26  20/20
    cities                       8.15     8.53    +0.38  +2.42  14/20
    pop                         42.07    44.32    +2.25  +2.87  13/20
    unitslost                   12.33    10.43    -1.90  -2.30   7/20
    unitskilled                 11.03     9.16    -1.87  -2.64   6/20

Read the decomposition before the total: `get_civ_score()` pays two
points a tech, so **+2.80 and +2.98 techs are +5.60 and +5.96 of the
+8.28 and +8.92.** The rest is fewer soldiers built and fewer lost —
the peace dividend PEACE went looking for and did not find, arriving
now that love is being paid for rather than waived.

### 3b. Head-to-head, `mixed_dc`, contested, 20 paired seeds

Deep on the odd seats, `ai/default` on the even ones, same game, same
map, both arms identically seated.

    metric        side          OFF       ON    delta      t   wins
    score         deep        86.15    88.08    +1.93  +0.72  13/20
    score         classic     89.85    95.11    +5.26  +2.15  13/20
    score         GAP d-c     -3.70    -7.04    -3.34  -0.86  10/20

    techs         deep        19.62    20.98    +1.35  +1.95  13/20
    techs         classic     20.21    22.06    +1.85  +3.23  15/20
    pop           classic     27.99    30.48    +2.49  +2.13  13/20
    pop           GAP d-c     -1.91    -4.28    -2.36  -1.16   7/20

**The only significant score move in the head-to-head belongs to the
player receiving the gifts.** The gap is not resolved at twenty seeds
(t −0.86), and it does not have to be: the claim being tested was that
the lever helps deep, and the direction is wrong while the
counterparty's gain is real.

Arithmetically it could hardly be otherwise. Deep gives an advance and
receives affection; the recipient receives an advance. Affection is
worth something — the unit columns say so — but on a metric that pays
two points a tech, the side holding the tech is ahead.

### 3c. The clamp TRADE has and this does not

`trade_route_love` is structurally clamped below
`req_love_for_alliance`, so commerce can carry a relationship to peace
and never on its own into an alliance. **The tech gift has no such
clamp and the census shows why it needs one**: on lategame-80 the
median gift is expected to add **+300** love in one step, against an
alliance bar of 250. The diplomatic census agrees — pairs ending
allied go from **27.1% to 33.2%** on contested.

That is a gap in this session's work, stated rather than papered over.
It was not added after the fact because every number above was taken
without it.

---

## 4. Proxy arming: the targeting is right and the clause is illegal

The lever selects exactly as designed and **never fires once.**

    deep-proxy census, contested, 2 games, willingness non-binding
    structurally eligible candidate-turns              97
    offers made                                         0
      of which at DS_WAR with us                    92/97
      median love of those candidates                -308
      median cease-fire price, greed() = love^2/10   9486 gold
      best safe advance that unlocks a better
        attacker for such a power                      45 gold
      rows where the leak veto removed everything    50/97

    lategame-80: 0 eligible candidates -- nobody is distant.

Two independent walls, both `ai/default`'s and neither a defect in the
selection:

1. **A tech clause is illegal while `ds_after == DS_WAR`.**
   `dai_treaty_evaluate` accepts gold, maps, vision and embassies as
   gifts at war and refuses techs. The clause that changes `ds_after`
   is a cease-fire, and a cease-fire is priced at `greed(their love)`
   — the square of the shortfall over ten. A distant power we have
   never dealt with has bled to −308, so the cease-fire costs it a
   median 9486 gold of grudge.

2. **`tech_trade_allow_holes = FALSE`.** A legal gift is one step of
   the receiver's tree, and a proxy worth arming is backward by
   construction, so a step of their tree is worth tens of gold. **No
   tech deep can legally give is worth four figures to the player it
   most wants to arm.** The two facts are jointly fatal and neither
   moves with tuning.

The census was built to make this legible rather than silent: one line
per eligible candidate whether or not an offer follows, carrying
`need` (what the cease-fire costs them) beside `reach` (the best offer
that exists after the safety veto). Four changes in this project have
measured as "no effect" while never executing.

What survives and is reusable: the menacer ranking off deep's own
border-pressure lattice, the distance test (the reachability law read
as "arming them cannot come back at us"), and `ai/default`'s tech-leak
veto inverted from a price adjustment into a targeting criterion, with
the embassy channel civ2civ3's leakage rule adds and stock has no term
for.

---

## 5. Culture in conquest worth: priced, executes, flat

    contested (T150)     OFF     delta      t   wins
    score              82.44     +0.76  +0.90   9/20
    unitskilled         9.06     +0.69  +1.18  12/20

    lategame-80 (T80)    OFF     delta      t   wins
    score             169.47     +0.15  +0.31   4/20
    unitsbuilt         39.67     -0.63  -2.21   1/20

Execution is not in doubt: 37 663 conquest valuations carried a culture
component on contested (mean 159 want) and 12 076 on lategame-80 (mean
82). The three conquest seats now read one function, which is worth
having on its own.

The arithmetic is the score's own — `pcity->history` transfers whole
because `transfer_city()` changes the owner on the same struct,
`player_culture()` then counts it, and `get_civ_score()` pays it at
1/50th of a point, valued at the citizen price that scores exactly one
point. DARKFX's culture FLOW credit cost −19.43 at the same face-value
weight; the STOCK is flat. The difference is population: the flow term
re-priced 830 533 improvements a cell, and the stock term only fires
where an assault is being valued.

**The venue warning DARKFX raised applies unchanged**: culture is 14.5%
of world score at T150 and 29.5% at T300, so a term whose whole value
is a score asset is being asked the wrong question by a 150-turn cell.
Both cells here are short-horizon, and the long-horizon cell remains
unreachable from this harness.

---

## 6. Ethics of play, stated for the changelog

With `proxy_arm_gain` above zero this AI arms your rivals to keep you
busy. It will not arm anyone allied to you, and under civ2civ3's
Embassies leakage it will not arm anyone you hold an embassy with,
because both hand the advance back to you — but the intent is exactly
what it looks like, and it is a real treaty with a real third party.

With `gift_love_gain` above zero this AI buys friendship with
technology. A human can farm that: hold deep's love just under its own
peace bar and collect an advance. Section 3b says the farming works —
the counterparty is the one who gains.

Both ship at zero.

---

## 7. What the next session must know

- **The primary metric cannot see a positive-sum mechanism.** This is
  the first lever in the project whose self-play and head-to-head
  readings disagree in SIGN of usefulness, and the machinery to catch
  it already existed: `mixed_dc` seats deep against `ai/default` in one
  game. Any future lever that gives something to another player must
  be run through it. (00-context §6 anticipated the reverse failure —
  an AI tuned to beat `ai/default` specifically — and this is its
  mirror.)
- **The mixed pair was run unmirrored.** `mixed_dc` only, not
  `mixed_cd`. Seat bias is identical in both arms so the paired delta
  is sound, but the LEVELS carry it; a mirrored pair is the 40-seed
  version of this claim.
- **`gift_love_gain` needs an alliance clamp before it is ever swept**
  (§3c), and the clamp TRADE already ships is the model.
- **Nothing about the gift levers is tuned.** 100 x 100 was chosen to
  make the willingness bound non-binding so a refusal would be the
  counterparty's, not the budget's. The pair `gift_border_value` x
  `gift_love_gain` is a product and has to be swept as one.
- **The gold instrument is dead on these cells and the reason is
  structural**, not a tuning miss: `dai_gold_reserve` plus a rush-buy
  loop that empties the treasury every turn. A gold-denominated
  diplomacy lever needs a treasury policy first.
- **Proxy arming cannot be revived by tuning** (§4). It could be
  revived by a ruleset with `tech_trade_allow_holes = TRUE`, or by a
  cell where distant powers are met early enough that love has not
  bled — neither of which is in the suite.
- **`deep_peace_worth`'s trade half has never been exercised.** Zero
  routes on both civ2civ3 cells. It is live on classic and no classic
  arm was run.
