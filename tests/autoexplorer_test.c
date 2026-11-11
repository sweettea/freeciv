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

/* Tests of static functions in server/advisors/autoexplorer.c.
 *
 * The file under test is compiled into this test, so its static
 * functions can be called by name. See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* server */
#include "maphand.h"

/* server/advisors */
#include "autoexplorer.c"

#include "fixture.h"

TestSuite(autoexplorer, .init = fx_init_classic, .fini = fx_fini);

/* An unknown tile is guessed from what the player knows: with all
 * eight neighbours known Grassland, a land unit thinks it almost
 * certainly native, whatever the tile really is. */
Test(autoexplorer, likely_native_reads_the_known_neighbours)
{
  struct player *pplayer = fx_player();
  struct unit *punit = fx_unit(pplayer, "Explorer", 2, 2, 0);
  struct tile *hidden = fx_tile(5, 5);
  int native;

  fx_set_terrain(5, 5, "Ocean");
  fx_show_all(pplayer);
  map_clear_known(hidden, pplayer);

  native = likely_native(hidden, pplayer, unit_class_get(punit));
  cr_expect_eq(native, 50 + 50 / MAP_NUM_VALID_DIRS * MAP_NUM_VALID_DIRS,
               "likely_native() = %d", native);
}

/* An AI explorer wants a hut on a tile its player knows, whatever its
 * hut handicap: H_HUTS hides only huts in unexplored territory. The
 * default skill level, easy, has H_HUTS. The whole map is known, so
 * the hut is the tile's only value. */
Test(autoexplorer, ai_explorer_wants_known_hut)
{
  struct player *pplayer = fx_ai_player(nullptr);
  struct unit *explorer = fx_unit(pplayer, "Explorer", 2, 5, 0);
  struct tile *hut = fx_tile(4, 5);
  int desirable;

  tile_add_extra(hut, extra_type_by_rule_name("Hut"));
  fx_show_all(pplayer);
  cr_assert(has_handicap(pplayer, H_HUTS));

  desirable = explorer_desirable(hut, pplayer, explorer);
  cr_expect_gt(desirable, 0, "known hut desirable %d", desirable);
}
