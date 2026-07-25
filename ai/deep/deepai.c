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

/* The deep AI module skeleton: forwards every hook to the default AI
 * (like ai/classic, from which this file is derived), layering its own
 * per-player state, savegame namespace, and decision-vector logging on
 * top. Strategy replaces individual forwards incrementally. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "log.h"
#include "mem.h"
#include "registry.h"

/* common */
#include "ai.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "research.h"

/* server */
#include "unittools.h"

/* server/advisors */
#include "advdata.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daidiplomacy.h"
#include "daidomestic.h"
#include "daiferry.h"
#include "daihand.h"
#include "dailog.h"
#include "daimilitary.h"
#include "daiplayer.h"
#include "daisanity.h"
#include "daisettler.h"
#include "daitools.h"
#include "daiunit.h"

#include "deepdefence.h"
#include "deepsettler.h"
#include "deeptuning.h"

#include "deepai.h"

/* This function is only needed as symbol in module. Nevertheless
   gcc requires that there is prior prototype. */
const char *fc_ai_deep_capstr(void);

static struct ai_type *self = NULL;

/**********************************************************************//**
  Set pointer to ai type of the deep ai.
**************************************************************************/
static void deep_ai_set_self(struct ai_type *ai)
{
  self = ai;
}

/**********************************************************************//**
  Get pointer to ai type of the deep ai.
**************************************************************************/
struct ai_type *deep_ai_get_self(void)
{
  return self;
}

/**********************************************************************//**
  Return module capability string
**************************************************************************/
const char *fc_ai_deep_capstr(void)
{
  return FC_AI_MOD_CAPSTR;
}

