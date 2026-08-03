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
#include "combat.h"
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
#include "maphand.h"
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

#include "deepsite.h"
#include "deeptuning.h"

#include "deepai.h"
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
  The overnight-danger bar with a tolerance multiplier. Same
  arithmetic as dai_consider_tile_dangerous() -- the authoritative
  path for AI players inside adv_danger_at() -- copied because the
  public function cannot carry a tolerance (rule 3, stated reason):
  the veto fires when accumulated adjacent enemy attack a^2 * 10
  reaches defence * tolerance, so 1.0 reproduces the stock bar
  exactly and higher is braver.
**************************************************************************/
static bool deep_settler_overnight_veto(struct unit *punit,
                                        const struct tile *ptile,
                                        double tolerance)
{
  struct player *pplayer = unit_owner(punit);
  struct city *pcity = tile_city(ptile);
  int a = 0, d, db;
  int extras_bonus = 0;

  if (pcity != NULL && pplayers_allied(city_owner(pcity), pplayer)
      && !is_non_allied_unit_tile(ptile, pplayer,
                                  unit_has_type_flag(punit,
                                                     UTYF_FLAGLESS))) {
    return FALSE;               /* Safe in a friendly city. */
  }

  db = 10 + tile_terrain(ptile)->defense_bonus / 10;
  extras_bonus += tile_extras_defense_bonus(ptile, unit_type_get(punit));
  db += (db * extras_bonus) / 100;
  d = adv_unit_def_rating_basic_squared(punit) * db;

  adjc_iterate(&(wld.map), (struct tile *) ptile, ptile1) {
    if (has_handicap(pplayer, H_FOG)
        && !map_is_known_and_seen(ptile1, pplayer, V_MAIN)) {
      continue;                 /* Cannot see it: assume no danger. */
    }
    unit_list_iterate(ptile1->units, enemy) {
      if (pplayers_at_war(unit_owner(enemy), pplayer)
          && (unit_attack_unit_at_tile_result(enemy, NULL, punit,
                                              (struct tile *) ptile)
              == ATT_OK)
          && (unit_attack_units_at_tile_result(enemy, NULL,
                                               (struct tile *) ptile)
              == ATT_OK)) {
        a += adv_unit_att_rating(enemy);
      }
    } unit_list_iterate_end;
  } adjc_iterate_end;

  return (double) (a * a * 10) >= (double) d * tolerance;
}

