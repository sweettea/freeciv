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

/* Airlift capacity rules, as game.ruleset and the airliftingstyle
 * setting document them: a city with no airlift capacity left can
 * neither send nor receive an airlift, unless airliftingstyle makes
 * that end unlimited AND the ruleset's airlift_*_always_enabled flag
 * allows it even at zero capacity. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

/* common */
#include "actions.h"
#include "city.h"
#include "game.h"
#include "improvement.h"
#include "map.h"
#include "player.h"
#include "unit.h"

/* server */
#include "unithand.h"

#include "fixture.h"

struct ruleset_name {
  char ruleset[16];
};

static struct ruleset_name rulesets[] = {
  { "classic" }, { "civ2civ3" },
};

#define NUM_RULESETS (sizeof(rulesets) / sizeof(rulesets[0]))

/**********************************************************************//**
  Build an Airport in `pcity` and give it this turn's airlift capacity,
  as the turn change does.
**************************************************************************/
static void give_airport(struct city *pcity)
{
  struct impr_type *airport = improvement_by_rule_name("Airport");

  cr_assert_not_null(airport, "no Airport in %s", game.control.name);
  city_add_improvement(pcity, airport);
  city_refresh_from_main_map(&(wld.map), pcity, nullptr);
  pcity->airlift = city_airlift_max(pcity);
}

/**********************************************************************//**
  A city with this turn's airlift capacity, Airport or not.
**************************************************************************/
static struct city *lift_city(struct player *owner, int x, int y,
                              const char *name, bool airport)
{
  struct city *pcity = fx_city(owner, x, y, name);

  if (airport) {
    give_airport(pcity);
  } else {
    pcity->airlift = city_airlift_max(pcity);
  }
  cr_assert_eq(pcity->airlift, airport ? 1 : 0, "%s: %s has capacity %d",
               game.control.name, name, pcity->airlift);

  return pcity;
}

/**********************************************************************//**
  A Mech. Inf. with full moves in `pcity`.
**************************************************************************/
static struct unit *lifter(struct city *pcity)
{
  struct tile *ptile = city_tile(pcity);
  struct unit *punit = fx_unit(city_owner(pcity), "Mech. Inf.",
                               index_to_map_pos_x(tile_index(ptile)),
                               index_to_map_pos_y(tile_index(ptile)), 0);

  cr_assert_gt(punit->moves_left, 0);

  return punit;
}

/**********************************************************************//**
  Whether the documented rule lets an end with `capacity` take part in
  an airlift.
**************************************************************************/
static bool documented_allows(bool always_enabled, bool unlimited,
                              int capacity)
{
  return capacity > 0 || (unlimited && always_enabled);
}

ParameterizedTestParameters(airlift, shipped_defaults)
{
  return cr_make_param_array(struct ruleset_name, rulesets, NUM_RULESETS);
}

/* The two rulesets ship the flags the rest of this file starts from,
 * and the server starts with airliftingstyle CLASSICAL. */
ParameterizedTest(struct ruleset_name *p, airlift, shipped_defaults,
                  .fini = fx_fini)
{
  fx_init(p->ruleset);
  cr_expect(game.info.airlift_to_always_enabled, "%s", p->ruleset);
  cr_expect(!game.info.airlift_from_always_enabled, "%s", p->ruleset);
  cr_expect_eq(game.info.airlifting_style, AIRLIFTING_CLASSICAL, "%s",
               p->ruleset);
}

ParameterizedTestParameters(airlift, destination_rule)
{
  return cr_make_param_array(struct ruleset_name, rulesets, NUM_RULESETS);
}

/* Every combination of airlift_to_always_enabled, DEST_UNLIMITED and
 * destination capacity, from a source with an Airport to a city of the
 * same player. The server's check and the action enabler both follow
 * the documented rule. */
