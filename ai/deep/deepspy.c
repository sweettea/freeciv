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
#include "log.h"

/* common */
#include "actions.h"
#include "city.h"
#include "game.h"
#include "improvement.h"
#include "map.h"
#include "player.h"
#include "research.h"
#include "tech.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* server */
#include "cityturn.h"
#include "diplomats.h"
#include "unithand.h"
#include "unittools.h"

/* ai/default */
#include "daiactions.h"
#include "daihand.h"

#include "deepai.h"
#include "deepops.h"
#include "deeptuning.h"

#include "deepspy.h"

/* The families deep has a price for. Everything else -- steal gold,
 * steal maps, poison, plague, nuke -- is DSK_NONE, worth zero, and
 * deep will not choose it: see deep_spy_city_choice() for why that is
 * an abstention rather than a veto. */
enum deep_spy_kind {
  DSK_NONE = 0,
  DSK_EMBASSY,
  DSK_STEAL,
  DSK_INCITE,
  DSK_SABOTAGE,
  DSK_COUNT
};

static const char *const deep_spy_kind_name[DSK_COUNT] = {
  "none", "embassy", "steal", "incite", "sabotage"
};

/* The execution census, per player, per process run. NOT saved, unlike
 * the stance counters: this one is read from the LOG of a run rather
 * than from a save, a benchmark game is one process, and the
 * alternative is six more fields through deep_plr's save and load for
 * a number no decision reads. A reload restarts the count and the log
 * line says so by starting again from zero. */
struct deep_spy_census {
  int seen;             /* Diplomat-turns considered */
  int at_seam;          /* ... standing where an action is possible */
  int acted[DSK_COUNT]; /* ... where deep acted, by family */
  int abstained;        /* ... at the seam with no positive deep price */
  int refused;          /* ... where the server did not run the action */
};

