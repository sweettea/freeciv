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
#include "actions.h"
#include "city.h"
#include "extras.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"
#include "world_object.h"

/* common/aicore */
#include "path_finding.h"
#include "pf_tools.h"

/* server */
#include "handicaps.h"
#include "unithand.h"

#include "deepsettler.h"
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

/* --- The backlog-empty worker policy -------------------------------
 *
 * Measured motivation (STATUS 2026-08-22, S-FORT). Once a city's
 * improvement backlog is exhausted, deep's workers do not stop: they
 * build fortresses across their own catchment, 10.9% of every working
 * worker-turn pooled over 126 player-games, reaching 71% of the tiles
 * a city actually works in the worst game. The same measurement killed
 * the obvious fix: an ini pricing a fortress 2.6x cheaper moved the
 * want distribution 4x down (median want 6.50 -> 1.50) and moved the
 * BEHAVIOUR not at all, because the base want does not clear a bar --
 * it competes against whatever else the advisor can offer, and when
 * the backlog is empty that is zero. A want of 1.5 beats zero exactly
 * as easily as a want of 13.5.
 *
 * So the lever is a policy for the worker, not a price for the tile:
 *
 *   reserve   keep some workers idle in place, ready for demand that
 *             has not arrived yet. They stay IDLE on purpose --
 *             auto_workers_player() sends an IDLE worker through
 *             settler_run every turn and a mid-activity one through
 *             settler_cont (autoworkers.c:1399-1414), so an idle
 *             reserve is re-offered to the auction every turn and a
 *             fortifying one would not be. That re-offer IS the
 *             reclaim leg: no second dispatcher, no queue, no state.
 *   disband    walk the surplus home and recycle it into the build
 *             queue for half its shields, which is the only way an
 *             unwanted worker turns back into anything.
 *
 * What this deliberately does NOT do: change the fortress price, or
 * touch the base want's exemption from the danger discount. Both are
 * separate epics; changing two mechanisms at once would leave the A/B
 * unable to say which one moved.
 *
 * Scope: dedicated workers. A cityfounder that runs out of work
 * already has a disposal path of its own (deep_settler_join_city(),
 * 5d422d0b44 -- hand it back as two citizens, which beats half its
 * shields), and putting a second policy on the same unit class would
 * make this A/B unattributable. The census below reports the base
 * worker-turns of both classes so the untouched half is visible
 * rather than merely excluded.
 */

/* Per-player policy accumulators. `reserved` is per TURN (the reserve
 * is a standing count, not a running total); everything else runs for
 * the game, so the last line of a game is that game's totals.
 *
 *   empty     worker-turns the auction had nothing real to offer
 *   held      of those, ones kept idle -- inside the reserve target, or
 *             above it with nowhere safe to walk home to
 *   walks     TURNS spent walking home, not workers walking: a worker
 *             four turns from a city raises this four times
 *   disbands  units actually recycled, and `shields` what the cities
 *             got for them */
