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

/* common/aicore */
#include "path_finding.h"
#include "pf_tools.h"

/* common */
#include "city.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* server */
#include "handicaps.h"
#include "maphand.h"
#include "srv_log.h"
#include "unittools.h"

/* server/advisors */
#include "advgoto.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daiferry.h"
#include "daiplayer.h"
#include "daitools.h"
#include "daiunit.h"

#include "deepai.h"
#include "deepcombat.h"
#include "deeptuning.h"

/* server */
#include "notify.h"

#include "deeptrade.h"

#include "deepops.h"

/**********************************************************************//**
  The slot index of the live operation claiming this unit, or -1.
  Every pool -- operations, raids, ordinary management borrow -- asks
  this before touching a unit, so two intentions never own one blade.
**************************************************************************/
int deep_op_claiming(const struct player *pplayer, int unit_id)
{
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int oi, i;

  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    const struct deep_operation *op = &dp->ops[oi];

    if (op->state == DEEP_OP_NONE) {
      continue;
    }
    /* A lift's boat is claimed as firmly as its manifest: the whole
     * point is that nothing else re-tasks it out from under the
     * crossing. */
    if (op->kind == DEEP_OPK_LIFT && op->ferry == unit_id
        && unit_id != 0) {
      return oi;
    }
    for (i = 0; i < op->n_members; i++) {
      if (op->members[i] == unit_id) {
        return oi;
      }
    }
  }

  return -1;
}

/**********************************************************************//**
  Does an operation hold this unit against the manage loop? TRUE for
  members whose phase the op itself drives: an assault mustering,
  striking or consolidating. Marching members are deliberately NOT
  held -- the stock attack path walks them -- and raiders hunt on
  stock's leash too.
**************************************************************************/
bool deep_op_holds(const struct player *pplayer, int unit_id)
{
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int oi = deep_op_claiming(pplayer, unit_id);
  const struct unit *m;

  if (oi < 0) {
    return FALSE;
  }
  if (dp->ops[oi].kind == DEEP_OPK_LIFT) {
    /* The lift drives both ends itself, every turn it is alive: the
     * boat must not wander off to stock's ferry manager, and the
     * manifest must WAIT at the port instead of re-picking a land
     * mission (the pick-drop churn of section 8). Passengers aboard
     * stay held too -- unlike an assault, here the op owns the hull,
     * so holding cargo cannot freeze the boat. */
    return dp->ops[oi].state == DEEP_OP_MUSTER
           || dp->ops[oi].state == DEEP_OP_MARCH;
  }
  if (dp->ops[oi].kind != DEEP_OPK_ASSAULT) {
    return FALSE;
  }
  m = game_unit_by_number(unit_id);
  if (m != NULL && unit_transported(m)) {
    /* Aboard, the ferry system owns the unit: holding a passenger
     * freezes its boat mid-ocean. The op re-claims it ashore. */
    return FALSE;
  }
  return dp->ops[oi].state == DEEP_OP_MUSTER
         || dp->ops[oi].state == DEEP_OP_STRIKE
         || dp->ops[oi].state == DEEP_OP_CONSOLIDATE;
}

/**********************************************************************//**
  Size the trip-sharing gap, once per player per turn.

  The 1:1 ferry cap (`passengers >= boats`) refuses 50.6% of the
  cross-water worker attempts and gates overseas site evaluation for
  settlers too, but it counts SETTLERS, not TRIPS. civ2civ3's Transport
  has capacity 8 and carries one passenger, so a unit boarding a boat
  that is ALREADY crossing costs nothing -- that is the argument for the
  lift operation, and this is the measurement of what it would be worth.

  A LADDER, not a single number: each rung relaxes one more of the
  constraints a trip-counting cap would have to satisfy, so a gap that
  closes at a particular rung names the constraint that binds.

    wait          units flagged waiting for a lift (only the sign of
                  unit_ai.ferryboat is read, per
                  deep_settler_lift_state())
    slot          ... that could ride some boat of ours holding a free
                  cargo slot, at any distance
    d0 / d3 / d5  ... with that boat 0 / within 3 / within 5 tiles away
    loaded        ... within 5 tiles of a boat that already has cargo
                  aboard -- a trip under way they could join for free

  `loaded` is the rung that matters: it is the demand a trip-counting
  cap would admit and the settler-counting cap refuses. If it is ~0 the
  manifest is worth nothing and the lift operation must earn its place
  by scheduling crossings rather than by sharing them.
**************************************************************************/
static void deep_lift_census(struct ai_type *ait, struct player *pplayer)
{
  const struct ai_plr *plr_data = def_ai_player_data(pplayer, ait);
  int slots = 0, loaded_boats = 0;
  int wait = 0, with_slot = 0, d0 = 0, d3 = 0, d5 = 0, on_loaded = 0;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  unit_list_iterate(pplayer->units, pferry) {
    if (dai_is_ferry(pferry, ait)) {
      int free_slots = get_transporter_capacity(pferry)
                       - get_transporter_occupancy(pferry);

      slots += MAX(free_slots, 0);
      if (get_transporter_occupancy(pferry) > 0) {
        loaded_boats++;
      }
    }
  } unit_list_iterate_end;

  unit_list_iterate(pplayer->units, punit) {
    int nearest_slot = -1, nearest_loaded = -1;

    if (unit_transported(punit)
        || def_ai_unit_data(punit, ait)->ferryboat >= 0) {
      continue;
    }
    wait++;

    unit_list_iterate(pplayer->units, pferry) {
      int d;

      /* could_unit_load(), NOT can_unit_load(): the latter opens with a
       * same_pos() test, so asking it about a boat five tiles away is
       * always FALSE and every distance rung below would collapse to
       * "already standing on the boat". The question here is whether
       * this unit could ride that hull at all -- type compatibility,
       * ownership, free-load rules -- with distance reported
       * separately. */
      if (!dai_is_ferry(pferry, ait)
          || get_transporter_occupancy(pferry)
             >= get_transporter_capacity(pferry)
          || !could_unit_load(punit, pferry)) {
        continue;
      }
      d = real_map_distance(unit_tile(punit), unit_tile(pferry));
      if (nearest_slot < 0 || d < nearest_slot) {
        nearest_slot = d;
      }
      if (get_transporter_occupancy(pferry) > 0
          && (nearest_loaded < 0 || d < nearest_loaded)) {
        nearest_loaded = d;
      }
    } unit_list_iterate_end;

    if (nearest_slot >= 0) {
      with_slot++;
      if (nearest_slot == 0) {
        d0++;
      }
      if (nearest_slot <= 3) {
        d3++;
      }
      if (nearest_slot <= 5) {
        d5++;
      }
    }
    if (nearest_loaded >= 0 && nearest_loaded <= 5) {
      on_loaded++;
    }
  } unit_list_iterate_end;

  log_base(deep_metric_log_level(),
           "deep-lift turn=%d player=\"%s\" boats=%d avail=%d psng=%d"
           " slots=%d loadedboats=%d wait=%d slot=%d d0=%d d3=%d d5=%d"
           " loaded=%d",
           game.info.turn, player_name(pplayer),
           plr_data->stats.boats, plr_data->stats.available_boats,
           plr_data->stats.passengers,
           slots, loaded_boats, wait, with_slot, d0, d3, d5, on_loaded);
}

/* Defined below, with the rest of the operation lifecycle. */
static void deep_op_dissolve(struct deep_operation *op, int oi,
                             const char *why, struct player *pplayer);
static bool deep_lift_wants_boat(struct ai_type *ait, struct unit *punit);

/**********************************************************************//**
  Size hitch-a-ride BEFORE building it, once per player per turn.

  The human design call is that a unit wanting passage should inspect
  boats ALREADY servicing someone bound for its continent and join if
  the detour is cheap. The first census of that idea (the trip-counting
  cap) measured 1.6% and killed it -- but it measured a world with no
  trips in it: boats carried cargo on 11.1% of boat-turns. The lift
  changed that denominator, which is why the question is re-asked here
  rather than assumed answered either way.

  A LADDER again, because a single number names no constraint. Per
  waiting passenger that no operation has claimed:

    want      unclaimed, wants a boat, and its target is OVERSEAS --
              the population hitch-a-ride could act on at all
    anyop     ... while some live lift's boat has a free cargo slot it
              could physically load onto
    cont      ... and that lift's beachhead is on the SAME continent as
              its own target -- the passage it wants is the passage
              already being run
    muster    ... and that lift is still MUSTERing. This is the rung
              that decides the design: joining a boat that has not
              sailed costs a detour, joining one mid-ocean costs a
              turnaround, and only the first can plausibly clear "less
              than one extra turn of waiting"
    d0/d3/d5  ... with that boat 0 / within 3 / within 5 tiles

  d0 is kept as the permanent tripwire the first census earned: if it
  reads equal to d5 the load predicate has silently collapsed to
  same-tile and every rung above is meaningless.

  `opfull` counts the turns where no slot a LIFT MAY USE is free -- the
  army's four plus however many of the lift-only slots lift_op_slots
  admits. It is deliberately the lift's own view rather than "the whole
  array is full": if lifts queue behind a storage bound, the ceiling is
  not the manifest and hitch-a-ride would be fixing the wrong thing.

  The hull rungs answer a separate question -- how often the AI gets to
  carry more than one unit at a time, and whether the boat could even
  if it wanted. Capacity is NOT uniform: civ2civ3's Trireme carries 2
  and its Transport carries 8, so a cell that starts at tech 0 is
  hull-limited where one starting at techlevel 50 is not. `caple2`
  separates "no room" from "no demand", which a single carry count
  would silently merge.
**************************************************************************/
static void deep_hitch_census(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int live = 0, lifts = 0, freeslots = 0, oi;
  int liftfree = 0, usable;
  int boats = 0, caple2 = 0, carry1 = 0, carry2p = 0;
  int want = 0, anyop = 0, cont = 0, muster = 0, d0 = 0, d3 = 0, d5 = 0;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  /* The slot range a lift may actually form into, mirroring
     deep_run_operations(): the army's four, plus lift_op_slots of the
     lift-only range. */
  usable = DEEP_ARMY_OPS_MAX + CLIP(0, tune->lift_op_slots,
                                    DEEP_LIFT_OPS_MAX);

  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    const struct deep_operation *op = &dp->ops[oi];

    if (op->state == DEEP_OP_NONE) {
      if (oi < DEEP_ARMY_OPS_MAX
          || oi < usable) {
        liftfree++;
      }
      continue;
    }
    live++;
    if (op->kind == DEEP_OPK_LIFT) {
      struct unit *ferry = game_unit_by_number(op->ferry);

      lifts++;
      if (ferry != NULL) {
        freeslots += MAX(get_transporter_capacity(ferry)
                         - get_transporter_occupancy(ferry), 0);
      }
    }
  }

  unit_list_iterate(pplayer->units, pferry) {
    int cap, occ;

    if (!dai_is_ferry(pferry, ait)) {
      continue;
    }
    cap = get_transporter_capacity(pferry);
    occ = get_transporter_occupancy(pferry);
    boats++;
    if (cap <= 2) {
      caple2++;
    }
    if (occ == 1) {
      carry1++;
    } else if (occ >= 2) {
      carry2p++;
    }
  } unit_list_iterate_end;

  unit_list_iterate(pplayer->units, punit) {
    int nearest = -1;
    bool has_op = FALSE, has_cont = FALSE, has_muster = FALSE;

    if (!deep_lift_wants_boat(ait, punit)
        || deep_op_claiming(pplayer, punit->id) >= 0
        || tile_continent(unit_tile(punit))
           == tile_continent(punit->goto_tile)) {
      continue;
    }
    want++;

    for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
      const struct deep_operation *op = &dp->ops[oi];
      struct unit *ferry;
      struct tile *beach;
      int d;

      if (op->state == DEEP_OP_NONE || op->kind != DEEP_OPK_LIFT) {
        continue;
      }
      ferry = game_unit_by_number(op->ferry);
      beach = index_to_tile(&(wld.map), op->beachhead);
      if (ferry == NULL || beach == NULL
          || get_transporter_occupancy(ferry)
             >= get_transporter_capacity(ferry)
          || !could_unit_load(punit, ferry)) {
        continue;
      }
      has_op = TRUE;
      if (tile_continent(beach) != tile_continent(punit->goto_tile)) {
        continue;
      }
      has_cont = TRUE;
      if (op->state != DEEP_OP_MUSTER) {
        continue;
      }
      has_muster = TRUE;
      d = real_map_distance(unit_tile(punit), unit_tile(ferry));
      if (nearest < 0 || d < nearest) {
        nearest = d;
      }
    }

    anyop += has_op ? 1 : 0;
    cont += has_cont ? 1 : 0;
    muster += has_muster ? 1 : 0;
    if (nearest >= 0) {
      d0 += (nearest == 0) ? 1 : 0;
      d3 += (nearest <= 3) ? 1 : 0;
      d5 += (nearest <= 5) ? 1 : 0;
    }
  } unit_list_iterate_end;

  log_base(deep_metric_log_level(),
           "deep-hitch turn=%d player=\"%s\" ops=%d lifts=%d opfull=%d"
           " liftslots=%d want=%d anyop=%d cont=%d muster=%d d0=%d"
           " d3=%d d5=%d boats=%d caple2=%d carry1=%d carry2p=%d",
           game.info.turn, player_name(pplayer), live, lifts,
           liftfree == 0 ? 1 : 0, freeslots,
           want, anyop, cont, muster, d0, d3, d5,
           boats, caple2, carry1, carry2p);
}

