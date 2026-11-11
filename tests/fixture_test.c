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

/* Checks of the fixture itself: each ruleset loads, the map is the
 * advertised size and terrain, and the helpers put real-looking objects
 * where they were asked to. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

/* common */
#include "city.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "nation.h"
#include "player.h"
#include "research.h"
#include "tech.h"
#include "terrain.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

#include "fixture.h"

/* Parameters are copied into each test's worker process, so they hold
 * names, not pointers. */
struct ruleset_name {
  char name[16];
};

/**********************************************************************//**
  What every ruleset's fixture must provide.
**************************************************************************/
static void check_fixture(const char *ruleset)
{
  struct player *a, *b;
  struct city *pcity;
  struct unit *punit;

  cr_log_info("fixture setup (%s): %.3f s", ruleset, fx_setup_seconds());
  cr_expect_str_eq(game.server.rulesetdir, ruleset);
  cr_expect_eq(map_num_tiles(), FX_MAP_XSIZE * FX_MAP_YSIZE);
  cr_expect(terrain_count() > 1, "only %d terrain(s) loaded",
            terrain_count());
  cr_expect(utype_count() > 10, "only %d unit type(s) loaded",
            utype_count());
  cr_expect_eq(tile_terrain(fx_tile(0, 0)), fx_terrain("Grassland"));
  cr_expect(tile_continent(fx_tile(5, 5)) > 0,
            "land tiles were not given a continent");

  a = fx_player();
  b = fx_player();
  cr_expect_neq(nation_of_player(a), nation_of_player(b));
  fx_war(a, b);
  cr_expect_eq(player_diplstate_get(a, b)->type, DS_WAR);
  cr_expect_eq(player_diplstate_get(b, a)->type, DS_WAR);

  pcity = fx_city(b, 5, 5, "Target");
  cr_expect_eq(tile_city(fx_tile(5, 5)), pcity);
  cr_expect_eq(game_city_by_number(pcity->id), pcity);
  cr_expect_eq(city_list_size(b->cities), 1);
  cr_expect_eq(city_size_get(pcity), 1);

  punit = fx_unit(a, "Musketeers", 6, 5, 0);
  cr_expect_eq(unit_tile(punit), fx_tile(6, 5));
  cr_expect_eq(unit_list_size(fx_tile(6, 5)->units), 1);
  cr_expect_eq(punit->hp, unit_type_get(punit)->hp);
  cr_expect_eq(game_unit_by_number(punit->id), punit);

  fx_set_terrain(3, 3, "Hills");
  cr_expect_eq(tile_terrain(fx_tile(3, 3)), fx_terrain("Hills"));
}

Test(fixture, classic, .init = fx_init_classic, .fini = fx_fini)
{
  check_fixture("classic");
}

Test(fixture, civ2civ3, .init = fx_init_civ2civ3, .fini = fx_fini)
{
  check_fixture("civ2civ3");
}

Test(fixture, sandbox, .init = fx_init_sandbox, .fini = fx_fini)
{
  check_fixture("sandbox");
}

ParameterizedTestParameters(fixture, any_ruleset)
{
  static struct ruleset_name params[] = {
    { "alien" }, { "civ1" }, { "civ2" }, { "civ2civ3" }, { "classic" },
    { "goldkeep" }, { "granularity" }, { "multiplayer" }, { "sandbox" },
    { "stub" },
  };

  return cr_make_param_array(struct ruleset_name, params,
                             sizeof(params) / sizeof(params[0]));
}

/**********************************************************************//**
  Every in-tree ruleset loads by name and gives a map, a player and a
  city on it. Unit and terrain names differ between rulesets, so only
  the first unit type and land terrain are used.
**************************************************************************/
ParameterizedTest(struct ruleset_name *p, fixture, any_ruleset,
                  .fini = fx_fini)
{
  struct player *pplayer;
  struct city *pcity;

  fx_init(p->name);
  cr_log_info("fixture setup (%s): %.3f s", p->name, fx_setup_seconds());
  cr_expect_str_eq(game.server.rulesetdir, p->name);
  cr_expect_eq(map_num_tiles(), FX_MAP_XSIZE * FX_MAP_YSIZE);
  cr_expect_eq(terrain_type_terrain_class(tile_terrain(fx_tile(0, 0))),
               TC_LAND);

  pplayer = fx_player();
  pcity = fx_city(pplayer, 5, 5, "Capital");
  cr_expect_eq(tile_city(fx_tile(5, 5)), pcity);
  cr_expect_eq(city_list_size(pplayer->cities), 1);
  fx_unit(pplayer, utype_rule_name(utype_by_number(0)), 5, 5, 0);
  cr_expect_eq(unit_list_size(fx_tile(5, 5)->units), 1);
}

/* Diplomatic states, techs and governments set through the helpers
 * read back through the common accessors. */
Test(fixture, player_state, .init = fx_init_classic, .fini = fx_fini)
{
  struct player *a = fx_player();
  struct player *b = fx_player();
  struct player *c = fx_player();
  Tech_type_id monarchy = advance_number(advance_by_rule_name("Monarchy"));

  fx_diplstate(a, b, DS_ALLIANCE);
  cr_expect(pplayers_allied(a, b));
  cr_expect(pplayers_allied(b, a));
  cr_expect(player_diplstate_get(a, b)->contact_turns_left > 0);
  fx_diplstate(a, c, DS_ARMISTICE);
  cr_expect_eq(player_diplstate_get(c, a)->type, DS_ARMISTICE);
  cr_expect_eq(player_diplstate_get(c, a)->turns_left, 16);
  cr_expect(!pplayers_at_war(a, c));
  fx_war(b, c);
  cr_expect(pplayers_at_war(c, b));

  cr_expect_neq(research_invention_state(research_get(a), monarchy),
                TECH_KNOWN);
  fx_tech(a, "Ceremonial Burial");
  fx_tech(a, "Alphabet");
  fx_tech(a, "Code of Laws");
  fx_tech(a, "Monarchy");
  cr_expect_eq(research_invention_state(research_get(a), monarchy),
               TECH_KNOWN);
  cr_expect_neq(research_invention_state(research_get(b), monarchy),
                TECH_KNOWN);

  fx_city(a, 5, 5, "Capital");
  fx_government(a, "Monarchy");
  cr_expect_str_eq(government_rule_name(government_of_player(a)),
                   "Monarchy");
  cr_expect(can_change_to_government(a, government_of_player(a)));
}
