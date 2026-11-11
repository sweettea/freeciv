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
#include "plrhand.h"

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

/* Whether pplayer has put a clause of `type` on the table with
 * aplayer. */
static bool proposed(struct player *pplayer, struct player *aplayer,
                     enum clause_type type)
{
  struct treaty *ptreaty = find_treaty(pplayer, aplayer);

  if (ptreaty == nullptr) {
    return FALSE;
  }
  clause_list_iterate(ptreaty->clauses, pclause) {
    if (pclause->type == type) {
      return TRUE;
    }
  } clause_list_iterate_end;

  return FALSE;
}

/* An AI at war with a human offers a cease-fire, also when it has an
 * ally, listed before the enemy, who is at peace with that enemy. */
Test(daidiplomacy, ceasefire_offered_past_an_idle_ally)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *a = fx_ai_player(nullptr);
  struct player *ally = fx_ai_player(nullptr);
  struct player *enemy = fx_player();

  fx_diplstate(a, ally, DS_ALLIANCE);
  fx_war(a, enemy);
  fx_diplstate(ally, enemy, DS_PEACE);
  fx_phase_begin();
  cr_assert_lt(player_index(ally), player_index(enemy));

  dai_diplomacy_actions(ait, a);
  cr_expect(proposed(a, enemy, CLAUSE_CEASEFIRE),
            "no cease-fire offered to the enemy");
}

/* An AI that plans no war offers peace to its only, well-liked,
 * cease-fire partner when the cease-fire is about to run out. */
Test(daidiplomacy, peace_offered_to_least_loved_when_no_war_planned)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *a = fx_ai_player(nullptr);
  struct player *b = fx_player();

  old_contact(a, b, DS_CEASEFIRE);
  player_diplstate_get(a, b)->turns_left = 3;
  player_diplstate_get(b, a)->turns_left = 3;
  fx_phase_begin();
  a->ai_common.love[player_index(b)] = MAX_AI_LOVE * 6 / 10;

  dai_diplomacy_actions(ait, a);
  cr_assert_eq(dai_diplomacy_get(ait, a, b)->countdown, -1);
  cr_expect(proposed(a, b, CLAUSE_PEACE), "no peace offered");
}

/* An AI calls on an ally that stays at peace with its enemy, and its
 * patience with that ally runs down over the turns. */
Test(daidiplomacy, ally_at_peace_with_our_enemy_is_called_to_arms)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *a = fx_ai_player(nullptr);
  struct player *ally = fx_ai_player(nullptr);
  struct player *enemy = fx_player();
  struct ai_dip_intel *adip;
  int turn;

  fx_diplstate(a, ally, DS_ALLIANCE);
  fx_war(a, enemy);
  fx_diplstate(ally, enemy, DS_PEACE);
  fx_phase_begin();
  adip = dai_diplomacy_get(ait, a, ally);

  for (turn = 0; turn < 30 && adip->ally_patience == 0; turn++) {
    dai_diplomacy_actions(ait, a);
  }
  cr_expect_lt(adip->ally_patience, 0,
               "patience %d after %d turns", adip->ally_patience, turn);
}

/* An AI thinks less of a player who breaks a peace treaty, also when
 * the victim is a human player. */
Test(daidiplomacy, war_on_a_human_costs_love)
{
  struct player *watcher = fx_ai_player(nullptr);
  struct player *violator = fx_player();
  struct player *victim = fx_player();
  int love;

  fx_diplstate(violator, victim, DS_PEACE);
  fx_diplstate(watcher, violator, DS_PEACE);
  fx_phase_begin();
  love = watcher->ai_common.love[player_index(violator)];

  handle_diplomacy_cancel_pact(violator, player_number(victim),
                               clause_type_invalid());
  cr_assert(pplayers_at_war(violator, victim), "%s",
            diplstate_type_name(player_diplstate_get(violator,
                                                     victim)->type));
  cr_expect_lt(watcher->ai_common.love[player_index(violator)], love,
               "love %d, was %d",
               watcher->ai_common.love[player_index(violator)], love);
}

/* At war, an AI accepts a tech it is researching as a pure gift. */
Test(daidiplomacy, tech_gift_accepted_at_war)
{
  struct player *a = fx_ai_player(nullptr);
  struct player *h = fx_player();
  struct advance *padvance = advance_by_rule_name("Alphabet");
  struct treaty *ptreaty;

  cr_assert_not_null(padvance);
  fx_war(a, h);
  fx_tech(h, "Alphabet");
  research_get(a)->researching = advance_number(padvance);
  fx_phase_begin();

  handle_diplomacy_init_meeting_req(h, player_number(a));
  handle_diplomacy_create_clause_req(h, player_number(a), player_number(h),
                                     CLAUSE_ADVANCE,
                                     advance_number(padvance));
  ptreaty = find_treaty(h, a);
  cr_assert_not_null(ptreaty, "no meeting");
  cr_assert_eq(clause_list_size(ptreaty->clauses), 1);
  cr_expect(ptreaty->plr0 == a ? ptreaty->accept0 : ptreaty->accept1,
            "AI did not accept the gift (tech_cost_style %d)",
            game.info.tech_cost_style);
}

/* A big gift raises the AI's love, but never past MAX_AI_LOVE. */
Test(daidiplomacy, gift_love_stays_in_range)
{
  struct player *a = fx_ai_player(nullptr);
  struct player *h = fx_player();
  int love;

  fx_diplstate(a, h, DS_PEACE);
  fx_phase_begin();
  h->economic.gold = 5000;
  love = a->ai_common.love[player_index(h)];

  handle_diplomacy_init_meeting_req(h, player_number(a));
  handle_diplomacy_create_clause_req(h, player_number(a), player_number(h),
                                     CLAUSE_GOLD, 5000);
  handle_diplomacy_accept_treaty_req(h, player_number(a));
  cr_assert_null(find_treaty(h, a), "treaty not concluded");
  cr_assert_eq(h->economic.gold, 0);

  cr_expect_gt(a->ai_common.love[player_index(h)], love);
  cr_expect_leq(a->ai_common.love[player_index(h)], MAX_AI_LOVE,
                "love %d", a->ai_common.love[player_index(h)]);
}

/* A human player in away mode keeps the shared vision they gave to a
 * peace partner: away mode does no diplomacy. */
Test(daidiplomacy, away_mode_keeps_shared_vision)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *h = fx_player();
  struct player *friend = fx_player();

  fx_diplstate(h, friend, DS_PEACE);
  give_shared_vision(h, friend);
  fx_phase_begin();
  player_set_to_ai_mode(h, AI_LEVEL_AWAY);
  cr_assert(is_ai(h));
  cr_assert(has_handicap(h, H_AWAY));

  dai_diplomacy_actions(ait, h);
  cr_expect(gives_shared_vision(h, friend), "shared vision removed");
}
