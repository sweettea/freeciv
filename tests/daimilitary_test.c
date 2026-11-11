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

/* Tests of static functions in ai/default/daimilitary.c.
 *
 * The file under test is compiled into this test, so its static
 * functions can be called by name. Its non-static definitions then
 * satisfy every reference the program makes to that object file, and
 * the linker does not pull the library's copy out of libfc_ai.a.
 * See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "ai.h"

/* server */
#include "aiiface.h"
#include "cityturn.h"

/* ai/default */
#include "daimilitary.c"

#include "fixture.h"

TestSuite(daimilitary, .init = fx_init_classic, .fini = fx_fini);

/* assess_danger() sees no danger to a city with no enemy around, and
 * some once an enemy Armor stands three tiles away. */
Test(daimilitary, assess_danger_sees_an_attacker)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct city *pcity = fx_city(us, 5, 5, "Home");

  fx_war(us, them);
  fx_show_all(us);
  fx_phase_begin();

  cr_expect_eq(assess_danger(ait, &(wld.map), pcity, nullptr), 0);
  cr_expect_eq(def_ai_city_data(pcity, ait)->danger, 0);

  fx_unit(them, "Armor", 8, 5, 0);
  fx_show_all(us);
  (void) assess_danger(ait, &(wld.map), pcity, nullptr);
  cr_expect_gt(def_ai_city_data(pcity, ait)->danger, 0);
}

/* Give `pplayer` the default builder trait and no decision fuzz. */
static void default_builder(struct player *pplayer)
{
  pplayer->ai_common.traits[TRAIT_BUILDER].val = TRAIT_DEFAULT_VALUE;
  pplayer->ai_common.traits[TRAIT_BUILDER].mod = 0;
  pplayer->ai_common.fuzzy = 0;
}

/* At the default builder trait, adjust_ai_unit_choice() swaps a
 * defender for Barracks at the defender's full want. The city holds a
 * defender already: an empty one keeps the unit choice. */
Test(daimilitary, veterancy_building_want_at_default_trait)
{
  struct player *us = fx_ai_player(nullptr);
  struct city *pcity = fx_city(us, 5, 5, "Home");
  struct adv_choice choice;

  default_builder(us);
  fx_unit(us, "Phalanx", 5, 5, 0);
  fx_phase_begin();

  adv_init_choice(&choice);
  choice.type = CT_DEFENDER;
  choice.value.utype = (struct unit_type *) fx_utype("Phalanx");
  choice.want = 100;
  adjust_ai_unit_choice(pcity, &choice);

  cr_assert_eq(choice.type, CT_BUILDING);
  cr_expect_str_eq(improvement_rule_name(choice.value.building), "Barracks");
  cr_expect_float_eq(choice.want, 100.0, 1e-9,
                     "want " ADV_WANT_PRINTF, choice.want);
  adv_deinit_choice(&choice);
}

/* At the default builder trait, military_advisor_choose_build() asks
 * for City Walls at their full building want. The city has no shield
 * surplus, so no defender is wanted and the advisor stops after the
 * defence choice. */
Test(daimilitary, walls_want_at_default_trait)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct city *pcity;
  const struct impr_type *walls;
  struct adv_choice *choice;

  default_builder(us);
  fx_war(us, them);
  fx_tech(us, "Masonry");
  fx_tech(us, "Gunpowder");
  pcity = fx_city(us, 5, 5, "Home");
  fx_unit(us, "Musketeers", 5, 5, 0);
  fx_unit(us, "Musketeers", 5, 5, 0);
  fx_unit(them, "Legion", 6, 5, 0);
  fx_show_all(us);
  fx_phase_begin();

  walls = improvement_by_rule_name("City Walls");
  pcity->server.adv->building_want[improvement_index(walls)] = 50;
  pcity->surplus[O_SHIELD] = 0;

  choice = military_advisor_choose_build(ait, &(wld.map), us, pcity,
                                         nullptr);
  cr_assert_eq(choice->type, CT_BUILDING, "choice type %d", choice->type);
  cr_assert_eq(choice->value.building, walls, "chose %s",
               improvement_rule_name(choice->value.building));
  cr_expect_float_eq(choice->want,
                     pcity->server.adv->building_want[improvement_index(walls)],
                     1e-9, "want " ADV_WANT_PRINTF " building_want "
                     ADV_WANT_PRINTF, choice->want,
                     pcity->server.adv->building_want[improvement_index(walls)]);
  adv_free_choice(choice);
}

/* With no shields in stock, dai_process_defender_want() keeps a
 * defender within shield_stock + 40 when one exists: Musketeers (30)
 * over Alpine Troops (50). */
