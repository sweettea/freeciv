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
#include "map.h"
#include "movement.h"
#include "player.h"
#include "terrain.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* common/aicore */
#include "citymap.h"
#include "path_finding.h"
#include "pf_tools.h"

/* server */
#include "citytools.h"
#include "handicaps.h"
#include "srv_log.h"
#include "unittools.h"

/* server/advisors */
#include "advgoto.h"
#include "advtools.h"
#include "autoworkers.h"

/* ai/default */
#include "daidata.h"
#include "daiferry.h"
#include "daiguard.h"
#include "dailog.h"
#include "daiplayer.h"
#include "daisettler.h"
#include "daitools.h"
#include "daiunit.h"

#include "deeptuning.h"

#include "deepsettler.h"

/* The stock settler site search prices travel as if the map were empty
 * and at peace: its land search parameter has no tile behaviour, no
 * risk cost and no danger callback (find_best_city_placement() in
 * daisettler.c), although the worker search and the overseas settler
 * search in the same subsystem both set protective callbacks. Sites
 * behind a war zone therefore look as cheap as safe ones. Here the
 * search pays for risk, refuses hostile territory and contested tiles,
 * and never plans to end a turn where the settler would likely die;
 * dangerous routes then get longer, and amortize() de-ranks the sites
 * behind them with no special-casing (W05). */

struct deep_settler_risk {
  struct adv_risk_cost base;    /* Keep first: adv_avoid_risks() stores
                                 * this struct's address in
                                 * parameter->data, and both its own
                                 * extra-cost callback and ours cast it
                                 * back. */
  struct unit *punit;
};

/**********************************************************************//**
  Tile behaviour for settler pathfinding: the worker rule (never path
  through non-allied territory) plus never pathing through a tile held
  by a non-allied unit -- a settler must not walk into a fight.
**************************************************************************/
static enum tile_behavior
deep_settler_tile_behavior(const struct tile *ptile,
                           enum known_type known,
                           const struct pf_parameter *param)
{
  const struct player *owner = tile_owner(ptile);

  if (owner != NULL && !pplayers_allied(owner, param->owner)) {
    return TB_IGNORE;
  }
  if (is_non_allied_unit_tile(ptile, param->owner,
                              utype_has_flag(param->utype, UTYF_FLAGLESS))) {
    return TB_IGNORE;
  }

  return TB_NORMAL;
}

/**********************************************************************//**
  pf_parameter is_pos_dangerous callback for civilians: never plan to
  end a turn where visible enemies could kill us.
**************************************************************************/
static bool deep_civilian_pos_dangerous(const struct tile *ptile,
                                        enum known_type known,
                                        const struct pf_parameter *param)
{
  const struct deep_settler_risk *risk = param->data;

  return adv_danger_at(risk->punit, (struct tile *) ptile);
}

/**********************************************************************//**
  Make a filled land pf_parameter risk-aware for a settler. 'risk' must
  stay alive for as long as the parameter (or any pf_map made from it)
  is in use.
**************************************************************************/
static void deep_settler_apply_risk(struct pf_parameter *parameter,
                                    struct deep_settler_risk *risk,
                                    struct unit *punit)
{
  if (uclass_has_flag(unit_class_get(punit), UCF_UNREACHABLE)
      || is_barbarian(unit_owner(punit))) {
    /* Same exemptions as dai_fill_unit_param(): the risk code is for
     * ordinary reachable units, and barbarians bravely/stupidly ignore
     * risks. */
    return;
  }

  adv_avoid_risks(parameter, &risk->base, punit,
                  deep_tuning_get()->settler_fearfulness_mult
                  * NORMAL_STACKING_FEARFULNESS);
  risk->punit = punit;
  parameter->get_TB = deep_settler_tile_behavior;
  /* Danger maps cannot be combined with fuel maps, and settlers do not
   * burn fuel. */
  parameter->get_moves_left_req = NULL;
  parameter->is_pos_dangerous = deep_civilian_pos_dangerous;
}

