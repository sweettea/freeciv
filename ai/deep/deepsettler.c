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
#include "unithand.h"
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

#include "deepdefence.h"
#include "deepodds.h"
#include "deepsite.h"
#include "deeptechcensus.h"
#include "deeptrunk.h"
#include "deeptuning.h"

#include "deepai.h"
#include "deepops.h"
#include "deepsettler.h"

/* Log-only fortress bookkeeping, defined with the base want far below
 * and called from the two places a worker is set to work. */
static void deep_fort_note_assignment(const struct player *pplayer,
                                      const struct tile *ptile);

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
bool deep_settler_overnight_veto(struct unit *punit,
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
        /* Same bar the mover uses, at its own tolerance: asking for an
         * escort and refusing to walk are the same judgement about the
         * same tiles, and one of them used to be unparameterised. */
        if (deep_settler_overnight_veto(punit, ppath->positions[i].tile,
                                        deep_tuning_get()
                                        ->settler_guard_tolerance)) {
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
  The nearest tile this settler can stand on from which a hull could
  carry it to 'dest': a SERVICEABLE BERTH.

  Serviceable is deep_water_serves_crossing() -- one body of water
  touching both the tile and the target -- so the answer is exact
  rather than a heuristic about "the sea". Distance is the settler's
  own risk pathfinder, the same parameter the site search plans with
  and the same one deep_settler_goto() will walk with, so a berth this
  returns is one the mover has already agreed it can reach and would
  not die reaching.

  pf yields positions in cost order, so the FIRST serviceable tile is
  the nearest one and the walk over the rest of the continent never
  happens. The map is bounded by the settler's own landmass in any
  case: a land parameter cannot cross the water it is looking for.

  Returns NULL when no berth on this continent serves this crossing --
  the honest answer that the target cannot be reached by hull from
  here, rather than a coast the founder would walk to for nothing.
  '*turns' gets the arrival turn, or -1.
**************************************************************************/
static struct tile *deep_settler_serviceable_berth(struct unit *punit,
                                                   const struct tile *dest,
                                                   int *turns)
{
  struct pf_parameter parameter;
  struct deep_settler_risk risk;
  struct pf_map *pfm;
  const struct civ_map *nmap = &(wld.map);
  struct tile *best = NULL;

  *turns = -1;

  pft_fill_unit_parameter(&parameter, nmap, punit);
  parameter.omniscience = !has_handicap(unit_owner(punit), H_MAP);
  deep_settler_apply_risk(&parameter, &risk, punit);
  pfm = pf_map_new(&parameter);

  pf_map_positions_iterate(pfm, pos, FALSE) {
    if (deep_water_serves_crossing(pos.tile, dest)) {
      best = pos.tile;
      *turns = pos.turn;
      break;
    }
  } pf_map_positions_iterate_end;

  pf_map_destroy(pfm);

  return best;
}

/**********************************************************************//**
  Where a founder that needs a lift is standing, and whether a hull
  could ever come for it there.

  EXPANSION slice 2 left this as its banked finding: the lift queue is
  GEOMETRY, not supply. 60 of 67 positive demand-supply gaps were
  founders queued beside water that closes on their own continent,
  while the bridging water that could have carried them sat one tile
  away holding 4-16 idle berths. Nothing in the founder's own path
  ever asked which water it was waiting on -- aiferry_gobyboat() tests
  is_terrain_class_near_tile(TC_OCEAN), under which a lake, a dead-end
  sea and the world ocean are one thing, and a founder that passes it
  requests a boat and stands still for the rest of the game.

  This is the demand side of that circle, counted before it is acted
  on. One line per founder-turn that needs a hull:

    coast     the founder touches water at all -- the test the stock
              ferry path stops at
    serves    that water reaches the target: the sharper test
    bridge    that water leads to SOME other continent (slice 2's
              build-side gate), so `bridge=1 serves=0` names a founder
              waiting on a sea that goes somewhere useless
    destsea   the target can be landed at by hull at all
    berth     the nearest tile that does serve this crossing, and the
              turns to walk there
    found     the passenger can found a city -- the ruleset's own
              answer (ACTION_FOUND_CITY), and the same predicate
              deep_founder_demand_on() counts the lift gap with, so
              this line and slice 2's gap line describe one population.
              A worker crossing to unimproved colony work reaches the
              same code (deep_idle_cross_water) and is counted here
              with found=0, because it is a different question with a
              different owner (worker_cross_continent_discount).

  Cost: one pf map per founder-turn that needs a boat, so it is built
  only when the metric log is on, and (from the routing knob) only for
  the founders that are not already at a serviceable berth.
**************************************************************************/
static void deep_settler_berth_census(struct player *pplayer,
                                      struct unit *punit,
                                      const struct tile *dest)
{
  const struct civ_map *nmap = &(wld.map);
  struct tile *berth;
  bool serves;
  int turns = -1;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  serves = deep_water_serves_crossing(unit_tile(punit), dest);
  berth = (serves ? unit_tile(punit)
           : deep_settler_serviceable_berth(punit, dest, &turns));
  if (serves) {
    turns = 0;
  }

  log_base(deep_metric_log_level(),
           "deep-berth turn=%d player=\"%s\" unit=%d at=(%d,%d)"
           " target=(%d,%d) coast=%d serves=%d bridge=%d destsea=%d"
           " berth=(%d,%d) turns=%d found=%d",
           game.info.turn, player_name(pplayer), punit->id,
           TILE_XY(unit_tile(punit)), TILE_XY(dest),
           is_terrain_class_near_tile(nmap, unit_tile(punit),
                                      TC_OCEAN) ? 1 : 0,
           serves ? 1 : 0,
           deep_city_water_bridges(unit_tile(punit)) ? 1 : 0,
           deep_city_water_bridges(dest) ? 1 : 0,
           TILE_XY(berth), turns,
           unit_can_do_action(punit, ACTION_FOUND_CITY) ? 1 : 0);
}

/**********************************************************************//**
  Send a founder that is waiting on the wrong water to a coast a hull
  could fetch it from.

  The population is the one the census above reports and slice 2
  measured from the supply side: `coast=1 serves=0`, a founder that
  satisfied the ferry path's "am I beside water" test beside a sea
  that does not reach its target. It asks for a boat, no boat can come,
  and the drop-boatless clause deliberately spares a unit holding
  FERRY_WANTED -- so it stands there, and the wait it accumulates is
  what damps the founder want that would have commissioned the hull.

  Three deliberate restrictions:

  * Only a FOUNDER, by the ruleset's own ACTION_FOUND_CITY, which is
    the population deep_founder_demand_on() counts the lift gap with
    and therefore the only one this slice has evidence about. A worker
    crossing to colony work reaches the same code and is left alone;
    the census still counts it (found=0).
  * Only where the founder ALREADY touches water. An inland founder is
    not stranded: aiferry_gobyboat() walks it toward a real hull, and
    if there is none the mission is dropped and re-searched land-only
    the same turn. The defect is exactly the tile at which that walk
    stops.
  * The berth must be one this settler's own risk pathfinder can
    reach; deep_settler_serviceable_berth() returns NULL otherwise and
    nothing moves.
  * The caller STILL calls aiferry_gobyboat() afterwards, so the
    founder asks for a boat from wherever it ends the turn. Skipping
    the request would leave ferryboat at FERRY_NONE and hand the
    mission straight to the drop-boatless clause -- the walk would
    cancel the very mission it was serving.

  Returns FALSE iff the settler died on the way.
**************************************************************************/
static bool deep_settler_berth_route(struct ai_type *ait,
                                     struct player *pplayer,
                                     struct unit *punit,
                                     const struct tile *dest)
{
  const struct civ_map *nmap = &(wld.map);
  struct tile *berth;
  int turns = -1;

  if (deep_tuning_get()->founder_berth_route <= 0
      || !unit_can_do_action(punit, ACTION_FOUND_CITY)
      || !is_terrain_class_near_tile(nmap, unit_tile(punit), TC_OCEAN)
      || deep_water_serves_crossing(unit_tile(punit), dest)) {
    return TRUE;
  }

  berth = deep_settler_serviceable_berth(punit, dest, &turns);
  if (berth == NULL || same_pos(berth, unit_tile(punit))) {
    return TRUE;
  }

  log_base(deep_metric_log_level(),
           "deep-berthroute turn=%d player=\"%s\" unit=%d at=(%d,%d)"
           " target=(%d,%d) berth=(%d,%d) dist=%d turns=%d",
           game.info.turn, player_name(pplayer), punit->id,
           TILE_XY(unit_tile(punit)), TILE_XY(dest), TILE_XY(berth),
           real_map_distance(unit_tile(punit), berth), turns);

  return deep_settler_goto(ait, punit, berth);
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
    if (!unit_transported(punit)) {
      /* Standing on its own legs and needing a hull: the population
       * slice 2's demand-supply gap is made of. */
      deep_settler_berth_census(pplayer, punit, dest_tile);
      if (!deep_settler_berth_route(ait, pplayer, punit, dest_tile)) {
        return FALSE;           /* Died walking to the berth. */
      }
    }
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

  /* Stock's RESULT_IS_ENOUGH is a compile-time 250 that decides
   * whether colonisation is evaluated at all: above it, phase 2 never
   * runs and no overseas site is ever seen. Tunable here. */
  if (cr1 && cr1->result > deep_tuning_get()->site_enough_result) {
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

          deep_techcensus_mark(ait, pplayer, DEEP_TC_OTHER);
          unit_tech_reqs_iterate(boattype, padv) {
            plr_data->tech_want[advance_index(padv)]
              += FERRY_TECH_WANT;
            TECH_LOG(ait, LOG_DEBUG, pplayer, padv,
                     "+ %d for %s to ferry settler",
                     FERRY_TECH_WANT,
                     utype_rule_name(boattype));
          } unit_tech_reqs_iterate_end;
          deep_techcensus_mark(ait, pplayer, DEEP_TC_DEEP_SETTLER);
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
  Count what is still buildable inside our own city work radii -- the
  only ground worker_evaluate_improvements() ever enumerates.

  A worker goes idle when every enumerated tile scores a delta of zero,
  and that has two very different causes: the radii really are improved
  out, or there is legal work left that is declined because
  city_tile_value() prices it at today's city size (RC1). This
  separates them by asking only "could a road / an irrigation / a mine
  still be added here", with no value judgement at all.

  There is a THIRD cause, and this census could not see it until now:
  the work is real, and it is on another landmass. Both
  worker_evaluate_improvements() and the idle fallback build a plain
  land pf map, so a tile across water is enumerated and then silently
  discarded as unreachable (findings/idle-workers.md section 9). The
  counts are therefore split by continent: `open` is work this unit
  could walk to, `xopen` is work it could only reach by boat, and the
  gap between them is the size of what a worker cannot currently see.

  Continent number is the proxy for "needs a boat", not a pf run: it is
  what assign_continent_numbers() maintains, and it is free. It counts
  a same-continent tile that is blocked by terrain as reachable, which
  makes `xopen` a LOWER bound on unreachable work.

  Read-only; no pathfinding. Called only for a unit that has just
  concluded it has nothing to do, and only when the metric lines are
  being written.
**************************************************************************/
static void deep_idle_work_census(const struct civ_map *nmap,
                                  struct player *pplayer,
                                  struct unit *punit,
                                  int *tiles, int *road, int *irrig,
                                  int *mine, int *open,
                                  int *xtiles, int *xopen, int *xcities)
{
  Continent_id here = tile_continent(unit_tile(punit));

  *tiles = *road = *irrig = *mine = *open = 0;
  *xtiles = *xopen = *xcities = 0;

  city_list_iterate(pplayer->cities, pcity) {
    struct tile *pcenter = city_tile(pcity);
    bool overseas = (tile_continent(pcenter) != here);

    if (overseas) {
      (*xcities)++;
    }

    city_tile_iterate_index(nmap, city_map_radius_sq_get(pcity), pcenter,
                            ptile, cindex) {
      bool any = FALSE;

      if (!city_can_work_tile(pcity, ptile)) {
        continue;
      }
      if (overseas) {
        (*xtiles)++;
      } else {
        (*tiles)++;
      }

      if (next_extra_for_tile(ptile, EC_ROAD, pplayer, punit) != NULL) {
        if (!overseas) {
          (*road)++;
        }
        any = TRUE;
      }
      if (next_extra_for_tile(ptile, EC_IRRIGATION, pplayer,
                              punit) != NULL) {
        if (!overseas) {
          (*irrig)++;
        }
        any = TRUE;
      }
      if (next_extra_for_tile(ptile, EC_MINE, pplayer, punit) != NULL) {
        if (!overseas) {
          (*mine)++;
        }
        any = TRUE;
      }
      if (any) {
        if (overseas) {
          (*xopen)++;
        } else {
          (*open)++;
        }
      }
    } city_tile_iterate_index_end;
  } city_list_iterate_end;
}

/**********************************************************************//**
  Last resort for a worker that found nothing worth doing: build the
  soonest-finished legal improvement inside our own city radii.

  worker_evaluate_improvements() scores a tile by how much the
  improvement raises its value TO THAT CITY AT TODAY'S SIZE
  (city_tile_value(pcity, ptile, 0, 0)), floored at zero. A tile a
  size-12 city would work is worth nothing to the size-5 city that owns
  it, so the delta is zero and no work is ordered -- RC1 at the worker
  layer. The unit then does nothing at all, because
  auto_workers_player() sends AI players to the settler_run hook rather
  than to auto_worker_findwork(), and so never reaches the
  adv_seek_shelter() a human auto-worker would get.

  Measured on lategame-80, 10 games, 12273 unit-turns that concluded
  "nothing to do": their own radii held 99.4 workable tiles of which
  34.9 still admitted a road, an irrigation or a mine, and only 11% of
  those unit-turns had nothing improvable anywhere
  (findings/idle-workers.md).

  This does not re-price anything and cannot outrank a real want: it
  runs ONLY where the unit was about to stand still. Candidates are
  ranked by soonest completion -- travel turns plus work turns -- which
  is an ordering, not a threshold, so there is no constant to invent
  (rule 7). Tiles another worker occupies or is already travelling to
  are skipped, and adv_worker_safe_tile() is respected.

  Returns TRUE if work was assigned.
**************************************************************************/
/* One candidate improvement: where, what, and how soon it would be
 * finished counting travel. */
struct deep_idle_pick {
  struct tile *tile;
  struct extra_type *target;
  enum unit_activity act;
  int turns;
};

/**********************************************************************//**
  Score every improvable tile reachable on 'pfm' and keep the soonest,
  writing into 'best' only when a candidate beats what is already there.

  Shared by the fallback's two passes. 'overseas' selects which cities
  are considered: FALSE for those on the unit's own continent, TRUE for
  the ones it could only reach by boat. 'extra_turns' is added to every
  candidate (the wait for a lift) and 'price' multiplies the total (the
  cross-continent discount), so a boat trip competes with work at home
  on one comparable number.
**************************************************************************/
static void deep_idle_scan(const struct civ_map *nmap,
                           struct player *pplayer, struct unit *punit,
                           struct pf_map *pfm, bool omniscience,
                           Continent_id home, bool overseas,
                           int extra_turns, double price,
                           struct deep_idle_pick *best)
{
  struct pf_position pos;

  city_list_iterate(pplayer->cities, pcity) {
    struct tile *pcenter = city_tile(pcity);

    if ((tile_continent(pcenter) != home) != overseas) {
      continue;
    }

    city_tile_iterate_index(nmap, city_map_radius_sq_get(pcity), pcenter,
                            ptile, cindex) {
      bool taken = FALSE;

      if (!city_can_work_tile(pcity, ptile)
          || !adv_worker_safe_tile(nmap, pplayer, punit, ptile)) {
        continue;
      }

      /* Do not pile onto a tile another of our workers already holds or
       * is walking to; the idle pool is large and would converge. */
      unit_list_iterate(ptile->units, aunit) {
        if (unit_owner(aunit) == pplayer && aunit->id != punit->id
            && unit_has_type_flag(aunit, UTYF_WORKERS)) {
          taken = TRUE;
          break;
        }
      } unit_list_iterate_end;
      if (taken) {
        /* struct workermap is opaque outside the advisors, so the
         * enroute reservation cannot be read here; the on-tile check
         * plus auto_worker_setup_work()'s own displacement handling is
         * what keeps the idle pool from converging on one tile. */
        continue;
      }

      if (!pf_map_position(pfm, ptile, &pos)) {
        continue;
      }

      aw_extra_action_iterate(act) {
        enum unit_activity activity = action_id_get_activity(act);
        enum extra_cause cause = activity_to_extra_cause(activity);
        struct extra_type *target;
        int turns;

        /* Roads, irrigation and mines only. aw_extra_action_iterate()
         * also offers bases, and an idle pool this large would carpet
         * the map with fortresses; the point here is the terrain
         * economy, not fortification. */
        if (cause != EC_ROAD && cause != EC_IRRIGATION
            && cause != EC_MINE) {
          continue;
        }
        target = next_extra_for_tile(ptile, cause, pplayer, punit);
        if (target == NULL) {
          continue;
        }
        if (!action_prob_possible(
              action_speculate_unit_on_tile(nmap, act, punit,
                                            unit_home(punit), ptile,
                                            omniscience,
                                            ptile, target))) {
          continue;
        }

        turns = pos.turn + get_turns_for_activity_at(punit, activity,
                                                     ptile, target);
        if (pos.moves_left == 0) {
          turns++;
        }
        turns = (int) ((turns + extra_turns) * price);
        if (turns < best->turns) {
          best->turns = turns;
          best->tile = ptile;
          best->target = target;
          best->act = activity;
        }
      } aw_extra_action_iterate_end;
    } city_tile_iterate_index_end;
  } city_list_iterate_end;
}

/**********************************************************************//**
  Second pass of the fallback: work on another landmass, reached by
  boat.

  Workers cannot cross water at all today. The land pf map enumerates an
  overseas city's radius and then discards every tile of it as
  unreachable, and there is no ferry leg anywhere on the worker path --
  every aiferry_* call in this file sits on the BUILD_CITY branch. A
  colony is therefore improved only by whatever founders happen to land
  there (findings/idle-workers.md section 9): on lategame-80, 12% of
  cities have no worker-capable unit within 8 tiles and the tail runs
  to 24.

  Seeing it uses the machinery the site search already uses for exactly
  this question -- a real ferry's overlap map, which prices the sea leg
  and lets the last step fall on a beach. Reaching it uses the settler's
  own ferry-enabled goto.

  This stays BACKUP behaviour by two separate guards, because a worker
  that takes a boat a settler wanted trades a colony for a road:

  - the 1:1 ferry cap the site search enforces (a waiting passenger per
    boat) is checked here too, so workers only bid on genuinely spare
    lift; and
  - the trip is priced by worker_cross_continent_discount on top of its
    real travel turns, so it must beat the best walkable candidate by
    that factor to win.

  Returns TRUE if the pick in 'best' is now a cross-water one.
**************************************************************************/
static bool deep_idle_cross_water(struct ai_type *ait,
                                  const struct civ_map *nmap,
                                  struct player *pplayer,
                                  struct unit *punit, Continent_id home,
                                  struct deep_idle_pick *best)
{
  const struct ai_plr *plr_data = def_ai_player_data(pplayer, ait);
  struct pf_parameter parameter;
  struct pf_path *ferry_path = NULL;
  struct pf_map *pfm;
  struct unit *ferry;
  struct tile *was = best->tile;
  int ferry_wait = 0;

#define CROSS_DECLINE(reason)                                           \
  do {                                                                  \
    log_base(deep_metric_log_level(),                                   \
             "deep-cross turn=%d player=\"%s\" unit=%d at=(%d,%d)"       \
             " why=%s land=%d boats=%d avail=%d psng=%d",                \
             game.info.turn, player_name(pplayer), punit->id,            \
             TILE_XY(unit_tile(punit)), (reason),                        \
             best->turns, plr_data->stats.boats,                         \
             plr_data->stats.available_boats,                            \
             plr_data->stats.passengers);                                \
    return FALSE;                                                       \
  } while (FALSE)

  if (plr_data->stats.passengers >= plr_data->stats.boats) {
    /* Every boat already has somebody waiting for it. The settler
     * pipeline owns that queue; a worker must not join it. */
    CROSS_DECLINE("cap");
  }

  /* A real boat only. The virtual-boat trick exists so the site search
   * can express DEMAND for a ferry that does not exist yet; a worker
   * asking for one would bid shields away from colonisation, which is
   * the opposite of backup behaviour. */
  ferry = game_unit_by_number(aiferry_find_boat(ait, punit, 1,
                                                &ferry_path));
  if (ferry_path != NULL) {
    /* Real turns to the rendezvous, not an estimate -- the same figure
     * the site search discounts an overseas site by. */
    ferry_wait = pf_path_last_position(ferry_path)->turn;
    pf_path_destroy(ferry_path);
  }
  if (ferry == NULL) {
    CROSS_DECLINE("noboat");
  }

  pft_fill_unit_overlap_param(&parameter, nmap, ferry);
  parameter.omniscience = !has_handicap(pplayer, H_MAP);
  parameter.get_TB = no_fights_or_unknown;
  pfm = pf_map_new(&parameter);

  deep_idle_scan(nmap, pplayer, punit, pfm, parameter.omniscience, home,
                 TRUE, ferry_wait,
                 1.0 + deep_tuning_get()->worker_cross_continent_discount,
                 best);

  pf_map_destroy(pfm);

  if (best->tile == was) {
    /* Told apart because they mean opposite things: "nocand" is an
     * empire with no reachable overseas work to do, "outbid" is work
     * that exists and lost to something walkable. Only the second is
     * the discount's doing. */
    CROSS_DECLINE(was == NULL ? "nocand" : "outbid");
  }

  return TRUE;
#undef CROSS_DECLINE
}

/**********************************************************************//**
  Last resort for a worker that found nothing worth doing.
**************************************************************************/
static bool deep_idle_fallback_work(struct ai_type *ait,
                                    const struct civ_map *nmap,
                                    struct player *pplayer,
                                    struct unit *punit,
                                    struct workermap *state)
{
  struct pf_parameter parameter;
  struct pf_map *pfm;
  struct deep_idle_pick best = { NULL, NULL, ACTIVITY_IDLE, FC_INFINITY };
  Continent_id home = tile_continent(unit_tile(punit));
  struct tile *best_tile;
  struct extra_type *best_target;
  enum unit_activity best_act;
  int best_turns;
  struct pf_path *path = NULL;
  bool cross;
  bool assigned = FALSE;

  if (!unit_has_type_flag(punit, UTYF_WORKERS)) {
    return FALSE;
  }

  pft_fill_unit_parameter(&parameter, nmap, punit);
  parameter.omniscience = !has_handicap(pplayer, H_MAP);
  /* autoworker_tile_behavior() is static in the advisors; deep's own
   * settler behaviour is the same worker rule plus a refusal to path
   * onto non-allied units, which is what we want here anyway. */
  parameter.get_TB = deep_settler_tile_behavior;
  pfm = pf_map_new(&parameter);

  deep_idle_scan(nmap, pplayer, punit, pfm, parameter.omniscience, home,
                 FALSE, 0, 1.0, &best);

  /* Only now, against whatever walking would achieve. */
  cross = deep_idle_cross_water(ait, nmap, pplayer, punit, home, &best);

  best_tile = best.tile;
  best_target = best.target;
  best_act = best.act;
  best_turns = best.turns;

  if (cross) {
    log_base(deep_metric_log_level(),
             "deep-idlework turn=%d player=\"%s\" unit=%d type=\"%s\""
             " at=(%d,%d) act=%d target=\"%s\" tile=(%d,%d) turns=%d"
             " cross=1",
             game.info.turn, player_name(pplayer), punit->id,
             unit_rule_name(punit), TILE_XY(unit_tile(punit)),
             (int) best_act, extra_rule_name(best_target),
             TILE_XY(best_tile), best_turns);

    adv_unit_new_task(punit, AUT_AUTO_WORKER, best_tile);
    punit->goto_tile = best_tile;
    /* The settler's ferry-enabled goto: it books the lift, waits for it
     * where waiting is right, and walks the far side after landing.
     * Whether it arrives this turn or in ten, the unit now has a
     * mission, which is the thing it did not have before. */
    (void) deep_settler_gothere(ait, pplayer, punit, best_tile);
    pf_map_destroy(pfm);

    return TRUE;
  }

  if (best_tile != NULL) {
    log_base(deep_metric_log_level(),
             "deep-idlework turn=%d player=\"%s\" unit=%d type=\"%s\""
             " at=(%d,%d) act=%d target=\"%s\" tile=(%d,%d) turns=%d",
             game.info.turn, player_name(pplayer), punit->id,
             unit_rule_name(punit), TILE_XY(unit_tile(punit)),
             (int) best_act, extra_rule_name(best_target),
             TILE_XY(best_tile), best_turns);

    path = pf_map_path(pfm, best_tile);
    adv_unit_new_task(punit, AUT_AUTO_WORKER, best_tile);
    assigned = auto_worker_setup_work(nmap, pplayer, punit, state, 0,
                                      &path, best_tile, best_act,
                                      &best_target, best_turns);
    if (assigned && best_act == ACTIVITY_BASE) {
      deep_fort_note_assignment(pplayer, best_tile);
    }
    if (path != NULL) {
      pf_path_destroy(path);
    }
  }

  pf_map_destroy(pfm);

  return assigned;
}

/**********************************************************************//**
  Hand a founder back to a city as population.

  A settler that is not going to found is worth more as the two
  citizens it cost than as a free worker: civ2civ3 Settlers carry
  AddToCity with pop_cost 2, and the Join City enabler asks only for a
  domestic target and one move fragment. No AI in the tree ever
  performs the action -- ACTRES_JOIN_CITY appears three times across
  ai/ and every one is a classification switch, never an actor
  (upstream-bugs.md #31).

  Measured motivation: 13.4 founders per game stand IDLE at the end of
  a lategame-80 game, 80% of all surviving founders, and giving them
  terrain work instead moved no output at all
  (findings/idle-workers.md). Human call 2026-08-03: a settler doing
  worker duty should join a city instead.

  Targets are own cities that can still grow by this unit's pop value
  -- city_can_grow_to() keeps us from pushing citizens into a city the
  ruleset will not let grow -- ranked by soonest arrival, which is an
  ordering rather than a threshold, so no constant is invented.

  Returns TRUE if the unit joined, or is under way to do so.
**************************************************************************/
static bool deep_settler_join_city(struct ai_type *ait,
                                   const struct civ_map *nmap,
                                   struct player *pplayer,
                                   struct unit *punit)
{
  struct pf_parameter parameter;
  struct deep_settler_risk risk;
  struct pf_map *pfm;
  struct pf_position pos;
  struct city *best = NULL;
  int best_turns = FC_INFINITY;
  int pop = unit_pop_value(punit);

  if (!unit_is_cityfounder(punit) || pop <= 0) {
    return FALSE;
  }

  /* Keep a pool. A founder in hand is the only way to take a site the
   * moment one opens, and joining is irreversible: the first cut of
   * this dissolved settlers down to 0.32 per player and cost 2800 land
   * area at t=-2.35 (findings/idle-workers.md 7). So dissolve only
   * what is ABOVE the target pool, and prefer to spend a founder that
   * a city is paying upkeep on -- a homeless one is free to keep and
   * free to settle with, which is the bias the human asked for
   * (2026-08-03). */
  {
    int founders = 0, homed = 0;

    unit_list_iterate(pplayer->units, aunit) {
      if (unit_is_cityfounder(aunit)) {
        founders++;
        if (aunit->homecity != 0) {
          homed++;
        }
      }
    } unit_list_iterate_end;

    /* Size the pool by SITES REMAINING (human call 2026-08-03). A
     * settler in hand is an option on a site, so the inventory worth
     * holding is set by how many sites are left, not by how many
     * cities we already have.
     *
     * Keying off city count got this backwards on the roomy cell:
     * 23 cities drove the target to ~3.6 while ~10 settlers were still
     * usefully expanding into 400 land/player, so the gate never held
     * and joining dissolved working expanders. deep_remaining_sites()
     * is free land divided by our own land-per-city, so it is large on
     * an empty map and goes to zero as the world fills. */
    log_base(deep_metric_log_level(),
             "deep-pool turn=%d player=\"%s\" founders=%d homed=%d"
             " cities=%d freeshare=%.3f freetiles=%d reachtiles=%d"
             " sites=%.1f target=%.1f",
             game.info.turn, player_name(pplayer), founders, homed,
             city_list_size(pplayer->cities),
             deep_free_land_share(pplayer),
             deep_plr_data(pplayer)->free_land_tiles,
             deep_reachable_free_land(pplayer),
             deep_remaining_sites(pplayer),
             deep_tuning_get()->founder_pool_per_site
             * deep_remaining_sites(pplayer));

    if (founders <= (int) (deep_tuning_get()->founder_pool_per_site
                           * deep_remaining_sites(pplayer))) {
      return FALSE;                 /* Pool is at or under target. */
    }
    if (punit->homecity == 0 && homed > 0
        && game.server.killunhomed <= 0) {
      /* Somebody else is costing a city upkeep; spend that one first
       * and leave this one free to settle.
       *
       * Only when the ruleset/settings let a homeless unit live:
       * killunhomed > 0 takes that percentage of a homeless unit's
       * hitpoints every turn until it dies (unittools.c:501), which
       * turns "free to keep" into "dying slowly". Under that setting
       * the homeless founder is the one to spend FIRST, so the bias
       * simply switches off rather than inverting on a guess. */
      return FALSE;
    }
  }

  pft_fill_unit_parameter(&parameter, nmap, punit);
  parameter.omniscience = !has_handicap(pplayer, H_MAP);
  deep_settler_apply_risk(&parameter, &risk, punit);
  pfm = pf_map_new(&parameter);

  city_list_iterate(pplayer->cities, pcity) {
    if (!city_can_grow_to(pcity, city_size_get(pcity) + pop)) {
      continue;
    }
    if (pf_map_position(pfm, city_tile(pcity), &pos)
        && pos.turn < best_turns) {
      best_turns = pos.turn;
      best = pcity;
    }
  } city_list_iterate_end;

  pf_map_destroy(pfm);

  if (best == NULL) {
    return FALSE;
  }

  if (same_pos(unit_tile(punit), city_tile(best))) {
    bool joined = FALSE;

    action_by_result_iterate(paction, ACTRES_JOIN_CITY) {
      if (action_prob_possible(action_prob_vs_city(nmap, punit,
                                                   action_id(paction),
                                                   best))) {
        if (unit_perform_action(pplayer, punit->id, best->id, 0, "",
                                action_id(paction), ACT_REQ_PLAYER)) {
          joined = TRUE;
          break;
        }
      }
    } action_by_result_iterate_end;

    log_base(deep_metric_log_level(),
             "deep-join turn=%d player=\"%s\" unit=%d city=%d size=%d"
             " pop=%d done=%d",
             game.info.turn, player_name(pplayer), punit->id, best->id,
             city_size_get(best), pop, joined ? 1 : 0);

    return joined;
  }

  log_base(deep_metric_log_level(),
           "deep-join turn=%d player=\"%s\" unit=%d city=%d size=%d"
           " pop=%d turns=%d walking=1",
           game.info.turn, player_name(pplayer), punit->id, best->id,
           city_size_get(best), pop, best_turns);

  punit->goto_tile = city_tile(best);

  return deep_settler_goto(ait, punit, city_tile(best));
}

/**********************************************************************//**
  Slice 1's travel discount, as a pure function of the gain: amortize()
  over worker_travel_discount turns per turn of walking. Extracted so
  the beyond-ceiling task source and the hook's
  deep_worker_travel_price() price a walk with the SAME shape and the
  SAME knob -- one discount, two callers -- rather than the source
  inventing a second one. Returns the gain unchanged when the knob is
  off, the walk is zero, or the gain is not positive (amortize()
  shrinks toward zero, so discounting a negative would flatter it).
**************************************************************************/
static adv_want deep_worker_travel_amortized(adv_want gain, int arrival)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (tune->worker_travel_discount <= 0.0 || arrival <= 0 || gain <= 0) {
    return gain;
  }

  return amortize(gain, (int) (tune->worker_travel_discount * arrival));
}

/**********************************************************************//**
  W12 slice 2: source worker tasks from the trunk corridor tiles the
  advisor's enumeration ceiling hides.

  worker_evaluate_improvements() offers exactly the tiles inside some
  own city's radius (autoworkers.c:586), so a corridor tile between two
  distant cities is never a candidate at all -- slice 1's want could
  re-rank the near-city end of a link but never dispatch a worker into
  the gap (STATUS "W12 slice 1" sections 2 and 7). This is the task
  SOURCE that ceiling calls for: deep's own scan over
  deep_trunk_beyond_tiles(), competing against the auction's champion
  on the auction's own scale.

  Pricing is parity with what the hook path would say if the advisor
  could see the tile. The want is deep_trunk_source_want() -- the same
  deep_trunk_price() the hook uses -- scaled by trunk_source_gain
  (0 = today's ceiling exactly, 1 = a beyond-ceiling corridor tile is
  worth what the same corridor is worth inside a radius), discounted
  for the walk by slice 1's own deep_worker_travel_amortized(), then
  amortized over completion turns exactly as the advisor amortizes its
  not-yet-worked champion (autoworkers.c:941; WORKER_FACTOR cancels
  through amortize(), which is a pure multiplication). No danger
  discount, deliberately: the hook adds the trunk want after the danger
  term rather than through it, and this prices the same want from the
  same reasoning. adv_worker_safe_tile() still refuses genuinely
  dangerous ground, and the on-tile worker check plus
  auto_worker_setup_work()'s displacement keep the source from piling
  onto one tile, the same two guards deep_idle_scan() relies on.

  Locality stays the arbiter: a distant corridor tile must survive both
  the walk discount and the completion amortize to outbid work at home,
  so a complete demand set is what this adds, not commuting.

  On a win, replaces the champion in *best_act/*best_target/*best_tile,
  rewrites *path from the source's own pf map, and returns the new
  value; otherwise returns best_impr untouched.
**************************************************************************/
static adv_want deep_trunk_source_task(const struct civ_map *nmap,
                                       struct player *pplayer,
                                       struct unit *punit,
                                       adv_want best_impr,
                                       enum unit_activity *best_act,
                                       struct extra_type **best_target,
                                       struct tile **best_tile,
                                       struct pf_path **path,
                                       int *completion_time)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct pf_parameter parameter;
  struct pf_map *pfm;
  struct pf_position pos;
  const int *beyond;
  int nbeyond, i;
  struct tile *pick_tile = NULL;
  struct extra_type *pick_target = NULL;
  enum unit_activity pick_act = ACTIVITY_LAST;
  adv_want pick_value = best_impr;
  int pick_dist = 0;

  if (tune->trunk_source_gain <= 0.0 || tune->trunk_road_gain <= 0.0) {
    return best_impr;
  }

  nbeyond = deep_trunk_beyond_tiles(pplayer, &beyond);
  if (nbeyond <= 0) {
    return best_impr;
  }

  pft_fill_unit_parameter(&parameter, nmap, punit);
  parameter.omniscience = !has_handicap(pplayer, H_MAP);
  parameter.get_TB = deep_settler_tile_behavior;
  pfm = pf_map_new(&parameter);

  for (i = 0; i < nbeyond; i++) {
    struct tile *ptile = index_to_tile(nmap, beyond[i]);
    bool taken = FALSE;

    if (ptile == NULL
        || !adv_worker_safe_tile(nmap, pplayer, punit, ptile)) {
      continue;
    }

    /* Same guard as deep_idle_scan(): the enroute reservation lives in
     * the advisors' opaque workermap, so the on-tile check here plus
     * auto_worker_setup_work()'s displacement is what keeps a pool of
     * workers from converging on one corridor tile. */
    unit_list_iterate(ptile->units, aunit) {
      if (unit_owner(aunit) == pplayer && aunit->id != punit->id
          && unit_has_type_flag(aunit, UTYF_WORKERS)) {
        taken = TRUE;
        break;
      }
    } unit_list_iterate_end;
    if (taken || !pf_map_position(pfm, ptile, &pos)) {
      continue;
    }

    aw_extra_action_iterate(act) {
      enum unit_activity activity = action_id_get_activity(act);
      struct extra_type *target;
      adv_want want, value;
      int arrival, delay;

      if (activity_to_extra_cause(activity) != EC_ROAD) {
        continue;
      }
      target = next_extra_for_tile(ptile, EC_ROAD, pplayer, punit);
      if (target == NULL
          || !action_prob_possible(
               action_speculate_unit_on_tile(nmap, act, punit,
                                             unit_home(punit), ptile,
                                             parameter.omniscience,
                                             ptile, target))) {
        continue;
      }

      want = deep_trunk_source_want(pplayer, target, ptile);
      if (want <= 0.0) {
        continue;
      }
      want *= tune->trunk_source_gain;

      arrival = pos.turn + (pos.moves_left == 0 ? 1 : 0);
      want = deep_worker_travel_amortized(want, arrival);

      delay = pos.turn + get_turns_for_activity_at(punit, activity,
                                                   ptile, target);
      if (pos.moves_left == 0) {
        delay++;
      }
      value = amortize(want, delay);

      if (value > pick_value) {
        pick_value = value;
        pick_tile = ptile;
        pick_target = target;
        pick_act = activity;
        pick_dist = real_map_distance(unit_tile(punit), ptile);
      }
    } aw_extra_action_iterate_end;
  }

  if (pick_tile == NULL) {
    pf_map_destroy(pfm);
    return best_impr;
  }

  if (*path != NULL) {
    pf_path_destroy(*path);
  }
  *path = pf_map_path(pfm, pick_tile);
  pf_map_destroy(pfm);
  if (*path == NULL) {
    /* Cannot happen for a tile pf_map_position() just priced, but a
     * champion without a path must not replace one with. */
    return best_impr;
  }

  *completion_time = pf_path_last_position(*path)->turn;
  *best_act = pick_act;
  *best_target = pick_target;
  *best_tile = pick_tile;

  deep_trunk_source_pick(pplayer, pick_dist);
  log_base(deep_metric_log_level(),
           "deep-trunkpick turn=%d player=\"%s\" unit=%d at=(%d,%d)"
           " tile=(%d,%d) dist=%d value=%.2f beat=" ADV_WANT_PRINTF,
           game.info.turn, player_name(pplayer), punit->id,
           TILE_XY(unit_tile(punit)), TILE_XY(pick_tile), pick_dist,
           (double) pick_value, best_impr);

  return pick_value;
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

  /* An operation's claim outranks worker autonomy, and THIS is the
   * entry point where that had to be said. deep_op_holds() is honoured
   * in deep_manage_units(), but auto_workers_player() is a second,
   * independent driver: it reaches every worker or cityfounder of an
   * AI player through the settler_run hook, gated on
   * `ssa_controller == SSA_AUTOWORKER || is_ai(pplayer)`
   * (autoworkers.c:1372). The `|| is_ai` is why the hold leaked --
   * for an AI player the agent flag is not even consulted.
   *
   * That is what was walking lift passengers off their boat in open
   * water: traced on isolated-150, a held settler steps onto adjacent
   * land with the hull mid-ocean and undamaged, repeatedly (unit 219
   * detached on turns 59, 67, 74 and 84 -- the pick-drop churn of
   * section 8). Suspending SSA_AUTOWORKER did not help precisely
   * because this gate ignores it, and the earlier diagnosis that
   * blamed the agent was reading a constant: SSA_AUTOWORKER was on
   * 100% of strays AND 100% of deliveries. */
  if (deep_op_claiming(pplayer, punit->id) >= 0
      && deep_op_holds(pplayer, punit->id)) {
    return;
  }

  /* The same leak, for the same reason, on this phase's non-operation
   * holds: a civilian the flee pass sent running for a city is marked
   * done, and this driver never looks at the flag -- so the fled
   * worker was re-tasked back into the field at end of phase, every
   * turn, and the flee logged a decision it did not keep
   * (mechanism-map D-A). */
  if (deep_phase_held(pplayer, punit->id)) {
    return;
  }

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
        if (!deep_settler_can_walk_there(punit, ptile)
            && !unit_transported(punit)
            && def_ai_unit_data(punit, ait)->ferryboat == 0
            && !land_only) {
          /* Boat-dependent mission and the settler is not in the ferry
           * queue at all: release it and re-search LAND-ONLY this same
           * turn, so it founds something instead of idling into an
           * identical overseas pick tomorrow (the first drop-only fix
           * produced a 1200-event pick-drop loop). Land-blocked
           * missions (temporary ZoC jams) keep stock's retry
           * behaviour.
           *
           * The test is == FERRY_NONE, not <= 0. FERRY_WANTED is -1,
           * so the old <= 0 dropped the mission of a settler that had
           * just successfully ASKED for a boat -- and
           * dai_unit_new_task() calls aiferry_clear_boat(), which
           * takes the flag straight back off and decrements
           * stats.passengers. The request never survived to the turn's
           * boat pass: 77% of idle boat-turns saw "nobody flagged
           * anywhere" while settlers stood on the coast, and 66% of
           * overseas missions were abandoned with "waiting for a lift"
           * as their last state. */
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
      /* W12 slice 2: the corridor tiles outside every city radius,
       * which the auction above structurally cannot offer, compete
       * here on the auction's own scale. Inert at the knob's 0. */
      best_impr = deep_trunk_source_task(nmap, pplayer, punit, best_impr,
                                         &best_act, &best_target,
                                         &best_tile, &path,
                                         &completion_time);
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
    } else {
      /* Not founding this turn. A founder is two citizens carrying a
       * shovel: hand it back to a city rather than let it do a
       * pop_cost 0 unit's job or stand still (human call 2026-08-03).
       * Terrain work stays as the fallback for when no city can take
       * it. */
      /* Join only when the search found NO site at all, not merely a
       * site that scored under this turn's terrain work. Joining is
       * irreversible, and a settler that is briefly outbid still
       * founds later once borders or the map move: the first cut
       * joined on "not founding this turn" and cost 0.22 cities and
       * 2800 land area per player over 10 seeds (t=-2.35) to buy an
       * idle-founder count of 0.3. No constant: the condition is the
       * search returning nothing. */
      if (result == NULL) {
        dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);
        if (deep_settler_join_city(ait, nmap, pplayer, punit)) {
          return;
        }
      } else {
        /* A foundable site exists but is worth less than working the
         * land right now; keep the unit and the option. */
        cityresult_destroy(result);
        dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);
      }
      if (best_impr > 0) {
        UNIT_LOG(LOG_DEBUG, punit, "improves terrain instead of founding");
        adv_unit_new_task(punit, AUT_AUTO_WORKER, best_tile);
      } else {
        int sanity = punit->id;

        UNIT_LOG(LOG_DEBUG, punit, "cannot find work");
        (void) deep_idle_fallback_work(ait, nmap, pplayer, punit, state);
        if (game_unit_by_number(sanity) == NULL) {
          /* The fallback can now put a worker on a boat, and a boat can
           * be sunk. Nothing below may touch the unit. */
          return;
        }
        goto CLEANUP;
      }
    }
  } else {
    /* We are a worker or engineer */
    if (best_tile == NULL) {
      int sanity = punit->id;

      if (deep_idle_fallback_work(ait, nmap, pplayer, punit, state)) {
        if (game_unit_by_number(sanity) == NULL) {
          return;
        }
        goto CLEANUP;
      }
    }
    adv_unit_new_task(punit, AUT_AUTO_WORKER, best_tile);
  }

  if (best_tile != NULL
      && auto_worker_setup_work(nmap, pplayer, punit, state, 0, &path,
                                best_tile, best_act, &best_target,
                                completion_time)) {
    if (best_act == ACTIVITY_BASE) {
      deep_fort_note_assignment(pplayer, best_tile);
    }
    if (pcity != NULL) {
      clear_worker_tasks(pcity);
    }
  }

