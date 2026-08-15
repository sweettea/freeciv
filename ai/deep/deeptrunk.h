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
#ifndef FC__DEEPTRUNK_H
#define FC__DEEPTRUNK_H

/* Trunk roads (W12, first slice). The auto-worker advisor values a
 * road by what it does for the TILE: adv_workers_road_bonus() counts
 * the road neighbours the tile already has (autoworkers.c:265-306) and
 * the move-cost term prices one tile's own movement. Nothing in it
 * knows that a road here would join two cities, so a road never
 * out-competes an irrigation because of where it sits in the empire.
 *
 * Here the empire's cities are spanned by a minimum spanning tree, its
 * edges walked into tile corridors, and the corridor tiles that still
 * lack a road carry the LINK's worth: the movement the link saves on
 * one traversal once it is complete, discounted by how much of it is
 * still missing. That want reaches the worker auction through the
 * task-value hook, so a corridor tile can outbid an isolated tile
 * improvement when the numbers say so.
 *
 * Ceiling, stated because it bounds what this can do: the advisor only
 * ever enumerates tiles inside the player's own city radii
 * (worker_evaluate_improvements(), autoworkers.c:576-583), so corridor
 * tiles in the gap between two distant cities are never offered to the
 * hook at all. What this slice can move is the near-city end of every
 * link -- which road tile a worker builds, not whether a worker is
 * dispatched into no-man's land to build one. */

#include "fc_types.h"

/* The trunk want for a candidate road on this tile, or 0.0 when the
 * tile is not on a corridor, the target is not a road that speeds
 * movement, or the knob is off. Builds (and memoises) this player's
 * plan for this turn on first call. */
adv_want deep_trunk_want(const struct player *pplayer,
                         const struct extra_type *target,
                         const struct tile *ptile);

/* W12 slice 2: the ceiling above, lifted from outside. The advisor's
 * enumeration cannot be widened from within (STATUS "W12 slice 1"
 * section 7), so deep sources the hidden demand itself: the corridor
 * gap tiles outside every own city radius, the same want pricing, and
 * a pick counter of its own. */
int deep_trunk_beyond_tiles(const struct player *pplayer,
                            const int **list);
adv_want deep_trunk_source_want(const struct player *pplayer,
                                const struct extra_type *target,
                                const struct tile *ptile);
void deep_trunk_source_pick(const struct player *pplayer, int dist);

/* Trunk census: links planned, corridor tiles still missing a road,
 * candidates the want reached, and the largest want issued. */
void deep_trunk_census(struct player *pplayer);

#endif /* FC__DEEPTRUNK_H */