/**********************************************************************//**
  Where a boat and a passenger can physically meet: the passenger's own
  tile if the boat can float there (a coastal city -- the boat docks on
  top of the settler), otherwise the adjacent tile the boat can reach
  that is nearest to it.

  Loading only ever happens on ONE tile, which is the whole problem the
  lift exists to solve: 0.6% of waiting unit-turns had a boat on their
  tile while 95.6% had one with a free slot somewhere
  (findings/settler-missions.md section 9).
**************************************************************************/
static struct tile *deep_lift_meeting_tile(const struct civ_map *nmap,
                                           struct unit *ferry,
                                           const struct tile *at)
{
  struct tile *best = NULL;
  int bestd = -1;

  if (can_unit_exist_at_tile(nmap, ferry, at)) {
    return (struct tile *) at;
  }
  adjc_iterate(nmap, at, atile) {
    if (can_unit_exist_at_tile(nmap, ferry, atile)) {
      int d = real_map_distance(unit_tile(ferry), atile);

      if (best == NULL || d < bestd) {
        best = atile;
        bestd = d;
      }
    }
  } adjc_iterate_end;

  return best;
}

/**********************************************************************//**
  A tile THIS unit can stand on, at or beside the rendezvous, nearest
  to where it is now. The land half of the meeting: the boat floats on
  the rendezvous tile itself (or docks in the city on it), and the
  passenger walks to the shore beside it.

  Returns NULL if the unit cannot reach any tile touching the
  rendezvous, which is the honest answer that the meeting cannot
  happen there rather than a tile it would fail to walk to.
**************************************************************************/
static struct tile *deep_lift_shore_tile(const struct civ_map *nmap,
                                         struct unit *punit,
                                         const struct tile *meet)
{
  struct tile *best = NULL;
  int bestd = -1;

  if (can_unit_exist_at_tile(nmap, punit, meet)) {
    return (struct tile *) meet;
  }
  adjc_iterate(nmap, meet, atile) {
    if (can_unit_exist_at_tile(nmap, punit, atile)) {
      int d = real_map_distance(unit_tile(punit), atile);

      if (best == NULL || d < bestd) {
        best = atile;
        bestd = d;
      }
    }
  } adjc_iterate_end;

  return best;
}

/**********************************************************************//**
  Is this unit waiting for a lift nobody is bringing? Only the SIGN of
  unit_ai.ferryboat is read (FERRY_WANTED is -1 and private to
  daiferry.c), the same convention deep_settler_lift_state() uses.

  Deliberately ANY unit, not just civilians. A lift is transport, not an
  attack: a settler going to a site, a worker going to an unimproved
  colony, and a defensive blade being redeployed to a garrison across
  the water are the same problem -- a unit that must cross and has no
  way to arrange it. Amphibious ASSAULT is a separate op kind with its
  own escort and invade gates; wanting a boat is not wanting a battle,
  and a garrison move must not have to pretend to be one to get a hull.
**************************************************************************/
static bool deep_lift_wants_boat(struct ai_type *ait, struct unit *punit)
{
  return !unit_transported(punit)
         && def_ai_unit_data(punit, ait)->ferryboat < 0
         && punit->goto_tile != NULL;
}

/**********************************************************************//**
  Form one lift: a named boat, a named passenger, a named beachhead.

  The pairing is nearest-first over (passenger, boat) pairs inside
  lift_form_radius, and the beachhead is the passenger's OWN target --
  punit->goto_tile, which dai_unit_new_task() set when the settler
  chose the site. The operation does not second-guess where the settler
  wanted to go; it only makes the crossing happen.

  Returns TRUE if a lift now occupies slot 'oi'.
**************************************************************************/
static bool deep_op_form_lift(struct ai_type *ait, struct player *pplayer,
                              struct deep_operation *op, int oi)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct unit *best_psngr = NULL, *best_boat = NULL;
  struct tile *meet;
  int best_d = -1;

  unit_list_iterate(pplayer->units, psngr) {
    if (!deep_lift_wants_boat(ait, psngr)
        || deep_op_claiming(pplayer, psngr->id) >= 0) {
      continue;
    }
    /* Only a crossing. A target on our own landmass is a walk, and
     * stock's own goto already handles it. */
    if (tile_continent(unit_tile(psngr))
        == tile_continent(psngr->goto_tile)) {
      continue;
    }
    unit_list_iterate(pplayer->units, boat) {
      int d;

      if (!dai_is_ferry(boat, ait)
          || deep_op_claiming(pplayer, boat->id) >= 0
          || get_transporter_occupancy(boat)
             >= get_transporter_capacity(boat)
          || !could_unit_load(psngr, boat)) {
        continue;
      }
      d = real_map_distance(unit_tile(psngr), unit_tile(boat));
      if (d > tune->lift_form_radius) {
        continue;
      }
      if (best_d < 0 || d < best_d) {
        best_d = d;
        best_psngr = psngr;
        best_boat = boat;
      }
    } unit_list_iterate_end;
  } unit_list_iterate_end;

  if (best_psngr == NULL) {
    return FALSE;
  }
  /* The boat must be able to reach the passenger at all, and the
   * manifest must have somewhere to land. */
  meet = deep_lift_meeting_tile(nmap, best_boat, unit_tile(best_psngr));
  if (meet == NULL
      || deep_lift_meeting_tile(nmap, best_boat,
                                best_psngr->goto_tile) == NULL) {
    return FALSE;
  }
  /* ... and the passenger must be able to walk to the shore beside
   * that tile, or there is no meeting to schedule. */
  if (deep_lift_shore_tile(nmap, best_psngr, meet) == NULL) {
    return FALSE;
  }

  op->state = DEEP_OP_MUSTER;
  op->kind = DEEP_OPK_LIFT;
  op->target_city = 0;
  op->staging_city = 0;
  op->required = 1;
  op->formed_turn = game.info.turn;
  op->march_turn = 0;
  op->march_eta = 0;
  op->phase_turn = game.info.turn;
  op->ferry = best_boat->id;
  op->beachhead = tile_index(best_psngr->goto_tile);
  op->rendezvous = tile_index(meet);
  op->n_members = 1;
  op->members[0] = best_psngr->id;

  log_base(deep_metric_log_level(),
           "deep-lift-op turn=%d player=\"%s\" op=%d form boat=%d"
           " psngr=%d gap=%d beach=(%d,%d)",
           game.info.turn, player_name(pplayer), oi, op->ferry,
           op->members[0], best_d, TILE_XY(best_psngr->goto_tile));

  return TRUE;
}

