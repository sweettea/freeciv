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

/* Unit-class nativity / move-type classification (MOVETYPE), moved
 * verbatim from deepdefence.c so the helpers can be linked into an L0
 * unit test against common/ alone. Pure move: no behaviour change. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "shared.h"

/* common */
#include "movement.h"
#include "terrain.h"
#include "unittype.h"

/* ai/deep */
#include "deepnativity.h"

/**********************************************************************//**
  Can units of this class stand on water / on land? Asked of the
  ruleset's own terrain nativity, NOT of utype_move_type().

  The derived move type is unusable for this question, and the
  diagnosis first recorded here was far too narrow. It was written as
  "civ2civ3 reports the TRIREME class as UMT_LAND", i.e. one class in
  one ruleset. It is EVERY class, in EVERY ruleset, in every
  server-side caller:

    $ grep -rn "set_unit_move_type" --include=*.c .
    common/unittype.c:2961:void set_unit_move_type(struct unit_class *)
    client/packhand.c:3688:    set_unit_move_type(pclass);

  `unit_class::move_type` is assigned in exactly one place and that
  place is the CLIENT, on receipt of the ruleset packet. The server
  never calls it, so the field keeps its zero-initialised value, which
  is UMT_LAND (unittype.h:133, SPECENUM_VALUE0). A one-shot dump of
  all nine civ2civ3 classes on a resumed save read move_type=0 for
  every one of them, Sea and Air included
  (findings/venues-2026-08-12.md section 0). So server-side no
  `== UMT_SEA` test can ever be true and no `!= UMT_LAND` test can
  ever be false, and there is no ruleset in which that is not so.

  Terrain nativity cannot drift that way: the Trireme class is
  `native_to` lake and ocean in both shipped rulesets, which is what
  "it floats" means. is_native_to_class() with no extras asks exactly
  that, and it is the same accessor movement.c uses.
**************************************************************************/
bool deep_uclass_swims(const struct unit_class *pclass)
{
  terrain_type_iterate(pterrain) {
    if (is_ocean(pterrain) && is_native_to_class(pclass, pterrain, NULL)) {
      return TRUE;
    }
  } terrain_type_iterate_end;

  return FALSE;
}

bool deep_uclass_walks(const struct unit_class *pclass)
{
  terrain_type_iterate(pterrain) {
    if (!is_ocean(pterrain) && is_native_to_class(pclass, pterrain, NULL)) {
      return TRUE;
    }
  } terrain_type_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  The two questions the rest of deep actually asked utype_move_type(),
  answered from the same terrain nativity the two helpers above use.

  WHY THE MAPPING IS EXACTLY THIS. set_unit_move_type() derives the
  field from nativity and from nothing else: land_moving | sea_moving
  -> UMT_LAND / UMT_SEA / UMT_BOTH, and the enum has only those three
  values (unittype.h:133-137). THERE IS NO UMT_AIR: a ruleset spells
  flight as a class native to both, which is why
  deep_utype_is_airborne() below tests walks && swims. So

      UMT_LAND  ==  walks && !swims
      UMT_SEA   ==  swims && !walks
      UMT_BOTH  ==  walks &&  swims   (flight)

  and these two reproduce what the accessor WOULD have returned had
  the server ever computed it, rather than substituting a looser test.
  In particular a helicopter is neither sea nor land under either arm,
  which is what the old `== UMT_SEA` / `!= UMT_LAND` code meant and
  what its air-aware callers (the wave's flies_in arm, the carrier
  test) already assume.

  UNCONDITIONAL, and deliberately not behind a knob. The comparisons
  these replace read an enum the server never sets; there is no
  defensible "off" value for a field that is uninitialised, so there
  is nothing for a switch to select between. Thirteen call sites move
  at once and the branch is measured against its base before it
  merges -- it is not assumed correct because it is correct in
  principle.

  NOT CACHED, deliberately. is_native_to_class() with no extras is a
  single BV_ISSET, so each call is bounded by the terrain count (~14
  in civ2civ3) of trivial tests with an early exit; a per-class memo
  would need invalidation on ruleset reload and is not worth that risk
  until a profile says so.
**************************************************************************/
bool deep_utype_is_sea(const struct unit_type *ut)
{
  return (deep_uclass_swims(utype_class(ut))
          && !deep_uclass_walks(utype_class(ut)));
}

bool deep_utype_is_land(const struct unit_type *ut)
{
  return (deep_uclass_walks(utype_class(ut))
          && !deep_uclass_swims(utype_class(ut)));
}

/**********************************************************************//**
  Can a unit of this type strike a city with no visible approach --
  drop onto it, or fly to it and take it?

  Expressed from ruleset CAPABILITY, never from a named-unit list: an
  action whose result is a paradrop, or a class that is native to land
  AND ocean (which is how a ruleset says "it flies": movement.c derives
  the move type from native terrain, and there is no UMT_AIR) and that
  can conquer a city. Classic's Paratroopers match the first and its
  Helicopter the second; a ruleset that spells either capability
  differently still matches, and a ruleset with neither reports the
  class dead, which is the correct answer for that ruleset.
**************************************************************************/
bool deep_utype_is_airborne(const struct unit_type *ut)
{
  return (utype_can_do_action_result(ut, ACTRES_PARADROP)
          || utype_can_do_action_result(ut, ACTRES_PARADROP_CONQUER)
          || (deep_uclass_walks(utype_class(ut))
              && deep_uclass_swims(utype_class(ut))
              && utype_can_take_over(ut)));
}

/**********************************************************************//**
  How far such a unit reaches in one flight, in tiles: its drop range
  or its move rate, whichever is longer. The same number the garrison
  floor's airborne class is sized on, so the floor that INSURES
  against air and the survival wave that would SEE it agree by
  construction.
**************************************************************************/
int deep_utype_air_reach(const struct unit_type *ut)
{
  return MAX(ut->paratroopers_range, ut->move_rate / SINGLE_MOVE);
}
