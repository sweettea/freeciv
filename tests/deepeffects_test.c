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

/* L0 unit test for deep's candidate-aware effect accessor.
 *
 * THE DEFECT IT PINS: an effect a ruleset gates on a building FLAG
 * rather than on the building itself was priced at ZERO by every deep
 * credit that asked "what would this building give me" -- the growth,
 * health and content credits, and both halves of M4b item 4, whose
 * `deep-barracks` counter was a literal 0.00 in a 20-game A/B for
 * exactly this reason (M4b findings 5a). civ2civ3's Barracks carries
 * HP_Regen and Veteran_Build behind `"BuildingFlag", "Barracks",
 * "City"`, and the city does not have a barracks yet at the moment the
 * question is asked, so the requirement reads FALSE and the effect
 * drops out as unreachable.
 *
 * Both accessors are run over the same effect here, so the test states
 * the defect and the fix side by side: whatever the stock accessor
 * does, deep's must see the effect the candidate itself would switch
 * on. The stock reading is asserted too, so that if common/ ever fixes
 * this the test fails loudly rather than going quietly redundant. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

/* common */
#include "effects.h"
#include "game.h"
#include "improvement.h"
#include "requirements.h"
#include "unittype.h"

/* ai/deep */
#include "deepeffects.h"

/* Two improvements: one carrying the flag the effects are gated on
 * (the "Barracks" of every in-tree ruleset), one carrying nothing. */
#define IMPR_FLAGGED 0
#define IMPR_PLAIN 1
#define TEST_FLAG IF_USER_FLAG_1

/* Two unit classes: one that benefits from the building (the "Barracks"
 * class flag every in-tree ruleset puts on its ground units) and one
 * that does not, with one unit type in each. */
#define UCLASS_BENEFITS 0
#define UCLASS_OTHER 1
#define UTYPE_BENEFITS 0
#define UTYPE_OTHER 1
#define TEST_UCFLAG UCF_USER_FLAG_1

static int failures = 0;

#define CHECK(cond, ...)                                                \
  do {                                                                  \
    if (!(cond)) {                                                      \
      failures++;                                                       \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);              \
      fprintf(stderr, __VA_ARGS__);                                     \
      fprintf(stderr, "\n");                                            \
    }                                                                   \
  } while (0)

/**********************************************************************//**
  A ruleset of two buildings and no city, which is all the accessors
  need: with no city, a city-range flag requirement is UNKNOWN rather
  than FALSE, and RPT_CERTAIN discards it just as the live game's FALSE
  does. The reading under test is therefore the same one the game takes,
  without a map, a player or a city to build.
**************************************************************************/
static void setup_ruleset(void)
{
  struct impr_type *pimpr;
  struct unit_class *pclass;
  struct unit_type *putype;

  improvements_init();
  game.control.num_impr_types = 2;

  pimpr = improvement_by_number(IMPR_FLAGGED);
  BV_CLR_ALL(pimpr->flags);
  BV_SET(pimpr->flags, TEST_FLAG);

  pimpr = improvement_by_number(IMPR_PLAIN);
  BV_CLR_ALL(pimpr->flags);

  unit_classes_init();
  game.control.num_unit_classes = 2;
  pclass = uclass_by_number(UCLASS_BENEFITS);
  BV_CLR_ALL(pclass->flags);
  BV_SET(pclass->flags, TEST_UCFLAG);
  pclass = uclass_by_number(UCLASS_OTHER);
  BV_CLR_ALL(pclass->flags);

  unit_types_init();
  game.control.num_unit_types = 2;
  putype = utype_by_number(UTYPE_BENEFITS);
  putype->uclass = uclass_by_number(UCLASS_BENEFITS);
  putype = utype_by_number(UTYPE_OTHER);
  putype->uclass = uclass_by_number(UCLASS_OTHER);

  ruleset_cache_init();
}

/**********************************************************************//**
  Register one effect of `type` and value `value`, gated on one
  requirement. The effect is returned so a second requirement can be
  hung on it.
**************************************************************************/
static struct effect *add_gated_effect(enum effect_type type, int value,
                                       int req_kind, int req_range,
                                       bool present, int req_value)
{
  struct effect *peffect = effect_new(type, value, nullptr);

  effect_req_append(peffect, req_from_values(req_kind, req_range,
                                             FALSE, present, FALSE,
                                             req_value));

  return peffect;
}

