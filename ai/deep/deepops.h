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
 * multi-turn intention (combat E6; W33's substrate, smallest viable
 * slice). One per player for now -- the smash the human asked for:
 * mass troops at a staging city, then launch at a specific goal as
 * one wave instead of the per-turn trickle stock produces.
 *
 * Not saved: an operation re-forms from the same want conditions
 * that created it, so a reload costs at most a few muster turns. */
#define DEEP_OP_MEMBERS 24

enum deep_op_state {
  DEEP_OP_NONE = 0,
  DEEP_OP_MUSTER,
};

struct deep_operation {
  enum deep_op_state state;
  int target_city;              /* enemy city id */
  int staging_city;             /* own city id */
  int required;                 /* members needed before launch */
  int formed_turn;
  int n_members;
  int members[DEEP_OP_MEMBERS]; /* unit ids */
  /* Second-wave bookkeeping (E7): the last launched strike, so the
   * next formation pass can ask "did it stick, and can the reserve
   * plus survivors crush what remains?" */
  int last_strike_city;
  int last_strike_turn;
};

void deep_run_operations(struct ai_type *ait, struct player *pplayer);
void deep_run_raids(struct ai_type *ait, struct player *pplayer);

#endif /* FC__DEEPOPS_H */
