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

/* Does this tile already carry a road that speeds movement? The plan's
 * own gap test, exported because the funnel ledger (deepfunnel.c) asks
 * the same question of a tile the plan has since forgotten. */
bool deep_trunk_tile_roaded(const struct tile *ptile);

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
void deep_trunk_source_pick(const struct player *pplayer,
                            const struct tile *ptile, int dist, bool flipped);
bool deep_trunk_tile_is_ally(const struct player *pplayer,
                             const struct tile *ptile);

/* W12 slice 4: the trade dividend alone out of the want above, so the
 * task source can price the champion it would have chosen without it
 * and say whether the dividend FLIPPED the assignment. */
adv_want deep_trunk_source_dividend(const struct player *pplayer,
                                    const struct extra_type *target,
                                    const struct tile *ptile);

/* W12 slice 3: the node set was pplayer->cities and nothing else, so
 * the network could never leave the shape of deep's own empire --
 * a corridor to the ally next door was exactly as invisible as any
 * road was before slice 1. Behind trunk_ally_gate the set gains one
 * city per allied player (the ally's city nearest to ours on a shared
 * continent), as a LEAF: an ally is an endpoint, never a hub between
 * two of our own cities.
 *
 * What a worker may do out there, checked before this was designed:
 * building is not restricted by tile ownership in the classic ruleset
 * -- the road enabler asks for the Workers flag and CanRoad terrain
 * (data/classic/actions.ruleset:1066-1074), the Road extra itself for
 * the Workers flag and a Land tile (terrain.ruleset:1603-1607), and
 * ACTRES_ROAD has no hard ownership requirement anywhere in
 * common/actions.c. REACHING the tile is the binding law: both worker
 * pathfinders TB_IGNORE every tile owned by a player we are not
 * allied with (autoworkers.c:521-535, deepsettler.c:99-114), and
 * players_non_invade() (common/player.c:1434) additionally bars
 * non-civilian movement into a peace partner's borders. Unowned and
 * allied ground is therefore both legal and reachable, third-party
 * ground is neither in practice, and the corridor is clipped at the
 * first tile of it. */

/* W12H: may a corridor cross this tile, given who owns it? TRUE for
 * unowned, our own and allied ground always, and for a PEACE PARTNER's
 * (DS_PEACE or DS_ARMISTICE) behind trunk_soft_border_gate. War,
 * cease-fire and no-contact ground stays shut in both settings.
 *
 * One definition, two callers, because they must agree: the planner's
 * border clip in deep_trunk_walk() and the corridor pathfinder's tile
 * behaviour in deepsettler.c. The header block above records why the
 * question is safety and not legality -- nothing in the core rules
 * stops a civilian entering or a road being built anywhere. */
bool deep_trunk_soft_open(const struct player *pplayer,
                          const struct tile *ptile);

/* W12G, the keepable-bid gate's own instrument: a corridor bid the
 * founder's own site value refused. Called by the task source when the
 * best beyond-ceiling candidate this turn beat the advisor's champion
 * and would have been assigned, but did not clear the floor the site
 * search set -- which is exactly the assignment the same call used to
 * make and then discard. Zero by construction when the gate is shut. */
void deep_trunk_source_veto(const struct player *pplayer);

/* W12G, the stickiness margin's memory and its instrument. The advisor
 * keeps no record of which tile a walking worker is walking to, so the
 * task source keeps one here: deep_trunk_hold_tile() reads it,
 * deep_trunk_hold_set() writes it (NULL forgets), and
 * deep_trunk_hold_census() charges each re-priced hold to the census,
 * saying whether the premium is what kept it. Written only while
 * trunk_keep_margin > 0. */
struct tile *deep_trunk_hold_tile(const struct player *pplayer, int uid);
void deep_trunk_hold_set(const struct player *pplayer, int uid,
                         const struct tile *ptile);
void deep_trunk_hold_census(const struct player *pplayer, bool kept);

/* Trunk census: links planned, corridor tiles still missing a road,
 * candidates the want reached, and the largest want issued -- plus,
 * on its own deep-trunkally line, the same for the corridors whose
 * far endpoint is an allied city. */
void deep_trunk_census(struct player *pplayer);

#endif /* FC__DEEPTRUNK_H */
