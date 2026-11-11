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
    def_ai_city_data(acity, ait)->building_wait = BUILDING_WAIT_MINIMUM;
  } city_list_iterate_end;
  dai_build_adv_init(ait, pplayer);
  dai_build_adv_adjust(ait, pplayer, nullptr);

  return pcity->server.adv->building_want[improvement_index(pimprove)];
}

/* Make `pcity` the home of `punit`, paying its upkeep. */
static void home_unit(struct unit *punit, struct city *pcity)
{
  punit->homecity = pcity->id;
  unit_list_prepend(pcity->units_supported, punit);
  city_refresh(pcity);
}

/* Unhome `punit` from `pcity`. */
static void unhome_unit(struct unit *punit, struct city *pcity)
{
  unit_list_remove(pcity->units_supported, punit);
  punit->homecity = IDENTITY_NUMBER_ZERO;
  city_refresh(pcity);
}

/* Home Warriors standing in `pcity` there until its shield surplus is
 * 0; returns the last one homed. */
static struct unit *home_warriors_to_break_even(struct city *pcity)
{
  struct unit *punit = nullptr;
  int i;

  for (i = 0; i < 10 && pcity->surplus[O_SHIELD] > 0; i++) {
    punit = fx_unit(city_owner(pcity), "Warriors",
                    index_to_map_pos_x(tile_index(city_tile(pcity))),
                    index_to_map_pos_y(tile_index(city_tile(pcity))), 0);
    home_unit(punit, pcity);
  }
  cr_assert_eq(pcity->surplus[O_SHIELD], 0);
  cr_assert_not_null(punit);

  return punit;
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

/* A city short of shields will not finish a building any sooner, so a
 * deficit does not make a building more wanted than at break-even. */
Test(daicity, shield_deficit_does_not_cheapen_buildings)
{
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 5, 5, "Capital");
  adv_want even, deficit;

  fx_government(pplayer, "Despotism");
  fx_tech(pplayer, "Ceremonial Burial");
  home_warriors_to_break_even(pcity);
  home_unit(fx_unit(pplayer, "Warriors", 5, 5, 0), pcity);
  cr_assert_eq(pcity->surplus[O_SHIELD], -1);
  fx_phase_begin();
  deficit = building_want(pcity, "Temple");

  unhome_unit(unit_list_get(pcity->units_supported, 0), pcity);
  cr_assert_eq(pcity->surplus[O_SHIELD], 0);
  even = building_want(pcity, "Temple");

  cr_expect_leq(deficit, even,
                "Temple want " ADV_WANT_PRINTF " at surplus -1, "
                ADV_WANT_PRINTF " at 0", deficit, even);
}

/* A building the city has costs nothing more to build, so how much the
 * AI holds Gunpowder against the Barracks it owns does not depend on
 * the city's shield surplus. The Warriors are the same either way; only
 * who pays their upkeep changes. */
Test(daicity, owned_building_has_no_build_cost)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 5, 5, "Capital");
  struct advance *gunpowder = advance_by_rule_name("Gunpowder");
  struct impr_type *barracks = improvement_by_rule_name("Barracks");
  adv_want *tech_want = def_ai_player_data(pplayer, ait)->tech_want;
  struct unit *last;
  adv_want even, surplus;

  fx_government(pplayer, "Despotism");
  city_add_improvement(pcity, barracks);
  city_refresh(pcity);
  last = home_warriors_to_break_even(pcity);
  fx_phase_begin();
  def_ai_city_data(pcity, ait)->building_wait = BUILDING_WAIT_MINIMUM;

  tech_want[advance_index(gunpowder)] = 0;
  adjust_improvement_wants_by_effects(ait, pplayer, pcity, barracks, TRUE);
  even = tech_want[advance_index(gunpowder)];

  unhome_unit(last, pcity);
  cr_assert_eq(pcity->surplus[O_SHIELD], 1);
  tech_want[advance_index(gunpowder)] = 0;
  adjust_improvement_wants_by_effects(ait, pplayer, pcity, barracks, TRUE);
  surplus = tech_want[advance_index(gunpowder)];

  cr_expect_eq(surplus, even,
               "Gunpowder want " ADV_WANT_PRINTF " at surplus 1, "
               ADV_WANT_PRINTF " at 0", surplus, even);
}

TestSuite(daicity_civ2civ3, .init = fx_init_civ2civ3, .fini = fx_fini);

/* civ2civ3's Great Library gives its city +4 science on the centre tile
 * and every Library city +50% science, at World range. Its base want
 * in a Library city counts those, as for any other building. */
Test(daicity_civ2civ3, great_library_base_want)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 5, 5, "Capital");
  struct impr_type *library = improvement_by_rule_name("Library");
  struct impr_type *great_library
    = improvement_by_rule_name("Great Library");
  struct adv_data *adv;
  adv_want want;

  fx_government(pplayer, "Despotism");
  fx_tech(pplayer, "Literacy");
  city_add_improvement(pcity, library);
  city_refresh(pcity);
  fx_phase_begin();
  adv = adv_data_get(pplayer, nullptr);

  cr_assert(can_city_build_improvement_now(pcity, great_library,
                                           RPT_CERTAIN));
  cr_assert_eq(adv->impr_range[improvement_index(great_library)],
               REQ_RANGE_WORLD);
  def_ai_city_data(pcity, ait)->worth
    = dai_city_want(pplayer, pcity, adv, nullptr);
  want = base_want(ait, pplayer, pcity, great_library);
  cr_expect_gt(want, 0, "Great Library base want " ADV_WANT_PRINTF, want);
}
