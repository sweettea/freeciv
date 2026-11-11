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

/* Tests of ai/default/daidomestic.c, including its static functions.
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
#include "daidomestic.c"

#include "fixture.h"

TestSuite(daidomestic, .init = fx_init_classic, .fini = fx_fini);

/* With nothing but one building wanted, a player of default builder
 * trait is advised to build it at the building's own want. */
Test(daidomestic, builder_trait_default_keeps_want)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 5, 5, "Capital");
  struct impr_type *barracks = improvement_by_rule_name("Barracks");
  struct ai_city *city_data;
  struct adv_choice *choice;

  fx_phase_begin();
  pplayer->ai_common.traits[TRAIT_BUILDER].val = TRAIT_DEFAULT_VALUE;
  pplayer->ai_common.traits[TRAIT_BUILDER].mod = 0;
  city_data = def_ai_city_data(pcity, ait);
  city_data->worker_want = 0;
  city_data->founder_want = 0;
  improvement_iterate(pimprove) {
    pcity->server.adv->building_want[improvement_index(pimprove)] = 0;
  } improvement_iterate_end;
  pcity->server.adv->building_want[improvement_index(barracks)] = 100;

  choice = domestic_advisor_choose_build(ait, pplayer, pcity);
  cr_assert_eq(choice->type, CT_BUILDING);
  cr_assert_eq(choice->value.building, barracks);
  cr_expect_eq(choice->want, 100, "want " ADV_WANT_PRINTF, choice->want);
  adv_free_choice(choice);
}

/* The want for a trade route unit in a city with no route and no
 * caravan of its own. */
static adv_want trade_route_want(struct ai_type *ait, struct city *pcity)
{
  struct adv_choice choice;

  adv_init_choice(&choice);
  dai_choose_trade_route(ait, pcity, &choice,
                         adv_data_get(city_owner(pcity), nullptr));
  cr_assert_eq(choice.type, CT_CIVILIAN);

  return choice.want;
}

/* A city with no trade route and no caravan wants a caravan just as
 * much whether it has two free trade route slots or three: it has no
 * idle caravans either way. */
Test(daidomestic, free_route_slots_are_not_idle_caravans)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 2, 2, "Home");
  adv_want two_slots, three_slots;

  fx_city(pplayer, 17, 2, "East");
  fx_city(pplayer, 2, 17, "South");
  fx_city(pplayer, 17, 17, "Far");
  fx_city(pplayer, 10, 10, "Middle");
  fx_tech(pplayer, "Trade");
  fx_phase_begin();

  cr_assert_eq(max_trade_routes(pcity), 2);
  two_slots = trade_route_want(ait, pcity);

  fx_tech(pplayer, "Magnetism");
  cr_assert_eq(max_trade_routes(pcity), 3);
  three_slots = trade_route_want(ait, pcity);

  cr_expect_eq(three_slots, two_slots,
               "want " ADV_WANT_PRINTF " with 3 slots, " ADV_WANT_PRINTF
               " with 2", three_slots, two_slots);
}
