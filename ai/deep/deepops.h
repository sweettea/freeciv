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
#ifndef FC__DEEPOPS_H
#define FC__DEEPOPS_H

/* common */
#include "fc_types.h"

struct ai_type;

/* An operation: the first object in the tree that represents a
 * multi-turn intention (combat E6; W33's substrate). The smash the
 * human asked for: mass troops at a staging city, then launch at a
 * specific goal as one wave instead of the per-turn trickle stock
 * produces. A player runs up to DEEP_OPS_MAX of them at once, each
 * against its own target with its own claimed members.
 *
 * Not saved: an operation re-forms from the same want conditions
 * that created it, so a reload costs at most a few muster turns. */
#define DEEP_OP_MEMBERS 24
/* Storage cap on concurrent operations, like DEEP_OP_MEMBERS a
 * bookkeeping bound rather than a tunable: forming an op at all
 * requires a distinct unclaimed target plus enough free attackers to
 * clear op_take_target against it, so the count is self-limiting. */
#define DEEP_OPS_MAX 4

/* Phase list (W33 commit 2). A land assault runs MUSTER -> MARCH;
 * launching into MARCH releases the members to the stock attack path
 * while the operation shadows them -- re-gating the take odds at the
 * remaining eta each turn and recording the strike at contact rather
 * than at departure. Amphibious assaults still dissolve at launch:
 * the convoy hold/escort/invade gates own the crossing unchanged. */
enum deep_op_state {
  DEEP_OP_NONE = 0,
  DEEP_OP_MUSTER,
  DEEP_OP_MARCH,
};

struct deep_operation {
  enum deep_op_state state;
  int target_city;              /* enemy city id */
  int staging_city;             /* own city id */
  int required;                 /* members needed before launch */
  int formed_turn;
  int march_turn;               /* turn the wave launched into MARCH */
  int march_eta;                /* eta to target measured at launch */
  int n_members;
  int members[DEEP_OP_MEMBERS]; /* unit ids */
};

void deep_run_operations(struct ai_type *ait, struct player *pplayer);
void deep_run_raids(struct ai_type *ait, struct player *pplayer);

int deep_op_claiming(const struct player *pplayer, int unit_id);

#endif /* FC__DEEPOPS_H */
