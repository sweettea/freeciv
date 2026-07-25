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
#include "sernet.h"
#include "srv_log.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daihand.h"
#include "daimilitary.h"
#include "daiplayer.h"
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

/**********************************************************************//**
  Activities after unit movement; derived from
  dai_do_last_activities(). The city management call becomes deep's own
  in a later commit; until then this is the stock sequence.
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
  dai_manage_cities(ait, pplayer);
  TIMING_LOG(AIT_CITIES, TIMER_STOP);
  TIMING_LOG(AIT_TECH, TIMER_START);
  dai_manage_tech(ait, pplayer);
  TIMING_LOG(AIT_TECH, TIMER_STOP);
  dai_manage_spaceship(pplayer);

  TIMING_LOG(AIT_ALL, TIMER_STOP);
}
