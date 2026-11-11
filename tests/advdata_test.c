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

/* A player created once the game is running (a civil war's rebels, a
 * /create) gets the same building analysis as those there at the
 * start: Harbour adds food per tile, so it is fully calculated. */
Test(advdata, player_created_in_game_gets_impr_calc)
{
  const struct impr_type *harbour = improvement_by_rule_name("Harbour");
  struct player *pplayer;

  set_server_state(S_S_RUNNING);
  pplayer = fx_ai_player(nullptr);

  cr_expect_eq(pplayer->server.adv->impr_calc[improvement_index(harbour)],
               ADV_IMPR_CALCULATE_FULL, "impr_calc %d",
               pplayer->server.adv->impr_calc[improvement_index(harbour)]);
}

/* An enemy that has died is no threat, so being allied with it does
 * not make a player dangerous. A is at war with B, B is allied with C,
 * and B dies. */
Test(advdata, dead_enemy_has_no_dangerous_allies)
{
  struct player *a = fx_ai_player(nullptr);
  struct player *b = fx_ai_player(nullptr);
  struct player *c = fx_ai_player(nullptr);

  fx_war(a, b);
  fx_diplstate(b, c, DS_ALLIANCE);
  fx_diplstate(a, c, DS_PEACE);
  a->ai_common.love[player_index(c)] = MAX_AI_LOVE;
  b->is_alive = FALSE;
  fx_phase_begin();

  cr_expect(!adv_dipl_get(a, c)->allied_with_enemy,
            "C counts as allied with A's enemy");
  cr_expect(!adv_is_player_dangerous(a, c), "C is dangerous to A");
}
