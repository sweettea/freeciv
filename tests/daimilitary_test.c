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

/* Tests of static functions in ai/default/daimilitary.c.
 *
 * The file under test is compiled into this test, so its static
 * functions can be called by name. Its non-static definitions then
 * satisfy every reference the program makes to that object file, and
 * the linker does not pull the library's copy out of libfc_ai.a.
 * See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "ai.h"

/* server */
#include "aiiface.h"

/* ai/default */
#include "daimilitary.c"

#include "fixture.h"

TestSuite(daimilitary, .init = fx_init_classic, .fini = fx_fini);

/* assess_danger() sees no danger to a city with no enemy around, and
 * some once an enemy Armor stands three tiles away. */
Test(daimilitary, assess_danger_sees_an_attacker)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct city *pcity = fx_city(us, 5, 5, "Home");

  fx_war(us, them);
  fx_show_all(us);
  fx_phase_begin();

  cr_expect_eq(assess_danger(ait, &(wld.map), pcity, nullptr), 0);
  cr_expect_eq(def_ai_city_data(pcity, ait)->danger, 0);

  fx_unit(them, "Armor", 8, 5, 0);
  fx_show_all(us);
  (void) assess_danger(ait, &(wld.map), pcity, nullptr);
  cr_expect_gt(def_ai_city_data(pcity, ait)->danger, 0);
}
