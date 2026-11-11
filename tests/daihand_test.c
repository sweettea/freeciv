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

/* Tests of ai/default/daihand.c.
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

/* ai/default */
#include "daihand.c"

#include "fixture.h"

TestSuite(daihand, .init = fx_init_classic, .fini = fx_fini);

/* With 100 trade and 30 gold of upkeep, and last turn's gold exactly
 * as the rates predict, the AI picks a tax rate that pays the upkeep:
 * at least 30%. */
Test(daihand, taxes_cover_expenses)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *a = fx_ai_player(nullptr);
  struct city *pcity = fx_city(a, 5, 5, "Home");

  /* Max rate 90%, so the search starts from 0% tax and what tax does
   * not need goes to science. */
  fx_government(a, "Democracy");
  fx_phase_begin();
  a->economic.gold = 1000;
  a->economic.tax = 30;
  a->economic.science = 70;
  a->economic.luxury = 0;
  /* As a city refresh at those rates would leave it, with 30 gold of
   * building upkeep: tax 30, upkeep 30, no gold left over. */
  pcity->surplus[O_TRADE] = 100;
  pcity->usage[O_GOLD] = 30;
  pcity->surplus[O_GOLD] = 0;
  cr_assert_lt(dai_gold_reserve(a), a->economic.gold);

  dai_manage_taxes(ait, a);
  cr_expect_geq(a->economic.tax, 30, "tax %d%%, sci %d%%, lux %d%%",
                a->economic.tax, a->economic.science, a->economic.luxury);
}
