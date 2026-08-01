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
#ifndef FC__DEEPCOMBAT_H
#define FC__DEEPCOMBAT_H

/* common */
#include "fc_types.h"

struct ai_type;
struct civ_map;

double deep_city_stack_survival(const struct civ_map *nmap,
                                struct city *pcity, int horizon,
                                int *wave_out);
double deep_city_stack_survival_without(const struct civ_map *nmap,
                                        struct city *pcity, int horizon,
                                        const struct unit *except,
                                        int *wave_out);
double deep_take_odds(const struct civ_map *nmap, struct city *acity,
                      struct unit *const *stream, int n_stream);
double deep_take_odds_at_arrival(const struct civ_map *nmap,
                                 struct city *acity,
                                 struct unit *const *stream, int n_stream,
                                 int eta);

#endif /* FC__DEEPCOMBAT_H */
