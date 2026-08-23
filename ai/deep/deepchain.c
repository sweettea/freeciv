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

/* common */
#include "city.h"
#include "player.h"
#include "terrain.h"
#include "tile.h"

/* server/advisors */
#include "advtools.h"
#include "infracache.h"

/* ai/deep */
#include "deepai.h"
#include "deepterra.h"
#include "deeptuning.h"

#include "deepchain.h"

/**********************************************************************//**
  What 'act' is worth on 'ptile' to 'pcity', given the one-step value
  the advisors computed. Registered on the advisors' terrain-value hook;
  see deepchain.h.
**************************************************************************/
static adv_want deep_chain_value(const struct city *pcity,
                                 const struct tile *ptile,
                                 enum unit_activity act,
                                 const struct terrain *result,
                                 adv_want one_step)
{
  const struct player *pplayer;
  const struct deep_tuning *tune;
  struct deep_terra_step reach[DEEP_TERRA_REACH_MAX];
  adv_want best = one_step;
  int n, i;

  if (pcity == NULL || ptile == NULL) {
    return one_step;
  }
  pplayer = city_owner(pcity);
  if (pplayer == NULL || pplayer->ai != deep_ai_get_self()) {
    /* A stock seat in a mixed game keeps stock's price, which is what
     * makes the mixed cell a control rather than a second treatment. */
    return one_step;
  }

  tune = deep_tuning_for(pplayer);
  if (tune->terrain_chain_gate <= 0) {
    return one_step;
  }

  n = deep_terra_reach(tile_terrain(ptile), act, reach);
  if (n <= 1) {
    /* No chain: the immediate result is everything this action reaches,
     * and its value is what we were handed. */
    return one_step;
  }

  /* reach[0] is the immediate result, whose price is 'one_step' -- the
   * caller has already computed exactly that, on exactly this tile, and
   * recomputing it here would only be a way to disagree with it. */
  fc_assert(reach[0].pterrain == result);

  for (i = 1; i < n; i++) {
    struct tile *vtile;
    adv_want value;

    if (tile_city(ptile)
        && terrain_has_flag(reach[i].pterrain, TER_NO_CITIES)) {
      /* The same refusal the one-step evaluators make. */
      continue;
    }

    vtile = tile_virtual_new(ptile);
    tile_change_terrain(vtile, (struct terrain *) reach[i].pterrain);
    value = city_tile_value(pcity, vtile, 0, 0);
    tile_virtual_destroy(vtile);

    /* Only the turns BEYOND this first step: the caller amortizes the
     * first step and the walk to the tile itself. */
    value = amortize(value, reach[i].turns - reach[0].turns);
    if (value > best) {
      best = value;
    }
  }

  return best;
}

/**********************************************************************//**
  Register the chain valuation with the advisors. See deepchain.h: the
  registration is unconditional and the gate is per-seat.
**************************************************************************/
void deep_chain_register(void)
{
  adv_terrain_value_hook_set(deep_chain_value);
}
