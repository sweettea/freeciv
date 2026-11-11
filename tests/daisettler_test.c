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

/* common */
#include "ai.h"
#include "extras.h"
#include "research.h"

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
