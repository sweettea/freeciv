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
#ifndef FC__DEEPSITE_H
#define FC__DEEPSITE_H

/* common */
#include "fc_types.h"

struct ai_type;
struct cityresult;
struct pf_parameter;

struct cityresult *deep_settler_map_iterate(struct ai_type *ait,
                                            struct pf_parameter *parameter,
                                            struct unit *punit,
                                            int boat_cost);
void deep_contemplate_new_city(struct ai_type *ait, struct city *pcity);

#endif /* FC__DEEPSITE_H */
