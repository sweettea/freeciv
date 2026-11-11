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

/* Tests of the ruleset analysis in server/advisors/advruleset.c,
 * which runs as each ruleset is loaded. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "unittype.h"

#include "fixture.h"

TestSuite(advruleset);

/* A unit type ignores walls when no positive Defend_Bonus can apply
 * against it. In classic only Howitzer escapes City Walls and every
 * other defence bonus. */
Test(advruleset, igwall_classic)
{
  fx_init_classic();
  cr_expect(fx_utype("Howitzer")->adv.igwall, "Howitzer igwall FALSE");
  cr_expect(!fx_utype("Armor")->adv.igwall, "Armor igwall TRUE");
  cr_expect(!fx_utype("Warriors")->adv.igwall, "Warriors igwall TRUE");
  fx_fini();
}

/* In civ1 City Walls and the Great Wall do not apply against
 * Artillery and Bomber; Militia is civ1's Warriors. */
Test(advruleset, igwall_civ1)
{
  fx_init("civ1");
  cr_expect(fx_utype("Artillery")->adv.igwall, "Artillery igwall FALSE");
  cr_expect(!fx_utype("Militia")->adv.igwall, "Militia igwall TRUE");
  cr_expect(!fx_utype("Armor")->adv.igwall, "Armor igwall TRUE");
  fx_fini();
}

/* In granularity a defence bonus applies to every unit type. */
Test(advruleset, igwall_granularity)
{
  fx_init("granularity");
  unit_type_iterate(ptype) {
    cr_expect(!ptype->adv.igwall, "%s igwall TRUE", utype_rule_name(ptype));
  } unit_type_iterate_end;
  fx_fini();
}
