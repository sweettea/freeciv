/***********************************************************************
 Freeciv - Copyright (C) 2002 - The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifndef FC__DAISETTLER_H
#define FC__DAISETTLER_H

/* utility */
#include "support.h"            /* bool type */

/* common */
#include "ai.h"
#include "city.h"
#include "fc_types.h"

struct ai_data;
struct ai_plr;
struct tile_data_cache;
struct pf_parameter;

/* Stop looking too hard for better tiles to found a new city at when
 * settler finds location with at least RESULT_IS_ENOUGH want
 * points. See city_desirability() for how base want is computed
 * before amortizing it.
 *
 * This is a big WAG to save a big amount of CPU. */
#define RESULT_IS_ENOUGH 250

#define FERRY_TECH_WANT 500

struct cityresult {
  struct tile *tile;
  adv_want total;              /* Total value of position */
  adv_want result;             /* Amortized and adjusted total value */
  int corruption, waste;
  bool overseas;               /* Have to use boat to get there */
  bool virt_boat;              /* Virtual boat was used in search,
                                * so need to build one */

  struct {
    struct tile_data_cache *tdc;  /* Values of city center; link to the data
                                   * in tdc_hash. */
  } city_center;

  struct {
    struct tile *tile;            /* Best other tile */
    int cindex;                   /* City-relative index for other tile */
    struct tile_data_cache *tdc;  /* Value of best other tile; link to the
                                   * data in tdc_hash. */
  } best_other;

  adv_want remaining;             /* Value of all other tiles */

  /* Save the result for print_citymap(). */
  struct tile_data_cache_hash *tdc_hash;

  int city_radius_sq;             /* Current squared radius of the city */
};

enum cb_error_level { CBE_OK, CBE_RECOVERABLE, CBE_FATAL };

void cityresult_destroy(struct cityresult *result);

struct cityresult *settler_map_iterate(struct ai_type *ait,
                                       struct pf_parameter *parameter,
                                       struct unit *punit,
                                       int boat_cost);

enum cb_error_level dai_do_build_city(struct ai_type *ait,
                                      struct player *pplayer,
                                      struct unit *punit);

void dai_auto_settler_init(struct ai_plr *ai);
void dai_auto_settler_free(struct ai_plr *ai);

void dai_auto_settler_reset(struct ai_type *ait, struct player *pplayer);
void dai_auto_settler_run(struct ai_type *ait, const struct civ_map *nmap,
                          struct player *pplayer,
                          struct unit *punit, struct workermap *state);
void dai_auto_settler_cont(struct ai_type *ait, const struct civ_map *nmap,
                           struct player *pplayer,
                           struct unit *punit, struct workermap *state);

void contemplate_new_city(struct ai_type *ait, struct city *pcity);

#endif /* FC__DAISETTLER_H */