static struct deep_spy_census census[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  One gold in the want currency. Money is the only quantity three of
  the four prices below are natively denominated in (a tech's bulb cost
  goes through stock's own x3 gold conversion, the incite cost is gold
  the server charges, a sabotage is the only one already in shields),
  so this is the single exchange rate the layer needs rather than one
  gain per action.

  PLACEHOLDER, and the number that most wants a tuning pass: it decides
  whether a spy is a cheap way to buy a tech or an expensive one, and
  every family except sabotage moves with it.
**************************************************************************/
static double deep_spy_gold_worth(double gold)
{
  return gold * deep_tuning_get()->spy_gold_value;
}

/**********************************************************************//**
  What a stolen tech is worth: its BULB cost at stock's own conversion
  (dai_goldequiv_tech(), daidiplomacy.c:171, bulbs x 3 = gold), then
  gold into the want currency.

  Stock's x3 is reused rather than replaced because it is the rate the
  rest of this AI already trades techs at, and a second, different
  bulb price in a neighbouring module is exactly the incommensurable-
  scale defect (RC4) this epic exists to remove.

  Which tech: the TARGETED variants name one, and it is priced exactly.
  The untargeted variants take a tech we cannot name in advance -- the
  server picks -- so they are priced at the step we are currently
  paying for ourselves, research_goal_bulbs_required() toward our own
  research target. That is an honest lower bound on a random unknown
  advance from a rival that has techs we do not, and it needs no
  constant: it is what OUR next tech costs.
**************************************************************************/
static double deep_spy_tech_worth(const struct player *pplayer,
                                  Tech_type_id tech)
{
  const struct research *presearch = research_get(pplayer);
  int bulbs;

  if (presearch == NULL) {
    return 0.0;
  }
  if (tech == A_UNSET || tech == A_NONE
      || !valid_advance_by_number(tech)
      || research_invention_state(presearch, tech) == TECH_KNOWN) {
    /* Untargeted, or a target we cannot price: our own next step. */
    tech = presearch->researching;
    if (!valid_advance_by_number(tech)) {
      return 0.0;
    }
  }

  bulbs = research_goal_bulbs_required(presearch, tech);

  return deep_spy_gold_worth(3.0 * (double)bulbs);
}

/**********************************************************************//**
  What deep would pay for one action against one city, in the want
  currency, NET of the actor: an action that consumes the unit costs
  the shields the unit is made of, which is why the escaping variant of
  an action outranks the consuming one at equal gross value without a
  rule saying so.

  The four families and their sources:

    embassy   embassy_science_value, the want deep ALREADY pays per
              missing embassy in its diplomat build want. The build
              side and the action side now agree by construction.
    steal     the tech's bulb cost through stock's x3 (above).
    incite    deep_conquest_worth() -- the same city valuation the
              assault want, the operation net-value gate and the
              defence counter-claim all read -- MINUS the incite cost
              the server computes. Incite and assault are then two
              prices for the same object, which is what W35 asks for.
    sabotage  the shields destroyed: the target's shield stock for a
              production sabotage, the improvement's build cost for a
              targeted one, times a denial gain because shields taken
              off a rival are not worth the same as shields added to
              us.

  Zero means DEEP HAS NO PRICE, not "worthless".
**************************************************************************/
static double deep_spy_action_value(struct ai_type *ait,
                                    const struct action *paction,
                                    struct unit *punit,
                                    struct city *target,
                                    int sub_tgt_id,
                                    enum deep_spy_kind *kind)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = unit_owner(punit);
  struct player *tplayer = city_owner(target);
  double value = 0.0;

  *kind = DSK_NONE;

  if (action_has_result(paction, ACTRES_ESTABLISH_EMBASSY)) {
    if (!player_has_embassy(pplayer, tplayer)) {
      *kind = DSK_EMBASSY;
      value = tune->embassy_science_value;
    }
  } else if (action_has_result(paction, ACTRES_SPY_STEAL_TECH)
             || action_has_result(paction,
                                  ACTRES_SPY_TARGETED_STEAL_TECH)) {
    /* Stock's guard, kept because it is a RULE and not a price: a
     * city that has already been robbed once resists a diplomat that
     * the theft would consume (civ2civ3 "may only be stolen once from
     * a given enemy city by Diplomats"). Without it deep re-priced the
     * same impossible theft every turn and stalled the unit -- 107
     * attempts in ten smoke games, all refused. */
    bool consumed = utype_is_consumed_by_action(paction,
                                                unit_type_get(punit));

    if (!pplayers_allied(pplayer, tplayer)
        && (!consumed
            || diplomats_unignored_tech_stealings(punit, target) == 0)) {
      *kind = DSK_STEAL;
      value = deep_spy_tech_worth(pplayer,
                                  action_has_result(
                                      paction,
                                      ACTRES_SPY_TARGETED_STEAL_TECH)
                                  ? (Tech_type_id)sub_tgt_id : A_UNSET);
    }
  } else if (action_has_result(paction, ACTRES_SPY_INCITE_CITY)) {
    int cost = city_incite_cost(pplayer, target);
    int expenses;

    dai_calc_data(pplayer, NULL, &expenses, NULL);
    if (!pplayers_allied(pplayer, tplayer)
        && cost < INCITE_IMPOSSIBLE_COST
        && cost <= pplayer->economic.gold - 2 * expenses) {
      *kind = DSK_INCITE;
      value = deep_conquest_worth(ait, pplayer, target)
        - deep_spy_gold_worth((double)cost);
    }
  } else if (action_has_result(paction, ACTRES_SPY_SABOTAGE_CITY)
             || action_has_result(paction,
                                  ACTRES_SPY_SABOTAGE_CITY_PRODUCTION)) {
    if (pplayers_at_war(pplayer, tplayer)) {
      *kind = DSK_SABOTAGE;
      value = tune->spy_sabotage_gain * (double)target->shield_stock;
    }
  } else if (action_has_result(paction,
                               ACTRES_SPY_TARGETED_SABOTAGE_CITY)) {
    const struct impr_type *pimprove = improvement_by_number(sub_tgt_id);

    if (pplayers_at_war(pplayer, tplayer) && pimprove != NULL
        && city_has_building(target, pimprove)) {
      *kind = DSK_SABOTAGE;
      value = tune->spy_sabotage_gain
        * (double)impr_build_shield_cost(target, pimprove);
    }
  }

  if (*kind == DSK_NONE || value <= 0.0) {
    *kind = DSK_NONE;

    return 0.0;
  }

  /* The actor, when the action eats it. Already shields, already this
   * currency: no conversion and no constant. */
  if (utype_is_consumed_by_action(paction, unit_type_get(punit))) {
    value -= (double)unit_build_shield_cost_base(punit);
  }

  return value;
}

