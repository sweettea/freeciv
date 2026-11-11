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

/* The default AI's bodyguard links and a change of owner (RM #2451).
 *
 * transfer_city() hands the units a city supports to its new owner
 * through unit_change_homecity_handling(), which changes punit->owner
 * in place: same struct, same id. The only AI hooks it calls are
 * unit_lost and unit_got, which for the classic AI touch ferry data,
 * so the per-ai-type struct unit_ai keeps its bodyguard and charge
 * fields. A guard that changes owner goes on "guarding" a unit of the
 * player it now may be at war with. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <signal.h>

#include <criterion/criterion.h>

/* common */
#include "city.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "unit.h"
#include "unittype.h"

/* server */
#include "aiiface.h"
#include "citytools.h"
#include "unittools.h"

/* ai/default */
#include "daiguard.h"
#include "daiplayer.h"
#include "daitools.h"
#include "daiunit.h"

#include "fixture.h"

/* Two players of one AI type at war: A has cities C1 and C2, the
 * attacker X is homed in C1 and its guard Y in C2, side by side in the
 * open. */
struct scene {
  struct ai_type *ait;
  struct player *a, *b;
  struct city *c1, *c2;
  struct unit *x, *y;
};

/**********************************************************************//**
  Build the scene for players of the named AI type with real server
  cities and units, and make Y the guard of X.
**************************************************************************/
static void scene_init(struct scene *s, const char *ai_name)
{
  s->ait = ai_type_by_name(ai_name);
  cr_assert_not_null(s->ait, "the %s AI is not compiled in", ai_name);

  s->a = fx_ai_player(ai_name);
  s->b = fx_ai_player(ai_name);
  fx_war(s->a, s->b);

  create_city(s->a, fx_tile(3, 3), "C1", s->a);
  s->c1 = tile_city(fx_tile(3, 3));
  create_city(s->a, fx_tile(15, 3), "C2", s->a);
  s->c2 = tile_city(fx_tile(15, 3));
  cr_assert_not_null(s->c1);
  cr_assert_not_null(s->c2);

  s->x = create_unit(s->a, fx_tile(9, 12), fx_utype("Armor"), 0,
                     s->c1->id, -1);
  s->y = create_unit(s->a, fx_tile(10, 12), fx_utype("Warriors"), 0,
                     s->c2->id, -1);
  cr_assert_not_null(s->x);
  cr_assert_not_null(s->y);
  fx_show_all(s->a);
  fx_show_all(s->b);

  aiguard_assign_guard_unit(s->ait, s->x, s->y);
  cr_assert_eq(aiguard_guard_of(s->ait, s->x), s->y);
}

/**********************************************************************//**
  Give C2 to B the way civil_war() gives a city to the new player.
**************************************************************************/
static void scene_transfer(struct scene *s)
{
  cr_assert(transfer_city(s->b, s->c2, -1, FALSE, FALSE, FALSE, FALSE));
}

TestSuite(bodyguard, .init = fx_init_classic, .fini = fx_fini);

/* The cause: after the transfer Y is B's, at war with A, and both
 * halves of the link are still in place. */
Test(bodyguard, link_survives_transfer)
{
  struct scene s;
  struct unit *y;
  int y_id;

  scene_init(&s, "classic");
  y = s.y;
  y_id = s.y->id;
  scene_transfer(&s);

  cr_expect_eq(unit_owner(y), s.b, "Y did not change owner");
  cr_expect_eq(game_unit_by_number(y_id), y,
               "Y is no longer the same unit struct under the same id");
  cr_expect(pplayers_at_war(s.a, s.b));
  cr_expect_eq(def_ai_unit_data(s.x, s.ait)->bodyguard, y_id,
               "X no longer lists Y as its bodyguard");
  cr_expect_eq(def_ai_unit_data(y, s.ait)->charge, s.x->id,
               "Y no longer lists X as its charge");
  cr_expect_eq(aiguard_guard_of(s.ait, s.x), y,
               "aiguard_guard_of() no longer returns Y");
}

/* The crash this leads to (RM #2451). The test passes while the code
 * segfaults.
 *
 * dai_unit_attack() reads bodyguard = aiguard_guard_of(X) before the
 * action, so it holds a pointer to Y, now an enemy unit standing on the
 * target tile. The attack kills Y: wipe_unit() frees the struct after
 * the AI's unit_free hook has cleared its server.ais slots. The
 * function then checks only that X survived, and since X moved onto
 * the tile it reads def_ai_unit_data(bodyguard, ait)->charge through
 * the freed struct's NULL slot.
 *
 * The read is reached only if X occupies the tile it won, so
 * occupychance is 100 here; classic defaults it to 0, civ2civ3 (the
 * ruleset of the game the crash was seen in) sets 100. Y has 1 hp so
 * Armor cannot lose the roll. */
Test(bodyguard, attack_kills_ex_guard_crashes, .signal = SIGSEGV)
{
  struct scene s;

  scene_init(&s, "classic");
  scene_transfer(&s);
  game.server.occupychance = 100;
  s.y->hp = 1;

  (void) dai_unit_attack(s.ait, s.x, unit_tile(s.y));
}
