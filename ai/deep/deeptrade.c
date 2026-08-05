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
#include "map.h"
#include "player.h"
#include "traderoutes.h"
#include "traits.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* common/aicore */
#include "caravan.h"

/* server */
#include "notify.h"
#include "srv_log.h"

/* server/advisors */
#include "advbuilding.h"
#include "advchoice.h"
#include "advdata.h"
#include "advtools.h"

/* ai */
#include "aitraits.h"
#include "handicaps.h"

/* ai/default */
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
  Substitute deep's amortized caravan want for stock's at the build
  seam, for one city. Called with the choice the domestic advisor just
  returned, before it is merged against the military choice.

  Three outcomes, all logged:

  - REPLACE: stock's trade bid is the returned winner, so the want it
    won with is overwritten by deep's. When that is a REDUCTION the
    winner it beat has to come back, or the city builds a caravan deep
    has just said is not worth it. The improvement runner-up is
    recoverable exactly -- building_advisor_choose() reads
    building_want[] and writes a choice, with no side effect of any
    kind, and stock's builder-trait scaling is one multiplication --
    so it is recovered. The worker, founder and wonder runners-up are
    not: their producers all reach dai_wants_role_unit(). The
    censoring is counted (recover=partial) rather than assumed away.
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
    if (merit < choice->want) {
      struct adv_choice *runner = adv_new_choice();

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
        partial = TRUE;
      }
      adv_free_choice(runner);
    } else {
      choice->want = merit;
    }
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
