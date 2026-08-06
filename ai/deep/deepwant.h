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
#ifndef FC__DEEPWANT_H
#define FC__DEEPWANT_H

/* common */
#include "fc_types.h"

struct adv_choice;
struct city;

/* One currency for every want (W01): discounted shield-equivalent
 * output. Stock advisors price their wants in incommensurable units
 * (output weightings, danger percentages, 1000-scale emergency tiers,
 * sign-overloaded boat flags); deep converts each at the aggregation
 * boundary. Priority is a TIER, never a magnitude. */
typedef double want_t;

enum deep_want_tier {
  DWT_NORMAL = 0,
  DWT_EMERGENCY,        /* Must-act: outranks any NORMAL value */
};

struct deep_want {
  want_t value;
  enum deep_want_tier tier;
};

/* The producer classes a scale can act on. Buildings are the ANCHOR:
 * they have no key and are always 1.0, because a rescale is only
 * meaningful relative to something held fixed, and the building want
 * is the one currency in the auction that is a weighted-output sum
 * (advbuilding.h) rather than a percentage, a trait product or a
 * 1000-scale tier magnitude. */
enum deep_want_class {
  DWC_BUILDING = 0,     /* the anchor; scale is 1.0 by definition */
  DWC_DEFENDER,
  DWC_ATTACKER,
  DWC_FERRY,
  DWC_OTHER,            /* civilians reached at the merge seam */
};

struct deep_want deep_want_from_choice(const struct adv_choice *choice);
void deep_want_apply(struct adv_choice *choice, struct deep_want want);
int deep_want_cmp(struct deep_want a, struct deep_want b);
void deep_want_log(const struct city *pcity,
                   const struct adv_choice *choice);
enum deep_want_class deep_want_class_of(const struct adv_choice *choice);
double deep_want_class_scale(enum deep_want_class cls);
void deep_want_scale_apply(const struct player *pplayer,
                           const struct city *pcity,
                           struct adv_choice *choice, const char *who);

#endif /* FC__DEEPWANT_H */
