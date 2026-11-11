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

/* Tests of static functions in ai/default/daiunit.c.
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
#include "citytools.h"
#include "unittools.h"

/* ai/default */
#include "daiunit.c"
#include "daicity.h"

#include "fixture.h"

TestSuite(daiunit, .init = fx_init_classic, .fini = fx_fini);

/* A unit next to an empty enemy city walks in when its rampage is told
 * to move only for a free city. The city is size 1, so conquering it
 * destroys it. Real cities and units, since conquest frees the city; the
 * map is shown before the unit's vision is set up so that the city is
 * known to us. */
Test(daiunit, rampage_takes_free_city)
{
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct unit *punit;
  int id;

  fx_war(us, them);
  create_city(us, fx_tile(3, 5), "Home", us);
  create_city(them, fx_tile(9, 5), "Target", them);
  cr_assert_not_null(tile_city(fx_tile(9, 5)));
  fx_show_all(us);
  fx_show_all(them);
  punit = create_unit(us, fx_tile(8, 5), fx_utype("Warriors"), 0,
                      tile_city(fx_tile(3, 5))->id, -1);
  cr_assert_not_null(punit);
  id = punit->id;
  fx_phase_begin();

  cr_assert_eq(dai_rampage_want(punit, fx_tile(9, 5)),
               -RAMPAGE_FREE_CITY_OR_BETTER);
  cr_expect(dai_military_rampage(punit, BODYGUARD_RAMPAGE_THRESHOLD,
                                 RAMPAGE_FREE_CITY_OR_BETTER));
  cr_assert_not_null(game_unit_by_number(id));
  cr_expect_eq(unit_tile(punit), fx_tile(9, 5),
               "Warriors still at (%d, %d)", TILE_XY(unit_tile(punit)));
  cr_expect_null(tile_city(fx_tile(9, 5)), "enemy city still stands");
}

/* A player is not hostile to itself; a player at war, or one never
 * met, still is. */
Test(daiunit, not_hostile_to_self)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *enemy = fx_ai_player(nullptr);
  struct player *stranger = fx_ai_player(nullptr);

  fx_war(us, enemy);
  fx_phase_begin();

  cr_expect(!POTENTIALLY_HOSTILE_PLAYER(ait, us, us));
  cr_expect(POTENTIALLY_HOSTILE_PLAYER(ait, us, enemy));
  cr_expect(POTENTIALLY_HOSTILE_PLAYER(ait, us, stranger));
}

/* A Legion being considered for building in a city held by one
 * Phalanx targets the enemy's distant, well-defended city, not the city
 * it is built in. dai_build_adv_init() sets each city's worth, as
 * building_advisor() does every turn. */
Test(daiunit, virtual_attacker_targets_enemy)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *enemy = fx_ai_player(nullptr);
  struct city *home, *target;
  struct unit *legion;
  struct tile *dest = nullptr;
  adv_want want;

  fx_war(us, enemy);
  home = fx_city(us, 3, 5, "Home");
  fx_unit(us, "Phalanx", 3, 5, 0);
  target = fx_city(enemy, 16, 15, "Target");
  fx_unit(enemy, "Musketeers", 16, 15, 0);
  fx_unit(enemy, "Musketeers", 16, 15, 0);
  fx_show_all(us);
  fx_show_all(enemy);
  fx_phase_begin();
  dai_build_adv_init(ait, us);
  dai_build_adv_init(ait, enemy);

  legion = unit_virtual_create(us, home, fx_utype("Legion"), 0);
  want = find_something_to_kill(ait, us, legion, &dest, nullptr, nullptr,
                                nullptr, nullptr, nullptr);
  cr_assert_not_null(dest);
  cr_expect_neq(tile_city(dest), home,
                "targets its own city, want " ADV_WANT_PRINTF, want);
  cr_expect_eq(dest, city_tile(target), "target (%d, %d)", TILE_XY(dest));
  unit_virtual_destroy(legion);
}

/* A Legion on its way to an enemy city counts as one attack on it, not
 * one per move fragment. */
