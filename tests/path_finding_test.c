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

/* Tests of common/aicore/path_finding.c through its public API. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

/* common */
#include "map.h"
#include "unit.h"

/* common/aicore */
#include "path_finding.h"
#include "pf_tools.h"

#include "fixture.h"

TestSuite(path_finding, .init = fx_init_classic, .fini = fx_fini);

/* Two land routes from (2, 5) to (8, 5): a corridor along y = 5 with
 * Ocean either side for x = 3..7, and routes two tiles longer along
 * y = 3 and y = 7. */
static void two_routes(void)
{
  int x;

  for (x = 3; x <= 7; x++) {
    fx_set_terrain(x, 4, "Ocean");
    fx_set_terrain(x, 6, "Ocean");
  }
}

static unsigned corridor_ec;

/* Extra cost `corridor_ec` on the corridor's middle tile. */
static unsigned corridor_cost(const struct tile *ptile, enum known_type known,
                              const struct pf_parameter *param)
{
  return same_pos(ptile, fx_tile(5, 5)) ? corridor_ec : 0;
}

/* The tile an Explorer's path from (2, 5) to (8, 5) takes first. Its
 * steps are a third of a move (IgTer), so the longer routes cost two
 * thirds of a turn more. */
static struct tile *first_step(unsigned ec)
{
  struct player *pplayer = fx_player();
  struct unit *punit = fx_unit(pplayer, "Explorer", 2, 5, 0);
  struct pf_parameter parameter;
  struct pf_map *pfm;
  struct pf_path *path;
  struct tile *step;

  two_routes();
  fx_show_all(pplayer);
  pft_fill_unit_parameter(&parameter, &(wld.map), punit);
  corridor_ec = ec;
  parameter.get_EC = corridor_cost;

  pfm = pf_map_new(&parameter);
  path = pf_map_path(pfm, fx_tile(8, 5));
  cr_assert_not_null(path);
  step = path->positions[1].tile;
  pf_path_destroy(path);
  pf_map_destroy(pfm);

  return step;
}

/* A small extra cost on the corridor does not outweigh two extra
 * steps. */
Test(path_finding, small_extra_cost_keeps_short_path)
{
  struct tile *step = first_step(1000);

  cr_expect(same_pos(step, fx_tile(3, 5)), "first step (%d, %d)",
            TILE_XY(step));
}

/* An extra cost of twenty turns on one corridor tile sends the path
 * round the longer route. 20 * PF_TURN_FACTOR does not fit the 16 bits
 * a node keeps; held at their maximum, just under a turn, it still
 * outweighs two thirds of a turn. */
Test(path_finding, large_extra_cost_changes_path)
{
  struct tile *step = first_step(20 * PF_TURN_FACTOR);

  cr_expect(!same_pos(step, fx_tile(3, 5)), "first step (%d, %d)",
            TILE_XY(step));
}

static unsigned spot_ec;

/* Extra cost `spot_ec` on (5, 5), 1000 on every other tile. */
static unsigned spot_cost(const struct tile *ptile, enum known_type known,
                          const struct pf_parameter *param)
{
  return same_pos(ptile, fx_tile(5, 5)) ? spot_ec : 1000;
}

struct pf_ruleset {
  char ruleset[32];
};

ParameterizedTestParameters(path_finding_rs, large_extra_cost_avoids_tile)
{
  static struct pf_ruleset params[] = {
    { "classic" }, { "granularity" },
  };

  return cr_make_param_array(struct pf_ruleset, params,
                             sizeof(params) / sizeof(params[0]));
}

/* On open Grassland a Warriors goes from (2, 5) to (8, 5). Every tile
 * has a small extra cost and (5, 5) one of twenty turns, so the path
 * goes round (5, 5) at no extra length. In granularity a move is
 * 3000 fragments, so this also checks that a large extra cost does not
 * overflow the combined cost and make (5, 5) look cheap. */
ParameterizedTest(struct pf_ruleset *p, path_finding_rs,
                  large_extra_cost_avoids_tile, .fini = fx_fini)
{
  struct player *pplayer;
  struct unit *punit;
  struct pf_parameter parameter;
  struct pf_map *pfm;
  struct pf_path *path;
  int i;

  fx_init(p->ruleset);
  pplayer = fx_player();
  punit = fx_unit(pplayer, "Warriors", 2, 5, 0);
  fx_show_all(pplayer);
  pft_fill_unit_parameter(&parameter, &(wld.map), punit);
  spot_ec = 20 * PF_TURN_FACTOR;
  parameter.get_EC = spot_cost;

  pfm = pf_map_new(&parameter);
  path = pf_map_path(pfm, fx_tile(8, 5));
  cr_assert_not_null(path);
  cr_expect_eq(path->length, 7, "%s: path of %d positions", p->ruleset,
               path->length);
  for (i = 0; i < path->length; i++) {
    cr_expect(!same_pos(path->positions[i].tile, fx_tile(5, 5)),
              "%s: the path goes through (5, 5)", p->ruleset);
  }
  pf_path_destroy(path);
  pf_map_destroy(pfm);
}
