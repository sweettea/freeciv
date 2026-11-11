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

/* Tests of the unit actions in server/unithand.c, driven through
 * unit_perform_action() as a client's request reaches them. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

/* common */
#include "actions.h"
#include "citizens.h"
#include "city.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "specialist.h"
#include "unit.h"
#include "unittype.h"

/* server */
#include "citytools.h"
#include "unithand.h"

/* server/generator */
#include "mapgen_utils.h"

#include "fixture.h"

/* A founder of type `utype` in ruleset `ruleset`, standing on
 * `terrain`. `specialist`, if set, is given to the unit type first, as
 * a ruleset's "specialist" field would. */
struct found_case {
  char ruleset[32];
  char utype[32];
  char terrain[32];
  char specialist[32];
};

static struct found_case found_cases[] = {
  /* city_size = 2, no specialist. */
  { "alien", "Water Engineer", "Savannah", "" },
  /* city_size = 1, no specialist. */
  { "classic", "Settlers", "Grassland", "" },
  /* city_size = 2 and a normal specialist, which only Join City uses. */
  { "alien", "Water Engineer", "Savannah", "Scientists" },
};

ParameterizedTestParameters(unithand, found_city_size)
{
  return cr_make_param_array(struct found_case, found_cases,
                             ARRAY_SIZE(found_cases));
}

/* Found City makes a city of the founder type's city_size, its citizens
 * all accounted for: working a tile or a specialist, and of the
 * founder's nationality. */
ParameterizedTest(struct found_case *p, unithand, found_city_size,
                  .fini = fx_fini)
{
  struct unit_type *ptype;
  struct player *us;
  struct unit *founder;
  struct tile *ptile;
  struct city *pcity;
  int workers = 0;

  fx_init(p->ruleset);
  ptype = unit_type_by_rule_name(p->utype);
  cr_assert_not_null(ptype);
  if (p->specialist[0] != '\0') {
    ptype->spec_type = specialist_by_rule_name(p->specialist);
    cr_assert_not_null(ptype->spec_type);
  }
  whole_map_iterate(&(wld.map), wtile) {
    tile_set_terrain(wtile, fx_terrain(p->terrain));
  } whole_map_iterate_end;
  assign_continent_numbers();

  us = fx_player();
  founder = fx_unit(us, p->utype, 10, 10, 0);
  fx_show_all(us);
  fx_unit_vision(founder);
  ptile = unit_tile(founder);

  cr_assert(unit_perform_action(us, founder->id, tile_index(ptile), 0,
                                city_name_suggestion(us, ptile),
                                ACTION_FOUND_CITY, ACT_REQ_PLAYER),
            "%s could not found a city", p->utype);
  pcity = tile_city(ptile);
  cr_assert_not_null(pcity);

  city_tile_iterate_skip_free_worked(&(wld.map),
                                     city_map_radius_sq_get(pcity),
                                     city_tile(pcity), wtile,
                                     _index, _x, _y) {
    if (tile_worked(wtile) == pcity) {
      workers++;
    }
  } city_tile_iterate_skip_free_worked_end;

  cr_log_info("%s %s (specialist \"%s\"): size %d, %d workers,"
              " %d specialists, %d citizens",
              p->ruleset, p->utype, p->specialist, city_size_get(pcity),
              workers,
              city_specialists(pcity), citizens_count(pcity));
  cr_expect_eq(city_size_get(pcity), ptype->city_size,
               "%s (city_size %d) founded a city of size %d",
               p->utype, ptype->city_size, city_size_get(pcity));
  cr_expect_eq(workers + city_specialists(pcity), city_size_get(pcity),
               "%d workers and %d specialists in a city of size %d",
               workers, city_specialists(pcity), city_size_get(pcity));
  cr_expect_eq(citizens_count(pcity), city_size_get(pcity),
               "%d citizens by nationality in a city of size %d",
               citizens_count(pcity), city_size_get(pcity));
  if (game.info.citizen_nationality) {
    cr_expect_eq(citizens_nation_get(pcity, us->slot),
                 city_size_get(pcity));
  }
}
