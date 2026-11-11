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

/* Tests of the default AI's air unit code. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* ai/default */
#include "daiair.c"

#include "fixture.h"

TestSuite(daiair, .init = fx_init_civ2civ3, .fini = fx_fini);

/* A damaged civ2civ3 Fighter in a city at (5, 5), an own Carrier on
 * the ocean next to it. Air units get no hp back outside a city or air
 * base in civ2civ3, so the Carrier tile never repairs the Fighter. */
static struct unit *fighter_beside_carrier(void)
{
  struct player *pplayer = fx_ai_player(nullptr);
  struct unit *fighter;

  fx_set_terrain(6, 5, "Ocean");
  fx_city(pplayer, 5, 5, "Home");
  fx_unit(pplayer, "Carrier", 6, 5, 0);
  fighter = fx_unit(pplayer, "Fighter", 5, 5, 0);
  fighter->hp = 12;
  fx_show_all(pplayer);
  fx_phase_begin();

  return fighter;
}

/* regen_turns() rates the no-repair Carrier tile as the worst value and
 * leaves the unit where it was. */
Test(daiair, regen_turns_no_regen)
{
  struct unit *fighter = fighter_beside_carrier();
  struct tile *home = fx_tile(5, 5);
  int lost_hp = unit_type_get(fighter)->hp - fighter->hp;
  int turns;

  cr_assert_gt(lost_hp, 0);
  turns = regen_turns(fighter, fx_tile(6, 5), lost_hp);
  cr_expect_eq(unit_tile(fighter), home, "unit left at (%d, %d)",
               TILE_XY(unit_tile(fighter)));
  cr_expect_geq(turns, 0, "regen_turns() = %d at the Carrier", turns);
}

/* The airbase search keeps the city, which repairs the Fighter, over
 * the Carrier, which does not. */
Test(daiair, airbase_prefers_repair)
{
  struct unit *fighter = fighter_beside_carrier();
  struct tile *home = fx_tile(5, 5);
  struct tile *best;

  cr_assert_gt(regen_turns(fighter, home,
                           unit_type_get(fighter)->hp - fighter->hp), 0);
  cr_assert_eq(unit_tile(fighter), home);

  best = find_nearest_airbase(fighter, nullptr);
  cr_assert_not_null(best);
  cr_expect_eq(best, home, "airbase search took (%d, %d)", TILE_XY(best));
  cr_expect_eq(unit_tile(fighter), home, "unit left at (%d, %d)",
               TILE_XY(unit_tile(fighter)));
}
