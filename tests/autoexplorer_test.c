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