/**********************************************************************//**
  Drive one lift. MUSTER is the rendezvous -- the boat goes to the
  manifest, which is the half stock never does -- and MARCH is the
  crossing. The op releases the manifest once the boat is beside the
  beachhead: a settler aboard a boat adjacent to its own target is a
  case the stock settler path already handles, and section 8 measured
  that half working.
**************************************************************************/
static void deep_op_drive_lift(struct ai_type *ait, struct player *pplayer,
                               struct deep_operation *op, int oi)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct unit *ferry = game_unit_by_number(op->ferry);
  struct tile *beach = index_to_tile(nmap, op->beachhead);
  int aboard = 0, i;

  if (ferry == NULL || unit_owner(ferry) != pplayer || beach == NULL) {
    deep_op_dissolve(op, oi, "lift-noboat", pplayer);
    return;
  }
  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (m != NULL && unit_transport_get(m) == ferry) {
      aboard++;
    }
  }

  if (op->state == DEEP_OP_MUSTER) {
    if (aboard > 0 && aboard == op->n_members) {
      op->state = DEEP_OP_MARCH;
      op->phase_turn = game.info.turn;
      op->march_turn = game.info.turn;
      log_base(deep_metric_log_level(),
               "deep-lift-op turn=%d player=\"%s\" op=%d sail boat=%d"
               " manifest=%d beach=(%d,%d)",
               game.info.turn, player_name(pplayer), oi, op->ferry,
               aboard, TILE_XY(beach));
    } else if (game.info.turn - op->phase_turn > tune->lift_gather_turns) {
      deep_op_dissolve(op, oi, "lift-gather-timeout", pplayer);
      return;
    } else {
      /* Both ends drive to the named rendezvous. The boat used to do
       * all of the travelling while deep_op_holds() froze the
       * passenger where it stood: measured over 20 games, that cost
       * ~570 held-idle passenger-turns, 57% of them on lifts that
       * never sailed, and EVERY gather-timeout burned the whole
       * window (mean 9.0 turns against a cap of 8, max 9) -- the
       * boat was not arriving late, it was not arriving at all.
       * A settler that can walk half the distance is half a
       * rendezvous the boat no longer has to sail. */
      struct unit *waiting = NULL;
      struct tile *meet = index_to_tile(nmap, op->rendezvous);

      for (i = 0; i < op->n_members && waiting == NULL; i++) {
        struct unit *m = game_unit_by_number(op->members[i]);

        if (m != NULL && unit_transport_get(m) != ferry) {
          waiting = m;
        }
      }
      if (waiting == NULL) {
        deep_op_dissolve(op, oi, "lift-empty", pplayer);
        return;
      }
      if (meet == NULL) {
        /* Pre-rendezvous save, or a tile that stopped working: fall
         * back to the old one-sided choice rather than dissolving a
         * lift that is otherwise healthy. */
        meet = deep_lift_meeting_tile(nmap, ferry, unit_tile(waiting));
        if (meet != NULL) {
          op->rendezvous = tile_index(meet);
        }
      }
      if (meet == NULL) {
        deep_op_dissolve(op, oi, "lift-unreachable", pplayer);
        return;
      }
      if (!same_pos(unit_tile(ferry), meet) && ferry->moves_left > 0) {
        (void) dai_unit_goto(ait, ferry, meet);
        ferry = game_unit_by_number(op->ferry);
        if (ferry == NULL) {
          deep_op_dissolve(op, oi, "lift-boatlost", pplayer);
          return;
        }
      }
      /* Board every member the boat is now standing on or beside. */
      for (i = 0; i < op->n_members; i++) {
        struct unit *m = game_unit_by_number(op->members[i]);

        if (m == NULL || unit_transport_get(m) == ferry) {
          continue;
        }
        if (same_pos(unit_tile(m), unit_tile(ferry))) {
          if (can_unit_load(m, ferry)) {
            unit_transport_load_send(m, ferry);
          }
        } else if (is_tiles_adjacent(unit_tile(m), unit_tile(ferry))
                   && m->moves_left > 0) {
          /* A land unit stepping onto the boat's tile loads itself. */
          (void) dai_unit_goto(ait, m, unit_tile(ferry));
        } else if (m->moves_left > 0) {
          /* The other half of the meeting: walk to the shore beside
           * the rendezvous. Toward the fixed TILE, never toward the
           * boat -- a passenger homing on a hull that is itself
           * homing on the passenger orbits it instead of meeting it. */
          struct tile *shore = deep_lift_shore_tile(nmap, m, meet);

          if (shore != NULL && !same_pos(unit_tile(m), shore)) {
            (void) dai_unit_goto(ait, m, shore);
          }
        }
      }
    }
  }

  if (op->state == DEEP_OP_MARCH) {
    struct tile *land = deep_lift_meeting_tile(nmap, ferry, beach);

    if (aboard == 0) {
      /* The manifest left the boat mid-crossing. This is NOT self
       * evidently a landing: it is equally the shape of a passenger
       * dumped somewhere useless, or dead. Report each member's fate
       * so the two can be told apart -- distance to the beachhead is
       * the discriminator, and a member that ends far from it with the
       * boat still at sea is a lost mission, not an arrival. */
      bool delivered = FALSE;

      for (i = 0; i < op->n_members; i++) {
        struct unit *m = game_unit_by_number(op->members[i]);
        struct tile *mt = (m != NULL ? unit_tile(m) : NULL);

        if (mt != NULL && real_map_distance(mt, beach) <= 1) {
          delivered = TRUE;
        }
        log_base(deep_metric_log_level(),
                 "deep-lift-op turn=%d player=\"%s\" op=%d offboard"
                 " boat=%d unit=%d alive=%d at=(%d,%d) beach=(%d,%d)"
                 " dist=%d trans=%d goal=%d turns=%d",
                 game.info.turn, player_name(pplayer), oi, op->ferry,
                 op->members[i], m != NULL ? 1 : 0,
                 mt != NULL ? index_to_map_pos_x(tile_index(mt)) : -1,
                 mt != NULL ? index_to_map_pos_y(tile_index(mt)) : -1,
                 TILE_XY(beach),
                 mt != NULL ? real_map_distance(mt, beach) : -1,
                 (m != NULL && unit_transported(m)) ? 1 : 0,
                 (m != NULL && m->goto_tile != NULL
                  && same_pos(m->goto_tile, beach)) ? 1 : 0,
                 game.info.turn - op->formed_turn);
      }
      /* Name the two apart in the metric itself, so a later reader
       * cannot mistake one for the other the way "lift-landed" once
       * invited: DELIVERED is a member standing on or beside the
       * beachhead, STRAYED is one that left the boat anywhere else.
       * Measured 9 / 1 / 1 at dist 0 / 3 / 25 over three games, all
       * alive -- the empty boat is overwhelmingly a success, because
       * the passenger disembarks itself the moment the hull touches
       * its target. */
      deep_op_dissolve(op, oi, delivered ? "lift-delivered"
                                         : "lift-strayed", pplayer);
      return;
    }
    if (game.info.turn - op->phase_turn > tune->lift_cross_turns) {
      deep_op_dissolve(op, oi, "lift-cross-timeout", pplayer);
      return;
    }
    if (land == NULL) {
      deep_op_dissolve(op, oi, "lift-nobeach", pplayer);
      return;
    }
    if (real_map_distance(unit_tile(ferry), beach) <= 1
        || same_pos(unit_tile(ferry), land)) {
      /* Beside the target: hand the manifest back. A settler aboard a
       * boat adjacent to its own goto_tile is exactly the case the
       * stock settler path disembarks correctly. */
      log_base(deep_metric_log_level(),
               "deep-lift-op turn=%d player=\"%s\" op=%d arrive boat=%d"
               " manifest=%d at=(%d,%d) beach=(%d,%d) turns=%d",
               game.info.turn, player_name(pplayer), oi, op->ferry,
               aboard, TILE_XY(unit_tile(ferry)), TILE_XY(beach),
               game.info.turn - op->formed_turn);
      deep_op_dissolve(op, oi, "lift-arrived", pplayer);
      return;
    }
    if (ferry->moves_left > 0) {
      (void) dai_unit_goto(ait, ferry, land);
    }
  }
}

/**********************************************************************//**
  The stock manage loop with one change: operation holds survive.
  dai_manage_units() clears every unit's done flag before managing,
  which silently erased everything the operation layer had marked --
  the muster "hold" never held, and STRIKE members were re-tasked by
  find_something_to_kill the same turn the op fought the battle (the
  stall tripwire caught a strike member walking AWAY from the walls
  under stock orders). Same airlift, same defender assignment, same
  per-unit management; the only difference is that units an op holds
  stay done.
**************************************************************************/
void deep_manage_units(struct ai_type *ait, struct player *pplayer)
{
  TIMING_LOG(AIT_AIRLIFT, TIMER_START);
  dai_airlift(ait, pplayer);
  TIMING_LOG(AIT_AIRLIFT, TIMER_STOP);

  deep_lift_census(ait, pplayer);
  /* After deep_run_operations() has already formed and driven every
   * lift it could this turn, which is the vantage the question needs:
   * who is STILL waiting once the lift has done its best, and was
   * there a crossing they could have joined? */
  deep_hitch_census(ait, pplayer);

  unit_list_iterate(pplayer->units, punit) {
    struct unit_ai *unit_data = def_ai_unit_data(punit, ait);

    unit_data->done = deep_op_holds(pplayer, punit->id);
    if (unit_data->task == AIUNIT_DEFEND_HOME && !unit_data->done) {
      dai_unit_new_task(ait, punit, AIUNIT_NONE, NULL);
    }
  } unit_list_iterate_end;

  dai_set_defenders(ait, pplayer);

  /* Trade destinations BEFORE the per-unit loop: stock's
   * dai_manage_caravan() only re-searches when the task is
   * AIUNIT_NONE, so choosing here pre-empts its two defective
   * parameter choices while leaving its ferry and goto handling
   * alone (deeptrade.c). */
  deep_choose_trade_targets(ait, pplayer);

  unit_list_iterate_safe(pplayer->units, punit) {
    if ((!unit_transported(punit)
         || unit_owner(unit_transport_get(punit)) != pplayer)
        && !def_ai_unit_data(punit, ait)->done) {
      bool is_ferry = dai_is_ferry(punit, ait);
      int psngr0 = is_ferry ? def_ai_unit_data(punit, ait)->passenger : 0;
      int id = punit->id;
      struct tile *from = unit_tile(punit);
      int moves0 = punit->moves_left;

      /* dai_manage_unit() dispatches on worker/cityfounder BEFORE it
       * dispatches on ferry, so a boat that also carries the Workers
       * flag never reaches the ferry code at all: it is handed to
       * dai_manage_settler(), parked on SSA_AUTOWORKER and marked
       * done. civ2civ3's Transport is exactly that -- role Ferryboat,
       * capacity 8, the colonisation boat -- and it was 100% of the
       * idle-boat turns that had a reachable settler waiting
       * (upstream-bugs #33).
       *
       * Only divert a boat that has cargo aboard or that somebody is
       * waiting for. With no demand the stock dispatch stands, so a
       * Transport keeps its ocean worker duty when no lift is wanted,
       * and this costs nothing on rulesets whose ferries carry no
       * Workers flag. */
      if (is_ferry
          && def_ai_unit_data(punit, ait)->task != AIUNIT_HUNTER
          && (get_transporter_occupancy(punit) > 0
              || def_ai_player_data(pplayer, ait)->stats.passengers > 0)) {
        dai_manage_ferryboat(ait, pplayer, punit);
      } else {
        dai_manage_unit(ait, pplayer, punit);
      }

      if (is_ferry && log_do_output_for_level(deep_metric_log_level())
          && game_unit_by_number(id) != NULL) {
        log_base(deep_metric_log_level(),
                 "deep-ferry turn=%d player=\"%s\" unit=%d type=\"%s\""
                 " at=(%d,%d) psngr=%d/%d cargo=%d moved=%d"
                 " moves=%d/%d boats=%d avail=%d psng=%d",
                 game.info.turn, player_name(pplayer), id,
                 unit_rule_name(punit), TILE_XY(unit_tile(punit)),
                 psngr0,
                 def_ai_unit_data(punit, ait)->passenger,
                 get_transporter_occupancy(punit),
                 same_pos(from, unit_tile(punit)) ? 0 : 1,
                 moves0, punit->moves_left,
                 def_ai_player_data(pplayer, ait)->stats.boats,
                 def_ai_player_data(pplayer, ait)->stats.available_boats,
                 def_ai_player_data(pplayer, ait)->stats.passengers);
      }
    }
  } unit_list_iterate_safe_end;
}

/**********************************************************************//**
  Release one unit from whatever operation claims it (swap-pop).
  Returns TRUE if a claim existed. The defence counter-claim uses
  this to pull a raider out of the hunt; the op machinery already
  tolerates member loss everywhere (validation prunes, quorums
  re-check).
**************************************************************************/
bool deep_op_release_member(const struct player *pplayer, int unit_id)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  int oi = deep_op_claiming(pplayer, unit_id);
  struct deep_operation *op;
  int i;

  if (oi < 0) {
    return FALSE;
  }
  op = &dp->ops[oi];
  for (i = 0; i < op->n_members; i++) {
    if (op->members[i] == unit_id) {
      op->members[i] = op->members[--op->n_members];
      return TRUE;
    }
  }

  return FALSE;
}

/**********************************************************************//**
  Is any live operation already committed against this city? One op
  per target: a second wave at the same city is the reserve's claim
  (E7), not a parallel operation.
**************************************************************************/
static bool deep_op_targeted(const struct player *pplayer, int city_id)
{
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int oi;

  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    if (dp->ops[oi].state != DEEP_OP_NONE
        && dp->ops[oi].target_city == city_id) {
      return TRUE;
    }
  }

  return FALSE;
}

