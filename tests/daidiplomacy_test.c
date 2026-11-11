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

/* Tests of ai/default/daidiplomacy.c.
 *
 * The file under test is compiled into this test, so its static
 * functions and macros can be used by name. See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "ai.h"

/* server */
#include "aiiface.h"
#include "animals.h"

/* ai/default */
#include "daidiplomacy.c"

#include "fixture.h"

TestSuite(daidiplomacy, .init = fx_init_civ2civ3, .fini = fx_fini);

/* Put a and b in `type`, met long enough ago for b to be a war
 * target. */
static void old_contact(struct player *a, struct player *b,
                        enum diplstate_type type)
{
  fx_diplstate(a, b, type);
  player_diplstate_get(a, b)->first_contact_turn
    = game.info.turn - TURNS_BEFORE_TARGET - 5;
  player_diplstate_get(b, a)->first_contact_turn
    = game.info.turn - TURNS_BEFORE_TARGET - 5;
}

/* The animal player, as the server makes it at game start, with one
 * Storm alive at sea. */
static struct player *live_animals(void)
{
  struct player *animals = nullptr;

  /* Big enough to make the player, too small to place any animal. */
  wld.map.server.animals = 1;
  create_animals();
  players_iterate(pplayer) {
    if (is_barbarian(pplayer)) {
      animals = pplayer;
    }
  } players_iterate_end;
  cr_assert_not_null(animals, "no animal player made");
  fx_set_terrain(19, 19, "Ocean");
  fx_unit(animals, "Storm", 19, 19, 0);

  return animals;
}

/* An AI that hates a peace partner enough plans a war for spoils.
 * Our war with the animals does not count as having a war already. */
static void spoils_war_planned(bool with_animals)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *a = fx_ai_player(nullptr);
  struct player *b = fx_ai_player(nullptr);
  struct player *animals = with_animals ? live_animals() : nullptr;

  old_contact(a, b, DS_PEACE);
  fx_phase_begin();
  if (animals != nullptr) {
    cr_assert(WAR(a, animals));
    cr_assert(animals->is_alive);
  }
  a->ai_common.love[player_index(b)] = -MAX_AI_LOVE / 2;

  dai_diplomacy_actions(ait, a);
  cr_expect_geq(dai_diplomacy_get(ait, a, b)->countdown, 0,
                "%s animals: countdown %d", with_animals ? "with" : "no",
                dai_diplomacy_get(ait, a, b)->countdown);
}

Test(daidiplomacy, spoils_war_without_animals)
{
  spoils_war_planned(FALSE);
}

Test(daidiplomacy, spoils_war_with_animals)
{
  spoils_war_planned(TRUE);
}
