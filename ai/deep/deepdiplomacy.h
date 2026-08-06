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
#ifndef FC__DEEPDIPLOMACY_H
#define FC__DEEPDIPLOMACY_H

/* common */
#include "fc_types.h"

struct ai_type;
struct player;
struct treaty;

/* Runs from deep's diplomacy_actions wrapper BEFORE ai/default's own
 * pass. Refreshes the leader signals (always) and, when the knobs are
 * open, proposes peace inside the cease-fire window and offers the
 * embassy swap peace makes free. */
void deep_diplomacy_prepass(struct ai_type *ait, struct player *pplayer);

/* TRUE when deep has ruled on this treaty itself and ai/default's
 * evaluation must NOT run. Deep only claims treaty shapes it fully
 * understands -- a lone peace clause, or a treaty made of nothing but
 * embassy clauses -- and forwards everything else. */
bool deep_treaty_evaluate(struct ai_type *ait, struct player *pplayer,
                          struct player *aplayer, struct treaty *ptreaty);

#endif /* FC__DEEPDIPLOMACY_H */