/**********************************************************************//**
  The most valuable assault target we have actually met and seen:
  best worth over distance from `from` on the same continent. Shared
  gate logic with deep_assault_want (deepdefence.c) -- contact
  required, tile known, at war for real.
**************************************************************************/
static struct city *deep_op_pick_target(struct ai_type *ait,
                                        struct player *pplayer,
                                        const struct tile *from)
{
  struct city *target = NULL;
  double best_score = 0.0;
  Continent_id home = tile_continent(from);
  bool can_sail
    = def_ai_player_data(pplayer, ait)->stats.boats > 0
      || best_role_unit_for_player(pplayer, L_FERRYBOAT) != NULL;

  players_iterate_alive(aplayer) {
    if (!pplayers_at_war(pplayer, aplayer)
        || (!is_barbarian(aplayer)
            && player_diplstate_get(pplayer, aplayer)->type
               == DS_NO_CONTACT)) {
      continue;
    }
    city_list_iterate(aplayer->cities, acity) {
      double score;
      bool overseas = tile_continent(city_tile(acity)) != home;

      if (deep_op_targeted(pplayer, acity->id)
          || !map_is_known(city_tile(acity), pplayer)
          || (overseas
              && (!can_sail
                  || !is_terrain_class_near_tile(&(wld.map),
                                                 city_tile(acity),
                                                 TC_OCEAN)))) {
        continue;
      }
      score = def_ai_city_data(acity, ait)->worth
              / (1.0 + real_map_distance(from, city_tile(acity)));
      if (score > best_score) {
        best_score = score;
        target = acity;
      }
    } city_list_iterate_end;
  } players_iterate_alive_end;

  return target;
}

/**********************************************************************//**
  Collect the player's free attackers on `home`, strongest first:
  military, attack-capable, not bodyguarding, not defending home,
  not already claimed by any live operation.
**************************************************************************/
static int deep_op_collect_pool(struct ai_type *ait,
                                struct player *pplayer,
                                Continent_id home,
                                struct unit **pool, int cap)
{
  int n = 0, i, j;

  unit_list_iterate(pplayer->units, punit) {
    struct unit_ai *ud = def_ai_unit_data(punit, ait);

    if (n >= cap) {
      break;
    }
    if (punit->hp <= 0
        || unit_has_type_flag(punit, UTYF_CIVILIAN)
        || adv_unit_att_rating(punit) <= 0
        || tile_continent(unit_tile(punit)) != home
        || ud->task == AIUNIT_DEFEND_HOME
        || ud->charge > 0
        || deep_op_claiming(pplayer, punit->id) >= 0) {
      continue;
    }
    pool[n++] = punit;
  } unit_list_iterate_end;

  /* Insertion sort, strongest first; pools are small. */
  for (i = 1; i < n; i++) {
    struct unit *u = pool[i];
    int r = adv_unit_att_rating(u);

    for (j = i; j > 0 && adv_unit_att_rating(pool[j - 1]) < r; j--) {
      pool[j] = pool[j - 1];
    }
    pool[j] = u;
  }

  return n;
}

/**********************************************************************//**
  Release every member back to ordinary management (survivors are
  banked, never wasted -- the deliberate-abort rule).
**************************************************************************/
/**********************************************************************//**
  Tell our allies what we are about to do, and what we have called off.

  PURELY SOCIAL. No want reads this, no decision branches on it, and
  nothing here changes a single game state -- it is one chat packet to
  players who are already our allies. It exists because fighting
  alongside an ally who tells you where they are going is a different
  experience from fighting near one who does not.

  Deep is the first version of this AI that CAN say it. Stock's
  war_countdown() (daidiplomacy.c) broadcasts a WAR -- a diplomatic
  state change -- because a war is the only "plan" it has to announce.
  An operation is an actual intention with a target and an ETA, and it
  exists as an announceable object only because W33 made the plan one.

  Deliberately sparse: launches and genuine cancellations only. A
  successful capture is not announced (the ally can see the city change
  hands), and neither is a raid, which has no fixed target to name.
**************************************************************************/
static void deep_brief_allies(const struct player *pplayer,
                              const struct city *target,
                              bool launching, int eta)
{
  if (target == NULL) {
    return;
  }
  players_iterate_alive(ally) {
    if (ally == pplayer || !pplayers_allied(pplayer, ally)) {
      continue;
    }
    if (launching) {
      notify_player(ally, city_tile(target), E_DIPLOMACY, ftc_chat_private,
                    _("*%s (AI)* We are moving against %s. Our forces should"
                      " be in position in about %d turn(s)."),
                    player_name(pplayer), city_name_get(target),
                    MAX(eta, 1));
    } else {
      notify_player(ally, city_tile(target), E_DIPLOMACY, ftc_chat_private,
                    _("*%s (AI)* We have called off our advance on %s."),
                    player_name(pplayer), city_name_get(target));
    }
  } players_iterate_alive_end;
}

static void deep_op_dissolve(struct deep_operation *op, int oi,
                             const char *why, struct player *pplayer)
{
  log_base(deep_metric_log_level(),
           "deep-op turn=%d player=\"%s\" op=%d %s target=%d members=%d",
           game.info.turn, player_name(pplayer), oi, why, op->target_city,
           op->n_members);
  /* Cancellations only: "held" is a capture and "raid-done" has no
   * named target, so neither is news an ally needs. */
  if (op->kind != DEEP_OPK_RAID
      && (0 == strncmp(why, "abort", 5) || 0 == strcmp(why, "target-gone"))) {
    deep_brief_allies(pplayer, game_city_by_number(op->target_city),
                      FALSE, 0);
  }
  op->state = DEEP_OP_NONE;
  op->n_members = 0;
  /* Cleared here as well as set at formation, so a slot reused by an
   * assault can never hand a stale meeting point to a later lift. */
  op->rendezvous = -1;
}

/**********************************************************************//**
  The nearest exposed enemy civilian this raider could hunt: at war,
  contact made (barbarians exempt: their units ARE the contact), out
  of any city, not aboard, same continent, within `radius` tiles.
**************************************************************************/
static struct unit *deep_raid_victim(const struct player *pplayer,
                                     const struct unit *u, int radius)
{
  struct unit *best = NULL;
  int bestd = radius + 1;

  players_iterate_alive(aplayer) {
    if (!pplayers_at_war(pplayer, aplayer)
        || (!is_barbarian(aplayer)
            && player_diplstate_get(pplayer, aplayer)->type
               == DS_NO_CONTACT)) {
      continue;
    }
    unit_list_iterate(aplayer->units, v) {
      int d;

      if (!unit_has_type_flag(v, UTYF_CIVILIAN)
          || tile_city(unit_tile(v)) != NULL
          || unit_transported(v)
          || tile_continent(unit_tile(v))
             != tile_continent(unit_tile(u))) {
        continue;
      }
      d = real_map_distance(unit_tile(u), unit_tile(v));
      if (d < bestd) {
        bestd = d;
        best = v;
      }
    } unit_list_iterate_end;
  } players_iterate_alive_end;

  return best;
}

/**********************************************************************//**
  Enroll idle FAST attackers with a victim in reach into `op`,
  appending after any existing members. Returns how many joined.
**************************************************************************/
static int deep_op_enroll_raiders(struct ai_type *ait,
                                  struct player *pplayer,
                                  struct deep_operation *op)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int enrolled = 0;

  unit_list_iterate(pplayer->units, u) {
    struct unit_ai *ud = def_ai_unit_data(u, ait);

    if (op->n_members + enrolled >= DEEP_OP_MEMBERS) {
      break;
    }
    if (u->hp <= 0
        || unit_has_type_flag(u, UTYF_CIVILIAN)
        || adv_unit_att_rating(u) <= 0
        || unit_type_get(u)->move_rate <= SINGLE_MOVE
        || unit_transported(u)
        || ud->task != AIUNIT_NONE
        || ud->charge > 0
        || deep_op_claiming(pplayer, u->id) >= 0
        || deep_raid_victim(pplayer, u, tune->raid_radius) == NULL) {
      continue;
    }
    op->members[op->n_members + enrolled++] = u->id;
  } unit_list_iterate_end;

  return enrolled;
}

/**********************************************************************//**
  Raid template (combat E8 graduated into the slot system; human
  design: "opportunistic exploration of the enemy's land, pillaging
  improvements and killing workers"). Claim idle FAST attackers that
  have a victim in reach: a worker killed is every work-turn it had
  sunk and every one it would have delivered, at the price of one
  duel against defence 1. No target city, no staging, no quorum.
  Assault formation ran first this turn, so the army's first claim
  on fresh units stands. All of a player's raiders pool into ONE op:
  a raid is a claim container, not a formation, and singleton hunts
  each burning a slot starved assault formation of slots entirely
  (B11: raids held 2+ of the 4 slots on 56% of live player-turns,
  and every fully-blocked turn included one).
**************************************************************************/
static bool deep_op_form_raid(struct ai_type *ait, struct player *pplayer,
                              struct deep_operation *op, int oi)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int enrolled;

  if (tune->raid_radius <= 0) {
    return FALSE;
  }

  enrolled = deep_op_enroll_raiders(ait, pplayer, op);
  if (enrolled == 0) {
    return FALSE;
  }

  op->state = DEEP_OP_MARCH;
  op->kind = DEEP_OPK_RAID;
  op->target_city = 0;
  op->staging_city = 0;
  op->required = 0;
  op->formed_turn = game.info.turn;
  op->march_turn = game.info.turn;
  op->march_eta = 0;
  op->n_members = enrolled;
  /* "raid" deliberately dodges cities.py's (form|march|strike|abort)
   * pool: the assault ledger stays pure. */
  log_base(deep_metric_log_level(),
           "deep-op turn=%d player=\"%s\" op=%d raid members=%d",
           game.info.turn, player_name(pplayer), oi, op->n_members);

  return TRUE;
}

/**********************************************************************//**
  Drive a raid for a turn: members whose last hunt resolved (task
  back to none) get the nearest fresh victim; when nobody is hunting
  any more -- victims cleared out, members re-purposed or dead --
  the raid dissolves and the cavalry goes back to the pool.
**************************************************************************/
static void deep_op_drive_raid(struct ai_type *ait, struct player *pplayer,
                               struct deep_operation *op, int oi)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int i, n = 0, hunting = 0;

  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);
    struct unit_ai *ud;
    struct unit *victim;

    if (m == NULL) {
      continue;
    }
    ud = def_ai_unit_data(m, ait);
    if (ud->task == AIUNIT_ATTACK) {
      if (deep_raid_victim(pplayer, m, tune->raid_radius) == NULL) {
        /* The prey is gone -- dead, hidden in a city, out of range
         * -- and stock "sit tights" a stale ATTACK forever (B13/B14:
         * ~7000 stalled raider-turns per 10 games, mostly this).
         * Clear the hunt and release the blade. */
        dai_unit_new_task(ait, m, AIUNIT_NONE, NULL);
        continue;
      }
      op->members[n++] = op->members[i];
      hunting++;
      continue;
    }
    if (ud->task != AIUNIT_NONE) {
      continue;                 /* Re-purposed: no longer ours. */
    }
    victim = deep_raid_victim(pplayer, m, tune->raid_radius);
    if (victim != NULL) {
      log_base(deep_metric_log_level(),
               "deep-raid turn=%d player=\"%s\" op=%d raider=\"%s\""
               " victim=\"%s\" dist=%d",
               game.info.turn, player_name(pplayer), oi,
               unit_rule_name(m), unit_rule_name(victim),
               real_map_distance(unit_tile(m), unit_tile(victim)));
      dai_unit_new_task(ait, m, AIUNIT_ATTACK, unit_tile(victim));
      (void) dai_gothere(ait, pplayer, m, unit_tile(victim));
      op->members[n++] = op->members[i];
      hunting++;
    }
    /* No hunt and no victim in reach: released. The pooled raid op
     * (one per player) otherwise keeps idle raiders claimed for as
     * long as ANYONE still hunts -- B12 measured 9094 stalled
     * raider-turns per 10 games, cavalry benched out of the assault
     * pool, the garrison draft and fresh raids alike. The raid
     * holds active hunters only; a released raider re-enrolls the
     * moment a new victim strays into reach. */
  }
  op->n_members = n;

  if (hunting == 0) {
    deep_op_dissolve(op, oi, "raid-done", pplayer);
  }
}