/**********************************************************************//**
  pf_parameter is_pos_dangerous callback for civilians: never plan to
  end a turn where visible enemies likely kill us -- "likely" priced
  by settler_risk_tolerance (human doctrine 2026-08-01: some loss
  chance is affordable on the way to a very good site; the hard
  any-threat veto was too absolute). Milder danger stays priced into
  the path by fearfulness rather than forbidden.
**************************************************************************/
static bool deep_civilian_pos_dangerous(const struct tile *ptile,
                                        enum known_type known,
                                        const struct pf_parameter *param)
{
  const struct deep_settler_risk *risk = param->data;
  double tolerance = deep_tuning_get()->settler_risk_tolerance;

  if (ptile == param->start_tile) {
    /* "Never plan to end a turn where enemies could kill us" cannot
     * apply to the tile we already stand on -- pathing away is the
     * point, and the flee pass owns the urgency. Also load-bearing:
     * pf danger maps mishandle a dangerous START (the segment walk
     * excludes it, and construct_path reads past the empty segment
     * -- the task #22 assertion; recorded in upstream-bugs.md). */
    return FALSE;
  }

  if (tolerance > 0.0) {
    return deep_settler_overnight_veto(risk->punit, ptile, tolerance);
  }
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
  Whether any route to dest exists under 'parameter'.
**************************************************************************/
static bool deep_settler_route_exists(struct pf_parameter *parameter,
                                      struct tile *dest)
{
  struct pf_map *pfm = pf_map_new(parameter);
  bool found = (pf_map_move_cost(pfm, dest) != PF_IMPOSSIBLE_MC);

  pf_map_destroy(pfm);

  return found;
}

/**********************************************************************//**
  Can this settler walk to dest at all?

  goto_is_sane() answers a nearby question with the wrong knowledge: it
  pathfinds NON-omnisciently, so every unexplored tile counts as
  passable and a target across unmapped water reports a land route that
  does not exist. Deep's site search and its mover both pathfind with
  the player's real handicaps, and this test does too, so all three
  agree about which trips need a boat.

  Measured cost of the disagreement (findings/settler-missions.md):
  5656 of 6323 stalled settler mission-turns on lategame-80 were
  missions the search itself had marked overseas, being walked to.
**************************************************************************/
static bool deep_settler_can_walk_there(struct unit *punit,
                                        struct tile *dest)
{
  struct pf_parameter parameter;
  const struct civ_map *nmap = &(wld.map);

  if (same_pos(unit_tile(punit), dest)) {
    return TRUE;
  }

  pft_fill_unit_parameter(&parameter, nmap, punit);
  parameter.omniscience = !has_handicap(unit_owner(punit), H_MAP);

  return deep_settler_route_exists(&parameter, dest);
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
                                          struct tile *dest_tile,
                                          bool can_walk)
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
  } else if (can_walk) {
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
  bool can_walk;

  CHECK_UNIT(punit);

  if (same_pos(dest_tile, unit_tile(punit)) || punit->moves_left <= 0) {
    /* Nowhere to go */
    return TRUE;
  }

  can_walk = deep_settler_can_walk_there(punit, dest_tile);

  /* See if we need a bodyguard for the trip */
  bg_needed = deep_settler_bodyguard_needed(ait, punit, dest_tile, can_walk);

  if (unit_transported(punit) || !can_walk) {
    /* Must go by boat, call an aiferryboat function */
    if (!aiferry_gobyboat(ait, pplayer, punit, dest_tile, bg_needed)) {
      return FALSE;
    }
    /* Landing changes what is walkable. */
    can_walk = deep_settler_can_walk_there(punit, dest_tile);
  }

  /* Go where we should be going if we can, and are at our destination
   * if we are on a ferry */
  if (can_walk && punit->moves_left > 0) {
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
  Turns the risk pathfinder -- the SAME parameter the site search plans
  with -- needs to reach dest from where the settler stands right now,
  or -1 if it cannot reach it at all. The first step of that route is
  returned in 'first_step' (NULL when there is no route, or when the
  route's next position is the tile we already occupy, which is pf's
  way of saying "wait here this turn").

  'dest_dangerous' reports the danger callback's verdict on the target
  tile itself, evaluated through the very parameter pf uses. No path
  may END a turn on a dangerous tile, so a target that answers TRUE
  here is one the mover can never reach however long it walks.

  'blocker' names the first constraint whose removal brings the route
  back, walking a ladder of relaxations from the mover's parameter out
  to the one goto_is_sane() itself uses:

    danger     the overnight veto (W05) has no acceptable stopping
               place along the way
    behaviour  non-allied territory, or a non-allied unit standing in
               the corridor
    vision     the route only exists if unknown tiles are assumed
               passable, which is what goto_is_sane() assumes and an
               omniscient mover does not
    attack     it exists only for a parameter willing to fight its way
               through, which a settler never is
    none       nothing routes there; goto_is_sane() is wrong for some
               other reason

  Only used for the stall diagnosis below, so it is built on demand:
  it costs one pf map per mission-turn and the caller gates it on the
  metric lines actually being written.
**************************************************************************/
static int deep_settler_risk_route(struct unit *punit, struct tile *dest,
                                   struct tile **first_step,
                                   bool *dest_dangerous, const char **blocker)
{
  struct pf_parameter parameter;
  struct deep_settler_risk risk;
  struct pf_map *pfm;
  struct pf_path *path;
  int turns = -1;
  const struct civ_map *nmap = &(wld.map);

  *first_step = NULL;
  *dest_dangerous = FALSE;
  *blocker = "-";
  if (same_pos(unit_tile(punit), dest)) {
    return 0;
  }

  pft_fill_unit_parameter(&parameter, nmap, punit);
  parameter.omniscience = !has_handicap(unit_owner(punit), H_MAP);
  deep_settler_apply_risk(&parameter, &risk, punit);
  if (parameter.is_pos_dangerous != NULL) {
    *dest_dangerous = parameter.is_pos_dangerous(dest, TILE_KNOWN_SEEN,
                                                 &parameter);
  }

  pfm = pf_map_new(&parameter);
  path = pf_map_path(pfm, dest);
  if (path != NULL) {
    turns = pf_path_last_position(path)->turn;
    if (path->length > 1
        && !same_pos(path->positions[1].tile, unit_tile(punit))) {
      *first_step = path->positions[1].tile;
    }
    pf_path_destroy(path);
  }
  pf_map_destroy(pfm);

  if (turns < 0) {
    /* Relax one constraint at a time; the first relaxation that brings
     * the route back is the one refusing it. */
    parameter.is_pos_dangerous = NULL;
    if (deep_settler_route_exists(&parameter, dest)) {
      *blocker = "danger";
      return turns;
    }
    parameter.get_TB = NULL;
    if (deep_settler_route_exists(&parameter, dest)) {
      *blocker = "behaviour";
      return turns;
    }
    parameter.omniscience = FALSE;
    if (deep_settler_route_exists(&parameter, dest)) {
      *blocker = "vision";
      return turns;
    }
    pft_fill_unit_attack_param(&parameter, nmap, punit);
    *blocker = (deep_settler_route_exists(&parameter, dest)
                ? "attack" : "none");
  }

  return turns;
}

/**********************************************************************//**
  Where a settler stands in the ferry protocol, as one loggable digit:

    0  not on a boat and holding none
    1  not on a boat, asked for one (unit_ai.ferryboat == FERRY_WANTED)
    2  not on a boat, holding a specific one
    3  aboard, but another passenger commands the boat
    4  aboard and in charge of the boat

  The pipeline is two independent searches -- the passenger raises a
  flag and sets done, the boat looks for flagged passengers -- so the
  states it can sit in are the only evidence of which half is losing
  the settler. FERRY_WANTED is private to daiferry.c but it is the
  documented "unit ids or special indicator values (<= 0)" convention
  of unit_ai, and only the sign is read here.
**************************************************************************/
static int deep_settler_lift_state(struct ai_type *ait, struct unit *punit)
{
  int boat = def_ai_unit_data(punit, ait)->ferryboat;

  if (unit_transported(punit)) {
    return is_boss_of_boat(ait, punit) ? 4 : 3;
  }
  if (boat > 0) {
    return 2;
  }

  return boat < 0 ? 1 : 0;
}

/**********************************************************************//**
  Distance to the nearest of our own boats that would take this settler
  (is_boat_free()), and to the nearest of our own boats at all.
  aiferry_find_boat_nearby() only looks one tile out, so a settler that
  reports a free boat five tiles away is one the boat half of the
  protocol was supposed to come and collect.
**************************************************************************/
static void deep_settler_nearest_boats(struct ai_type *ait,
                                       struct unit *punit,
                                       int *any, int *free_one)
{
  struct player *pplayer = unit_owner(punit);

  *any = -1;
  *free_one = -1;
  unit_list_iterate(pplayer->units, pferry) {
    if (dai_is_ferry(pferry, ait)) {
      int d = real_map_distance(unit_tile(punit), unit_tile(pferry));

      if (*any < 0 || d < *any) {
        *any = d;
      }
      if (is_boat_free(ait, pferry, punit, 1)
          && (*free_one < 0 || d < *free_one)) {
        *free_one = d;
      }
    }
  } unit_list_iterate_end;
}

/**********************************************************************//**
  Distance from the settler to the boat it holds, or -1 if it holds
  none or the boat has died. A boat that never closes this distance is
  a different defect from one that was never assigned.
**************************************************************************/
static int deep_settler_boat_distance(struct ai_type *ait, struct unit *punit)
{
  int boat = def_ai_unit_data(punit, ait)->ferryboat;
  struct unit *pferry = (boat > 0 ? game_unit_by_number(boat) : NULL);

  if (pferry == NULL) {
    return -1;
  }

  return real_map_distance(unit_tile(punit), unit_tile(pferry));
}

/**********************************************************************//**
  Who claims the target tile, as one loggable digit: 0 nobody, 1 us,
  2 an ally, 3 someone else. Borders move under a walking settler, and
  deep_settler_tile_behavior() refuses to path into a tile the third
  case describes, so this says whether a route died because the target
  itself was claimed while the settler was on its way.
**************************************************************************/
static int deep_settler_tile_claim(const struct player *pplayer,
                                   const struct tile *ptile)
{
  const struct player *owner = tile_owner(ptile);

  if (owner == NULL) {
    return 0;
  }
  if (owner == pplayer) {
    return 1;
  }
  return pplayers_allied(owner, pplayer) ? 2 : 3;
}

/**********************************************************************//**
  Why a settler holding a city mission did not reach its target this
  turn.

  The mission-continuation block only ever knew "did not arrive", and
  half of all mission-turns on a crowded cell are a settler standing on
  the tile it stood on last turn (findings/settler-missions.md). Search
  and execution share deep_settler_apply_risk(), so they should agree
  about what is walkable; that they visibly do not is the RC5 case, and
  dropping the mission on the strength of a guess cost a benchmark cell
  (6ab78f8d98, reverted). These names say which side refused.
**************************************************************************/
static const char *deep_settler_stall_reason(const struct unit *punit,
                                             const struct tile *from,
                                             const struct tile *dest,
                                             bool can_walk, int moves_before,
                                             int risk_turns,
                                             const struct tile *first_step)
{
  if (same_pos(unit_tile(punit), dest)) {
    return "arrived";
  }
  if (!same_pos(unit_tile(punit), from)) {
    return "moved";               /* Progress, just not there yet. */
  }
  if (moves_before <= 0) {
    return "nomoves";             /* Spent before the mission was run. */
  }
  if (!can_walk) {
    return "ferry";               /* No land route at all: needs a boat. */
  }
  if (risk_turns < 0) {
    return "nopath";              /* A land route exists; risk vetoes it. */
  }
  if (first_step == NULL) {
    return "wait";                /* pf wants us to hold for movement. */
  }
  return "blocked";               /* A step was planned and not taken. */
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
struct cityresult *deep_find_best_city_placement(struct ai_type *ait,
                                                        struct unit *punit,
                                                        bool look_for_boat,
                                                        bool use_virt_boat)
{
  struct pf_parameter parameter;
  struct deep_settler_risk risk;
  struct player *pplayer = unit_owner(punit);
  struct unit *ferry = NULL;
  struct cityresult *cr1 = NULL, *cr2 = NULL;
  int ferry_wait = 0;
  const struct civ_map *nmap = &(wld.map);

  fc_assert_ret_val(is_ai(pplayer), NULL);
  /* Only virtual units may use virtual boats: */
  fc_assert_ret_val(0 == punit->id || !use_virt_boat, NULL);

  /* Phase 1: Consider building cities on our continent */

  pft_fill_unit_parameter(&parameter, nmap, punit);
  parameter.omniscience = !has_handicap(pplayer, H_MAP);
  deep_settler_apply_risk(&parameter, &risk, punit);
  cr1 = deep_settler_map_iterate(ait, &parameter, punit, 0);

  if (cr1 && cr1->result > RESULT_IS_ENOUGH) {
    /* Skip further searches */
    return cr1;
  }

  /* Phase 2: Consider travelling to another continent */

  if (look_for_boat) {
    const struct ai_plr *plr_data = def_ai_player_data(pplayer, ait);

    if (plr_data->stats.passengers >= plr_data->stats.boats) {
      /* The ferry pipeline is already oversubscribed: every boat has
       * a waiting passenger. An overseas site the settler cannot be
       * DELIVERED to is worth nothing, and chasing one parks the
       * unit at a dock indefinitely (seed 205: five settlers on
       * cross-water missions at T135, two boats, half the home
       * island unfounded). Take the best on-continent site instead;
       * the cap is the ferry count itself, not a tunable.
       *
       * But record what the cap is COSTING (task #20): with zero
       * boats this branch reads 0 >= 0 and silences overseas demand
       * forever -- nobody wants a transport nobody has asked for,
       * so colonization never starts while 71% of the world sits
       * unowned (Europe T153). A demand-only probe with a virtual
       * boat scores the best overseas site and leaves the number
       * for the build side's colony-lift want; the mission itself
       * stays on land. */
      if (tile_city(unit_tile(punit)) != NULL
          && is_terrain_class_near_tile(nmap, unit_tile(punit),
                                        TC_OCEAN)) {
        struct unit_type *boattype
          = best_role_unit_for_player(pplayer, L_FERRYBOAT);

        if (boattype != NULL) {
          struct unit *probe
            = unit_virtual_create(pplayer, NULL, boattype, 0);
          struct cityresult *crd;

          unit_tile_set(probe, unit_tile(punit));
          pft_fill_unit_overlap_param(&parameter, nmap, probe);
          parameter.omniscience = !has_handicap(pplayer, H_MAP);
          parameter.get_TB = no_fights_or_unknown;
          crd = deep_settler_map_iterate(ait, &parameter, punit,
                                         unit_build_shield_cost_base(
                                           probe));
          unit_virtual_destroy(probe);
          if (crd != NULL) {
            if (cr1 == NULL || crd->result > cr1->result) {
              struct deep_plr *dp = deep_plr_data(pplayer);

              dp->colony_want = (int) crd->result;
              dp->colony_turn = game.info.turn;
              log_base(deep_metric_log_level(),
                       "deep-colony turn=%d player=\"%s\" demand=%d"
                       " at=(%d,%d)",
                       game.info.turn, player_name(pplayer),
                       dp->colony_want, TILE_XY(crd->tile));
            }
            cityresult_destroy(crd);
          }
        }
      }
      return cr1;
    }
    {
      struct pf_path *ferry_path = NULL;
      int ferry_id = aiferry_find_boat(ait, punit, 1, &ferry_path);

      ferry = game_unit_by_number(ferry_id);
      if (ferry != NULL && ferry_path != NULL) {
        /* Turns to reach the meeting point with the nearest available
         * boat -- a real pathfinding figure, not an estimate. The
         * overseas candidate is discounted by this wait below, so a
         * site the settler cannot be delivered to promptly loses to
         * on-continent work it could be doing in the meantime
         * (human design 2026-07-31). */
        ferry_wait = pf_path_last_position(ferry_path)->turn;
      }
      if (ferry_path != NULL) {
        pf_path_destroy(ferry_path);
      }
    }
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

    cr2 = deep_settler_map_iterate(ait, &parameter, punit,
                                   unit_build_shield_cost_base(ferry));
    if (cr2) {
      cr2->overseas = TRUE;
      cr2->virt_boat = (ferry->id == 0);
      if (ferry_wait > 0) {
        /* Price the boat-delivery delay: the search's own travel
         * costs start when the settler is aboard, but the queue is
         * real time too. amortize() is the project-standard
         * discounter; no tunable involved. */
        cr2->result = amortize(cr2->result, ferry_wait);
      }
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
  bool land_only = FALSE; /* Set when a boat-dependent mission was just
                           * dropped: this turn's re-search must not
                           * pick another undeliverable overseas site. */
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
    bool walk = deep_settler_can_walk_there(punit, ptile);

    log_base(deep_metric_log_level(),
             "deep-settler turn=%d player=\"%s\" unit=%d at=(%d,%d)"
             " mission target=(%d,%d) walk=%d boat=%d",
             game.info.turn, player_name(pplayer), punit->id,
             TILE_XY(unit_tile(punit)), TILE_XY(ptile),
             walk ? 1 : 0,
             def_ai_unit_data(punit, ait)->ferryboat);

    /* Check that the mission is still possible. If the tile has become
     * unavailable, call it off. */
    if (!city_can_be_built_here(nmap, ptile, punit, FALSE)) {
      dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);
      set_unit_activity(punit, ACTIVITY_IDLE, ACTION_NONE);
      send_unit_info(NULL, punit);

      return; /* Avoid recursion at all cost */
    } else {
      /* Go there */
      struct tile *from = unit_tile(punit);
      struct tile *first_step = NULL;
      bool dest_dangerous = FALSE;
      const char *blocker = "-";
      int lift_before = deep_settler_lift_state(ait, punit);
      int near_any = -1, near_free = -1;
      int psng_before = def_ai_player_data(pplayer, ait)->stats.passengers;
      int moves_before = punit->moves_left;
      /* The route the mover is about to plan, sampled before it moves.
       * One pf map per mission-turn, so it is skipped entirely when
       * the metric lines are not being written. */
      bool diagnose = log_do_output_for_level(deep_metric_log_level());
      int risk_turns = (diagnose
                        ? deep_settler_risk_route(punit, ptile, &first_step,
                                                  &dest_dangerous, &blocker)
                        : 0);
      bool got;

      if (diagnose) {
        deep_settler_nearest_boats(ait, punit, &near_any, &near_free);
      }
      got = deep_settler_gothere(ait, pplayer, punit, ptile);
      bool alive = (NULL != game_unit_by_number(sanity));

      if (diagnose) {
        log_base(deep_metric_log_level(),
                 "deep-settler turn=%d player=\"%s\" unit=%d at=(%d,%d)"
                 " stall target=(%d,%d) reason=%s walk=%d riskturns=%d"
                 " dangerdest=%d blocker=%s towner=%d"
                 " lift=%d/%d psng0=%d boatdist=%d nearboat=%d nearfree=%d coast=%d"
                 " boats=%d avail=%d psng=%d moves=%d/%d got=%d",
                 game.info.turn, player_name(pplayer), sanity,
                 TILE_XY(alive ? unit_tile(punit) : from), TILE_XY(ptile),
                 (alive
                  ? deep_settler_stall_reason(punit, from, ptile, walk,
                                              moves_before, risk_turns,
                                              first_step)
                  : "dead"),
                 walk ? 1 : 0, risk_turns, dest_dangerous ? 1 : 0,
                 blocker, deep_settler_tile_claim(pplayer, ptile),
                 lift_before,
                 alive ? deep_settler_lift_state(ait, punit) : 0,
                 psng_before,
                 alive ? deep_settler_boat_distance(ait, punit) : -1,
                 near_any, near_free,
                 (alive
                  && is_terrain_class_near_tile(nmap, unit_tile(punit),
                                                TC_OCEAN)) ? 1 : 0,
                 def_ai_player_data(pplayer, ait)->stats.boats,
                 def_ai_player_data(pplayer, ait)->stats.available_boats,
                 def_ai_player_data(pplayer, ait)->stats.passengers,
                 moves_before,
                 alive ? punit->moves_left : 0, got ? 1 : 0);
      }

      if ((!got && !alive) || punit->moves_left <= 0) {
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
        if (!goto_is_sane(punit, ptile) && !unit_transported(punit)
            && def_ai_unit_data(punit, ait)->ferryboat <= 0
            && !land_only) {
          /* Boat-dependent mission and no boat was assigned this
           * turn: release the mission and re-search LAND-ONLY this
           * same turn, so the settler founds something instead of
           * idling into an identical overseas pick tomorrow (the
           * first drop-only fix produced a 1200-event pick-drop
           * loop). Land-blocked missions (temporary ZoC jams) keep
           * stock's retry behaviour. */
          log_base(deep_metric_log_level(),
                   "deep-settler turn=%d player=\"%s\" unit=%d"
                   " drop-boatless target=(%d,%d)",
                   game.info.turn, player_name(pplayer), punit->id,
                   TILE_XY(ptile));
          dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);
          land_only = TRUE;
          goto FIND_WORK;
        }
        return;
      }
    }
  }

  /*** Try find some work ***/

FIND_WORK:

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
    result = deep_find_best_city_placement(ait, punit, !land_only, FALSE);
    TIMING_LOG(AIT_SETTLERS, TIMER_STOP);
    if (result && result->result > best_impr) {
      log_base(deep_metric_log_level(),
               "deep-settler turn=%d player=\"%s\" unit=%d at=(%d,%d)"
               " pick target=(%d,%d) result=%d overseas=%d virt=%d",
               game.info.turn, player_name(pplayer), punit->id,
               TILE_XY(unit_tile(punit)), TILE_XY(result->tile),
               (int) result->result, result->overseas ? 1 : 0,
               result->virt_boat ? 1 : 0);
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

/**********************************************************************//**
  Danger-aware worker task selection (human request 2026-07-30),
  registered on the advisors' inert task-value hook: candidates on
  exposed tiles -- unowned ground, or with an at-war military unit
  within worker_guard_radius -- are discounted by
  1 / (1 + worker_danger_discount), so interior improvements win ties
  and border work happens only when clearly more valuable (and then
  under the bodyguard-request cover). Returns the value unchanged for
  players not managed by deep, which keeps every other module,
  including the frozen baseline, bit-identical.
**************************************************************************/
adv_want deep_worker_task_value(const struct player *pplayer,
                                const struct unit *punit,
                                struct tile *ptile, adv_want value)
{
  const struct deep_tuning *tune = deep_tuning_get();
  bool exposed;

  if (pplayer->ai != deep_ai_get_self()
      || tune->worker_danger_discount <= 0.0 || value <= 0) {
    return value;
  }

  exposed = (tile_owner(ptile) != pplayer);
  if (!exposed) {
    players_iterate_alive(aplayer) {
      if (!pplayers_at_war((struct player *) pplayer, aplayer)) {
        continue;
      }
      unit_list_iterate(aplayer->units, e) {
        if (adv_unit_att_rating(e) > 0
            && real_map_distance(unit_tile(e), ptile)
               <= tune->worker_guard_radius) {
          exposed = TRUE;
          break;
        }
      } unit_list_iterate_end;
      if (exposed) {
        break;
      }
    } players_iterate_alive_end;
  }
  if (!exposed) {
    return value;
  }
  return value / (1.0 + tune->worker_danger_discount);
}
