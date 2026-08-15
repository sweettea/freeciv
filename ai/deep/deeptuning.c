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
#include <string.h>

/* utility */
#include "log.h"
#include "registry.h"

/* common */
#include "player.h"

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
  .site_defence_weight = 0.0,           /* MEASURED off: D3 contested A/B */
  .site_naval_gate = 0,                 /* MEASURED off: D3 isolated-150 */
  .site_wait_discount = 0.0,            /* PLACEHOLDER = off */
  .site_naval_weight = 20.0,            /* PLACEHOLDER = NAVAL_EMPHASIS */
  .site_naval_connect_weight = 0.0,     /* PLACEHOLDER off: the split's A/B */
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
  .expansion_gate_enabled = 1,          /* 1 = gate on, as shipped */
  .caravan_discount = 0.95,             /* = stock, untuned */
  .caravan_foreign_trade = 3,           /* PLACEHOLDER, untuned */
  .cm_war_survival = 0.9,               /* PLACEHOLDER, untuned */
  .cm_war_growth_charge = 0.0,          /* PLACEHOLDER, 0 = off */
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
  .floor_barb_weight = 1.0,             /* PLACEHOLDER, untuned */
  .floor_sea_weight = 1.0,              /* PLACEHOLDER, untuned */
  .floor_air_weight = 1.0,              /* PLACEHOLDER, untuned */
  .floor_border_weight = 1.0,           /* PLACEHOLDER, untuned */
  /* Same magnitude as menace_overseas_weight, which is this project's
   * existing "it cannot reach us properly" discount, rather than a
   * separately invented number. */
  .floor_interior_weight = 0.25,        /* PLACEHOLDER, untuned */
  .floor_marines_mult = 1.5,            /* PLACEHOLDER, untuned */
  .barbarian_fear = 30,                 /* PLACEHOLDER, untuned */
  .barb_anticipation_turns = 10,        /* PLACEHOLDER, untuned */
  /* PLACEHOLDER, untuned. Normalisation only: at this many
   * cities under BARBS_NORMAL the shared scaling is 1.0, so the
   * two bases above keep the level they shipped with. */
  .barb_size_ref_cities = 10,
  .boat_shadow_fear = 25,               /* PLACEHOLDER, untuned */
  .density_fear_value = 6,              /* PLACEHOLDER, untuned */
  .density_fear_radius = 8,             /* PLACEHOLDER, untuned */
  .density_fear_ramp_turns = 0,         /* PLACEHOLDER, untuned; 0=off */
  .density_prior_value = 20,            /* PLACEHOLDER, untuned */
  .density_prior_tiles = 200,           /* PLACEHOLDER, untuned */
  .wall_fear_weight = 0.3,              /* PLACEHOLDER, untuned */
  .birth_guard_gain = 0.0,              /* PLACEHOLDER, 0 = off */
  .defensive_techwall_weight = 0.0,     /* PLACEHOLDER, 0 = off */
  .attacker_hold_damper = 0.0,          /* PLACEHOLDER, 0 = off */
  .diplomat_pump_damper = 0.0,          /* PLACEHOLDER, 0 = off */
  .wrace_margin_turns = 10,             /* PLACEHOLDER, the ramp SCALE */
  .wrace_pwin_gain = 0.0,               /* PLACEHOLDER, 0 = off */
  .wrace_eta_gain = 0.0,                /* PLACEHOLDER, 0 = off */
  .wrace_obsolete_gain = 0.0,           /* PLACEHOLDER, 0 = off */
  .wrace_urgency_gain = 0.0,            /* PLACEHOLDER, 0 = off */
  .expiry_building_gain = 0.0,          /* PLACEHOLDER, 0 = off */
  .expiry_unit_gain = 0.0,              /* PLACEHOLDER, 0 = off */
  .expiry_unit_gap = 2,                 /* PLACEHOLDER, the hysteresis */
  .sell_defend_gain = 0.0,              /* PLACEHOLDER, 0 = off */
  .late_buy_gate = 0,                   /* PLACEHOLDER, 0/1 off-switch */
  .evacuate_gate = 0,                   /* PLACEHOLDER, 0/1 off-switch */
  .settler_rush_weight = 1.0,           /* PLACEHOLDER, untuned */
  .site_choke_bonus = 12.0,             /* PLACEHOLDER, untuned */
  .base_fort_gain = 40.0,               /* PLACEHOLDER, untuned */
  .fortify_ground = 1,                  /* PLACEHOLDER, untuned */
  .pillage_denial_gain = 1.0,           /* PLACEHOLDER, untuned */
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
  .lift_muster_release = 0,             /* off; A/B switch */
  .founder_berth_route = 0,             /* off; A/B switch */
  .lift_wait_tail_pct = 80,             /* measured: 20% of spells, 60%
                                         * of waiting */
  .worker_flee_radius = 3,              /* PLACEHOLDER, untuned */
  .worker_guard_radius = 6,             /* PLACEHOLDER, untuned */
  .worker_danger_discount = 1.0,        /* PLACEHOLDER, untuned */
  .worker_cross_continent_discount = 1.0, /* PLACEHOLDER, untuned */
  .worker_travel_discount = 0.0,       /* PLACEHOLDER, off by default */
  .trunk_road_gain = 0.0,              /* PLACEHOLDER, off by default */
  .trunk_source_gain = 0.0,            /* PLACEHOLDER, off by default */
  .upkeep_gold_weight = 0.125,          /* PLACEHOLDER, untuned */
  .upkeep_unit_weight = 0.5,            /* PLACEHOLDER, untuned */
  .rotation_power_ratio = 2.0,          /* PLACEHOLDER, untuned */
  .rotation_want = 35,                  /* PLACEHOLDER, untuned */
  .rotation_finish_relax = 0.5,         /* PLACEHOLDER, untuned */
  .embassy_science_value = 8.0,         /* PLACEHOLDER, untuned */
  .explore_hut_value = 15.0,            /* PLACEHOLDER, untuned */
  .explore_discovery_value = 0.05,      /* PLACEHOLDER, untuned */
  .explore_explorer_claim = 1.0,        /* PLACEHOLDER, untuned */
  .explore_salvage_gap = 1.0,           /* PLACEHOLDER, untuned */
  .explore_retain_floor = 1,            /* PLACEHOLDER, untuned */
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
  .cm_founder_food = 0.0,               /* PLACEHOLDER = off */
  .cm_trade_quality_slope = 0.5,        /* PLACEHOLDER, untuned */
  .cm_grow_price_blend = 0.0,           /* PLACEHOLDER, 0 = off */
  .cm_specialist_gold_gain = 0.0,       /* PLACEHOLDER, 0 = off */
  .cm_specialist_sci_gain = 0.0,        /* PLACEHOLDER, 0 = off */
  .survival_target = 0.5,               /* PLACEHOLDER, untuned */
  .survival_fear_value = 20.0,          /* PLACEHOLDER, untuned */
  .survival_horizon = 3,                /* PLACEHOLDER, untuned */
  .survival_pf_reach = 0,               /* 0 = straight-line reach */
  .threat_pin_discount = 0.0,           /* 0 = off; measured 0.687 */
  .tired_attack_floor = 0.9,            /* PLACEHOLDER, untuned */
  .attack_value_weight = 1.0,           /* PLACEHOLDER, untuned */
  .attack_loss_gain = 1.0,              /* PLACEHOLDER, untuned */
  .conquest_worth_mult = 2.0,           /* human band ~1.9-2.2 */
  .conquest_culture_gain = 0.0,         /* PLACEHOLDER, 0 = off */
  .defence_worth_mult = 2.0,            /* symmetric: tilt = ratio */
  .assault_pool_reach = 1,              /* geometry, not a magnitude */
  .sortie_win_floor = 0.85,             /* PLACEHOLDER, untuned */
  .sortie_prize_gain = 0.0,             /* PLACEHOLDER, 0 = off */
  .deterrence_gain = 0.0,               /* PLACEHOLDER, 0 = off */
  .attacker_garrison_gain = 0.0,        /* PLACEHOLDER, 0 = off */
  .cover_removal = 0,                   /* 0 = the local hole exactly */
  .unit_upkeep_charge = 0.0,            /* PLACEHOLDER, 0 = off */
  .pop_insurance_gain = 0.0,            /* PLACEHOLDER, 0 = off */
  .heal_stream_gain = 0.0,              /* PLACEHOLDER, 0 = off */
  .veteran_stream_gain = 0.0,           /* PLACEHOLDER, 0 = off */
  .op_take_target = 0.7,                /* PLACEHOLDER, untuned */
  .op_muster_wait = 10,                 /* PLACEHOLDER, untuned */
  .op_draft_marginal = 0,               /* 0 = the absolute floor */
  .op_muster = 0,                       /* 0 = odds gate formation (ERA 3b) */
  .op_muster_stall = 10,                /* PLACEHOLDER, = op_muster_wait */
  .op_muster_home = 1,                  /* the trigger as designed */
  .op_launch_odds = 1,                  /* the bar, not a magnitude */
  .op_net_value = 1,                    /* human call 2026-08-06 */
  .op_hold_survivors = 1.0,             /* PLACEHOLDER, untuned */
  .satisfied_fear_discount = 0.0,       /* PLACEHOLDER, untuned */
  .colony_lift_weight = 0.5,            /* PLACEHOLDER, untuned */
  .hull_commission_weight = 0.0,        /* off; A/B switch */
  .invasion_reinforce_rate = 0.34,      /* PLACEHOLDER, untuned */
  .invasion_reinforce_slope = 0.0,      /* PLACEHOLDER; 0 = the flat model */
  .invasion_embark_turns = 2,           /* PLACEHOLDER = the shipped literal */
  .op_wave_window = 6,                  /* PLACEHOLDER, untuned */
  .raid_radius = 12,                    /* PLACEHOLDER, untuned */
  .hut_risk_radius = 5,                 /* PLACEHOLDER, untuned */
  .hut_risk_discount = 0.25,            /* PLACEHOLDER, untuned */
  .hut_risk_honest = 0,                 /* Off: today's sign, pending A/B */
  .op_abort_floor = 0.35,               /* PLACEHOLDER, untuned */
  .barb_reinforce_rate = 0.0,           /* PLACEHOLDER; T160 observed 0 */
  .stance_forced = -1,                  /* FREE: what ships */
  .stance_source = 0,                   /* Identity; E34.3 adds traits */
  .stance_trait_span = 0.5,             /* PLACEHOLDER, untuned */
  .stance_fear_gain = 1.0,              /* PLACEHOLDER, untuned */
  .stance_op_gain = 1.0,                /* PLACEHOLDER, untuned */
  .stance_growth_gain = 1.0,            /* PLACEHOLDER, untuned */
  .stance_expand_gain = 1.0,            /* PLACEHOLDER, untuned */
  .stance_assault_gain = 1.0,           /* PLACEHOLDER, untuned */
  .stance_floor_gain = 1.0,             /* PLACEHOLDER, untuned */
  .stance_position_gain = 0.5,          /* PLACEHOLDER, untuned */
  .stance_techlean_gain = 0.5,          /* PLACEHOLDER, untuned */
  .stance_pressure_ref = 4.0,           /* PLACEHOLDER, untuned */
  .spy_enabled = 0,                     /* Off: stock's ladder decides */
  .spy_gold_value = 0.5,                /* PLACEHOLDER, untuned */
  .spy_sabotage_gain = 0.5,             /* PLACEHOLDER, untuned */
  .rates_enabled = 0,                   /* PLACEHOLDER, 0 = stock's ladder */
  .rates_science_gain = 1.0,            /* PLACEHOLDER, untuned */
  .rates_luxury_gain = 1.0,             /* PLACEHOLDER, untuned */
  .gov_valuation = 0,                   /* Off: stock's gov_value forwarded */
  .gov_anarchy_weight = 1.0,            /* Measured loss at face value */
  .gov_rate_ceiling_gain = 0.0,         /* PLACEHOLDER, 0 = off */
  .trade_merit = 0,                     /* PLACEHOLDER, 0 = stock's bid */
  .trade_merit_gain = 1.0,              /* PLACEHOLDER, untuned */
  .trade_ceasefire_peace_odds = 0.0,    /* PLACEHOLDER, 0 = no optimism */
  .trade_route_risk_gain = 0.0,         /* PLACEHOLDER, 0 = no discount */
  /* ON by compiled default -- human call 2026-08-06: "remove the love
   * bar, trade want anchors love". Ships as a PAIR with
   * peace_seek_gain below; see deeptuning.h. PLACEHOLDER value. */
  .trade_route_love = 5.0,
  .trade_partner_target_discount = 0.0, /* PLACEHOLDER, 0 = opportunist */
  .wonder_merit = 0,                    /* PLACEHOLDER, 0 = stock's bid */
  .wonder_merit_gain = 1.0,             /* PLACEHOLDER, untuned */
  .pay_switch_penalty = 1,              /* human call 2026-08-06: honest */
  .switch_cost_price = 1,               /* human call 2026-08-06: priced */
  .switch_cost_gain = 1.0,              /* PLACEHOLDER, untuned */
  .scale_settler = 1.0,                 /* PLACEHOLDER, 1.0 = no rescale */
  .scale_worker = 1.0,                  /* PLACEHOLDER, 1.0 = no rescale */
  .scale_defender = 1.0,                /* PLACEHOLDER, 1.0 = no rescale */
  .scale_attacker = 1.0,                /* PLACEHOLDER, 1.0 = no rescale */
  .scale_ferry = 1.0,                   /* PLACEHOLDER, 1.0 = no rescale */
  .emergency_value_cap = 0.0,           /* PLACEHOLDER, 0 = no cap */
  /* Same human call: the pre-pass runs and the love precondition is
   * waived. PLACEHOLDER value -- 1.0 is the setting PEACE measured its
   * population at, not a tuned optimum. */
  .peace_seek_gain = 1.0,
  .diplo_deep_only_pacts = 0,           /* PLACEHOLDER, 0 = off */
  .gift_border_value = 0.0,             /* PLACEHOLDER, 0 = off */
  .gift_love_gain = 0.0,                /* PLACEHOLDER, 0 = off */
  /* GIFT-REFOLD: 1.0 = every class priced as today = identity. */
  .gift_ally_gain = 1.0,
  .gift_instrument_gain = 1.0,
  .gift_rival_gain = 1.0,
  .proxy_arm_gain = 0.0,                /* PLACEHOLDER, 0 = off */
  .embassy_price_gain = 0.0,            /* PLACEHOLDER, 0 = off */
  .embassy_give_gain = 1.0,             /* PLACEHOLDER, untuned */
  .leader_check_gain = 0.0,             /* PLACEHOLDER, 0 = off */
  .endgame_horizon = 0,                 /* PLACEHOLDER, 0 = off */
  .endgame_window = 0,                  /* PLACEHOLDER, 0 = off */
  .endgame_unfinished_gain = 1.0,       /* PLACEHOLDER, 1.0 = off */
  .endgame_wonder_gain = 1.0,           /* PLACEHOLDER, 1.0 = off */
  .horizon_decay_gate = 0,              /* 0 = off = legacy rectangle */
  .horizon_decay_early = 0.0,           /* PLACEHOLDER; measured 0.037 */
  .horizon_decay_late = 0.0,            /* PLACEHOLDER; measured 0.002 */
  .horizon_decay_break_start = 50,      /* census's measured phase edge */
  .horizon_decay_break_end = 100,       /* census's measured phase edge */
  .horizon_govcenter_weight = 0.0,      /* PLACEHOLDER, 0 = off */
  .horizon_upkeepfree_weight = 0.0,     /* PLACEHOLDER, 0 = off */
  .horizon_culture_weight = 0.0,        /* PLACEHOLDER, 0 = off */
  .buy_per_gold = 0,                    /* PLACEHOLDER, 0 = off */
  .honest_info = 0,                     /* 0 = off = omniscient, as shipped */
  .techgoal_deep = 0,                   /* 0 = stock selection, byte-identical */
  .techgoal_distance_power = 1.0,       /* PLACEHOLDER, stock's shape; only
                                         * read when techgoal_deep != 0 */
};

