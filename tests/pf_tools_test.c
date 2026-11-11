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

/* Path-finding parameters from common/aicore/pf_tools.c. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "game.h"
#include "map.h"
#include "movement.h"
#include "unittype.h"

/* common/aicore */
#include "pf_tools.h"

#include "fixture.h"

TestSuite(pf_tools, .init = fx_init_classic, .fini = fx_fini);

/**********************************************************************//**
  A virtual unit type's path starts with the full move rate, bonuses
  included: with Nuclear Power a classic Galleon moves 5, and has 5
  moves on its first turn too.
**************************************************************************/
Test(pf_tools, utype_starts_with_bonus_moves)
{
  struct player *owner = fx_player();
  const struct unit_type *galleon = fx_utype("Galleon");
  struct pf_parameter param;

  fx_set_terrain(5, 5, "Ocean");
  fx_tech(owner, "Nuclear Power");
  pft_fill_utype_parameter(&param, &(wld.map), galleon, fx_tile(5, 5),
                           owner);

  cr_expect_eq(param.move_rate, galleon->move_rate + SINGLE_MOVE,
               "move_rate %d, base %d", param.move_rate,
               galleon->move_rate);
  cr_expect_eq(param.moves_left_initially, param.move_rate,
               "moves_left_initially %d, move_rate %d",
               param.moves_left_initially, param.move_rate);
}
