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

#include <math.h>

/* utility */
#include "log.h"
#include "rand.h"

/* common */
#include "city.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* server */
#include "citytools.h"
#include "cityturn.h"
#include "sernet.h"
#include "srv_log.h"

/* server/advisors */
#include "advbuilding.h"
#include "advchoice.h"
#include "infracache.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daidiplomacy.h"
#include "daihand.h"
#include "daimilitary.h"
#include "daiplayer.h"
#include "daisettler.h"
#include "daitech.h"
#include "daiunit.h"

#include "deepai.h"
#include "deeptuning.h"

#include "deepdefence.h"

/* Defence bookkeeping (W11). Stock cities only count defenders that are
 * physically on the city tile (assess_defense_backend), so a city whose
 * help is two turns out keeps requisitioning at emergency priority; and
 * simultaneous city emergencies are resolved in city list order with no
 * arbitration. The deep turn drivers below are derived from
 * dai_do_first_activities() / dai_do_last_activities() /
 * dai_manage_cities() so those two behaviours can change; everything
 * else is the stock call sequence, reused. */

/**********************************************************************//**
  Log one bookkeeping line per city: the raw assessed danger, the
  on-tile defence, and what the city is building
  (benchmark/defence.py). Runs before any deep adjustment so before and
  after arms measure need on the same scale.
**************************************************************************/
static void deep_log_defence(struct ai_type *ait, struct player *pplayer)
{
  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    int ndef = 0;

    unit_list_iterate(city_tile(pcity)->units, punit) {
      if (unit_owner(punit) == pplayer
          && !unit_has_type_flag(punit, UTYF_CIVILIAN)) {
        ndef++;
      }
    } unit_list_iterate_end;

    log_base(LOG_NORMAL,
             "deep-def turn=%d player=\"%s\" city=%d danger=%u"
             " urgency=%u defense=%d ndef=%d prod=\"%s\"",
             game.info.turn, player_name(pplayer), pcity->id,
             city_data->danger, city_data->urgency,
             assess_defense(ait, pcity), ndef,
             universal_rule_name(&pcity->production));
  } city_list_iterate_end;
}

/**********************************************************************//**
  Credit defenders that are on their way home against their target
  city's freshly assessed danger, so the city stops requisitioning
  replacements for help that is already coming. This is the missing
  mirror of the invasion bookkeeping the AI keeps about enemy cities
  (reinforcements_cost_and_value() filling ai_invasion).

  A unit counts if its task is AIUNIT_DEFEND_HOME and its goto points
  at an own city it has not reached yet; its contribution is its
  defence power in that city discounted by expected arrival time and
  the chance of surviving the trip.
**************************************************************************/
static void deep_credit_incoming_defence(struct ai_type *ait,
                                         struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();

  unit_list_iterate(pplayer->units, punit) {
    struct unit_ai *unit_data = def_ai_unit_data(punit, ait);
    struct city *pcity;
    struct ai_city *city_data;
    int eta, power;
    unsigned int credit;

    if (unit_data->task != AIUNIT_DEFEND_HOME
        || punit->goto_tile == NULL
        || same_pos(punit->goto_tile, unit_tile(punit))) {
      /* Units already in place are counted by assess_defense(). */
      continue;
    }
    pcity = tile_city(punit->goto_tile);
    if (pcity == NULL || city_owner(pcity) != pplayer) {
      continue;
    }

    power = assess_defense_unit(ait, pcity, punit, FALSE);
    if (power <= 0) {
      continue;
    }
    eta = real_map_distance(unit_tile(punit), punit->goto_tile)
          * SINGLE_MOVE / MAX(unit_type_get(punit)->move_rate, 1);
    credit = power
             * pow(tune->incoming_defence_survival, eta)
             / (1.0 + tune->incoming_defence_eta_weight * eta);

    city_data = def_ai_city_data(pcity, ait);
    city_data->danger -= MIN(city_data->danger, credit);
    UNIT_LOG(LOG_DEBUG, punit,
             "credits %u defence to %s (eta %d)",
             credit, city_name_get(pcity), eta);
  } unit_list_iterate_end;
}