/**********************************************************************//**
  Try to form a new operation in slot `oi`: is there a met, unclaimed
  target our free attackers could actually take? required = the
  smallest strongest-first prefix of the unclaimed pool that clears
  op_take_target. Returns TRUE if the slot went live.
**************************************************************************/
static bool deep_op_form(struct ai_type *ait, struct player *pplayer,
                         struct deep_operation *op, int oi)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct deep_plr *dp = deep_plr_data(pplayer);
  struct unit *pool[DEEP_OP_MEMBERS];
  struct city *home_city = NULL, *target, *staging;
  int required = 0, drafted = 0;
  int n, i, eta;
  bool overseas;

  city_list_iterate(pplayer->cities, pcity) {
    home_city = pcity;
    break;
  } city_list_iterate_end;
  if (home_city == NULL) {
    return FALSE;
  }

  /* Second wave first (E7, human design: "backup attackers around
   * the nation as a second wave if the first fails and we think we
   * can crush with just a little more"): while the last strike is
   * fresh and its target still stands hostile, the reserve gets
   * first claim on the next operation -- the garrison it faces is
   * the one wave one softened. If the window closes or the odds
   * say no, formation falls through to ordinary target picking,
   * which is the formal withdrawal (survivors are already banked). */
  target = NULL;
  if (dp->last_strike_city > 0
      && game.info.turn - dp->last_strike_turn <= tune->op_wave_window) {
    struct city *prev = game_city_by_number(dp->last_strike_city);

    if (prev == NULL || !pplayers_at_war(pplayer, city_owner(prev))) {
      /* Taken or turned: the strike is history. */
      dp->last_strike_city = 0;
    } else if (!deep_op_targeted(pplayer, prev->id)) {
      target = prev;
    }
  } else {
    dp->last_strike_city = 0;
  }
  if (target == NULL) {
    target = deep_op_pick_target(ait, pplayer, city_tile(home_city));
  }
  if (target == NULL) {
    return FALSE;
  }

  overseas = tile_continent(city_tile(target))
             != tile_continent(city_tile(home_city));

  /* Staging: nearest own city on the target's continent for a land
   * assault; for an amphibious one, the nearest own COASTAL city on
   * the members' continent -- the convoy gates (naval slice 3) own
   * the crossing from there. */
  staging = NULL;
  {
    int best = FC_INFINITY;

    city_list_iterate(pplayer->cities, pcity) {
      int d = real_map_distance(city_tile(pcity), city_tile(target));

      if (d >= best) {
        continue;
      }
      if (overseas
          ? (tile_continent(city_tile(pcity))
               == tile_continent(city_tile(home_city))
             && is_terrain_class_near_tile(nmap, city_tile(pcity),
                                           TC_OCEAN))
          : tile_continent(city_tile(pcity))
              == tile_continent(city_tile(target))) {
        best = d;
        staging = pcity;
      }
    } city_list_iterate_end;
  }
  if (staging == NULL) {
    return FALSE;
  }

  /* Required force is priced at the defence at arrival: sail (or
   * march) time from the staging point is the defender's budget. */
  {
    int rate = SINGLE_MOVE;

    if (overseas) {
      const struct unit_type *ferrytype
        = best_role_unit_for_player(pplayer, L_FERRYBOAT);

      if (ferrytype != NULL) {
        rate = MAX(ferrytype->move_rate, 1);
      }
    }
    eta = (real_map_distance(city_tile(staging), city_tile(target))
           * SINGLE_MOVE + rate - 1) / rate + (overseas ? 2 : 0);
  }
  n = deep_op_collect_pool(ait, pplayer,
                           tile_continent(city_tile(home_city)),
                           pool, DEEP_OP_MEMBERS);
  for (i = 1; i <= n; i++) {
    if (deep_take_odds_at_arrival(nmap, target, pool, i, eta)
        >= tune->op_take_target) {
      required = i;
      break;
    }
  }
  if (required == 0 && tune->survival_target > 0.0) {
    /* Garrison draft (T160 forensics): when the free pool cannot
     * clear the bar, borrow from the walls -- one sparable blade per
     * city, weakest defender first, and only where the city still
     * clears survival_target without it. The same leave-home-briefly
     * honesty gate the sorties use, pointed at reconquest: a nation
     * whose whole army is DEFEND_HOME infantry can still claim the
     * empty city two tiles away. Drafted blades are ordinary members
     * -- claimed, marched, re-gated, banked on dissolve. */
    city_list_iterate(pplayer->cities, pcity) {
      struct unit *weak = NULL;

      if (n >= DEEP_OP_MEMBERS) {
        break;
      }
      if (tile_continent(city_tile(pcity))
          != tile_continent(city_tile(home_city))) {
        continue;
      }
      unit_list_iterate(city_tile(pcity)->units, u) {
        if (unit_owner(u) != pplayer
            || unit_has_type_flag(u, UTYF_CIVILIAN)
            || u->hp <= 0
            || adv_unit_att_rating(u) <= 0
            || def_ai_unit_data(u, ait)->charge > 0
            || deep_op_claiming(pplayer, u->id) >= 0) {
          continue;
        }
        if (weak == NULL
            || adv_unit_def_rating_basic(u)
               < adv_unit_def_rating_basic(weak)) {
          weak = u;
        }
      } unit_list_iterate_end;
      if (weak != NULL
          && deep_city_stack_survival_without(nmap, pcity,
                                              tune->survival_horizon,
                                              weak, NULL)
             >= tune->survival_target) {
        pool[n++] = weak;
        drafted++;
      }
    } city_list_iterate_end;
    if (drafted > 0) {
      for (i = 1; i <= n; i++) {
        if (deep_take_odds_at_arrival(nmap, target, pool, i, eta)
            >= tune->op_take_target) {
          required = i;
          break;
        }
      }
    }
  }
  if (required < 1) {
    /* No force clears the bar. */
    return FALSE;
  }
  /* required == 1 forms too (T160 forensics): "one unit needs no
   * operation" assumed stock would claim singleton targets, but
   * stock garrisons sit at DEFEND_HOME and never walk into even an
   * EMPTY city -- 47 of the 52 barbarian-held cities at T160 had no
   * garrison and no suitor. The op machinery gives the singleton
   * exactly what it needs: a claimed unit, a march, a re-gate. */

  op->state = DEEP_OP_MUSTER;
  op->kind = DEEP_OPK_ASSAULT;
  op->target_city = target->id;
  op->staging_city = staging->id;
  op->required = required;
  op->formed_turn = game.info.turn;
  /* Enroll the required force plus a small margin; the rest of the
   * pool stays free as the designated reserve (E7) -- the second
   * wave if the first falls short, ordinary units otherwise. */
  op->n_members = MIN(MIN(n, required + (required + 1) / 2),
                      DEEP_OP_MEMBERS);
  for (i = 0; i < op->n_members; i++) {
    op->members[i] = pool[i]->id;
  }
  log_base(deep_metric_log_level(),
           "deep-op turn=%d player=\"%s\" op=%d form target=%d staging=%d"
           " required=%d pool=%d drafted=%d wave2=%d",
           game.info.turn, player_name(pplayer), oi, op->target_city,
           op->staging_city, op->required, op->n_members, drafted,
           dp->last_strike_city == target->id ? 1 : 0);

  return TRUE;
}

/**********************************************************************//**
  Release every member at the target through the stock attack path.
  They leave together; that was the whole point.
**************************************************************************/
static void deep_op_launch(struct ai_type *ait, struct player *pplayer,
                           struct deep_operation *op)
{
  int i;

  /* Re-resolve the target on EVERY iteration, and never hold the
   * pointer across dai_gothere().
   *
   * dai_gothere() moves the member, and a move can end the target: deep
   * takes undefended cities by walking into them, and an emptied city
   * is destroyed outright rather than captured. The city struct is then
   * freed, so a `target` hoisted out of this loop is dangling for every
   * member after the one that won -- and city_tile() on it yields a
   * plausible-looking pointer into freed memory. The wave's own success
   * corrupts the rest of its launch.
   *
   * Observed as two different assertions from one cause, depending on
   * what reoccupied the freed block: same_pos() [common/map.c:1087] on
   * a null tile, which killed the EAST2 tuning campaign in r0-ALL gen1,
   * and dai_gothere_bodyguard() [ai/default/daitools.c:178] on a tile
   * whose unit list was null, reproduced locally on the same candidate,
   * config and seed at the same turn (primary-120 seed 5, T102). */
  for (i = 0; i < op->n_members; i++) {
    struct city *target = game_city_by_number(op->target_city);
    struct unit *m = game_unit_by_number(op->members[i]);

    if (target == NULL) {
      /* Taken or razed mid-launch -- by this wave, most likely. The
       * remaining members have nowhere to be sent; the op layer will
       * re-plan them next turn. */
      log_base(deep_metric_log_level(),
               "deep-op turn=%d player=\"%s\" launch stopped at member %d/%d:"
               " target=%d no longer exists",
               game.info.turn, player_name(pplayer), i, op->n_members,
               op->target_city);
      return;
    }
    if (m == NULL || unit_owner(m) != pplayer) {
      continue;
    }
    dai_unit_new_task(ait, m, AIUNIT_ATTACK, city_tile(target));
    (void) dai_gothere(ait, pplayer, m, city_tile(target));
  }
}

/**********************************************************************//**
  Turns until the STRIKING QUORUM reaches the target: the required-th
  smallest member arrival, raw-distance at each member's own rate --
  the same currency the formation pricing used. Not the slowest
  member: the wave attacks when its quorum arrives, and the enrolled
  margin straggling behind is a reinforcement, not the strike clock.
  (B11 ledger: 27 of 47 march aborts re-gated on the launch turn
  itself with straggler-driven etas of 3-8 -- a full quorum standing
  at the walls priced as if it waited for its last reserve -- while
  all four waves that did strike launched at eta 1-2.)
**************************************************************************/
static int deep_op_eta(struct player *pplayer,
                       const struct deep_operation *op,
                       const struct city *target)
{
  const struct unit_type *ferrytype
    = best_role_unit_for_player(pplayer, L_FERRYBOAT);
  Continent_id tcont = tile_continent(city_tile(target));
  int t[DEEP_OP_MEMBERS];
  int i, j, n = 0, k;

  for (i = 0; i < op->n_members; i++) {
    const struct unit *m = game_unit_by_number(op->members[i]);
    int rate, ti;

    if (m == NULL) {
      continue;
    }
    /* A member crossing water travels at the boat's pace, not its
     * own: aboard, the transporter's rate; ashore on the wrong
     * continent, the best ferry we could build (the same currency
     * formation priced the sail with). */
    rate = MAX(unit_type_get(m)->move_rate, 1);
    if (unit_transported(m)) {
      rate = MAX(unit_type_get(unit_transport_get((struct unit *) m))
                 ->move_rate, 1);
    } else if (tile_continent(unit_tile(m)) != tcont
               && ferrytype != NULL) {
      rate = MAX(ferrytype->move_rate, 1);
    }
    ti = (real_map_distance(unit_tile(m), city_tile(target))
          * SINGLE_MOVE + rate - 1) / rate;
    for (j = n; j > 0 && t[j - 1] > ti; j--) {
      t[j] = t[j - 1];
    }
    t[j] = ti;
    n++;
  }

  if (n == 0) {
    return 0;
  }
  k = MIN(MAX(op->required, 1), n);

  return t[k - 1];
}