CLEANUP:

  if (best_tile == NULL
      && log_do_output_for_level(deep_metric_log_level())) {
    int tiles, road, irrig, mine, open, xtiles, xopen, xcities;
    const struct ai_plr *plr_data = def_ai_player_data(pplayer, ait);

    deep_idle_work_census(nmap, pplayer, punit, &tiles, &road, &irrig,
                          &mine, &open, &xtiles, &xopen, &xcities);
    log_base(deep_metric_log_level(),
             "deep-idle turn=%d player=\"%s\" unit=%d type=\"%s\""
             " at=(%d,%d) founder=%d impr=%d cities=%d workable=%d"
             " open=%d road=%d irrig=%d mine=%d"
             " xcities=%d xworkable=%d xopen=%d boats=%d avail=%d",
             game.info.turn, player_name(pplayer), punit->id,
             unit_rule_name(punit), TILE_XY(unit_tile(punit)),
             unit_is_cityfounder(punit) ? 1 : 0, (int) best_impr,
             city_list_size(pplayer->cities), tiles, open, road, irrig,
             mine, xcities, xtiles, xopen,
             plr_data->stats.boats, plr_data->stats.available_boats);
  }

  if (NULL != path) {
    pf_path_destroy(path);
  }
}

/* --- The base want (W03/S-GROUND item 1) --------------------------
 *
 * A fortress's want in the shipped advisor is EXACTLY ZERO and it is
 * zero by construction, not by accident: the only valuation a worker
 * candidate gets is adv_calc_extra() -> city_tile_value(), which sums
 * food, shield and trade. A base changes none of the three. The
 * mechanism has always been wired (ACTRES_BASE is in the extra action
 * array, autoworkers.c:121-122) and the measured result is 0
 * fortresses, 0 airbases and 0 buoys in a 175-city game at T162
 * (findings/deep-dive-2026-08-05/action-surface.md, upstream bug #36).
 *
 * Deep prices a base as what it actually buys: the SURVIVAL of a
 * defender standing on that tile, times how much that tile matters.
 * Both halves come from machinery this project already has -- the
 * sequential-attrition substrate (deepodds.h) and the chokepoint test
 * exported from site scoring -- so nothing here is a new theory of
 * combat or of terrain.
 *
 * Bounded scope, stated: the advisor only ever offers candidate tiles
 * inside some city's work radius, so this can never want a fortress on
 * open ground between empires. That is the advisor's loop, not this
 * want, and moving it is a separate change with its own A/B.
 */

