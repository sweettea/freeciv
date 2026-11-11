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

/* Tests of static functions in server/advisors/autoexplorer.c.
 *
 * The file under test is compiled into this test, so its static
 * functions can be called by name. See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* server */
#include "maphand.h"

/* server/advisors */
#include "autoexplorer.c"

#include "fixture.h"

TestSuite(autoexplorer, .init = fx_init_classic, .fini = fx_fini);

/* An unknown tile is guessed from what the player knows: with all
 * eight neighbours known Grassland, a land unit thinks it almost
 * certainly native, whatever the tile really is. */
Test(autoexplorer, likely_native_reads_the_known_neighbours)
{
  struct player *pplayer = fx_player();
  struct unit *punit = fx_unit(pplayer, "Explorer", 2, 2, 0);
  struct tile *hidden = fx_tile(5, 5);
  int native;

  fx_set_terrain(5, 5, "Ocean");
  fx_show_all(pplayer);
  map_clear_known(hidden, pplayer);

  native = likely_native(hidden, pplayer, unit_class_get(punit));
  cr_expect_eq(native, 50 + 50 / MAP_NUM_VALID_DIRS * MAP_NUM_VALID_DIRS,
               "likely_native() = %d", native);
}

/* An AI explorer wants a hut on a tile its player knows, whatever its
 * hut handicap: H_HUTS hides only huts in unexplored territory. The
 * default skill level, easy, has H_HUTS. The whole map is known, so
 * the hut is the tile's only value. */
Test(autoexplorer, ai_explorer_wants_known_hut)
{
  struct player *pplayer = fx_ai_player(nullptr);
  struct unit *explorer = fx_unit(pplayer, "Explorer", 2, 5, 0);
  struct tile *hut = fx_tile(4, 5);
  int desirable;

  tile_add_extra(hut, extra_type_by_rule_name("Hut"));
  fx_show_all(pplayer);
  cr_assert(has_handicap(pplayer, H_HUTS));

  desirable = explorer_desirable(hut, pplayer, explorer);
  cr_expect_gt(desirable, 0, "known hut desirable %d", desirable);
}

/* Where an Explorer of `pplayer` at (2, 5) with moves for one step goes
 * on its way to (8, 5). A Grassland corridor runs along y = 5 with
 * Ocean either side for x = 3..7, and a longer land route along
 * y = 3. Another player's Warriors, never seen by `pplayer`, blocks
 * the corridor at (5, 5). */
static struct tile *explorer_step_past_unseen_unit(struct player *pplayer)
{
  struct player *other = fx_ai_player(nullptr);
  struct unit *explorer;
  int x;

  for (x = 3; x <= 7; x++) {
    fx_set_terrain(x, 4, "Ocean");
    fx_set_terrain(x, 6, "Ocean");
  }
  explorer = fx_unit(pplayer, "Explorer", 2, 5, 0);
  fx_unit(other, "Warriors", 5, 5, 0);
  fx_show_all(pplayer);
  cr_assert(!map_is_known_and_seen(fx_tile(5, 5), pplayer, V_MAIN));
  explorer->moves_left = terrain_control.igter_cost;

  cr_assert(explorer_goto(explorer, fx_tile(8, 5)));

  return unit_tile(explorer);
}

/* A human's explorer cannot route round a unit its owner has not seen:
 * it takes the corridor. */
Test(autoexplorer, human_explorer_ignores_unseen_units)
{
  struct tile *step = explorer_step_past_unseen_unit(fx_player());

  cr_expect(same_pos(step, fx_tile(3, 5)),
            "the human's Explorer stepped to (%d, %d)", TILE_XY(step));
}

/* An AI explorer still routes round it. */
Test(autoexplorer, ai_explorer_avoids_unseen_units)
{
  struct tile *step = explorer_step_past_unseen_unit(fx_ai_player(nullptr));

  cr_expect(same_pos(step, fx_tile(2, 4)),
            "the AI's Explorer stepped to (%d, %d)", TILE_XY(step));
}