/**********************************************************************//**
  Is this operation (still) an amphibious crossing: any member
  afloat, or ashore on a continent that is not the target's?
**************************************************************************/
static bool deep_op_crossing(const struct deep_operation *op,
                             const struct city *target)
{
  Continent_id tcont = tile_continent(city_tile(target));
  int i;

  for (i = 0; i < op->n_members; i++) {
    const struct unit *m = game_unit_by_number(op->members[i]);

    if (m != NULL
        && (unit_transported(m)
            || tile_continent(unit_tile(m)) != tcont)) {
      return TRUE;
    }
  }

  return FALSE;
}

/**********************************************************************//**
  Before any abort banks the wave, try to re-muster it where it
  stands (human doctrine 2026-08-01: an army in the field -- or a
  fleet at sea -- is a sunk investment that can pick a second
  target). Candidates are every known at-war city except the one
  being abandoned; feasibility is the FULL formation bar at the
  force's own arrival eta (a retarget is a new commitment, not a
  discount), and among the feasible the pick is formation's own
  scoring, worth over distance, with the quorum eta as the distance
  -- so near targets win and far ones pay their sail in virtual
  defenders. Success re-launches the op as a fresh MARCH from here.
**************************************************************************/
static bool deep_op_retarget(struct ai_type *ait, struct player *pplayer,
                             struct deep_operation *op, int oi)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct unit *stream[DEEP_OP_MEMBERS];
  struct city *alt = NULL;
  double alt_odds = 0.0, alt_score = 0.0;
  int alt_eta = 0;
  int i, ns = 0;
  bool can_sail
    = def_ai_player_data(pplayer, ait)->stats.boats > 0
      || best_role_unit_for_player(pplayer, L_FERRYBOAT) != NULL;

  if (tune->op_take_target <= 0.0 || op->kind != DEEP_OPK_ASSAULT) {
    return FALSE;
  }
  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (m != NULL && m->hp > 0) {
      stream[ns++] = m;
    }
  }
  if (ns == 0) {
    return FALSE;
  }

  players_iterate_alive(aplayer) {
    if (!pplayers_at_war(pplayer, aplayer)
        || (!is_barbarian(aplayer)
            && player_diplstate_get(pplayer, aplayer)->type
               == DS_NO_CONTACT)) {
      continue;
    }
    city_list_iterate(aplayer->cities, acity) {
      Continent_id ccont = tile_continent(city_tile(acity));
      bool reachable = FALSE;
      double codds, score;
      int ceta;

      if (acity->id == op->target_city
          || deep_op_targeted(pplayer, acity->id)
          || !map_is_known(city_tile(acity), pplayer)) {
        continue;
      }
      for (i = 0; i < ns; i++) {
        if (tile_continent(unit_tile(stream[i])) == ccont) {
          reachable = TRUE;
          break;
        }
      }
      if (!reachable
          && (!can_sail
              || !is_terrain_class_near_tile(nmap, city_tile(acity),
                                             TC_OCEAN))) {
        continue;
      }
      ceta = deep_op_eta(pplayer, op, acity);
      codds = deep_take_odds_at_arrival(nmap, acity, stream, ns, ceta);
      if (codds < tune->op_take_target) {
        continue;
      }
      score = def_ai_city_data(acity, ait)->worth / (1.0 + ceta);
      if (score > alt_score) {
        alt = acity;
        alt_odds = codds;
        alt_score = score;
        alt_eta = ceta;
      }
    } city_list_iterate_end;
  } players_iterate_alive_end;

  if (alt == NULL) {
    return FALSE;
  }

  /* Prefer a city as the re-muster point (human doctrine
   * 2026-08-01: field musters are random squares). A land wave
   * rallies at the own city nearest the new target among those no
   * farther from it than the wave's quorum already stands -- on the
   * way, never backtracking -- and walks the proper MUSTER there.
   * No city on the route, or a fleet mid-crossing (its port call is
   * the offshore-hold future work): march direct. */
  {
    struct city *rally = NULL;

    if (!deep_op_crossing(op, alt)) {
      int wd[DEEP_OP_MEMBERS];
      int j, k, nq = 0, best = FC_INFINITY;

      for (i = 0; i < ns; i++) {
        int d = real_map_distance(unit_tile(stream[i]),
                                  city_tile(alt));

        for (j = nq; j > 0 && wd[j - 1] > d; j--) {
          wd[j] = wd[j - 1];
        }
        wd[j] = d;
        nq++;
      }
      k = MIN(MAX(op->required, 1), nq);
      city_list_iterate(pplayer->cities, pcity) {
        int d = real_map_distance(city_tile(pcity), city_tile(alt));

        if (d <= wd[k - 1] && d < best
            && tile_continent(city_tile(pcity))
               == tile_continent(city_tile(alt))) {
          best = d;
          rally = pcity;
        }
      } city_list_iterate_end;
    }

    log_base(deep_metric_log_level(),
             "deep-op turn=%d player=\"%s\" op=%d retarget from=%d"
             " to=%d staging=%d members=%d eta=%d p=%.3f",
             game.info.turn, player_name(pplayer), oi, op->target_city,
             alt->id, rally != NULL ? rally->id : 0, ns, alt_eta,
             alt_odds);
    op->target_city = alt->id;
    if (rally != NULL) {
      op->staging_city = rally->id;
      op->required = ns;
      op->state = DEEP_OP_MUSTER;
      op->formed_turn = game.info.turn;
    } else {
      op->state = DEEP_OP_MARCH;
      op->march_turn = game.info.turn;
      op->march_eta = MAX(alt_eta, 1);
      deep_op_launch(ait, pplayer, op);
    }
  }

  return TRUE;
}

/**********************************************************************//**
  A sinking city outbids a mustering op for one blade (the defence
  counter-claim, findings/defence-counter-claim.md). The comparison
  is expected worth in the one currency both sides already use:
  the city risks worth_own * (1 - p_survival); the op's expected
  take is worth_target * odds at its remaining eta. Only MUSTER ops
  bid -- a launched wave turning around mid-assault is the exact
  dithering W33 exists to kill. Returns the released blade (best
  defender arriving within the survival horizon) or NULL.
**************************************************************************/
struct unit *deep_op_poach_defender(struct ai_type *ait,
                                    struct player *pplayer,
                                    struct city *pcity, double p_survival)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct deep_plr *dp = deep_plr_data(pplayer);
  double city_risk
    = def_ai_city_data(pcity, ait)->worth * (1.0 - p_survival);
  int oi, i;

  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    struct deep_operation *op = &dp->ops[oi];
    struct city *target;
    struct unit *stream[DEEP_OP_MEMBERS];
    struct unit *blade = NULL;
    int blade_i = -1, ns = 0, eta;
    double odds;

    if (op->state != DEEP_OP_MUSTER || op->kind != DEEP_OPK_ASSAULT) {
      continue;
    }
    target = game_city_by_number(op->target_city);
    if (target == NULL) {
      continue;
    }
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);

      if (m != NULL && m->hp > 0) {
        stream[ns++] = m;
      }
    }
    if (ns == 0) {
      continue;
    }
    eta = deep_op_eta(pplayer, op, target);
    odds = deep_take_odds_at_arrival(nmap, target, stream, ns, eta);
    if (city_risk <= def_ai_city_data(target, ait)->worth * odds) {
      continue;                 /* The op's claim outbids the city. */
    }
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);
      int rate, t;

      if (m == NULL || m->hp <= 0
          || adv_unit_def_rating_basic(m) <= 0
          || tile_continent(unit_tile(m))
             != tile_continent(city_tile(pcity))) {
        continue;
      }
      rate = MAX(unit_type_get(m)->move_rate, 1);
      t = (real_map_distance(unit_tile(m), city_tile(pcity))
           * SINGLE_MOVE + rate - 1) / rate;
      if (t > tune->survival_horizon) {
        continue;               /* Would arrive after the wave. */
      }
      if (blade == NULL
          || adv_unit_def_rating_basic(m)
             > adv_unit_def_rating_basic(blade)) {
        blade = m;
        blade_i = i;
      }
    }
    if (blade != NULL) {
      log_base(deep_metric_log_level(),
               "deep-op turn=%d player=\"%s\" op=%d poach unit=\"%s\""
               " city=%d target=%d odds=%.2f",
               game.info.turn, player_name(pplayer), oi,
               unit_rule_name(blade), pcity->id, op->target_city, odds);
      op->members[blade_i] = op->members[--op->n_members];
      return blade;
    }
  }

  return NULL;
}

/**********************************************************************//**
  Hold what the wave took (W33 phase 3, CONSOLIDATE). Members
  standing in the captured city convert to its garrison -- best
  defender first, handed to stock as AIUNIT_DEFEND_HOME and released
  from the op -- until the stack clears survival_target, the same
  bar every other city answers to (no new constant). Members still
  outside walk in under op control while the bar is short. When the
  bar clears, or the patience knob runs out, the rest are banked and
  the op dissolves: "held".
**************************************************************************/
static void deep_op_drive_consolidate(struct ai_type *ait,
                                      struct player *pplayer,
                                      struct deep_operation *op, int oi)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct city *target = game_city_by_number(op->target_city);
  struct tile *ctile;
  bool cleared;
  int i;

  if (target == NULL) {
    /* deep_run_operations() validates every slot BEFORE driving any of
     * them, so an earlier op in the drive loop can destroy this one's
     * target between the two passes. Re-checked here rather than
     * trusted from ~500 lines away. */
    deep_op_dissolve(op, oi, "target-gone", pplayer);
    return;
  }
  ctile = city_tile(target);

  /* Convert best defenders in the city until the bar clears. */
  for (;;) {
    struct unit *gar = NULL;
    int gar_i = -1;

    cleared = tune->survival_target <= 0.0
              || deep_city_stack_survival(nmap, target,
                                          tune->survival_horizon, NULL)
                 >= tune->survival_target;
    if (cleared) {
      break;
    }
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);

      if (m == NULL || m->hp <= 0
          || adv_unit_def_rating_basic(m) <= 0
          || !same_pos(unit_tile(m), ctile)) {
        continue;
      }
      if (gar == NULL
          || adv_unit_def_rating_basic(m)
             > adv_unit_def_rating_basic(gar)) {
        gar = m;
        gar_i = i;
      }
    }
    if (gar == NULL) {
      break;
    }
    log_base(deep_metric_log_level(),
             "deep-op turn=%d player=\"%s\" op=%d garrison target=%d"
             " unit=\"%s\"",
             game.info.turn, player_name(pplayer), oi, op->target_city,
             unit_rule_name(gar));
    dai_unit_new_task(ait, gar, AIUNIT_DEFEND_HOME, ctile);
    def_ai_unit_data(gar, ait)->done = TRUE;
    op->members[gar_i] = op->members[--op->n_members];
  }

  if (cleared || game.info.turn - op->phase_turn >= tune->op_muster_wait) {
    /* Held (or held as well as this wave ever will): bank the rest. */
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);

      if (m != NULL
          && def_ai_unit_data(m, ait)->task == AIUNIT_ATTACK) {
        dai_unit_new_task(ait, m, AIUNIT_NONE, NULL);
      }
    }
    deep_op_dissolve(op, oi, "held", pplayer);
    return;
  }

  /* Bar still short: the rest of the wave walks in under op
   * control (passengers excepted -- the ferry lands them). */
  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);
    int m_id;

    if (m == NULL || m->hp <= 0 || unit_transported(m)) {
      continue;
    }
    m_id = m->id;
    if (!def_ai_unit_data(m, ait)->done
        && m->moves_left > 0
        && !same_pos(unit_tile(m), ctile)) {
      (void) dai_gothere(ait, pplayer, m, ctile);
      m = game_unit_by_number(m_id);
    }
    if (m != NULL) {
      def_ai_unit_data(m, ait)->done = TRUE;
    }
  }
}