/**********************************************************************//**
  The best deep-priced action this unit could take against this city,
  or DSK_NONE if deep has no positive price for anything legal here.

  Only legal actions are considered, by the same test stock uses
  (action_prob_possible on action_prob_vs_city), and sub targets are
  chosen by stock's own dai_action_choose_sub_tgt_unit_vs_city() --
  reuse, not a second chooser: W35's targeted-pick improvement is a
  later slice, and mixing it in here would make this epic's measurement
  two changes wide.
**************************************************************************/
static double deep_spy_city_choice(struct ai_type *ait, struct unit *punit,
                                   struct city *target,
                                   struct action **chosen,
                                   int *chosen_sub_tgt,
                                   enum deep_spy_kind *chosen_kind,
                                   int *legal)
{
  const struct civ_map *nmap = &(wld.map);
  double best = 0.0;

  *chosen = NULL;
  *chosen_sub_tgt = 0;
  *chosen_kind = DSK_NONE;

  action_iterate(act_id) {
    struct action *paction = action_by_number(act_id);
    enum deep_spy_kind kind;
    double value;
    int sub_tgt_id;

    if (action_get_actor_kind(paction) != AAK_UNIT
        || action_get_target_kind(paction) != ATK_CITY) {
      continue;
    }
    /* BOTH tests: action_prob_possible() is stock's, and it answers
     * "might be possible"; is_action_enabled_unit_on_city() is the one
     * the server's own handler applies before it runs anything. Deep
     * commits a unit to the action it chooses, so it asks the
     * stricter question. */
    if (!action_prob_possible(action_prob_vs_city(nmap, punit,
                                                  action_number(paction),
                                                  target))
        || !is_action_enabled_unit_on_city(nmap, action_number(paction),
                                           punit, target)) {
      continue;
    }
    (*legal)++;

    if (paction->target_complexity == ACT_TGT_COMPL_SIMPLE) {
      sub_tgt_id = 0;
    } else {
      sub_tgt_id = dai_action_choose_sub_tgt_unit_vs_city(paction, punit,
                                                          target);
    }

    value = deep_spy_action_value(ait, paction, punit, target, sub_tgt_id,
                                  &kind);
    if (kind != DSK_NONE && value > best) {
      best = value;
      *chosen = paction;
      *chosen_sub_tgt = sub_tgt_id;
      *chosen_kind = kind;
    }
  } action_iterate_end;

  return best;
}

/**********************************************************************//**
  One line per player per turn: the census, so "deep prices spy
  actions" is a claim with an execution count behind it rather than a
  hope. Four changes in this project have measured as no effect while
  never executing.

  at_seam against seen is the coverage number this design has to be
  judged on: deep takes the action choice for a diplomat that stands
  where an action is legal at the START of the phase, and leaves every
  other diplomat-turn -- approaching, bribing, defending, boat-bound --
  to stock. If at_seam is a small share of seen, the seam is mostly
  reached from inside stock's own goto and this slice cannot price it
  without owning the movement too. That is a finding to report, not a
  number to hide.
**************************************************************************/
static void deep_spy_log(const struct player *pplayer)
{
  const struct deep_spy_census *c = &census[player_index(pplayer)];

  if (c->seen == 0) {
    return;
  }

  log_base(deep_metric_log_level(),
           "deep-spy turn=%d player=\"%s\" on=%d seen=%d at_seam=%d"
           " abstained=%d refused=%d embassy=%d steal=%d incite=%d"
           " sabotage=%d",
           game.info.turn, player_name(pplayer),
           deep_tuning_get()->spy_enabled, c->seen, c->at_seam,
           c->abstained, c->refused, c->acted[DSK_EMBASSY],
           c->acted[DSK_STEAL],
           c->acted[DSK_INCITE], c->acted[DSK_SABOTAGE]);
}