/* Per-player, per-turn reference duel and its memoised survival
 * curve. The duel is fixed for the turn (our best defender against
 * the strongest attacker anyone we have met is fielding), so the
 * defender's survival depends only on its post-multiplier strength
 * and can be memoised on that integer -- otherwise this runs inside
 * the candidate loop, several thousand times a player-turn. */
#define DEEP_FORT_STR_MAX 1024
static int deep_fort_turn = -1;
static int deep_fort_plr = -1;
static const struct unit_type *deep_fort_def = NULL;
static const struct unit_type *deep_fort_att = NULL;
static double deep_fort_surv[DEEP_FORT_STR_MAX];
static bool deep_fort_have[DEEP_FORT_STR_MAX];

/* Per-player census accumulators for the base want, reset per turn. */
static int deep_fort_offers[MAX_NUM_PLAYER_SLOTS];
static int deep_fort_evals[MAX_NUM_PLAYER_SLOTS];
static int deep_fort_wants[MAX_NUM_PLAYER_SLOTS];
static double deep_fort_maxwant[MAX_NUM_PLAYER_SLOTS];

/* Whole-GAME accumulators for the base want, never reset: the question
 * "what does a fortress task cost, across a game" cannot be answered
 * by pooling per-turn medians, so the histogram runs for the process
 * and every turn reprints the running answer. The benchmark harness
 * runs one game per server process, so the LAST such line of a game is
 * that game's distribution.
 *
 * The bucket width is a LOGGING resolution, not a tunable: it decides
 * how precisely a printed quantile names a want, and no game state
 * reads it. Positive base wants live in (0, base_fort_gain] by
 * construction -- survival gain and ground weight are both in [0,1] --
 * so 1.0-wide buckets to 200 cover the whole tunable range
 * (tuning-params.ini caps base_fort_gain at 200) with an overflow
 * bucket for anything a future ruleset or arena file puts above it. */