/**********************************************************************//**
  Fight the battle under op control (W33 phase 3, strike control).
  The wave stands at the walls; instead of two dozen independent
  find_something_to_kill passes, the op orders the attacks itself:
  best duel first -- each round, the adjacent member with the highest
  win chance against the target's current best defender strikes --
  and every blade goes at the op target (focus fire), so kills
  compound into a fallen city instead of spreading across the front.
  Members not yet adjacent close on the walls. Everyone the op
  touches is marked done: the manage loop never re-tasks a committed
  member (done-flag discipline; the stall tripwire watches the cost).
  A strike that stalls past the patience knob, or whose odds against
  the standing garrison fall through the abort floor, banks its
  survivors -- the deliberate-abort rule, now at the walls too.
**************************************************************************/
static void deep_op_drive_strike(struct ai_type *ait,
                                 struct player *pplayer,
                                 struct deep_operation *op, int oi)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct city *target = game_city_by_number(op->target_city);
  struct tile *ctile;
  struct unit *stream[DEEP_OP_MEMBERS];
  int i, ns = 0;

  if (target == NULL) {
    /* See deep_op_drive_consolidate(): validation happens a whole pass
     * earlier, and the ops ahead of this one in the drive loop run in
     * between. */
    deep_op_dissolve(op, oi, "target-gone", pplayer);
    return;
  }
  ctile = city_tile(target);

  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (m != NULL) {
      stream[ns++] = m;
    }
  }

  /* Deliberate abort before the first blow: the wave at the walls
   * against the garrison as it stands right now (eta 0 -- no
   * virtual reinforcements; whoever is coming is already here). */
  if (tune->op_abort_floor > 0.0 && ns > 0) {
    double odds = deep_take_odds(nmap, target, stream, ns);

    if (odds < tune->op_abort_floor) {
      /* The walls held: probe done, pick a second target if one is
       * feasible from right here, else bank. */
      if (deep_op_retarget(ait, pplayer, op, oi)) {
        return;
      }
      for (i = 0; i < ns; i++) {
        struct unit_ai *ud = def_ai_unit_data(stream[i], ait);

        if (ud->task == AIUNIT_ATTACK) {
          dai_unit_new_task(ait, stream[i], AIUNIT_NONE, NULL);
        }
      }
      log_base(deep_metric_log_level(),
               "deep-op turn=%d player=\"%s\" op=%d regate target=%d"
               " odds=%.2f eta=0 members=%d",
               game.info.turn, player_name(pplayer), oi,
               op->target_city, odds, ns);
      deep_op_dissolve(op, oi, "abort-strike", pplayer);
      return;
    }
  }

  /* The battle: repeat picking the best matchup until nobody
   * adjacent can act. Each round re-reads the defender -- the stack
   * shrinks as it dies, and an emptied city is a walk-in (win chance
   * 1.0 with the conquer chain doing the entering). Progress is
   * guaranteed: every attacker is marked done win or lose. */
  for (;;) {
    struct unit *att = NULL;
    double best = -1.0;

    if (city_owner(target) == pplayer) {
      break;                    /* Fell to us mid-battle. */
    }
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);
      struct unit *pdef;
      double chance;

      if (m == NULL || m->hp <= 0 || m->moves_left <= 0
          || def_ai_unit_data(m, ait)->done
          || adv_unit_att_rating(m) <= 0
          || unit_transported(m)   /* Ferry lands it; op waits. */
          || !is_tiles_adjacent(unit_tile(m), ctile)) {
        continue;
      }
      pdef = get_defender(nmap, m, ctile, NULL);
      chance = pdef != NULL ? unit_win_chance(nmap, m, pdef, NULL) : 1.0;
      if (chance > best) {
        best = chance;
        att = m;
      }
    }
    if (att == NULL) {
      break;
    }
    {
      int att_id = att->id;

      (void) dai_unit_attack(ait, att, ctile);
      att = game_unit_by_number(att_id);
      if (att != NULL) {
        def_ai_unit_data(att, ait)->done = TRUE;
      }
    }
  }

  /* Convergence and commitment: stragglers close on the walls, and
   * every surviving member -- moved or not -- is done for the turn.
   * The op owns them; stock gets them back when it banks or wins.
   * Transported members are the exception both ways: the ferry
   * system lands them (never mark a passenger done). */
  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);
    int m_id;

    if (m == NULL || m->hp <= 0 || unit_transported(m)) {
      continue;
    }
    m_id = m->id;
    if (!def_ai_unit_data(m, ait)->done
        && m->moves_left > 0
        && !is_tiles_adjacent(unit_tile(m), ctile)
        && !same_pos(unit_tile(m), ctile)
        && city_owner(target) != pplayer) {
      (void) dai_gothere(ait, pplayer, m, ctile);
      m = game_unit_by_number(m_id);
      /* Same hazard as the launch: dai_gothere() moves the member, and
       * walking into an emptied city destroys a size-1 one. `ctile`
       * survives -- tiles are never freed -- but `target` does not, and
       * the next iteration and the check below both dereference it. */
      target = game_city_by_number(op->target_city);
      if (target == NULL) {
        if (m != NULL) {
          def_ai_unit_data(m, ait)->done = TRUE;
        }
        deep_op_dissolve(op, oi, "target-gone", pplayer);
        return;
      }
    }
    if (m != NULL) {
      def_ai_unit_data(m, ait)->done = TRUE;
    }
  }

  if (city_owner(target) != pplayer
      && game.info.turn - op->phase_turn >= tune->op_muster_wait) {
    /* The walls held past our patience: a second target from here,
     * or bank the survivors. */
    if (deep_op_retarget(ait, pplayer, op, oi)) {
      return;
    }
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);

      if (m != NULL
          && def_ai_unit_data(m, ait)->task == AIUNIT_ATTACK) {
        dai_unit_new_task(ait, m, AIUNIT_NONE, NULL);
      }
    }
    deep_op_dissolve(op, oi, "abort-strike", pplayer);
  }
}

/**********************************************************************//**
  Drive one live operation for a turn. MUSTER: walk members in, hold
  arrivals, launch at quorum (or half-quorum at the wait cap). A land
  launch enters MARCH: members go to the stock attack path while the
  operation shadows them -- keeping them claimed, re-gating the take
  odds at the remaining eta each turn, banking everyone if the odds
  collapse. At contact the op enters STRIKE, takes the members back,
  and fights the battle itself (the E7 window opens at the walls).
  An amphibious launch still dissolves on the spot: the convoy
  hold/escort/invade gates own the crossing unchanged. Caller has
  already validated target and staging.
**************************************************************************/
static void deep_op_drive(struct ai_type *ait, struct player *pplayer,
                          struct deep_operation *op, int oi)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct deep_plr *dp = deep_plr_data(pplayer);
  struct city *target = game_city_by_number(op->target_city);
  struct city *staging = game_city_by_number(op->staging_city);
  int i;

  /* The comment above claims the caller "already validated target and
   * staging". It did -- in a SEPARATE pass over every slot, before any
   * op was driven. The ops ahead of this one in the drive loop then
   * ran, and deep takes an undefended city by walking into it, so an
   * earlier op's success can raze this one's target in between. Two
   * core dumps caught it: a freed pointer through
   * deep_take_odds_at_arrival(), and NULL through deep_op_eta(), both
   * from the MARCH block below, which reads this hoist. */
  if (target == NULL) {
    deep_op_dissolve(op, oi, "target-gone", pplayer);
    return;
  }
  if (op->state == DEEP_OP_MUSTER && staging == NULL) {
    /* A muster with no staging city has nowhere to gather. */
    deep_op_dissolve(op, oi, "staging-gone", pplayer);
    return;
  }

  if (op->state == DEEP_OP_MUSTER) {
    int present = 0;

    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);

      if (m != NULL && same_pos(unit_tile(m), city_tile(staging))) {
        present++;
      }
    }

    if (present >= op->required
        || (game.info.turn - op->formed_turn >= tune->op_muster_wait
            && present >= (op->required + 1) / 2)) {
      /* Land or sea, the wave marches shadowed: the ferry system
       * carries the crossing (transported members stay on stock's
       * leash throughout), while the op re-gates the landing every
       * turn -- the invade gate, finally at the right seam. */
      deep_op_launch(ait, pplayer, op);
      /* The wave's own success can destroy the target: deep takes an
       * undefended city by walking into it, and a size-1 city is
       * removed outright rather than captured. deep_op_launch() already
       * re-resolves per member and stops when that happens -- but every
       * use of `target` from here down is the PRE-launch pointer, and
       * the march block falls through to them on this same call. Read
       * it again. */
      target = game_city_by_number(op->target_city);
      if (target == NULL) {
        deep_op_dissolve(op, oi, "target-gone", pplayer);
        return;
      }
      op->state = DEEP_OP_MARCH;
      op->march_turn = game.info.turn;
      op->march_eta = MAX(deep_op_eta(pplayer, op, target), 1);
      deep_brief_allies(pplayer, target, TRUE, op->march_eta);
      log_base(deep_metric_log_level(),
               "deep-op turn=%d player=\"%s\" op=%d march target=%d"
               " present=%d required=%d waited=%d eta=%d",
               game.info.turn, player_name(pplayer), oi, op->target_city,
               present, op->required, game.info.turn - op->formed_turn,
               op->march_eta);
      /* Fall through: the first march turn re-gates immediately. */
    } else if (game.info.turn - op->formed_turn >= tune->op_muster_wait) {
      /* Wait cap without even half a quorum: before banking, see if
       * the force that DID gather can take something else from
       * where it stands. */
      if (!deep_op_retarget(ait, pplayer, op, oi)) {
        deep_op_dissolve(op, oi, "abort-wait", pplayer);
      }
      return;
    } else {
      for (i = 0; i < op->n_members; i++) {
        struct unit *m = game_unit_by_number(op->members[i]);
        struct unit_ai *ud;

        if (m == NULL) {
          continue;
        }
        ud = def_ai_unit_data(m, ait);
        if (same_pos(unit_tile(m), city_tile(staging))) {
          ud->done = TRUE;      /* Hold at the muster point. */
        } else if (m->moves_left > 0) {
          if (dai_unit_goto(ait, m, city_tile(staging))) {
            ud->done = TRUE;
          }
        }
      }
      return;
    }
  }

  if (op->state == DEEP_OP_MARCH) {
    struct unit *stream[DEEP_OP_MEMBERS];
    int ns = 0, eta;

    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);

      if (m != NULL) {
        stream[ns++] = m;
      }
    }
    eta = deep_op_eta(pplayer, op, target);

    if (eta <= 1) {
      /* Contact: the wave stands at the walls. The op takes its
       * members back from the stock attack path and fights the
       * battle itself; the E7 second-wave window opens now. */
      if (deep_op_crossing(op, target) && ns > 0) {
        /* The landing is on: the crossing cleared every re-gate.
         * This is the invade gate's "go", at the seam where a
         * landing actually happens (#16). */
        log_base(deep_metric_log_level(),
                 "deep-invade turn=%d player=\"%s\" go target=%d"
                 " force=%d eta=%d p=%.3f",
                 game.info.turn, player_name(pplayer), op->target_city,
                 ns, eta,
                 deep_take_odds_at_arrival(nmap, target, stream, ns,
                                           eta));
      }
      log_base(deep_metric_log_level(),
               "deep-op turn=%d player=\"%s\" op=%d strike target=%d"
               " present=%d required=%d waited=%d",
               game.info.turn, player_name(pplayer), oi, op->target_city,
               ns, op->required, game.info.turn - op->march_turn);
      dp->last_strike_city = op->target_city;
      dp->last_strike_turn = game.info.turn;
      op->state = DEEP_OP_STRIKE;
      op->phase_turn = game.info.turn;
      /* Fall through to the STRIKE block: the battle starts this
       * turn -- the members have their moves, the manage loop has
       * not run yet. */
    } else {
      /* Re-gate: the odds the surviving wave takes the CURRENT
       * garrison, reinforced for the remaining travel time. Formation
       * priced this at op_take_target; the march holds until it falls
       * through the lower abort floor -- hysteresis, so a wave formed
       * at the bar does not turn back on the first wobble. Banked
       * members go back to stock with a clean slate. */
      if (tune->op_abort_floor > 0.0 && ns > 0) {
        double odds = deep_take_odds_at_arrival(nmap, target, stream, ns,
                                                eta);

        if (odds < tune->op_abort_floor) {
          bool crossing = deep_op_crossing(op, target);

          if (crossing) {
            /* The invade gate's "hold": the target reinforced past
             * the bar while we sailed, and the convoy does NOT land
             * into it (#16 -- the decision the port scan never got
             * to make). */
            log_base(deep_metric_log_level(),
                     "deep-invade turn=%d player=\"%s\" hold target=%d"
                     " force=%d eta=%d p=%.3f",
                     game.info.turn, player_name(pplayer),
                     op->target_city, ns, eta, odds);
          }
          if (deep_op_retarget(ait, pplayer, op, oi)) {
            return;   /* Re-mustered in the field toward a new goal. */
          }
          for (i = 0; i < ns; i++) {
            struct unit_ai *ud = def_ai_unit_data(stream[i], ait);

            if (ud->task == AIUNIT_ATTACK) {
              dai_unit_new_task(ait, stream[i], AIUNIT_NONE, NULL);
            }
          }
          log_base(deep_metric_log_level(),
                   "deep-op turn=%d player=\"%s\" op=%d regate target=%d"
                   " odds=%.2f eta=%d members=%d",
                   game.info.turn, player_name(pplayer), oi,
                   op->target_city, odds, eta, ns);
          deep_op_dissolve(op, oi, "abort-odds", pplayer);
          return;
        }
      }

      if (game.info.turn - op->march_turn
          >= op->march_eta + tune->op_muster_wait) {
        /* The wave has wandered: stock re-tasked members somewhere
         * the eta never closes. Same patience knob as the muster.
         * A retarget re-launches from wherever they drifted to. */
        if (!deep_op_retarget(ait, pplayer, op, oi)) {
          deep_op_dissolve(op, oi, "abort-march", pplayer);
        }
      }
      return;
    }
  }

  if (op->state == DEEP_OP_STRIKE) {
    deep_op_drive_strike(ait, pplayer, op, oi);
    return;
  }

  if (op->state == DEEP_OP_CONSOLIDATE) {
    deep_op_drive_consolidate(ait, pplayer, op, oi);
  }
}

