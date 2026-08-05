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
  .settler_risk_tolerance = 1.0,        /* PLACEHOLDER = stock bar */
  .settler_guard_tolerance = 1.0,       /* PLACEHOLDER = stock bar */
  .site_enough_result = 250.0,          /* PLACEHOLDER = stock */
  .founder_pool_per_site = 0.06,        /* PLACEHOLDER, see deeptuning.h */
  .committed_fear_mult = 1.0,           /* PLACEHOLDER = no effect */
  .incoming_defence_eta_weight = 0.5,   /* PLACEHOLDER, untuned */
  .incoming_defence_survival = 0.9,     /* PLACEHOLDER, untuned */
  .site_stream_weight = 1.0,            /* PLACEHOLDER, untuned */
  .land_pressure_tiles = 200,           /* PLACEHOLDER, untuned */
  .site_potential_discount = 0.5,       /* PLACEHOLDER, untuned */
  .site_special_bonus = 10.0,           /* PLACEHOLDER, untuned */
  .site_special_cluster = 10.0,         /* PLACEHOLDER, untuned */
  .horizon_capacity_weight = 1.0,       /* PLACEHOLDER, untuned */
  .horizon_growth_weight = 0.5,         /* PLACEHOLDER, untuned */
  .horizon_bundle_weight = 1.0,         /* PLACEHOLDER, untuned */
  .horizon_health_weight = 1.0,         /* PLACEHOLDER, untuned */
  .horizon_content_weight = 1.0,        /* PLACEHOLDER, untuned */
  .horizon_multiplier_weight = 1.0,     /* PLACEHOLDER, untuned */
  .site_travel_discount = 0.9,          /* human-set 2026-08-02, untuned */
  .site_travel_crowding = 0.0,          /* PLACEHOLDER, 0 = off */
  .site_potential_weight = 1.5,         /* = PERFECTION/2, untuned */
  .site_settler_cost_weight = 1.0,      /* = stock, untuned */
  .site_boat_cost_weight = 0.333333,    /* = stock 1/3, untuned */
  .site_rival_pressure = 10.0,          /* PLACEHOLDER, untuned */
  .site_rival_crowding = 0.9,           /* human-set 2026-08-02, untuned */
  .expansion_war_survival = 0.9,        /* PLACEHOLDER, untuned */
  .caravan_discount = 0.95,             /* = stock, untuned */
  .caravan_foreign_trade = 3,           /* PLACEHOLDER, untuned */
  .cm_war_survival = 0.9,               /* PLACEHOLDER, untuned */
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
  .density_fear_value = 6,              /* PLACEHOLDER, untuned */
  .density_fear_radius = 8,             /* PLACEHOLDER, untuned */
  .density_fear_ramp_turns = 0,         /* PLACEHOLDER, untuned; 0=off */
  .density_prior_value = 20,            /* PLACEHOLDER, untuned */
  .density_prior_tiles = 200,           /* PLACEHOLDER, untuned */
  .wall_fear_weight = 0.3,              /* PLACEHOLDER, untuned */
  .settler_rush_weight = 1.0,           /* PLACEHOLDER, untuned */
  .site_choke_bonus = 12.0,             /* PLACEHOLDER, untuned */
  .convoy_min_load = 2,                 /* PLACEHOLDER, untuned */
  .convoy_wait_radius = 6,              /* PLACEHOLDER, untuned */
  .lift_form_radius = 8,                /* PLACEHOLDER, untuned */
  .lift_gather_turns = 8,               /* PLACEHOLDER, untuned */
  .lift_cross_turns = 20,               /* PLACEHOLDER, untuned */
  .lift_op_slots = 0,                   /* MEASURED negative; off */
  .lift_manifest_max = 8,               /* human call 2026-08-04 */
  .lift_hitch_wait_ratio = 0.25,        /* PLACEHOLDER, untuned */
  .boat_idle_damping = 0.5,             /* PLACEHOLDER, untuned */
  .boat_water_gate = 1,                 /* geometry, not a magnitude */
  .boat_gate_at_bid = 1,                /* filter before the choice */
  .lift_drop_radius = 12,               /* human: two boat moves */
  .lift_hitch_wait_min = 1,             /* PLACEHOLDER, untuned */
  .worker_flee_radius = 3,              /* PLACEHOLDER, untuned */
  .worker_guard_radius = 6,             /* PLACEHOLDER, untuned */
  .worker_danger_discount = 1.0,        /* PLACEHOLDER, untuned */
  .worker_cross_continent_discount = 1.0, /* PLACEHOLDER, untuned */
  .upkeep_gold_weight = 3.0,            /* PLACEHOLDER, untuned */
  .upkeep_unit_weight = 0.5,            /* PLACEHOLDER, untuned */
  .rotation_power_ratio = 2.0,          /* PLACEHOLDER, untuned */
  .rotation_want = 35,                  /* PLACEHOLDER, untuned */
  .rotation_finish_relax = 0.5,         /* PLACEHOLDER, untuned */
  .embassy_science_value = 8.0,         /* PLACEHOLDER, untuned */
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
  .cm_grow_safety_margin = 1.0,         /* PLACEHOLDER, untuned */
  .sortie_defence_weight = 0.5,         /* PLACEHOLDER, untuned */
  .cm_grow_food = 16,                   /* PLACEHOLDER, untuned */
  .cm_grow_shield = 4,                  /* PLACEHOLDER, untuned */
  .cm_grow_trade = 4,                   /* PLACEHOLDER, untuned */
  .cm_trade_quality_slope = 0.5,        /* PLACEHOLDER, untuned */
  .survival_target = 0.5,               /* PLACEHOLDER, untuned */
  .survival_fear_value = 20.0,          /* PLACEHOLDER, untuned */
  .survival_horizon = 3,                /* PLACEHOLDER, untuned */
  .tired_attack_floor = 0.9,            /* PLACEHOLDER, untuned */
  .attack_value_weight = 1.0,           /* PLACEHOLDER, untuned */
  .sortie_win_floor = 0.85,             /* PLACEHOLDER, untuned */
  .op_take_target = 0.7,                /* PLACEHOLDER, untuned */
  .op_muster_wait = 10,                 /* PLACEHOLDER, untuned */
  .satisfied_fear_discount = 0.0,       /* PLACEHOLDER, untuned */
  .colony_lift_weight = 0.5,            /* PLACEHOLDER, untuned */
  .invasion_reinforce_rate = 0.34,      /* PLACEHOLDER, untuned */
  .op_wave_window = 6,                  /* PLACEHOLDER, untuned */
  .raid_radius = 12,                    /* PLACEHOLDER, untuned */
  .hut_risk_radius = 5,                 /* PLACEHOLDER, untuned */
  .hut_risk_discount = 0.25,            /* PLACEHOLDER, untuned */
  .op_abort_floor = 0.35,               /* PLACEHOLDER, untuned */
  .barb_reinforce_rate = 0.0,           /* PLACEHOLDER; T160 observed 0 */
  .stance_forced = -1,                  /* FREE: what ships */
  .stance_source = 0,                   /* Identity; E34.3 adds traits */
  .stance_trait_span = 0.5,             /* PLACEHOLDER, untuned */
  .stance_fear_gain = 1.0,              /* PLACEHOLDER, untuned */
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
  TUNE_DOUBLE(settler_risk_tolerance);
  TUNE_DOUBLE(settler_guard_tolerance);
  TUNE_DOUBLE(site_enough_result);
  TUNE_DOUBLE(founder_pool_per_site);
  TUNE_DOUBLE(committed_fear_mult);
  TUNE_DOUBLE(incoming_defence_eta_weight);
  TUNE_DOUBLE(incoming_defence_survival);
  TUNE_DOUBLE(embassy_science_value);
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
  TUNE_DOUBLE(cm_grow_safety_margin);
  TUNE_DOUBLE(sortie_defence_weight);
  TUNE_INT(cm_grow_food);
  TUNE_INT(cm_grow_shield);
  TUNE_INT(cm_grow_trade);
  TUNE_DOUBLE(cm_trade_quality_slope);
  TUNE_DOUBLE(diplomat_defence_value);
  TUNE_DOUBLE(site_stream_weight);
  TUNE_INT(land_pressure_tiles);
  TUNE_DOUBLE(site_potential_discount);
  TUNE_DOUBLE(site_special_bonus);
  TUNE_DOUBLE(site_special_cluster);
  TUNE_DOUBLE(site_claim_bonus);
  TUNE_DOUBLE(site_pressure_factor);
  TUNE_DOUBLE(horizon_capacity_weight);
  TUNE_DOUBLE(horizon_growth_weight);
  TUNE_DOUBLE(horizon_bundle_weight);
  TUNE_DOUBLE(horizon_health_weight);
  TUNE_DOUBLE(horizon_content_weight);
  TUNE_DOUBLE(horizon_multiplier_weight);
  TUNE_DOUBLE(site_travel_discount);
  TUNE_DOUBLE(site_travel_crowding);
  TUNE_DOUBLE(site_potential_weight);
  TUNE_DOUBLE(site_settler_cost_weight);
  TUNE_DOUBLE(site_boat_cost_weight);
  TUNE_DOUBLE(site_rival_pressure);
  TUNE_DOUBLE(site_rival_crowding);
  TUNE_DOUBLE(expansion_war_survival);
  TUNE_DOUBLE(caravan_discount);
  TUNE_INT(caravan_foreign_trade);
  TUNE_DOUBLE(cm_war_survival);
  TUNE_DOUBLE(horizon_tech_weight);
  TUNE_DOUBLE(overseas_fear);
  TUNE_DOUBLE(overseas_decay);
  TUNE_INT(garrison_floor);
  TUNE_INT(garrison_floor_danger);
  TUNE_DOUBLE(garrison_menace_slope);
  TUNE_DOUBLE(menace_overseas_weight);
  TUNE_INT(barbarian_fear);
  TUNE_INT(boat_shadow_fear);
  TUNE_INT(density_fear_value);
  TUNE_INT(density_fear_radius);
  TUNE_INT(density_fear_ramp_turns);
  TUNE_INT(density_prior_value);
  TUNE_INT(density_prior_tiles);
  TUNE_DOUBLE(wall_fear_weight);
  TUNE_DOUBLE(settler_rush_weight);
  TUNE_DOUBLE(site_choke_bonus);
  TUNE_INT(convoy_min_load);
  TUNE_INT(convoy_wait_radius);
  TUNE_INT(lift_form_radius);
  TUNE_INT(lift_gather_turns);
  TUNE_INT(lift_cross_turns);
  TUNE_INT(lift_op_slots);
  TUNE_INT(lift_manifest_max);
  TUNE_DOUBLE(lift_hitch_wait_ratio);
  TUNE_DOUBLE(boat_idle_damping);
  TUNE_INT(boat_water_gate);
  TUNE_INT(boat_gate_at_bid);
  TUNE_INT(lift_drop_radius);
  TUNE_INT(lift_hitch_wait_min);
  TUNE_INT(worker_flee_radius);
  TUNE_INT(worker_guard_radius);
  TUNE_DOUBLE(worker_danger_discount);
  TUNE_DOUBLE(worker_cross_continent_discount);
  TUNE_DOUBLE(upkeep_gold_weight);
  TUNE_DOUBLE(upkeep_unit_weight);
  TUNE_DOUBLE(rotation_power_ratio);
  TUNE_INT(rotation_want);
  TUNE_DOUBLE(rotation_finish_relax);
  TUNE_DOUBLE(survival_target);
  TUNE_DOUBLE(survival_fear_value);
  TUNE_INT(survival_horizon);
  TUNE_DOUBLE(tired_attack_floor);
  TUNE_DOUBLE(attack_value_weight);
  TUNE_DOUBLE(sortie_win_floor);
  TUNE_DOUBLE(op_take_target);
  TUNE_INT(op_muster_wait);
  TUNE_DOUBLE(satisfied_fear_discount);
  TUNE_DOUBLE(colony_lift_weight);
  TUNE_DOUBLE(invasion_reinforce_rate);
  TUNE_INT(op_wave_window);
  TUNE_INT(raid_radius);
  TUNE_INT(hut_risk_radius);
  TUNE_DOUBLE(hut_risk_discount);
  TUNE_DOUBLE(op_abort_floor);
  TUNE_DOUBLE(barb_reinforce_rate);
  TUNE_INT(stance_forced);
  TUNE_INT(stance_source);
  TUNE_DOUBLE(stance_trait_span);
  TUNE_DOUBLE(stance_fear_gain);

  secfile_destroy(sf);

  log_normal("deep-tuning file=\"%s\" fear=%f srt=%f cfm=%f eta=%f surv=%f emb=%f hut=%f"
             " disc=%f happy=%d wf=%d ws=%d wt=%d cf=%d cs=%d ct=%d"
             " wos=%d wot=%d gb=%d gsm=%f sdw=%f gf=%d gs=%d gt=%d tqs=%f dip=%f stream=%f pot=%f"
             " spb=%f spc=%f claim=%f press=%f hcap=%f hgrow=%f hbun=%f hhealth=%f hcont=%f cmws=%f htech=%f"
             " osf=%f osd=%f gfl=%d gfd=%d gms=%f mow=%f bfear=%d bshadow=%d dfv=%d dfr=%d dfrt=%d dpv=%d dpt=%d wfw=%f srw=%f chk=%f cml=%d cwr=%d wfr=%d wgr=%d wdd=%f wxcd=%f ugw=%f uuw=%f rpr=%f rwant=%d rfin=%f svt=%f svf=%f svh=%d taf=%f avw=%f swf=%f ott=%f omw=%d sfd=%f clw=%f irr=%f oww=%d rrad=%d hrr=%d hrd=%f oaf=%f brr=%f lfr=%d lgt=%d lct=%d los=%d lmm=%d lhwr=%f bid=%f bwg=%d bgab=%d ldr=%d lhwm=%d stf=%d sts=%d stsp=%f stfg=%f",
             path,
             deep_tuning_active.settler_fearfulness_mult,
             deep_tuning_active.settler_risk_tolerance,
             deep_tuning_active.committed_fear_mult,
             deep_tuning_active.incoming_defence_eta_weight,
             deep_tuning_active.incoming_defence_survival,
             deep_tuning_active.embassy_science_value,
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
             deep_tuning_active.cm_grow_safety_margin,
             deep_tuning_active.sortie_defence_weight,
             deep_tuning_active.cm_grow_food,
             deep_tuning_active.cm_grow_shield,
             deep_tuning_active.cm_grow_trade,
             deep_tuning_active.cm_trade_quality_slope,
             deep_tuning_active.diplomat_defence_value,
             deep_tuning_active.site_stream_weight,
             deep_tuning_active.site_potential_discount,
             deep_tuning_active.site_special_bonus,
             deep_tuning_active.site_special_cluster,
             deep_tuning_active.site_claim_bonus,
             deep_tuning_active.site_pressure_factor,
             deep_tuning_active.horizon_capacity_weight,
             deep_tuning_active.horizon_growth_weight,
             deep_tuning_active.horizon_bundle_weight,
             deep_tuning_active.horizon_health_weight,
             deep_tuning_active.horizon_content_weight,
             deep_tuning_active.cm_war_survival,
             deep_tuning_active.horizon_tech_weight,
             deep_tuning_active.overseas_fear,
             deep_tuning_active.overseas_decay,
             deep_tuning_active.garrison_floor,
             deep_tuning_active.garrison_floor_danger,
             deep_tuning_active.garrison_menace_slope,
             deep_tuning_active.menace_overseas_weight,
             deep_tuning_active.barbarian_fear,
             deep_tuning_active.boat_shadow_fear,
             deep_tuning_active.density_fear_value,
             deep_tuning_active.density_fear_radius,
             deep_tuning_active.density_fear_ramp_turns,
             deep_tuning_active.density_prior_value,
             deep_tuning_active.density_prior_tiles,
             deep_tuning_active.wall_fear_weight,
             deep_tuning_active.settler_rush_weight,
             deep_tuning_active.site_choke_bonus,
             deep_tuning_active.convoy_min_load,
             deep_tuning_active.convoy_wait_radius,
             deep_tuning_active.worker_flee_radius,
             deep_tuning_active.worker_guard_radius,
             deep_tuning_active.worker_danger_discount,
             deep_tuning_active.worker_cross_continent_discount,
             deep_tuning_active.upkeep_gold_weight,
             deep_tuning_active.upkeep_unit_weight,
             deep_tuning_active.rotation_power_ratio,
             deep_tuning_active.rotation_want,
             deep_tuning_active.rotation_finish_relax,
             deep_tuning_active.survival_target,
             deep_tuning_active.survival_fear_value,
             deep_tuning_active.survival_horizon,
             deep_tuning_active.tired_attack_floor,
             deep_tuning_active.attack_value_weight,
             deep_tuning_active.sortie_win_floor,
             deep_tuning_active.op_take_target,
             deep_tuning_active.op_muster_wait,
             deep_tuning_active.satisfied_fear_discount,
             deep_tuning_active.colony_lift_weight,
             deep_tuning_active.invasion_reinforce_rate,
             deep_tuning_active.op_wave_window,
             deep_tuning_active.raid_radius,
             deep_tuning_active.hut_risk_radius,
             deep_tuning_active.hut_risk_discount,
             deep_tuning_active.op_abort_floor,
             deep_tuning_active.barb_reinforce_rate,
             deep_tuning_active.lift_form_radius,
             deep_tuning_active.lift_gather_turns,
             deep_tuning_active.lift_cross_turns,
             deep_tuning_active.lift_op_slots,
             deep_tuning_active.lift_manifest_max,
             deep_tuning_active.lift_hitch_wait_ratio,
             deep_tuning_active.boat_idle_damping,
             deep_tuning_active.boat_water_gate,
             deep_tuning_active.boat_gate_at_bid,
             deep_tuning_active.lift_drop_radius,
             deep_tuning_active.lift_hitch_wait_min,
             deep_tuning_active.stance_forced,
             deep_tuning_active.stance_source,
             deep_tuning_active.stance_trait_span,
             deep_tuning_active.stance_fear_gain);
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
