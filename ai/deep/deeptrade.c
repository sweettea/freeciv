/***********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "fcintl.h"
#include "log.h"
#include "support.h"

/* common */
#include "city.h"
#include "game.h"
#include "improvement.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "traderoutes.h"
#include "traits.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* common/aicore */
#include "caravan.h"

/* server */
#include "citytools.h"
#include "notify.h"
#include "srv_log.h"

/* server/advisors */
#include "advbuilding.h"
#include "advchoice.h"
#include "advdata.h"
#include "advtools.h"
#include "infracache.h"

/* ai */
#include "aitraits.h"
#include "handicaps.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daidomestic.h"
#include "daiplayer.h"
#include "daitools.h"
#include "daiunit.h"

#include "deepdefence.h"
#include "deeptuning.h"

#include "deeptrade.h"

#define DEEP_TRADE_BRIEF_MAX 3

/* Trade-route destination choice.
 *
 * The valuation itself is NOT reimplemented. common/aicore/caravan.c is
 * a genuine net-present-value model -- infinite horizon, a documented
 * per-turn discount, perpetuity and annuity helpers -- and is the most
 * carefully priced code in the AI. Deep replaces only the PARAMETERS
 * stock feeds it, which is where both defects live, and leaves stock's
 * dai_manage_caravan() to execute the move: it re-searches only when
 * the unit's task is AIUNIT_NONE, so setting the task here pre-empts
 * the decision while keeping stock's ferry handling, tired-of-waiting
 * logic and goto untouched. No function is copied.
 *
 * Why it matters. Under CBS_CLASSIC the one-shot windfall for
 * establishing a route is
 *
 *   tb = (weighted_distance + 10)
 *        * (src trade surplus + dest trade surplus) / 24
 *
 * doubled when the bonus pays into science AND gold (traderoutes.c:478,
 * caravan.c:275). It is multiplicative in distance and in both
 * endpoints' size, so the best play available is a long route between
 * two big cities -- a whole tech plus a treasury, from a unit costing a
 * fraction of a wonder. See findings/trade-routes.md.
 */

/**********************************************************************//**
  How far afield this player will look for a trade partner, as a
  caravan.c foreign-trade level.

  Stock re-rolls fc_rand(2) per unit per call (daiunit.c:2452),
  alternating between FTL_ALLIED and FTL_NONWAR, so half the time the
  best destination on the board is not even a candidate -- and the
  foreign bonus is exactly what makes a long route worth walking. That
  is RC2's "statistical mechanics rather than logic" stated literally.

  The evaluator already prices every candidate, including the war risk
  implicit in who owns the destination, so widening the candidate set
  cannot make it choose worse -- it can only give it more to compare.
  The level is a tunable rather than a constant so the tuner can pull
  it back if losing routes to later wars outweighs the reach.
**************************************************************************/
static enum foreign_trade_limit deep_caravan_reach(void)
{
  switch (deep_tuning_get()->caravan_foreign_trade) {
  case 0:  return FTL_NATIONAL_ONLY;
  case 1:  return FTL_ALLIED;
  case 2:  return FTL_PEACEFUL;
  default: return FTL_NONWAR;
  }
}

/**********************************************************************//**
  One line per turn telling our allies which cities our caravans have
  just set out for, and roughly when to expect them.

  PURELY SOCIAL, like deep_brief_allies() in deepops.c: nothing reads it
  and no state changes. It exists because a caravan crossing an ally's
  territory for ten turns is otherwise indistinguishable from an
  incursion, and because "look for them in about N turns" is the sort of
  thing an ally says.

  Aggregated deliberately. Announcing per unit would put six lines in
  the chat on the turn a player's caravans go idle together; this emits
  at most one, naming the first few destinations and counting the rest.
  The ETA quoted is the LONGEST of the batch, so it reads as "by then"
  rather than promising the earliest arrival for all of them.
**************************************************************************/
static void deep_brief_allies_trade(const struct player *pplayer,
                                    const char *const *dests, int ndest,
                                    int ntrade, int maxeta)
{
  char list[256] = "";
  int i;

  if (ndest <= 0) {
    return;
  }
  for (i = 0; i < ndest; i++) {
    if (i > 0) {
      fc_strlcat(list,
                 (i == ndest - 1 && ntrade == ndest) ? _(" and ") : _(", "),
                 sizeof(list));
    }
    fc_strlcat(list, dests[i], sizeof(list));
  }
  if (ntrade > ndest) {
    cat_snprintf(list, sizeof(list),
                 PL_(" and %d other", " and %d others", ntrade - ndest),
                 ntrade - ndest);
  }

  players_iterate_alive(ally) {
    if (ally == pplayer || !pplayers_allied(pplayer, ally)) {
      continue;
    }
    notify_player(ally, NULL, E_DIPLOMACY, ftc_chat_private,
                  PL_("*%s (AI)* We are sending a caravan to %s to open a"
                      " trade route -- look for it in about %d turn(s).",
                      "*%s (AI)* We are sending caravans to %s to open"
                      " trade routes -- look for them in about %d turn(s).",
                      ntrade),
                  player_name(pplayer), list, MAX(maxeta, 1));
  } players_iterate_alive_end;
}

