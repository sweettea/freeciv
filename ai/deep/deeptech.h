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
#ifndef FC__DEEPTECH_H
#define FC__DEEPTECH_H

/* TECHMIL slice 2 -- deep's own research-goal selection.
 *
 * Deep has no tech vtable hook of its own; it reaches research
 * management by calling stock's `dai_manage_tech()` from
 * deep_do_last_activities() (deepdefence.c). That call now comes
 * through here. At `techgoal_deep = 0` (the default) this wrapper IS
 * that call and nothing else -- byte-identical. Gated on, stock still
 * runs first, unchanged (its `dai_tech_effect_values()` writer is part
 * of the want pipeline, and its committed choice is the counter's
 * honesty field); deep then re-selects both the researching advance
 * and the goal from the same tech_want[] mass with the distance shape
 * under our control. See deeptech.c for the formula and deeptuning.h
 * for the knobs. */

#include "fc_types.h"

struct ai_type;

void deep_manage_tech(struct ai_type *ait, struct player *pplayer);

#endif /* FC__DEEPTECH_H */
