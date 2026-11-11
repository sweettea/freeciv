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

/* Tests of server/advisors/advdata.c.
 *
 * The file under test is compiled into this test, so its static
 * functions can be called by name. See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* server */
#include "srv_main.h"

/* server/advisors */
#include "advdata.c"

#include "fixture.h"

TestSuite(advdata, .init = fx_init_classic, .fini = fx_fini);

/* Only units that have something to upgrade to are counted as
 * upgradeable: Warriors once Feudalism brings Pikemen, but not
 * Workers, which wait for Explosives. */
Test(advdata, upgradeable_counts_obsolete_units_only)
{
  struct player *pplayer = fx_ai_player(nullptr);

  fx_tech(pplayer, "Feudalism");
  fx_unit(pplayer, "Warriors", 5, 5, 0);
  fx_unit(pplayer, "Workers", 6, 5, 0);
  fx_phase_begin();

  cr_expect_eq(adv_data_get(pplayer, nullptr)->stats.units.upgradeable, 1,
               "upgradeable = %d",
               adv_data_get(pplayer, nullptr)->stats.units.upgradeable);
}