/**********************************************************************//**
  Pick destinations for idle caravans, ahead of stock's unit loop.

  Only units with no current task are touched: one that is already
  walking to a destination keeps it, and stock's own validity check
  still tears it up if the destination stops being legal.
**************************************************************************/
void deep_choose_trade_targets(struct ai_type *ait, struct player *pplayer)
{
  const struct civ_map *nmap = &(wld.map);
  const struct deep_tuning *tune = deep_tuning_get();
  int chosen = 0;
  /* Aggregated for ONE ally briefing per turn, rather than one line per
   * caravan: a player with six idle caravans should get a sentence, not
   * a wall. Only trade destinations are collected -- wonder help is
   * domestic and is nobody else's business. */
  const char *dests[DEEP_TRADE_BRIEF_MAX];
  int ndest = 0, maxeta = 0, ntrade = 0;

  /* Legality is caravan.c's own test, not a re-derivation of it. The
   * re-derived guard admitted exactly 1.0 -- which
   * caravan_parameter_is_legal() rejects, because an infinite horizon
   * with no discount is an infinite reward: perpetuity() divides by
   * (1 - discount) and every destination scores inf or NaN, making the
   * choice arbitrary. 1.0 was inside the registered tuner range, so a
   * campaign could draw it. */

  unit_list_iterate_safe(pplayer->units, punit) {
    struct caravan_parameter parameter;
    struct caravan_result result;
    struct city *homecity;

    if (def_ai_unit_data(punit, ait)->task != AIUNIT_NONE) {
      continue;
    }
    if (!unit_can_do_action(punit, ACTION_TRADE_ROUTE)
        && !unit_can_do_action(punit, ACTION_MARKETPLACE)
        && !unit_can_do_action(punit, ACTION_HELP_WONDER)) {
      continue;
    }
    homecity = game_city_by_number(punit->homecity);
    if (homecity == NULL) {
      /* Stock will try to find one; let it, and pick this unit up next
       * turn rather than duplicating search_homecity_for_caravan(). */
      continue;
    }

    caravan_parameter_init_from_unit(&parameter, punit);

    /* ONE transit-time convention. Stock switches this on a continent
     * test (daiunit.c:2466): a cross-continent route is evaluated with
     * ignore_transit_time = TRUE -- infinite patience -- while a
     * same-continent one is discounted at 0.95^turns. A 30-turn sea
     * voyage is therefore free while a 15-turn land walk keeps 46%.
     * Either the walk costs something or it does not; two opposite
     * answers selected by geography is not a policy.
     *
     * Deep always counts transit and expresses patience as ONE number.
     * That matters most where the prize is biggest: the windfall grows
     * with distance, so an impatient discount bites hardest on exactly
     * the routes worth having. */
    parameter.ignore_transit_time = FALSE;
    parameter.discount = tune->caravan_discount;

    parameter.allow_foreign_trade = deep_caravan_reach();

    if (!caravan_parameter_is_legal(&parameter)) {
      /* Leave this unit to stock rather than evaluate destinations
       * against a divergent NPV. */
      continue;
    }

    caravan_result_init_zero(&result);
    caravan_find_best_destination(nmap, punit, &parameter, &result,
                                  !has_handicap(pplayer, H_MAP));

    if (result.dest != NULL && result.value > 0.0) {
      dai_unit_new_task(ait, punit,
                        result.help_wonder ? AIUNIT_WONDER : AIUNIT_TRADE,
                        result.dest->tile);
      chosen++;
      /* Only routes that LEAVE our own territory are anybody else's
       * business. A caravan walking from one of our cities to another
       * of our cities is domestic housekeeping; announcing it to an
       * ally is noise, and it was: Europe T122 briefed a human ally
       * about a Carthaginian caravan going from Carthaginian city 528
       * to Carthaginian city 1088. */
      if (!result.help_wonder && city_owner(result.dest) != pplayer) {
        ntrade++;
        maxeta = MAX(maxeta, result.arrival_time);
        if (ndest < DEEP_TRADE_BRIEF_MAX) {
          dests[ndest++] = city_name_get(result.dest);
        }
      }
      log_base(deep_metric_log_level(),
               "deep-trade turn=%d player=\"%s\" unit=%d src=%d dest=%d"
               " kind=%s eta=%d value=%.1f dist=%d",
               game.info.turn, player_name(pplayer), punit->id,
               homecity->id, result.dest->id,
               result.help_wonder ? "wonder" : "route",
               result.arrival_time, result.value,
               real_map_distance(city_tile(homecity),
                                 city_tile(result.dest)));
    }
  } unit_list_iterate_safe_end;

  if (chosen > 0) {
    log_base(deep_metric_log_level(),
             "deep-trade turn=%d player=\"%s\" chosen=%d reach=%d"
             " discount=%.3f",
             game.info.turn, player_name(pplayer), chosen,
             (int) deep_caravan_reach(), tune->caravan_discount);
  }

  if (ntrade > 0) {
    deep_brief_allies_trade(pplayer, dests, ndest, ntrade, maxeta);
  }
}