static struct deep_tuning deep_tuning_active;
static bool deep_tuning_loaded = FALSE;

/* The per-seat layer (see the loading scheme in deeptuning.h). Every
 * slot starts UNRESOLVED and resolves on its first read, because the
 * player_alloc hook runs before the player has a name to key on.
 * `resolved` is what tells "no arena entry for this seat" (points at
 * the base values, deliberately) from "not looked up yet". */
static const struct deep_tuning *deep_tuning_seat[MAX_NUM_PLAYER_SLOTS];
static bool deep_tuning_seat_resolved[MAX_NUM_PLAYER_SLOTS];

/* The seat whose decision is being made. Set by deepai.c's hook
 * wrappers; NULL outside any of them, which is not an error in the
 * single-file case (every seat's values ARE the base values) and is a
 * counted coverage defect when an arena is configured. */
static const struct deep_tuning *deep_tuning_scope = NULL;
static int deep_tuning_scopeless = 0;

/* Candidate files, cached by path: eight seats on one file share one
 * struct rather than eight copies of it. */
#define DEEP_ARENA_FILES_MAX 16
static struct {
  char path[512];
  struct deep_tuning val;
} deep_arena_file[DEEP_ARENA_FILES_MAX];
static int deep_arena_files = 0;

/* The manifest, loaded at most once per process. */
static struct section_file *deep_arena_manifest = NULL;
static bool deep_arena_manifest_tried = FALSE;

