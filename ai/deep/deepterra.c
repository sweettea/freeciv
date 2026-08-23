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

/* common */
#include "terrain.h"

#include "deepterra.h"

/* The two terrain-changing activities this graph is built from. Order
 * only decides the order equal-cost discoveries appear in. */
static const enum unit_activity deep_terra_edges[] = {
  ACTIVITY_CULTIVATE, ACTIVITY_PLANT
};
#define DEEP_TERRA_EDGE_COUNT \
  (int) (sizeof(deep_terra_edges) / sizeof(deep_terra_edges[0]))

/**********************************************************************//**
  The terrain 'act' produces on 'from', or NULL.
**************************************************************************/
const struct terrain *deep_terra_result(const struct terrain *from,
                                        enum unit_activity act)
{
  const struct terrain *result;

  if (from == NULL) {
    return NULL;
  }

  switch (act) {
  case ACTIVITY_CULTIVATE:
    result = from->cultivate_result;
    break;
  case ACTIVITY_PLANT:
    result = from->plant_result;
    break;
  default:
    return NULL;
  }

  if (result == T_NONE || result == from) {
    return NULL;
  }

  return result;
}

/**********************************************************************//**
  Reference worker-turns to do 'act' on 'from'. See deepterra.h on why
  the ruleset time is already a turn count.
**************************************************************************/
static int deep_terra_edge_turns(const struct terrain *from,
                                 enum unit_activity act)
{
  switch (act) {
  case ACTIVITY_CULTIVATE:
    return from->cultivate_time;
  case ACTIVITY_PLANT:
    return from->plant_time;
  default:
    return 0;
  }
}

/**********************************************************************//**
  Terrains reachable from 'from' by doing 'first' and then cultivating
  and planting freely. See deepterra.h.

  Shortest paths by relaxation rather than by a priority queue: the graph
  has at most DEEP_TERRA_REACH_MAX nodes of degree two, so the whole
  search is a handful of integer comparisons, and a relaxation loop is
  the version whose termination is obvious -- every pass either lowers a
  cost, adds a node, or is the last one.
**************************************************************************/
int deep_terra_reach(const struct terrain *from, enum unit_activity first,
                     struct deep_terra_step out[DEEP_TERRA_REACH_MAX])
{
  const struct terrain *start = deep_terra_result(from, first);
  int n = 0, pass;

  if (start == NULL) {
    return 0;
  }

  out[0].pterrain = start;
  out[0].turns = deep_terra_edge_turns(from, first);
  n = 1;

  /* Bounded by node count: a shortest path visits each node once, so
   * DEEP_TERRA_REACH_MAX passes settle every cost even in the worst
   * order of discovery. */
  for (pass = 0; pass < DEEP_TERRA_REACH_MAX; pass++) {
    bool changed = FALSE;
    int i, e;

    for (i = 0; i < n; i++) {
      for (e = 0; e < DEEP_TERRA_EDGE_COUNT; e++) {
        const struct terrain *next = deep_terra_result(out[i].pterrain,
                                                       deep_terra_edges[e]);
        int cost, j;

        if (next == NULL || next == from) {
          /* Back to the start terrain is a path that spends turns to
           * arrive where it began (civ2civ3's Grassland<->Forest and
           * Plains<->Forest are both such cycles). */
          continue;
        }
        cost = out[i].turns
          + deep_terra_edge_turns(out[i].pterrain, deep_terra_edges[e]);

        for (j = 0; j < n; j++) {
          if (out[j].pterrain == next) {
            break;
          }
        }
        if (j < n) {
          if (cost < out[j].turns) {
            out[j].turns = cost;
            changed = TRUE;
          }
        } else if (n < DEEP_TERRA_REACH_MAX) {
          out[n].pterrain = next;
          out[n].turns = cost;
          n++;
          changed = TRUE;
        }
      }
    }
    if (!changed) {
      break;
    }
  }

  return n;
}
