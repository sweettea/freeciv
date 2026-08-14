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
#ifndef FC__DEEPGOV_H
#define FC__DEEPGOV_H

/* common */
#include "fc_types.h"

struct ai_type;
struct government;
struct player;

/* The `gov_value` vtable hook. Deep forwarded it to `dai_gov_value`
 * from the day the module existed; this is where deep answers it
 * itself. With `gov_valuation` at its default the call is still
 * forwarded unchanged, so the hook's ownership costs nothing until
 * the knob is moved. */
void deep_gov_value(struct ai_type *ait, struct player *pplayer,
                    struct government *gov, adv_want *val, bool *override);

#endif /* FC__DEEPGOV_H */