Test(daimilitary, defender_within_cost_limit)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct city *pcity;
  struct adv_choice choice;

  fx_tech(us, "Gunpowder");
  fx_tech(us, "Tactics");
  pcity = fx_city(us, 5, 5, "Home");
  fx_phase_begin();
  pcity->shield_stock = 0;

  adv_init_choice(&choice);
  cr_assert(dai_process_defender_want(ait, &(wld.map), us, pcity, 100,
                                      &choice, 0));
  cr_expect_leq(utype_build_shield_cost(pcity, nullptr, choice.value.utype),
                40, "chose %s", utype_rule_name(choice.value.utype));
  cr_expect_str_eq(utype_rule_name(choice.value.utype), "Musketeers");
  adv_deinit_choice(&choice);
}

/* Under Monarchy (Martial_Law_Max 3) a city with an entertainer, one
 * Phalanx, one Workers and one Settlers has room for two more police
 * units: the civilians impose no martial law. The advisor asks for a
 * defender. */
Test(daimilitary, civilians_do_not_fill_martial_law)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct city *pcity;
  struct adv_choice *choice;

  fx_tech(us, "Monarchy");
  fx_government(us, "Monarchy");
  pcity = fx_city(us, 5, 5, "Home");
  fx_unit(us, "Phalanx", 5, 5, 0);
  fx_unit(us, "Workers", 5, 5, 0);
  fx_unit(us, "Settlers", 5, 5, 0);
  us->ai_common.fuzzy = 0;
  fx_phase_begin();
  /* Make the one citizen an entertainer. */
  city_tile_iterate(&(wld.map), city_map_radius_sq_get(pcity),
                    city_tile(pcity), ptile) {
    if (tile_worked(ptile) == pcity && ptile != city_tile(pcity)) {
      tile_set_worked(ptile, nullptr);
    }
  } city_tile_iterate_end;
  pcity->specialists[DEFAULT_SPECIALIST] = city_size_get(pcity);
  city_refresh(pcity);
  cr_assert_eq(get_city_bonus(pcity, EFT_MARTIAL_LAW_MAX), 3);
  cr_assert_eq(pcity->specialists[DEFAULT_SPECIALIST], 1, "%d specialists, size %d",
               pcity->specialists[DEFAULT_SPECIALIST], city_size_get(pcity));

  choice = military_advisor_choose_build(ait, &(wld.map), us, pcity,
                                         nullptr);
  cr_expect_eq(choice->type, CT_DEFENDER, "choice type %d", choice->type);
  cr_expect_gt(choice->want, 0, "want " ADV_WANT_PRINTF, choice->want);
  adv_free_choice(choice);
}

/* With one enemy Legion able to reach the city within three turns
 * (urgency 1) and one Phalanx there, a Workers in the city does not
 * count as a second defender: the advisor asks for a defender. The
 * city is marked as the wonder city, so the advisor stops after the
 * defence choice. */
Test(daimilitary, civilians_are_not_defenders)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct city *pcity;
  struct adv_choice *choice;

  fx_war(us, them);
  pcity = fx_city(us, 5, 5, "Home");
  fx_unit(us, "Phalanx", 5, 5, 0);
  fx_unit(us, "Workers", 5, 5, 0);
  fx_unit(them, "Legion", 8, 5, 0);
  us->ai_common.fuzzy = 0;
  fx_show_all(us);
  fx_phase_begin();
  adv_data_get(us, nullptr)->wonder_city = pcity->id;

  choice = military_advisor_choose_build(ait, &(wld.map), us, pcity,
                                         nullptr);
  cr_assert_eq(def_ai_city_data(pcity, ait)->urgency, 1);
  cr_assert_eq(choice->type, CT_DEFENDER, "choice type %d", choice->type);
  cr_expect(utype_has_role(choice->value.utype, L_DEFEND_GOOD)
            || utype_has_role(choice->value.utype, L_DEFEND_OK),
            "chose %s", utype_rule_name(choice->value.utype));
  adv_free_choice(choice);
}

/* A city with a Phalanx, Writing known, and an enemy Diplomat and
 * Legion next to it asks for a defensive Diplomat, the want
 * dai_choose_diplomat_defensive() puts above soldiers. The city is
 * marked as the wonder city, so the advisor stops after the defence
 * choice. */
Test(daimilitary, defensive_diplomat_survives_defender_choice)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct city *pcity;
  struct adv_choice *choice;

  fx_war(us, them);
  fx_tech(us, "Writing");
  pcity = fx_city(us, 5, 5, "Home");
  fx_unit(us, "Phalanx", 5, 5, 0);
  fx_unit(them, "Diplomat", 6, 5, 0);
  fx_unit(them, "Legion", 6, 6, 0);
  us->ai_common.fuzzy = 0;
  fx_show_all(us);
  fx_phase_begin();
  adv_data_get(us, nullptr)->wonder_city = pcity->id;

  choice = military_advisor_choose_build(ait, &(wld.map), us, pcity,
                                         nullptr);
  cr_assert(def_ai_city_data(pcity, ait)->diplomat_threat);
  cr_assert(is_unit_choice_type(choice->type), "choice type %d",
            choice->type);
  cr_expect_str_eq(utype_rule_name(choice->value.utype), "Diplomat");
  cr_expect_float_eq(choice->want, 16000.0, 1e-9,
                     "want " ADV_WANT_PRINTF, choice->want);
  adv_free_choice(choice);
}