/* ------------------------------------------------------------------
 * The BUILD want (the trade-merit epic).
 *
 * Everything above decides what a caravan that already EXISTS should
 * do. Nothing above decides to build one: that is still stock's
 * dai_choose_trade_route(), and its income estimate under CBS_CLASSIC
 * is
 *
 *   income = (10 + 10) * (1.75 * pcity->surplus[O_TRADE]) / 24
 *
 * (daidomestic.c:360) -- a hardcoded distance of TEN, only the source
 * city's trade, and no destination at all, against a windfall that is
 * multiplicative in the real distance and in BOTH endpoints' surplus.
 * Deep raised what a caravan is worth and left the price of deciding
 * to build one exactly where it was, so caravans are under-built in
 * proportion to how much better the destinations got
 * (findings/deep-dive-2026-08-04/economy-growth.md section 3.8).
 *
 * The NPV that answers this is already computed and already thrown
 * away: caravan_result.value, used above only as a > 0 gate (audit
 * section 2.4). This is the consumer.
 * ------------------------------------------------------------------ */

/**********************************************************************//**
  How many caravans this city has already committed to the trade
  errand -- supported units that can establish a route. A caravan
  walking to a destination is still supported, so this is the
  assigned-and-enroute count without a second traversal, and it is the
  same set stock counts (daidomestic.c:322) before throwing the number
  away on a flat clamp.
**************************************************************************/
static int deep_committed_caravans(const struct city *pcity)
{
  int n = 0;

  unit_list_iterate(pcity->units_supported, punit) {
    if (unit_can_do_action(punit, ACTION_TRADE_ROUTE)) {
      n++;
    }
  } unit_list_iterate_end;

  return n;
}

/**********************************************************************//**
  What a caravan built in this city would be CARRYING.

  A virtual caravan carries nothing, and on any ruleset whose
  goods_selection is "Leaving" -- which is civ2civ3's, i.e. every
  economy cell this project runs -- that is fatal rather than merely
  inaccurate: actres.c:927 reads unit_current_goods(), which returns
  punit->carrying unexamined, and dereferences it one line later. A
  speculative ACTION_TRADE_ROUTE against a unit that was not created by
  the server therefore segfaults. Measured, not reasoned: isolated-150
  seed 1, turn 127, actres_possible + 0x259. Filed in
  findings/upstream-bugs.md; deep does not fix it in place, it stops
  handing the function a unit it cannot cope with.

  The pick is goods_from_city_to_unit()'s, minus its fc_rand() tie-
  break between goods of equal select_priority. A PLANNER must not
  consume the game's random state -- an evaluation that moves the RNG
  is an evaluation that changes the game it is evaluating -- and on the
  rulesets in use the tie-break has nothing to break: civ2civ3 defines
  exactly one goods type. Where a ruleset defines several at equal
  priority the real unit may leave with a different one, whose
  replace_priority could differ; that is a valuation inaccuracy and is
  the correct trade for determinism.
**************************************************************************/
static struct goods_type *deep_ghost_goods(const struct city *pcity,
                                           const struct unit *ghost)
{
  struct goods_type *best = NULL;
  int prio = -1;

  goods_type_iterate(pgood) {
    if (goods_can_be_provided(pcity, pgood, ghost)
        && pgood->select_priority > prio) {
      prio = pgood->select_priority;
      best = pgood;
    }
  } goods_type_iterate_end;

  return best;
}