/**********************************************************************//**
  Decide whether this settler wants a bodyguard for the trip to
  dest_tile, and request or release one accordingly.

  The stock dai_gothere_bodyguard() counts hostiles on the destination
  tile only, so a settler crossing a corridor of enemy units to reach an
  empty meadow requests no escort. Here the planned route itself is
  sampled: any tile on it where adv_danger_at() sees a credible threat
  makes the trip guarded.
**************************************************************************/
static bool deep_settler_bodyguard_needed(struct ai_type *ait,
                                          struct unit *punit,
                                          struct tile *dest_tile)
{
  struct unit *guard = aiguard_guard_of(ait, punit);
  bool needed = FALSE;

  if (is_barbarian(unit_owner(punit))) {
    /* Barbarians must have more courage (ie less brains) */
    aiguard_clear_guard(ait, punit);
    return FALSE;
  }

  if (guard != NULL && unit_tile(guard) == unit_tile(punit)) {
    /* The guard is with us already. */
    return TRUE;
  }

  if (adv_danger_at(punit, dest_tile)) {
    needed = TRUE;
  } else if (goto_is_sane(punit, dest_tile)) {
    struct pf_parameter parameter;
    struct deep_settler_risk risk;
    struct pf_map *pfm;
    struct pf_path *ppath;
    const struct civ_map *nmap = &(wld.map);

    pft_fill_unit_parameter(&parameter, nmap, punit);
    parameter.omniscience = !has_handicap(unit_owner(punit), H_MAP);
    deep_settler_apply_risk(&parameter, &risk, punit);

    pfm = pf_map_new(&parameter);
    ppath = pf_map_path(pfm, dest_tile);
    if (ppath != NULL) {
      int i;

      for (i = 1; i < ppath->length; i++) {
        if (adv_danger_at(punit, ppath->positions[i].tile)) {
          needed = TRUE;
          break;
        }
      }
      pf_path_destroy(ppath);
    }
    pf_map_destroy(pfm);
  }

  if (needed) {
    UNIT_LOG(LOG_DEBUG, punit, "deep settler wants a bodyguard to (%d, %d)",
             TILE_XY(dest_tile));
    aiguard_request_guard(ait, punit);
  } else {
    aiguard_clear_guard(ait, punit);
  }

  return needed;
}

/**********************************************************************//**
  Move a settler towards ptile with the same protections the site
  search plans with. The stock execution path (dai_unit_goto() via
  dai_fill_unit_param()) gives settlers a fight-refusing tile behaviour
  and the default risk cost, but no is_pos_dangerous, so it will
  happily plan to end a turn next to a visible attacker.

  Returns FALSE iff the settler died.
**************************************************************************/
static bool deep_settler_goto(struct ai_type *ait, struct unit *punit,
                              struct tile *ptile)
{
  struct pf_parameter parameter;
  struct deep_settler_risk risk;
  const struct civ_map *nmap = &(wld.map);

  pft_fill_unit_parameter(&parameter, nmap, punit);
  parameter.omniscience = !has_handicap(unit_owner(punit), H_MAP);
  deep_settler_apply_risk(&parameter, &risk, punit);

  return dai_unit_goto_constrained(ait, punit, ptile, &parameter);
}

/**********************************************************************//**
  Ferry-enabled goto for settlers; the deep counterpart of
  dai_gothere(), using the route-sampling bodyguard request and the
  risk-aware land leg above. Zone of control stays respected: the
  ZoC-ignoring rationale in the stock long-path code is about attackers
  pushing toward a target that may clear, which does not apply to a
  settler that must not die.

  Return values: TRUE if got to or next to our destination, FALSE
  otherwise.
**************************************************************************/
static bool deep_settler_gothere(struct ai_type *ait,
                                 struct player *pplayer,
                                 struct unit *punit,
                                 struct tile *dest_tile)
{
  bool bg_needed;

  CHECK_UNIT(punit);

  if (same_pos(dest_tile, unit_tile(punit)) || punit->moves_left <= 0) {
    /* Nowhere to go */
    return TRUE;
  }

  /* See if we need a bodyguard for the trip */
  bg_needed = deep_settler_bodyguard_needed(ait, punit, dest_tile);