/* Every tunable, once, in one list. The loader and the log line are
 * both generated from it: a knob that can be set from the file and
 * cannot be read back from the log is the exact hazard that cost
 * this project a 40-game batch, and four economy knobs were in that
 * state when this list was written. Adding a knob in one place now
 * adds it in both. */
#define DEEP_TUNING_FIELDS \
  TUNE_DOUBLE(settler_fearfulness_mult) \
  TUNE_DOUBLE(settler_risk_tolerance) \
  TUNE_DOUBLE(settler_guard_tolerance) \
  TUNE_DOUBLE(site_enough_result) \
  TUNE_DOUBLE(site_defence_weight) \
  TUNE_INT(site_naval_gate) \
  TUNE_DOUBLE(site_wait_discount) \
  TUNE_DOUBLE(site_naval_weight) \
  TUNE_DOUBLE(site_naval_connect_weight) \
  TUNE_DOUBLE(founder_pool_per_site) \
  TUNE_DOUBLE(committed_fear_mult) \
  TUNE_DOUBLE(incoming_defence_eta_weight) \
  TUNE_DOUBLE(incoming_defence_survival) \
  TUNE_DOUBLE(embassy_science_value) \
  TUNE_DOUBLE(explore_hut_value) \
  TUNE_DOUBLE(explore_discovery_value) \
  TUNE_DOUBLE(explore_explorer_claim) \
  TUNE_DOUBLE(explore_salvage_gap) \
  TUNE_INT(explore_retain_floor) \
  TUNE_INT(cm_happy_factor) \
  TUNE_INT(cm_war_food) \
  TUNE_INT(cm_war_shield) \
  TUNE_INT(cm_war_trade) \
  TUNE_INT(cm_cap_food) \
  TUNE_INT(cm_cap_shield) \
  TUNE_INT(cm_cap_trade) \
  TUNE_INT(cm_wonder_shield) \
  TUNE_INT(cm_wonder_trade) \
  TUNE_INT(cm_grow_below) \
  TUNE_DOUBLE(cm_grow_safety_margin) \
  TUNE_DOUBLE(sortie_defence_weight) \
  TUNE_INT(cm_grow_food) \
  TUNE_INT(cm_grow_shield) \
  TUNE_INT(cm_grow_trade) \
  TUNE_DOUBLE(cm_founder_food) \
  TUNE_DOUBLE(cm_trade_quality_slope) \
  TUNE_DOUBLE(cm_grow_price_blend) \
  TUNE_DOUBLE(cm_specialist_gold_gain) \
  TUNE_DOUBLE(cm_specialist_sci_gain) \
  TUNE_DOUBLE(diplomat_defence_value) \
  TUNE_DOUBLE(site_stream_weight) \
  TUNE_INT(land_pressure_tiles) \
  TUNE_DOUBLE(site_potential_discount) \
  TUNE_DOUBLE(site_special_bonus) \
  TUNE_DOUBLE(site_special_cluster) \
  TUNE_DOUBLE(site_claim_bonus) \
  TUNE_DOUBLE(site_pressure_factor) \
  TUNE_DOUBLE(horizon_capacity_weight) \
  TUNE_DOUBLE(horizon_growth_weight) \
  TUNE_DOUBLE(horizon_bundle_weight) \
  TUNE_DOUBLE(horizon_health_weight) \
  TUNE_DOUBLE(horizon_content_weight) \
  TUNE_DOUBLE(horizon_multiplier_weight) \
  TUNE_DOUBLE(site_travel_discount) \
  TUNE_DOUBLE(site_travel_crowding) \
  TUNE_DOUBLE(site_potential_weight) \
  TUNE_DOUBLE(site_settler_cost_weight) \
  TUNE_DOUBLE(site_boat_cost_weight) \
  TUNE_DOUBLE(site_rival_pressure) \
  TUNE_DOUBLE(site_rival_crowding) \
  TUNE_DOUBLE(expansion_war_survival) \
  TUNE_INT(expansion_gate_enabled) \
  TUNE_DOUBLE(caravan_discount) \
  TUNE_INT(caravan_foreign_trade) \
  TUNE_DOUBLE(cm_war_survival) \
  TUNE_DOUBLE(cm_war_growth_charge) \
  TUNE_DOUBLE(horizon_tech_weight) \
  TUNE_DOUBLE(overseas_fear) \
  TUNE_DOUBLE(overseas_decay) \
  TUNE_INT(garrison_floor) \
  TUNE_INT(garrison_floor_danger) \
  TUNE_DOUBLE(garrison_menace_slope) \
  TUNE_DOUBLE(menace_overseas_weight) \
  TUNE_DOUBLE(floor_barb_weight) \
  TUNE_DOUBLE(floor_sea_weight) \
  TUNE_DOUBLE(floor_air_weight) \
  TUNE_DOUBLE(floor_border_weight) \
  TUNE_DOUBLE(floor_interior_weight) \
  TUNE_DOUBLE(floor_marines_mult) \
  TUNE_INT(barbarian_fear) \
  TUNE_INT(barb_anticipation_turns) \
  TUNE_INT(barb_size_ref_cities) \
  TUNE_INT(boat_shadow_fear) \
  TUNE_INT(density_fear_value) \
  TUNE_INT(density_fear_radius) \
  TUNE_INT(density_fear_ramp_turns) \
  TUNE_INT(density_prior_value) \
  TUNE_INT(density_prior_tiles) \
  TUNE_DOUBLE(wall_fear_weight) \
  TUNE_DOUBLE(birth_guard_gain) \
  TUNE_DOUBLE(defensive_techwall_weight) \
  TUNE_DOUBLE(attacker_hold_damper) \
  TUNE_DOUBLE(diplomat_pump_damper) \
  TUNE_INT(wrace_margin_turns) \
  TUNE_DOUBLE(wrace_pwin_gain) \
  TUNE_DOUBLE(wrace_eta_gain) \
  TUNE_DOUBLE(wrace_obsolete_gain) \
  TUNE_DOUBLE(wrace_urgency_gain) \
  TUNE_DOUBLE(expiry_building_gain) \
  TUNE_DOUBLE(expiry_unit_gain) \
  TUNE_INT(expiry_unit_gap) \
  TUNE_DOUBLE(sell_defend_gain) \
  TUNE_INT(late_buy_gate) \
  TUNE_INT(evacuate_gate) \
  TUNE_DOUBLE(settler_rush_weight) \
  TUNE_DOUBLE(site_choke_bonus) \
  TUNE_DOUBLE(base_fort_gain) \
  TUNE_INT(fortify_ground) \
  TUNE_DOUBLE(pillage_denial_gain) \
  TUNE_INT(convoy_min_load) \
  TUNE_INT(convoy_wait_radius) \
  TUNE_INT(lift_form_radius) \
  TUNE_INT(lift_gather_turns) \
  TUNE_INT(lift_cross_turns) \
  TUNE_INT(lift_op_slots) \
  TUNE_INT(lift_manifest_max) \
  TUNE_DOUBLE(lift_hitch_wait_ratio) \
  TUNE_DOUBLE(boat_idle_damping) \
  TUNE_INT(boat_water_gate) \
  TUNE_INT(boat_gate_at_bid) \
  TUNE_INT(lift_drop_radius) \
  TUNE_INT(lift_hitch_wait_min) \
  TUNE_INT(lift_muster_release) \
  TUNE_INT(founder_berth_route) \
  TUNE_INT(lift_wait_tail_pct) \
  TUNE_INT(worker_flee_radius) \
  TUNE_INT(worker_guard_radius) \
  TUNE_DOUBLE(worker_danger_discount) \
  TUNE_DOUBLE(worker_cross_continent_discount) \
  TUNE_DOUBLE(worker_travel_discount) \
  TUNE_DOUBLE(trunk_road_gain) \
  TUNE_DOUBLE(trunk_source_gain) \
  TUNE_DOUBLE(upkeep_gold_weight) \
  TUNE_DOUBLE(upkeep_unit_weight) \
  TUNE_DOUBLE(rotation_power_ratio) \
  TUNE_INT(rotation_want) \
  TUNE_DOUBLE(rotation_finish_relax) \
  TUNE_DOUBLE(survival_target) \
  TUNE_DOUBLE(survival_fear_value) \
  TUNE_INT(survival_horizon) \
  TUNE_INT(survival_pf_reach) \
  TUNE_DOUBLE(threat_pin_discount) \
  TUNE_DOUBLE(tired_attack_floor) \
  TUNE_DOUBLE(attack_value_weight) \
  TUNE_DOUBLE(attack_loss_gain) \
  TUNE_DOUBLE(conquest_worth_mult) \
  TUNE_DOUBLE(conquest_culture_gain) \
  TUNE_DOUBLE(defence_worth_mult) \
  TUNE_INT(assault_pool_reach) \
  TUNE_DOUBLE(sortie_win_floor) \
  TUNE_DOUBLE(sortie_prize_gain) \
  TUNE_DOUBLE(deterrence_gain) \
  TUNE_DOUBLE(attacker_garrison_gain) \
  TUNE_INT(cover_removal) \
  TUNE_DOUBLE(unit_upkeep_charge) \
  TUNE_DOUBLE(pop_insurance_gain) \
  TUNE_DOUBLE(heal_stream_gain) \
  TUNE_DOUBLE(veteran_stream_gain) \
  TUNE_DOUBLE(op_take_target) \
  TUNE_INT(op_muster_wait) \
  TUNE_INT(op_draft_marginal) \
  TUNE_INT(op_muster) \
  TUNE_INT(op_muster_stall) \
  TUNE_INT(op_muster_home) \
  TUNE_INT(op_launch_odds) \
  TUNE_INT(op_net_value) \
  TUNE_DOUBLE(op_hold_survivors) \
  TUNE_DOUBLE(satisfied_fear_discount) \
  TUNE_DOUBLE(colony_lift_weight) \
  TUNE_DOUBLE(hull_commission_weight) \
  TUNE_DOUBLE(invasion_reinforce_rate) \
  TUNE_DOUBLE(invasion_reinforce_slope) \
  TUNE_INT(invasion_embark_turns) \
  TUNE_INT(op_wave_window) \
  TUNE_INT(raid_radius) \
  TUNE_INT(hut_risk_radius) \
  TUNE_DOUBLE(hut_risk_discount) \
  TUNE_INT(hut_risk_honest) \
  TUNE_DOUBLE(op_abort_floor) \
  TUNE_DOUBLE(barb_reinforce_rate) \
  TUNE_INT(stance_forced) \
  TUNE_INT(stance_source) \
  TUNE_DOUBLE(stance_trait_span) \
  TUNE_DOUBLE(stance_fear_gain) \
  TUNE_DOUBLE(stance_op_gain) \
  TUNE_DOUBLE(stance_growth_gain) \
  TUNE_DOUBLE(stance_expand_gain) \
  TUNE_DOUBLE(stance_assault_gain) \
  TUNE_DOUBLE(stance_floor_gain) \
  TUNE_DOUBLE(stance_position_gain) \
  TUNE_DOUBLE(stance_techlean_gain) \
  TUNE_DOUBLE(stance_pressure_ref) \
  TUNE_INT(spy_enabled) \
  TUNE_DOUBLE(spy_gold_value) \
  TUNE_DOUBLE(spy_sabotage_gain) \
  TUNE_INT(rates_enabled) \
  TUNE_DOUBLE(rates_science_gain) \
  TUNE_DOUBLE(rates_luxury_gain) \
  TUNE_INT(gov_valuation) \
  TUNE_DOUBLE(gov_anarchy_weight) \
  TUNE_DOUBLE(gov_rate_ceiling_gain) \
  TUNE_INT(trade_merit) \
  TUNE_DOUBLE(trade_merit_gain) \
  TUNE_DOUBLE(trade_ceasefire_peace_odds) \
  TUNE_DOUBLE(trade_route_risk_gain) \
  TUNE_DOUBLE(trade_route_love) \
  TUNE_DOUBLE(trade_partner_target_discount) \
  TUNE_INT(wonder_merit) \
  TUNE_DOUBLE(wonder_merit_gain) \
  TUNE_INT(pay_switch_penalty) \
  TUNE_INT(switch_cost_price) \
  TUNE_DOUBLE(switch_cost_gain) \
  TUNE_DOUBLE(scale_settler) \
  TUNE_DOUBLE(scale_worker) \
  TUNE_DOUBLE(scale_defender) \
  TUNE_DOUBLE(scale_attacker) \
  TUNE_DOUBLE(scale_ferry) \
  TUNE_DOUBLE(emergency_value_cap) \
  TUNE_DOUBLE(peace_seek_gain) \
  TUNE_INT(diplo_deep_only_pacts) \
  TUNE_DOUBLE(gift_border_value) \
  TUNE_DOUBLE(gift_love_gain) \
  TUNE_DOUBLE(gift_ally_gain) \
  TUNE_DOUBLE(gift_instrument_gain) \
  TUNE_DOUBLE(gift_rival_gain) \
  TUNE_DOUBLE(proxy_arm_gain) \
  TUNE_DOUBLE(embassy_price_gain) \
  TUNE_DOUBLE(embassy_give_gain) \
  TUNE_DOUBLE(leader_check_gain) \
  TUNE_INT(endgame_horizon) \
  TUNE_INT(horizon_decay_gate) \
  TUNE_DOUBLE(horizon_decay_early) \
  TUNE_DOUBLE(horizon_decay_late) \
  TUNE_INT(horizon_decay_break_start) \
  TUNE_INT(horizon_decay_break_end) \
  TUNE_INT(endgame_window) \
  TUNE_DOUBLE(endgame_unfinished_gain) \
  TUNE_DOUBLE(endgame_wonder_gain) \
  TUNE_DOUBLE(horizon_govcenter_weight) \
  TUNE_DOUBLE(horizon_upkeepfree_weight) \
  TUNE_DOUBLE(horizon_culture_weight) \
  TUNE_INT(buy_per_gold) \
  TUNE_INT(honest_info) \
  TUNE_INT(techgoal_deep) \
  TUNE_DOUBLE(techgoal_distance_power)

