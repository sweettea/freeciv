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

/* Move a civilian with the protections the site search plans with:
 * deep's risk parameter plus is_pos_dangerous, which stock's settler
 * mover never assigns. Exported for the idle-worker policy, whose
 * surplus workers walk home to be recycled and must walk it the same
 * way a settler would. Returns FALSE iff the unit died. */
bool deep_settler_goto(struct ai_type *ait, struct unit *punit,
                       struct tile *ptile);

/* The nearest own city this civilian can reach under deep's civilian
 * risk parameter, by arrival turn, or NULL when none is safely
 * reachable. Same map the mover walks with, so the answer is one the
 * walk will honour. */
struct city *deep_settler_nearest_own_city(struct player *pplayer,
                                           struct unit *punit, int *turns);

/* The overnight-danger bar with a tolerance multiplier -- deep's own
 * civilian risk test, shared with the trade evaluator so that "would a
 * settler refuse to stand here" and "is this caravan's road dangerous"
 * are the same question asked of the same tiles. */
bool deep_settler_overnight_veto(struct unit *punit,
                                 const struct tile *ptile,
                                 double tolerance);

/* Base-want census: candidates priced, positives, largest want, and
 * the behaviour metric -- workers actually building a base now. */
void deep_fort_census(struct player *pplayer);

/* Saturation census: how much of each deep city's work radius, and of
 * the part of it the city actually works, carries a base. */
void deep_fort_saturation_census(struct player *pplayer);

/* Worker-turns by task class, accumulated over the game, so the
 * fortress spend is readable as a share of the worker corps. */
void deep_worker_class_census(struct player *pplayer);

/* Travel-discount census: candidates offered, candidates discounted,
 * their mean arrival and the want removed, plus how far the workers
 * that ended the turn en route still have to walk. */
void deep_worker_travel_census(struct player *pplayer);

adv_want deep_worker_task_value(const struct player *pplayer,
                                const struct unit *punit,
                                enum unit_activity act,
                                const struct extra_type *target,
                                struct tile *ptile, adv_want value,
                                adv_want old_tile_value, int arrival);

#endif /* FC__DEEPSETTLER_H */
