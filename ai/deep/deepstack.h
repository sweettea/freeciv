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
#ifndef FC__DEEPSTACK_H
#define FC__DEEPSTACK_H

/* The worker-stack census (W28).
 *
 * total_activity() (server/unittools.c) sums activity_count across
 * EVERY unit on a tile, so N workers on one job finish it in 1/N the
 * turns. W28's first half made the advisor's own tile auction able to
 * exploit that (autoworkers.c, merged 2026-07-25). Whether it ever
 * DOES was never measured: the acceptance evidence was a corpus
 * position and two end-of-game snapshots, which say a stack can form,
 * not how often one does or what it buys.
 *
 * This is the missing denominator, and it is deliberately a general
 * worker instrument rather than a corridor one -- the question "do
 * workers stack" cannot be answered from the corridor ledger, which
 * only ever sees corridor tiles.
 *
 * One ledger entry per LIVE WORKSITE: a tile some of this player's
 * workers are performing the same terrain activity on. Each entry is
 * followed from the turn work is first seen there to the turn the last
 * worker leaves, and then charged to one of two outcomes -- the
 * improvement appeared, or it did not.
 *
 *   sites    worksites opened
 *   turns    worksite-turns observed (the denominator of everything)
 *   n1..n5p  those turns split by how many own workers were working
 *            the tile that turn -- the workers-per-active-tile
 *            distribution, which is the shape W28 exists to change
 *   wturns   worker-turns spent on worksites (sum of the stack sizes)
 *   stk      worksite-turns with two or more workers
 *   done     worksites whose improvement appeared
 *   dsolo    of those, ones never worked by more than one worker
 *   dstk     of those, ones that were stacked at least once
 *   tsolo    total calendar turns those solo worksites took
 *   tstk     total calendar turns those stacked worksites took
 *   lost     worksites abandoned with no improvement to show
 *
 * tsolo/dsolo against tstk/dstk is the completion-time comparison, with
 * the caveat that it is observational: a stacked site is stacked
 * BECAUSE it is slow, so the stacked mean is expected to be the larger
 * of the two even when stacking helps. What the A/B reads is the
 * change in each between two arms of the same seeds, not the gap
 * between them within one arm.
 *
 * Nothing here is read back into a decision. */

#include "fc_types.h"

/* End of this player's phase: age the worksite ledger, harvest the ones
 * that ended, and print the census line. Called from
 * deep_do_last_activities() beside deep_funnel_census(). */
void deep_stack_census(struct player *pplayer);

#endif /* FC__DEEPSTACK_H */
