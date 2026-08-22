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
#ifndef FC__DEEPIDLE_H
#define FC__DEEPIDLE_H

/* What a worker does once the improvement backlog is empty.
 *
 * The fortress instrumentation (STATUS 2026-08-22) measured the answer
 * deep gives today: it carpets its own city catchments with fortresses,
 * 10.9% of all working worker-turns, and it does so price-INSENSITIVELY
 * -- an ini that made a fortress 2.6x cheaper moved the want
 * distribution 4x down and moved the behaviour not at all. A want of
 * 1.5 wins as easily as a want of 13.5 when the alternative is zero.
 *
 * So the lever is not the fortress price, it is a POLICY for the
 * backlog-empty worker, and the first thing a policy needs is a
 * measurement of the demand it must stay ready for. Pollution is that
 * demand: it arrives without warning, it costs output every turn it
 * sits, and it is the reason an empire keeps workers it is not using.
 *
 * deep_pollution_census() measures it -- how many catchment tiles are
 * polluted at once, how long an event waits before anybody starts
 * cleaning it, and how often events arrive -- so the size of the
 * reserve is DERIVED from the concurrency rather than guessed.
 */

struct player;

void deep_pollution_census(struct player *pplayer);

#endif /* FC__DEEPIDLE_H */
