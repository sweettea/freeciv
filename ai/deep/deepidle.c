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

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "log.h"
#include "mem.h"
#include "support.h"

/* common */
#include "city.h"
#include "extras.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "tile.h"
#include "unit.h"
#include "world_object.h"

#include "deeptuning.h"

#include "deepidle.h"

/* Per-player pollution accumulators, never reset: the question "how
 * much pollution does a cluster carry at once, over a game" cannot be
 * answered from a per-turn snapshot, and the benchmark harness runs one
 * game per server process, so the LAST such line of a game is that
 * game's answer. */
static int deep_poll_turns[MAX_NUM_PLAYER_SLOTS];
static int deep_poll_cityturns[MAX_NUM_PLAYER_SLOTS];
static int deep_poll_tileturns[MAX_NUM_PLAYER_SLOTS];
static int deep_poll_waitturns[MAX_NUM_PLAYER_SLOTS];
static int deep_poll_events[MAX_NUM_PLAYER_SLOTS];
static int deep_poll_cleared[MAX_NUM_PLAYER_SLOTS];

/* Last turn's polluted catchment tiles, one byte per map tile, so an
 * ARRIVAL can be distinguished from a tile that was already dirty. Both
 * are needed: the outstanding count alone cannot say whether one event
 * sat for thirty turns or thirty events were cleaned the turn they
 * landed, and those two worlds want different reserves.
 *
 * Allocated on first use at the map's size and reallocated if the map
 * changes under us (a server that loads a second game). */
static unsigned char *deep_poll_seen[MAX_NUM_PLAYER_SLOTS];
static int deep_poll_seen_size[MAX_NUM_PLAYER_SLOTS];

#define DEEP_POLL_LASTTURN 1  /* Polluted at the previous census. */
#define DEEP_POLL_NOW      2  /* Polluted and already counted this turn. */

/**********************************************************************//**
  Does this tile carry pollution?

  Counts every EC_POLLUTION extra rather than a named one: rulesets
  spell the extra differently (classic and civ2civ3 both use
  "Pollution", but the cause is what the terrain generator, the city
  turn and the clean action all key off), and EC_FALLOUT is deliberately
  NOT counted -- nuclear fallout is a different arrival process with a
  different cleanup, and pooling the two would give the reserve a
  demand curve neither of them has.
**************************************************************************/
static bool deep_poll_tile_polluted(const struct tile *ptile)
{
  extra_type_by_cause_iterate(EC_POLLUTION, pextra) {
    if (tile_has_extra(ptile, pextra)) {
      return TRUE;
    }
  } extra_type_by_cause_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  Is somebody already cleaning this tile?

  Any unit standing on it with the clean activity counts, ours or not:
  the question the dwell metric asks is whether the tile is still
  WAITING for a worker, and a tile an ally is cleaning is not waiting
  for one of ours.
**************************************************************************/
static bool deep_poll_tile_attended(const struct tile *ptile)
{
  unit_list_iterate(ptile->units, punit) {
    if (punit->activity == ACTIVITY_CLEAN) {
      return TRUE;
    }
  } unit_list_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  One line per player per turn: the pollution outstanding inside this
  player's city catchments, the tile-turns it has cost so far, and how
  long an event waits before anybody starts cleaning it.

  This is the demand a worker reserve exists to answer, and it is
  measured rather than assumed because the reserve's size is a knob and
  rule 7 forbids guessing it. Three numbers come out of it:

    concur   mean polluted catchment tiles outstanding, empire-wide
    percity  the same divided by cities -- mean concurrent events per
             CLUSTER, which is exactly the quantity the reserve holds
             workers against
    dwell    waitturns / events, the mean turns an event sits before
             cleanup begins (Little's law on the waiting queue)

  Catchment tiles only. Pollution outside every work radius costs the
  empire nothing this instrument can price, and the reserve is sized per
  cluster, so the cluster is the frame.

  Log-only: nothing reads these accumulators back into a decision.
**************************************************************************/
void deep_pollution_census(struct player *pplayer)
{
  const struct civ_map *nmap = &(wld.map);
  int pn = player_number(pplayer);
  int cities = 0, polluted = 0, attended = 0;
  int size = MAP_INDEX_SIZE;
  int i;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  if (deep_poll_seen[pn] == NULL || deep_poll_seen_size[pn] != size) {
    if (deep_poll_seen[pn] != NULL) {
      free(deep_poll_seen[pn]);
    }
    deep_poll_seen[pn] = fc_calloc(size, sizeof(unsigned char));
    deep_poll_seen_size[pn] = size;
  }

  city_list_iterate(pplayer->cities, pcity) {
    cities++;
    city_tile_iterate(nmap, city_map_radius_sq_get(pcity),
                      city_tile(pcity), ptile) {
      int idx = tile_index(ptile);

      if (idx < 0 || idx >= size
          || deep_poll_seen[pn][idx] == DEEP_POLL_NOW) {
        continue;                    /* Two catchments, one tile. */
      }
      if (!deep_poll_tile_polluted(ptile)) {
        continue;
      }
      if (deep_poll_seen[pn][idx] != DEEP_POLL_LASTTURN) {
        deep_poll_events[pn]++;      /* It was not dirty last turn. */
      }
      deep_poll_seen[pn][idx] = DEEP_POLL_NOW;
      polluted++;
      if (deep_poll_tile_attended(ptile)) {
        attended++;
      }
    } city_tile_iterate_end;
  } city_list_iterate_end;

  /* Retire last turn's marks: what is still dirty stays marked, what is
   * gone (cleaned, or the city that watched it lost) is a departure. */
  for (i = 0; i < size; i++) {
    if (deep_poll_seen[pn][i] == DEEP_POLL_NOW) {
      deep_poll_seen[pn][i] = DEEP_POLL_LASTTURN;
    } else if (deep_poll_seen[pn][i] == DEEP_POLL_LASTTURN) {
      deep_poll_seen[pn][i] = 0;
      deep_poll_cleared[pn]++;
    }
  }

  deep_poll_turns[pn]++;
  deep_poll_cityturns[pn] += cities;
  deep_poll_tileturns[pn] += polluted;
  deep_poll_waitturns[pn] += polluted - attended;

  log_base(deep_metric_log_level(),
           "deep-pollute turn=%d player=\"%s\" cities=%d polluted=%d"
           " attended=%d waiting=%d tileturns=%d waitturns=%d events=%d"
           " cleared=%d dwell=%.2f concur=%.3f percity=%.4f",
           game.info.turn, player_name(pplayer), cities, polluted,
           attended, polluted - attended, deep_poll_tileturns[pn],
           deep_poll_waitturns[pn], deep_poll_events[pn],
           deep_poll_cleared[pn],
           deep_poll_events[pn] > 0
           ? (double) deep_poll_waitturns[pn] / deep_poll_events[pn] : 0.0,
           deep_poll_turns[pn] > 0
           ? (double) deep_poll_tileturns[pn] / deep_poll_turns[pn] : 0.0,
           deep_poll_cityturns[pn] > 0
           ? (double) deep_poll_tileturns[pn] / deep_poll_cityturns[pn]
           : 0.0);
}
