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

/* Tests of the default AI's paratrooper code. */

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
#include "daiparadrop.c"

#include "fixture.h"

struct ruleset_name {
  char ruleset[16];
};

static struct ruleset_name rulesets[] = {
  { "classic" }, { "civ2civ3" },
};

#define NUM_RULESETS (sizeof(rulesets) / sizeof(rulesets[0]))

/**********************************************************************//**
  An AI player with a city at (5, 5), at war with a player whose city
  at (12, 5) is in Paratroopers range. The AI knows Combined Arms if
  `paratroopers` is set.
**************************************************************************/
static struct city *drop_setup(const char *ruleset, bool paratroopers)
{
  struct player *us, *them;
  struct city *pcity;

  fx_init(ruleset);
  us = fx_ai_player(nullptr);
  them = fx_ai_player(nullptr);
  fx_war(us, them);
  if (paratroopers) {
    fx_tech(us, "Combined Arms");
  }
  pcity = fx_city(us, 5, 5, "Home");
  fx_city(them, 12, 5, "Target");
  fx_show_all(us);
  fx_show_all(them);
  fx_phase_begin();

  return pcity;
}

ParameterizedTestParameters(daiparadrop, builds_paratroopers)
{
  return cr_make_param_array(struct ruleset_name, rulesets, NUM_RULESETS);
}

/* A city that can build Paratroopers with an enemy city in range
 * chooses them. */
ParameterizedTest(struct ruleset_name *p, daiparadrop, builds_paratroopers,
                  .fini = fx_fini)
{
  struct ai_type *ait;
  struct city *pcity = drop_setup(p->ruleset, TRUE);
  struct adv_choice *choice = adv_new_choice();

  ait = ai_type_by_name(default_ai_type_name());
  dai_choose_paratrooper(ait, &(wld.map), city_owner(pcity), pcity,
                         choice, TRUE);
  cr_log_info("%s: %s at want " ADV_WANT_PRINTF, p->ruleset,
              choice->value.utype != nullptr
              ? utype_rule_name(choice->value.utype) : "nothing",
              choice->want);
  cr_expect_gt(choice->want, 0, "%s: want " ADV_WANT_PRINTF,
               p->ruleset, choice->want);
  cr_expect_eq(choice->value.utype, fx_utype("Paratroopers"),
               "%s: chose %s", p->ruleset,
               choice->value.utype != nullptr
               ? utype_rule_name(choice->value.utype) : "nothing");
  adv_free_choice(choice);
}

ParameterizedTestParameters(daiparadrop, wants_paratrooper_tech)
{
  return cr_make_param_array(struct ruleset_name, rulesets, NUM_RULESETS);
}

/* A player without Combined Arms raises its want for it. */
ParameterizedTest(struct ruleset_name *p, daiparadrop,
                  wants_paratrooper_tech, .fini = fx_fini)
{
  struct ai_type *ait;
  struct city *pcity = drop_setup(p->ruleset, FALSE);
  struct player *us = city_owner(pcity);
  struct adv_choice *choice = adv_new_choice();
  Tech_type_id tech
    = advance_number(advance_by_rule_name("Combined Arms"));
  adv_want before, after;

  ait = ai_type_by_name(default_ai_type_name());
  before = def_ai_player_data(us, ait)->tech_want[tech];

  dai_choose_paratrooper(ait, &(wld.map), us, pcity, choice, TRUE);
  after = def_ai_player_data(us, ait)->tech_want[tech];
  cr_log_info("%s: Combined Arms want " ADV_WANT_PRINTF " -> "
              ADV_WANT_PRINTF, p->ruleset, before, after);
  cr_expect_gt(after, before, "%s: Combined Arms want " ADV_WANT_PRINTF
               " -> " ADV_WANT_PRINTF, p->ruleset, before, after);
  adv_free_choice(choice);
}

