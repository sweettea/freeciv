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

/* Tests of ai/default/daieffects.c, on effects as the rulesets ship
 * them. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* utility */
#include "support.h"

/* common */
#include "city.h"
#include "effects.h"
#include "improvement.h"
#include "player.h"
#include "research.h"

/* server */
#include "cityturn.h"
#include "plrhand.h"

/* server/advisors */
#include "advdata.h"

/* ai/default */
#include "daieffects.h"

#include "fixture.h"

/* The ruleset's effect of type `type` that building `impr_name` gives. */
static struct effect *building_effect(const char *impr_name,
                                      enum effect_type type)
{
  struct impr_type *pimprove = improvement_by_rule_name(impr_name);
  struct universal source = { .kind = VUT_IMPROVEMENT };

  cr_assert_not_null(pimprove, "no building \"%s\"", impr_name);
  source.value.building = pimprove;
  effect_list_iterate(get_req_source_effects(&source), peffect) {
    if (peffect->type == type) {
      return peffect;
    }
  } effect_list_iterate_end;
  cr_assert_fail("%s has no %s effect", impr_name, effect_type_name(type));

  return nullptr;
}

TestSuite(daieffects_sandbox, .init = fx_init_sandbox, .fini = fx_fini);

/* Sandbox's Great Library is Tech_Parasite_Pct_Max 30. With three
 * researches, 30% of them rounds down to 0, which the server treats as
 * no effect at all; the AI values it at 0 too. */
Test(daieffects_sandbox, tech_parasite_pct_rounding_to_zero)
{
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity;
  struct effect *peffect;
  bool close;
  adv_want v;

  fx_ai_player(nullptr);
  fx_ai_player(nullptr);
  pcity = fx_city(pplayer, 5, 5, "Capital");
  peffect = building_effect("Great Library", EFT_TECH_PARASITE_PCT_MAX);
  fx_phase_begin();

  cr_assert_eq(research_count(), 3);
  cr_assert_eq(peffect->value, 30);
  v = dai_effect_value(pplayer, adv_data_get(pplayer, &close), pcity,
                       TRUE, 50, peffect, 1, normal_player_count());
  cr_expect_eq(v, 0, "valued at " ADV_WANT_PRINTF, v);
}

TestSuite(daieffects, .init = fx_init_classic, .fini = fx_fini);

/* Classic's Magellan's Expedition gives ships +2 moves, the Lighthouse
 * +1. To a player with no ships yet, both are worth something, and
 * Magellan's more. */
Test(daieffects, move_bonus_without_ships)
{
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 5, 5, "Capital");
  struct effect *lighthouse, *magellan;
  struct adv_data *adv;
  bool close;
  adv_want v_lighthouse, v_magellan;

  lighthouse = building_effect("Lighthouse", EFT_MOVE_BONUS);
  magellan = building_effect("Magellan's Expedition", EFT_MOVE_BONUS);
  fx_phase_begin();
  adv = adv_data_get(pplayer, &close);

  cr_assert_eq(lighthouse->value, 1);
  cr_assert_eq(magellan->value, 2);
  v_lighthouse = dai_effect_value(pplayer, adv, pcity, TRUE, 50,
                                  lighthouse, 1, normal_player_count());
  v_magellan = dai_effect_value(pplayer, adv, pcity, TRUE, 50,
                                magellan, 1, normal_player_count());
  cr_expect_gt(v_lighthouse, 0, "Lighthouse valued at " ADV_WANT_PRINTF,
               v_lighthouse);
  cr_expect_gt(v_magellan, v_lighthouse,
               "Magellan's valued at " ADV_WANT_PRINTF ", Lighthouse at "
               ADV_WANT_PRINTF, v_magellan, v_lighthouse);
}

/* The value of Make_Content 1 in the first of `ncities` cities, placed
 * along map row `y`, of a new player under `gov`, rated as for an
 * effect reaching 2 cities. */
static adv_want content_value(const char *gov, int ncities, int y)
{
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *first = nullptr;
  int i;

  fx_government(pplayer, gov);
  for (i = 0; i < ncities; i++) {
    char name[16];

    fc_snprintf(name, sizeof(name), "City %d", i);
    first = fx_city(pplayer, i, y, name);
  }
  city_refresh_for_player(pplayer);

  return dai_content_effect_value(pplayer, first, 1, 2, FEELING_EFFECT);
}

/* Classic's Communism has Empire_Size_Base 12 and no Empire_Size_Step:
 * past 12 cities one citizen more is unhappy, however many more cities
 * there are. So 20 cities need no more contentment than 14. */
Test(daieffects, content_value_without_empire_size_step)
{
  adv_want v14, v20;
  struct player *probe = fx_ai_player(nullptr);

  fx_government(probe, "Communism");
  cr_assert_eq(get_player_bonus(probe, EFT_EMPIRE_SIZE_BASE), 12);
  cr_assert_eq(get_player_bonus(probe, EFT_EMPIRE_SIZE_STEP), 0);

  v14 = content_value("Communism", 14, 5);
  v20 = content_value("Communism", 20, 10);
  cr_expect_eq(v20, v14,
               "Make_Content valued at " ADV_WANT_PRINTF " with 20 cities, "
               ADV_WANT_PRINTF " with 14", v20, v14);
}
