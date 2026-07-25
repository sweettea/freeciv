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

#include <stdlib.h>

/* utility */
#include "log.h"
#include "registry.h"

#include "deeptuning.h"

/* Compiled-in defaults; every value is a PLACEHOLDER until it has been
 * through a benchmark/tune.py pass on training seeds. A file named by
 * $FREECIV_DEEP_TUNING overrides them (see deep_tuning_init()); the
 * benchmark result cache keys on that file's content (env TUNING in
 * benchmark/run.sh), so tuned and untuned results never mix. */
static const struct deep_tuning deep_tuning_defaults = {
  .settler_fearfulness_mult = 2.0,      /* PLACEHOLDER, untuned */
  .incoming_defence_eta_weight = 0.5,   /* PLACEHOLDER, untuned */
  .incoming_defence_survival = 0.9,     /* PLACEHOLDER, untuned */
  .site_tail_weight = 0.15,             /* PLACEHOLDER, untuned */
  .site_potential_discount = 0.5,       /* PLACEHOLDER, untuned */
  .site_special_bonus = 10.0,           /* PLACEHOLDER, untuned */
  .site_special_cluster = 10.0,         /* PLACEHOLDER, untuned */
  .diplomat_defence_value = 300.0,      /* corpus-derived, untuned */
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

static struct deep_tuning deep_tuning_active;
static bool deep_tuning_loaded = FALSE;

#define TUNE_DOUBLE(field) \
  deep_tuning_active.field \
    = secfile_lookup_float_default(sf, deep_tuning_defaults.field, \
                                   "tuning." #field)
#define TUNE_INT(field) \
  deep_tuning_active.field \
    = secfile_lookup_int_default(sf, deep_tuning_defaults.field, \
                                 "tuning." #field)

/**********************************************************************//**
  Load tuning overrides from $FREECIV_DEEP_TUNING, if set. Called once
  from module setup; unknown keys in the file are ignored by lookup,
  absent keys keep their compiled defaults.
**************************************************************************/
void deep_tuning_init(void)
{
  const char *path = getenv("FREECIV_DEEP_TUNING");
  struct section_file *sf;

  deep_tuning_active = deep_tuning_defaults;
  deep_tuning_loaded = TRUE;

  if (path == NULL || path[0] == '\0') {
    return;
  }

  sf = secfile_load(path, FALSE);
  if (sf == NULL) {
    log_error("deep: cannot load tuning file \"%s\"; using defaults",
              path);
    return;
  }

  TUNE_DOUBLE(settler_fearfulness_mult);
  TUNE_DOUBLE(incoming_defence_eta_weight);
  TUNE_DOUBLE(incoming_defence_survival);
  TUNE_DOUBLE(explore_hut_value);
  TUNE_DOUBLE(explore_discovery_value);
  TUNE_INT(cm_happy_factor);
  TUNE_INT(cm_war_food);
  TUNE_INT(cm_war_shield);
  TUNE_INT(cm_war_trade);
  TUNE_INT(cm_cap_food);
  TUNE_INT(cm_cap_shield);
  TUNE_INT(cm_cap_trade);
  TUNE_INT(cm_wonder_shield);
  TUNE_INT(cm_wonder_trade);

  secfile_destroy(sf);

  log_normal("deep-tuning file=\"%s\" fear=%f eta=%f surv=%f hut=%f"
             " disc=%f happy=%d wf=%d ws=%d wt=%d cf=%d cs=%d ct=%d"
             " wos=%d wot=%d",
             path,
             deep_tuning_active.settler_fearfulness_mult,
             deep_tuning_active.incoming_defence_eta_weight,
             deep_tuning_active.incoming_defence_survival,
             deep_tuning_active.explore_hut_value,
             deep_tuning_active.explore_discovery_value,
             deep_tuning_active.cm_happy_factor,
             deep_tuning_active.cm_war_food,
             deep_tuning_active.cm_war_shield,
             deep_tuning_active.cm_war_trade,
             deep_tuning_active.cm_cap_food,
             deep_tuning_active.cm_cap_shield,
             deep_tuning_active.cm_cap_trade,
             deep_tuning_active.cm_wonder_shield,
             deep_tuning_active.cm_wonder_trade);
}

/**********************************************************************//**
  The active tuning values.
**************************************************************************/
const struct deep_tuning *deep_tuning_get(void)
{
  if (!deep_tuning_loaded) {
    deep_tuning_init();
  }
  return &deep_tuning_active;
}
