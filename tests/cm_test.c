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

/* The city governor's solver, common/aicore/cm.c, through its public
 * entry points. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "city.h"
#include "game.h"
#include "map.h"
#include "specialist.h"

/* common/aicore */
#include "cm.h"

#include "fixture.h"

TestSuite(cm, .init = fx_init_classic, .fini = fx_fini);

/**********************************************************************//**
  Fill the map with `terrain`.
**************************************************************************/
static void fill_map(const char *terrain)
{
  int x, y;

  for (x = 0; x < FX_MAP_XSIZE; x++) {
    for (y = 0; y < FX_MAP_YSIZE; y++) {
      fx_set_terrain(x, y, terrain);
    }
  }
}

/**********************************************************************//**
  A city of `size` on (x, y), all its citizens default specialists,
  refreshed.
**************************************************************************/
static struct city *sized_city(struct player *owner, int x, int y,
                               int size)
{
  struct city *pcity = fx_city(owner, x, y, "Test");

  fx_see_all(owner);

  city_size_set(pcity, size);
  pcity->specialists[DEFAULT_SPECIALIST] = size;
  city_refresh_from_main_map(&(wld.map), pcity, nullptr);

  return pcity;
}

/**********************************************************************//**
  Turns to fill `pcity`'s granary at `surplus` food a turn, or -1 if it
  never fills.
**************************************************************************/
static int turns_to_grow(const struct city *pcity, int surplus)
{
  int needed = city_granary_size(city_size_get(pcity)) - pcity->food_stock;

  if (surplus <= 0) {
    return -1;
  }
  return (needed + surplus - 1) / surplus;
}

/**********************************************************************//**
  max_growth asks for the fastest growth the city can reach. A size 4
  city in forest with four grassland tiles can grow; with shields worth
  ten times food the governor must still pick the arrangement that
  grows in the fewest turns, not a starving one.
**************************************************************************/
Test(cm, max_growth_grows_fastest)
{
  struct player *owner;
  struct city *pcity;
  struct cm_parameter param;
  struct cm_result *result;
  int best_food, best_turns, turns;

  fill_map("Forest");
  fx_set_terrain(5, 5, "Grassland");
  fx_set_terrain(4, 5, "Grassland");
  fx_set_terrain(6, 5, "Grassland");
  fx_set_terrain(5, 4, "Grassland");
  fx_set_terrain(5, 6, "Grassland");
  owner = fx_player();
  pcity = sized_city(owner, 5, 5, 4);
  result = cm_result_new(pcity);

  /* The most food the city can make. */
  cm_init_parameter(&param);
  param.factor[O_FOOD] = 1;
  param.factor[O_SHIELD] = 0;
  param.factor[O_TRADE] = 0;
  param.factor[O_GOLD] = 0;
  param.factor[O_LUXURY] = 0;
  param.factor[O_SCIENCE] = 0;
  param.happy_factor = 0;
  cm_query_result(pcity, &param, result, FALSE);
  cr_assert(result->found_a_valid, "food %d shield %d trade %d gold %d"
            " lux %d sci %d disorder %d", result->surplus[O_FOOD],
            result->surplus[O_SHIELD], result->surplus[O_TRADE],
            result->surplus[O_GOLD], result->surplus[O_LUXURY],
            result->surplus[O_SCIENCE], result->disorder);
  best_food = result->surplus[O_FOOD];
  best_turns = turns_to_grow(pcity, best_food);
  cr_assert_gt(best_turns, 0, "best food surplus %d", best_food);

  cm_init_parameter(&param);
  param.factor[O_FOOD] = 1;
  param.factor[O_SHIELD] = 10;
  param.minimal_surplus[O_FOOD] = 1;
  param.max_growth = TRUE;
  cm_query_result(pcity, &param, result, FALSE);
  cr_assert(result->found_a_valid);
  turns = turns_to_grow(pcity, result->surplus[O_FOOD]);
  cr_expect_geq(result->surplus[O_FOOD], 1, "max_growth food surplus %d",
                result->surplus[O_FOOD]);
  cr_expect_eq(turns, best_turns, "max_growth grows in %d turns (food %d),"
               " the city can in %d (food %d)", turns,
               result->surplus[O_FOOD], best_turns, best_food);

  cm_result_destroy(result);
}
