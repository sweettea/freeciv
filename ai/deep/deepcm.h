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
#ifndef FC__DEEPCM_H
#define FC__DEEPCM_H

/* utility */
#include "support.h"            /* bool type */

/* common */
#include "fc_types.h"

void deep_set_city_manager_params(struct ai_type *ait,
                                  struct player *pplayer);

/* War-footing surplus minus the surplus the city would have had under
 * the posture it would otherwise run, per output, for THIS turn.
 * FALSE means no counterfactual was taken for this city -- which is
 * "no opinion", not "costs nothing". */
bool deep_cm_posture_delta(const struct city *pcity, int *food,
                           int *shield, int *trade);

#endif /* FC__DEEPCM_H */