  if (unit_transported(punit)
      || !goto_is_sane(punit, dest_tile)) {
    /* Must go by boat, call an aiferryboat function */
    if (!aiferry_gobyboat(ait, pplayer, punit, dest_tile, bg_needed)) {
      return FALSE;
    }
  }

  /* Go where we should be going if we can, and are at our destination
   * if we are on a ferry */
  if (goto_is_sane(punit, dest_tile) && punit->moves_left > 0) {
    punit->goto_tile = dest_tile;
    UNIT_LOG(LOG_DEBUG, punit, "deep settler walking to (%d,%d)",
             TILE_XY(dest_tile));
    if (!deep_settler_goto(ait, punit, dest_tile)) {
      /* Died */
      return FALSE;
    }
  } else {
    UNIT_LOG(LOG_DEBUG, punit, "deep settler not moving");
    return FALSE;
  }

  if (def_ai_unit_data(punit, ait)->ferryboat > 0
      && !unit_transported(punit)) {
    /* We probably just landed, release our boat */
    aiferry_clear_boat(ait, punit);
  }

  /* Dead unit shouldn't reach this point */
  CHECK_UNIT(punit);

  return (same_pos(unit_tile(punit), dest_tile)
          || is_tiles_adjacent(unit_tile(punit), dest_tile));
}

/**********************************************************************//**
  Find nearest and best city placement for 'punit'.

  Derived from find_best_city_placement() in daisettler.c (see the
  header comment for why it is not simply reused): the land search gets
  the risk-aware parameter above where the stock search is bare. The
  overseas leg already sets no_fights_or_unknown and is kept as stock.

  Option look_for_boat forces us to find a (real) boat before
  considering going overseas. Option use_virt_boat allows to use
  virtual boat but only if punit is in a coastal city right now (should
  only be used by virtual units).
  If (!look_for_boat && !use_virt_boat), will not consider placements
  overseas.

  Returns the better cityresult or NULL if no result was found.
**************************************************************************/
static struct cityresult *deep_find_best_city_placement(struct ai_type *ait,
                                                        struct unit *punit,
                                                        bool look_for_boat,
                                                        bool use_virt_boat)
{
  struct pf_parameter parameter;
  struct deep_settler_risk risk;
  struct player *pplayer = unit_owner(punit);
  struct unit *ferry = NULL;
  struct cityresult *cr1 = NULL, *cr2 = NULL;
  const struct civ_map *nmap = &(wld.map);

  fc_assert_ret_val(is_ai(pplayer), NULL);
  /* Only virtual units may use virtual boats: */
  fc_assert_ret_val(0 == punit->id || !use_virt_boat, NULL);

  /* Phase 1: Consider building cities on our continent */

  pft_fill_unit_parameter(&parameter, nmap, punit);
  parameter.omniscience = !has_handicap(pplayer, H_MAP);
  deep_settler_apply_risk(&parameter, &risk, punit);
  cr1 = settler_map_iterate(ait, &parameter, punit, 0);

  if (cr1 && cr1->result > RESULT_IS_ENOUGH) {
    /* Skip further searches */
    return cr1;
  }

  /* Phase 2: Consider travelling to another continent */

  if (look_for_boat) {
    int ferry_id = aiferry_find_boat(ait, punit, 1, NULL);

    ferry = game_unit_by_number(ferry_id);
  }

  if (ferry
      || (use_virt_boat
          && is_terrain_class_near_tile(nmap, unit_tile(punit), TC_OCEAN)
          && tile_city(unit_tile(punit)))) {
    if (!ferry) {
      /* No boat?  Get a virtual one! */
      struct unit_type *boattype
        = best_role_unit_for_player(pplayer, L_FERRYBOAT);

      if (boattype == NULL) {
        /* Sea travel not possible yet. Bump tech want for ferries. */
        boattype = get_role_unit(L_FERRYBOAT, 0);

        if (NULL != boattype) {
          struct ai_plr *plr_data = def_ai_player_data(pplayer, ait);

          unit_tech_reqs_iterate(boattype, padv) {
            plr_data->tech_want[advance_index(padv)]
              += FERRY_TECH_WANT;
            TECH_LOG(ait, LOG_DEBUG, pplayer, padv,
                     "+ %d for %s to ferry settler",
                     FERRY_TECH_WANT,
                     utype_rule_name(boattype));
          } unit_tech_reqs_iterate_end;
        }

        /* Return the result from the search on our current continent */
        return cr1;
      }
      ferry = unit_virtual_create(pplayer, NULL, boattype, 0);
      unit_tile_set(ferry, unit_tile(punit));
    }

    fc_assert(dai_is_ferry(ferry, ait));
    pft_fill_unit_overlap_param(&parameter, nmap, ferry);
    parameter.omniscience = !has_handicap(pplayer, H_MAP);
    parameter.get_TB = no_fights_or_unknown;

    cr2 = settler_map_iterate(ait, &parameter, punit,
                              unit_build_shield_cost_base(ferry));
    if (cr2) {
      cr2->overseas = TRUE;
      cr2->virt_boat = (ferry->id == 0);
    }

    if (ferry->id == 0) {
      unit_virtual_destroy(ferry);
    }

    /* If we use a virtual boat, we must have permission and be
     * emigrating: */
    fc_assert_ret_val(!cr2 || (!cr2->virt_boat || use_virt_boat), NULL);
    fc_assert_ret_val(!cr2 || (!cr2->virt_boat || cr2->overseas), NULL);
  }

  if (!cr1) {
    /* No want for a new city on our current continent; return the
     * result for traveling by boat. */
    return cr2;
  } else if (!cr2) {
    /* No want for an overseas city; return the result for a city on
     * our current continent. */
    return cr1;
  }

  /* We want an overseas city and a city on the current continent -
   * select the best! */
  if (cr1->result > cr2->result) {
    cityresult_destroy(cr2);
    return cr1;
  } else {
    cityresult_destroy(cr1);
    return cr2;
  }
}

