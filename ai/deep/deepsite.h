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
#ifndef FC__DEEPSITE_H
#define FC__DEEPSITE_H

/* common */
#include "fc_types.h"

struct ai_type;
struct cityresult;
struct pf_parameter;

struct cityresult *deep_settler_map_iterate(struct ai_type *ait,
                                            struct pf_parameter *parameter,
                                            struct unit *punit,
                                            int boat_cost);
void deep_contemplate_new_city(struct ai_type *ait, struct city *pcity);

/* The settler pathfinder's tile-behaviour callback refused a tile
   before the pf map could queue it, so the refusal ladder never saw
   it: `owned` for a non-allied owner, otherwise a non-allied unit.
   Reported as tbskip=/tbown=/tbunit=/tbland= on deep-siteladder. */
void deep_site_note_tbskip(const struct tile *ptile, bool owned);

/* The last turn on which a site search of this player's was refused
   land for a reason a turn can undo -- a peace partner's border, a
   non-allied owner the pathfinder never queued, or another settler's
   citymindist reservation -- or -1 if it never was. Read by the
   founder join guard. */
int deep_site_blocked_seen_turn(const struct player *pplayer);
void deep_site_free(void);

/* Local isthmus test, pure map geometry: among the tile's eight
 * neighbours the land forms two or more separate arcs and the land
 * share is narrow, so the tile is a GATE through the landmass. Site
 * scoring pays a bonus for founding in one; exported because a
 * fortress in a gate closes it for the same reason a city does, and
 * two definitions of "chokepoint" would be two laws about one fact. */
bool deep_site_is_choke(const struct civ_map *nmap, struct tile *ptile);

/* Real map distance from a tile to the nearest OTHER city this player
 * holds, or -1 if it holds none. The site evaluator buckets its
 * candidates on it; the founding census records it for the city that
 * was actually planted, and both must mean the same thing or the
 * census cannot check the evaluator. */
int deep_site_own_spacing(struct player *pplayer, const struct tile *ptile);

/* And its bucket, 0..DEEP_SPACING_BUCKETS-1 (2 / 3 / 4 / 5-or-more) or
 * -1 for a player holding no other city. */
int deep_site_spacing_bucket(int d);

#endif /* FC__DEEPSITE_H */