#define DEEP_FORTW_BUCKETS 200
#define DEEP_FORTW_WIDTH 1.0
static int deep_fortw_hist[MAX_NUM_PLAYER_SLOTS][DEEP_FORTW_BUCKETS + 1];
static int deep_fortw_n[MAX_NUM_PLAYER_SLOTS];
static double deep_fortw_min[MAX_NUM_PLAYER_SLOTS];
static double deep_fortw_max[MAX_NUM_PLAYER_SLOTS];
static double deep_fortw_sum[MAX_NUM_PLAYER_SLOTS];

/* Base tasks a worker was actually SET TO WORK on, and the ones whose
 * tile was later seen carrying a base. The ring is a bookkeeping
 * bound, not a game bound: pending base tasks are bounded by the
 * worker count, and an assignment evicted before its tile finished is
 * simply not counted as finished. */
#define DEEP_FORT_RING 64
static int deep_fort_ring[MAX_NUM_PLAYER_SLOTS][DEEP_FORT_RING];
static int deep_fort_ring_n[MAX_NUM_PLAYER_SLOTS];
static int deep_fort_assigned[MAX_NUM_PLAYER_SLOTS];
static int deep_fort_finished[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  Record one positive base want in this player's whole-game histogram.
  Log-only: nothing reads these accumulators back into a decision.
**************************************************************************/
static void deep_fort_want_sample(int pn, double want)
{
  int b;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  b = (int) (want / DEEP_FORTW_WIDTH);
  if (b < 0) {
    b = 0;
  } else if (b > DEEP_FORTW_BUCKETS) {
    b = DEEP_FORTW_BUCKETS;
  }
  deep_fortw_hist[pn][b]++;

  if (deep_fortw_n[pn] == 0 || want < deep_fortw_min[pn]) {
    deep_fortw_min[pn] = want;
  }
  if (want > deep_fortw_max[pn]) {
    deep_fortw_max[pn] = want;
  }
  deep_fortw_sum[pn] += want;
  deep_fortw_n[pn]++;
}

/**********************************************************************//**
  The want at or below which 'pct' percent of this player's positive
  base wants fell, read off the histogram, so the answer is exact to
  the bucket width and clamped to the exactly-tracked extremes.
**************************************************************************/
static double deep_fort_want_pct(int pn, int pct)
{
  int target = (deep_fortw_n[pn] * pct + 99) / 100;
  int seen = 0;
  int b;

  if (deep_fortw_n[pn] <= 0) {
    return 0.0;
  }
  if (target < 1) {
    target = 1;
  }

  for (b = 0; b <= DEEP_FORTW_BUCKETS; b++) {
    seen += deep_fortw_hist[pn][b];
    if (seen >= target) {
      double mid = (b + 0.5) * DEEP_FORTW_WIDTH;

      if (mid < deep_fortw_min[pn]) {
        return deep_fortw_min[pn];
      }
      if (mid > deep_fortw_max[pn]) {
        return deep_fortw_max[pn];
      }

      return mid;
    }
  }

  return deep_fortw_max[pn];
}

/**********************************************************************//**
  Does this tile carry any base at all? Extras do not record who built
  them, so every base-counting question this file asks is answered by
  presence and cannot be attributed -- see the counter names.
**************************************************************************/
static bool deep_fort_tile_has_base(const struct tile *ptile)
{
  extra_type_by_cause_iterate(EC_BASE, pextra) {
    if (tile_has_extra(ptile, pextra)) {
      return TRUE;
    }
  } extra_type_by_cause_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  A worker was just set to work building a base here. Counts the task
  and remembers the tile, so the next census can see whether a base
  appeared on it.
**************************************************************************/
static void deep_fort_note_assignment(const struct player *pplayer,
                                      const struct tile *ptile)
{
  int pn = player_number(pplayer);
  int idx = tile_index(ptile);
  int i;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  deep_fort_assigned[pn]++;

  for (i = 0; i < deep_fort_ring_n[pn]; i++) {
    if (deep_fort_ring[pn][i] == idx) {
      return;
    }
  }

  if (deep_fort_ring_n[pn] < DEEP_FORT_RING) {
    deep_fort_ring[pn][deep_fort_ring_n[pn]++] = idx;
  } else {
    /* Drop the oldest pending tile. Its completion goes uncounted,
     * which is the honest direction to lose accuracy in. */
    memmove(&deep_fort_ring[pn][0], &deep_fort_ring[pn][1],
            (DEEP_FORT_RING - 1) * sizeof(deep_fort_ring[pn][0]));
    deep_fort_ring[pn][DEEP_FORT_RING - 1] = idx;
  }
}

/**********************************************************************//**
  Retire pending base assignments whose tile now carries a base. The
  base need not be the one we ordered and need not be ours -- extras
  record no builder -- so this is "a base task we gave, on a tile that
  now has a base", not a proof of authorship.
**************************************************************************/
static void deep_fort_ring_sweep(const struct player *pplayer)
{
  int pn = player_number(pplayer);
  int i = 0;

  while (i < deep_fort_ring_n[pn]) {
    struct tile *ptile = index_to_tile(&(wld.map), deep_fort_ring[pn][i]);

    if (ptile != NULL && deep_fort_tile_has_base(ptile)) {
      deep_fort_finished[pn]++;
      deep_fort_ring[pn][i] = deep_fort_ring[pn][deep_fort_ring_n[pn] - 1];
      deep_fort_ring_n[pn]--;
      continue;
    }
    i++;
  }
}

/**********************************************************************//**
  Pick this turn's reference duel: our strongest defender against the
  strongest attacker fielded by anyone we have met and are not allied
  to, barbarians INCLUDED -- they are half the reason a fortress is
  worth anything. Omniscient (RC6), like every other input this module
  reads. Returns FALSE when either side of the duel does not exist,
  which is the correct answer for a player nobody can reach yet.
**************************************************************************/
static bool deep_fort_reference(const struct player *pplayer)
{
  int best_def = 0, best_att = 0;

  if (deep_fort_turn == game.info.turn
      && deep_fort_plr == player_number(pplayer)) {
    return (deep_fort_def != NULL && deep_fort_att != NULL);
  }
  deep_fort_turn = game.info.turn;
  deep_fort_plr = player_number(pplayer);
  deep_fort_def = NULL;
  deep_fort_att = NULL;
  memset(deep_fort_have, 0, sizeof(deep_fort_have));

  unit_list_iterate(pplayer->units, u) {
    const struct unit_type *ut = unit_type_get(u);

    if (ut->defense_strength > best_def) {
      best_def = ut->defense_strength;
      deep_fort_def = ut;
    }
  } unit_list_iterate_end;

  players_iterate_alive(aplayer) {
    if (aplayer == pplayer
        || pplayers_allied(pplayer, aplayer)
        || player_diplstate_get(pplayer, aplayer)->type == DS_NO_CONTACT) {
      continue;
    }
    unit_list_iterate(aplayer->units, u) {
      const struct unit_type *ut = unit_type_get(u);

      if (ut->attack_strength > best_att) {
        best_att = ut->attack_strength;
        deep_fort_att = ut;
      }
    } unit_list_iterate_end;
  } players_iterate_alive_end;

  return (deep_fort_def != NULL && deep_fort_att != NULL);
}

/**********************************************************************//**
  P(the reference defender survives one duel) at this defence
  strength, through the sequential-attrition substrate. Memoised for
  the turn.
**************************************************************************/
static double deep_fort_survival(int def_str)
{
  struct deep_duel duel;
  int att_hp, def_hp;

  def_str = CLIP(0, def_str, DEEP_FORT_STR_MAX - 1);
  if (deep_fort_have[def_str]) {
    return deep_fort_surv[def_str];
  }

  duel.att_str = deep_fort_att->attack_strength * POWER_FACTOR;
  duel.att_fp = deep_fort_att->firepower;
  duel.def_str = def_str;
  duel.def_fp = deep_fort_def->firepower;
  att_hp = deep_fort_att->hp;
  def_hp = deep_fort_def->hp;

  deep_fort_surv[def_str]
    = deep_stack_survival(1, &att_hp, 1, &def_hp, &duel);
  deep_fort_have[def_str] = TRUE;

  return deep_fort_surv[def_str];
}

/**********************************************************************//**
  The reference defender's defence power on this tile, WITH one more
  extra present -- the counterfactual the advisor's own valuation
  cannot form.

  `get_virtual_defense_power()` is the ruleset's own answer and it is
  used deliberately in place of anything hand-rolled, because the two
  benchmark rulesets deliver a fortress's defence through DIFFERENT
  channels and only this call reads both:

    classic    `defense_bonus = 100` on the Fortress extra
               (terrain.ruleset:1489), read by
               tile_extras_defense_bonus()
    civ2civ3   NO defense_bonus field at all; +50% Fort and +50%
               Fortress arrive as Defend_Bonus EFFECTS
               (effects.ruleset:273-290)

  A first cut of this want read `extra->defense_bonus` and measured
  ZERO base evaluations in 2 civ2civ3 games with 625 candidate offers,
  which is how the asymmetry was found. The virtual tile mirrors
  adv_calc_extra()'s own construction, conflicting extras removed and
  all.
**************************************************************************/
static int deep_def_power_with(const struct civ_map *nmap,
                               struct player *pplayer,
                               struct tile *ptile,
                               const struct extra_type *added)
{
  struct tile *vtile = tile_virtual_new(ptile);
  int power;

  tile_add_extra(vtile, added);
  extra_type_iterate(cextra) {
    if (tile_has_extra(vtile, cextra)
        && !can_extras_coexist(added, cextra)) {
      tile_remove_extra(vtile, cextra);
    }
  } extra_type_iterate_end;

  power = get_virtual_defense_power(nmap, deep_fort_att, deep_fort_def,
                                    pplayer, vtile, TRUE, 0);
  tile_virtual_destroy(vtile);

  return power;
}

/**********************************************************************//**
  How much this tile matters, 0..1. A chokepoint is worth the full
  weight -- a gate through the landmass is the one piece of ground
  whose holder decides where a war happens -- and otherwise the tile
  is worth G1's border shape, R / (R + d) to the nearest met
  non-allied city on the tile's own landmass, with R the same
  density_fear_radius the fear lattice uses. MAX, not sum: a choke on
  a border does not buy two fortresses.
**************************************************************************/
static double deep_ground_weight(const struct civ_map *nmap,
                                 const struct player *pplayer,
                                 struct tile *ptile)
{
  const struct deep_tuning *tune = deep_tuning_get();
  Continent_id home = tile_continent(ptile);
  int r = tune->density_fear_radius;
  int nearest = -1;
  double border = 0.0;

  if (deep_site_is_choke(nmap, ptile)) {
    return 1.0;
  }
  if (r <= 0) {
    return 0.0;
  }

  players_iterate_alive(aplayer) {
    if (aplayer == pplayer
        || pplayers_allied((struct player *) pplayer, aplayer)
        || player_diplstate_get(pplayer, aplayer)->type == DS_NO_CONTACT) {
      continue;
    }
    city_list_iterate(aplayer->cities, acity) {
      int d;

      if (tile_continent(city_tile(acity)) != home) {
        continue;
      }
      d = real_map_distance(city_tile(acity), ptile);
      if (nearest < 0 || d < nearest) {
        nearest = d;
      }
    } city_list_iterate_end;
  } players_iterate_alive_end;

  if (nearest >= 0) {
    border = (double) r / (r + nearest);
  }

  return border;
}

/**********************************************************************//**
  What a base on this tile is worth, in the same currency the advisor
  values a tile's output in: base_fort_gain x the survival the base
  buys a defender standing there x how much the ground matters.

  Zero unless the candidate really is a defensive base for the class
  our defender belongs to, so a Buoy or a plain airstrip -- which buy
  no survival -- get nothing and the era-dead capability surface does
  not have to be special-cased.
**************************************************************************/
static adv_want deep_base_want(const struct player *pplayer,
                               const struct extra_type *target,
                               struct tile *ptile)
{
  /* Reached from the advisors' task-value hook, outside every deepai.c
   * wrapper: name the seat rather than trust the ambient scope. */
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  const struct civ_map *nmap = &(wld.map);
  struct player *owner = (struct player *) pplayer;
  int d_now, d_with;
  double p_now, p_with, ground;
  adv_want want;

  if (tune->base_fort_gain <= 0.0
      || target == NULL
      || !deep_fort_reference(pplayer)) {
    return 0.0;
  }

  if (!is_native_extra_to_uclass(target, utype_class(deep_fort_def))
      || tile_has_extra(ptile, target)) {
    return 0.0;
  }

  deep_fort_evals[player_number(pplayer)]++;

  ground = deep_ground_weight(nmap, pplayer, ptile);
  if (ground <= 0.0) {
    return 0.0;
  }

  d_now = get_virtual_defense_power(nmap, deep_fort_att, deep_fort_def,
                                    owner, ptile, TRUE, 0);
  d_with = deep_def_power_with(nmap, owner, ptile, target);
  if (d_with <= d_now) {
    /* Buys no defence for a land defender: a Buoy, an airstrip, or a
     * base this ruleset makes purely logistical. Nothing to want. */
    return 0.0;
  }

  p_now = deep_fort_survival(d_now);
  p_with = deep_fort_survival(d_with);
  if (p_with <= p_now) {
    return 0.0;
  }

  want = tune->base_fort_gain * (p_with - p_now) * ground;
  if (want > 0.0) {
    int pn = player_number(pplayer);

    deep_fort_wants[pn]++;
    if (want > deep_fort_maxwant[pn]) {
      deep_fort_maxwant[pn] = want;
    }
    deep_fort_want_sample(pn, (double) want);
  }

  return want;
}

/**********************************************************************//**
  One line per player per turn: how many base candidates were priced,
  how many came out positive, the largest want, and -- the behaviour
  metric, not the want metric -- how many of our workers are actually
  standing on a tile building a base right now.
**************************************************************************/
void deep_fort_census(struct player *pplayer)
{
  int pn = player_number(pplayer);
  int building = 0;

  if (log_do_output_for_level(deep_metric_log_level())) {
    unit_list_iterate(pplayer->units, u) {
      if (u->activity == ACTIVITY_BASE) {
        building++;
      }
    } unit_list_iterate_end;

    if (deep_fort_offers[pn] > 0 || building > 0) {
      log_base(deep_metric_log_level(),
               "deep-fort turn=%d player=\"%s\" offers=%d evals=%d"
               " wants=%d maxwant=%.2f building=%d",
               game.info.turn, player_name(pplayer), deep_fort_offers[pn],
               deep_fort_evals[pn], deep_fort_wants[pn],
               deep_fort_maxwant[pn], building);
    }

    /* Whole-game totals, on their own line so the per-turn one keeps
     * the shape other sessions already grep. */
    deep_fort_ring_sweep(pplayer);
    if (deep_fortw_n[pn] > 0 || deep_fort_assigned[pn] > 0) {
      log_base(deep_metric_log_level(),
               "deep-fortwant turn=%d player=\"%s\" gamewants=%d"
               " assigned=%d finished=%d min=%.2f p25=%.2f p50=%.2f"
               " p75=%.2f max=%.2f mean=%.2f",
               game.info.turn, player_name(pplayer), deep_fortw_n[pn],
               deep_fort_assigned[pn], deep_fort_finished[pn],
               deep_fortw_n[pn] > 0 ? deep_fortw_min[pn] : 0.0,
               deep_fort_want_pct(pn, 25), deep_fort_want_pct(pn, 50),
               deep_fort_want_pct(pn, 75), deep_fortw_max[pn],
               deep_fortw_n[pn] > 0
               ? deep_fortw_sum[pn] / deep_fortw_n[pn] : 0.0);
    }
  }

  deep_fort_offers[pn] = 0;
  deep_fort_evals[pn] = 0;
  deep_fort_wants[pn] = 0;
  deep_fort_maxwant[pn] = 0.0;
}

/* Per-player census for the travel discount, reset per turn. */
static int deep_local_cand[MAX_NUM_PLAYER_SLOTS];
static int deep_local_disc[MAX_NUM_PLAYER_SLOTS];
static int deep_local_turns[MAX_NUM_PLAYER_SLOTS];
static double deep_local_shave[MAX_NUM_PLAYER_SLOTS];

/**********************************************************************//**
  Price the walk on the PRIMARY worker task auction.

  worker_evaluate_improvements() folds travel into amortize() exactly
  once -- after every loop has run, on the champion
  (autoworkers.c:933-937). Inside the loops, the branch that picks
  between tiles nobody works yet compares the resulting tile value and
  nothing else (autoworkers.c:452-469), so two candidates worth the
  same to the city rank identically whether the worker is standing on
  one of them or fifteen tiles away. Only the tie-break between them is
  distance, and only on exact equality.

  Here the IMPROVEMENT -- the candidate value minus the same tile
  without it -- is discounted by amortize() over
  worker_travel_discount * arrival turns, the shape deepsite.c:1487
  already uses for the site search's own travel term. Discounting the
  improvement rather than the total is deliberate: the total is a tile
  value that the advisor also compares against old_tile_value to decide
  whether a candidate improves anything at all, and shrinking THAT
  makes a distant candidate stop being an improvement and vanish
  instead of ranking lower.

  Why this is not the idle fallback again: `deep_idle_fallback_work()`
  measured inert because it only ran where nothing else was wanted, and
  in that regime the declined improvements really were worth nothing
  (findings/idle-workers.md section 7 -- lategame-80 cities average size
  5.8 and never grow to work the outer tiles). This term never adds a
  job; it re-ranks jobs the advisor already wants.
**************************************************************************/
static adv_want deep_worker_travel_price(const struct player *pplayer,
                                         adv_want want,
                                         adv_want old_tile_value,
                                         int arrival)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int pn = player_number(pplayer);
  adv_want gain, priced;

  /* Counted before the gate, like the base want's offers: "the auction
   * offered a candidate" and "we discounted it" are separate facts. */
  deep_local_cand[pn]++;

  if (tune->worker_travel_discount <= 0.0 || arrival <= 0) {
    return want;
  }

  gain = want - old_tile_value;
  if (gain <= 0) {
    /* amortize() shrinks toward zero, so discounting a negative delta
     * would make a distant non-improvement look BETTER than a near one.
     * The auction discards these anyway. */
    return want;
  }

  priced = old_tile_value + deep_worker_travel_amortized(gain, arrival);

  deep_local_disc[pn]++;
  deep_local_turns[pn] += arrival;
  deep_local_shave[pn] += want - priced;

  return priced;
}

/**********************************************************************//**
  One line per player per turn: how many candidates the primary auction
  offered, how many the travel term discounted, the mean arrival of
  those, and the want it removed -- plus the BEHAVIOUR metric the want
  metrics cannot give, how far the workers who ended this turn en route
  still have to walk. That last number is the one the discount exists
  to move, and it is comparable between a knobs-on and a knobs-off run
  of the same seed.
**************************************************************************/
void deep_worker_travel_census(struct player *pplayer)
{
  int pn = player_number(pplayer);

  if (log_do_output_for_level(deep_metric_log_level())) {
    int walkers = 0, walkdist = 0;

    unit_list_iterate(pplayer->units, u) {
      if (unit_has_type_flag(u, UTYF_WORKERS)
          && u->activity == ACTIVITY_IDLE
          && u->goto_tile != NULL) {
        walkers++;
        walkdist += real_map_distance(unit_tile(u), u->goto_tile);
      }
    } unit_list_iterate_end;

    if (deep_local_cand[pn] > 0 || walkers > 0) {
      log_base(deep_metric_log_level(),
               "deep-worklocal turn=%d player=\"%s\" cand=%d disc=%d"
               " meanturns=%.2f shave=%.2f walkers=%d meanwalk=%.2f",
               game.info.turn, player_name(pplayer), deep_local_cand[pn],
               deep_local_disc[pn],
               deep_local_disc[pn] > 0
               ? (double) deep_local_turns[pn] / deep_local_disc[pn] : 0.0,
               deep_local_shave[pn], walkers,
               walkers > 0 ? (double) walkdist / walkers : 0.0);
    }
  }

  deep_local_cand[pn] = 0;
  deep_local_disc[pn] = 0;
  deep_local_turns[pn] = 0;
  deep_local_shave[pn] = 0.0;
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

  Also the seat of the wants the advisor's tile arithmetic structurally
  cannot hold: the BASE want (deep_base_want() above), because
  adv_calc_extra() sums food, shield and trade and a fortress changes
  none of them, and the TRUNK want (deep_trunk_want(), deeptrunk.c),
  because no term in the advisor knows that a road here would join two
  cities. Both are added AFTER the danger discount and are deliberately
  not subject to it: exposed ground is exactly where a fortress is
  worth building and where a corridor most needs closing, and
  discounting them for standing where they are needed would be the
  danger term arguing with itself.

  The travel term (deep_worker_travel_price() above) is applied LAST,
  to whatever the other terms settled on, because it prices the walk
  to the job and not the job: a fortress twelve tiles away is still a
  fortress, and it is still twelve turns of a worker not improving
  anything nearer.
**************************************************************************/
adv_want deep_worker_task_value(const struct player *pplayer,
                                const struct unit *punit,
                                enum unit_activity act,
                                const struct extra_type *target,
                                struct tile *ptile, adv_want value,
                                adv_want old_tile_value, int arrival)
{
  /* Registered globally with the advisors, so it is reached without
   * passing any of deepai.c's hook wrappers and has to name its own
   * seat -- the explicit accessor rather than the ambient scope. */
  const struct deep_tuning *tune = deep_tuning_for(pplayer);
  adv_want addon;
  bool exposed;

  if (pplayer->ai != deep_ai_get_self()) {
    return value;
  }

  addon = deep_trunk_want(pplayer, target, ptile);

  if (act == ACTIVITY_BASE) {
    /* Counted BEFORE any gate: "the advisor offered a base candidate
     * at all" and "we priced it" are different questions, and the
     * first one is the one a zero want has to answer first. */
    deep_fort_offers[player_number(pplayer)]++;
    addon += deep_base_want(pplayer, target, ptile);
  }

  if (tune->worker_danger_discount <= 0.0 || value <= 0) {
    return deep_worker_travel_price(pplayer, value + addon,
                                    old_tile_value, arrival);
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
    return deep_worker_travel_price(pplayer, value + addon,
                                    old_tile_value, arrival);
  }

  return deep_worker_travel_price(pplayer,
                                  value / (1.0 + tune->worker_danger_discount)
                                  + addon, old_tile_value, arrival);
}