static void deep_tuning_load_file(struct deep_tuning *t, const char *path);

/**********************************************************************//**
  Load the BASE tuning from $FREECIV_DEEP_TUNING, if set, and reset the
  per-seat layer. Called once from module setup; unknown keys in the
  file are ignored by lookup, absent keys keep their compiled defaults.
  The arena layer that can give individual seats their own files is
  documented at the top of deeptuning.h and is inert unless
  $FREECIV_DEEP_ARENA names a manifest.
**************************************************************************/
void deep_tuning_init(void)
{
  const char *path = getenv("FREECIV_DEEP_TUNING");
  int i;

  deep_tuning_active = deep_tuning_defaults;
  deep_tuning_loaded = TRUE;

  /* A reload (the L0 test reloads; a savegame load re-enters module
   * setup) starts the per-seat layer over: pointers into the candidate
   * cache must not outlive the cache. */
  for (i = 0; i < MAX_NUM_PLAYER_SLOTS; i++) {
    deep_tuning_seat[i] = NULL;
    deep_tuning_seat_resolved[i] = FALSE;
  }
  deep_tuning_scope = NULL;
  deep_tuning_scopeless = 0;
  deep_arena_files = 0;
  if (deep_arena_manifest != NULL) {
    secfile_destroy(deep_arena_manifest);
    deep_arena_manifest = NULL;
  }
  deep_arena_manifest_tried = FALSE;

  if (path == NULL || path[0] == '\0') {
    return;
  }

  deep_tuning_load_file(&deep_tuning_active, path);
}

