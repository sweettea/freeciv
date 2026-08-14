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

#include <math.h>               /* pow() */

/* utility */
#include "fcintl.h"
#include "log.h"
#include "support.h"

/* common */
#include "city.h"
#include "game.h"
#include "improvement.h"
#include "map.h"
#include "maphand.h"
#include "movement.h"
#include "player.h"
#include "traderoutes.h"
#include "traits.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* common/aicore */
#include "caravan.h"
#include "path_finding.h"
#include "pf_tools.h"

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

#include "deepai.h"
#include "deepdefence.h"
#include "deepsettler.h"
#include "deeptuning.h"

#include "deepwonder.h"

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

/**********************************************************************//**
  How many active trade routes we hold with each other player.

  Counted from the cities, which is where the routes live: a route is
  an object with a value, not an intention, so this is the same
  quantity the final-save census counts and the two cannot drift.
  Domestic routes are excluded on purpose -- both endpoints are ours
  and no relationship is created.
**************************************************************************/
int deep_routes_with_players(const struct player *pplayer, int *routes,
                             int size)
{
  int total = 0;

  memset(routes, 0, sizeof(*routes) * size);
  city_list_iterate(pplayer->cities, pcity) {
    trade_routes_iterate(pcity, proute) {
      struct city *partner = game_city_by_number(proute->partner);
      int slot;

      if (partner == NULL || proute->value <= 0) {
        continue;
      }
      slot = player_index(city_owner(partner));
      if (slot < 0 || slot >= size || city_owner(partner) == pplayer) {
        continue;
      }
      routes[slot]++;
      total++;
    } trade_routes_iterate_end;
  } city_list_iterate_end;

  return total;
}

/**********************************************************************//**
  The per-turn trade our cities actually earn from routes with
  `aplayer`, summed over both endpoints we own.

  The sibling of deep_routes_with_players(), which counts routes; this
  one reads what they are WORTH, because a treaty is priced in a
  currency and a count is not one. `proute->value` is the live figure
  the city's own trade surplus is built from, so nothing is modelled
  here and nothing is estimated.
**************************************************************************/
int deep_route_value_with(const struct player *pplayer,
                          const struct player *aplayer)
{
  int total = 0;

  city_list_iterate(pplayer->cities, pcity) {
    trade_routes_iterate(pcity, proute) {
      const struct city *partner = game_city_by_number(proute->partner);

      if (partner == NULL || proute->value <= 0) {
        continue;
      }
      if (city_owner(partner) == aplayer) {
        total += proute->value;
      }
    } trade_routes_iterate_end;
  } city_list_iterate_end;

  return total;
}

