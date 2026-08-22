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
#ifndef FC__DEEPFUNNEL_H
#define FC__DEEPFUNNEL_H

/* The corridor task funnel (W12): where 255 picks a game become 14
 * roads.
 *
 * The corridor-150 pilot (STATUS 2026-08-22) measured 255 beyond-ceiling
 * corridor tasks assigned per game buying +14 net roaded land tiles
 * map-wide across six empires. Both numbers came from instruments that
 * count ASSIGNMENTS and count OUTCOMES, with nothing in between, so the
 * gap between them admits every explanation at once: re-picks of the
 * same tile, workers turned around before arrival, activities preempted
 * mid-build, or corridors that were never worth finishing.
 *
 * This file is the in-between. It keeps one ledger entry per worker per
 * live corridor assignment and follows it to a terminal state, so the
 * funnel can be read as stages rather than as two endpoints:
 *
 *   picks    assignments made (the pilot's trunk-picks, re-counted here)
 *   tiles    DISTINCT corridor tiles ever assigned -- is 255 re-picks?
 *   start    times a worker actually began the road activity
 *   wtiles   distinct tiles a worker began work on
 *   wturns   turns of work accumulated on corridor tiles
 *   done     ever-assigned corridor tiles later seen carrying a road
 *   dwork    of those, ones a deep worker had begun work on
 *   dsalv    of those, ones no deep worker ever began -- somebody else's
 *            road on a tile deep walked away from
 *
 * and every assignment that ends without becoming work is charged to a
 * CAUSE, recorded at the site where the task is dropped rather than
 * inferred afterwards. What it does NOT do is change any of those
 * sites: every predicate it reads was already evaluated by the code it
 * sits in, and nothing here is read back into a decision.
 *
 * Attribution caveat, the same one deepfort states: extras do not
 * record who built them, so `dwork` means "a deep worker began a road
 * here and the tile now has one", not "deep laid it". */

#include "fc_types.h"

/* Why an assignment ended. Ordered by how far the task got: everything
 * from DFC_UNSAFE down was still in this turn's corridor plan. */
enum deep_funnel_cause {
  DFC_ROADED = 0,  /* The tile got its road; the plan dropped it. */
  DFC_REPLAN,      /* Out of the plan for another reason (the MST moved,
                    * or a new city's radius swallowed the tile). */
  DFC_UNSAFE,      /* adv_worker_safe_tile() refused the tile. */
  DFC_TAKEN,       /* Another of our own workers stands on it. */
  DFC_NOPATH,      /* The worker's pf map cannot reach it. */
  DFC_NOACT,       /* No road action is possible there. */
  DFC_PRICE,       /* The trunk price came out zero. */
  DFC_SWITCH,      /* A DIFFERENT corridor tile outbid it. */
  DFC_OUTBID,      /* The advisor's own champion outbid every corridor
                    * tile, so the worker went back inside a radius. */
  DFC_REQUEST,     /* A city work request took the worker before the
                    * task source ran at all. */
  DFC_NORUN,       /* The worker never reached the task source this turn
                    * (held by an operation, mid-activity elsewhere, out
                    * of moves, under orders). */
  DFC_GONE,        /* The worker died or stopped being a worker. */
  DFC_FLEE,        /* Mid-build, the safety pass sent it idle. */
  DFC_STOP,        /* Mid-build, the activity ended with no road. */
  DFC_CAUSE_COUNT
};

/* Opening and closing the task source's own turn for one worker.
 * deep_funnel_open() returns the corridor tile this worker was last
 * assigned (NULL when it holds none) so the source can watch that one
 * tile through its own loop; deep_funnel_close() takes the tile the
 * source picked (NULL for none) and the stage the previous tile reached,
 * and either renews the assignment or charges the drop. */
struct tile *deep_funnel_open(const struct player *pplayer,
                              const struct unit *punit);
void deep_funnel_close(const struct player *pplayer,
                       const struct unit *punit,
                       struct tile *picked, int prev_stage);

/* How far the previously-assigned tile got through the source's loop.
 * deep_funnel_close() maps the stage onto a cause only when the tile
 * was NOT re-picked. */
#define DEEP_FUNNEL_ST_ABSENT 0  /* Not in this turn's beyond list. */
#define DEEP_FUNNEL_ST_UNSAFE 1
#define DEEP_FUNNEL_ST_TAKEN  2
#define DEEP_FUNNEL_ST_NOPATH 3
#define DEEP_FUNNEL_ST_NOACT  4
#define DEEP_FUNNEL_ST_PRICE  5
#define DEEP_FUNNEL_ST_LIVE   6  /* Priced above zero and still lost. */

/* A worker began its activity on 'ptile' (auto_worker_setup_work()
 * returned TRUE). Counts as an arrival only when 'ptile' is the corridor
 * tile this worker holds. */
void deep_funnel_note_work(const struct player *pplayer,
                           const struct unit *punit,
                           const struct tile *ptile);

/* A city work request took this worker before the task source ran. */
void deep_funnel_note_request(const struct player *pplayer,
                              const struct unit *punit);

/* The safety pass in settler_cont just idled this worker mid-activity. */
void deep_funnel_note_flee(const struct player *pplayer,
                           const struct unit *punit);

/* End of this player's phase: age the ledger, accumulate work turns,
 * harvest finished corridor tiles, and print the funnel line. Called
 * from deep_do_last_activities() beside deep_trunk_census(). */
void deep_funnel_census(struct player *pplayer);

#endif /* FC__DEEPFUNNEL_H */
