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
#ifndef FC__DEEPWONDER_H
#define FC__DEEPWONDER_H

/* common */
#include "fc_types.h"

struct ai_type;
struct city;
struct impr_type;
struct player;

/* Turns of OUR OWN research until `goal` is known, or -1 when the goal
 * is already known, unreachable, or we are producing no bulbs. Own
 * research only, deliberately: readable, and no rival tech modelling
 * (the human's framing, 2026-08-05). */
int deep_tech_eta(const struct player *pplayer, Tech_type_id goal);

/* Turns until this improvement goes obsolete FOR US, or -1 when it
 * never does within what we can estimate. Reads the improvement's own
 * obsolete_by requirement vector and prices only the Player-range
 * present Tech requirements -- the ones our own research triggers. */
int deep_impr_obsolete_eta(const struct player *pplayer,
                           const struct impr_type *pimprove);

/* THE ONE FUNCTION W04 SUBSTITUTES. The best rival's completion ETA
 * for this wonder, read omnisciently today: every other alive player's
 * cities, their current production and their shield stock. -1 when
 * nobody else is building it. Under the honesty flag this becomes an
 * estimate over known_city and everything above it inherits the
 * staleness through this seam and no other. */
int deep_wonder_rival_eta(const struct player *pplayer,
                          const struct impr_type *pimprove,
                          const struct player **who_out);

/* Our own completion ETA for this wonder in this city, shield stock
 * and production-change penalty included. -1 when it cannot be built
 * here or the city makes no shields. */
int deep_wonder_own_eta(struct city *pcity,
                        const struct impr_type *pimprove);

/* P(we finish first), from the margin in turns against the best rival
 * and wrace_margin_turns as the scale. 1.0 with no rival, 0.0 when we
 * lose by more than the margin -- which is exactly the go/no-go bar. */
double deep_wonder_p_win(struct city *pcity,
                         const struct impr_type *pimprove,
                         int *ours_out, int *theirs_out,
                         const struct player **who_out);

/* Turns of our own research until the unit type that OBSOLETES `ut`
 * becomes buildable, or -1 when it has no successor, the successor is
 * already available, or no estimate is possible. The MAXIMUM over the
 * successor's unknown advance requirements, because a unit needs all
 * of them. */
int deep_utype_successor_eta(const struct player *pplayer,
                             const struct unit_type *ut,
                             const struct unit_type **next_out);

/* The per-player race pass: adjudicate finished races, price every
 * great wonder the wonder city could build, and (with the gains open)
 * apply P(win), the completion-ETA discount and the obsolescence
 * truncation to its building want. Inert at every gain 0. */
void deep_wonder_race(struct ai_type *ait, struct player *pplayer);

/* Race urgency for the help-wonder caravan (R2b's delivery arm): 1.0
 * off the pace, rising toward wrace_urgency_gain as a race we lead
 * gets close. Reads the same estimator as everything else here. */
double deep_wonder_race_urgency(struct city *wonder_city,
                                const struct impr_type *pimprove);

#endif /* FC__DEEPWONDER_H */