/**********************************************************************//**
  A one-tile island at (x, y), ringed with Ocean.
**************************************************************************/
static void make_island(int x, int y)
{
  int dx, dy;

  for (dx = -1; dx <= 1; dx++) {
    for (dy = -1; dy <= 1; dy++) {
      if (dx != 0 || dy != 0) {
        fx_set_terrain(x + dx, y + dy, "Ocean");
      }
    }
  }
}

ParameterizedTestParameters(daiparadrop, below_emergency_still_chooses)
{
  return cr_make_param_array(struct ruleset_name, rulesets, NUM_RULESETS);
}

/* A defender want of 150 is far below a military emergency, so the
 * paratrooper choice still runs, and two enemy island cities nine tiles
 * off make Paratroopers worth more than that. */
ParameterizedTest(struct ruleset_name *p, daiparadrop,
                  below_emergency_still_chooses, .fini = fx_fini)
{
  struct ai_type *ait;
  struct player *us, *them;
  struct city *pcity;
  struct adv_choice *choice;

  fx_init(p->ruleset);
  make_island(14, 5);
  make_island(14, 12);
  us = fx_ai_player(nullptr);
  them = fx_ai_player(nullptr);
  fx_war(us, them);
  fx_tech(us, "Combined Arms");
  pcity = fx_city(us, 5, 5, "Home");
  fx_city(them, 14, 5, "Isle");
  fx_city(them, 14, 12, "Rock");
  fx_show_all(us);
  fx_show_all(them);
  fx_phase_begin();

  ait = ai_type_by_name(default_ai_type_name());
  choice = adv_new_choice();
  choice->type = CT_DEFENDER;
  choice->want = 150;
  cr_assert_lt(choice->want, DAI_WANT_MILITARY_EMERGENCY);

  dai_choose_paratrooper(ait, &(wld.map), us, pcity, choice, TRUE);
  cr_log_info("%s: %s at want " ADV_WANT_PRINTF, p->ruleset,
              choice->value.utype != nullptr
              ? utype_rule_name(choice->value.utype) : "nothing",
              choice->want);
  cr_expect_eq(choice->value.utype, fx_utype("Paratroopers"),
               "%s: chose %s at want " ADV_WANT_PRINTF, p->ruleset,
               choice->value.utype != nullptr
               ? utype_rule_name(choice->value.utype) : "nothing",
               choice->want);
  adv_free_choice(choice);
}

ParameterizedTestParameters(daiparadrop, counts_only_war_cities)
{
  return cr_make_param_array(struct ruleset_name, rulesets, NUM_RULESETS);
}

/* A foreign city at peace or in ceasefire is no drop target, so it adds
 * nothing to the paratrooper's worth; the same city at war does. */
ParameterizedTest(struct ruleset_name *p, daiparadrop,
                  counts_only_war_cities, .fini = fx_fini)
{
  struct city *pcity = drop_setup(p->ruleset, TRUE);
  struct player *us = city_owner(pcity);
  struct player *them = city_owner(tile_city(fx_tile(12, 5)));
  struct unit *para = unit_virtual_create(us, pcity,
                                          fx_utype("Paratroopers"), 0);
  int war, peace, ceasefire;

  war = calculate_want_for_paratrooper(para, city_tile(pcity));
  fx_diplstate(us, them, DS_PEACE);
  peace = calculate_want_for_paratrooper(para, city_tile(pcity));
  fx_diplstate(us, them, DS_CEASEFIRE);
  ceasefire = calculate_want_for_paratrooper(para, city_tile(pcity));
  unit_virtual_destroy(para);

  cr_log_info("%s: war %d, peace %d, ceasefire %d", p->ruleset,
              war, peace, ceasefire);
  cr_expect_lt(peace, war, "%s: peace %d, war %d", p->ruleset, peace, war);
  cr_expect_eq(ceasefire, peace, "%s: ceasefire %d, peace %d", p->ruleset,
               ceasefire, peace);
}
