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

/* Tech upkeep as player_tech_upkeep() computes it. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

/* common */
#include "effects.h"
#include "game.h"
#include "player.h"
#include "research.h"
#include "team.h"
#include "tech.h"

/* ai */
#include "difficulty.h"

#include "fixture.h"

struct upkeep_case {
  char ruleset[16];
  bool ai;           /* The player whose upkeep is checked is a novice AI. */
  bool team;         /* It shares research with a human team mate. */
};

ParameterizedTestParameters(research, tech_upkeep_full_tree)
{
  static struct upkeep_case params[] = {
    { "civ2civ3", FALSE, FALSE }, { "civ2civ3", TRUE, FALSE },
    { "goldkeep", FALSE, FALSE }, { "goldkeep", TRUE, FALSE },
    { "civ2civ3", TRUE, TRUE },   { "goldkeep", TRUE, TRUE },
  };

  return cr_make_param_array(struct upkeep_case, params,
                             sizeof(params) / sizeof(params[0]));
}

/**********************************************************************//**
  A human player, or a novice AI one: a novice AI's science_cost is
  250, where a human's factor is 1 and other AI levels' 100.
**************************************************************************/
static struct player *upkeep_player(bool ai)
{
  struct player *pplayer;

  if (!ai) {
    return fx_player();
  }
  pplayer = fx_ai_player(nullptr);
  set_ai_level_directer(pplayer, AI_LEVEL_NOVICE);
  cr_assert_eq(pplayer->ai_common.science_cost, 250,
               "novice science_cost %d", pplayer->ai_common.science_cost);

  return pplayer;
}

/**********************************************************************//**
  The whole tech tree known, under the nation's starting government
  (Despotism), with three cities. The rulesets' game.ruleset documents
  the upkeep as
    ; "Basic"  - upkeep is calculated as:
    ;     <Cost of technology> / tech_upkeep_divider - <Free tech upkeep>
    ; "Cities" - upkeep is calculated like "Basic", but multiplied by
    ;            number of cities
  where the cost of a technology is its ruleset cost scaled by
  Tech_Cost_Pct and, for an AI, its science_cost, as
  research_total_bulbs_required() scales it. Pooled research is
  charged to the team members in equal shares, at the members' mean
  cost factor.
**************************************************************************/
ParameterizedTest(struct upkeep_case *p, research, tech_upkeep_full_tree,
                  .fini = fx_fini)
{
  struct player *pplayer, *mate = nullptr;
  struct research *presearch;
  double bulbs = 0.0, factor = 0.0, expected;
  int members = 0, free_upkeep, upkeep, ntechs = 0;

  fx_init(p->ruleset);
  if (p->team) {
    game.info.team_pooled_research = TRUE;
  }
  pplayer = upkeep_player(p->ai);
  if (p->team) {
    mate = upkeep_player(FALSE);
    cr_assert(team_add_player(mate, pplayer->team), "no team");
  }
  fx_city(pplayer, 3, 3, "One");
  fx_city(pplayer, 10, 3, "Two");
  fx_city(pplayer, 3, 10, "Three");

  presearch = research_get(pplayer);
  if (p->team) {
    cr_assert_eq(presearch, research_get(mate), "research not pooled");
  }
  advance_iterate(padvance) {
    if (advance_number(padvance) != A_NONE) {
      research_invention_set(presearch, advance_number(padvance),
                             TECH_KNOWN);
    }
  } advance_iterate_end;
  research_update(presearch);

  cr_assert(game.info.tech_cost_style != TECH_COST_CIV1CIV2,
            "%s uses preset costs", p->ruleset);
  advance_iterate(padvance) {
    if (research_invention_state(presearch, advance_number(padvance))
        == TECH_KNOWN) {
      bulbs += padvance->cost;
      ntechs++;
    }
  } advance_iterate_end;
  research_players_iterate(presearch, member) {
    factor += get_player_bonus(member, EFT_TECH_COST_PCT) / 100.0
              * (is_ai(member) ? member->ai_common.science_cost / 100.0
                               : 1.0);
    members++;
  } research_players_iterate_end;
  cr_assert_eq(members, p->team ? 2 : 1, "%d research members", members);

  free_upkeep = get_player_bonus(pplayer, EFT_TECH_UPKEEP_FREE);
  expected = bulbs * factor / members * game.info.sciencebox / 100.0
             / members / game.info.tech_upkeep_divider - free_upkeep;
  if (game.info.tech_upkeep_style == TECH_UPKEEP_PER_CITY) {
    expected *= city_list_size(pplayer->cities);
  }
  cr_assert_gt(expected, 0.0, "nothing to pay");

  upkeep = player_tech_upkeep(pplayer);
  /* Truncated as the upkeep is, allowing for floating-point order. */
  cr_expect_eq(upkeep, (int) (expected + 1e-6),
               "%s %s%s, %d techs costing %.0f, factor %.2f, divider %d, "
               "free %d, %d cities: upkeep %d, expected %d (%.2f)",
               p->ruleset, p->ai ? "novice AI" : "human",
               p->team ? " with a human team mate" : "", ntechs, bulbs,
               factor / members, game.info.tech_upkeep_divider,
               free_upkeep, city_list_size(pplayer->cities), upkeep,
               (int) (expected + 1e-6), expected);
}