/**********************************************************************//**
  Interdependence: an active trade route is a standing reason to keep
  the peace with the player at the other end of it.

  Human design call, 2026-08-05. Every active foreign route adds
  trade_route_love to love[partner] per turn, from deep's own phase
  wrapper -- struct deep_plr embeds struct ai_plr, so stock's war and
  peace code reads whatever love holds and NOTHING in ai/default
  changes. It is stateless by construction: stock decays love by 4% a
  turn (daidiplomacy.c), so the moment the routes stop being counted
  the addition unwinds itself. Nothing has to remember it, and a save
  that reloads mid-game re-derives it from the routes that exist.

  Why it belongs here rather than in the diplomacy module: it is the
  counterweight to a bleed that already exists. best_target costs a
  chosen victim up to 50 love a turn against a 125 peace bar, and it
  chooses on worth-over-distance -- which is systematically the RICH
  neighbour, i.e. the one worth trading with. PEACE measured 20-42% of
  pacified pairs stuck at cease-fire forever. This adds a pull in the
  opposite direction exactly where interdependence should create one,
  and compounds with PEACE's pre-pass: routes raise love, love clears
  the bar, peace keeps the routes legal.

  THE CLAMP IS STRUCTURAL, not a second knob. Trade love is only ever
  added while love[partner] stays below the player's OWN alliance
  requirement (ai->diplomacy.req_love_for_alliance, which varies by
  difficulty) -- so routes can carry a relationship to peace and can
  never, on their own, carry it into an alliance. Love raised past that
  bar by anything else is left alone; this only declines to push.

  Exploitability, stated rather than hidden: a rival who deliberately
  opens routes with deep can pacify it. That is real interdependence,
  it is symmetric (deep gets the same pull on them if they are deep),
  and it is tunable to zero.
**************************************************************************/
void deep_trade_route_love(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct deep_plr *dp = deep_plr_data(pplayer);
  int routes[MAX_NUM_PLAYER_SLOTS];
  struct ai_plr *ai;
  bool close_here;
  int cap, applied = 0, pairs = 0;

  if (tune->trade_route_love <= 0.0
      || !is_ai(pplayer) || is_barbarian(pplayer)) {
    return;
  }
  if (deep_routes_with_players(pplayer, routes,
                               ARRAY_SIZE(routes)) <= 0) {
    return;
  }

  ai = dai_plr_data_get(ait, pplayer, &close_here);
  cap = ai->diplomacy.req_love_for_alliance - 1;
  if (close_here) {
    dai_data_phase_finished(ait, pplayer);
  }

  players_iterate_alive(aplayer) {
    int slot = player_index(aplayer);
    int add, *love;

    if (aplayer == pplayer || slot < 0 || slot >= MAX_NUM_PLAYER_SLOTS
        || routes[slot] <= 0) {
      continue;
    }
    love = &pplayer->ai_common.love[slot];
    add = (int) (tune->trade_route_love * routes[slot]);
    if (add <= 0 || *love >= cap) {
      continue;
    }
    add = MIN(add, cap - *love);
    *love += add;
    applied += add;
    pairs++;
    dp->trade_love_applied++;
  } players_iterate_alive_end;

  if (pairs > 0) {
    log_base(deep_metric_log_level(),
             "deep-trade-love turn=%d player=\"%s\" pairs=%d added=%d"
             " cap=%d total=%d",
             game.info.turn, player_name(pplayer), pairs, applied, cap,
             dp->trade_love_applied);
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

/* ------------------------------------------------------------------
 * Deep's own route NPV (the portfolio epic).
 *
 * R2 priced a caravan with caravan.c's own model at horizon = MORT.
 * That horizon is the only thing in the tree that reaches
 * caravan.c's annuity(), and annuity() is INVERTED
 * (upstream-bugs.md): it computes
 *
 *   perpetuity(payment) * (1 - 1 / rate^term)
 *
 * where the closed form is perpetuity(payment) * (1 - rate^term). For
 * rate < 1 the bracket is negative and grows without bound in term, so
 * at rate 0.95 and MORT 24 a route worth one trade per turn is priced
 * at -48.5 where it is worth +14.2. Since get_discounted_reward() then
 * returns trade + windfall and compares it against the wonder branch, a
 * destination where a route CAN be established scores tens of points
 * BELOW one where it cannot -- exactly inverted.
 *
 * Deep cannot fix it in place: ai/baseline and ai/default both link
 * common/aicore, and both are measurement controls. So deep assembles
 * the NPV itself, from the same pricing primitives caravan.c uses
 * (common/traderoutes.c, which is correct) and its own three-line
 * discount arithmetic. What deep does NOT reimplement is the pricing:
 * trade_base_between_cities(), get_caravan_enter_city_trade_bonus() and
 * city_trade_removable() are called, not copied.
 *
 * Having its own assembly is also what makes the rest of this epic
 * expressible, and that is the reason to build it rather than a
 * workaround for the bug alone:
 *
 *  - the SLOT is a scarce asset, so the Nth caravan a city commits is
 *    worth the Nth-best destination, not the best one N times over;
 *  - a cease-fire partner's route has a KNOWN death date, so its
 *    annuity is truncated at the lapse rather than run to the horizon;
 *  - a route walked through danger is a route that may never open, so
 *    the NPV is discounted by the path's own risk.
 *
 * None of those can be said to caravan.c through its parameter struct.
 * ------------------------------------------------------------------ */

/* Enough for every city a caravan can reach inside the horizon on the
 * cells this project runs (measured p90 well under 30 on both). The
 * enumeration stops at the cap and says so on its census line rather
 * than silently truncating the comparison the epic rests on. */
#define DEEP_ROUTE_CAND_MAX 128

struct deep_route_cand {
  struct city *dest;
  int eta;                      /* turns until the caravan arrives */
  double windfall;              /* one-shot at arrival, undiscounted */
  double per_turn;              /* ongoing route income, undiscounted */
  int life;                     /* turns of ongoing income priced */
  double npv;                   /* the answer, discounted to now */
  double caravan_npv;           /* what caravan.c's assembly would say */
  double survival;              /* the transit-risk factor applied */
  bool ceasefire;               /* partner's route has a known death date */
};

/**********************************************************************//**
  A lump sum 'term' turns out, discounted to now. caravan.c's
  presentvalue(), which is correct.
**************************************************************************/
static double deep_route_pv(double payment, int term, double rate)
{
  return payment * pow(rate, term);
}

/**********************************************************************//**
  The present value, at the moment of arrival, of 'payment' per turn for
  'term' turns. This is the function caravan.c means and does not
  compute; see the comment above and upstream-bugs.md.

  rate >= 1 is not a discount at all and the sum is the undiscounted
  total; caravan_parameter_is_legal() already refuses rate > 1, and the
  branch exists so this function is total rather than trusting a caller.
**************************************************************************/
static double deep_route_annuity(double payment, int term, double rate)
{
  if (term <= 0) {
    return 0.0;
  }
  if (rate >= 1.0) {
    return payment * term;
  }

  return payment * (1.0 - pow(rate, term)) / (1.0 - rate);
}

/**********************************************************************//**
  The same annuity restricted to turns (from, to] after arrival -- the
  TAIL a cease-fire truncation refuses to count, which is also the part
  trade_ceasefire_peace_odds buys back.
**************************************************************************/
static double deep_route_annuity_tail(double payment, int from, int to,
                                      double rate)
{
  if (to <= from) {
    return 0.0;
  }

  return deep_route_annuity(payment, to, rate)
    - deep_route_annuity(payment, from, rate);
}

/**********************************************************************//**
  What one endpoint of a new route is worth per turn to US, net of what
  establishing it destroys.

  This is caravan.c's one_city_trade_benefit() re-derived from the
  public primitives, and it is the ONE LEDGER the epic's design call
  asks for: a city whose slots are full does not gain the new route's
  income, it gains the DIFFERENCE between the new route and the worst
  incumbent city_trade_removable() would evict -- and if the evicted
  partner is also ours, the income that partner loses on its own side of
  the same route is subtracted as well. Both halves of a domestic route
  belong to the same empire and only one ledger may count them.

  city_trade_removable() is the game's own answer to "what would this
  cost me", priority-ordered by the goods' replace_priority, and it is
  the function that decides in can_establish_trade_route() whether the
  route may be made at all. Deep asks it the value question at the same
  time the rules ask it the legality question, so the price and the rule
  cannot disagree.

  Guarded where caravan.c is not: game_city_by_number() on a route's
  partner can return NULL and caravan.c dereferences it (same
  upstream-bugs.md entry).
**************************************************************************/
static int deep_route_endpoint_gain(const struct city *pcity,
                                    const struct player *pplayer,
                                    const struct goods_type *pgood,
                                    int newtrade)
{
  int losttrade = 0;

  if (city_owner(pcity) != pplayer) {
    /* Someone else's city keeps the income from its own side. We still
     * care about what OUR cities lose when its slots are re-shuffled. */
    newtrade = 0;
  }

  if (city_num_trade_routes(pcity) >= (int) max_trade_routes(pcity)) {
    struct trade_route_list *would_remove = trade_route_list_new();
    int oldtrade = city_trade_removable(pcity, pgood->replace_priority,
                                        would_remove);

    if (city_owner(pcity) == pplayer) {
      newtrade -= oldtrade;
    }
    trade_route_list_iterate(would_remove, plost) {
      struct city *losercity = game_city_by_number(plost->partner);

      if (losercity != NULL && city_owner(losercity) == pplayer) {
        trade_routes_iterate(losercity, pback) {
          if (pback->partner == pcity->id) {
            losttrade += pback->value;
          }
        } trade_routes_iterate_end;
      }
    } trade_route_list_iterate_end;
    trade_route_list_destroy(would_remove);
  }

  return newtrade - losttrade;
}

/**********************************************************************//**
  The two flows a caravan arriving at 'dest' from 'src' would start: the
  one-shot establishment/marketplace bonus and the ongoing per-turn
  route income, both undiscounted and both at TODAY's prices.

  The primitives are the game's. The windfall is
  get_caravan_enter_city_trade_bonus() with the same can_establish flag
  the server will use, doubled when the ruleset pays into science AND
  gold -- which is classic's "Both" and is not civ2civ3's "Gold".
**************************************************************************/
static void deep_route_terms(const struct player *pplayer,
                             const struct city *src, const struct city *dest,
                             const struct unit *carrier,
                             struct goods_type *pgood,
                             double *windfall, double *per_turn)
{
  bool can_establish;

  *windfall = 0.0;
  *per_turn = 0.0;

  if (!can_cities_trade(src, dest)) {
    return;
  }
  can_establish = unit_can_do_action(carrier, ACTION_TRADE_ROUTE)
    && can_establish_trade_route(src, dest, pgood->replace_priority);

  *windfall = get_caravan_enter_city_trade_bonus(src, dest,
                                                 unit_type_get(carrier),
                                                 pgood, can_establish);
  if (TBONUS_BOTH == trade_route_settings_by_type
      (cities_trade_route_type(src, dest))->bonus_type) {
    *windfall *= 2;
  }

  if (!can_establish
      || max_trade_routes(src) <= 0 || max_trade_routes(dest) <= 0) {
    /* A marketplace sale and nothing more. */
    return;
  }

  {
    int newtrade = trade_base_between_cities(src, dest);

    *per_turn = deep_route_endpoint_gain(src, pplayer, pgood, newtrade)
      + deep_route_endpoint_gain(dest, pplayer, pgood, newtrade);
  }
}

/**********************************************************************//**
  How long this route is priced to LIVE, in turns after the caravan
  arrives, and whether the answer came from a cease-fire.

  Design call (orchestrator, 2026-08-05): a cease-fire route is priced
  by TRUNCATION, not banned and not bonused. A cease-fire's lapse turn
  is KNOWN STATE -- player_diplstate_get()->turns_left, the same counter
  PEACE's pre-pass watches -- and when it lapses the pair is at war,
  where both rulesets in use set the Enemy trade pct to 0 with
  cancelling "Cancel". The route dies on a date we can read. So it is
  worth the annuity up to that date and nothing after it, which is the
  honest known-mechanics price and needs no constant at all.

  What the tuner may add on top is optimism: trade_ceasefire_peace_odds
  buys back the post-lapse tail at whatever probability it thinks peace
  follows a cease-fire. At its default 0.0 no optimism is expressed and
  no fragile route is farmed.

  Only DS_CEASEFIRE truncates. An armistice already has a date on which
  it becomes PEACE rather than war, and peace keeps the route legal, so
  it is not a death date and is not treated as one.

  Returns the priced life in turns after arrival, and sets *ceasefire
  when a truncation happened -- which is what the census counts.
**************************************************************************/
static int deep_route_life(const struct player *pplayer,
                           const struct city *dest, int eta, int horizon,
                           bool *ceasefire)
{
  const struct player *them = city_owner(dest);
  const struct player_diplstate *ds;
  int full = MAX(horizon - eta, 0);

  *ceasefire = FALSE;
  if (them == pplayer) {
    return full;
  }
  ds = player_diplstate_get(pplayer, them);
  if (ds == NULL || ds->type != DS_CEASEFIRE) {
    return full;
  }

  *ceasefire = TRUE;
  /* turns_left counts from NOW; the caravan spends eta of it walking. */
  return CLIP(0, (int) ds->turns_left - eta, full);
}

/**********************************************************************//**
  Price one candidate destination completely, and record what
  caravan.c's own assembly would have said about the same pair so the
  census can compare the two rather than assert a difference.
**************************************************************************/
static void deep_route_price(const struct player *pplayer,
                             const struct city *src,
                             const struct unit *carrier,
                             struct goods_type *pgood,
                             int horizon, double discount, double survival,
                             struct deep_route_cand *c)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int full = MAX(horizon - c->eta, 0);
  double ongoing;

  deep_route_terms(pplayer, src, c->dest, carrier, pgood,
                   &c->windfall, &c->per_turn);
  c->life = deep_route_life(pplayer, c->dest, c->eta, horizon, &c->ceasefire);

  ongoing = deep_route_annuity(c->per_turn, c->life, discount)
    + tune->trade_ceasefire_peace_odds
      * deep_route_annuity_tail(c->per_turn, c->life, full, discount);

  if (c->ceasefire && c->life <= 0) {
    /* We arrive after the cease-fire has already lapsed: at war the
     * route is illegal and the establishment bonus is not paid either.
     * Only the optimism branch pays anything at all. */
    c->npv = tune->trade_ceasefire_peace_odds
      * deep_route_pv(c->windfall + deep_route_annuity(c->per_turn, full,
                                                       discount),
                      c->eta, discount);
  } else {
    c->npv = deep_route_pv(c->windfall + ongoing, c->eta, discount);
  }
  c->npv *= survival;
  c->survival = survival;

  /* The same pair through caravan.c's assembly, for the census only.
   * Nothing reads it; it is what turns "the annuity is inverted" from
   * a claim into a measurement, per game, per city, per destination. */
  c->caravan_npv
    = deep_route_pv(c->windfall
                    + (c->per_turn / (1.0 - discount))
                      * (1.0 - 1.0 / pow(discount, MAX(full, 1))),
                    c->eta, discount);
}

/**********************************************************************//**
  Every city a caravan of this type, standing here, could reach inside
  the horizon -- with the number of turns it would take.

  This is caravan_search_from()'s traversal, which is static, and it is
  the one piece of caravan.c deep must run itself: the candidate SET has
  to survive the pricing, and caravan.c drops any destination whose
  assembled value loses to the wonder branch. Under the inverted annuity
  that is precisely the set of destinations where a route can be
  established, so harvesting through caravan.c's own callback would
  filter out the population this epic exists to price.

  The pf map is handed back so the caller can ask it for a PATH without
  paying for a second traversal -- which is what makes the transit-risk
  discount affordable.
**************************************************************************/
static int deep_route_candidates(struct player *pplayer, struct city *src,
                                 struct unit *carrier, int horizon,
                                 bool omniscient,
                                 struct deep_route_cand *cand, int max,
                                 struct pf_map **pfm_out, bool *capped)
{
  const struct civ_map *nmap = &(wld.map);
  struct pf_parameter parameter;
  struct pf_map *pfm;
  int n = 0;

  *capped = FALSE;
  pft_fill_unit_parameter(&parameter, nmap, carrier);
  parameter.start_tile = unit_tile(carrier);
  parameter.moves_left_initially = carrier->moves_left;
  parameter.omniscience = omniscient;
  pfm = pf_map_new(&parameter);

  pf_map_positions_iterate(pfm, pos, TRUE) {
    struct city *pcity;

    if (pos.turn > horizon) {
      break;
    }
    pcity = tile_city(pos.tile);
    if (pcity == NULL || pcity == src) {
      continue;
    }
    if (n >= max) {
      *capped = TRUE;
      break;
    }
    memset(&cand[n], 0, sizeof(cand[n]));
    cand[n].dest = pcity;
    cand[n].eta = pos.turn;
    n++;
  } pf_map_positions_iterate_end;

  *pfm_out = pfm;

  return n;
}

static bool deep_route_reach_allows(const struct player *src,
                                    const struct player *dest);

/**********************************************************************//**
  How many legal trade partners sit on ANOTHER continent -- the cities
  a caravan built here would want and cannot walk to.

  A census, and nothing reads it. Design call 3 (cross-water caravans)
  asked for trade caravans to become lift-eligible behind a gate; they
  already are. deep_lift_wants_boat() filters on "not transported, no
  ferry assigned, has a goto tile" and has no unit-class test at all,
  so a caravan holding a cross-continent goto would be picked up by the
  next lift formation exactly like a settler.

  The binding constraint is upstream of that: the destination search is
  a LAND pathfinder over the caravan's own unit class, in deep's
  enumeration and in caravan.c's alike, so an overseas city is never a
  candidate and the goto that would make it lift-eligible is never set.
  (Stock reaches them only by setting ignore_transit_time, i.e. by
  pretending the crossing is free -- the very defect deep removed.)

  So before anything is built, the population is counted: how many
  legal partners are out of reach for want of a boat, and what the
  ruleset would pay for them. civ2civ3 pays INIC 200% against IN 100%,
  which is the 2x premium the design call was aiming at.
**************************************************************************/
static int deep_route_offshore(const struct player *pplayer,
                               const struct city *src, int *ic_premium)
{
  Continent_id home = tile_continent(city_tile(src));
  int n = 0;

  *ic_premium = 0;
  players_iterate_alive(aplayer) {
    city_list_iterate(aplayer->cities, acity) {
      if (tile_continent(city_tile(acity)) == home
          || !map_is_known(city_tile(acity), pplayer)
          || !deep_route_reach_allows(pplayer, aplayer)
          || !can_cities_trade(src, acity)) {
        continue;
      }
      n++;
      *ic_premium
        = MAX(*ic_premium,
              trade_route_type_trade_pct(cities_trade_route_type(src,
                                                                 acity)));
    } city_list_iterate_end;
  } players_iterate_alive_end;

  return n;
}

/**********************************************************************//**
  How much of the walk to 'dest' is dangerous, as a fraction of the
  path's length.

  The machinery is the settler's, not a second opinion about danger:
  deep_settler_overnight_veto() is the same bar deep's civilian
  pathfinder refuses to end a turn on and the same bar its escort
  request uses, asked here at the escort tolerance. A caravan is a
  civilian walking across the same map for the same reason.

  Returns 0.0 when the path is unknown, so an unmeasurable route is
  never penalised for being unmeasurable.
**************************************************************************/
static double deep_route_danger_fraction(struct pf_map *pfm,
                                         struct unit *carrier,
                                         struct tile *dest)
{
  struct pf_path *ppath = pf_map_path(pfm, dest);
  int i, danger = 0, len;
  double f;

  if (ppath == NULL) {
    return 0.0;
  }
  len = ppath->length - 1;
  if (len <= 0) {
    pf_path_destroy(ppath);
    return 0.0;
  }
  for (i = 1; i < ppath->length; i++) {
    if (deep_settler_overnight_veto(carrier, ppath->positions[i].tile,
                                    deep_tuning_get()
                                    ->settler_guard_tolerance)) {
      danger++;
    }
  }
  f = (double) danger / (double) len;
  pf_path_destroy(ppath);

  return f;
}

/**********************************************************************//**
  Compare two priced candidates, best first.
**************************************************************************/
static int deep_route_cand_cmp(const void *a, const void *b)
{
  double da = ((const struct deep_route_cand *) a)->npv;
  double db = ((const struct deep_route_cand *) b)->npv;

  return (da < db) - (da > db);
}

/**********************************************************************//**
  Is this destination inside the reach the tuner set?

  caravan.c's does_foreign_trade_param_allow(), which is static, applied
  to the same enum deep_caravan_reach() already returns -- so the
  candidate set deep prices is the candidate set caravan.c would have
  offered, and the two remain comparable at the seam.
**************************************************************************/
static bool deep_route_reach_allows(const struct player *src,
                                    const struct player *dest)
{
  switch (deep_caravan_reach()) {
  case FTL_NATIONAL_ONLY: return src == dest;
  case FTL_ALLIED:        return pplayers_allied(src, dest);
  case FTL_PEACEFUL:      return pplayers_in_peace(src, dest);
  case FTL_NONWAR:        return !pplayers_at_war(src, dest);
  }

  return FALSE;
}

/* Everything the trade price is made of, so the census line can print
   the inputs beside the answer. A want that cannot be decomposed in the
   log is a want nobody can calibrate (R3). */
struct deep_trade_case {
  const struct city *dest;      /* the destination that set the want */
  int eta;                      /* turns to walk there */
  int build;                    /* turns to build the caravan */
  int committed;                /* caravans this city already has out */
  int slots;                    /* free route slots here */
  int rank;                     /* which best destination we were sold */
  int seen;                     /* candidates enumerated */
  int priced;                   /* ... that carried a positive NPV */
  int truncated;                /* ... truncated by a cease-fire */
  bool capped;                  /* the enumeration hit its cap */
  double npv;                   /* deep's NPV of the chosen candidate */
  double caravan_npv;           /* what caravan.c's assembly says */
  double windfall;
  double per_turn;
  int life;
  double danger;                /* fraction of the path that is dangerous */
  double survival;              /* the factor that came out of it */
  int offshore;                 /* legal partners we cannot walk to */
  int offshore_pct;             /* what the ruleset would pay for them */
};

/**********************************************************************//**
  What a caravan built here would be worth, as a build want.

  The destinations are the real ones a unit of this type could reach
  from this city, and the price is deep's own NPV of each (see the
  section comment above). Three things this replaces, in order of size:

  1. **The inverted annuity.** R2 ran caravan.c's assembly at
     horizon = MORT, the only finite-horizon caller in the tree, and so
     ran the one defective branch in the file. Deep now assembles the
     NPV from the same primitives with the correct closed form.

  2. **The slot, priced as the scarce asset it is.** civ2civ3 and
     classic both give a city a flat number of route slots. R2 answered
     the Nth-caravan question with a boolean -- consider_trade off once
     every free slot was claimed by a caravan in the field -- which on
     classic silently deletes the whole candidate rather than repricing
     it, because `enter_marketplace_blocked_by = "Establish Trade
     Route"` means the windfall it expected to fall back on is not
     legal exactly where a route is. Deep instead sells the Nth caravan
     the Nth-BEST destination: the candidates are priced, sorted, and
     the one at rank `committed` is the marginal one. That IS "value
     minus the best alternative for the same slot", expressed as a
     comparison rather than a coefficient, and it degrades smoothly
     instead of falling off a cliff.

     Replacement pricing needs no separate mechanism: when the slots
     are full, the game's own city_trade_removable() prices the
     incumbent that would be evicted and deep_route_endpoint_gain()
     subtracts it, on both endpoints, with the far side of a broken
     domestic route counted once. A new partner is therefore worth
     building for exactly when it beats the worst incumbent -- and the
     caravan's own cost is charged by amortize() over the build.

  3. **Cease-fire truncation and transit risk**, both inside
     deep_route_price().

  The conversions to a build want are R2's and are unchanged: divide the
  NPV by MORT to turn a stock into the per-turn flow build wants are
  denominated in (MORT is exactly amortize()'s own annuity factor, so no
  constant is minted), price an output point at adv->science_priority
  the way deep_unworked_tile_value() does, and amortize over the turns
  until the caravan is BUILT -- the walk is already discounted inside
  the NPV and must not be charged twice.

  Known blindness, stated rather than hidden: two cities can still each
  price the same destination. The rank test catches the case that
  matters (one city over-committing); two cities racing for one partner
  is not modelled.
**************************************************************************/
static adv_want deep_trade_merit_want(struct ai_type *ait,
                                      struct player *pplayer,
                                      struct city *pcity,
                                      struct unit_type *utype,
                                      struct adv_data *adv,
                                      struct deep_trade_case *tc)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct deep_route_cand cand[DEEP_ROUTE_CAND_MAX];
  struct universal target = { .kind = VUT_UTYPE, .value.utype = utype };
  struct unit *ghost;
  struct pf_map *pfm = NULL;
  struct deep_route_cand *pick;
  double discount = tune->caravan_discount;
  adv_want want;
  int n, i, keep = 0;

  memset(tc, 0, sizeof(*tc));
  tc->build = -1;
  tc->eta = -1;
  tc->rank = -1;
  tc->survival = 1.0;
  tc->committed = deep_committed_caravans(pcity);
  tc->slots = max_trade_routes(pcity) - city_num_trade_routes(pcity);

  if (discount <= 0.0 || discount > 1.0) {
    /* The same legality caravan_parameter_is_legal() enforces: a
     * discount outside (0,1] is not a discount and the tuner range
     * admits the endpoint. */
    return 0;
  }

  ghost = unit_virtual_create(pplayer, pcity, utype, 0);
  ghost->carrying = deep_ghost_goods(pcity, ghost);
  if (ghost->carrying == NULL) {
    /* Nothing this city could load, and a caravan that can carry
     * nothing is worth nothing. (It is also what would take
     * actres.c:927's null dereference, upstream bug 34.) */
    unit_virtual_destroy(ghost);
    return 0;
  }

  tc->offshore = deep_route_offshore(pplayer, pcity, &tc->offshore_pct);

  n = deep_route_candidates(pplayer, pcity, ghost, MORT,
                            !has_handicap(pplayer, H_MAP),
                            cand, DEEP_ROUTE_CAND_MAX, &pfm, &tc->capped);
  tc->seen = n;

  for (i = 0; i < n; i++) {
    double survival = 1.0;

    if (!deep_route_reach_allows(pplayer, city_owner(cand[i].dest))) {
      continue;
    }
    if (tune->trade_route_risk_gain > 0.0) {
      /* Priced off the SAME pf map the enumeration built, so the risk
       * discount costs one path extraction rather than a traversal. */
      double f = deep_route_danger_fraction(pfm, ghost,
                                            city_tile(cand[i].dest));

      survival = CLIP(0.0, 1.0 - tune->trade_route_risk_gain * f, 1.0);
    }
    deep_route_price(pplayer, pcity, ghost, ghost->carrying, MORT,
                     discount, survival, &cand[i]);
    if (cand[i].npv <= 0.0) {
      continue;
    }
    if (cand[i].ceasefire) {
      tc->truncated++;
    }
    cand[keep++] = cand[i];
  }
  pf_map_destroy(pfm);
  unit_virtual_destroy(ghost);

  tc->priced = keep;
  if (keep == 0) {
    return 0;
  }

  qsort(cand, keep, sizeof(cand[0]), deep_route_cand_cmp);

  /* The slot opportunity cost, structurally: this caravan is the
   * (committed+1)-th, so what it can still have is the
   * (committed+1)-th best destination. Past the end of the list every
   * remaining partner is already spoken for and the marginal caravan is
   * worth nothing -- which is the same answer R2's boolean gave, but
   * reached by comparison and only at the point where it is true. */
  tc->rank = MIN(MAX(tc->committed, 0), keep);
  if (tc->rank >= keep) {
    return 0;
  }
  pick = &cand[tc->rank];

  tc->dest = pick->dest;
  tc->eta = pick->eta;
  tc->npv = pick->npv;
  tc->caravan_npv = pick->caravan_npv;
  tc->windfall = pick->windfall;
  tc->per_turn = pick->per_turn;
  tc->life = pick->life;
  tc->survival = pick->survival;
  tc->danger = 1.0 - pick->survival;

  /* The delay is DISCOUNTED, not gated. A city three turns from
   * finishing a caravan and a city thirty turns from one are answering
   * the same question with different patience, which is exactly what
   * amortize() is for; a cutoff at the horizon would be a second,
   * unearned constant, and city_turns_to_build() returns FC_INFINITY
   * for a city with no shield surplus, which amortize() takes to zero
   * on its own. */
  tc->build = city_turns_to_build(pcity, &target, TRUE);

  want = amortize((adv_want) (pick->npv * adv->science_priority / MORT),
                  MAX(tc->build, 0))
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
  struct deep_trade_case tc;
  struct unit_type *utype, *stock_utype = NULL;
  adv_want stock_bid, merit;
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
  merit = deep_trade_merit_want(ait, pplayer, pcity, utype, adv, &tc);

  /* The winner's want is compared against the bid AS THE ADVISOR LEFT
   * IT -- clamped to DAI_WANT_DOMESTIC_MAX (daidomestic.c:634).
   * Comparing against the raw bid declines to act on exactly the
   * cities that bid hardest; R2b measured that gap on the wonder
   * producer at 6 of 84 eligible seam lines and fixed it there, and
   * left it here on purpose because trade_merit was shipping at 0.
   * This epic reprices the trade path, so it is fixed here too. */
  if (stock_bid > 0 && stock_utype != NULL
      && choice->type == CT_CIVILIAN
      && choice->value.utype == stock_utype
      && choice->want == MIN(stock_bid, DAI_WANT_DOMESTIC_MAX)) {
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
           " stock=%.1f merit=%.1f npv=%.1f cnpv=%.1f eta=%d build=%d"
           " dest=%d committed=%d slots=%d rank=%d seen=%d priced=%d"
           " trunc=%d wind=%.1f flow=%.2f life=%d risk=%.2f capped=%d"
           " offshore=%d offpct=%d recover=%s",
           game.info.turn, player_name(pplayer), pcity->id, mode,
           (double) stock_bid, (double) merit, tc.npv, tc.caravan_npv,
           tc.eta, tc.build, tc.dest != NULL ? tc.dest->id : -1,
           tc.committed, tc.slots, tc.rank, tc.seen, tc.priced,
           tc.truncated, tc.windfall, tc.per_turn, tc.life, tc.danger,
           tc.capped ? 1 : 0, tc.offshore, tc.offshore_pct,
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

  /* RACE TO WIN (WRACE step 2). A finish line is where a delivered
   * caravan pays most: the shields it carries decide the race rather
   * than merely advancing a build nobody contests. Symmetric to the
   * go/no-go, and deliberately one-sided -- it only lifts a race we
   * are AHEAD in, because feeding a race we are losing is the
   * go/no-go's business and it has already priced that want down.
   * Exactly 1.0 at wrace_urgency_gain = 0, off a race, or behind. */
  {
    double urg = deep_wonder_race_urgency(wonder_city, wonder);

    if (urg != 1.0) {
      log_base(deep_metric_log_level(),
               "deep-wrace-feed turn=%d player=\"%s\" city=%d"
               " wonder=\"%s\" urgency=%.3f want=%.1f",
               game.info.turn, player_name(pplayer), pcity->id,
               improvement_rule_name(wonder), urg, (double) want);
      want = (adv_want) (want * urg);
    }
  }

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

  /* The winner's want is compared against the bid AS THE ADVISOR LEFT
   * IT, which is the bid clamped to DAI_WANT_DOMESTIC_MAX
   * (daidomestic.c:634). Comparing against the raw bid loses exactly
   * the cities where the wonder producer bid hardest: measured on one
   * probe game of primary-120-classic, 6 of 84 eligible seam lines
   * carry a stock bid above the clamp, all of them recognised as
   * "keep" -- the repricing silently declining to act on the top 7% of
   * its own population. The raw bid is still what gets LOGGED, because
   * that is the number R3's corpus needs. */
  if (stock_bid > 0 && stock_utype != NULL
      && choice->type == CT_CIVILIAN
      && choice->value.utype == stock_utype
      && choice->want == MIN(stock_bid, DAI_WANT_DOMESTIC_MAX)) {
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