/**********************************************************************//**
  The per-turn operation driver (combat E6; multi-op substrate,
  W33 commit 1): validate every live slot, form at most one new
  operation per turn -- the pool shrinks as live ops claim members,
  and a one-per-turn cadence keeps the single-op behaviour as the
  degenerate case when targets or attackers are scarce -- then drive
  every live operation.
**************************************************************************/
void deep_run_operations(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct deep_plr *dp = deep_plr_data(pplayer);
  int oi, i;

  if (tune->op_take_target <= 0.0) {
    return;
  }

  /* After a reload, marching members have lost their tasks (no
   * savegame carries them): re-point each at its op's target once,
   * exactly what launch did. One-shot -- after this, stock's leash
   * and the re-gate own the march again. */
  if (dp->ops_reissue) {
    dp->ops_reissue = FALSE;
    for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
      struct deep_operation *op = &dp->ops[oi];

      if (op->state != DEEP_OP_MARCH || op->kind != DEEP_OPK_ASSAULT
          || game_city_by_number(op->target_city) == NULL) {
        continue;
      }
      for (i = 0; i < op->n_members; i++) {
        /* Re-resolved per member for the same reason as in
         * deep_op_launch(): dai_gothere() below can end the target
         * city, freeing it under a hoisted pointer. */
        struct city *target = game_city_by_number(op->target_city);
        struct unit *m = game_unit_by_number(op->members[i]);

        if (target == NULL) {
          break;
        }
        if (m != NULL && unit_owner(m) == pplayer
            && def_ai_unit_data(m, ait)->task == AIUNIT_NONE) {
          dai_unit_new_task(ait, m, AIUNIT_ATTACK, city_tile(target));
          (void) dai_gothere(ait, pplayer, m, city_tile(target));
        }
      }
      log_base(deep_metric_log_level(),
               "deep-op turn=%d player=\"%s\" op=%d remarch target=%d"
               " members=%d",
               game.info.turn, player_name(pplayer), oi, op->target_city,
               op->n_members);
    }
  }

  /* Validate each live operation: prune dead members, drop the op
   * if the world moved on. */
  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    struct deep_operation *op = &dp->ops[oi];
    struct city *target, *staging;
    int n;

    if (op->state == DEEP_OP_NONE) {
      continue;
    }
    target = game_city_by_number(op->target_city);
    staging = game_city_by_number(op->staging_city);
    n = 0;
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);

      if (m != NULL && unit_owner(m) == pplayer && m->hp > 0) {
        op->members[n++] = op->members[i];
      }
    }
    op->n_members = n;
    if (op->kind == DEEP_OPK_RAID) {
      if (op->n_members == 0) {
        deep_op_dissolve(op, oi, "raid-done", pplayer);
      }
      continue;
    }
    if (op->kind == DEEP_OPK_LIFT) {
      /* Validation only; the crossing is driven in the drive pass
       * below, like every other kind. */
      if (op->n_members == 0) {
        deep_op_dissolve(op, oi, "lift-nobody", pplayer);
      }
      continue;
    }
    if (target != NULL && city_owner(target) == pplayer
        && (op->state == DEEP_OP_MARCH || op->state == DEEP_OP_STRIKE)) {
      /* The city fell to us: hold it. (A march can capture too --
       * its members fight on the stock attack path.) */
      op->state = DEEP_OP_CONSOLIDATE;
      op->phase_turn = game.info.turn;
      log_base(deep_metric_log_level(),
               "deep-op turn=%d player=\"%s\" op=%d consolidate"
               " target=%d members=%d",
               game.info.turn, player_name(pplayer), oi, op->target_city,
               op->n_members);
    }
    if (op->state == DEEP_OP_CONSOLIDATE) {
      if (target == NULL || city_owner(target) != pplayer) {
        deep_op_dissolve(op, oi, "abort", pplayer);
      } else if (op->n_members == 0) {
        /* Everyone converted to garrison (or died): the hold is
         * whatever it is; the op's work is over. */
        deep_op_dissolve(op, oi, "held", pplayer);
      }
      continue;
    }
    if (target == NULL
        || !pplayers_at_war(pplayer, city_owner(target))
        || op->n_members == 0
        /* A muster needs its staging city; a marching wave has
         * already left it behind. */
        || (op->state == DEEP_OP_MUSTER
            && (staging == NULL || city_owner(staging) != pplayer))) {
      /* The world moved on -- target razed, taken by a rival, peace
       * made, staging lost. The gathered force retargets from where
       * it stands when anything feasible remains. */
      if (op->n_members == 0
          || !deep_op_retarget(ait, pplayer, op, oi)) {
        deep_op_dissolve(op, oi, "abort", pplayer);
      }
    }
  }

  /* Form at most one assault and one raid per turn; assault first,
   * so the army's claim on fresh units outranks the hunt's. Fresh
   * raiders merge into a standing raid op rather than burning
   * another slot -- one hunt container per player. */
  {
    bool tried_assault = FALSE, tried_raid = FALSE;

    for (oi = 0; oi < DEEP_ARMY_OPS_MAX; oi++) {
      if (dp->ops[oi].state != DEEP_OP_NONE
          && dp->ops[oi].kind == DEEP_OPK_RAID) {
        int joined = deep_op_enroll_raiders(ait, pplayer, &dp->ops[oi]);

        tried_raid = TRUE;
        if (joined > 0) {
          dp->ops[oi].n_members += joined;
          log_base(deep_metric_log_level(),
                   "deep-op turn=%d player=\"%s\" op=%d raid members=%d",
                   game.info.turn, player_name(pplayer), oi, joined);
        }
        break;
      }
    }
    /* The army keeps exactly its four slots: it never forms into the
     * lift-only range, so adding that range cannot change where an
     * assault or a raid goes. */
    for (oi = 0; oi < DEEP_ARMY_OPS_MAX; oi++) {
      if (dp->ops[oi].state != DEEP_OP_NONE) {
        continue;
      }
      if (!tried_assault) {
        tried_assault = TRUE;
        if (deep_op_form(ait, pplayer, &dp->ops[oi], oi)) {
          continue;             /* The raid tries the next free slot. */
        }
      }
      if (!tried_raid) {
        (void) deep_op_form_raid(ait, pplayer, &dp->ops[oi], oi);
      }
      break;
    }
  }

  /* One lift per turn. Formed AFTER the army, so where the two pools
   * DO overlap -- a spare hull, or a military unit that could either
   * join an assault or be redeployed across the water -- the army's
   * claim wins and the lift takes what is left. deep_op_claiming() is
   * what enforces that: anything an assault already holds is invisible
   * to the lift's scan.
   *
   * Slot order: the lift's OWN range first, then the army's leftovers.
   * Preferring its own range is what keeps a lift from sitting in an
   * army slot for the eight turns of a muster and blocking an assault
   * that wants it, and taking leftovers afterwards means the extra
   * range only ever adds capacity. lift_op_slots == 0 leaves exactly
   * the old behaviour -- leftovers only -- which is the A/B switch. */
  if (tune->lift_form_radius > 0) {
    int slots = CLIP(0, tune->lift_op_slots, DEEP_LIFT_OPS_MAX);
    bool formed = FALSE;

    for (oi = DEEP_ARMY_OPS_MAX; oi < DEEP_ARMY_OPS_MAX + slots; oi++) {
      if (dp->ops[oi].state == DEEP_OP_NONE) {
        formed = deep_op_form_lift(ait, pplayer, &dp->ops[oi], oi);
        break;
      }
    }
    for (oi = 0; !formed && oi < DEEP_ARMY_OPS_MAX; oi++) {
      if (dp->ops[oi].state == DEEP_OP_NONE) {
        (void) deep_op_form_lift(ait, pplayer, &dp->ops[oi], oi);
        break;
      }
    }
  }

  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    if (dp->ops[oi].state == DEEP_OP_NONE) {
      continue;
    }
    if (dp->ops[oi].kind == DEEP_OPK_RAID) {
      deep_op_drive_raid(ait, pplayer, &dp->ops[oi], oi);
    } else if (dp->ops[oi].kind == DEEP_OPK_LIFT) {
      deep_op_drive_lift(ait, pplayer, &dp->ops[oi], oi);
    } else {
      deep_op_drive(ait, pplayer, &dp->ops[oi], oi);
    }
  }
}
