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

#include "deeptuning.h"

/* W25 will load these from a file keyed into the benchmark result
 * cache; until then the compiled-in placeholders below are the single
 * source of truth. */
static const struct deep_tuning deep_tuning_defaults = {
  .settler_fearfulness_mult = 2.0,      /* PLACEHOLDER, untuned */
  .incoming_defence_eta_weight = 0.5,   /* PLACEHOLDER, untuned */
  .incoming_defence_survival = 0.9,     /* PLACEHOLDER, untuned */
  .explore_hut_value = 15.0,            /* PLACEHOLDER, untuned */
  .explore_discovery_value = 0.05,      /* PLACEHOLDER, untuned */
  .cm_happy_factor = 1,                 /* PLACEHOLDER, untuned */
  .cm_war_food = 10,                    /* PLACEHOLDER, untuned */
  .cm_war_shield = 12,                  /* PLACEHOLDER, untuned */
  .cm_war_trade = 1,                    /* PLACEHOLDER, untuned */
  .cm_cap_food = 2,                     /* PLACEHOLDER, untuned */
  .cm_cap_shield = 8,                   /* PLACEHOLDER, untuned */
  .cm_cap_trade = 8,                    /* PLACEHOLDER, untuned */
  .cm_wonder_shield = 10,               /* PLACEHOLDER, untuned */
  .cm_wonder_trade = 4,                 /* PLACEHOLDER, untuned */
};

/**********************************************************************//**
  The active tuning values.
**************************************************************************/
const struct deep_tuning *deep_tuning_get(void)
{
  return &deep_tuning_defaults;
}
