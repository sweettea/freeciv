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
#include "player.h"
#include "unittype.h"

/* server/advisors */
#include "advchoice.h"

/* ai/default */
#include "daitools.h"

#include "deeptuning.h"

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
  Which producer class a choice belongs to, for the per-class scale.

  Read off `choice->type`, which every producer sets, rather than off
  the ADV_CHOICE_TRACK producer tag: the tag exists only in a
  census build (M3 item 6) and the scale has to work in the shipped
  one. The one distinction the type does not make is the ferry --
  daimilitary.c:1700 and deep's colony lift both file a hull as
  CT_ATTACKER -- so a transport-capable boat is separated out by the
  ruleset's own `transport_capacity`, which is what makes it a ferry
  rather than a warship.
**************************************************************************/
enum deep_want_class deep_want_class_of(const struct adv_choice *choice)
{
  switch (choice->type) {
  case CT_BUILDING:
    return DWC_BUILDING;
  case CT_DEFENDER:
    return DWC_DEFENDER;
  case CT_ATTACKER:
    if (choice->value.utype != NULL
        && choice->value.utype->transport_capacity > 0) {
      return DWC_FERRY;
    }
    return DWC_ATTACKER;
  case CT_CIVILIAN:
  case CT_NONE:
  case CT_LAST:
    break;
  }

  return DWC_OTHER;
}

/**********************************************************************//**
  The scale for one class. The building anchor and everything the
  merge seam does not own are 1.0.

  The two domestic classes with their own keys -- the founder and the
  worker -- are NOT scaled here. Their wants are scaled where they are
  FORMED, before domestic_advisor_choose_build() runs its own auction,
  because a factor applied to that advisor's WINNER can only ever
  change whether the domestic bid beats the military one; it cannot
  make a building outbid a settler inside the advisor, which is the
  comparison the July corpus says is broken.
**************************************************************************/
double deep_want_class_scale(enum deep_want_class cls)
{
  const struct deep_tuning *t = deep_tuning_get();

  switch (cls) {
  case DWC_DEFENDER:
    return t->scale_defender;
  case DWC_ATTACKER:
    return t->scale_attacker;
  case DWC_FERRY:
    return t->scale_ferry;
  case DWC_BUILDING:
  case DWC_OTHER:
    break;
  }

  return 1.0;
}

/**********************************************************************//**
  Apply the class scale to one choice, in place.

  NORMAL tier only, and the tier is re-derived rather than assumed:
  the emergency tier is the ruleset's own must-act signal and a
  deflator that leaked across it would let a rescale lose a city.
  A scale of exactly 1.0 does nothing at all and logs nothing, so
  the compiled defaults are byte-identical and the counter proves it.
**************************************************************************/
void deep_want_scale_apply(const struct player *pplayer,
                           const struct city *pcity,
                           struct adv_choice *choice, const char *who)
{
  struct deep_want w = deep_want_from_choice(choice);
  double scale = deep_want_class_scale(deep_want_class_of(choice));

  if (scale == 1.0 || w.tier == DWT_EMERGENCY || choice->want <= 0) {
    return;
  }

  log_base(deep_metric_log_level(),
           "deep-scale turn=%d player=\"%s\" city=%d who=\"%s\""
           " pick=\"%s\" class=%d scale=%.4f want=%.1f to=%.1f",
           game.info.turn, player_name(pplayer), pcity->id, who,
           adv_choice_rule_name(choice),
           (int) deep_want_class_of(choice), scale,
           (double) choice->want, (double) (choice->want * scale));

  choice->want = choice->want * scale;
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

  log_base(deep_metric_log_level(),
           "deep-want turn=%d city=%d producer=\"%s\" pick=\"%s\""
           " native=%.1f tier=%d value=%.1f",
           game.info.turn, pcity->id, producer,
           adv_choice_rule_name(choice),
           (double) choice->want, (int) w.tier, (double) w.value);
}