/**********************************************************************//**
  What a caravan built here would be worth, as a build want.

  The valuation is caravan.c's, run against the REAL destinations
  reachable from this city by a unit of this type -- the same model,
  the same discount and the same reach the caravan will be given the
  moment it exists (deep_choose_trade_targets above), so the price and
  the plan cannot disagree (RC5).

  Three deliberate differences from the destination call, each of
  which is also what makes this affordable to run per city per turn:

  1. horizon = MORT rather than FC_INFINITY. Every other deep credit
     is summed over MORT turns; an infinite horizon also turns the
     ongoing route into perpetuity() -- a 20x multiplier at 0.95 --
     and makes the pathfinder walk the entire map instead of stopping
     at the planning horizon. A route the caravan cannot even REACH
     inside the horizon is not an argument for building it. (The
     executing caravan keeps the infinite horizon: it is choosing
     among destinations, not deciding whether to exist.)
  2. consider_wonders = FALSE. The wonder caravan is a separate
     producer with a separate want (dai_choose_help_wonder,
     daidomestic.c:66) and this epic does not touch it. Pricing the
     wonder here would double-count it against itself.
  3. consider_trade is switched off when every route slot this city
     has is already claimed by a caravan in the field. That is the
     honest marginal value of the Nth caravan, and it replaces stock's
     "if unassigned_caravans * 10 > want then want = 0.1" -- a clamp
     that answers a real question (the second caravan is worth less)
     with a number that is not an answer. What is genuinely left for
     an over-committed city is the one-shot windfall, and caravan.c
     prices that correctly on its own.

  The result is an NPV in raw output points at the moment the caravan
  would set out. Two conversions bring it to a build want:

  - divide by MORT. Build wants are PER-TURN weighted output deltas
    (base_want measures what a building adds to this turn's output),
    and an NPV is a stock. MORT is exactly the annuity factor of
    amortize()'s own rate: its discount is 1 - 1/MORT, so a perpetual
    per-turn flow of x is worth x * MORT and the inverse is the
    division here. No new constant.
  - price an output point at adv->science_priority, which is what
    deep_unworked_tile_value() already pays for a point of tile trade
    everywhere else in the module. The windfall's science-AND-gold
    double payout is already applied by caravan.c (caravan.c:275), so
    it must not be applied again here.

  Finally amortize() over the turns until the caravan is BUILT, the
  same way the horizon credits amortize over time-to-grow; the walk to
  the destination is already discounted inside the NPV, so that delay
  must not be charged twice.

  Known blindness, stated rather than hidden: caravan.c's optimizer
  "does not take into account other caravans in transit", so two
  cities can each price the same destination. The slot test above
  catches the case that matters (one city over-committing); two cities
  racing for one partner is not modelled.
**************************************************************************/
static adv_want deep_trade_merit_want(struct ai_type *ait,
                                      struct player *pplayer,
                                      struct city *pcity,
                                      struct unit_type *utype,
                                      struct adv_data *adv,
                                      struct caravan_result *result,
                                      int *committed, int *slots,
                                      int *build)
{
  const struct civ_map *nmap = &(wld.map);
  const struct deep_tuning *tune = deep_tuning_get();
  struct caravan_parameter parameter;
  struct unit *ghost;
  struct universal target = { .kind = VUT_UTYPE, .value.utype = utype };
  adv_want want;
  int build_turns;

  caravan_result_init_zero(result);
  *build = -1;
  *committed = deep_committed_caravans(pcity);
  *slots = max_trade_routes(pcity) - city_num_trade_routes(pcity);

  ghost = unit_virtual_create(pplayer, pcity, utype, 0);
  ghost->carrying = deep_ghost_goods(pcity, ghost);
  if (ghost->carrying == NULL) {
    /* Nothing this city could load. The speculation below would read
     * the null straight through actres.c:927, and a caravan that can
     * carry nothing is worth nothing anyway. */
    unit_virtual_destroy(ghost);
    return 0;
  }
  caravan_parameter_init_from_unit(&parameter, ghost);
  parameter.horizon = MORT;
  parameter.consider_wonders = FALSE;
  parameter.ignore_transit_time = FALSE;
  parameter.discount = tune->caravan_discount;
  parameter.allow_foreign_trade = deep_caravan_reach();
  if (*slots - *committed <= 0) {
    parameter.consider_trade = FALSE;
  }

  if (!caravan_parameter_is_legal(&parameter)) {
    unit_virtual_destroy(ghost);
    return 0;
  }

  caravan_find_best_destination(nmap, ghost, &parameter, result,
                                !has_handicap(pplayer, H_MAP));
  unit_virtual_destroy(ghost);

  if (result->dest == NULL || result->value <= 0.0) {
    return 0;
  }

  /* The delay is DISCOUNTED, not gated. A city three turns from
   * finishing a caravan and a city thirty turns from one are answering
   * the same question with different patience, which is exactly what
   * amortize() is for; a cutoff at the horizon would be a second,
   * unearned constant, and city_turns_to_build() returns FC_INFINITY
   * for a city with no shield surplus, which amortize() takes to zero
   * on its own. */
  build_turns = city_turns_to_build(pcity, &target, TRUE);
  *build = build_turns;

  want = amortize((adv_want) (result->value * adv->science_priority / MORT),
                  MAX(build_turns, 0))
    * tune->trade_merit_gain;

  return MIN(want, DAI_WANT_DOMESTIC_MAX);
}

