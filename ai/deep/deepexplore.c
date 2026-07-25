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
#include "extras.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* server */
#include "maphand.h"

#include "deeptuning.h"

#include "deepexplore.h"

/* Exploration want (W06). No advisor in the stock AI ever wants an
 * explorer built: exploration happens only as an AIUNIT_NONE fallback,
 * and dai_spend_gold() disbands explorers that come home. In hut games
 * that forfeits one of the strongest early investments. */

/**********************************************************************//**
  How much this player should currently want to build explorers, in
  build-want units: visible unclaimed huts plus unexplored area, with
  diminishing returns per explorer already owned. Zero when the map was
  generated without huts, so hut-free behaviour stays stock (the
  discovery term is deliberately inside the gate for now; widening it
  is a W08-era decision).
**************************************************************************/
adv_want deep_explore_want(struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int huts_visible = 0, unknown = 0, explorers = 0;
  adv_want want;

  if (wld.map.server.huts <= 0) {
    return 0;
  }

  whole_map_iterate(&(wld.map), ptile) {
    if (!map_is_known(ptile, pplayer)) {
      unknown++;
    } else if (hut_on_tile(ptile)) {
      huts_visible++;
    }
  } whole_map_iterate_end;

  unit_list_iterate(pplayer->units, punit) {
    if (unit_has_type_role(punit, L_EXPLORER)) {
      explorers++;
    }
  } unit_list_iterate_end;

  want = (huts_visible * tune->explore_hut_value
          + unknown * tune->explore_discovery_value)
         / (1 + explorers);

  log_base(LOG_NORMAL,
           "deep-explore turn=%d player=\"%s\" want=%d huts_vis=%d"
           " unknown=%d explorers=%d",
           game.info.turn, player_name(pplayer), (int) want,
           huts_visible, unknown, explorers);

  return want;
}
