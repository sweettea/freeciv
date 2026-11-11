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
#include <criterion/parameterized.h>

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
 * Ocean either side for x = 3..7. Another player's Explorer, never seen
 * by `pplayer`, blocks the corridor at (5, 5); it has no ZOC. With
 * `two_blockers` a second one stands at (4, 5).
 *
 * The other land routes run along y = 3 and y = 7, two steps longer,
 * or with `far_detour` along y = 2 and y = 8, four steps longer. An IgTer step is a
 * third of a move, so only the far detour costs more than the turn's
 * worth of extra cost path finding allows a tile. */
static struct tile *explorer_step_past_unseen_unit(struct player *pplayer,
                                                   bool far_detour,
                                                   bool two_blockers)
{
  struct player *other = fx_ai_player(nullptr);
  struct unit *explorer;
  int x;

  for (x = 3; x <= 7; x++) {
    fx_set_terrain(x, 4, "Ocean");
    fx_set_terrain(x, 6, "Ocean");
    if (far_detour) {
      fx_set_terrain(x, 3, "Ocean");
      fx_set_terrain(x, 7, "Ocean");
    }
  }
  explorer = fx_unit(pplayer, "Explorer", 2, 5, 0);
  fx_unit(other, "Explorer", 5, 5, 0);
  if (two_blockers) {
    fx_unit(other, "Explorer", 4, 5, 0);
  }
  fx_show_all(pplayer);
  cr_assert(!map_is_known_and_seen(fx_tile(5, 5), pplayer, V_MAIN));
  explorer->moves_left = terrain_control.igter_cost;

  cr_assert(explorer_goto(explorer, fx_tile(8, 5)));

  return unit_tile(explorer);
}

/* A human's explorer is not barred from a tile by a unit its owner has
 * not seen: it takes the corridor rather than a far detour. */
Test(autoexplorer, human_explorer_ignores_unseen_units)
{
  struct tile *step = explorer_step_past_unseen_unit(fx_player(), TRUE,
                                                     FALSE);

  cr_expect(same_pos(step, fx_tile(3, 5)),
            "the human's Explorer stepped to (%d, %d)", TILE_XY(step));
}

/* Nor do such units make the corridor look costly to the human's
 * explorer: with two of them in it and a detour only two thirds of a
 * turn longer, it still takes the corridor. */
Test(autoexplorer, human_explorer_risk_ignores_unseen_units)
{
  struct tile *step = explorer_step_past_unseen_unit(fx_player(), FALSE,
                                                     TRUE);

  cr_expect(same_pos(step, fx_tile(3, 5)),
            "the human's Explorer stepped to (%d, %d)", TILE_XY(step));
}

/* An AI explorer still routes round it, however far. */
Test(autoexplorer, ai_explorer_avoids_unseen_units)
{
  struct tile *step = explorer_step_past_unseen_unit(fx_ai_player(nullptr),
                                                     TRUE, FALSE);

  cr_expect(!same_pos(step, fx_tile(3, 5)),
            "the AI's Explorer took the corridor");
}

struct explore_ruleset {
  char ruleset[32];
};

ParameterizedTestParameters(autoexplorer_rs, discount_per_move)
{
  static struct explore_ruleset params[] = {
    { "classic" }, { "civ2civ3" }, { "sandbox" },
  };

  return cr_make_param_array(struct explore_ruleset, params,
                             sizeof(params) / sizeof(params[0]));
}

/* A human Explorer at (2, 10) with no moves left chooses between one
 * unknown tile at (6, 10) and an unknown pair at (2, 15), (3, 15), one
 * step farther. Its vision is set to radius_sq 2 in every ruleset, so
 * the rulesets differ only in how a move is split into fragments; each
 * step costs a third of a move in all three. The distance discount is
 * per move, so the pair, worth more than twice the single tile, wins
 * everywhere. */
ParameterizedTest(struct explore_ruleset *p, autoexplorer_rs,
                  discount_per_move, .fini = fx_fini)
{
  struct player *human;
  struct unit *explorer;
  struct tile *target;

  fx_init(p->ruleset);
  human = fx_player();
  ((struct unit_type *) fx_utype("Explorer"))->vision_radius_sq = 2;
  explorer = fx_unit(human, "Explorer", 2, 10, 0);
  fx_show_all(human);
  map_clear_known(fx_tile(6, 10), human);
  map_clear_known(fx_tile(2, 15), human);
  map_clear_known(fx_tile(3, 15), human);
  explorer->moves_left = 0;

  cr_assert_eq(manage_auto_explorer(explorer), MR_OK);
  target = explorer->goto_tile;
  cr_assert_not_null(target);
  cr_expect_geq(index_to_map_pos_y(tile_index(target)), 14,
                "%s: the Explorer heads for (%d, %d), not the pair",
                p->ruleset, TILE_XY(target));
  cr_log_info("%s: target (%d, %d)", p->ruleset, TILE_XY(target));
}
