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

/* Tests of static functions in server/advisors/advgoto.c.
 *
 * The file under test is compiled into this test, so its static
 * functions can be called by name. See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* server/advisors */
#include "advgoto.c"

#include "fixture.h"

TestSuite(advgoto, .init = fx_init_classic, .fini = fx_fini);

/* The extra cost adv_avoid_risks() gives a tile beside a foreign unit,
 * for a unit of type `utype` three tiles away. */
static unsigned zoc_tile_cost(const char *utype)
{
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct unit *punit = fx_unit(us, utype, 2, 5, 0);
  struct pf_parameter parameter;
  struct adv_risk_cost risk_cost;

  fx_war(us, them);
  fx_unit(them, "Warriors", 5, 6, 0);
  fx_show_all(us);
  cr_assert(!is_plr_zoc_srv(us, fx_tile(5, 5), &(wld.map)));

  pft_fill_unit_parameter(&parameter, &(wld.map), punit);
  adv_avoid_risks(&parameter, &risk_cost, punit,
                  NORMAL_STACKING_FEARFULNESS);

  return parameter.get_EC(fx_tile(5, 5), TILE_KNOWN_SEEN, &parameter);
}

/* A unit that ignores ZOC cannot be stuck by it, so an enemy's ZOC
 * costs it nothing extra. */
Test(advgoto, zoc_cost_spares_igzoc_units)
{
  unsigned cost = zoc_tile_cost("Explorer");

  cr_expect_lt(cost, PF_TURN_FACTOR,
               "Explorer pays %u EC (%.2f turns) beside an enemy",
               cost, (double) cost / PF_TURN_FACTOR);
}

/* A unit subject to ZOC still pays for the risk of being stuck. */
Test(advgoto, zoc_cost_charged_to_zoc_units)
{
  unsigned cost = zoc_tile_cost("Warriors");

  cr_expect_geq(cost, PF_TURN_FACTOR * 20,
                "Warriors pays %u EC beside an enemy", cost);
}

/* The first step of the path adv_avoid_risks() gives an AI Explorer
 * from (2, 5) to (8, 5). A Grassland corridor runs along y = 5 with
 * Ocean either side for x = 3..7; the other routes, along y = 3 and
 * y = 7, are two IgTer steps (two thirds of a turn) longer. An enemy
 * Warriors on a Grassland spur at (5, 6) puts three corridor tiles in
 * its ZOC. */
static struct tile *explorer_first_step_past_zoc(void)
{
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct unit *explorer;
  struct pf_parameter parameter;
  struct adv_risk_cost risk_cost;
  struct pf_map *pfm;
  struct pf_path *path;
  struct tile *step;
  int x;

  for (x = 3; x <= 7; x++) {
    fx_set_terrain(x, 4, "Ocean");
    if (x != 5) {
      fx_set_terrain(x, 6, "Ocean");
    }
  }
  explorer = fx_unit(us, "Explorer", 2, 5, 0);
  fx_war(us, them);
  fx_unit(them, "Warriors", 5, 6, 0);
  fx_show_all(us);

  pft_fill_unit_parameter(&parameter, &(wld.map), explorer);
  adv_avoid_risks(&parameter, &risk_cost, explorer,
                  NORMAL_STACKING_FEARFULNESS);
  pfm = pf_map_new(&parameter);
  path = pf_map_path(pfm, fx_tile(8, 5));
  cr_assert_not_null(path);
  step = path->positions[1].tile;
  pf_path_destroy(path);
  pf_map_destroy(pfm);

  return step;
}

/* The ZOC cannot stop an Explorer, so its path takes the corridor. */
Test(advgoto, igzoc_path_ignores_enemy_zoc)
{
  struct tile *step = explorer_first_step_past_zoc();

  cr_expect(same_pos(step, fx_tile(3, 5)), "the Explorer steps to (%d, %d)",
            TILE_XY(step));
}
