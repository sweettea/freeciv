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
  .site_depth_weight = 0.5,             /* PLACEHOLDER, untuned */
  .site_overlap_weight = 0.05,          /* PLACEHOLDER, untuned */
  .site_potential_discount = 0.5,       /* PLACEHOLDER, untuned */
  .site_special_bonus = 10.0,           /* PLACEHOLDER, untuned */
  .site_special_cluster = 10.0,         /* PLACEHOLDER, untuned */
  .horizon_capacity_weight = 1.0,       /* PLACEHOLDER, untuned */
  .horizon_growth_weight = 0.5,         /* PLACEHOLDER, untuned */
  .horizon_bundle_weight = 1.0,         /* PLACEHOLDER, untuned */
  .horizon_health_weight = 1.0,         /* PLACEHOLDER, untuned */
  .horizon_tech_weight = 1.0,           /* PLACEHOLDER, untuned */
  .site_claim_bonus = 8.0,              /* PLACEHOLDER, untuned */
  .site_pressure_factor = 0.3,          /* PLACEHOLDER, untuned */
  .diplomat_defence_value = 300.0,      /* corpus-derived, untuned */
  .overseas_fear = 0.5,                 /* PLACEHOLDER, untuned */
  .overseas_decay = 0.2,                /* PLACEHOLDER, untuned */
  .garrison_floor = 1,                  /* PLACEHOLDER, untuned */
  .garrison_floor_danger = 40,          /* PLACEHOLDER, untuned */
  .garrison_menace_slope = 1.0,         /* PLACEHOLDER, untuned */
  .menace_overseas_weight = 0.25,       /* PLACEHOLDER, untuned */
  .barbarian_fear = 30,                 /* PLACEHOLDER, untuned */
  .boat_shadow_fear = 25,               /* PLACEHOLDER, untuned */
  .upkeep_gold_weight = 3.0,            /* PLACEHOLDER, untuned */
  .upkeep_unit_weight = 0.5,            /* PLACEHOLDER, untuned */
  .rotation_power_ratio = 2.0,          /* PLACEHOLDER, untuned */
  .rotation_want = 35,                  /* PLACEHOLDER, untuned */
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
  .cm_grow_below = 6,                   /* PLACEHOLDER, untuned */
  .cm_grow_food = 16,                   /* PLACEHOLDER, untuned */
  .cm_grow_shield = 4,                  /* PLACEHOLDER, untuned */
  .cm_grow_trade = 4,                   /* PLACEHOLDER, untuned */
  .cm_trade_quality_slope = 0.5,        /* PLACEHOLDER, untuned */
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
  TUNE_INT(cm_grow_below);
  TUNE_INT(cm_grow_food);
  TUNE_INT(cm_grow_shield);
  TUNE_INT(cm_grow_trade);
  TUNE_DOUBLE(cm_trade_quality_slope);
  TUNE_DOUBLE(diplomat_defence_value);
  TUNE_DOUBLE(site_tail_weight);
  TUNE_DOUBLE(site_depth_weight);
  TUNE_DOUBLE(site_overlap_weight);
  TUNE_DOUBLE(site_potential_discount);
  TUNE_DOUBLE(site_special_bonus);
  TUNE_DOUBLE(site_special_cluster);
  TUNE_DOUBLE(site_claim_bonus);
  TUNE_DOUBLE(site_pressure_factor);
  TUNE_DOUBLE(horizon_capacity_weight);
  TUNE_DOUBLE(horizon_growth_weight);
  TUNE_DOUBLE(horizon_bundle_weight);
  TUNE_DOUBLE(horizon_health_weight);
  TUNE_DOUBLE(horizon_tech_weight);
  TUNE_DOUBLE(overseas_fear);
  TUNE_DOUBLE(overseas_decay);
  TUNE_INT(garrison_floor);
  TUNE_INT(garrison_floor_danger);
  TUNE_DOUBLE(garrison_menace_slope);
  TUNE_DOUBLE(menace_overseas_weight);
  TUNE_INT(barbarian_fear);
  TUNE_INT(boat_shadow_fear);
  TUNE_DOUBLE(upkeep_gold_weight);
  TUNE_DOUBLE(upkeep_unit_weight);
  TUNE_DOUBLE(rotation_power_ratio);
  TUNE_INT(rotation_want);

  secfile_destroy(sf);

  log_normal("deep-tuning file=\"%s\" fear=%f eta=%f surv=%f hut=%f"
             " disc=%f happy=%d wf=%d ws=%d wt=%d cf=%d cs=%d ct=%d"
             " wos=%d wot=%d gb=%d gf=%d gs=%d gt=%d tqs=%f dip=%f tail=%f depth=%f ovl=%f pot=%f"
             " spb=%f spc=%f claim=%f press=%f hcap=%f hgrow=%f hbun=%f hhealth=%f htech=%f"
             " osf=%f osd=%f gfl=%d gfd=%d gms=%f mow=%f bfear=%d bshadow=%d ugw=%f uuw=%f rpr=%f rwant=%d",
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
             deep_tuning_active.cm_wonder_trade,
             deep_tuning_active.cm_grow_below,
             deep_tuning_active.cm_grow_food,
             deep_tuning_active.cm_grow_shield,
             deep_tuning_active.cm_grow_trade,
             deep_tuning_active.cm_trade_quality_slope,
             deep_tuning_active.diplomat_defence_value,
             deep_tuning_active.site_tail_weight,
             deep_tuning_active.site_depth_weight,
             deep_tuning_active.site_overlap_weight,
             deep_tuning_active.site_potential_discount,
             deep_tuning_active.site_special_bonus,
             deep_tuning_active.site_special_cluster,
             deep_tuning_active.site_claim_bonus,
             deep_tuning_active.site_pressure_factor,
             deep_tuning_active.horizon_capacity_weight,
             deep_tuning_active.horizon_growth_weight,
             deep_tuning_active.horizon_bundle_weight,
             deep_tuning_active.horizon_health_weight,
             deep_tuning_active.horizon_tech_weight,
             deep_tuning_active.overseas_fear,
             deep_tuning_active.overseas_decay,
             deep_tuning_active.garrison_floor,
             deep_tuning_active.garrison_floor_danger,
             deep_tuning_active.garrison_menace_slope,
             deep_tuning_active.menace_overseas_weight,
             deep_tuning_active.barbarian_fear,
             deep_tuning_active.boat_shadow_fear,
             deep_tuning_active.upkeep_gold_weight,
             deep_tuning_active.upkeep_unit_weight,
             deep_tuning_active.rotation_power_ratio,
             deep_tuning_active.rotation_want);
}

/**********************************************************************//**
  Log level for the deep metric lines (deep-def/want/explore/loss/
  emergency). $FREECIV_DEEP_QUIET demotes them to verbose: pure tuning
  runs only need scorelogs, and on large maps the metric lines are the
  disk cost.
**************************************************************************/
enum log_level deep_metric_log_level(void)
{
  static int quiet = -1;

  if (quiet < 0) {
    const char *q = getenv("FREECIV_DEEP_QUIET");

    quiet = (q != NULL && q[0] != '\0') ? 1 : 0;
  }
  return quiet ? LOG_VERBOSE : LOG_NORMAL;
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