/**********************************************************************//**
  Read stock's caravan bid without disturbing anything.

  dai_choose_trade_route() writes only its choice argument and, through
  dai_wants_role_unit(), the player's tech_want[] array. Snapshotting
  and restoring that array makes this probe provably inert: the real
  call inside domestic_advisor_choose_build() still contributes its
  tech want exactly once, which is why the number cannot be obtained by
  simply running the domestic advisor a second time.

  The bid is needed as a NUMBER for two things. Recognising it: a
  returned CT_CIVILIAN caravan may equally have come from the WONDER
  producer, which shares the unit type and which this epic leaves
  alone, and ADV_CHOICE_TRACK's producer tag is compiled out by
  default. And calibration: logging deep's price beside stock's at the
  identical seam is the corpus that says whether the two live on the
  same scale -- the question R1 could not answer for the rates because
  it had nothing to compare against.
**************************************************************************/
static adv_want deep_stock_trade_bid(struct ai_type *ait,
                                     struct player *pplayer,
                                     struct city *pcity,
                                     struct adv_data *adv,
                                     struct unit_type **utype_out)
{
  struct ai_plr *plr_data = def_ai_player_data(pplayer, ait);
  adv_want saved[A_LAST + 1];
  struct adv_choice *probe = adv_new_choice();
  adv_want bid;

  memcpy(saved, plr_data->tech_want, sizeof(saved));
  dai_choose_trade_route(ait, pcity, probe, adv);
  memcpy(plr_data->tech_want, saved, sizeof(saved));

  bid = probe->want;
  *utype_out = (probe->want > 0 && probe->type == CT_CIVILIAN)
    ? probe->value.utype : NULL;
  adv_free_choice(probe);

  return bid;
}

/**********************************************************************//**
  Overwrite a winning caravan bid with deep's price, bringing the
  runner-up back when that is a REDUCTION.

  Shared by both caravan producers, because the hazard is the same one:
  if deep lowers the want a caravan won with and nothing else is
  reconsidered, the city still builds the caravan deep has just said is
  not worth it -- at a lower want, which changes nothing it decides.

  The improvement runner-up is recoverable exactly:
  building_advisor_choose() reads building_want[] and calls the AI's
  choose_building hook, and neither writes anything else, so it can be
  re-run. Stock's builder-trait scaling is the one multiplication that
  follows it in domestic_advisor_choose_build(). The worker, founder
  and wonder runners-up are NOT recoverable -- their producers all
  reach dai_wants_role_unit(), which writes the player's tech_want[] --
  so when the improvement does not clear the merit the want is simply
  lowered and the censoring is COUNTED (recover=partial) rather than
  assumed away.

  Returns TRUE if the runner-up was recovered; sets *partial when it
  was not.
**************************************************************************/
static bool deep_caravan_replace(struct player *pplayer, struct city *pcity,
                                 struct adv_choice *choice, adv_want merit,
                                 bool *partial)
{
  struct adv_choice *runner;
  bool recovered = FALSE;

  if (merit >= choice->want) {
    choice->want = merit;
    return FALSE;
  }

  runner = adv_new_choice();
  building_advisor_choose(pcity, runner);
  runner->want = runner->want
    * (0.5 + (ai_trait_get_value(TRAIT_BUILDER, pplayer)
              / (double) TRAIT_DEFAULT_VALUE / 2));
  if (runner->want > merit && runner->type == CT_BUILDING
      && runner->value.building != NULL) {
    adv_choice_copy(choice, runner);
    choice->want = MIN(runner->want, DAI_WANT_DOMESTIC_MAX);
    recovered = TRUE;
  } else {
    choice->want = merit;
    *partial = TRUE;
  }
  adv_free_choice(runner);

  return recovered;
}

/**********************************************************************//**
  Substitute deep's amortized caravan want for stock's at the build
  seam, for one city. Called with the choice the domestic advisor just
  returned, before it is merged against the military choice.

  Three outcomes, all logged:

  - REPLACE: stock's trade bid is the returned winner, so the want it
    won with is overwritten by deep's, and the runner-up comes back
    when that is a reduction (deep_caravan_replace above).
  - PROMOTE: stock's bid lost but deep's price beats the winner.
  - KEEP: neither. Logged anyway, because the distribution of deep's
    want against stock's at the cities where nothing happens is most
    of the calibration corpus.

  trade_merit = 0 skips all of it, including the probe, and is the
  compiled default.
**************************************************************************/
void deep_trade_merit_apply(struct ai_type *ait, struct player *pplayer,
                            struct city *pcity, struct adv_choice *choice)
{
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  struct caravan_result result;
  struct unit_type *utype, *stock_utype = NULL;
  adv_want stock_bid, merit;
  int committed = 0, slots = 0, build_turns = -1;
  const char *mode = "keep";
  bool recovered = FALSE, partial = FALSE;

  if (deep_tuning_get()->trade_merit <= 0) {
    return;
  }

  utype = best_role_unit(pcity, action_id_get_role(ACTION_TRADE_ROUTE));
  if (utype == NULL) {
    /* Cannot build one now. Stock's bid, if any, is a tech-stimulation
     * bid that never becomes a build order, and deep has no opinion
     * about a unit that does not exist yet. */
    return;
  }

