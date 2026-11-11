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

/* Tests of ai/default/daisettler.c, static functions included.
 * See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

/* common */
#include "ai.h"
#include "extras.h"
#include "research.h"
#include "specialist.h"

/* server */
#include "aiiface.h"

/* ai/default */
#include "daisettler.c"

#include "fixture.h"

TestSuite(daisettler, .init = fx_init_classic, .fini = fx_fini);

/* Add the extra `rule_name` to the tile at (x, y). */
static void add_extra(int x, int y, const char *rule_name)
{
  struct extra_type *pextra = extra_type_by_rule_name(rule_name);

  cr_assert_not_null(pextra, "no extra \"%s\"", rule_name);
  tile_add_extra(fx_tile(x, y), pextra);
}

/* A settler standing on the best site of an even grassland, the only
 * river tile, picks the tile it stands on. */
Test(daisettler, settler_considers_own_tile)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = fx_ai_player(nullptr);
  struct unit *punit;
  struct cityresult *cr;

  add_extra(10, 10, "River");
  punit = fx_unit(pplayer, "Settlers", 10, 10, 0);
  fx_show_all(pplayer);
  fx_phase_begin();
  citymap_turn_init(pplayer);

  cr = find_best_city_placement(ait, punit, FALSE, FALSE);
  cr_assert_not_null(cr);
  cr_expect(same_pos(cr->tile, unit_tile(punit)),
            "picked (%d, %d), result " ADV_WANT_PRINTF
            ", standing on (%d, %d)",
            TILE_XY(cr->tile), cr->result, TILE_XY(unit_tile(punit)));
  cityresult_destroy(cr);
}

/* The value cityresult_fill() adds for a river on the centre: the
 * total of a river site minus that of a dry one, both on grassland. */
static adv_want river_gain(struct ai_type *ait, struct player *pplayer)
{
  struct cityresult *wet, *dry;
  adv_want gain;

  dai_auto_settler_reset(ait, pplayer);
  wet = cityresult_fill(ait, pplayer, fx_tile(10, 10));
  dry = cityresult_fill(ait, pplayer, fx_tile(4, 4));
  cr_assert_not_null(wet);
  cr_assert_not_null(dry);
  gain = wet->total - dry->total;
  cityresult_destroy(wet);
  cityresult_destroy(dry);

  return gain;
}

/* Start a new phase for `pplayer`'s advisor twice, so the priorities,
 * which are set from the previous phase's wants_science, follow it. */
static void adv_phase_again(struct player *pplayer)
{
  int i;

  for (i = 0; i < 2; i++) {
    adv_data_phase_done(pplayer);
    adv_data_phase_init(pplayer, TRUE);
  }
}

/* When science is not wanted (researching a future tech), trade still
 * becomes gold: the river on a site keeps at least the share of its
 * value that gold has against science. */
Test(daisettler, trade_valued_without_science)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = fx_ai_player(nullptr);
  struct adv_data *adv;
  adv_want with_science, without_science, sci_priority;

  add_extra(10, 10, "River");
  fx_show_all(pplayer);
  fx_phase_begin();
  citymap_turn_init(pplayer);
  adv = adv_data_get(pplayer, nullptr);
  cr_assert(adv->wants_science);
  sci_priority = adv->science_priority;
  with_science = river_gain(ait, pplayer);

  research_get(pplayer)->researching = A_FUTURE;
  adv_phase_again(pplayer);
  cr_assert(!adv->wants_science);
  without_science = river_gain(ait, pplayer);

  cr_expect_gt(with_science, 0, "gain " ADV_WANT_PRINTF, with_science);
  cr_expect_geq(without_science,
                with_science * adv->gold_priority / sci_priority,
                "river worth " ADV_WANT_PRINTF " without science, "
                ADV_WANT_PRINTF " with (science %g, gold %g)",
                without_science, with_science,
                (double) sci_priority, (double) adv->gold_priority);
}

/* A site three tiles from our city, legal under citymindist 2, is
 * still a site when the city works the tile between them. */
