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
#ifndef FC__DEEPDEFENCE_H
#define FC__DEEPDEFENCE_H

/* common */
#include "fc_types.h"

void deep_do_first_activities(struct ai_type *ait, struct player *pplayer);
void deep_do_last_activities(struct ai_type *ait, struct player *pplayer);

/* Map crowding from lobby settings: min(1, density_prior_tiles /
 * land-per-player). Shared by the density prior, the settler rush
 * and the chokepoint site bonus. */
double deep_map_crowding(void);

#endif /* FC__DEEPDEFENCE_H */