  stock_bid = deep_stock_trade_bid(ait, pplayer, pcity, adv, &stock_utype);
  merit = deep_trade_merit_want(ait, pplayer, pcity, utype, adv, &result,
                                &committed, &slots, &build_turns);

  if (stock_bid > 0 && stock_utype != NULL
      && choice->type == CT_CIVILIAN
      && choice->value.utype == stock_utype
      && choice->want == stock_bid) {
    /* Stock's trade producer is the domestic winner. */
    mode = "replace";
    recovered = deep_caravan_replace(pplayer, pcity, choice, merit, &partial);
  } else if (merit > choice->want) {
    mode = "promote";
    choice->type = CT_CIVILIAN;
    choice->value.utype = utype;
    choice->want = merit;
    choice->need_boat = FALSE;
    adv_choice_set_use(choice, "deep trade merit");
  }

  log_base(deep_metric_log_level(),
           "deep-trade-merit turn=%d player=\"%s\" city=%d mode=%s"
           " stock=%.1f merit=%.1f npv=%.1f eta=%d build=%d dest=%d"
           " committed=%d slots=%d recover=%s",
           game.info.turn, player_name(pplayer), pcity->id, mode,
           (double) stock_bid, (double) merit, result.value,
           result.arrival_time, build_turns,
           result.dest != NULL ? result.dest->id : -1,
           committed, slots,
           recovered ? "yes" : (partial ? "partial" : "-"));
}

/* ------------------------------------------------------------------
 * The WONDER caravan (the wonder-merit epic, R2b).
 *
 * Everything above prices the caravan that opens a trade route. This
 * prices the other one, and R2's own census says it is the bigger
 * half: of 354 caravan build decisions per game on primary-120-classic
 * the WINNING want has p50 197, while stock's trade bid has p50 71.
 * The caravan build queue belongs to dai_choose_help_wonder()
 * (daidomestic.c:71), which prices a wonder caravan as
 *
 *   want = wonder_city->building_want[wonder] / MAX(walk_turns, 1)
 *
 * behind a count gate: build another while
 *
 *   build_points_left(wonder_city) > one caravan's shields * caravans
 *
 * where caravans is every help-wonder-capable unit on this continent,
 * fielded or in production.
 *
 * The gate is not the problem -- it is a real marginality test, just a
 * boolean one. The want is. It credits a caravan with the WHOLE
 * wonder's want no matter what fraction of the wonder its shields
 * actually buy, and it divides by the walk instead of discounting it,
 * which is the same undiscounted arithmetic R2 found on the trade
 * side and which no other deep credit uses.
 *
 * D2 made building_want[] honest in the wonder city. This re-
 * denominates it PER SHIELD DELIVERED. Nothing new is invented: the
 * output is the wonder city's own want, multiplied by a share and
 * amortized over a delay.
 * ------------------------------------------------------------------ */

/* Everything the wonder price is made of, kept together so the census
 * line can print the inputs beside the answer. A want that cannot be
 * decomposed in the log is a want nobody can calibrate (R3). */
struct deep_wonder_case {
  const struct city *wcity;         /* the wonder city, or NULL */
  adv_want wonder_want;             /* its building_want for the wonder */
  int cost;                         /* the wonder's full shield cost */
  int left;                         /* shields still to find */
  int pledged;                      /* shields already walking or on order */
  int deliver;                      /* what THIS caravan would put in */
  int usable;                       /* the part of that which is not surplus */
  int walk;                         /* turns to walk there */
  int build;                        /* turns to build it here */
};

/**********************************************************************//**
  How many shields of help are already committed to the wonder.

  The POPULATION is stock's, deliberately: every unit on this continent
  that can do Help Wonder, plus every one in production on it
  (daidomestic.c:104-120). Stock then counts them; deep asks each what
  it would deliver, through the same unit_shield_value() the server
  charges at the moment of delivery (unithand.c:5970), so a ruleset
  that pays a Freight more than a Caravan is priced rather than
  averaged.

  Not narrowed to units actually tasked AIUNIT_WONDER. An idle caravan
  on this continent is exactly what the next wonder call will send, and
  a caravan whose route was better spent elsewhere is a bid this
  producer does not get to make. Counting stock's set keeps the two
  prices comparable at the seam, which is the whole point of the
  census.
**************************************************************************/
static int deep_pledged_wonder_shields(const struct player *pplayer,
                                       Continent_id continent,
                                       const struct action *paction)
{
  int shields = 0;

  unit_list_iterate(pplayer->units, punit) {
    if (unit_can_do_action(punit, ACTION_HELP_WONDER)
        && tile_continent(unit_tile(punit)) == continent) {
      shields += unit_shield_value(punit, unit_type_get(punit), paction);
    }
  } unit_list_iterate_end;

  city_list_iterate(pplayer->cities, acity) {
    if (VUT_UTYPE == acity->production.kind
        && utype_can_do_action(acity->production.value.utype,
                               ACTION_HELP_WONDER)
        && tile_continent(acity->tile) == continent) {
      shields += unit_shield_value(NULL, acity->production.value.utype,
                                   paction);
    }
  } city_list_iterate_end;

  return shields;
}

