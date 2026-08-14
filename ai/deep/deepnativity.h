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
#ifndef FC__DEEPNATIVITY_H
#define FC__DEEPNATIVITY_H

/* common */
#include "fc_types.h"

/* Unit-class nativity / move-type classification (MOVETYPE).
 *
 * Everything here is answered from the ruleset's own terrain nativity,
 * NEVER from utype_move_type(): unit_class::move_type is assigned in
 * exactly one place and that place is the CLIENT (packhand.c), so on
 * the server the field keeps its zero-initialised value -- UMT_LAND
 * for every class in every ruleset. See deep_uclass_swims() in
 * deepnativity.c for the full diagnosis. */

bool deep_uclass_swims(const struct unit_class *pclass);
bool deep_uclass_walks(const struct unit_class *pclass);

/* "Does it float" / "does it walk", for the thirteen call sites that
 * used to ask utype_move_type(). */
bool deep_utype_is_sea(const struct unit_type *ut);
bool deep_utype_is_land(const struct unit_type *ut);

/* Ruleset capability tests shared with the survival wave
 * (deepcombat.c): can a unit of this type strike a city with no
 * visible approach, and how far does it reach doing it. */
bool deep_utype_is_airborne(const struct unit_type *ut);
int deep_utype_air_reach(const struct unit_type *ut);

#endif /* FC__DEEPNATIVITY_H */