/**********************************************************************//**
  Activities before unit movement; derived from
  dai_do_first_activities() with defence bookkeeping added between the
  danger assessment and the unit management that consumes it.
**************************************************************************/
void deep_do_first_activities(struct ai_type *ait, struct player *pplayer)
{
  TIMING_LOG(AIT_ALL, TIMER_START);
  dai_assess_danger_player(ait, &(wld.map), pplayer);

  deep_log_defence(ait, pplayer);
  deep_credit_incoming_defence(ait, pplayer);

  TIMING_LOG(AIT_UNITS, TIMER_START);
  dai_manage_units(ait, pplayer);
  TIMING_LOG(AIT_UNITS, TIMER_STOP);
  /* STOP.  Everything else is at end of turn. */

  TIMING_LOG(AIT_ALL, TIMER_STOP);

  flush_packets(); /* AIs can be such spammers... */
}

struct deep_emergency {
  struct city *pcity;
  unsigned int grave_danger;
  unsigned int urgency;
  int capital;
  adv_want worth;
  int id;
};

/**********************************************************************//**
  Severity order for simultaneous city emergencies: military gravity
  first, then how close the danger is, then strategic role, then plain
  city value; city id keeps the sort deterministic. Lexicographic on
  purpose -- no scale-mixing weights to invent.
**************************************************************************/
static int deep_emergency_cmp(const void *a_, const void *b_)
{
  const struct deep_emergency *a = a_, *b = b_;

  if (a->grave_danger != b->grave_danger) {
    return a->grave_danger > b->grave_danger ? -1 : 1;
  }
  if (a->urgency != b->urgency) {
    return a->urgency > b->urgency ? -1 : 1;
  }
  if (a->capital != b->capital) {
    return b->capital - a->capital;
  }
  if (a->worth != b->worth) {
    return a->worth > b->worth ? -1 : 1;
  }
  return a->id - b->id;
}

