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
 * Saved under the deep_state namespace (W33 phase 4) -- benchmarks
 * never reload, but human-game saves do, and an op that dissolved
 * on every reload cost those games their assaults. The enum values
 * below are part of the savegame format: append, never reorder. */
#define DEEP_OP_MEMBERS 24
/* Storage cap on concurrent operations, like DEEP_OP_MEMBERS a
 * bookkeeping bound rather than a tunable: forming an op at all
 * requires a distinct unclaimed target plus enough free attackers to
 * clear op_take_target against it, so the count is self-limiting. */
#define DEEP_OPS_MAX 4

/* Phase list (W33 commits 2-3). A land assault runs MUSTER -> MARCH
 * -> STRIKE; launching into MARCH releases the members to the stock
 * attack path while the operation shadows them -- re-gating the take
 * odds at the remaining eta each turn. At contact the op takes them
 * BACK: STRIKE fights the battle under op control, best duel first,
 * every blade on the op target. Amphibious assaults still dissolve
 * at launch: the convoy hold/escort/invade gates own the crossing
 * unchanged. */
enum deep_op_state {
  DEEP_OP_NONE = 0,
  DEEP_OP_MUSTER,
  DEEP_OP_MARCH,
  DEEP_OP_STRIKE,
  /* The city fell to the wave: garrison it from the wave itself,
   * best defender first, until the stack clears the same survival
   * bar every other city answers to -- a capture left open is a
   * recapture (B9-B11 games lost takes this way). */
  DEEP_OP_CONSOLIDATE,
};

/* Op templates (W33). ASSAULT is the muster-and-smash above. RAID
 * (combat E8 graduated into the slot system) claims only fast idle
 * attackers and hunts exposed enemy civilians; it has no target
 * city, no staging, no quorum -- it forms when victims are in reach
 * and dissolves when the hunting stops. Raids coexist with assaults
 * now, but assault formation runs first each turn: the army's first
 * claim on fresh units stands. */
/* LIFT (the colony-lift epic) owns a crossing end to end: a named boat,
 * a named passenger list, a named beachhead, held against re-tasking,
 * with an abort condition. It exists because the ferry pipeline's
 * binding constraint is the RENDEZVOUS, not capacity and not the 1:1
 * cap's arithmetic -- 95.6% of capped waiting unit-turns have a
 * rideable boat with a free slot and only 0.6% ever stand on the same
 * tile, which is where loading happens
 * (findings/settler-missions.md section 9). Stock's two-sided protocol
 * -- passenger raises a flag, boat independently looks for flagged
 * passengers -- is what never closes that gap, so the lift drives the
 * boat itself rather than routing units back into that manager. */
enum deep_op_kind {
  DEEP_OPK_ASSAULT = 0,
  DEEP_OPK_RAID,
  DEEP_OPK_LIFT,
};

struct deep_operation {
  enum deep_op_state state;
  enum deep_op_kind kind;
  int target_city;              /* enemy city id */
  int staging_city;             /* own city id */
  int required;                 /* members needed before launch */
  int formed_turn;
  int march_turn;               /* turn the wave launched into MARCH */
  int march_eta;                /* eta to target measured at launch */
  int phase_turn;               /* turn the current phase was entered */
  /* LIFT only: the named boat, and the named landing tile. A lift
   * reuses MUSTER for "the boat is coming to fetch the manifest" and
   * MARCH for "loaded and crossing"; members[] is the manifest. */
  int ferry;                    /* unit id of the boat, 0 if none */
  int beachhead;                /* tile index to land on, -1 if none */
  int n_members;
  int members[DEEP_OP_MEMBERS]; /* unit ids */
};

void deep_run_operations(struct ai_type *ait, struct player *pplayer);

int deep_op_claiming(const struct player *pplayer, int unit_id);
bool deep_op_release_member(const struct player *pplayer, int unit_id);
bool deep_op_holds(const struct player *pplayer, int unit_id);
void deep_manage_units(struct ai_type *ait, struct player *pplayer);
struct unit *deep_op_poach_defender(struct ai_type *ait,
                                    struct player *pplayer,
                                    struct city *pcity,
                                    double p_survival);

#endif /* FC__DEEPOPS_H */