/**********************************************************************//**
  Free resources allocated by the deep AI module
**************************************************************************/
static void dpai_module_close(void)
{
  struct ai_type *deftype = deep_ai_get_self();

  FC_FREE(deftype->private);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_player_alloc(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  struct deep_plr *plr_data = fc_calloc(1, sizeof(struct deep_plr));

  /* struct deep_plr embeds struct ai_plr as its first member, so the
   * forwarded default AI finds its data in the same slot. */
  player_set_ai_data(pplayer, deftype, plr_data);
  dai_data_init(deftype, pplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_player_free(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_player_free(deftype, pplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
  Deepai stores information to "deep" like the default ai common code.
**************************************************************************/
static void dpai_player_save_relations(struct player *pplayer,
                                      struct player *other,
                                      struct section_file *file,
                                      int plrno)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_player_save_relations(deftype, "deep", pplayer, other, file, plrno);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_player_load_relations(struct player *pplayer,
                                      struct player *other,
                                      const struct section_file *file,
                                      int plrno)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_player_load_relations(deftype, "deep", pplayer, other, file, plrno);
}

/**********************************************************************//**
  Save deep-only per-player state under the "deep_state" namespace.
  (Relations flow through dpai_player_save_relations; this hook holds
  what no dai function knows about.)
**************************************************************************/
static void dpai_player_save(struct player *pplayer,
                             struct section_file *file, int plrno)
{
  struct deep_plr *plr_data = deep_plr_data(pplayer);

  secfile_insert_int(file, plr_data->dv_turns_logged,
                     "player%d.deep_state.dv_turns_logged", plrno);
}

/**********************************************************************//**
  Load deep-only per-player state; every key is defaulted so saves
  written without this module load cleanly.
**************************************************************************/
static void dpai_player_load(struct player *pplayer,
                             const struct section_file *file, int plrno)
{
  struct deep_plr *plr_data = deep_plr_data(pplayer);

  plr_data->dv_turns_logged
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.dv_turns_logged",
                                 plrno);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_gained_control(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_gained_control(deftype, pplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_split_by_civil_war(struct player *original,
                                   struct player *created)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_assess_danger_player(deftype, &(wld.map), original);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_created_by_civil_war(struct player *original,
                                     struct player *created)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_player_copy(deftype, original, created);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_data_phase_begin(struct player *pplayer, bool is_new_phase)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_data_phase_begin(deftype, pplayer, is_new_phase);

  if (is_new_phase) {
    struct deep_plr *plr_data = deep_plr_data(pplayer);

    plr_data->dv_turns_logged++;
    log_base(LOG_VERBOSE,
             "deep-dv turn=%d player=\"%s\" cities=%d units=%d gold=%d"
             " techs=%d science=%d",
             game.info.turn, player_name(pplayer),
             city_list_size(pplayer->cities),
             unit_list_size(pplayer->units),
             pplayer->economic.gold,
             research_get(pplayer)->techs_researched,
             pplayer->economic.science);
  }
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_data_phase_finished(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_data_phase_finished(deftype, pplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_alloc(struct city *pcity)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_city_alloc(deftype, pcity);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_free(struct city *pcity)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_city_free(deftype, pcity);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_save(struct section_file *file, const struct city *pcity,
                          const char *citystr)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_city_save(deftype, "deep", file, pcity, citystr);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_load(const struct section_file *file, struct city *pcity,
                          const char *citystr)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_city_load(deftype, "deep", file, pcity, citystr);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_build_adv_override(struct city *pcity, struct adv_choice *choice)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_build_adv_override(deftype, pcity, choice);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_wonder_city_distance(struct player *pplayer, struct adv_data *adv)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_wonder_city_distance(deftype, pplayer, adv);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_build_adv_init(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_build_adv_init(deftype, pplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_build_adv_adjust(struct player *pplayer, struct city *wonder_city)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_build_adv_adjust(deftype, pplayer, wonder_city);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_gov_value(struct player *pplayer, struct government *gov,
                          adv_want *val, bool *override)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_gov_value(deftype, pplayer, gov, val, override);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_units_ruleset_init(void)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_units_ruleset_init(deftype);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_units_ruleset_close(void)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_units_ruleset_close(deftype);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_init(struct unit *punit)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_unit_init(deftype, punit);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_close(struct unit *punit)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_unit_close(deftype, punit);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_ferry_init_ferry(struct unit *ferry)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_ferry_init_ferry(deftype, ferry);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_ferry_transformed(struct unit *ferry, const struct unit_type *old)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_ferry_transformed(deftype, ferry, old);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_ferry_lost(struct unit *punit)
{
  struct ai_type *deftype = deep_ai_get_self();

  /* Loss-rate bookkeeping (benchmark/losses.py). The hook also fires
   * when a unit merely changes owner (capture, bribe); the server then
   * leaves loss_reason at -1. */
  log_base(LOG_NORMAL,
           "deep-loss turn=%d player=\"%s\" unit=\"%s\" founder=%d"
           " reason=\"%s\" at=(%d,%d)",
           game.info.turn, player_name(unit_owner(punit)),
           unit_rule_name(punit),
           unit_is_cityfounder(punit) ? 1 : 0,
           punit->server.loss_reason >= 0
             ? unit_loss_reason_name(punit->server.loss_reason)
             : "transfer",
           TILE_XY(unit_tile(punit)));

  dai_ferry_lost(deftype, punit);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_turn_end(struct unit *punit)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_unit_turn_end(deftype, punit);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_move_or_attack(struct unit *punit, struct tile *ptile,
                                    struct pf_path *path, int step)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_unit_move_or_attack(deftype, punit, ptile, path, step);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_new_adv_task(struct unit *punit, enum adv_unit_task task,
                                  struct tile *ptile)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_unit_new_adv_task(deftype, punit, task, ptile);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_save(struct section_file *file, const struct unit *punit,
                          const char *unitstr)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_unit_save(deftype, "deep_", file, punit, unitstr);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_load(const struct section_file *file, struct unit *punit,
                          const char *unitstr)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_unit_load(deftype, "deep_", file, punit, unitstr);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_auto_settler_reset(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_auto_settler_reset(deftype, pplayer);
}

/**********************************************************************//**
  Deep settler engine: risk-aware site search and routing (W05).
**************************************************************************/
static void dpai_auto_settler_run(struct player *pplayer,
                                 struct unit *punit,
                                 struct workermap *state)
{
  struct ai_type *deftype = deep_ai_get_self();

  deep_auto_settler_run(deftype, &(wld.map), pplayer, punit, state);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_auto_settler_cont(struct player *pplayer,
                                  struct unit *punit,
                                  struct workermap *state)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_auto_settler_cont(deftype, &(wld.map), pplayer, punit, state);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_switch_to_explore(struct unit *punit, struct tile *target,
                                  enum override_bool *allow)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_switch_to_explore(deftype, punit, target, allow);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_do_first_activities(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  deep_do_first_activities(deftype, pplayer);

  pplayer->ai_phase_done = TRUE;
}

/**********************************************************************//**
  Mark turn done as we have already done everything before game was saved.
**************************************************************************/
static void dpai_restart_phase(struct player *pplayer)
{
  pplayer->ai_phase_done = TRUE;
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_diplomacy_actions(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_diplomacy_actions(deftype, pplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_do_last_activities(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  deep_do_last_activities(deftype, pplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_treaty_evaluate(struct player *pplayer, struct player *aplayer,
                                struct treaty *ptreaty)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_treaty_evaluate(deftype, pplayer, aplayer, ptreaty);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_treaty_accepted(struct player *pplayer, struct player *aplayer,
                                struct treaty *ptreaty)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_treaty_accepted(deftype, pplayer, aplayer, ptreaty);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_diplomacy_first_contact(struct player *pplayer,
                                        struct player *aplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_diplomacy_first_contact(deftype, pplayer, aplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_incident(enum incident_type type,
                         enum casus_belli_range scope,
                         const struct action *paction,
                         struct player *receiver,
                         struct player *violator, struct player *victim)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_incident(deftype, type, scope, paction, receiver, violator, victim);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_log(char *buffer, int buflength, const struct city *pcity)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_city_log(deftype, buffer, buflength, pcity);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_log(char *buffer, int buflength, const struct unit *punit)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_unit_log(deftype, buffer, buflength, punit);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_consider_plr_dangerous(struct player *plr1, struct player *plr2,
                                       enum override_bool *result)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_consider_plr_dangerous(deftype, plr1, plr2, result);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_consider_tile_dangerous(struct tile *ptile, struct unit *punit,
                                        enum override_bool *result)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_consider_tile_dangerous(deftype, ptile, punit, result);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_consider_wonder_city(struct city *pcity, bool *result)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_consider_wonder_city(deftype, pcity, result);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_revolution_start(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_revolution_start(deftype, pplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_sanity_check(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_sanity_check(deftype, pplayer);
}

/**********************************************************************//**
  Setup player ai_funcs function pointers.
**************************************************************************/
bool fc_ai_deep_setup(struct ai_type *ai)
{
  struct dai_private_data *private;

  deep_ai_set_self(ai);
  deep_tuning_init();

  strncpy(ai->name, "deep", sizeof(ai->name));

  private = fc_malloc(sizeof(struct dai_private_data));
  private->contemplace_workers = TRUE;
  ai->private = private;

  ai->funcs.module_close = dpai_module_close;

  /* ai->funcs.map_alloc = NULL; */
  /* ai->funcs.map_ready = NULL; */
  /* ai->funcs.map_free = NULL; */
  /* ai->funcs.game_start = NULL; */
  /* ai->funcs.game_free = NULL; */

  ai->funcs.player_alloc = dpai_player_alloc;
  ai->funcs.player_free = dpai_player_free;
  ai->funcs.player_save = dpai_player_save;
  ai->funcs.player_load = dpai_player_load;
  ai->funcs.player_save_relations = dpai_player_save_relations;
  ai->funcs.player_load_relations = dpai_player_load_relations;
  ai->funcs.gained_control = dpai_gained_control;
  /* ai->funcs.lost_control = NULL; */
  ai->funcs.split_by_civil_war = dpai_split_by_civil_war;
  ai->funcs.created_by_civil_war = dpai_created_by_civil_war;

  ai->funcs.phase_begin = dpai_data_phase_begin;
  ai->funcs.phase_finished = dpai_data_phase_finished;

  ai->funcs.city_alloc = dpai_city_alloc;
  ai->funcs.city_free = dpai_city_free;
  /*
    ai->funcs.city_created = NULL;
    ai->funcs.city_destroyed = NULL;
    ai->funcs.city_got = NULL;
    ai->funcs.city_lost = NULL;
  */
  ai->funcs.city_save = dpai_city_save;
  ai->funcs.city_load = dpai_city_load;
  ai->funcs.choose_building = dpai_build_adv_override;
  ai->funcs.build_adv_prepare = dpai_wonder_city_distance;
  ai->funcs.build_adv_init = dpai_build_adv_init;
  ai->funcs.build_adv_adjust_want = dpai_build_adv_adjust;

  ai->funcs.gov_value = dpai_gov_value;

  ai->funcs.units_ruleset_init = dpai_units_ruleset_init;
  ai->funcs.units_ruleset_close = dpai_units_ruleset_close;

  /* FIXME: We should allocate memory only for units owned by
     default ai in unit_got. We track no data
     about enemy units.
     But advisors code still depends on some default ai data (role) to be
     always allocated. */
  /*
    ai->funcs.unit_alloc = NULL;
    ai->funcs.unit_free = NULL;
    ai->funcs.unit_created = NULL;
    ai->funcs.unit_destroyed = NULL;
    ai->funcs.unit_got = dai_unit_init;
    ai->funcs.unit_lost = dai_unit_close;
  */
  ai->funcs.unit_alloc = dpai_unit_init;
  ai->funcs.unit_free = dpai_unit_close;
  ai->funcs.unit_got = dpai_ferry_init_ferry;
  ai->funcs.unit_lost = dpai_ferry_lost;
  ai->funcs.unit_transformed = dpai_ferry_transformed;

  ai->funcs.unit_turn_end = dpai_unit_turn_end;
  ai->funcs.unit_move = dpai_unit_move_or_attack;
  /* ai->funcs.unit_move_seen = NULL; */
  ai->funcs.unit_task = dpai_unit_new_adv_task;

  ai->funcs.unit_save = dpai_unit_save;
  ai->funcs.unit_load = dpai_unit_load;

  ai->funcs.settler_reset = dpai_auto_settler_reset;
  ai->funcs.settler_run = dpai_auto_settler_run;
  ai->funcs.settler_cont = dpai_auto_settler_cont;

  ai->funcs.want_to_explore = dpai_switch_to_explore;

  ai->funcs.first_activities = dpai_do_first_activities;
  ai->funcs.restart_phase = dpai_restart_phase;
  ai->funcs.diplomacy_actions = dpai_diplomacy_actions;
  ai->funcs.last_activities = dpai_do_last_activities;
  ai->funcs.treaty_evaluate = dpai_treaty_evaluate;
  ai->funcs.treaty_accepted = dpai_treaty_accepted;
  ai->funcs.first_contact = dpai_diplomacy_first_contact;
  ai->funcs.incident = dpai_incident;

  ai->funcs.log_fragment_city = dpai_city_log;
  ai->funcs.log_fragment_unit = dpai_unit_log;

  ai->funcs.consider_plr_dangerous = dpai_consider_plr_dangerous;
  ai->funcs.consider_tile_dangerous = dpai_consider_tile_dangerous;
  ai->funcs.consider_wonder_city = dpai_consider_wonder_city;

  /* ai->funcs.refresh = NULL; */

  /* ai->funcs.tile_info = NULL; */
  /* ai->funcs.city_info = NULL; */
  /* ai->funcs.unit_info = NULL; */

  ai->funcs.revolution_start = dpai_revolution_start;
  ai->funcs.check_sanity = dpai_sanity_check;

  return TRUE;
}