/**********************************************************************//**
  City management. Derived from dai_manage_cities(); the difference is
  that city emergencies are collected first, ranked by severity, and
  resolved in that order, instead of in city list order where the first
  city in the list strips worked tiles and gold from more important
  ones behind it (README.AI: 'Emergencies in two cities at once aren't
  handled properly').
**************************************************************************/
static void deep_manage_cities(struct ai_type *ait, struct player *pplayer)
{
  int ecount = 0, i;
  struct deep_emergency emergencies[city_list_size(pplayer->cities) + 1];

  pplayer->ai_common.maxbuycost = 0;

  TIMING_LOG(AIT_EMERGENCY, TIMER_START);
  city_list_iterate(pplayer->cities, pcity) {
    if (CITY_EMERGENCY(pcity)
        || city_granary_size(city_size_get(pcity)) == pcity->food_stock) {
      /* Having a full granary isn't an emergency, but we want to rearrange */
      auto_arrange_workers(pcity); /* This usually helps */
    }
    if (CITY_EMERGENCY(pcity)) {
      struct ai_city *city_data = def_ai_city_data(pcity, ait);

      emergencies[ecount++] = (struct deep_emergency) {
        .pcity = pcity,
        .grave_danger = city_data->grave_danger,
        .urgency = city_data->urgency,
        .capital = is_capital(pcity) ? 1 : 0,
        .worth = city_data->worth,
        .id = pcity->id,
      };
    }
  } city_list_iterate_end;

  qsort(emergencies, ecount, sizeof(emergencies[0]), deep_emergency_cmp);

  for (i = 0; i < ecount; i++) {
    log_base(LOG_NORMAL,
             "deep-emergency turn=%d player=\"%s\" rank=%d city=%d"
             " grave=%u urgency=%u capital=%d worth=%d",
             game.info.turn, player_name(pplayer), i,
             emergencies[i].id, emergencies[i].grave_danger,
             emergencies[i].urgency, emergencies[i].capital,
             (int) emergencies[i].worth);
    resolve_city_emergency(ait, pplayer, emergencies[i].pcity);
  }

  city_list_iterate(pplayer->cities, pcity) {
    dai_city_sell_noncritical(pcity, TRUE);
    sync_cities();
  } city_list_iterate_end;
  if (pplayer->economic.tax >= 30 /* Otherwise expect it to increase tax */
      && player_get_expected_income(pplayer) < -(pplayer->economic.gold)) {
    int count = city_list_size(pplayer->cities);
    struct city *sellers[count + 1];

    /* Randomized order */
    count = 0;
    city_list_iterate(pplayer->cities, pcity) {
      sellers[count++] = pcity;
    } city_list_iterate_end;

    for (i = 0; i < count; i++) {
      int replace = fc_rand(count);
      struct city *tmp;

      tmp = sellers[i];
      sellers[i] = sellers[replace];
      sellers[replace] = tmp;
    }

    i = 0;
    while (player_get_expected_income(pplayer) < -(pplayer->economic.gold)
           && i < count) {
      dai_city_sell_noncritical(sellers[i++], FALSE);
    }
  }
  TIMING_LOG(AIT_EMERGENCY, TIMER_STOP);

  TIMING_LOG(AIT_BUILDINGS, TIMER_START);
  building_advisor(pplayer);
  TIMING_LOG(AIT_BUILDINGS, TIMER_STOP);

  /* Initialize the infrastructure cache, which is used shortly. */
  initialize_infrastructure_cache(pplayer);
  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    struct adv_choice *choice;

    if (city_data->choice.want <= 0) {
      /* Note that this function mungs the seamap, but we don't care */
      TIMING_LOG(AIT_CITY_MILITARY, TIMER_START);
      choice = military_advisor_choose_build(ait, &(wld.map), pplayer, pcity,
                                             NULL);
      adv_choice_copy(&(city_data->choice), choice);
      adv_free_choice(choice);
      TIMING_LOG(AIT_CITY_MILITARY, TIMER_STOP);
    }
    if (dai_on_war_footing(ait, pplayer) && city_data->choice.want > 0) {
      city_data->worker_want = 0;
      city_data->founder_want = 0;
      city_data->founder_turn = game.info.turn; /* Do not consider zero we set
                                                 * here a valid value,
                                                 * if real want is needed.
                                                 * Recalculate immediately
                                                 * in such situation. */
      continue; /* Go, soldiers! */
    }
    /* Will record its findings in pcity->worker_want */
    TIMING_LOG(AIT_CITY_TERRAIN, TIMER_START);
    contemplate_terrain_improvements(ait, pcity);
    TIMING_LOG(AIT_CITY_TERRAIN, TIMER_STOP);

    TIMING_LOG(AIT_CITY_SETTLERS, TIMER_START);
    if (city_data->founder_turn <= game.info.turn) {
      /* Will record its findings in pcity->founder_want */
      contemplate_new_city(ait, pcity);
      /* Avoid recalculating all the time.. */
      city_data->founder_turn
        = game.info.turn + fc_rand(AI_CITY_RECALC_SPEED)
          + AI_CITY_RECALC_SPEED;
    } else if (pcity->server.debug) {
      /* Recalculate every turn */
      contemplate_new_city(ait, pcity);
    }
    TIMING_LOG(AIT_CITY_SETTLERS, TIMER_STOP);
    ADV_CHOICE_ASSERT(city_data->choice);
  } city_list_iterate_end;
  /* Reset auto settler state for the next run. */
  dai_auto_settler_reset(ait, pplayer);

  city_list_iterate(pplayer->cities, pcity) {
    dai_city_choose_build(ait, pplayer, pcity);

    /* Initialize for next turn */
    def_ai_city_data(pcity, ait)->choice.want = -1;
  } city_list_iterate_end;

  dai_spend_gold(ait, pplayer);
}

/**********************************************************************//**
  Activities after unit movement; derived from
  dai_do_last_activities() with deep's ranked emergency handling in
  place of the stock city management.
**************************************************************************/
void deep_do_last_activities(struct ai_type *ait, struct player *pplayer)
{
  TIMING_LOG(AIT_ALL, TIMER_START);
  dai_clear_tech_wants(ait, pplayer);

  dai_manage_government(ait, pplayer);
  dai_adjust_policies(ait, pplayer);
  TIMING_LOG(AIT_TAXES, TIMER_START);
  dai_manage_taxes(ait, pplayer);
  TIMING_LOG(AIT_TAXES, TIMER_STOP);
  TIMING_LOG(AIT_CITIES, TIMER_START);
  deep_manage_cities(ait, pplayer);
  TIMING_LOG(AIT_CITIES, TIMER_STOP);
  TIMING_LOG(AIT_TECH, TIMER_START);
  dai_manage_tech(ait, pplayer);
  TIMING_LOG(AIT_TECH, TIMER_STOP);
  dai_manage_spaceship(pplayer);

  TIMING_LOG(AIT_ALL, TIMER_STOP);
}
