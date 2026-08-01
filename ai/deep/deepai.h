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
#ifndef FC__DEEPAI_H
#define FC__DEEPAI_H

/* common */
#include "player.h"

/* ai/default */
#include "daidata.h"

#include "deepops.h"

struct ai_type;
struct section_file;

struct ai_type *deep_ai_get_self(void);

/* Per-player state of the deep AI. The embedded default-AI state must
 * stay the first member: every forwarded dai_* call finds its data in
 * this module's player slot and casts it to struct ai_plr. */
struct deep_plr {
  struct ai_plr defai;          /* Keep first */

  /* Deep-only state below. Everything here must either be saved in
   * dpai_player_save() or be recomputable. */
  int dv_turns_logged;          /* Decision-vector lines emitted */
  /* Operation slots (W33); DEEP_OP_NONE marks a free slot.
   * Recomputable: each re-forms from wants. */
  struct deep_operation ops[DEEP_OPS_MAX];
  /* Second-wave bookkeeping (E7): the last launched strike, so the
   * next formation pass can ask "did it stick, and can the reserve
   * plus survivors crush what remains?" Player-level, not per-op:
   * the reserve answers whichever strike was most recent. */
  int last_strike_city;
  int last_strike_turn;
  /* Colony lift demand (task #20): the best overseas site score a
   * lift-limited settler evaluation recorded, and when. Recomputable
   * -- refreshed every turn the settler pipeline runs. */
  int colony_want;
  int colony_turn;
};

static inline struct deep_plr *deep_plr_data(const struct player *pplayer)
{
  return (struct deep_plr *) player_ai_data(pplayer, deep_ai_get_self());
}

bool fc_ai_deep_setup(struct ai_type *ai);

#endif /* FC__DEEPAI_H */
