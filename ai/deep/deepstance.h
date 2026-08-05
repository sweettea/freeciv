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
#ifndef FC__DEEPSTANCE_H
#define FC__DEEPSTANCE_H

/* utility */
#include "support.h"            /* bool */

/* The stance layer (W34 phase A): what a player is trying to win,
 * expressed as a WEIGHT PER AXIS rather than a value of one enum.
 * The three run simultaneously -- "what shape is the empire" and
 * "what is force for" are not alternatives (findings/
 * doctrine-architecture.md section 1) -- so every consumer reads a
 * RATIO between axes and is identity when the axes are equal.
 *
 * Identity is 1.0 on every axis. That is not a default to be improved
 * on later: it is the acceptance gate (W34 A.6). With the axes equal,
 * every gate must multiply by exactly 1.0 and the binary must be
 * BYTE-IDENTICAL to the one without the stance layer.
 *
 * The culture axis is DECLARED AND INERT on purpose (W34 A.1c):
 * culture victory is off in every cell we run, culture is worth
 * culture/50 in get_civ_score(), and the lever is already priced by
 * stock. It exists so a third axis does not have to be retrofitted
 * through every gate later; nothing reads it yet. */
struct deep_stance {
  double econ;                  /* Growth, infrastructure, trade, expansion */
  double mil;                   /* Force: garrison, deterrence, campaigns */
  double cult;                  /* Culture and prestige -- INERT, see above */
};

/* The named presets survive ONLY as labels: for logging, for talking
 * about behaviour, and as the corners the tuning file can pin every
 * seat to. Nothing branches on the name except the pin itself, which
 * has to translate an integer key into axis weights somewhere. */
enum deep_stance_preset {
  DEEP_STANCE_BALANCED = 0,     /* Identity: today's game exactly */
  DEEP_STANCE_ECONOMIC = 1,     /* (hi, lo, .) */
  DEEP_STANCE_MILITARY = 2,     /* (lo, hi, .) */
  DEEP_STANCE_PRESET_LAST
};

struct player;

struct deep_stance deep_stance_identity(void);
bool deep_stance_is_identity(const struct deep_stance *st);
bool deep_stance_pinned(void);
const char *deep_stance_preset_name(int preset);
const char *deep_stance_source_name(int source);

void deep_stance_refresh(struct player *pplayer);
void deep_stance_log(struct player *pplayer);

/* The four gates (W34 A.2), each a multiplier on an EXISTING want or
 * bar and each exactly 1.0 whenever the axes are equal. Gate 1 reads
 * (mil/econ) because fear is what a military weight raises; gates 2-4
 * read (econ/mil). No gate may push a want below a survival floor. */
double deep_stance_fear_mult(const struct player *pplayer);
double deep_stance_op_mult(const struct player *pplayer);
double deep_stance_growth_mult(const struct player *pplayer);
double deep_stance_expand_mult(const struct player *pplayer);

#endif /* FC__DEEPSTANCE_H */