/**********************************************************************//**
  Take the ACTION CHOICE for every diplomat that already stands beside
  a city it could act against, and leave everything else about the
  diplomat class to stock.

  Where this runs and why: before deep_do_first_activities(), from
  deepai.c, which is the last moment at which a unit is untouched.
  Stock's dai_manage_diplomat() bribes, defends, picks a target, walks
  toward it and -- if it arrives with movement to spare -- acts, all in
  one call, so the only way to intercept the ACTION without also owning
  the walk is to reach the unit before that call. A unit deep acts with
  is then HELD (deep_hold_unit), which deep's unit loop honours, so
  stock never re-tasks it; a unit deep does not act with is left
  entirely alone, keeping stock's bribe pass, its defensive stay and
  its approach exactly as they are.

  The deliberate omissions, so the next slice knows what it inherits:
  - deep does not choose the TARGET, so a diplomat still walks where
    stock sends it. Incite-vs-assault as competing plans for one city
    (W35's design) needs the target choice and is not this epic.
  - a diplomat standing in one of OUR cities is skipped outright: that
    is stock's garrison-diplomat duty, it reads city danger deep has
    not refreshed yet at this point in the phase, and taking it over
    from a stale read would be worse than not taking it over.
**************************************************************************/
void deep_spy_manage(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct deep_spy_census *c = &census[player_index(pplayer)];
  bool on = (tune->spy_enabled > 0);

  unit_list_iterate_safe(pplayer->units, punit) {
    struct action *chosen = NULL;
    struct city *best_city = NULL;
    enum deep_spy_kind kind = DSK_NONE;
    enum deep_spy_kind best_kind = DSK_NONE;
    struct city *home;
    struct unit *acted_unit;
    int best_sub = 0;
    int legal = 0;
    double best = 0.0;
    int id, moves0;

    if (!utype_has_flag(unit_type_get(punit), UTYF_DIPLOMAT)
        || punit->moves_left <= 0) {
      continue;
    }
    c->seen++;

    home = tile_city(unit_tile(punit));
    if (home != NULL && city_owner(home) == pplayer) {
      continue;                 /* Garrison duty stays stock's. */
    }

    /* Every city we could act against from where we stand. Legality,
     * including the range the action allows, is action_prob_vs_city's
     * answer rather than ours. */
    square_iterate(&(wld.map), unit_tile(punit), 1, ptile) {
      struct city *acity = tile_city(ptile);
      struct action *act;
      double value;
      int sub;

      if (acity == NULL || city_owner(acity) == pplayer
          || is_barbarian(city_owner(acity))) {
        continue;
      }
      value = deep_spy_city_choice(ait, punit, acity, &act, &sub, &kind,
                                   &legal);
      if (act != NULL && value > best) {
        best = value;
        best_city = acity;
        chosen = act;
        best_sub = sub;
        best_kind = kind;
      }
    } square_iterate_end;

    if (legal == 0) {
      continue;                 /* Not at the seam: stock's, untouched. */
    }
    c->at_seam++;
    if (chosen == NULL) {
      /* Legal actions exist and deep priced none of them above zero --
       * a steal-gold-only opportunity, an embassy we already hold, an
       * incite we cannot afford. Stock's ladder answers, which is an
       * ABSTENTION: deep declining to bid is not deep vetoing. */
      c->abstained++;
      continue;
    }
    if (!on) {
      continue;                 /* Census only: the game is unchanged. */
    }

    log_base(deep_metric_log_level(),
             "deep-spy-act turn=%d player=\"%s\" unit=%d action=%s"
             " kind=%s target=\"%s\" want=%.1f",
             game.info.turn, player_name(pplayer), punit->id,
             action_rule_name(chosen), deep_spy_kind_name[best_kind],
             city_name_get(best_city), best);

    id = punit->id;
    moves0 = punit->moves_left;
    unit_activity_handling(punit, ACTIVITY_IDLE, ACTION_NONE);
    unit_do_action(pplayer, id, best_city->id, best_sub, "",
                   action_number(chosen));

    /* Did the server RUN it. An action it refuses is silent -- the
     * request is answered with an illegal-action packet and the unit
     * stands exactly where it was -- so a counter that trusted the
     * request would report a mechanism that never fired, which is this
     * project's most expensive recurring mistake. Every diplomat
     * action that happens charges movement or eats the unit. A refused
     * one is NOT held: stock gets its normal turn with the unit. */
    acted_unit = game_unit_by_number(id);
    if (acted_unit == NULL || acted_unit->moves_left != moves0) {
      c->acted[best_kind]++;
      if (acted_unit != NULL) {
        deep_hold_unit(ait, acted_unit, "spy");
      }
    } else {
      c->refused++;
      continue;
    }
  } unit_list_iterate_safe_end;

  deep_spy_log(pplayer);
}