/* As above, with two Phalanxes and City Walls wanted and buildable:
 * the walls do not displace the defensive Diplomat either. */
Test(daimilitary, defensive_diplomat_survives_walls_choice)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct city *pcity;
  struct adv_choice *choice;
  const struct impr_type *walls;

  fx_war(us, them);
  fx_tech(us, "Writing");
  fx_tech(us, "Masonry");
  pcity = fx_city(us, 5, 5, "Home");
  fx_unit(us, "Phalanx", 5, 5, 0);
  fx_unit(us, "Phalanx", 5, 5, 0);
  fx_unit(them, "Diplomat", 6, 5, 0);
  fx_unit(them, "Legion", 6, 6, 0);
  us->ai_common.fuzzy = 0;
  fx_show_all(us);
  fx_phase_begin();
  adv_data_get(us, nullptr)->wonder_city = pcity->id;
  walls = improvement_by_rule_name("City Walls");
  pcity->server.adv->building_want[improvement_index(walls)] = 50;

  choice = military_advisor_choose_build(ait, &(wld.map), us, pcity,
                                         nullptr);
  cr_assert(def_ai_city_data(pcity, ait)->diplomat_threat);
  cr_assert(is_unit_choice_type(choice->type), "choice type %d, %s",
            choice->type, adv_choice_rule_name(choice));
  cr_expect_str_eq(utype_rule_name(choice->value.utype), "Diplomat");
  cr_expect_float_eq(choice->want, 16000.0, 1e-9,
                     "want " ADV_WANT_PRINTF, choice->want);
  adv_free_choice(choice);
}

/* An enemy Legion next to an empty city is urgent, overwhelming
 * danger: assess_danger() sets the City Walls want to 200 + urgency,
 * and a second call under the same threat leaves it there. */
Test(daimilitary, walls_want_steady_under_steady_threat)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct city *pcity;
  const struct impr_type *walls;
  adv_want *want;
  unsigned int urgency;

  fx_war(us, them);
  fx_tech(us, "Masonry");
  pcity = fx_city(us, 5, 5, "Home");
  fx_unit(them, "Legion", 6, 5, 0);
  fx_show_all(us);
  fx_phase_begin();
  walls = improvement_by_rule_name("City Walls");
  want = &pcity->server.adv->building_want[improvement_index(walls)];
  *want = 50;

  urgency = assess_danger(ait, &(wld.map), pcity, nullptr);
  cr_assert_gt(urgency, 0);
  cr_expect_float_eq(*want, 200 + urgency, 1e-9,
                     "first call: want " ADV_WANT_PRINTF ", urgency %u",
                     *want, urgency);
  urgency = assess_danger(ait, &(wld.map), pcity, nullptr);
  cr_expect_float_eq(*want, 200 + urgency, 1e-9,
                     "second call: want " ADV_WANT_PRINTF ", urgency %u",
                     *want, urgency);
}

/* Run military_advisor_choose_build() for a city of `defenders`
 * Phalanxes against `attackers` enemy Legions next to it, and return
 * the defender want, which is the danger in percent of our defence.
 * The city is marked as the wonder city, so the advisor stops after
 * the defence choice. */
static adv_want phalanxes_vs_legions(int defenders, int attackers)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *them = fx_ai_player(nullptr);
  struct city *pcity;
  struct adv_choice *choice;
  struct ai_city *city_data;
  adv_want want;
  int i;

  fx_war(us, them);
  fx_tech(us, "Bronze Working");
  fx_set_terrain(4, 5, "Forest");
  pcity = fx_city(us, 5, 5, "Home");
  /* A capital whose citizen works the Forest has shields to spare. */
  city_add_improvement(pcity, improvement_by_rule_name("Palace"));
  pcity->specialists[DEFAULT_SPECIALIST] = 0;
  tile_set_worked(fx_tile(4, 5), pcity);
  city_refresh(pcity);
  for (i = 0; i < defenders; i++) {
    fx_unit(us, "Phalanx", 5, 5, 0);
  }
  for (i = 0; i < attackers; i++) {
    fx_unit(them, "Legion", 6, 4 + i, 0);
  }
  us->ai_common.fuzzy = 0;
  fx_show_all(us);
  fx_phase_begin();
  adv_data_get(us, nullptr)->wonder_city = pcity->id;
  cr_assert_gt(pcity->surplus[O_SHIELD], 0, "prod %d usage %d waste %d spec %d",
               pcity->prod[O_SHIELD], pcity->usage[O_SHIELD], pcity->waste[O_SHIELD],
               pcity->specialists[DEFAULT_SPECIALIST]);

  choice = military_advisor_choose_build(ait, &(wld.map), us, pcity,
                                         nullptr);
  city_data = def_ai_city_data(pcity, ait);
  want = choice->want;
  cr_log_info("%s: %d Phalanx vs %d Legion: danger %d our_def %d "
              "urgency %d shield surplus %d want " ADV_WANT_PRINTF,
              game.control.name, defenders, attackers, city_data->danger,
              assess_defense_quadratic(ait, pcity), city_data->urgency,
              pcity->surplus[O_SHIELD], want);
  cr_assert_eq(choice->type, CT_DEFENDER, "choice type %d", choice->type);
  adv_free_choice(choice);

  return want;
}

