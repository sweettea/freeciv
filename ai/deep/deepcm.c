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
#include "mem.h"

/* common */
#include "city.h"
#include "fc_types.h"
#include "game.h"
#include "improvement.h"
#include "player.h"

/* common/aicore */
#include "cm.h"

/* server */
#include "cityturn.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daiplayer.h"

#include "deeptuning.h"

#include "deepcm.h"

/* City manager parameters from city state (W09-lite). Stock AI cities
 * all run the hardcoded factors in set_default_city_manager()
 * (cityturn.c) -- the comment there says priorities should someday
 * flow in via a separate set of variables, and pcity->cm_parameter is
 * exactly that hook: auto_arrange_workers() honours it when set, and
 * nothing in the stock AI ever sets it. Cities that match no rule get
 * their cm_parameter cleared, so the change is incremental and
 * reversible per city. */

/**********************************************************************//**
  Fill the stock default booleans and minimal surpluses; only the
  factors differ between deep's rules (keeping the safety rails while
  the factors go through W25 tuning).
**************************************************************************/
static void deep_cm_base(struct cm_parameter *cmp, struct city *pcity)
{
  int csize = city_size_get(pcity);
  int gsize = city_granary_size(csize);

  cm_init_parameter(cmp);
  cmp->require_happy = FALSE;
  cmp->allow_disorder = FALSE;
  cmp->allow_specialists = TRUE;

  if (gsize == pcity->food_stock) {
    cmp->minimal_surplus[O_FOOD] = 0;
  } else {
    cmp->minimal_surplus[O_FOOD] = 1;
  }
  cmp->minimal_surplus[O_SHIELD] = 1;
  cmp->minimal_surplus[O_TRADE] = 0;
  cmp->minimal_surplus[O_GOLD] = -FC_INFINITY;
  cmp->minimal_surplus[O_LUXURY] = 0;
  cmp->minimal_surplus[O_SCIENCE] = 0;

  /* Stock values for the outputs deep's rules leave alone. */
  cmp->factor[O_GOLD] = 2;
  cmp->factor[O_LUXURY] = 0;
  cmp->factor[O_SCIENCE] = 2;
}

/**********************************************************************//**
  Derive and set (or clear) cm_parameter for every owned city.
**************************************************************************/
void deep_set_city_manager_params(struct ai_type *ait,
                                  struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();

  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    struct cm_parameter cmp;
    bool matched = TRUE;

    deep_cm_base(&cmp, pcity);

    if (city_data->urgency > 0 || city_data->grave_danger > 0) {
      /* Under threat: shields now; the happy factor stops the
       * arrangement fighting the (stock-ignored) happiness signal. */
      cmp.factor[O_FOOD] = tune->cm_war_food;
      cmp.factor[O_SHIELD] = tune->cm_war_shield;
      cmp.factor[O_TRADE] = tune->cm_war_trade;
      cmp.happy_factor = tune->cm_happy_factor;
    } else if (!city_can_grow_to(pcity, city_size_get(pcity) + 1)) {
      /* Growth-capped: food is nearly worthless, work the mines and
       * trade specials instead. */
      cmp.factor[O_FOOD] = tune->cm_cap_food;
      cmp.factor[O_SHIELD] = tune->cm_cap_shield;
      cmp.factor[O_TRADE] = tune->cm_cap_trade;
      cmp.happy_factor = tune->cm_happy_factor;
    } else if (VUT_IMPROVEMENT == pcity->production.kind
               && is_wonder(pcity->production.value.building)) {
      /* Wonder under construction: shields, with trade kept up to
       * fund completion. */
      cmp.factor[O_FOOD] = tune->cm_war_food;
      cmp.factor[O_SHIELD] = tune->cm_wonder_shield;
      cmp.factor[O_TRADE] = tune->cm_wonder_trade;
      cmp.happy_factor = tune->cm_happy_factor;
    } else if (city_size_get(pcity) < tune->cm_grow_below) {
      /* Young city with headroom: growth compounds, so feed it.
       * Deep runs fewer cities than stock at stock's growth pace
       * (mean size 3.5->5.4 IDENTICAL both sides, garden2b); quality
       * must compensate quantity, and civ2civ3 has no rapture, so
       * the CM food weight is the only growth lever there is. */
      cmp.factor[O_FOOD] = tune->cm_grow_food;
      cmp.factor[O_SHIELD] = tune->cm_grow_shield;
      cmp.factor[O_TRADE] = tune->cm_grow_trade;
      cmp.happy_factor = tune->cm_happy_factor;
    } else {
      matched = FALSE;
    }

    if (matched) {
      if (pcity->cm_parameter == NULL) {
        pcity->cm_parameter = fc_malloc(sizeof(*pcity->cm_parameter));
        cm_init_parameter(pcity->cm_parameter);
      }
      if (!cm_are_parameter_equal(pcity->cm_parameter, &cmp)) {
        /* The rule for this city changed; rearrange now rather than
         * waiting for the next organic rearrangement, or the new
         * priorities sit unused for many turns. */
        cm_copy_parameter(pcity->cm_parameter, &cmp);
        auto_arrange_workers(pcity);
      }
    } else if (pcity->cm_parameter != NULL) {
      /* Back to the stock default for this city. */
      free(pcity->cm_parameter);
      pcity->cm_parameter = NULL;
      auto_arrange_workers(pcity);
    }
  } city_list_iterate_end;
}