/**********************************************************************//**
  Auto settler that can also build cities. Derived from
  dai_auto_settler_run() in daisettler.c; the differences are that city
  placements come from the risk-aware search above and that the debug
  citymap printout is dropped.
**************************************************************************/
void deep_auto_settler_run(struct ai_type *ait, const struct civ_map *nmap,
                           struct player *pplayer,
                           struct unit *punit, struct workermap *state)
{
  adv_want best_impr = 0; /* Value of best terrain improvement we can do */
  enum unit_activity best_act;
  struct extra_type *best_target;
  struct tile *best_tile = NULL;
  struct pf_path *path = NULL;
  struct city *pcity = NULL;

  /* Time it will take worker to complete its given task */
  int completion_time = 0;

  CHECK_UNIT(punit);

  /*** If we are on a city mission: Go where we should ***/

BUILD_CITY:

  if (def_ai_unit_data(punit, ait)->task == AIUNIT_BUILD_CITY) {
    struct tile *ptile = punit->goto_tile;
    int sanity = punit->id;

    /* Check that the mission is still possible. If the tile has become
     * unavailable, call it off. */
    if (!city_can_be_built_here(nmap, ptile, punit, FALSE)) {
      dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);
      set_unit_activity(punit, ACTIVITY_IDLE, ACTION_NONE);
      send_unit_info(NULL, punit);

      return; /* Avoid recursion at all cost */
    } else {
      /* Go there */
      if ((!deep_settler_gothere(ait, pplayer, punit, ptile)
           && NULL == game_unit_by_number(sanity))
          || punit->moves_left <= 0) {
        return;
      }
      if (same_pos(unit_tile(punit), ptile)) {
        enum cb_error_level elevel = dai_do_build_city(ait, pplayer, punit);

        if (elevel != CBE_OK) {
          UNIT_LOG(LOG_DEBUG, punit, "could not make city on %s",
                   tile_get_info_text(unit_tile(punit), TRUE, 0));
          dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);

          if (elevel == CBE_RECOVERABLE) {
            /* Only known way to end in here is that hut turned in to a
             * city when settler entered tile. So this is not going to
             * lead in any serious recursion. */
            deep_auto_settler_run(ait, nmap, pplayer, punit, state);
          }

          return;
        } else {
          return; /* We came, we saw, we built... */
        }
      } else {
        UNIT_LOG(LOG_DEBUG, punit, "could not go to target");
        /* dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL); */
        return;
      }
    }
  }

  /*** Try find some work ***/

  if (unit_has_type_flag(punit, UTYF_WORKERS)) {
    struct worker_task *best_task;

    TIMING_LOG(AIT_WORKERS, TIMER_START);

    /* Have nearby cities requests? */
    pcity = worker_evaluate_city_requests(punit, &best_task, &path, state);

    if (pcity != NULL) {
      if (path != NULL) {
        completion_time = pf_path_last_position(path)->turn;
        best_impr = 1;
        best_act = best_task->act;
        best_target = best_task->tgt;
        best_tile = best_task->ptile;
      } else {
        pcity = NULL;
      }
    }

    if (pcity == NULL) {
      best_impr = worker_evaluate_improvements(nmap, punit,
                                               &best_act, &best_target,
                                               &best_tile, &path, state);
      if (path != NULL) {
        completion_time = pf_path_last_position(path)->turn;
      }
    }
    UNIT_LOG(LOG_DEBUG, punit, "impr want " ADV_WANT_PRINTF, best_impr);
    TIMING_LOG(AIT_WORKERS, TIMER_STOP);
  }

  if (unit_is_cityfounder(punit)) {
    struct cityresult *result;

    /* May use a boat: */
    TIMING_LOG(AIT_SETTLERS, TIMER_START);
    result = deep_find_best_city_placement(ait, punit, TRUE, FALSE);
    TIMING_LOG(AIT_SETTLERS, TIMER_STOP);
    if (result && result->result > best_impr) {
      UNIT_LOG(LOG_DEBUG, punit, "city want " ADV_WANT_PRINTF,
               result->result);
      if (tile_city(result->tile)) {
        UNIT_LOG(LOG_DEBUG, punit, "immigrates to %s (%d, %d)",
                 city_name_get(tile_city(result->tile)),
                 TILE_XY(result->tile));
      } else {
        UNIT_LOG(LOG_DEBUG, punit, "makes city at (%d, %d)",
                 TILE_XY(result->tile));
      }
      /* Go make a city! */
      adv_unit_new_task(punit, AUT_BUILD_CITY, result->tile);
      if (result->best_other.tile && result->best_other.tdc->sum >= 0) {
        /* Reserve best other tile (if there is one). It is the tile
         * where the first citizen of the city is working. */
        citymap_reserve_tile(result->best_other.tile, punit->id);
      }
      punit->goto_tile = result->tile; /* TMP */

      cityresult_destroy(result);

      /*** Go back to and found a city ***/
      if (path != NULL) {
        pf_path_destroy(path);
      }
      path = NULL;
      goto BUILD_CITY;
    } else if (best_impr > 0) {
      UNIT_LOG(LOG_DEBUG, punit, "improves terrain instead of founding");
      /* Terrain improvements follows the old model, and is recalculated
       * each turn. */
      if (result) {
        /* We had a city result, just worse than best impr */
        cityresult_destroy(result);
      }
      adv_unit_new_task(punit, AUT_AUTO_WORKER, best_tile);
    } else {
      UNIT_LOG(LOG_DEBUG, punit, "cannot find work");
      fc_assert(result == NULL);
      dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);
      goto CLEANUP;
    }
  } else {
    /* We are a worker or engineer */
    adv_unit_new_task(punit, AUT_AUTO_WORKER, best_tile);
  }

  if (best_tile != NULL
      && auto_worker_setup_work(nmap, pplayer, punit, state, 0, &path,
                                best_tile, best_act, &best_target,
                                completion_time)) {
    if (pcity != NULL) {
      clear_worker_tasks(pcity);
    }
  }

CLEANUP:

  if (NULL != path) {
    pf_path_destroy(path);
  }
}
