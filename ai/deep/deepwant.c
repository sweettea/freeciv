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

/* common */
#include "city.h"
#include "game.h"

/* server/advisors */
#include "advchoice.h"

/* ai/default */
#include "daitools.h"

#include "deepwant.h"

/* Conversion at the boundary (W01 first landing). The maps are
 * deliberately rank-preserving: stock producers keep emitting their
 * native currencies, deep separates the priority TIER (stock encodes
 * it as a magnitude above DAI_WANT_BELOW_MIL_EMERGENCY) from the
 * VALUE, and later workstreams replace the per-producer identity maps
 * with real shield-equivalent valuations, calibrated from the
 * deep-want log this file emits. */

/**********************************************************************//**
  Convert a stock choice's want into (tier, value).
**************************************************************************/
struct deep_want deep_want_from_choice(const struct adv_choice *choice)
{
  struct deep_want w;

  if (choice->want >= DAI_WANT_BELOW_MIL_EMERGENCY) {
    w.tier = DWT_EMERGENCY;
    w.value = choice->want - DAI_WANT_BELOW_MIL_EMERGENCY;
  } else {
    w.tier = DWT_NORMAL;
    w.value = choice->want;
  }

  return w;
}

/**********************************************************************//**
  Write (tier, value) back into a stock choice, inverting
  deep_want_from_choice() exactly, for interop with stock consumers
  (dai_spend_gold's buy list, change_build_target logging).
**************************************************************************/
void deep_want_apply(struct adv_choice *choice, struct deep_want want)
{
  if (want.tier == DWT_EMERGENCY) {
    choice->want = want.value + DAI_WANT_BELOW_MIL_EMERGENCY;
  } else {
    choice->want = want.value;
  }
}

/**********************************************************************//**
  Compare wants: tier first, value second. Returns <0, 0, >0.
**************************************************************************/
int deep_want_cmp(struct deep_want a, struct deep_want b)
{
  if (a.tier != b.tier) {
    return a.tier < b.tier ? -1 : 1;
  }
  if (a.value != b.value) {
    return a.value < b.value ? -1 : 1;
  }
  return 0;
}

/**********************************************************************//**
  One line per final build decision: producer tag, native want, tier
  and value. This is the calibration corpus for replacing the identity
  conversions with real valuations.
**************************************************************************/
void deep_want_log(const struct city *pcity,
                   const struct adv_choice *choice)
{
  struct deep_want w = deep_want_from_choice(choice);
#ifdef ADV_CHOICE_TRACK
  const char *producer = choice->use ? choice->use : "-";
#else
  /* Producer tags ride on ADV_CHOICE_TRACK, which is off by default;
   * the pick name still identifies most producers for calibration. */
  const char *producer = "-";
#endif

  log_base(LOG_NORMAL,
           "deep-want turn=%d city=%d producer=\"%s\" pick=\"%s\""
           " native=%.1f tier=%d value=%.1f",
           game.info.turn, pcity->id, producer,
           adv_choice_rule_name(choice),
           (double) choice->want, (int) w.tier, (double) w.value);
}