static int deep_idle_turn[MAX_NUM_PLAYER_SLOTS];
static int deep_idle_reserved[MAX_NUM_PLAYER_SLOTS];
static int deep_idle_target[MAX_NUM_PLAYER_SLOTS];
static int deep_idle_empty[MAX_NUM_PLAYER_SLOTS];
static int deep_idle_held[MAX_NUM_PLAYER_SLOTS];
static int deep_idle_walks[MAX_NUM_PLAYER_SLOTS];
static int deep_idle_disbands[MAX_NUM_PLAYER_SLOTS];
static int deep_idle_shields[MAX_NUM_PLAYER_SLOTS];
static int deep_idle_basew[MAX_NUM_PLAYER_SLOTS];
static int deep_idle_basef[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  Start this player's turn if it has not started yet: the reserve is a
  standing count of workers held THIS turn, so it resets, and the target
  is recomputed from the city count it is scaled by.

  Empire pool rather than a per-city one, stated because "per cluster"
  is how the reserve is SIZED: the target is idle_reserve_per_city times
  the number of cities, and membership is decided by the order the
  advisor reaches workers. Where a reserve worker stands is not managed
  here on purpose -- the auction that reclaims it already prices travel
  (worker_travel_discount), so placing the pool would be a second
  dispatcher competing with the first.
**************************************************************************/
static void deep_idle_turn_start(struct player *pplayer)
{
  int pn = player_number(pplayer);

  if (deep_idle_turn[pn] == game.info.turn) {
    return;
  }
  deep_idle_turn[pn] = game.info.turn;
  deep_idle_reserved[pn] = 0;
  deep_idle_target[pn] =
    (int) (deep_tuning_get()->idle_reserve_per_city
           * city_list_size(pplayer->cities) + 0.5);
}

/**********************************************************************//**
  Recycle this worker into the nearest own city, walking there first if
  it is not standing in one already.

  Returns TRUE if the policy took charge of the unit (recycled it, or
  sent it walking), FALSE if it could not -- no reachable own city, or a
  ruleset with no Disband Unit Recover enabler -- in which case the
  caller falls back to what it would have done without the policy.
**************************************************************************/
static bool deep_idle_recycle(struct ai_type *ait, const struct civ_map *nmap,
                              struct player *pplayer, struct unit *punit)
{
  const struct action *paction = action_by_number(ACTION_DISBAND_UNIT_RECOVER);
  struct city *best;
  int turns = -1;

  if (paction == NULL
      || !unit_can_do_action(punit, ACTION_DISBAND_UNIT_RECOVER)) {
    return FALSE;
  }

  best = deep_settler_nearest_own_city(pplayer, punit, &turns);
  if (best == NULL) {
    return FALSE;
  }

  if (same_pos(unit_tile(punit), city_tile(best))) {
    int pn = player_number(pplayer);
    /* What the city will actually receive: the server adds exactly this
     * to shield_stock (unithand.c:5970-5988), so the counter is the
     * amount the city gets and not an estimate of it. */
    int shields = unit_shield_value(punit, unit_type_get(punit), paction);
    bool done = FALSE;

    if (is_action_enabled_unit_on_city(nmap, ACTION_DISBAND_UNIT_RECOVER,
                                       punit, best)
        && unit_perform_action(pplayer, punit->id, best->id, 0, "",
                               ACTION_DISBAND_UNIT_RECOVER,
                               ACT_REQ_PLAYER)) {
      deep_idle_disbands[pn]++;
      deep_idle_shields[pn] += shields;
      done = TRUE;
    }

    log_base(deep_metric_log_level(),
             "deep-idlepolicy turn=%d player=\"%s\" unit=%d city=%d"
             " recycle shields=%d done=%d",
             game.info.turn, player_name(pplayer), punit->id, best->id,
             shields, done ? 1 : 0);

    return done;
  }

  log_base(deep_metric_log_level(),
           "deep-idlepolicy turn=%d player=\"%s\" unit=%d city=%d"
           " walking turns=%d",
           game.info.turn, player_name(pplayer), punit->id, best->id,
           turns);

  deep_idle_walks[player_number(pplayer)]++;
  punit->goto_tile = city_tile(best);

  /* May return FALSE because the unit died on the way home; the caller
   * checks the id, and either way the policy has taken charge. */
  (void) deep_settler_goto(ait, punit, city_tile(best));

  return TRUE;
}

/**********************************************************************//**
  Is the policy switched on for this player?
**************************************************************************/
bool deep_idle_policy_on(const struct player *pplayer)
{
  return deep_tuning_for(pplayer)->idle_policy_gate > 0;
}

/**********************************************************************//**
  Decide what to do with a worker whose backlog is empty.

  The caller has already established the state: this unit's best task is
  either nothing at all or a base, which the fortress measurement showed
  is what an empty backlog looks like from inside the auction. Counting
  happens in BOTH arms -- the state is the thing the policy targets, and
  a count of it that only exists when the policy is on cannot say
  whether the policy changed it.
**************************************************************************/
enum deep_idle_verdict deep_idle_policy(struct ai_type *ait,
                                        const struct civ_map *nmap,
                                        struct player *pplayer,
                                        struct unit *punit)
{
  int pn = player_number(pplayer);

  deep_idle_turn_start(pplayer);
  deep_idle_empty[pn]++;

  if (deep_tuning_get()->idle_policy_gate <= 0) {
    return DEEP_IDLE_ACT_NONE;      /* Today's carpet, byte for byte. */
  }
  if (unit_is_cityfounder(punit)) {
    return DEEP_IDLE_ACT_NONE;      /* Its own disposal path; see above. */
  }

  if (deep_idle_reserved[pn] < deep_idle_target[pn]) {
    deep_idle_reserved[pn]++;
    deep_idle_held[pn]++;

    return DEEP_IDLE_ACT_RESERVE;
  }

  if (deep_idle_recycle(ait, nmap, pplayer, punit)) {
    return DEEP_IDLE_ACT_DISBAND;
  }

  /* Nowhere to recycle it: hold it rather than invent a third
   * behaviour. It is over the reserve target, so this is the policy
   * failing safe, and `held` counts it. */
  deep_idle_held[pn]++;

  return DEEP_IDLE_ACT_RESERVE;
}

/**********************************************************************//**
  One line per player per turn: what the policy did, and the worker
  corps it did it to.

  basew/basef split the fortress worker-turns of deep-workturn by
  whether the unit can found a city, because the policy deliberately
  leaves cityfounders alone: without the split, a fall in the pooled
  base share could be the policy working or the founder mix moving, and
  the prediction under test is about the half the policy can reach.
**************************************************************************/
void deep_idle_census(struct player *pplayer)
{
  int pn = player_number(pplayer);
  int workers = 0, founders = 0;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  unit_list_iterate(pplayer->units, punit) {
    if (!unit_has_type_flag(punit, UTYF_WORKERS)) {
      continue;
    }
    if (unit_is_cityfounder(punit)) {
      founders++;
      if (punit->activity == ACTIVITY_BASE) {
        deep_idle_basef[pn]++;
      }
    } else {
      workers++;
      if (punit->activity == ACTIVITY_BASE) {
        deep_idle_basew[pn]++;
      }
    }
  } unit_list_iterate_end;

  if (workers > 0 || founders > 0 || deep_idle_empty[pn] > 0) {
    log_base(deep_metric_log_level(),
             "deep-idlepol turn=%d player=\"%s\" gate=%d reserve=%.3f"
             " target=%d workers=%d founders=%d empty=%d held=%d"
             " walks=%d disbands=%d shields=%d basew=%d basef=%d",
             game.info.turn, player_name(pplayer),
             deep_tuning_get()->idle_policy_gate,
             deep_tuning_get()->idle_reserve_per_city,
             deep_idle_target[pn], workers, founders, deep_idle_empty[pn],
             deep_idle_held[pn], deep_idle_walks[pn],
             deep_idle_disbands[pn], deep_idle_shields[pn],
             deep_idle_basew[pn], deep_idle_basef[pn]);
  }
}