int main(void)
{
  const struct impr_type *flagged, *plain;
  int stock, deep;

  setup_ruleset();
  flagged = improvement_by_number(IMPR_FLAGGED);
  plain = improvement_by_number(IMPR_PLAIN);

  /* The civ2civ3 shape: HP_Regen 100 behind a city-range building-flag
   * requirement (effects.ruleset:1836). */
  add_gated_effect(EFT_HP_REGEN, 100, VUT_IMPR_FLAG, REQ_RANGE_CITY,
                   TRUE, TEST_FLAG);

  stock = get_potential_improvement_bonus(flagged, nullptr, EFT_HP_REGEN,
                                          RPT_CERTAIN, FALSE);
  deep = deep_potential_improvement_bonus(flagged, nullptr, EFT_HP_REGEN,
                                          RPT_CERTAIN, FALSE);

  CHECK(stock == 0,
        "the stock accessor now reads a flag-gated effect (%d); if"
        " common/ has been fixed, deep's accessor should be retired"
        " rather than left to duplicate it", stock);
  CHECK(deep == 100,
        "a flag-gated HP_Regen 100 on a candidate carrying the flag"
        " credits %d, expected 100", deep);

  /* A candidate WITHOUT the flag is owed nothing -- the effect is not
   * even in its list, so this also checks the fix did not turn the
   * accessor into "any effect of this type". */
  CHECK(deep_potential_improvement_bonus(plain, nullptr, EFT_HP_REGEN,
                                         RPT_CERTAIN, FALSE) == 0,
        "a candidate without the flag was credited for the flag-gated"
        " effect");

  /* A building-gated effect must read exactly as it always did: this
   * is the half of the accessor that is stock's, unchanged. */
  add_gated_effect(EFT_GROWTH_FOOD, 25, VUT_IMPROVEMENT, REQ_RANGE_CITY,
                   TRUE, IMPR_FLAGGED);
  stock = get_potential_improvement_bonus(flagged, nullptr,
                                          EFT_GROWTH_FOOD, RPT_CERTAIN,
                                          FALSE);
  deep = deep_potential_improvement_bonus(flagged, nullptr,
                                          EFT_GROWTH_FOOD, RPT_CERTAIN,
                                          FALSE);
  CHECK(stock == 25 && deep == stock,
        "building-gated Growth_Food 25 reads stock %d, deep %d --"
        " the two must agree on every effect gated on a BUILDING",
        stock, deep);

  /* An effect the candidate would switch OFF is a debit, the same way
   * stock charges a "must not have building Y" requirement. */
  add_gated_effect(EFT_HEALTH_PCT, 30, VUT_IMPR_FLAG, REQ_RANGE_CITY,
                   FALSE, TEST_FLAG);
  deep = deep_potential_improvement_bonus(flagged, nullptr,
                                          EFT_HEALTH_PCT, RPT_CERTAIN,
                                          FALSE);
  CHECK(deep == -30,
        "an effect requiring the flag ABSENT credits %d for a candidate"
        " that carries it, expected -30", deep);

  /* THE FULL civ2civ3 BARRACKS SHAPE: gated on the building's flag AND
   * on the unit's class (effects.ruleset:1824). Unit-blind, a local
   * unit-class requirement is UNKNOWN and RPT_CERTAIN discards it, so
   * the scalar read is still zero and the credit has to say which unit
   * it means. */
  {
    struct effect *peffect
      = add_gated_effect(EFT_VETERAN_BUILD, 1, VUT_IMPR_FLAG,
                         REQ_RANGE_CITY, TRUE, TEST_FLAG);

    effect_req_append(peffect, req_from_values(VUT_UCFLAG, REQ_RANGE_LOCAL,
                                               FALSE, TRUE, FALSE,
                                               TEST_UCFLAG));
  }

  CHECK(deep_potential_improvement_bonus(flagged, nullptr,
                                         EFT_VETERAN_BUILD, RPT_CERTAIN,
                                         FALSE) == 0,
        "a unit-class-gated effect read with no unit in the context is"
        " UNKNOWN, and RPT_CERTAIN must keep discarding it");
  CHECK(deep_potential_improvement_bonus_for(flagged, nullptr,
                                             utype_by_number(UTYPE_BENEFITS),
                                             EFT_VETERAN_BUILD, RPT_CERTAIN,
                                             FALSE) == 1,
        "Veteran_Build 1 is owed to a unit of the class the ruleset"
        " names, and was not paid");
  CHECK(deep_potential_improvement_bonus_for(flagged, nullptr,
                                             utype_by_number(UTYPE_OTHER),
                                             EFT_VETERAN_BUILD, RPT_CERTAIN,
                                             FALSE) == 0,
        "a unit of the wrong class was credited for a class-gated"
        " effect");

  ruleset_cache_free();
  improvements_free();

  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return EXIT_FAILURE;
  }

  printf("deepeffects: all checks passed\n");

  return EXIT_SUCCESS;
}
