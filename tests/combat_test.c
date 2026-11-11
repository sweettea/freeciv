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

/* Orderings the combat and action code must keep on real game objects:
 * a loaded ruleset, units and cities placed by the fixture. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

/* common */
#include "actions.h"
#include "city.h"
#include "combat.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "terrain.h"
#include "tile.h"
#include "unit.h"
#include "unittype.h"

#include "fixture.h"

/* Parameters are copied into each test's worker process, so they hold
 * names, not pointers. */
struct ruleset_unit {
  char ruleset[16];
  char utype[32];
};

/**********************************************************************//**
  Whether any conquer-city action is enabled for `punit` against
  `pcity` under the loaded ruleset's enablers.
**************************************************************************/
static bool can_conquer(const struct unit *punit, const struct city *pcity)
{
  action_by_result_iterate(paction, ACTRES_CONQUER_CITY) {
    if (is_action_enabled_unit_on_city(&(wld.map), action_id(paction),
                                       punit, pcity)) {
      return TRUE;
    }
  } action_by_result_iterate_end;

  return FALSE;
}

ParameterizedTestParameters(combat, empty_city_conquerable)
{
  static struct ruleset_unit params[] = {
    { "civ2civ3", "Musketeers" }, { "civ2civ3", "Warriors" },
    { "civ2civ3", "Riflemen" },   { "classic", "Musketeers" },
    { "classic", "Warriors" },    { "classic", "Riflemen" },
  };

  return cr_make_param_array(struct ruleset_unit, params,
                             sizeof(params) / sizeof(params[0]));
}

/**********************************************************************//**
  A land unit with moves left beside an empty enemy city it is at war
  with finds no defender there, and the ruleset lets it walk in.
**************************************************************************/
ParameterizedTest(struct ruleset_unit *p, combat, empty_city_conquerable,
                  .fini = fx_fini)
{
  struct player *us, *them;
  struct city *pcity;
  struct unit *punit;

  fx_init(p->ruleset);
  us = fx_player();
  them = fx_player();
  fx_war(us, them);
  pcity = fx_city(them, 5, 5, "Target");
  punit = fx_unit(us, p->utype, 6, 5, 0);
  fx_show_all(us);

  cr_assert(punit->moves_left > 0, "%s has no moves", p->utype);
  cr_expect_null(get_defender(&(wld.map), punit, city_tile(pcity),
                              nullptr),
                 "the empty city has a defender");
  cr_expect(can_conquer(punit, pcity),
            "%s may not conquer an empty enemy city in %s", p->utype,
            p->ruleset);
}

struct ruleset_site {
  char ruleset[16];
  bool in_city;
};

ParameterizedTestParameters(combat, defence_monotone_in_terrain)
{
  static struct ruleset_site params[] = {
    { "civ2civ3", FALSE }, { "civ2civ3", TRUE },
    { "classic", FALSE },  { "classic", TRUE },
  };

  return cr_make_param_array(struct ruleset_site, params,
                             sizeof(params) / sizeof(params[0]));
}

/**********************************************************************//**
  A Musketeers' defence against a Legion on each land terrain the
  ruleset has, alone or in a city: ordering the terrains by their
  defense_bonus must order the defence power the same way (ties
  allowed).
**************************************************************************/
ParameterizedTest(struct ruleset_site *p, combat,
                  defence_monotone_in_terrain, .fini = fx_fini)
{
  const struct unit_type *def, *att;
  struct player *owner;
  struct tile *ptile;
  int bonus[MAX_NUM_TERRAINS], power[MAX_NUM_TERRAINS];
  const char *name[MAX_NUM_TERRAINS];
  int n = 0, i, j;

  fx_init(p->ruleset);
  def = fx_utype("Musketeers");
  att = fx_utype("Legion");
  owner = fx_player();
  ptile = fx_tile(5, 5);
  if (p->in_city) {
    fx_city(owner, 5, 5, "Site");
  }

  terrain_type_iterate(pterrain) {
    if (terrain_type_terrain_class(pterrain) != TC_LAND
        || terrain_has_flag(pterrain, TER_NO_CITIES)) {
      continue;
    }
    tile_set_terrain(ptile, pterrain);
    if (!can_exist_at_tile(&(wld.map), def, ptile)) {
      continue;
    }
    bonus[n] = pterrain->defense_bonus;
    power[n] = get_virtual_defense_power(&(wld.map), att, def, owner,
                                         ptile, FALSE, 0);
    name[n] = terrain_rule_name(pterrain);
    n++;
  } terrain_type_iterate_end;

  cr_assert_geq(n, 3, "only %d land terrains to compare", n);
  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      if (bonus[i] < bonus[j]) {
        cr_expect_leq(power[i], power[j],
                      "%s%s: %s (+%d%%) defends at %d, %s (+%d%%) at %d",
                      p->ruleset, p->in_city ? " city" : "", name[i],
                      bonus[i], power[i], name[j], bonus[j], power[j]);
      }
    }
  }
}
