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
#ifndef FC__DEEPEFFECTS_H
#define FC__DEEPEFFECTS_H

/* common */
#include "effects.h"
#include "fc_types.h"

/* What an improvement WOULD provide if it were built here, evaluated
 * against a requirement context that knows the candidate's own building
 * flags as well as its identity.
 *
 * Same signature and same currency as common/effects.c's
 * get_potential_improvement_bonus(), and identical to it on every
 * effect a ruleset gates on a BUILDING. It differs on the effects a
 * ruleset gates on a building FLAG, which the stock accessor prices at
 * zero for the same reason it would price a building-gated effect at
 * zero without its own special case: the city does not have the
 * candidate yet, so "a building with flag X stands in this city" is
 * FALSE at evaluation time and the effect drops out as unreachable.
 * (M4b findings section 5a: measured 363 times in one civ2civ3 game on
 * the Barracks, whose HP_Regen and Veteran_Build are both flag-gated.
 * daieffects.c:479 carries the matching FIXME.) */
int deep_potential_improvement_bonus(const struct impr_type *pimprove,
                                     const struct city *pcity,
                                     enum effect_type effect_type,
                                     const enum req_problem_type prob_type,
                                     bool consider_multipliers);

/* The same question asked ON BEHALF OF ONE UNIT TYPE, for the effects
 * a ruleset gates on what the unit is as well as on what the building
 * is: civ2civ3's Barracks pair is "BuildingFlag Barracks, City" AND
 * "UnitClassFlag Barracks, Local" (effects.ruleset:1824, :1835), and a
 * local unit-class requirement with no unit in the context is UNKNOWN,
 * which RPT_CERTAIN discards. A credit that is about particular units
 * -- the damaged ones standing here, the defender this city would
 * build -- knows which unit it means and can say so. `utype` nullptr
 * is exactly deep_potential_improvement_bonus(). */
int deep_potential_improvement_bonus_for(const struct impr_type *pimprove,
                                         const struct city *pcity,
                                         const struct unit_type *utype,
                                         enum effect_type effect_type,
                                         const enum req_problem_type prob_type,
                                         bool consider_multipliers);

/* The test the accessor above is built on, exported for the one other
 * place deep walks an improvement's effect list itself
 * (deep_output_bonus_pct(), which needs the OutputType requirement
 * rather than a scalar): would building this improvement, by itself,
 * satisfy this requirement's source? TRUE when the requirement names
 * the improvement, and TRUE when it names a building flag the
 * improvement carries. The caller reads `preq->present` itself. */
bool deep_req_source_is_candidate(const struct requirement *preq,
                                  const struct impr_type *pimprove);

#endif /* FC__DEEPEFFECTS_H */