ParameterizedTest(struct ruleset_name *p, airlift, destination_rule,
                  .fini = fx_fini)
{
  struct player *us;
  struct city *src, *dst;
  struct unit *punit;
  int i;

  fx_init(p->ruleset);
  us = fx_player();
  src = lift_city(us, 3, 3, "Source", TRUE);
  dst = lift_city(us, 12, 12, "Dest", FALSE);
  punit = lifter(src);
  fx_show_all(us);

  for (i = 0; i < 8; i++) {
    bool always = (i & 1) != 0;
    bool unlimited = (i & 2) != 0;
    int capacity = (i & 4) != 0 ? 1 : 0;
    bool expect = documented_allows(always, unlimited, capacity);
    enum unit_airlift_result res;
    bool enabled;

    game.info.airlift_to_always_enabled = always;
    game.info.airlifting_style = unlimited ? AIRLIFTING_UNLIMITED_DEST
                                           : AIRLIFTING_CLASSICAL;
    dst->airlift = capacity;

    res = test_unit_can_airlift_to(&(wld.map), nullptr, punit, dst);
    enabled = is_action_enabled_unit_on_city(&(wld.map), ACTION_AIRLIFT,
                                             punit, dst);
    cr_expect_eq(res, expect ? AR_OK : AR_DST_NO_FLIGHTS,
                 "%s: to_always %d, DEST_UNLIMITED %d, capacity %d: "
                 "result %d", p->ruleset, always, unlimited, capacity, res);
    cr_expect_eq(enabled, expect,
                 "%s: to_always %d, DEST_UNLIMITED %d, capacity %d: "
                 "enabled %d", p->ruleset, always, unlimited, capacity,
                 enabled);
    /* The owner of both cities knows their counters. */
    cr_expect_eq(test_unit_can_airlift_to(&(wld.map), us, punit, dst), res,
                 "%s: owner's view differs", p->ruleset);
  }
}

ParameterizedTestParameters(airlift, source_rule)
{
  return cr_make_param_array(struct ruleset_name, rulesets, NUM_RULESETS);
}

/* The same over airlift_from_always_enabled, SRC_UNLIMITED and source
 * capacity, to a destination with an Airport. The predicate agrees
 * with the action's own source check in action_hard_reqs_actor(). */
ParameterizedTest(struct ruleset_name *p, airlift, source_rule,
                  .fini = fx_fini)
{
  struct player *us;
  struct city *src, *dst;
  struct unit *punit;
  int i;

  fx_init(p->ruleset);
  us = fx_player();
  src = lift_city(us, 3, 3, "Source", FALSE);
  dst = lift_city(us, 12, 12, "Dest", TRUE);
  punit = lifter(src);
  fx_show_all(us);

  for (i = 0; i < 8; i++) {
    bool always = (i & 1) != 0;
    bool unlimited = (i & 2) != 0;
    int capacity = (i & 4) != 0 ? 1 : 0;
    bool expect = documented_allows(always, unlimited, capacity);
    enum unit_airlift_result res;
    bool enabled;

    game.info.airlift_from_always_enabled = always;
    game.info.airlifting_style = unlimited ? AIRLIFTING_UNLIMITED_SRC
                                           : AIRLIFTING_CLASSICAL;
    src->airlift = capacity;

    res = test_unit_can_airlift_to(&(wld.map), nullptr, punit, dst);
    enabled = is_action_enabled_unit_on_city(&(wld.map), ACTION_AIRLIFT,
                                             punit, dst);
    cr_expect_eq(res, expect ? AR_OK : AR_SRC_NO_FLIGHTS,
                 "%s: from_always %d, SRC_UNLIMITED %d, capacity %d: "
                 "result %d", p->ruleset, always, unlimited, capacity, res);
    cr_expect_eq(enabled, expect,
                 "%s: from_always %d, SRC_UNLIMITED %d, capacity %d: "
                 "enabled %d", p->ruleset, always, unlimited, capacity,
                 enabled);
  }
}

ParameterizedTestParameters(airlift, allied_destination)
{
  return cr_make_param_array(struct ruleset_name, rulesets, NUM_RULESETS);
}

/* With TO_ALLIES, an ally's city without an Airport refuses an airlift.
 * The actor's player cannot see the ally's counter, so from its view
 * the answer is unknown, unless the destination end is unlimited
 * whatever the counter says. */