/* Two Phalanxes in a city outmatch one Legion next to it: the danger
 * is below our defence, so the defender want stays under 100. */
Test(daimilitary, danger_below_defense_classic)
{
  adv_want want = phalanxes_vs_legions(2, 1);

  cr_expect_lt(want, 100, "want " ADV_WANT_PRINTF, want);
}

/* One Phalanx against three Legions is outmatched: the want reaches
 * the 200 + urgency cap. */
Test(daimilitary, danger_above_defense_classic)
{
  adv_want want = phalanxes_vs_legions(1, 3);

  cr_expect_gt(want, 200, "want " ADV_WANT_PRINTF, want);
}

TestSuite(daimilitary_civ2civ3, .init = fx_init_civ2civ3, .fini = fx_fini);

Test(daimilitary_civ2civ3, danger_below_defense)
{
  adv_want want = phalanxes_vs_legions(2, 1);

  cr_expect_lt(want, 100, "want " ADV_WANT_PRINTF, want);
}

Test(daimilitary_civ2civ3, danger_above_defense)
{
  adv_want want = phalanxes_vs_legions(1, 3);

  cr_expect_gt(want, 200, "want " ADV_WANT_PRINTF, want);
}

/* What kill_something_with() chooses to build at Home (7, 5) for a
 * virtual 'attacker' when the only enemy city, Target (13, 5) held by
 * Warriors, is across the sea at x = 8..11. 'techs' (nullptr-terminated)
 * are known to us. The caller frees the choice. */
static struct adv_choice *attack_overseas(const char *attacker,
                                          const char *const techs[])
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *us = fx_ai_player(nullptr);
  struct player *enemy = fx_ai_player(nullptr);
  struct city *home;
  struct unit *punit;
  struct adv_choice *choice;
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
  fx_city(enemy, 13, 5, "Target");
  fx_unit(enemy, "Warriors", 13, 5, 0);
  fx_show_all(us);
  fx_show_all(enemy);
  fx_phase_begin();
  dai_build_adv_init(ait, us);
  dai_build_adv_init(ait, enemy);

  punit = unit_virtual_create(us, home, fx_utype(attacker), 0);
  choice = kill_something_with(ait, &(wld.map), us, home, punit,
                               adv_new_choice());
  unit_virtual_destroy(punit);

  return choice;
}

/* Whether 'choice' builds a ferry that cannot carry 'cargo'. */
static bool builds_useless_ferry(const struct adv_choice *choice,
                                 const char *cargo)
{
  return (choice->type == CT_ATTACKER
          && utype_has_role(choice->value.utype, L_FERRYBOAT)
          && !can_unit_type_transport(choice->value.utype,
                                      utype_class(fx_utype(cargo))));
}

/* civ2civ3's Chariot is class Big Land, which a Trireme cannot carry.
 * The city must not build a Trireme for it. */
Test(daimilitary_civ2civ3, no_trireme_for_big_land)
{
  const char *const techs[] = { "Map Making", "The Wheel", nullptr };
  struct adv_choice *choice = attack_overseas("Chariot", techs);

  cr_expect(!builds_useless_ferry(choice, "Chariot"),
            "builds a %s, want " ADV_WANT_PRINTF,
            utype_rule_name(choice->value.utype), choice->want);
  adv_free_choice(choice);
}

/* A Legion is class Land: the city still builds it a Trireme. */
Test(daimilitary_civ2civ3, trireme_for_land)
{
  const char *const techs[] = { "Map Making", "Iron Working", nullptr };
  struct adv_choice *choice = attack_overseas("Legion", techs);

  cr_expect_eq(choice->type, CT_ATTACKER);
  cr_expect_eq(choice->value.utype, fx_utype("Trireme"), "builds %s",
               choice->type == CT_ATTACKER
               ? utype_rule_name(choice->value.utype) : "no unit");
  cr_expect(choice->need_boat);
  adv_free_choice(choice);
}
