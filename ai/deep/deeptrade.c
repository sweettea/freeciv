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
#include "unit.h"
#include "unitlist.h"

/* common/aicore */
#include "caravan.h"

/* server */
#include "notify.h"
#include "srv_log.h"

/* ai */
#include "handicaps.h"

/* ai/default */
#include "daidata.h"
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

  if (tune->caravan_discount <= 0.0 || tune->caravan_discount > 1.0) {
    /* Out of range for caravan_parameter_is_legal(); leave stock to it. */
    return;
  }

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