Test(daiunit, invasion_counts_attacks)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *enemy = fx_ai_player(nullptr);
  struct city *target;
  struct unit *legion, *armor;
  struct tile *dest = nullptr;

  fx_war(us, enemy);
  fx_city(us, 2, 5, "Home");
  target = fx_city(enemy, 12, 5, "Target");
  fx_unit(enemy, "Phalanx", 12, 5, 0);
  fx_unit(enemy, "Phalanx", 12, 5, 0);
  legion = fx_unit(us, "Legion", 6, 5, 0);
  legion->activity = ACTIVITY_GOTO;
  legion->goto_tile = city_tile(target);
  armor = fx_unit(us, "Armor", 4, 5, 0);
  fx_show_all(us);
  fx_show_all(enemy);
  fx_phase_begin();

  (void) find_something_to_kill(ait, us, armor, &dest, nullptr, nullptr,
                                nullptr, nullptr, nullptr);
  cr_expect_eq(def_ai_city_data(target, ait)->invasion.attack, 1,
               "invasion.attack %d",
               def_ai_city_data(target, ait)->invasion.attack);
  cr_expect_eq(def_ai_city_data(target, ait)->invasion.occupy, 1);
}

/* Musketeers on Hills defend at twice their rating, so a Catapult and
 * a Knights next to them (combined attack below that) are no danger;
 * adding a second Catapult is. */
Test(daiunit, hills_danger_uses_defence_squared)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *enemy = fx_ai_player(nullptr);
  struct unit *musketeers;
  enum override_bool result = NO_OVERRIDE;
  int def, att;

  fx_war(us, enemy);
  fx_set_terrain(5, 5, "Hills");
  musketeers = fx_unit(us, "Musketeers", 5, 5, 0);
  att = adv_unit_att_rating(fx_unit(enemy, "Catapult", 6, 5, 0));
  att += adv_unit_att_rating(fx_unit(enemy, "Knights", 6, 5, 0));
  fx_show_all(us);
  fx_phase_begin();

  def = adv_unit_def_rating_basic(musketeers);
  cr_assert_gt(att, def);
  cr_assert_lt(att, 2 * def);
  dai_consider_tile_dangerous(ait, fx_tile(5, 5), musketeers, &result);
  cr_expect_eq(result, OVERRIDE_FALSE, "attack %d against defence %d on Hills"
               " read as dangerous", att, def);

  fx_unit(enemy, "Catapult", 4, 5, 0);
  result = NO_OVERRIDE;
  dai_consider_tile_dangerous(ait, fx_tile(5, 5), musketeers, &result);
  cr_expect_eq(result, OVERRIDE_TRUE);
}

TestSuite(daiunit_civ2civ3, .init = fx_init_civ2civ3, .fini = fx_fini);

/* The civ2civ3 Transport has the Workers flag. It is still managed as
 * a ferry: carrying Workers, it makes them its passenger-in-charge. */
Test(daiunit_civ2civ3, transport_managed_as_ferry)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct unit *transport, *workers;
  int y;

  for (y = 0; y < FX_MAP_YSIZE; y++) {
    fx_set_terrain(10, y, "Ocean");
    fx_set_terrain(11, y, "Ocean");
  }
  fx_city(us, 9, 5, "Port");
  /* create_unit() registers the ferry with the AI. */
  transport = create_unit(us, fx_tile(10, 5), fx_utype("Transport"), 0, 0,
                          -1);
  cr_assert_not_null(transport);
  workers = fx_unit(us, "Workers", 10, 5, 0);
  cr_assert(unit_transport_load(workers, transport, FALSE));
  fx_show_all(us);
  fx_phase_begin();

  cr_assert(dai_is_ferry(transport, ait));
  cr_assert(unit_type_get(transport)->adv.worker);
  dai_manage_unit(ait, us, transport);
  cr_expect_eq(def_ai_unit_data(transport, ait)->passenger, workers->id,
               "passenger-in-charge %d",
               def_ai_unit_data(transport, ait)->passenger);
}

