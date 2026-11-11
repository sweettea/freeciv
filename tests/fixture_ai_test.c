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

/* Checks that fixture AI players are set up far enough for the default
 * AI's and the advisors' entry points to run on them. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "ai.h"
#include "city.h"
#include "effects.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "unit.h"

/* server */
#include "aiiface.h"

/* server/advisors */
#include "advdata.h"

/* ai/default */
#include "daidata.h"
#include "daidiplomacy.h"
#include "daieffects.h"
#include "daiplayer.h"
#include "daiunit.h"

#include "fixture.h"

#define NUM_AI 3

TestSuite(fixture_ai, .init = fx_init_classic, .fini = fx_fini);

/* N players of the default AI type are AI-controlled, have the AI's
 * per-player data, and get advisor and AI phase data. */
Test(fixture_ai, players_have_ai_data)
{
  struct player *p[NUM_AI];
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  int i;

  for (i = 0; i < NUM_AI; i++) {
    p[i] = fx_ai_player(nullptr);
  }
  fx_phase_begin();

  for (i = 0; i < NUM_AI; i++) {
    bool close;
    struct adv_data *adv;

    cr_expect(is_ai(p[i]));
    cr_expect_eq(p[i]->ai, ait);
    cr_expect_eq(p[i]->ai_common.skill_level, game.info.skill_level);
    cr_expect_not_null(def_ai_player_data(p[i], ait));
    cr_expect(def_ai_player_data(p[i], ait)->phase_initialized);
    adv = adv_data_get(p[i], &close);
    cr_expect_not_null(adv);
    cr_expect(adv->phase_is_initialized);
    cr_expect_not_null(dai_plr_data_get(ait, p[i], nullptr));
  }
}

/* dai_effect_value() rates a happiness effect in an AI city. */
Test(fixture_ai, effect_value)
{
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 5, 5, "Capital");
  struct effect *peffect = effect_new(EFT_MAKE_HAPPY, 2, nullptr);
  bool close;
  adv_want v;

  fx_phase_begin();
  v = dai_effect_value(pplayer, adv_data_get(pplayer, &close), pcity,
                       TRUE, 1, peffect, 1, NUM_AI);
  cr_expect_gt(v, 0, "Make_Happy 2 valued at " ADV_WANT_PRINTF, v);
}

/* find_something_to_kill() sends an Armor at the enemy's empty city
 * three tiles away. */
Test(fixture_ai, find_something_to_kill)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *a = fx_ai_player(nullptr);
  struct player *b = fx_ai_player(nullptr);
  struct city *target;
  struct unit *armor;
  struct tile *dest = nullptr;
  adv_want want;

  fx_war(a, b);
  fx_city(a, 2, 5, "Home");
  target = fx_city(b, 9, 5, "Target");
  armor = fx_unit(a, "Armor", 6, 5, 0);
  fx_show_all(a);
  fx_show_all(b);
  fx_phase_begin();

  want = find_something_to_kill(ait, a, armor, &dest, nullptr, nullptr,
                                nullptr, nullptr, nullptr);
  cr_expect_gt(want, 0, "want " ADV_WANT_PRINTF, want);
  cr_expect_eq(dest, city_tile(target));
}

/* The diplomacy phase runs for AI players in contact: two AIs at war
 * with nothing between them agree to stop. */
Test(fixture_ai, diplomacy_actions)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *a = fx_ai_player(nullptr);
  struct player *b = fx_ai_player(nullptr);

  fx_war(a, b);
  fx_phase_begin();

  dai_diplomacy_actions(ait, a);
  dai_diplomacy_actions(ait, b);
  cr_expect_not_null(dai_diplomacy_get(ait, a, b));
  cr_expect(!pplayers_at_war(a, b), "still %s",
            diplstate_type_name(player_diplstate_get(a, b)->type));
}