ParameterizedTest(struct ruleset_name *p, airlift, allied_destination,
                  .fini = fx_fini)
{
  struct player *us, *ally;
  struct city *src, *dst;
  struct unit *punit;

  fx_init(p->ruleset);
  us = fx_player();
  ally = fx_player();
  fx_diplstate(us, ally, DS_ALLIANCE);
  src = lift_city(us, 3, 3, "Source", TRUE);
  dst = lift_city(ally, 12, 12, "Ally", FALSE);
  punit = lifter(src);
  fx_show_all(us);
  game.info.airlifting_style = AIRLIFTING_ALLIED_DEST;

  cr_expect_eq(test_unit_can_airlift_to(&(wld.map), nullptr, punit, dst),
               AR_DST_NO_FLIGHTS, "%s: server view", p->ruleset);
  cr_expect(!is_action_enabled_unit_on_city(&(wld.map), ACTION_AIRLIFT,
                                            punit, dst),
            "%s: action enabled", p->ruleset);
  cr_expect_eq(test_unit_can_airlift_to(&(wld.map), us, punit, dst),
               AR_OK_DST_UNKNOWN, "%s: actor's view", p->ruleset);

  game.info.airlifting_style |= AIRLIFTING_UNLIMITED_DEST;
  cr_expect_eq(test_unit_can_airlift_to(&(wld.map), nullptr, punit, dst),
               AR_OK, "%s: unlimited, server view", p->ruleset);
  cr_expect_eq(test_unit_can_airlift_to(&(wld.map), us, punit, dst),
               AR_OK, "%s: unlimited, actor's view", p->ruleset);
}

ParameterizedTestParameters(airlift, perform_respects_capacity)
{
  return cr_make_param_array(struct ruleset_name, rulesets, NUM_RULESETS);
}

/* Through the server's action path under the default settings: a city
 * with an Airport takes one airlift per turn, and a city without one
 * takes none. Its counter never goes below zero. */
ParameterizedTest(struct ruleset_name *p, airlift, perform_respects_capacity,
                  .fini = fx_fini)
{
  struct player *us;
  struct city *src1, *src2, *src3, *port, *field;
  struct unit *first, *second, *third;
  bool done;

  fx_init(p->ruleset);
  us = fx_player();
  src1 = lift_city(us, 2, 2, "Source 1", TRUE);
  src2 = lift_city(us, 2, 8, "Source 2", TRUE);
  src3 = lift_city(us, 2, 14, "Source 3", TRUE);
  port = lift_city(us, 14, 4, "Port", TRUE);
  field = lift_city(us, 14, 14, "Field", FALSE);
  first = lifter(src1);
  second = lifter(src2);
  third = lifter(src3);
  /* Fixture cities have no vision of their own, and a city that loses
   * sight of its centre (when a lifter leaves it, or at teardown) fails
   * a sanity check. fx_see_all() keeps every tile seen until the
   * cities are gone. The cities are shown first: a tile that is
   * already seen does not pick its city up. */
  fx_show_all(us);
  fx_see_all(us);
  fx_unit_vision(first);
  fx_unit_vision(second);
  fx_unit_vision(third);

  done = unit_perform_action(us, first->id, port->id, NO_TARGET, "",
                             ACTION_AIRLIFT, ACT_REQ_PLAYER);
  cr_expect(done, "%s: first airlift to the Airport refused", p->ruleset);
  cr_expect_eq(unit_tile(first), city_tile(port), "%s: not in Port",
               p->ruleset);
  cr_expect_eq(port->airlift, 0, "%s: Port after one landing: %d",
               p->ruleset, port->airlift);

  done = unit_perform_action(us, second->id, port->id, NO_TARGET, "",
                             ACTION_AIRLIFT, ACT_REQ_PLAYER);
  cr_expect(!done, "%s: second airlift to the Airport done", p->ruleset);
  cr_expect_eq(unit_tile(second), city_tile(src2), "%s: left Source 2",
               p->ruleset);
  cr_expect_eq(port->airlift, 0, "%s: Port after a second try: %d",
               p->ruleset, port->airlift);
  cr_expect_eq(src2->airlift, 1, "%s: Source 2 left with %d", p->ruleset,
               src2->airlift);

  done = unit_perform_action(us, third->id, field->id, NO_TARGET, "",
                             ACTION_AIRLIFT, ACT_REQ_PLAYER);
  cr_expect(!done, "%s: airlift to a city without an Airport done",
            p->ruleset);
  cr_expect_eq(unit_tile(third), city_tile(src3), "%s: left Source 3",
               p->ruleset);
  cr_expect_eq(field->airlift, 0, "%s: Field after a try: %d",
               p->ruleset, field->airlift);
}