/* An old Caravan at Home (7, 5), coastal, with an overseas trade task
 * for Partner (15, 5) across the sea at x = 8..11. Turn 30; the
 * Caravan was built 25 turns ago. No boats. */
static struct unit *boat_wait_scene(struct ai_type *ait, struct player *us)
{
  struct city *home, *partner;
  struct unit *caravan;
  int x, y;

  for (x = 8; x <= 11; x++) {
    for (y = 0; y < FX_MAP_YSIZE; y++) {
      fx_set_terrain(x, y, "Ocean");
    }
  }
  home = fx_city(us, 7, 5, "Home");
  partner = fx_city(us, 15, 5, "Partner");
  caravan = fx_unit(us, "Caravan", 7, 5, 0);
  caravan->homecity = home->id;
  fx_show_all(us);
  fx_phase_begin();

  game.info.turn = 30;
  caravan->birth_turn = game.info.turn - 25;
  dai_unit_new_task(ait, caravan, AIUNIT_TRADE, city_tile(partner));
  cr_assert_eq(caravan->goto_tile, city_tile(partner));
  cr_assert(!goto_is_sane(caravan, city_tile(partner)));
  cr_assert_eq(aiferry_avail_boats(ait, us), 0);

  return caravan;
}

/* The wait for a boat is timed from the request, not from the unit's
 * birth: not tired when the target is new, nor 15 turns after asking;
 * tired 16 turns after. */
Test(daiunit, caravan_boat_wait_from_request)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct unit *caravan = boat_wait_scene(ait, us);

  cr_expect(!dai_is_unit_tired_waiting_boat(ait, caravan),
            "tired on the turn it got the target");
  cr_assert(dai_find_boat_for_unit(ait, caravan));
  game.info.turn = 45;
  cr_expect(!dai_is_unit_tired_waiting_boat(ait, caravan),
            "tired 15 turns after asking");
  game.info.turn = 46;
  cr_expect(dai_is_unit_tired_waiting_boat(ait, caravan),
            "not tired 16 turns after asking");
}

/* A new task restarts the wait. */
Test(daiunit, caravan_boat_wait_cleared_by_new_task)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct unit *caravan = boat_wait_scene(ait, us);
  struct tile *dest = caravan->goto_tile;

  cr_assert(dai_find_boat_for_unit(ait, caravan));
  game.info.turn = 40;
  dai_unit_new_task(ait, caravan, AIUNIT_TRADE, dest);
  game.info.turn = 46;
  cr_expect(!dai_is_unit_tired_waiting_boat(ait, caravan),
            "tired 6 turns into the new task");
}

/* Boarding ends the wait: a Caravan that boarded and is back ashore
 * waits afresh. */
Test(daiunit, caravan_boat_wait_cleared_by_boarding)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct unit *caravan = boat_wait_scene(ait, us);
  struct unit *boat = fx_unit(us, "Transport", 7, 5, 0);

  cr_assert(dai_find_boat_for_unit(ait, caravan));
  game.info.turn = 40;
  cr_assert(unit_transport_load(caravan, boat, FALSE));
  caravan->moves_left = 0;
  dai_caravan_goto(ait, us, caravan, tile_city(caravan->goto_tile), FALSE,
                   TRUE, FALSE);
  cr_assert(unit_transport_unload(caravan));
  game.info.turn = 46;
  cr_expect(!dai_is_unit_tired_waiting_boat(ait, caravan),
            "tired 6 turns after boarding");
}

/* The sea trip a virtual 'attacker' at Home (7, 5) plans against Target
 * (13, 5), an enemy city across the sea at x = 8..11 held by Warriors.
 * 'techs' (nullptr-terminated) are known to us. */
struct sea_trip {
  struct tile *dest;
  struct tile *target;
  const struct unit_type *boattype;
  bool ferry_map;
};