Test(daisettler, site_next_to_worked_tile)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 5, 5, "Home");
  struct unit *punit = fx_unit(pplayer, "Settlers", 12, 5, 0);
  struct cityresult *cr;

  cr_assert_eq(game.info.citymindist, 2);
  fx_show_all(pplayer);
  fx_phase_begin();

  /* Its one citizen works (7, 5). */
  city_tile_iterate(&(wld.map), city_map_radius_sq_get(pcity),
                    city_tile(pcity), ptile) {
    if (tile_worked(ptile) == pcity && ptile != city_tile(pcity)) {
      tile_set_worked(ptile, nullptr);
    }
  } city_tile_iterate_end;
  specialist_type_iterate(sp) {
    pcity->specialists[sp] = 0;
  } specialist_type_iterate_end;
  tile_set_worked(fx_tile(7, 5), pcity);
  citymap_turn_init(pplayer);

  cr_assert(city_can_be_built_here(&(wld.map), fx_tile(8, 5), punit,
                                   FALSE));
  cr = city_desirability(ait, pplayer, punit, fx_tile(8, 5));
  cr_expect_not_null(cr, "site (8, 5) refused");
  if (cr != nullptr) {
    cityresult_destroy(cr);
  }
  tile_set_worked(fx_tile(7, 5), nullptr);
}

/* A site within citymindist of another founder's target spot is not a
 * site. */
Test(daisettler, site_next_to_founder_target)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = fx_ai_player(nullptr);
  struct unit *other = fx_unit(pplayer, "Settlers", 7, 6, 0);
  struct unit *punit = fx_unit(pplayer, "Settlers", 12, 5, 0);
  struct cityresult *cr;

  cr_assert_eq(game.info.citymindist, 2);
  fx_show_all(pplayer);
  fx_phase_begin();
  citymap_turn_init(pplayer);
  adv_unit_new_task(other, AUT_BUILD_CITY, fx_tile(7, 5));
  cr_assert(same_pos(other->goto_tile, fx_tile(7, 5)));

  cr = city_desirability(ait, pplayer, punit, fx_tile(8, 5));
  cr_expect_null(cr, "site (8, 5) taken beside a founder's target");
  if (cr != nullptr) {
    cityresult_destroy(cr);
  }
}

struct ruleset_name {
  char name[32];
};

ParameterizedTestParameters(daisettler_rs, search_horizon)
{
  /* Not granularity: its Settlers have 50 moves a turn, so they search
   * 150 turns out, the whole map. */
  static struct ruleset_name params[] = {
    { "classic" }, { "civ2civ3" }, { "sandbox" }, { "goldkeep" },
  };

  return cr_make_param_array(struct ruleset_name, params,
                             sizeof(params) / sizeof(params[0]));
}

/* A one-move settler on open grassland, where a tile is a turn, finds
 * a good enough site at once and stops rating sites at the first one
 * past three turns out, whatever the ruleset's move fragments. The
 * farthest site rated is the farthest tile in the settler's tile cache
 * less the city radius. */
ParameterizedTest(struct ruleset_name *p, daisettler_rs, search_horizon,
                  .fini = fx_fini)
{
  struct ai_type *ait;
  struct player *pplayer;
  struct unit *punit;
  struct cityresult *cr;
  int radius = 0, farthest = 0;

  fx_init(p->name);
  ait = ai_type_by_name(default_ai_type_name());
  pplayer = fx_ai_player(nullptr);
  punit = fx_unit(pplayer, "Settlers", 0, 0, 0);
  cr_assert_eq(unit_move_rate(punit),
               fx_terrain("Grassland")->movement_cost * SINGLE_MOVE,
               "%s Settlers do not move one Grassland tile a turn",
               p->name);
  while ((radius + 1) * (radius + 1) <= game.info.init_city_radius_sq) {
    radius++;
  }
  fx_show_all(pplayer);
  fx_phase_begin();
  citymap_turn_init(pplayer);

  cr = find_best_city_placement(ait, punit, FALSE, FALSE);
  cr_assert_not_null(cr);
  cr_assert_gt(cr->result, RESULT_IS_ENOUGH);
  cityresult_destroy(cr);

  whole_map_iterate(&(wld.map), ptile) {
    if (tdc_plr_get(ait, pplayer, tile_index(ptile)) != nullptr) {
      farthest = MAX(farthest, real_map_distance(ptile, unit_tile(punit)));
    }
  } whole_map_iterate_end;
  cr_expect_eq(farthest - radius, 4, "%s rated sites out to %d turns",
               p->name, farthest - radius);
}
