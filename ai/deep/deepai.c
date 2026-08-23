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

/* ai */
#include "handicaps.h"

/* server */
#include "srv_main.h"
#include "unittools.h"

/* server/advisors */
#include "advdata.h"

/* ai/default */
#include "daicity.h"
#include "notify.h"

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

#include "deepcombat.h"
#include "deepdefence.h"
#include "deepdiplomacy.h"
#include "deepfunnel.h"
#include "deepgov.h"
#include "deepsettler.h"
#include "deepsite.h"
#include "deepspy.h"
#include "deepchain.h"
#include "deepchat.h"
#include "deepstance.h"
#include "deeptechcensus.h"
#include "deeptrade.h"
#include "deeptuning.h"

#include "autoworkers.h"

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

  deep_site_free();
  FC_FREE(deftype->private);
}

/**********************************************************************//**
  Free per-game caches held by the deep AI. Called from
  server_game_free(), i.e. at game end and before a reload, so nothing
  a site evaluation cached about last game's cities survives into the
  next one.
**************************************************************************/
static void dpai_game_free(void)
{
  deep_site_free();
  /* Unit ids are recycled between games in one server process, so a
   * tag that outlived its game would be read against a stranger. */
  deep_pin_reset();
  deep_pindisc_reset();
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
  /* Identity until the first phase resolves it: a zeroed stance is a
   * ratio nothing can be read from. */
  plr_data->stance = deep_stance_identity();
  deep_tuning_player_init(pplayer);
  dai_data_init(deftype, pplayer);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_player_free(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();

  deep_tuning_player_free(pplayer);
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
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_player_save_relations(deftype, "deep", pplayer, other, file, plrno);
  deep_tuning_scope_restore(deep_scope);
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
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_player_load_relations(deftype, "deep", pplayer, other, file, plrno);
  deep_tuning_scope_restore(deep_scope);
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
  int oi, i;
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  secfile_insert_int(file, plr_data->dv_turns_logged,
                     "player%d.deep_state.dv_turns_logged", plrno);

  /* The stance (W34 phase A). A save carries what this player's axes
   * WERE; the next phase re-resolves them from the tuning file in
   * force at load time, which is the right behaviour for a control
   * surface -- a pin belongs to the run, not to the position. Stored
   * as floats: every value the layer can hold today (1.0, 1 +/- span)
   * is exact in binary32. */
  secfile_insert_float(file, plr_data->stance.econ,
                       "player%d.deep_state.stance_econ", plrno);
  secfile_insert_float(file, plr_data->stance.mil,
                       "player%d.deep_state.stance_mil", plrno);
  secfile_insert_float(file, plr_data->stance.cult,
                       "player%d.deep_state.stance_cult", plrno);
  secfile_insert_int(file, plr_data->stance_fear_gated,
                     "player%d.deep_state.stance_fear_gated", plrno);
  secfile_insert_int(file, plr_data->stance_op_gated,
                     "player%d.deep_state.stance_op_gated", plrno);
  secfile_insert_int(file, plr_data->stance_growth_gated,
                     "player%d.deep_state.stance_growth_gated", plrno);
  secfile_insert_int(file, plr_data->stance_expand_gated,
                     "player%d.deep_state.stance_expand_gated", plrno);
  secfile_insert_int(file, plr_data->stance_assault_gated,
                     "player%d.deep_state.stance_assault_gated", plrno);
  secfile_insert_int(file, plr_data->stance_floor_gated,
                     "player%d.deep_state.stance_floor_gated", plrno);
  secfile_insert_int(file, plr_data->stance_pos_moved,
                     "player%d.deep_state.stance_pos_moved", plrno);

  /* The operations layer (W33 phase 4). Free slots save only their
   * NONE state, so quiet players cost a handful of keys. */
  secfile_insert_int(file, plr_data->ops_formed,
                     "player%d.deep_state.ops_formed", plrno);
  secfile_insert_int(file, plr_data->ops_launched,
                     "player%d.deep_state.ops_launched", plrno);
  secfile_insert_int(file, plr_data->cadence_home,
                     "player%d.deep_state.cadence_home", plrno);
  secfile_insert_int(file, plr_data->cadence_unitturns,
                     "player%d.deep_state.cadence_unitturns", plrno);
  secfile_insert_int(file, plr_data->last_strike_city,
                     "player%d.deep_state.last_strike_city", plrno);
  secfile_insert_int(file, plr_data->last_strike_turn,
                     "player%d.deep_state.last_strike_turn", plrno);
  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    const struct deep_operation *op = &plr_data->ops[oi];

    secfile_insert_int(file, op->state,
                       "player%d.deep_state.op%d.state", plrno, oi);
    if (op->state == DEEP_OP_NONE) {
      continue;
    }
    secfile_insert_int(file, op->kind,
                       "player%d.deep_state.op%d.kind", plrno, oi);
    secfile_insert_int(file, op->target_city,
                       "player%d.deep_state.op%d.target", plrno, oi);
    secfile_insert_int(file, op->staging_city,
                       "player%d.deep_state.op%d.staging", plrno, oi);
    secfile_insert_int(file, op->required,
                       "player%d.deep_state.op%d.required", plrno, oi);
    secfile_insert_int(file, op->formed_turn,
                       "player%d.deep_state.op%d.formed_turn", plrno, oi);
    secfile_insert_int(file, op->march_turn,
                       "player%d.deep_state.op%d.march_turn", plrno, oi);
    secfile_insert_int(file, op->march_eta,
                       "player%d.deep_state.op%d.march_eta", plrno, oi);
    secfile_insert_int(file, op->phase_turn,
                       "player%d.deep_state.op%d.phase_turn", plrno, oi);
    secfile_insert_int(file, op->ferry,
                       "player%d.deep_state.op%d.ferry", plrno, oi);
    secfile_insert_int(file, op->beachhead,
                       "player%d.deep_state.op%d.beachhead", plrno, oi);
    secfile_insert_int(file, op->rendezvous,
                       "player%d.deep_state.op%d.rendezvous", plrno, oi);
    /* MUSTER (ERA 3b): a muster that dissolved on reload would hand
     * its accumulated force back to ordinary management, which is the
     * one thing the mechanism exists to prevent. */
    secfile_insert_int(file, op->muster,
                       "player%d.deep_state.op%d.muster", plrno, oi);
    secfile_insert_int(file, op->muster_worth,
                       "player%d.deep_state.op%d.muster_worth", plrno, oi);
    secfile_insert_int(file, op->muster_best,
                       "player%d.deep_state.op%d.muster_best", plrno, oi);
    secfile_insert_int(file, op->muster_grew,
                       "player%d.deep_state.op%d.muster_grew", plrno, oi);
    secfile_insert_int(file, op->n_members,
                       "player%d.deep_state.op%d.n_members", plrno, oi);
    for (i = 0; i < op->n_members; i++) {
      secfile_insert_int(file, op->members[i],
                         "player%d.deep_state.op%d.member%d",
                         plrno, oi, i);
    }
  }
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Load deep-only per-player state; every key is defaulted so saves
  written without this module load cleanly.
**************************************************************************/
static void dpai_player_load(struct player *pplayer,
                             const struct section_file *file, int plrno)
{
  struct deep_plr *plr_data = deep_plr_data(pplayer);
  int oi, i;
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  plr_data->dv_turns_logged
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.dv_turns_logged",
                                 plrno);

  /* Defaulted to identity, so a save written by a build without the
   * stance layer loads as today's game rather than as a zeroed
   * stance, which is a ratio nothing can be read from. */
  plr_data->stance.econ
    = secfile_lookup_float_default(file, 1.0,
                                   "player%d.deep_state.stance_econ",
                                   plrno);
  plr_data->stance.mil
    = secfile_lookup_float_default(file, 1.0,
                                   "player%d.deep_state.stance_mil",
                                   plrno);
  plr_data->stance.cult
    = secfile_lookup_float_default(file, 1.0,
                                   "player%d.deep_state.stance_cult",
                                   plrno);
  plr_data->stance_fear_gated
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.stance_fear_gated",
                                 plrno);
  plr_data->stance_op_gated
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.stance_op_gated",
                                 plrno);
  plr_data->stance_growth_gated
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.stance_growth_gated",
                                 plrno);
  plr_data->stance_expand_gated
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.stance_expand_gated",
                                 plrno);
  plr_data->stance_assault_gated
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.stance_assault_gated",
                                 plrno);
  plr_data->stance_floor_gated
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.stance_floor_gated",
                                 plrno);
  plr_data->stance_pos_moved
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.stance_pos_moved",
                                 plrno);
  plr_data->ops_formed
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.ops_formed", plrno);
  plr_data->ops_launched
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.ops_launched", plrno);
  plr_data->cadence_home
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.cadence_home", plrno);
  plr_data->cadence_unitturns
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.cadence_unitturns",
                                 plrno);

  /* Read back out loud: this hook runs before the next phase
   * re-resolves the axes, so it is the only place the round trip is
   * observable. Keyed on plrno, not the name -- this fires inside
   * sg_load_player_main and the player is not fully built yet. */
  log_base(deep_metric_log_level(),
           "deep-stance-load plr=%d econ=%.3f mil=%.3f cult=%.3f",
           plrno, plr_data->stance.econ, plr_data->stance.mil,
           plr_data->stance.cult);

  plr_data->last_strike_city
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.last_strike_city",
                                 plrno);
  plr_data->last_strike_turn
    = secfile_lookup_int_default(file, 0,
                                 "player%d.deep_state.last_strike_turn",
                                 plrno);
  /* This hook fires inside sg_load_player_main, BEFORE the player's
   * units load -- ids cannot be validated here (every lookup would
   * come back NULL and empty the op). Restore them raw; the per-turn
   * operations pass prunes dead and defected members and aborts
   * hollowed ops before anything acts on them. */
  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    struct deep_operation *op = &plr_data->ops[oi];
    int state = secfile_lookup_int_default(file, DEEP_OP_NONE,
                                           "player%d.deep_state.op%d.state",
                                           plrno, oi);
    int n;

    if (state <= DEEP_OP_NONE || state > DEEP_OP_CONSOLIDATE) {
      op->state = DEEP_OP_NONE;
      op->n_members = 0;
      continue;
    }
    op->state = state;
    {
      int kind = secfile_lookup_int_default(file, DEEP_OPK_ASSAULT,
                                            "player%d.deep_state.op%d.kind",
                                            plrno, oi);

      /* Anything unrecognised loads as an assault, which is what a
       * save written by a newer build would otherwise smuggle in. */
      op->kind = (kind == DEEP_OPK_RAID ? DEEP_OPK_RAID
                  : kind == DEEP_OPK_LIFT ? DEEP_OPK_LIFT
                  : DEEP_OPK_ASSAULT);
    }
    op->target_city
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.target",
                                   plrno, oi);
    op->staging_city
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.staging",
                                   plrno, oi);
    op->required
      = secfile_lookup_int_default(file, 1,
                                   "player%d.deep_state.op%d.required",
                                   plrno, oi);
    op->formed_turn
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.formed_turn",
                                   plrno, oi);
    op->march_turn
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.march_turn",
                                   plrno, oi);
    op->march_eta
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.march_eta",
                                   plrno, oi);
    op->phase_turn
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.phase_turn",
                                   plrno, oi);
    op->ferry
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.ferry",
                                   plrno, oi);
    op->beachhead
      = secfile_lookup_int_default(file, -1,
                                   "player%d.deep_state.op%d.beachhead",
                                   plrno, oi);
    /* Saves written before the rendezvous existed default to -1, and
     * the muster re-derives one on its next pass. */
    op->rendezvous
      = secfile_lookup_int_default(file, -1,
                                   "player%d.deep_state.op%d.rendezvous",
                                   plrno, oi);
    /* Saves written before ERA 3b default to 0: not a muster, which is
     * exactly what an op formed under the odds gate was. */
    op->muster
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.muster",
                                   plrno, oi);
    op->muster_worth
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.muster_worth",
                                   plrno, oi);
    op->muster_best
      = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.muster_best",
                                   plrno, oi);
    /* An absent key defaults to the load turn rather than to 0, so a
     * pre-3b save cannot present a muster that has stalled since turn
     * zero. A save that carries the field keeps it: turn numbering is
     * continuous across a reload of the same game. */
    op->muster_grew
      = secfile_lookup_int_default(file, game.info.turn,
                                   "player%d.deep_state.op%d.muster_grew",
                                   plrno, oi);
    n = secfile_lookup_int_default(file, 0,
                                   "player%d.deep_state.op%d.n_members",
                                   plrno, oi);
    n = MIN(MAX(n, 0), DEEP_OP_MEMBERS);
    op->n_members = 0;
    for (i = 0; i < n; i++) {
      int id = secfile_lookup_int_default(file, 0,
                                          "player%d.deep_state.op%d.member%d",
                                          plrno, oi, i);

      if (id > 0) {
        op->members[op->n_members++] = id;
      }
    }
    if (op->n_members == 0) {
      op->state = DEEP_OP_NONE;
      continue;
    }
    if (op->state == DEEP_OP_MARCH && op->kind == DEEP_OPK_ASSAULT) {
      /* Member tasks are not saved: re-point the marchers once the
       * server is fully up (the first operations pass). */
      plr_data->ops_reissue = TRUE;
    }
  }
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_gained_control(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_gained_control(deftype, pplayer);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_split_by_civil_war(struct player *original,
                                   struct player *created)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(original);

  dai_assess_danger_player(deftype, &(wld.map), original);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_created_by_civil_war(struct player *original,
                                     struct player *created)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(created);

  dai_player_copy(deftype, original, created);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Hand this seat the change-production penalty every human pays.

  `change_build_target()` (server/citytools.c:3221) runs
  `city_change_production_penalty()` only for humans and for AI players
  carrying H_PRODCHGPEN, and no difficulty level above `easy` sets that
  flag (ai/difficulty.c:71/91) -- so every AI seat, deep included,
  switches production for free while a human forfeits half the standing
  stock. Deep declines the exemption: it ORs the handicap onto its own
  player in its own hook, which is the same shape as reading its own
  H_MAP instead of asking the server for an exception, and costs
  nothing in ai/default, ai/baseline or the server.

  Re-checked every phase rather than once, because set_ai_level() can
  rewrite the whole handicap vector mid-game and would otherwise
  silently hand the exemption back.

  Never REMOVES the flag: a seat that legitimately carries it from its
  difficulty level keeps it when the knob is off, so off means "deep
  adds nothing", not "deep guarantees exemption".
**************************************************************************/
static void deep_impose_switch_penalty(struct player *pplayer)
{
  bv_handicap *handicaps;

  if (deep_tuning_get()->pay_switch_penalty <= 0
      || is_human(pplayer)
      || pplayer->ai_common.handicaps == NULL
      || has_handicap(pplayer, H_PRODCHGPEN)) {
    return;
  }

  handicaps = (bv_handicap *) pplayer->ai_common.handicaps;
  BV_SET(*handicaps, H_PRODCHGPEN);

  /* Once per seat per game in the ordinary case, which makes it the
   * execution counter for the switch: no line, no imposition. */
  log_base(deep_metric_log_level(),
           "deep-payswitch turn=%d player=\"%s\" imposed=1",
           game.info.turn, player_name(pplayer));
}

/**********************************************************************//**
  Hand this seat the information handicaps no difficulty level gives it.

  `H_MAP` and `H_FOG` are set at exactly one level -- `AI_LEVEL_AWAY`,
  the "I am not playing this civ" mode -- and `H_TARGETS` stops at
  `normal` (ai/difficulty.c:52-133). So at every level this project
  benchmarks, deep sees the whole map, through all fog, and may target
  anything. RC6 in 00-context.md is the consequence: a strategy layer
  measured under omniscience is not being measured.

  `honest_info != 0` ORs the three bits onto deep's own player, in
  deep's own hook, exactly as `deep_impose_switch_penalty()` does with
  H_PRODCHGPEN -- nothing in ai/default, ai/baseline or the server
  changes, and no exemption is asked for.

  Re-checked every phase rather than once, because `set_ai_level()` can
  rewrite the whole handicap vector mid-game.

  Never REMOVES a bit: an AWAY seat keeps the handicaps its level gave
  it when the knob is off, so off means "deep adds nothing", not "deep
  guarantees omniscience". The counter is therefore edge-triggered --
  it counts the phase that first imposed each bit on each seat, which
  is the execution evidence that the switch reached the player.

  This closes the GATED sites only. Everything ungated stays
  omniscient; the inventory is in
  findings/omniscience-census-2026-08-11.md.
**************************************************************************/
static void deep_impose_honest_info(struct player *pplayer)
{
  bv_handicap *handicaps;
  int imposed = 0;

  if (deep_tuning_get()->honest_info <= 0
      || is_human(pplayer)
      || pplayer->ai_common.handicaps == NULL) {
    return;
  }

  handicaps = (bv_handicap *) pplayer->ai_common.handicaps;

  if (!has_handicap(pplayer, H_MAP)) {
    BV_SET(*handicaps, H_MAP);
    imposed++;
  }
  if (!has_handicap(pplayer, H_FOG)) {
    BV_SET(*handicaps, H_FOG);
    imposed++;
  }
  if (!has_handicap(pplayer, H_TARGETS)) {
    BV_SET(*handicaps, H_TARGETS);
    imposed++;
  }

  if (imposed > 0) {
    log_base(deep_metric_log_level(),
             "deep-honest turn=%d player=\"%s\" imposed=%d"
             " map=%d fog=%d targets=%d",
             game.info.turn, player_name(pplayer), imposed,
             has_handicap(pplayer, H_MAP) ? 1 : 0,
             has_handicap(pplayer, H_FOG) ? 1 : 0,
             has_handicap(pplayer, H_TARGETS) ? 1 : 0);
  }
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_data_phase_begin(struct player *pplayer, bool is_new_phase)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  /* Before the forwarded phase, not after: dai_data_phase_begin()
   * refreshes the advisor data through adv_data_get(), and
   * adv_data_phase_init() gates its continent/ocean and threat census
   * on H_TARGETS and map_is_known (advdata.c:426-446). Imposing
   * afterwards would leave that census omniscient for the whole phase
   * it was built in. */
  deep_impose_honest_info(pplayer);

  dai_data_phase_begin(deftype, pplayer, is_new_phase);

  deep_impose_switch_penalty(pplayer);

  /* The phase is the unit of coherence for "will this city hold": the
   * citizen arrangement asks at the top of it and the growth ladder at
   * the bottom, and they must get the same answer. Clearing here also
   * means no verdict can outlive the phase it was taken in. */
  deep_survival_cache_clear();
  /* A hold expresses one phase's decision about one unit and may never
   * outlive it; this is what lets the manage loop tell a hold taken
   * this turn from the stale flag left by the last one. */
  deep_phase_holds_clear(pplayer);

  if (is_new_phase) {
    struct deep_plr *plr_data = deep_plr_data(pplayer);

    /* Before anything reads it: every gate this turn asks the same
     * axes the log line reports. */
    deep_stance_refresh(pplayer);
    deep_stance_log(pplayer);

    /* A fresh rate cap for whatever deep says to its allies this turn
     * (CHATTY). Output only, and a no-op while chatty_gate is 0. */
    deep_chat_phase_begin(pplayer);

    /* And retire the ally requests whose window has closed, before any
     * decision this turn reads them (CHATTY slice 2). A no-op while
     * chatty_inbound_gate is 0, because nothing can ever have been
     * stored. */
    deep_request_phase_begin(pplayer);

    /* Before the turn's movement, so a unit that marches out today is
     * counted where it slept (NETTHREAT item 3's cadence). */
    deep_cadence_sample(pplayer);

    /* And the mirror question about THEIR units (NETTHREAT item 2's
     * probe): the enemy garrisons we sized a wave against -- did any
     * of them ever come? Same placement and the same reason: a tag
     * opened last turn is scored on where its unit slept. */
    deep_pin_poll(pplayer);
    /* Beside it, the discount's own ledger: how much wave mass the
     * pin classification weighed down for this player so far. Silent
     * until threat_pin_discount arms the seam. */
    deep_pindisc_log(pplayer);

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
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.

  Also reports the arena's coverage instrument. A scopeless read is a
  tunable fetched with no seat in scope while seats hold DIFFERENT
  values -- i.e. a decision taken on the base values instead of the
  seat's. It cannot happen in the single-file case (every seat's
  pointer is the base struct's), so the line only appears in an arena,
  and a nonzero count there names a path no guard covers.
**************************************************************************/
static void dpai_data_phase_finished(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  /* Before the forwarded call, while this phase's state is still the
   * one deep decided in: what deep says to its allies about the turn
   * it just played (CHATTY). Output only, and a no-op while
   * chatty_gate is 0. */
  deep_chat_phase_finished(pplayer);

  dai_data_phase_finished(deftype, pplayer);
  /* end_phase() has run last_activities for EVERY player by now
   * (srv_main.c:1526 then :1598), so this is the one point at which a
   * STOCK seat's tech_want[] is complete and observable from here.
   * Self-guarded to once per turn. */
  deep_techcensus_turn_report();
  if (deep_tuning_arena_active()) {
    log_normal("deep-arena turn=%d player=\"%s\" scopeless_reads=%d",
               game.info.turn, player_name(pplayer),
               deep_tuning_scopeless_reads());
  }
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_alloc(struct city *pcity)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(city_owner(pcity));

  dai_city_alloc(deftype, pcity);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_free(struct city *pcity)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(city_owner(pcity));

  dai_city_free(deftype, pcity);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  City-ownership ledger (benchmark/cities.py). city_got fires both when
  a city is founded and when one is taken; founded==turn tells the
  extractor which. city_lost fires when a city is taken away or
  destroyed; the server does not say which here, and both count as a
  defence failure. The stock AI has no handler on either hook, so
  logging is all these do.
**************************************************************************/
static void dpai_city_got(struct player *pplayer, struct city *pcity)
{
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  if (server_state() != S_S_RUNNING) {
    deep_tuning_scope_restore(deep_scope);
    return;   /* Savegame load is not a founding. */
  }
  log_base(deep_metric_log_level(),
           "deep-city turn=%d player=\"%s\" got=%d size=%d founded=%d"
           " at=(%d,%d)",
           game.info.turn, player_name(pplayer), pcity->id,
           city_size_get(pcity), pcity->turn_founded,
           TILE_XY(city_tile(pcity)));
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  See dpai_city_got().
**************************************************************************/
static void dpai_city_lost(struct player *pplayer, struct city *pcity)
{
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  if (server_state() != S_S_RUNNING) {
    deep_tuning_scope_restore(deep_scope);
    return;   /* Game-end teardown frees every city through
               * remove_city(); that is not a defence failure.
               * (Smoke run 2026-07-31: founded == lost in every
               * seed, all at the final turn.) */
  }
  log_base(deep_metric_log_level(),
           "deep-city turn=%d player=\"%s\" lost=%d size=%d founded=%d"
           " at=(%d,%d)",
           game.info.turn, player_name(pplayer), pcity->id,
           city_size_get(pcity), pcity->turn_founded,
           TILE_XY(city_tile(pcity)));
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_save(struct section_file *file, const struct city *pcity,
                          const char *citystr)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(city_owner(pcity));

  dai_city_save(deftype, "deep", file, pcity, citystr);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_load(const struct section_file *file, struct city *pcity,
                          const char *citystr)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(city_owner(pcity));

  dai_city_load(deftype, "deep", file, pcity, citystr);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_build_adv_override(struct city *pcity, struct adv_choice *choice)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(city_owner(pcity));

  dai_build_adv_override(deftype, pcity, choice);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_wonder_city_distance(struct player *pplayer, struct adv_data *adv)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_wonder_city_distance(deftype, pplayer, adv);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_build_adv_init(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_build_adv_init(deftype, pplayer);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_build_adv_adjust(struct player *pplayer, struct city *wonder_city)
{
  struct ai_type *deftype = deep_ai_get_self();
  bool hide = FALSE;
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  /* Charge the switch once. adjust_improvement_wants_by_effects()
   * (daicity.c:1899-1911) docks a building want for the shields a
   * class change would lose, and the dock DOUBLES from
   * `shield_stock * SHIELD_WEIGHTING / 15` to `/ 4` the moment the seat
   * carries H_PRODCHGPEN -- so the honesty flag alone would silently
   * quadruple a WAG that deep is about to replace with the real
   * forfeit. While deep prices the switch itself, the handicap is
   * hidden from this call, which leaves stock's dock in the /15 form it
   * has had in every measurement ever taken and makes deep's debit the
   * only response to the new cost.
   *
   * Hidden, not cleared: the bit goes back before the function
   * returns, and nothing between here and there changes production. */
  if (deep_tuning_get()->switch_cost_price > 0
      && deep_tuning_get()->pay_switch_penalty > 0
      && !is_human(pplayer)
      && pplayer->ai_common.handicaps != NULL
      && has_handicap(pplayer, H_PRODCHGPEN)) {
    BV_CLR(*((bv_handicap *) pplayer->ai_common.handicaps), H_PRODCHGPEN);
    hide = TRUE;
  }

  dai_build_adv_adjust(deftype, pplayer, wonder_city);

  if (hide) {
    BV_SET(*((bv_handicap *) pplayer->ai_common.handicaps), H_PRODCHGPEN);
  }
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Deep's own government valuation (deepgov.c), which forwards to the
  default AI's until its knob is moved.
**************************************************************************/
static void dpai_gov_value(struct player *pplayer, struct government *gov,
                          adv_want *val, bool *override)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  deep_gov_value(deftype, pplayer, gov, val, override);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_units_ruleset_init(void)
{
  struct ai_type *deftype = deep_ai_get_self();

  dai_units_ruleset_init(deftype);
  /* The census classifies advances by the unit types gated on them. */
  deep_techcensus_ruleset_reset();
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
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  dai_unit_init(deftype, punit);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_close(struct unit *punit)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  dai_unit_close(deftype, punit);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_ferry_init_ferry(struct unit *ferry)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(ferry));

  dai_ferry_init_ferry(deftype, ferry);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_ferry_transformed(struct unit *ferry, const struct unit_type *old)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(ferry));

  dai_ferry_transformed(deftype, ferry, old);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_ferry_lost(struct unit *punit)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  /* Loss-rate bookkeeping (benchmark/losses.py). The hook also fires
   * when a unit merely changes owner (capture, bribe); the server then
   * leaves loss_reason at -1. */
  log_base(deep_metric_log_level(),
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
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter. Also the stall
  tripwire (W33 phase 3): a unit claimed by a live operation that
  ends the turn with moves unspent is a blade the op froze without
  using -- the exact symptom of deep's intentions and stock's manage
  loop fighting over a unit, watched distributionally rather than
  asserted (holds like tired-attack are legitimate stalls).
**************************************************************************/
static void dpai_unit_turn_end(struct unit *punit)
{
  struct ai_type *deftype = deep_ai_get_self();
  struct player *owner = unit_owner(punit);
  int oi = deep_op_claiming(owner, punit->id);
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  if (oi >= 0 && punit->moves_left > 0) {
    const struct deep_operation *op = &deep_plr_data(owner)->ops[oi];

    log_base(deep_metric_log_level(),
             "deep-op-stall turn=%d player=\"%s\" op=%d kind=%d state=%d"
             " unit=\"%s\" moves=%d at=(%d,%d)",
             game.info.turn, player_name(owner), oi, op->kind, op->state,
             unit_rule_name(punit), punit->moves_left,
             TILE_XY(unit_tile(punit)));
  }

  dai_unit_turn_end(deftype, punit);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_move_or_attack(struct unit *punit, struct tile *ptile,
                                    struct pf_path *path, int step)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  dai_unit_move_or_attack(deftype, punit, ptile, path, step);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Every move the server completed, ours and everyone else's -- the
  hook is CALL_FUNC_EACH_AI, so this fires for units belonging to
  players running other modules, which is exactly what the
  garrison-pin probe needs (NETTHREAT item 2).

  Read-only and gated: it looks a tagged unit up and records where it
  now stands. Deep took this hook for the first time here; it was NULL
  before, and it forwards nothing because `ai/default` never claimed
  it either.
**************************************************************************/
static void dpai_unit_move_seen(struct unit *punit)
{
  deep_pin_note_move(punit);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_new_adv_task(struct unit *punit, enum adv_unit_task task,
                                  struct tile *ptile)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  dai_unit_new_adv_task(deftype, punit, task, ptile);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_save(struct section_file *file, const struct unit *punit,
                          const char *unitstr)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  dai_unit_save(deftype, "deep_", file, punit, unitstr);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_load(const struct section_file *file, struct unit *punit,
                          const char *unitstr)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  dai_unit_load(deftype, "deep_", file, punit, unitstr);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_auto_settler_reset(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_auto_settler_reset(deftype, pplayer);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Deep settler engine: risk-aware site search and routing (W05).
**************************************************************************/
static void dpai_auto_settler_run(struct player *pplayer,
                                 struct unit *punit,
                                 struct workermap *state)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  deep_auto_settler_run(deftype, &(wld.map), pplayer, punit, state);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_auto_settler_cont(struct player *pplayer,
                                  struct unit *punit,
                                  struct workermap *state)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);
  /* This hook is the ONE path by which a worker already building loses
   * its activity to a decision rather than to finishing:
   * auto_workers_player() sends a non-idle worker here and nowhere else
   * (autoworkers.c:1399-1408), and all dai_auto_settler_cont() does is
   * idle it when adv_worker_safe_tile() refuses the ground under it.
   * Observed, not intercepted -- the call below is unchanged, and the
   * comparison either side of it costs one enum read. */
  enum unit_activity before = punit->activity;

  dai_auto_settler_cont(deftype, &(wld.map), pplayer, punit, state);
  if (before != ACTIVITY_IDLE && punit->activity == ACTIVITY_IDLE) {
    deep_funnel_note_flee(pplayer, punit);
  }
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_switch_to_explore(struct unit *punit, struct tile *target,
                                  enum override_bool *allow)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  dai_switch_to_explore(deftype, punit, target, allow);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_do_first_activities(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  /* Before anything touches a unit: stock's diplomat handling bribes,
   * defends, targets, walks and ACTS in one call, so the action choice
   * can only be intercepted upstream of it (deepspy.c). Off by
   * default, in which case this is a census and nothing else. */
  deep_spy_manage(deftype, pplayer);

  deep_do_first_activities(deftype, pplayer);

  pplayer->ai_phase_done = TRUE;
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Mark turn done as we have already done everything before game was saved.
**************************************************************************/
static void dpai_restart_phase(struct player *pplayer)
{
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  pplayer->ai_phase_done = TRUE;
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_diplomacy_actions(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  struct deep_plr *dp = deep_plr_data(pplayer);
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  /* PEACE's pre-pass, and it must run BEFORE stock's pass: stock's
   * anti-spam gate skips the whole AI-AI branch for fc_rand(4)+3
   * turns and the cease-fire peace window is four turns wide, so the
   * pass that would propose peace is not running on the turns the
   * counterparty is allowed to say yes. With every PEACE knob at its
   * compiled 0 this refreshes the leader read, logs it, and returns.
   * (Off-switch proof: benchmark/tuning/peace-off.ini.) */
  /* Interdependence before either pass reads love: an active trade
   * route is a standing reason to keep the peace, and it has to be in
   * the number by the time the peace decision looks at it.
   * trade_route_love = 0.0 (the compiled default) returns immediately.
   * (deeptrade.c) */
  deep_trade_route_love(deftype, pplayer);

  deep_diplomacy_prepass(deftype, pplayer);

  dai_diplomacy_actions(deftype, pplayer);

  /* Retract a war countdown that quietly lapsed.
   *
   * war_countdown() (daidiplomacy.c) broadcasts "we will declare war on
   * X in N turns" to every ally, and NOTHING ever takes it back. The
   * countdown can run out, or the reason can evaporate, and the last
   * thing the alliance was told remains a war that never came. Deep
   * watches the same counter stock is decrementing and says so when it
   * stops -- without a declaration having happened.
   *
   * Purely social, like deep_brief_allies(): no want reads this and no
   * state changes. It closes a loop stock opens and abandons. */
  players_iterate_alive(aplayer) {
    struct ai_dip_intel *adip;
    int slot = player_index(aplayer);
    int was, now;

    if (aplayer == pplayer || slot < 0 || slot >= MAX_NUM_PLAYER_SLOTS) {
      continue;
    }
    adip = dai_diplomacy_get(deftype, pplayer, aplayer);
    now = (adip != NULL) ? adip->countdown : -1;
    was = dp->war_countdown_seen[slot];
    dp->war_countdown_seen[slot] = now;

    if (was > 0 && now <= 0 && !pplayers_at_war(pplayer, aplayer)) {
      /* The countdown ended and no war followed. */
      players_iterate_alive(ally) {
        if (ally == pplayer || !pplayers_allied(pplayer, ally)) {
          continue;
        }
        notify_player(ally, NULL, E_DIPLOMACY, ftc_chat_private,
                      _("*%s (AI)* We are no longer preparing war against"
                        " %s. Disregard our earlier call to arms."),
                      player_name(pplayer), player_name(aplayer));
      } players_iterate_alive_end;
    }
  } players_iterate_alive_end;
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_do_last_activities(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  deep_do_last_activities(deftype, pplayer);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_treaty_evaluate(struct player *pplayer, struct player *aplayer,
                                struct treaty *ptreaty)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  /* Deep claims only the treaty shapes it prices in full -- a lone
   * peace clause, or a treaty made of nothing but embassy clauses --
   * and forwards every other treaty untouched. */
  if (deep_treaty_evaluate(deftype, pplayer, aplayer, ptreaty)) {
    deep_tuning_scope_restore(deep_scope);
    return;
  }

  dai_treaty_evaluate(deftype, pplayer, aplayer, ptreaty);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_treaty_accepted(struct player *pplayer, struct player *aplayer,
                                struct treaty *ptreaty)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_treaty_accepted(deftype, pplayer, aplayer, ptreaty);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_diplomacy_first_contact(struct player *pplayer,
                                        struct player *aplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_diplomacy_first_contact(deftype, pplayer, aplayer);
  deep_tuning_scope_restore(deep_scope);
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
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(receiver);

  dai_incident(deftype, type, scope, paction, receiver, violator, victim);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_city_log(char *buffer, int buflength, const struct city *pcity)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(city_owner(pcity));

  dai_city_log(deftype, buffer, buflength, pcity);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_unit_log(char *buffer, int buflength, const struct unit *punit)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  dai_unit_log(deftype, buffer, buflength, punit);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_consider_plr_dangerous(struct player *plr1, struct player *plr2,
                                       enum override_bool *result)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(plr1);

  dai_consider_plr_dangerous(deftype, plr1, plr2, result);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_consider_tile_dangerous(struct tile *ptile, struct unit *punit,
                                        enum override_bool *result)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(unit_owner(punit));

  dai_consider_tile_dangerous(deftype, ptile, punit, result);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_consider_wonder_city(struct city *pcity, bool *result)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope
    = deep_tuning_scope_set(city_owner(pcity));

  dai_consider_wonder_city(deftype, pcity, result);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
static void dpai_revolution_start(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_revolution_start(deftype, pplayer);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Call default ai with deep ai type as parameter.
**************************************************************************/
/**********************************************************************//**
  A chat line addressed to this deep player, from the server's own idea
  of who typed it. The per-seat tuning scope is entered here because
  chat arrives OUTSIDE any phase -- a human can type between turns --
  so nothing else has set the scope for us.
**************************************************************************/
static void dpai_player_chat_msg(struct player *pplayer,
                                 struct player *sender, const char *msg)
{
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  deep_request_hear(pplayer, sender, msg);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  The AI console, `aicmd <player> <command>` (server/stdinhand.c:5918,
  ALLOW_ADMIN). Deep implements exactly one verb:

    aicmd <deep player> chat <sender> <the line>

  and it exists so a benchmark game can exercise the inbound protocol
  at all. A benchmark game has NO CLIENT CONNECTED, so no chat packet
  can ever arrive; without this the four commands could be reasoned
  about and never run.

  Say plainly what is and is not proved through it. The line goes into
  deep_request_hear() unmodified, so the parse, the alliance test, the
  city grounding, the rate cap, the reply and all three prices are the
  real ones -- <sender> is looked up as a real player and the alliance
  test is against the real diplomatic state, so "allies only" is
  genuinely exercised, refusal included. What is NOT the real one is
  the ATTRIBUTION: over a socket the sender is the connection's own
  player and cannot be chosen, whereas here the operator names it. That
  is why the verb is behind an ADMIN-only server command and not behind
  anything a player can reach.
**************************************************************************/
static void dpai_player_console(struct player *pplayer, const char *cmd)
{
  const struct deep_tuning *deep_scope;
  char buf[MAX_LEN_MSG], *rest;
  struct player *sender;

  if (cmd == NULL || 0 != fc_strncasecmp(cmd, "chat ", 5)) {
    log_normal("deep aicmd: only \"chat <sender> <line>\" is understood");
    return;
  }
  sz_strlcpy(buf, cmd + 5);
  rest = strchr(buf, ' ');
  if (rest == NULL) {
    log_normal("deep aicmd: \"chat\" wants a sender and a line");
    return;
  }
  *rest++ = '\0';
  sender = player_by_name(buf);
  if (sender == NULL) {
    log_normal("deep aicmd: no player named \"%s\"", buf);
    return;
  }

  deep_scope = deep_tuning_scope_set(pplayer);
  deep_request_hear(pplayer, sender, rest);
  deep_tuning_scope_restore(deep_scope);
}

static void dpai_sanity_check(struct player *pplayer)
{
  struct ai_type *deftype = deep_ai_get_self();
  const struct deep_tuning *deep_scope = deep_tuning_scope_set(pplayer);

  dai_sanity_check(deftype, pplayer);
  deep_tuning_scope_restore(deep_scope);
}

/**********************************************************************//**
  Setup player ai_funcs function pointers.
**************************************************************************/
bool fc_ai_deep_setup(struct ai_type *ai)
{
  aw_set_task_value_hook(deep_worker_task_value);
  deep_chain_register();

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
  ai->funcs.game_free = dpai_game_free;

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
  */
  ai->funcs.city_got = dpai_city_got;
  ai->funcs.city_lost = dpai_city_lost;
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
  ai->funcs.unit_move_seen = dpai_unit_move_seen;
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

  /* CHATTY slice 2: the inbound channel. server/handchat.c calls this
   * for a chat line ADDRESSED to a deep player -- the only way any
   * text reaches a module, since an AI player holds no connection for
   * the ordinary delivery paths to walk. The parser behind it returns
   * on its first comparison while chatty_inbound_gate is 0. */
  ai->funcs.player_chat_msg = dpai_player_chat_msg;
  ai->funcs.player_console = dpai_player_console;

  return TRUE;
}
