# Caravans and long-distance trade routes (2026-08-01)

Specced, not built. `ai/deep` contains **no caravan logic at all** --
two error strings mention the word and nothing else -- so it inherits
stock's behaviour wholesale.

The interesting part is that stock's caravan evaluator is *not* the
usual WAG. `common/aicore/caravan.c` is the most principled valuation
code anywhere in the AI, and its defects are therefore specific and
fixable rather than structural.

---

## 1. Why long routes are the ones worth having

`trade_base_between_cities()` (`common/traderoutes.c:332`) grows the
route's value with `real_map_distance` between the endpoints under
`TRS_CLASSIC`, plus city sizes, plus bonuses for different continents
and different players. **The best routes are long, foreign and
overseas.** That is the whole reason caravans are interesting: they are
one of the few investments whose payoff rises with the distance you are
willing to walk.

### 1a. The WINDFALL is the headline, not the per-turn route

The one-shot bonus for establishing the route is the big play, and it
is multiplicative in exactly the two things that make a route hard to
reach (`common/traderoutes.c:478`, `CBS_CLASSIC`):

    tb = weighted_distance + 10;
    tb = (tb * (pc1->surplus[O_TRADE] + pc2->surplus[O_TRADE])) / 24;

    /* caravan.c:275 */
    if (TBONUS_BOTH == ...) { bonus *= 2; }   /* science AND gold */

**distance x (combined trade surplus of BOTH endpoints)**, doubled when
the bonus pays into science and gold together. Two large cities far
apart is the highest-value one-shot action in the game -- a whole tech
plus a treasury, immediately, from a unit that costs a fraction of a
wonder.

`caravan.c` models this correctly (`consider_windfall`, on by default).

**Which makes the transit defect in §3 worse, not incidental.** The
largest windfalls are by construction the most distant ones, so
`0.95^turns` bites hardest exactly where the prize is biggest -- unless
the route happens to cross a continent boundary, where transit is
ignored outright and the discount disappears. The valuation is right
and the patience policy is incoherent, and the incoherence is aimed
straight at the best plays.

## 2. What stock already gets RIGHT

`caravan_parameter_init_default()` (`caravan.c:35`):

    horizon      = FC_INFINITY
    discount     = 0.95            /* per turn */
    consider_windfall / trade / wonders = TRUE
    account_for_broken_routes = TRUE

and `discount_relative()` is `payment * pow(rate, term)`, with proper
perpetuity and annuity NPV helpers beside it. This is a real
net-present-value model with a documented rate and an infinite horizon.
Nothing else in the AI is priced this carefully -- compare `base_want()`
measuring an improvement at today's city size, or PERFECTION's
compile-time 3 standing in as a settler travel discount
(`findings/europe-T80.md` §5a).

**So the fix here is not "write a valuation". It is to stop the two
things that corrupt an otherwise sound one.**

## 3. Defect A: transit time is treated incoherently

    parameter->ignore_transit_time = FALSE;          /* default */

    /* daiunit.c:2466 */
    if (dai_caravan_can_trade_cities_diff_cont(pplayer, punit)) {
      parameter.ignore_transit_time = TRUE;
    }

So a **cross-continent** route is evaluated with *infinite patience* --
transit time ignored entirely -- while a **same-continent** route is
discounted at `0.95^turns`. A 30-turn sea voyage is free; a 15-turn
land walk keeps 46%.

Either the walk costs something or it does not. Two opposite answers
selected by a continent test is not a policy.

And on the same-continent side the discount fights the very thing that
makes the route valuable: route worth grows roughly linearly with
distance while `0.95^t` decays exponentially, so beyond a modest range
the discount always wins. **Long domestic routes are structurally
under-chosen** -- the identical shape as the settler travel discount,
in a different subsystem.

    turns   0.95^t retained
       10     0.599
       20     0.358
       30     0.215
       40     0.129

## 4. Defect B: foreign trade is decided by a coin flip

    /* daiunit.c:2452 */
    if (fc_rand(2)) {
      parameter.allow_foreign_trade = FTL_NONWAR;
    } else {
      parameter.allow_foreign_trade = FTL_ALLIED;
    }

Every time a caravan is retasked, the AI flips a coin to decide whether
non-allied nations are even *considered* as destinations -- and foreign
routes carry the bonus that makes them worth the trip. The comment
frames it as deliberate ("Make more trade with allies than other
peaceful nations"), but a policy that is re-rolled per unit per call is
RC2's *"statistical mechanics rather than logic"* stated literally in
code.

## 5. What `ai/deep` should do

Not a new evaluator -- `caravan.c` is `common/aicore/`, shared and
sound. A destination *policy* on top of it:

1. **One transit-time convention.** Price the walk the same way whether
   it crosses water or not; if a sea leg is genuinely cheaper in
   opportunity cost because the convoy carries it, say that with a
   weight rather than by switching the term off. Deep already owns
   `convoy_min_load` / `convoy_wait_radius` for exactly this kind of
   sea logistics.
2. **Decide foreign trade on merit.** Replace `fc_rand(2)` with the
   diplomatic state deep already tracks -- war risk against route
   value, which is the same trade-off `deep_op_*` makes for targets.
3. **A caravan WANT, not just a destination.** Stock only decides what
   an existing caravan should do. Nothing decides to *build* one
   against the alternatives, which is the same gap W06 found for
   explorers -- and, per `findings/europe-T80.md`, the build queue is
   exactly where deep's arbitration is already failing. **Do not add a
   caravan want until that arbitration is fixed**, or it becomes the
   sixth claimant on a scale nobody owns.
4. **Any distance term goes in `struct tuning` as a PLACEHOLDER.**

## 6. Sequencing

This should come **after** the expansion/arbitration work
(`europe-T80.md`), not before. A caravan competes for shields with
settlers and buildings, and tonight's ablation showed that adding
claimants to that queue is precisely what suppressed expansion:
zeroing the seven horizon building credits raised the Settlers share
from ~18% to ~50% at turns 80-99.

It also pairs naturally with W31 naval doctrine -- overseas routes are
worth the most and need the transport the naval workstream is building.

## 7. Not yet measured

Nothing here has a benchmark number. Specifically unknown:

- how often deep builds caravans at all, and what it does with them
- whether `ignore_transit_time` on the cross-continent path is
  actually producing long overseas routes or just noise
- the ruleset spread: `trade_revenue_style` is `TRS_CLASSIC` here, and
  civ2civ3 differs from classic in route caps and bonuses, so any
  distance weight is ruleset-scoped until shown otherwise (the same
  caution as `findings/multiplier-column.md` §4a)
