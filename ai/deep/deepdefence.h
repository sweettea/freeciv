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
#ifndef FC__DEEPDEFENCE_H
#define FC__DEEPDEFENCE_H

/* common */
#include "fc_types.h"

void deep_do_first_activities(struct ai_type *ait, struct player *pplayer);
void deep_do_last_activities(struct ai_type *ait, struct player *pplayer);

/* Map crowding from lobby settings: min(1, density_prior_tiles /
 * land-per-player). Shared by the density prior, the settler rush
 * and the chokepoint site bonus. */
double deep_map_crowding(void);

/* Share of known land that nobody owns, 0..1. The LIVE figure, unlike
 * the two lobby-setting priors below; cached per player per turn. */
double deep_free_land_share(const struct player *pplayer);

/* How many more cities the unclaimed land could hold at this player's
 * own packing density. Sites remaining, not cities owned. */
double deep_remaining_sites(const struct player *pplayer);

/* Unowned land tiles a founder of this player could actually stand on:
 * own landmasses always, the rest of the world only while sea lift
 * exists. Exposed for the founder-pool census. */
int deep_reachable_free_land(const struct player *pplayer);

/* Land pressure: land_pressure_tiles / (land_pressure_tiles +
 * land-per-player). Same evidence as deep_map_crowding() and the same
 * direction, but it never saturates, so it keeps discriminating below
 * the reference where the crowding prior is pinned at 1.0. */
double deep_land_pressure(void);

/* Fleet supply and demand on one body of water (a negative continent
 * id), exposed so the build-choice override can price EVERY ferry
 * proposal -- stock's founder's boat and attacker ferry as well as
 * deep's own colony want. */
int deep_idle_ferries_on(struct ai_type *ait, struct player *pplayer,
                         Continent_id ocean);
int deep_ferry_demand_on(struct ai_type *ait, struct player *pplayer,
                         Continent_id ocean);
/* The same demand with the distance to its nearest unit, for the hull
 * census: on an island map one ocean can be the whole world sea. */
int deep_ferry_demand_near(struct ai_type *ait, struct player *pplayer,
                           Continent_id ocean, const struct tile *from,
                           int *nearest);
int deep_water_surplus(struct ai_type *ait, struct player *pplayer,
                       const struct tile *ptile);

/* Pure GEOMETRY: does this water touch more than one land continent,
 * so that a hull launched into it can ever land a passenger anywhere
 * but where it embarked. A single-surrounder sea is useless to every
 * city that can build into it, whatever the demand does. */
bool deep_water_bridges(Continent_id ocean);
bool deep_city_water_bridges(const struct tile *ptile);

#endif /* FC__DEEPDEFENCE_H */
