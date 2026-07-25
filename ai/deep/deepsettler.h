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
#ifndef FC__DEEPSETTLER_H
#define FC__DEEPSETTLER_H

/* common */
#include "fc_types.h"

struct workermap;

struct cityresult *deep_find_best_city_placement(struct ai_type *ait,
                                                 struct unit *punit,
                                                 bool look_for_boat,
                                                 bool use_virt_boat);

void deep_auto_settler_run(struct ai_type *ait, const struct civ_map *nmap,
                           struct player *pplayer,
                           struct unit *punit, struct workermap *state);

#endif /* FC__DEEPSETTLER_H */