/**********************************************************************//**
  Overlay one tuning file's [tuning] keys onto values that already hold
  the compiled defaults, and log every resulting value.

  Absent keys keep their compiled defaults EXACTLY, which is why the
  double path asks whether the entry exists before reading it.
  secfile_lookup_float_default() takes its default as a `float`
  (utility/registry_ini.h), so passing a double default through it
  quantises the value even when the file never mentions the key:
  0.9 came back as 0.8999999761581421. That made loading any tuning
  file a second, uncontrolled treatment -- a file with zero keys
  diverged from tuning=none in 5 of 10 smoke seeds (session H1;
  W34 E34.3 FINDING 2 measured 12/20 on smoke-traits). Keys the file
  DOES set still go through the float lookup, so every archived ini
  under benchmark/tuning keeps the value it has always had.
**************************************************************************/
static void deep_tuning_load_file(struct deep_tuning *t, const char *path)
{
  struct section_file *sf;
  char buf[8192];

  sf = secfile_load(path, FALSE);
  if (sf == NULL) {
    log_error("deep: cannot load tuning file \"%s\"; using defaults",
              path);
    return;
  }

#define TUNE_DOUBLE(field) \
  if (secfile_entry_lookup(sf, "tuning." #field) != NULL) { \
    t->field \
      = secfile_lookup_float_default(sf, deep_tuning_defaults.field, \
                                     "tuning." #field); \
  }
#define TUNE_INT(field) \
  t->field \
    = secfile_lookup_int_default(sf, deep_tuning_defaults.field, \
                                 "tuning." #field);
  DEEP_TUNING_FIELDS
#undef TUNE_DOUBLE
#undef TUNE_INT

  secfile_destroy(sf);

  buf[0] = '\0';
#define TUNE_DOUBLE(field) \
  cat_snprintf(buf, sizeof(buf), " %s=%g", #field, \
               t->field);
#define TUNE_INT(field) \
  cat_snprintf(buf, sizeof(buf), " %s=%d", #field, \
               t->field);
  DEEP_TUNING_FIELDS
#undef TUNE_DOUBLE
#undef TUNE_INT

  log_normal("deep-tuning file=\"%s\"%s", path, buf);
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
  The tuning file this seat is to run, from the arena manifest, or NULL
  for the base values. Name first, slot second; see deeptuning.h.
**************************************************************************/
static const char *deep_arena_seat_file(const struct player *pplayer)
{
  const char *env = getenv("FREECIV_DEEP_ARENA");
  const char *path;

  if (env == NULL || env[0] == '\0') {
    return NULL;
  }
  if (!deep_arena_manifest_tried) {
    deep_arena_manifest_tried = TRUE;
    deep_arena_manifest = secfile_load(env, FALSE);
    if (deep_arena_manifest == NULL) {
      log_error("deep: cannot load arena manifest \"%s\";"
                " every seat keeps the base tuning", env);
    }
  }
  if (deep_arena_manifest == NULL) {
    return NULL;
  }

  path = secfile_lookup_str_default(deep_arena_manifest, NULL, "arena.%s",
                                    player_name(pplayer));
  if (path == NULL) {
    path = secfile_lookup_str_default(deep_arena_manifest, NULL,
                                      "arena.slot%d",
                                      player_number(pplayer));
  }

  return path;
}

/**********************************************************************//**
  The values behind one candidate file, loaded once and shared by every
  seat that names it.
**************************************************************************/
static const struct deep_tuning *deep_arena_values(const char *path)
{
  int i;

  for (i = 0; i < deep_arena_files; i++) {
    if (strcmp(deep_arena_file[i].path, path) == 0) {
      return &deep_arena_file[i].val;
    }
  }
  if (deep_arena_files >= DEEP_ARENA_FILES_MAX) {
    log_error("deep: more than %d arena tuning files; \"%s\" runs the"
              " base values", DEEP_ARENA_FILES_MAX, path);
    return &deep_tuning_active;
  }

  i = deep_arena_files++;
  fc_snprintf(deep_arena_file[i].path, sizeof(deep_arena_file[i].path),
              "%s", path);
  deep_arena_file[i].val = deep_tuning_defaults;
  deep_tuning_load_file(&deep_arena_file[i].val, path);

  return &deep_arena_file[i].val;
}

/**********************************************************************//**
  The tuning values of one seat. Resolves the seat's arena assignment on
  first call (the alloc hook is too early to key on a name) and caches
  the pointer, so every later read is an array index.
**************************************************************************/
const struct deep_tuning *deep_tuning_for(const struct player *pplayer)
{
  int idx;

  if (!deep_tuning_loaded) {
    deep_tuning_init();
  }
  if (pplayer == NULL) {
    return &deep_tuning_active;
  }

  idx = player_index(pplayer);
  fc_assert_ret_val(idx >= 0 && idx < MAX_NUM_PLAYER_SLOTS,
                    &deep_tuning_active);

  if (!deep_tuning_seat_resolved[idx]) {
    const char *path = deep_arena_seat_file(pplayer);

    deep_tuning_seat_resolved[idx] = TRUE;
    if (path == NULL) {
      deep_tuning_seat[idx] = &deep_tuning_active;
    } else {
      deep_tuning_seat[idx] = deep_arena_values(path);
      log_normal("deep-arena seat=%s slot=%d file=\"%s\"",
                 player_name(pplayer), player_number(pplayer), path);
    }
  }

  return deep_tuning_seat[idx];
}

/**********************************************************************//**
  Forget a seat's assignment. The slot can be reused by a later player
  (civil war, reload), and that player is entitled to its own lookup.
**************************************************************************/
void deep_tuning_player_free(const struct player *pplayer)
{
  int idx;

  if (pplayer == NULL) {
    return;
  }
  idx = player_index(pplayer);
  if (idx >= 0 && idx < MAX_NUM_PLAYER_SLOTS) {
    deep_tuning_seat[idx] = NULL;
    deep_tuning_seat_resolved[idx] = FALSE;
  }
}

/**********************************************************************//**
  Called from the player_alloc hook. The assignment itself resolves
  lazily (deep_tuning_for), so this only guarantees a clean slot.
**************************************************************************/
void deep_tuning_player_init(const struct player *pplayer)
{
  if (!deep_tuning_loaded) {
    deep_tuning_init();
  }
  deep_tuning_player_free(pplayer);
}

/**********************************************************************//**
  Enter the scope of one seat, returning the scope to restore. Save and
  restore rather than set and clear: hooks nest (a city hook inside a
  phase hook), and a nested scope must give the outer one back.
**************************************************************************/
const struct deep_tuning *deep_tuning_scope_set(const struct player *pplayer)
{
  const struct deep_tuning *saved = deep_tuning_scope;

  deep_tuning_scope = deep_tuning_for(pplayer);

  return saved;
}

/**********************************************************************//**
  Leave a scope entered by deep_tuning_scope_set().
**************************************************************************/
void deep_tuning_scope_restore(const struct deep_tuning *saved)
{
  deep_tuning_scope = saved;
}

/**********************************************************************//**
  Whether any seat runs values of its own. False for every run this
  project has ever measured, and for every run that does not set
  $FREECIV_DEEP_ARENA.
**************************************************************************/
bool deep_tuning_arena_active(void)
{
  return deep_arena_files > 0;
}

/**********************************************************************//**
  Reads that found no seat scope while an arena was configured -- the
  coverage instrument for the hook guards. Zero is the only good value;
  anything else names a path into deep that no guard covers, and it is
  reported rather than smoothed over.
**************************************************************************/
int deep_tuning_scopeless_reads(void)
{
  return deep_tuning_scopeless;
}

/**********************************************************************//**
  The tuning values of the seat whose decision is being made.
**************************************************************************/
const struct deep_tuning *deep_tuning_get(void)
{
  if (!deep_tuning_loaded) {
    deep_tuning_init();
  }
  if (deep_tuning_scope != NULL) {
    return deep_tuning_scope;
  }
  if (deep_arena_files > 0) {
    deep_tuning_scopeless++;
  }

  return &deep_tuning_active;
}
