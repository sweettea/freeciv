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

/* Tests of the default AI's hunter code. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

/* common */
#include "ai.h"

/* server */
#include "aiiface.h"

/* ai/default */
#include "daihunter.c"

#include "fixture.h"

struct ruleset_name {
  char ruleset[16];
};

ParameterizedTestParameters(daihunter, takes_target)
{
  static struct ruleset_name params[] = {
    { "classic" }, { "civ2civ3" },
  };

  return cr_make_param_array(struct ruleset_name, params,
                             sizeof(params) / sizeof(params[0]));
}

/**********************************************************************//**
  A hard AI player (the levels below have H_TARGETS, which turns off
  hunter building) with a city at (5, 5) that knows Horseback Riding, at
  war with a player whose Catapult stands in the open at (8, 5).
**************************************************************************/
static struct city *hunt_setup(const char *ruleset, struct unit **target)
{
  struct player *us, *them;
  struct city *pcity;

  fx_init(ruleset);
  game.info.skill_level = AI_LEVEL_HARD;
  us = fx_ai_player(nullptr);
  them = fx_ai_player(nullptr);
  fx_war(us, them);
  fx_tech(us, "Horseback Riding");
  pcity = fx_city(us, 5, 5, "Home");
  fx_city(them, 15, 15, "Away");
  *target = fx_unit(them, "Catapult", 8, 5, 0);
  fx_show_all(us);
  fx_show_all(them);
  fx_phase_begin();

  return pcity;
}

/* A virtual Horsemen hunts the Catapult: dai_hunter_manage() picks it
 * and returns a want, which is what dai_hunter_choice() builds on. */
ParameterizedTest(struct ruleset_name *p, daihunter, takes_target,
                  .fini = fx_fini)
{
  struct ai_type *ait;
  struct unit *target;
  struct city *pcity = hunt_setup(p->ruleset, &target);
  struct player *us = city_owner(pcity);
  struct unit *hunter;
  int want;

  ait = ai_type_by_name(default_ai_type_name());
  hunter = unit_virtual_create(us, pcity, fx_utype("Horsemen"), 0);
  want = dai_hunter_manage(ait, us, hunter);
  cr_log_info("%s: hunter want %d", p->ruleset, want);
  cr_expect_gt(want, 0, "%s: hunter want %d", p->ruleset, want);
  cr_expect_eq(def_ai_unit_data(hunter, ait)->target, target->id,
               "%s: target %d, Catapult is %d", p->ruleset,
               def_ai_unit_data(hunter, ait)->target, target->id);
  unit_virtual_destroy(hunter);
}

ParameterizedTestParameters(daihunter, choice_builds_hunter)
{
  static struct ruleset_name params[] = {
    { "classic" }, { "civ2civ3" },
  };

  return cr_make_param_array(struct ruleset_name, params,
                             sizeof(params) / sizeof(params[0]));
}

/* The city's build advisor wants a hunter for the Catapult. */
ParameterizedTest(struct ruleset_name *p, daihunter, choice_builds_hunter,
                  .fini = fx_fini)
{
  struct ai_type *ait;
  struct unit *target;
  struct city *pcity = hunt_setup(p->ruleset, &target);
  struct adv_choice *choice = adv_new_choice();

  ait = ai_type_by_name(default_ai_type_name());
  dai_hunter_choice(ait, city_owner(pcity), pcity, choice, TRUE);
  cr_log_info("%s: %s at want " ADV_WANT_PRINTF, p->ruleset,
              choice->value.utype != nullptr
              ? utype_rule_name(choice->value.utype) : "nothing",
              choice->want);
  cr_expect_gt(choice->want, 0, "%s: hunter choice want " ADV_WANT_PRINTF,
               p->ruleset, choice->want);
  cr_expect_eq(choice->type, CT_ATTACKER, "%s: choice type %d",
               p->ruleset, choice->type);
  adv_free_choice(choice);
}
