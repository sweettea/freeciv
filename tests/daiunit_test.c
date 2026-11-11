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

/* Tests of static functions in ai/default/daiunit.c.
 *
 * The file under test is compiled into this test, so its static
 * functions can be called by name. See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "ai.h"

/* server */
#include "aiiface.h"
#include "citytools.h"
#include "unittools.h"

/* ai/default */
#include "daiunit.c"

#include "fixture.h"

TestSuite(daiunit, .init = fx_init_classic, .fini = fx_fini);

/* A unit next to an empty enemy city walks in when its rampage is told
 * to move only for a free city. The city is size 1, so conquering it
 * destroys it. Real cities and units, since conquest frees the city; the
 * map is shown before the unit's vision is set up so that the city is
 * known to us. */
Test(daiunit, rampage_takes_free_city)
{
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct unit *punit;
  int id;

  fx_war(us, them);
  create_city(us, fx_tile(3, 5), "Home", us);
  create_city(them, fx_tile(9, 5), "Target", them);
  cr_assert_not_null(tile_city(fx_tile(9, 5)));
  fx_show_all(us);
  fx_show_all(them);
  punit = create_unit(us, fx_tile(8, 5), fx_utype("Warriors"), 0,
                      tile_city(fx_tile(3, 5))->id, -1);
  cr_assert_not_null(punit);
  id = punit->id;
  fx_phase_begin();

  cr_assert_eq(dai_rampage_want(punit, fx_tile(9, 5)),
               -RAMPAGE_FREE_CITY_OR_BETTER);
  cr_expect(dai_military_rampage(punit, BODYGUARD_RAMPAGE_THRESHOLD,
                                 RAMPAGE_FREE_CITY_OR_BETTER));
  cr_assert_not_null(game_unit_by_number(id));
  cr_expect_eq(unit_tile(punit), fx_tile(9, 5),
               "Warriors still at (%d, %d)", TILE_XY(unit_tile(punit)));
  cr_expect_null(tile_city(fx_tile(9, 5)), "enemy city still stands");
}