static struct sea_trip plan_sea_trip(const char *attacker,
                                     const char *const techs[])
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *enemy = fx_ai_player(nullptr);
  struct city *home, *target;
  struct unit *punit, *ferryboat;
  struct pf_map *ferry_map;
  struct sea_trip trip;
  int x, y, i;

  for (x = 8; x <= 11; x++) {
    for (y = 0; y < FX_MAP_YSIZE; y++) {
      fx_set_terrain(x, y, "Ocean");
    }
  }
  for (i = 0; techs[i] != nullptr; i++) {
    fx_tech(us, techs[i]);
  }
  fx_war(us, enemy);
  home = fx_city(us, 7, 5, "Home");
  fx_unit(us, "Warriors", 7, 5, 0);
  target = fx_city(enemy, 13, 5, "Target");
  fx_unit(enemy, "Warriors", 13, 5, 0);
  fx_show_all(us);
  fx_show_all(enemy);
  fx_phase_begin();
  dai_build_adv_init(ait, us);
  dai_build_adv_init(ait, enemy);

  punit = unit_virtual_create(us, home, fx_utype(attacker), 0);
  (void) find_something_to_kill(ait, us, punit, &trip.dest, nullptr,
                                &ferry_map, &ferryboat, &trip.boattype,
                                nullptr);
  trip.ferry_map = (ferry_map != nullptr);
  if (ferry_map != nullptr) {
    pf_map_destroy(ferry_map);
  }
  trip.target = city_tile(target);
  unit_virtual_destroy(punit);

  return trip;
}

/* Classic's Chariot is class Land, which a Trireme carries: it plans
 * the trip on a Trireme. */
Test(daiunit, chariot_sails_on_trireme)
{
  const char *const techs[] = { "Map Making", "The Wheel", nullptr };
  struct sea_trip trip = plan_sea_trip("Chariot", techs);

  cr_expect_eq(trip.dest, trip.target, "target (%d, %d)",
               TILE_XY(trip.dest));
  cr_expect_eq(trip.boattype, fx_utype("Trireme"), "boat %s",
               trip.boattype != nullptr
               ? utype_rule_name(trip.boattype) : "none");
  cr_expect(trip.ferry_map);
}

/* civ2civ3's Chariot is class Big Land. Trireme is the only ferry it
 * can build and cannot carry it, so it plans no sea trip at all. */
Test(daiunit_civ2civ3, big_land_plans_no_trip_on_trireme)
{
  const char *const techs[] = { "Map Making", "The Wheel", nullptr };
  struct sea_trip trip = plan_sea_trip("Chariot", techs);

  cr_expect_null(trip.boattype, "boat %s",
                 trip.boattype != nullptr
                 ? utype_rule_name(trip.boattype) : "none");
  cr_expect(!trip.ferry_map);
  cr_expect_neq(trip.dest, trip.target, "plans to sail to the target");
}

/* With Navigation the Galleon, which carries Big Land, is the boat. */
Test(daiunit_civ2civ3, big_land_sails_on_galleon)
{
  const char *const techs[] = { "Map Making", "The Wheel", "Navigation",
                                nullptr };
  struct sea_trip trip = plan_sea_trip("Chariot", techs);

  cr_expect_eq(trip.dest, trip.target, "target (%d, %d)",
               TILE_XY(trip.dest));
  cr_expect_eq(trip.boattype, fx_utype("Galleon"), "boat %s",
               trip.boattype != nullptr
               ? utype_rule_name(trip.boattype) : "none");
  cr_expect(trip.ferry_map);
}

/* A Legion is class Land: it still plans the trip on a Trireme. */
Test(daiunit_civ2civ3, land_sails_on_trireme)
{
  const char *const techs[] = { "Map Making", "Iron Working", nullptr };
  struct sea_trip trip = plan_sea_trip("Legion", techs);

  cr_expect_eq(trip.dest, trip.target, "target (%d, %d)",
               TILE_XY(trip.dest));
  cr_expect_eq(trip.boattype, fx_utype("Trireme"), "boat %s",
               trip.boattype != nullptr
               ? utype_rule_name(trip.boattype) : "none");
  cr_expect(trip.ferry_map);
}