/**********************************************************************//**
  What a wonder caravan built HERE would be worth, as a build want.

  The value side. A caravan delivers its shields into the wonder city's
  current build, so what it buys is a SHARE of the wonder:

    want = building_want[wonder] * usable_shields / wonder_shield_cost

  building_want[] is the wonder city's own number, horizon-credited by
  deep and made honest by D2, so this session mints no scale and no
  second opinion about which wonder is worth having: it re-expresses
  the opinion the wonder city already holds, per shield.

  usable_shields is where stock's count gate goes, graded. The shields
  pledged by caravans already fielded or on order are subtracted from
  what the wonder still needs, and this caravan is credited only with
  what is left after them -- so the second caravan to a nearly funded
  wonder is worth a fraction of the first, and the one that arrives
  after the wonder is paid for is worth nothing, which is the same
  answer stock's boolean gives at the point where it flips.

  Deliberately NOT credited: the shields that overshoot the finish
  line. They are not lost (the city keeps its stock for the next
  build), but they are not what this caravan was priced for, and
  crediting them here would price a caravan as a shield-transfer
  service, which is a different mechanism with a different gate.

  The delay side. Two delays, both real and both currently ignored:
  the turns to BUILD the caravan (stock charges nothing for it -- the
  same omission R2 measured as the largest single term on the trade
  side) and the turns to WALK it there (stock divides by them, which
  is not a discount -- it makes a wonder 20 turns away worth 5% of one
  next door, where amortize() at MORT makes it 43%). Deep amortizes
  over their sum, the same amortize() every horizon credit uses.

  The walk is stock's own number, and it is already a real path:
  distance_to_wonder_city is a pathfinder move cost from the wonder
  city (daidomestic.c:643), so dividing by the unit's move rate gives
  turns over actual terrain rather than tiles as the crow flies. Zero
  means "unreachable inside seven turns of travel" and this producer
  declines, exactly as stock's does.
**************************************************************************/
static adv_want deep_wonder_merit_want(struct ai_type *ait,
                                       struct player *pplayer,
                                       struct city *pcity,
                                       struct unit_type *utype,
                                       struct adv_data *adv,
                                       struct deep_wonder_case *wc)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct ai_city *city_data = def_ai_city_data(pcity, ait);
  struct city *wonder_city = game_city_by_number(adv->wonder_city);
  const struct action *paction = action_by_number(ACTION_HELP_WONDER);
  const struct impr_type *wonder;
  struct universal target = { .kind = VUT_UTYPE, .value.utype = utype };
  int residual, move_rate;
  adv_want want;

  memset(wc, 0, sizeof(*wc));
  wc->walk = -1;
  wc->build = -1;

  /* Eligibility is stock's, unchanged (daidomestic.c:86-101): this
   * epic reforms the PRICE, not the population. A distance of zero
   * means the pathfinder never reached us. */
  if (pcity == wonder_city
      || wonder_city == NULL
      || city_data->distance_to_wonder_city <= 0
      || !city_production_gets_caravan_shields(&wonder_city->production)
      || VUT_UTYPE == wonder_city->production.kind
      || !is_wonder(wonder_city->production.value.building)) {
    return 0;
  }

  wonder = wonder_city->production.value.building;
  wc->wcity = wonder_city;
  wc->wonder_want
    = wonder_city->server.adv->building_want[improvement_index(wonder)];
  wc->cost = impr_build_shield_cost(wonder_city, wonder);
  wc->left = build_points_left(wonder_city);
  wc->deliver = unit_shield_value(NULL, utype, paction);
  wc->pledged = deep_pledged_wonder_shields(pplayer,
                                            tile_continent(pcity->tile),
                                            paction);

  if (wc->wonder_want <= 0 || wc->cost <= 0 || wc->left <= 0) {
    return 0;
  }

  residual = wc->left - wc->pledged;
  if (residual <= 0) {
    /* The wonder is already fully funded by help that exists. Stock's
     * gate says the same thing here; deep says it as a zero rather
     * than a branch, and says it on the way down as well. */
    return 0;
  }
  wc->usable = MIN(wc->deliver, residual);

  /* The rate the DISTANCE was measured in: dai_wonder_city_distance()
   * walks a fresh, undamaged unit of this role (daidomestic.c:673), so
   * veteran level 0 and full hit points are that unit, not an
   * optimistic reading. */
  move_rate = utype_move_rate(utype, city_tile(pcity), pplayer, 0,
                              utype->hp);
  if (move_rate <= 0) {
    return 0;
  }
  wc->walk = city_data->distance_to_wonder_city / move_rate;
  wc->build = city_turns_to_build(pcity, &target, TRUE);

  want = amortize((adv_want) (wc->wonder_want * wc->usable
                              / (double) wc->cost),
                  MAX(wc->walk, 0) + MAX(wc->build, 0))
    * tune->wonder_merit_gain;

  return MIN(want, DAI_WANT_DOMESTIC_MAX);
}

