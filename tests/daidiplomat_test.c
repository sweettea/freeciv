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

/* Tests of ai/default/daidiplomat.c.
 *
 * The file under test is compiled into this test, so its static
 * functions can be called by name. See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "ai.h"

/* server */
#include "aiiface.h"

/* ai/default */
#include "daidiplomat.c"

#include "fixture.h"

TestSuite(daidiplomat, .init = fx_init_classic, .fini = fx_fini);

/* Run dai_diplomat_bribe_nearby() for `dipl` on the map
 * dai_manage_diplomat() builds for it. */
static bool bribe_nearby(struct ai_type *ait, struct unit *dipl)
{
  struct player *pplayer = unit_owner(dipl);
  struct pf_parameter parameter;
  struct pf_map *pfm;
  bool alive;

  pft_fill_unit_parameter(&parameter, &(wld.map), dipl);
  parameter.omniscience = !has_handicap(pplayer, H_MAP);
  parameter.get_zoc = nullptr;
  parameter.get_TB = no_intermediate_fights;
  pfm = pf_map_new(&parameter);
  alive = dai_diplomat_bribe_nearby(ait, pplayer, dipl, pfm);
  pf_map_destroy(pfm);

  return alive;
}

/* A Diplomat with gold to spare bribes a lone enemy Warriors `dist`
 * tiles away, in sight and within reach this turn. */
static void bribes_at(int dist)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *a = fx_ai_player(nullptr);
  struct player *b = fx_ai_player(nullptr);
  struct unit *dipl;

  fx_war(a, b);
  dipl = fx_unit(a, "Diplomat", 5, 5, 0);
  fx_unit(b, "Warriors", 5 + dist, 5, 0);
  fx_show_all(a);
  fx_unit_vision(dipl);
  /* A Diplomat sees one tile; let another unit spot the target. */
  fx_unit_vision(fx_unit(a, "Explorer", 5 + dist, 6, 0));
  fx_phase_begin();
  a->economic.gold = 1000;
  cr_assert_eq(dipl->moves_left, 2 * SINGLE_MOVE);

  (void) bribe_nearby(ait, dipl);
  cr_expect_eq(unit_list_size(b->units), 0,
               "the Warriors %d tiles away are still %s", dist,
               nation_adjective_for_player(b));
}

Test(daidiplomat, bribes_an_adjacent_unit)
{
  bribes_at(1);
}

Test(daidiplomat, bribes_a_unit_two_tiles_away)
{
  bribes_at(2);
}
