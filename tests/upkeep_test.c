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

/* Unit upkeep as utype_upkeep_cost() computes it from the ruleset's
 * uk_* values and the Upkeep_Pct effect. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

/* common */
#include "city.h"
#include "effects.h"
#include "requirements.h"
#include "unit.h"
#include "unittype.h"

#include "fixture.h"

struct upkeep_case {
  char utype[32];
  int extra_pct;
};

ParameterizedTestParameters(upkeep, pct_scales_shield_upkeep)
{
  static struct upkeep_case params[] = {
    { "Hunters", 0 },   { "Hunters", 100 }, { "Hunters", 50 },
    { "Hunters", -50 }, { "Hunters", 25 },  { "Warriors", 50 },
  };

  return cr_make_param_array(struct upkeep_case, params,
                             sizeof(params) / sizeof(params[0]));
}

/**********************************************************************//**
  granularity's only Upkeep_Pct effect is 100 with no requirements, and
  its Hunters cost 50 shields. Add a shield-only Upkeep_Pct effect, the
  way the ruleset loader does, so the summed percentage is 100 +
  extra_pct. README.effects: "Unit upkeep is amount percents.", so the
  upkeep is uk_shield * sum / 100, rounded down as integer upkeep is.
  An extra_pct of 0 (sum 100) and 100 (sum 200) are controls.
**************************************************************************/
ParameterizedTest(struct upkeep_case *p, upkeep, pct_scales_shield_upkeep,
                  .fini = fx_fini)
{
  const struct unit_type *ut;
  struct player *owner;
  struct unit *punit;
  int base, sum, cost;

  fx_init("granularity");
  ut = fx_utype(p->utype);
  owner = fx_player();
  punit = fx_unit(owner, p->utype, 5, 5, 0);
  base = ut->upkeep[O_SHIELD];
  cr_assert_gt(base, 1, "%s shield upkeep %d", p->utype, base);

  if (p->extra_pct != 0) {
    struct effect *peffect = effect_new(EFT_UPKEEP_PCT, p->extra_pct,
                                        nullptr);

    effect_req_append(peffect, req_from_str("OutputType", "Local", FALSE,
                                            TRUE, FALSE, "Shield"));
  }
  sum = get_target_bonus_effects(nullptr,
                                 &(const struct req_context) {
                                   .player = owner,
                                   .output = get_output_type(O_SHIELD),
                                   .unittype = ut,
                                   .unit = punit,
                                   .tile = unit_tile(punit),
                                 },
                                 nullptr, EFT_UPKEEP_PCT);
  cr_assert_eq(sum, 100 + p->extra_pct, "Upkeep_Pct sums to %d", sum);

  cost = unit_upkeep_cost(punit, O_SHIELD);
  cr_expect_eq(cost, base * sum / 100,
               "%s, uk_shield %d, Upkeep_Pct %d: upkeep %d, expected %d",
               p->utype, base, sum, cost, base * sum / 100);
}

/**********************************************************************//**
  A shipped sum of 200, unchanged by any test effect: classic's
  Republic adds Upkeep_Pct 100 for food to the base 100, so Settlers
  eat twice their uk_food.
**************************************************************************/
Test(upkeep, classic_republic_settlers_food, .fini = fx_fini)
{
  struct player *owner;
  struct unit *punit;
  int base;

  fx_init_classic();
  owner = fx_player();
  fx_government(owner, "Republic");
  punit = fx_unit(owner, "Settlers", 5, 5, 0);
  base = unit_type_get(punit)->upkeep[O_FOOD];
  cr_assert_eq(base, 1, "Settlers uk_food %d", base);
  cr_expect_eq(unit_upkeep_cost(punit, O_FOOD), 2 * base,
               "Settlers food upkeep %d under Republic, expected %d",
               unit_upkeep_cost(punit, O_FOOD), 2 * base);
  cr_expect_eq(unit_upkeep_cost(punit, O_SHIELD),
               unit_type_get(punit)->upkeep[O_SHIELD],
               "Settlers shield upkeep %d, expected uk_shield %d",
               unit_upkeep_cost(punit, O_SHIELD),
               unit_type_get(punit)->upkeep[O_SHIELD]);
}