/**********************************************************************//**
  Read stock's wonder-caravan bid without disturbing anything.

  Same probe R2 built for the trade producer, against the producer next
  door. dai_choose_help_wonder() writes its choice argument and, through
  dai_wants_role_unit(), the player's tech_want[]; snapshotting and
  restoring that array is what makes the probe inert, and it is why the
  bid cannot be obtained by re-running domestic_advisor_choose_build()
  (which would double every producer's tech want, not just this one's).

  It reads the incoming choice as a FLOOR (it only ever raises the
  want), so the probe is handed a fresh zeroed choice and what comes
  back is the producer's own number.
**************************************************************************/
static adv_want deep_stock_wonder_bid(struct ai_type *ait,
                                      struct player *pplayer,
                                      struct city *pcity,
                                      struct adv_data *adv,
                                      const struct unit_type **utype_out)
{
  struct ai_plr *plr_data = def_ai_player_data(pplayer, ait);
  adv_want saved[A_LAST + 1];
  struct adv_choice *probe = adv_new_choice();
  adv_want bid;

  memcpy(saved, plr_data->tech_want, sizeof(saved));
  dai_choose_help_wonder(ait, pcity, probe, adv);
  memcpy(plr_data->tech_want, saved, sizeof(saved));

  bid = probe->want;
  *utype_out = (probe->want > 0 && probe->type == CT_CIVILIAN)
    ? probe->value.utype : NULL;
  adv_free_choice(probe);

  return bid;
}

/**********************************************************************//**
  Substitute deep's per-shield wonder-caravan want for stock's at the
  build seam, for one city.

  The three outcomes and the recovery are the trade path's, for the
  same reasons (see deep_trade_merit_apply); what differs is which
  producer is being repriced. The two switches are independent by
  design -- trade_merit is dead on every civ2civ3 cell this project
  runs, because civ2civ3 sets the domestic trade-route bonus to zero,
  while the wonder caravan is legal and alive on BOTH rulesets -- so
  folding them into one key would tie a live mechanism to a dead one.

  wonder_merit = 0 skips all of it, including the probe, and is the
  compiled default.
**************************************************************************/
void deep_wonder_merit_apply(struct ai_type *ait, struct player *pplayer,
                             struct city *pcity, struct adv_choice *choice)
{
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  struct deep_wonder_case wc;
  struct unit_type *utype;
  const struct unit_type *stock_utype = NULL;
  adv_want stock_bid, merit;
  const char *mode = "keep";
  bool recovered = FALSE, partial = FALSE;

  if (deep_tuning_get()->wonder_merit <= 0) {
    return;
  }

  if (num_role_units(action_id_get_role(ACTION_HELP_WONDER)) == 0) {
    return;
  }

  utype = best_role_unit(pcity, action_id_get_role(ACTION_HELP_WONDER));
  if (utype == NULL) {
    /* Cannot build one now. Stock's bid, if it makes one, exists only
     * to stimulate the tech, and deep has no opinion about the price
     * of a unit that cannot be ordered. */
    return;
  }

  stock_bid = deep_stock_wonder_bid(ait, pplayer, pcity, adv, &stock_utype);
  merit = deep_wonder_merit_want(ait, pplayer, pcity, utype, adv, &wc);

  if (stock_bid > 0 && stock_utype != NULL
      && choice->type == CT_CIVILIAN
      && choice->value.utype == stock_utype
      && choice->want == stock_bid) {
    mode = "replace";
    recovered = deep_caravan_replace(pplayer, pcity, choice, merit, &partial);
  } else if (merit > choice->want) {
    mode = "promote";
    choice->type = CT_CIVILIAN;
    choice->value.utype = utype;
    choice->want = merit;
    choice->need_boat = FALSE;
    adv_choice_set_use(choice, "deep wonder merit");
  }

  log_base(deep_metric_log_level(),
           "deep-wonder-merit turn=%d player=\"%s\" city=%d mode=%s"
           " stock=%.1f merit=%.1f wwant=%.1f cost=%d left=%d pledged=%d"
           " deliver=%d usable=%d walk=%d build=%d wcity=%d recover=%s",
           game.info.turn, player_name(pplayer), pcity->id, mode,
           (double) stock_bid, (double) merit, (double) wc.wonder_want,
           wc.cost, wc.left, wc.pledged, wc.deliver, wc.usable,
           wc.walk, wc.build,
           wc.wcity != NULL ? wc.wcity->id : -1,
           recovered ? "yes" : (partial ? "partial" : "-"));
}
