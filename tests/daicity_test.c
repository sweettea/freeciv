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

/* Tests of ai/default/daicity.c, including its static functions.
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
#include "daicity.c"

#include "fixture.h"

/* The building want the default AI's building advisor gives `impr_name`
 * in `pcity`, from a recalculation for its owner this turn. */
static adv_want building_want(struct city *pcity, const char *impr_name)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = city_owner(pcity);
  struct impr_type *pimprove = improvement_by_rule_name(impr_name);

  cr_assert_not_null(pimprove, "no building \"%s\"", impr_name);
  city_list_iterate(pplayer->cities, acity) {
    def_ai_city_data(acity, ait)->building_turn = game.info.turn;
  } city_list_iterate_end;
  dai_build_adv_init(ait, pplayer);
  dai_build_adv_adjust(ait, pplayer, nullptr);

  return pcity->server.adv->building_want[improvement_index(pimprove)];
}

TestSuite(daicity, .init = fx_init_classic, .fini = fx_fini);

/* Mysticism gives classic's Temple a second Make_Content 1. In a
 * one-city empire's size-1 city there is no unhappy citizen to make
 * content, so knowing Mysticism does not make a Temple more wanted. */
Test(daicity, temple_make_content_in_content_city)
{
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 5, 5, "Capital");
  adv_want without, with;

  fx_government(pplayer, "Despotism");
  fx_tech(pplayer, "Ceremonial Burial");
  fx_phase_begin();

  cr_assert_eq(pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL], 0);
  without = building_want(pcity, "Temple");
  fx_tech(pplayer, "Mysticism");
  cr_assert_eq(pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL], 0);
  with = building_want(pcity, "Temple");
  cr_expect_eq(with, without,
               "Temple want " ADV_WANT_PRINTF " with Mysticism, "
               ADV_WANT_PRINTF " without", with, without);
}
