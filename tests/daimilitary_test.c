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

/* Give `pplayer` the default builder trait and no decision fuzz. */
static void default_builder(struct player *pplayer)
{
  pplayer->ai_common.traits[TRAIT_BUILDER].val = TRAIT_DEFAULT_VALUE;
  pplayer->ai_common.traits[TRAIT_BUILDER].mod = 0;
  pplayer->ai_common.fuzzy = 0;
}

/* At the default builder trait, adjust_ai_unit_choice() swaps a
 * defender for Barracks at the defender's full want. */
Test(daimilitary, veterancy_building_want_at_default_trait)
{
  struct player *us = fx_ai_player(nullptr);
  struct city *pcity = fx_city(us, 5, 5, "Home");
  struct adv_choice choice;

  default_builder(us);
  fx_phase_begin();

  adv_init_choice(&choice);
  choice.type = CT_DEFENDER;
  choice.value.utype = (struct unit_type *) fx_utype("Phalanx");
  choice.want = 100;
  adjust_ai_unit_choice(pcity, &choice);

  cr_assert_eq(choice.type, CT_BUILDING);
  cr_expect_str_eq(improvement_rule_name(choice.value.building), "Barracks");
  cr_expect_float_eq(choice.want, 100.0, 1e-9,
                     "want " ADV_WANT_PRINTF, choice.want);
  adv_deinit_choice(&choice);
}

/* At the default builder trait, military_advisor_choose_build() asks
 * for City Walls at their full building want. The city has no shield
 * surplus, so no defender is wanted and the advisor stops after the
 * defence choice. */
Test(daimilitary, walls_want_at_default_trait)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct city *pcity;
  const struct impr_type *walls;
  struct adv_choice *choice;

  default_builder(us);
  fx_war(us, them);
  fx_tech(us, "Masonry");
  fx_tech(us, "Gunpowder");
  pcity = fx_city(us, 5, 5, "Home");
  fx_unit(us, "Musketeers", 5, 5, 0);
  fx_unit(us, "Musketeers", 5, 5, 0);
  fx_unit(them, "Legion", 6, 5, 0);
  fx_show_all(us);
  fx_phase_begin();

  walls = improvement_by_rule_name("City Walls");
  pcity->server.adv->building_want[improvement_index(walls)] = 50;
  pcity->surplus[O_SHIELD] = 0;

  choice = military_advisor_choose_build(ait, &(wld.map), us, pcity,
                                         nullptr);
  cr_assert_eq(choice->type, CT_BUILDING, "choice type %d", choice->type);
  cr_assert_eq(choice->value.building, walls, "chose %s",
               improvement_rule_name(choice->value.building));
  cr_expect_float_eq(choice->want,
                     pcity->server.adv->building_want[improvement_index(walls)],
                     1e-9, "want " ADV_WANT_PRINTF " building_want "
                     ADV_WANT_PRINTF, choice->want,
                     pcity->server.adv->building_want[improvement_index(walls)]);
  adv_free_choice(choice);
}
