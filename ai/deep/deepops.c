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
#include "unithand.h"
#include "unittools.h"

/* server/advisors */
#include "advbuilding.h"
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
#include "deepdefence.h"
#include "deepdiplomacy.h"
#include "deepstance.h"
#include "deeptuning.h"

/* server */
#include "notify.h"

#include "deephorizon.h"
#include "deeptrade.h"

#include "deepops.h"

/* server/advisors */
#include "advdata.h"

/* get_civ_score()'s own exchange rate between culture and score
 * (server/score.c:361: `+ pplayer->score.culture / 50`). The same
 * constant deephorizon.c's culture credit reads, and for the same
 * reason: it is the rate the benchmark itself is scored at. */
#define DEEP_CULTURE_PER_SCORE 50

/* BALANCE census: how many conquest valuations carried a culture
 * component, and what the component summed to. Read alongside
 * `deep-netval` and the assault want, because the term is a bid
 * modifier and never appears as a line of its own. */
static int deep_conq_culture_cands = 0;
static double deep_conq_culture_paid = 0.0;

/**********************************************************************//**
  What taking `target` is worth to `pplayer`, in the want currency
  stock's own city worth is denominated in.

  Two components, both applied ONCE here so that every conquest seat
  reads the same number:

  - `worth x conquest_worth_mult`: the take plus the denial (M1 rider
    2, human call 2026-08-06). Unchanged; this function only gathers
    the three call sites that already computed it.

  - The CULTURE STOCK the city carries. `pcity->history` is not
    recomputed under a new owner and transfer_city() never touches it
    (server/citytools.c:1077-, which changes `pcity->owner` on the
    same struct), so a captured city brings its whole accumulated
    history across: the Europe derby's Carthago inherited Gdansk's
    2445 and grew it (derby N2). player_culture() sums city_culture()
    over the cities a player owns, and get_civ_score() pays culture at
    1/50th of a point -- so the stock is a score asset that changes
    hands whole and nothing in deep or in ai/default has ever priced
    it. The exchange is the score's own: history / 50 score points,
    each worth what deep pays for the citizen that scores exactly one
    point, which is the identical chain deephorizon.c's culture credit
    uses for the flow.

  What is deliberately NOT counted: city_culture()'s EFT_PERFORMANCE
  term, which is an instantaneous function of the buildings standing
  and is recomputed under the new owner rather than transferred, and
  the flow the city will earn afterwards, which is the building
  credit's business and would be double-counted here.

  The citizen price is read at the TARGET with our own adv weights --
  what a citizen there would be worth to us. That inherits the base's
  own omniscience (census finding #4: stock's `worth` is an
  omniscient read of an enemy city) and no more; W04's estimator
  replaces the base and this term with it.
**************************************************************************/
double deep_conquest_worth(struct ai_type *ait, struct player *pplayer,
                           struct city *target)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double worth = def_ai_city_data(target, ait)->worth
                 * tune->conquest_worth_mult;
  struct adv_data *adv;
  double culture;

  if (tune->conquest_culture_gain <= 0.0 || target->history <= 0) {
    return worth;
  }
  adv = adv_data_get(pplayer, NULL);
  if (adv == NULL) {
    return worth;
  }
  culture = (double) target->history / DEEP_CULTURE_PER_SCORE
            * deep_unworked_tile_value(target, adv, 3, NULL)
            * tune->conquest_culture_gain;
  if (culture <= 0.0) {
    return worth;
  }
  deep_conq_culture_cands++;
  deep_conq_culture_paid += culture;

  return worth + culture;
}

/**********************************************************************//**
  The culture component's execution counter, emitted once per player
  per turn by the operations pass.
**************************************************************************/
void deep_conquest_culture_census(struct player *pplayer)
{
  if (deep_conq_culture_cands <= 0) {
    return;
  }
  log_base(deep_metric_log_level(),
           "deep-conqculture turn=%d player=\"%s\" priced=%d paid=%.1f",
           game.info.turn, player_name(pplayer),
           deep_conq_culture_cands, deep_conq_culture_paid);
  deep_conq_culture_cands = 0;
  deep_conq_culture_paid = 0.0;
}

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
  Is this unit committed to an intention OTHER than taking `city_id`?

  The build side prices the marginal attacker against a stream of
  units, and its stream was every free attacker on the continent
  regardless of what any of them was already doing -- so with four
  concurrent operations the marginal blade was priced against a force
  that will never assemble anywhere, P(take) already looked high, and
  attacker production stopped exactly when the campaigns ran
  (military audit C4). The stream that WILL fight for a given city is
  the unclaimed attackers plus the members of the operation aimed at
  it; a raider, a lift passenger or another op's wave is not in it.

  No constant: the claim registry already knows.
**************************************************************************/
bool deep_op_claimed_elsewhere(const struct player *pplayer, int unit_id,
                               int city_id)
{
  int oi = deep_op_claiming(pplayer, unit_id);

  if (oi < 0) {
    return FALSE;
  }

  return deep_plr_data(pplayer)->ops[oi].target_city != city_id;
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

/* This phase's non-operation holds.
 *
 * deep_manage_units() has to reset unit_ai.done -- the flag is stale
 * from last turn, and stock's dai_manage_units() clears it for exactly
 * that reason -- but the passes that run BEFORE it express their
 * decisions through that same flag: the tired-attack hold, the convoy
 * HOLD, the worker flee mark and the garrison a consolidating assault
 * just converted. An unconditional reset erased all four every turn,
 * so each of them logged a decision it never took (mechanism-map D-A).
 *
 * The distinction the reset needs is not "set" but "set THIS phase",
 * which the flag cannot carry, so the holds are recorded here. Two
 * independent drivers consume them: deep_manage_units(), and the
 * settler entry point, because auto_workers_player() reaches every
 * worker of an AI player at end of phase whatever the flag says
 * (deepsettler.c:1484).
 *
 * Each entry also keeps what the unit looked like when it was held. A
 * hold that is never observed holding is precisely the failure this
 * fix exists to end, so deep_hold_census() reports the same unit again
 * once every driver has run. */
struct deep_hold {
  int plr_no;
  int unit_id;
  int turn;
  const char *why;              /* Static string: the mechanism. */
  int tile;                     /* Where it stood when held. */
  int moves_left;
  enum ai_unit_task task;
  int cargo;
};

static struct deep_hold *deep_holds = NULL;
static int deep_holds_n = 0;
static int deep_holds_cap = 0;

/**********************************************************************//**
  Is this unit held by one of THIS player's THIS-turn phase holds?
**************************************************************************/
bool deep_phase_held(const struct player *pplayer, int unit_id)
{
  int i;

  for (i = 0; i < deep_holds_n; i++) {
    if (deep_holds[i].unit_id == unit_id
        && deep_holds[i].plr_no == player_number(pplayer)
        && deep_holds[i].turn == game.info.turn) {
      return TRUE;
    }
  }

  return FALSE;
}

/**********************************************************************//**
  Forget this player's holds. Called at phase begin, which is what
  makes the marker phase-scoped: a hold expresses one turn's decision
  and may never survive into the next one.
**************************************************************************/
void deep_phase_holds_clear(const struct player *pplayer)
{
  int i = 0;

  while (i < deep_holds_n) {
    if (deep_holds[i].plr_no == player_number(pplayer)) {
      deep_holds[i] = deep_holds[--deep_holds_n];   /* swap-pop */
    } else {
      i++;
    }
  }
}

/**********************************************************************//**
  Hold this unit for the rest of the phase: nothing else re-tasks it,
  and the census can check afterwards that nothing did. `why` names
  the mechanism and must be a literal.
**************************************************************************/
void deep_hold_unit(struct ai_type *ait, struct unit *punit,
                    const char *why)
{
  const struct player *pplayer = unit_owner(punit);
  struct deep_hold *h;

  def_ai_unit_data(punit, ait)->done = TRUE;
  if (deep_phase_held(pplayer, punit->id)) {
    return;   /* One entry per unit per phase; the first reason wins. */
  }
  if (deep_holds_n >= deep_holds_cap) {
    deep_holds_cap = MAX(64, deep_holds_cap * 2);
    deep_holds = fc_realloc(deep_holds,
                            deep_holds_cap * sizeof(*deep_holds));
  }
  h = &deep_holds[deep_holds_n++];
  h->plr_no = player_number(pplayer);
  h->unit_id = punit->id;
  h->turn = game.info.turn;
  h->why = why;
  h->tile = tile_index(unit_tile(punit));
  h->moves_left = punit->moves_left;
  h->task = def_ai_unit_data(punit, ait)->task;
  h->cargo = get_transporter_occupancy(punit);
}

/**********************************************************************//**
  Report every hold of this player's phase, AFTER the manage loop and
  the auto-worker pass have both run: alive, still held, still where it
  was, still on the task it was given. "Held" and "actually held" are
  different claims, and only this line tells them apart.
**************************************************************************/
void deep_hold_census(struct ai_type *ait, struct player *pplayer)
{
  int i;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  for (i = 0; i < deep_holds_n; i++) {
    const struct deep_hold *h = &deep_holds[i];
    struct unit *punit;

    if (h->plr_no != player_number(pplayer)
        || h->turn != game.info.turn) {
      continue;
    }
    punit = game_unit_by_number(h->unit_id);
    log_base(deep_metric_log_level(),
             "deep-hold turn=%d player=\"%s\" unit=%d why=%s alive=%d"
             " same_tile=%d moved=%d spent=%d same_task=%d cargo=%d/%d",
             game.info.turn, player_name(pplayer), h->unit_id, h->why,
             punit != NULL ? 1 : 0,
             punit != NULL && tile_index(unit_tile(punit)) == h->tile
               ? 1 : 0,
             punit != NULL && tile_index(unit_tile(punit)) != h->tile
               ? 1 : 0,
             punit != NULL ? h->moves_left - punit->moves_left : -1,
             punit != NULL
               && def_ai_unit_data(punit, ait)->task == h->task ? 1 : 0,
             punit != NULL ? get_transporter_occupancy(punit) : -1,
             h->cargo);
  }
}

/**********************************************************************//**
  Capturable-adjacency census, both directions, once per player per
  turn (S-GROUND item 3, instrument-before-mechanism).

  "Capture Units" is performed by stock only as the first branch of a
  fixed else-if chain reached AFTER the decision to attack
  (`daitools.c:835-839`); no code seeks a capturable target and nothing
  protects our own capturables. Before either half is built, the
  population is measured: how often does an enemy capturer stand
  adjacent to a stack of ours it could take THIS INSTANT, and how often
  do we stand adjacent to one of theirs.

  Tiles, not units, are the unit of count on both sides, because the
  action takes a whole stack and the hard requirement is that EVERY
  unit on the target tile is capturable -- `is_action_enabled_unit_on_
  stack()` answers exactly that question, so a defended stack is
  correctly not counted. Shields are the stack's summed build cost:
  the thing actually at stake.

  The ruleset gate is `action_is_in_use()`: Capture Units is
  civ2civ3-only, so a classic cell emits ZERO lines and the venue
  split is visible in the census itself rather than assumed.

  Omniscient (RC6), like the rest of this module's censuses -- it reads
  enemy unit positions directly. A known-map version would filter by
  `map_is_known_and_seen()`; the census is measuring the ceiling.
**************************************************************************/
static bool deep_capture_gate_said = FALSE;

void deep_capture_census(struct ai_type *ait, struct player *pplayer)
{
  const struct civ_map *nmap = &(wld.map);
  struct action *pact;
  int exp_tiles = 0, exp_units = 0, exp_shields = 0, exp_threats = 0;
  int off_tiles = 0, off_units = 0, off_shields = 0, off_actors = 0;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }
  pact = action_by_number(ACTION_CAPTURE_UNITS);
  if (pact == NULL || !action_is_in_use(pact)) {
    /* Not this ruleset. Silence IS the finding -- but silence has two
     * causes and only one of them is interesting, so the gate says
     * which, once per game. */
    if (!deep_capture_gate_said) {
      deep_capture_gate_said = TRUE;
      log_base(deep_metric_log_level(),
               "deep-capture-venue turn=%d inuse=0",
               game.info.turn);
    }
    return;
  }
  if (!deep_capture_gate_said) {
    deep_capture_gate_said = TRUE;
    log_base(deep_metric_log_level(),
             "deep-capture-venue turn=%d inuse=1", game.info.turn);
  }

  /* Ours, exposed. */
  unit_list_iterate(pplayer->units, punit) {
    struct tile *ptile = unit_tile(punit);
    int threats = 0;

    if (unit_list_get(ptile->units, 0) != punit) {
      continue;                 /* Count each tile once. */
    }
    adjc_iterate(nmap, ptile, atile) {
      unit_list_iterate(atile->units, e) {
        struct player *eowner = unit_owner(e);

        if (eowner == pplayer || pplayers_allied(pplayer, eowner)) {
          continue;
        }
        if (is_action_enabled_unit_on_stack(nmap, ACTION_CAPTURE_UNITS,
                                            e, ptile)) {
          threats++;
        }
      } unit_list_iterate_end;
    } adjc_iterate_end;

    if (threats > 0) {
      exp_tiles++;
      exp_threats += threats;
      unit_list_iterate(ptile->units, victim) {
        exp_units++;
        exp_shields += unit_build_shield_cost_base(victim);
      } unit_list_iterate_end;
    }
  } unit_list_iterate_end;

  /* Theirs, on offer. */
  players_iterate_alive(aplayer) {
    if (aplayer == pplayer || pplayers_allied(pplayer, aplayer)) {
      continue;
    }
    unit_list_iterate(aplayer->units, e) {
      struct tile *etile = unit_tile(e);
      int actors = 0;

      if (unit_list_get(etile->units, 0) != e) {
        continue;
      }
      adjc_iterate(nmap, etile, atile) {
        unit_list_iterate(atile->units, mine) {
          if (unit_owner(mine) != pplayer) {
            continue;
          }
          if (is_action_enabled_unit_on_stack(nmap, ACTION_CAPTURE_UNITS,
                                              mine, etile)) {
            actors++;
          }
        } unit_list_iterate_end;
      } adjc_iterate_end;

      if (actors > 0) {
        off_tiles++;
        off_actors += actors;
        unit_list_iterate(etile->units, prize) {
          off_units++;
          off_shields += unit_build_shield_cost_base(prize);
        } unit_list_iterate_end;
      }
    } unit_list_iterate_end;
  } players_iterate_alive_end;

  if (exp_tiles > 0 || off_tiles > 0) {
    log_base(deep_metric_log_level(),
             "deep-capture turn=%d player=\"%s\" exptiles=%d expunits=%d"
             " expshields=%d expthreats=%d offtiles=%d offunits=%d"
             " offshields=%d offactors=%d units=%d",
             game.info.turn, player_name(pplayer), exp_tiles, exp_units,
             exp_shields, exp_threats, off_tiles, off_units, off_shields,
             off_actors, unit_list_size(pplayer->units));
  }
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

/**********************************************************************//**
  One line per HULL per turn, against `deep-lift`'s one line per fleet.

  The fleet census cannot answer the question the epic is now on. Five
  interventions moved the fleet between 4.57 and 6.24 hulls per
  player-turn and left the idle share pinned at 75-80% every time, and
  cutting hulls cut arrivals with it -- which says the removed boats
  were about to be useful and indicts the per-turn snapshot itself. A
  count cannot tell "idle" from "idle now, sailing next turn", and no
  aggregate over hulls can say WHY any particular hull is idle.

  So this emits the state, keyed on unit id, that lets a lifetime be
  reconstructed offline: when the boat was born, what it holds, what
  claims it, which body of water it sits on, and how many units are
  waiting to cross THAT water. The three causes a count conflates then
  separate -- too many boats (demand elsewhere is served), wrong
  harbour (demand never appears on its water at all), demand
  evaporated (demand was there and stopped) -- and each has a
  different fix.

  Every component of the idle predicate is emitted separately rather
  than only its conjunction, so the predicate can be varied in
  analysis without another 20-game batch.

  `bridge` is the geometry rather than the demand: 1 when some water
  this hull touches reaches a second land continent at all. It sizes
  what a build-time gate could refuse, and unlike the demand fields it
  cannot be an accident of the turn it was sampled on.
**************************************************************************/
static void deep_hull_census(struct ai_type *ait, struct player *pplayer)
{
  const struct civ_map *nmap = &(wld.map);

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  unit_list_iterate(pplayer->units, pferry) {
    const struct unit_ai *ud = def_ai_unit_data(pferry, ait);
    struct tile *ptile = unit_tile(pferry);
    Continent_id seen[8], best = 0;
    int nseen = 0, demand = 0, best_demand = -1, nearest = -1, oc;
    int bridge = 0;

    if (!dai_is_ferry(pferry, ait)) {
      continue;
    }

    /* The waters this hull can actually serve. A docked boat stands on
     * the city's LAND tile, so its own continent is the wrong answer
     * for exactly the boats that are idle -- that bug is why the
     * per-ocean damping silently never fired. A hull belongs to the
     * water it TOUCHES. */
    oc = tile_continent(ptile);
    if (oc < 0) {
      seen[nseen++] = oc;
    }
    adjc_iterate(nmap, ptile, atile) {
      Continent_id ac = tile_continent(atile);
      int si;

      if (ac >= 0) {
        continue;
      }
      for (si = 0; si < nseen; si++) {
        if (seen[si] == ac) {
          break;
        }
      }
      if (si < nseen || nseen >= (int) ARRAY_SIZE(seen)) {
        continue;
      }
      seen[nseen++] = ac;
    } adjc_iterate_end;

    for (oc = 0; oc < nseen; oc++) {
      int d = deep_ferry_demand_near(ait, pplayer, seen[oc], ptile, &nearest);

      demand += d;
      if (deep_water_bridges(seen[oc])) {
        bridge = 1;
      }
      if (d > best_demand) {
        best_demand = d;
        best = seen[oc];
      }
    }

    log_base(deep_metric_log_level(),
             "deep-hull-turn turn=%d player=\"%s\" unit=%d type=\"%s\""
             " born=%d at=(%d,%d) incity=%d ocean=%d waters=%d"
             " cargo=%d psngr=%d task=%d ssa=%d op=%d"
             " demand=%d neard=%d idle=%d bridge=%d",
             game.info.turn, player_name(pplayer), pferry->id,
             unit_rule_name(pferry), pferry->birth_turn,
             TILE_XY(ptile), tile_city(ptile) != NULL ? 1 : 0,
             (int) best, nseen,
             get_transporter_occupancy(pferry), ud->passenger,
             (int) ud->task, (int) pferry->ssa_controller,
             deep_op_claiming(pplayer, pferry->id),
             demand, nearest,
             get_transporter_occupancy(pferry) == 0
             && ud->passenger < 0
             && ud->task != AIUNIT_EXPLORE
             && pferry->ssa_controller != SSA_AUTOEXPLORE
             && deep_op_claiming(pplayer, pferry->id) < 0 ? 1 : 0,
             bridge);
  } unit_list_iterate_end;
}

/**********************************************************************//**
  Turns from now until this city finishes `ptype`. -1 when it never
  finishes (no shield surplus) and -2 when it cannot build the type at
  all.

  Deliberately the crudest possible estimate -- stock plus surplus, no
  growth, no waste, no buy -- because that is exactly what W36's award
  step would have at order time, and sizing the award on a better
  estimate than the award could use would overstate the machinery.

  `mode` picks which stock the candidate is credited with, and the
  three answers are genuinely different questions:

    DEEP_ETA_STOCK    everything in the box. Right for the city that
                      is ALREADY building the hull.
    DEEP_ETA_SWITCH   what survives switching production to it, via
                      city_change_production_penalty(). Right for a
                      rival, and the correction that matters: a city
                      holding 200 shields for a wonder shows an ETA of
                      0 for a 20-shield hull under DEEP_ETA_STOCK, and
                      awarding the commission there spends a wonder.
    DEEP_ETA_FRESH    no stock at all, cost/surplus. The floor: how
                      fast this city PRODUCES, with nothing borrowed
                      from what it was already doing.
**************************************************************************/
enum deep_eta_mode { DEEP_ETA_STOCK, DEEP_ETA_SWITCH, DEEP_ETA_FRESH };

static int deep_build_eta(const struct civ_map *nmap, struct city *pcity,
                          const struct unit_type *ptype,
                          enum deep_eta_mode mode)
{
  struct universal target = { .kind = VUT_UTYPE, .value.utype = ptype };
  int cost, stock, left;

  if (!can_city_build_unit_now(nmap, pcity, ptype, RPT_CERTAIN)) {
    return -2;
  }
  cost = utype_build_shield_cost(pcity, NULL, ptype);
  switch (mode) {
  case DEEP_ETA_STOCK:
    stock = pcity->shield_stock;
    break;
  case DEEP_ETA_SWITCH:
    stock = city_change_production_penalty(pcity, &target);
    break;
  default:
    stock = 0;
    break;
  }
  left = cost - stock;
  if (left <= 0) {
    return 0;
  }
  if (pcity->surplus[O_SHIELD] <= 0) {
    return -1;
  }

  return (left + pcity->surplus[O_SHIELD] - 1) / pcity->surplus[O_SHIELD];
}

/**********************************************************************//**
  Log one hull order with the award comparison W36 step 0 needs: this
  city's own completion ETA against the best a rival on the SAME water
  could offer, both as it would arrive after a production switch and as
  a fresh build.

  `when` separates the two vantages, because they answer different
  halves of the question and neither alone is enough:

    choice   the turn the build decision was COMMITTED. The order
             event proper, and the only vantage that sees a hull the
             city buys outright -- which the production census cannot,
             since a bought hull is ordered and delivered inside one
             turn and never stands in production when units are
             managed.
    prod     production is standing on a hull this turn. Gives the
             order a LENGTH: how long it stood, and therefore how long
             the demand behind it had to survive.
**************************************************************************/
static void deep_hull_order_log(struct ai_type *ait, struct player *pplayer,
                                struct city *pcity,
                                const struct unit_type *ptype,
                                const char *when)
{
  const struct civ_map *nmap = &(wld.map);
  struct tile *ptile = city_tile(pcity);
  Continent_id seen[8];
  int nseen = 0, ncand = 0, best_eta = -1, best_fresh = -1, best_city = 0;
  int bridge = 0;

  /* The waters this city can launch into, deduplicated: the same
   * adjacency the gate and the hull census use. */
  adjc_iterate(nmap, ptile, atile) {
    Continent_id ac = tile_continent(atile);
    int si;

    if (ac >= 0) {
      continue;
    }
    for (si = 0; si < nseen; si++) {
      if (seen[si] == ac) {
        break;
      }
    }
    if (si < nseen || nseen >= (int) ARRAY_SIZE(seen)) {
      continue;
    }
    seen[nseen++] = ac;
    if (deep_water_bridges(ac)) {
      bridge = 1;
    }
  } adjc_iterate_end;

  city_list_iterate(pplayer->cities, other) {
    bool shares = FALSE;
    int eta, fresh;

    if (other == pcity) {
      continue;
    }
    adjc_iterate(nmap, city_tile(other), atile) {
      int si;

      for (si = 0; si < nseen; si++) {
        if (seen[si] == tile_continent(atile)) {
          shares = TRUE;
          break;
        }
      }
      if (shares) {
        break;
      }
    } adjc_iterate_end;
    if (!shares) {
      continue;
    }
    eta = deep_build_eta(nmap, other, ptype, DEEP_ETA_SWITCH);
    fresh = deep_build_eta(nmap, other, ptype, DEEP_ETA_FRESH);
    if (eta < 0) {
      continue;                 /* Cannot build it, or never finishes. */
    }
    ncand++;
    if (best_eta < 0 || eta < best_eta) {
      best_eta = eta;
      best_city = other->id;
    }
    if (fresh >= 0 && (best_fresh < 0 || fresh < best_fresh)) {
      best_fresh = fresh;
    }
  } city_list_iterate_end;

  log_base(deep_metric_log_level(),
           "deep-hull-order turn=%d player=\"%s\" city=%d type=\"%s\""
           " when=%s stock=%d surplus=%d cost=%d eta=%d waters=%d"
           " bridge=%d ncand=%d best=%d besteta=%d bestfresh=%d",
           game.info.turn, player_name(pplayer), pcity->id,
           utype_rule_name(ptype), when, pcity->shield_stock,
           pcity->surplus[O_SHIELD],
           utype_build_shield_cost(pcity, NULL, ptype),
           deep_build_eta(nmap, pcity, ptype, DEEP_ETA_STOCK), nseen,
           bridge, ncand, best_city, best_eta, best_fresh);
}

/**********************************************************************//**
  The order log at its committing vantage: called from the build
  choice once the winner is known, so a hull the city then BUYS is
  still on the record.
**************************************************************************/
void deep_hull_order_choice(struct ai_type *ait, struct player *pplayer,
                            struct city *pcity,
                            const struct unit_type *ptype)
{
  if (!log_do_output_for_level(deep_metric_log_level())
      || !dai_is_ferry_type(ptype, ait)) {
    return;
  }
  deep_hull_order_log(ait, pplayer, pcity, ptype, "choice");
}

/**********************************************************************//**
  One line per CITY BUILDING A HULL per turn -- W36 step 0's first
  question, and the one thing the hull census cannot answer.

  The census reconstructs a hull's life from its birth; the order that
  produced it is older than that, and nothing has ever logged it. Two
  things follow that the birth turn cannot give: whether the demand the
  order answered still existed when the hull appeared, and -- the
  question here -- whether the city that placed it was the RIGHT
  producer.

  So each line carries the ordering city's own completion ETA and the
  best ETA available at any other city of this player that shares a
  body of water with it and could build the same hull today. If those
  two are usually equal, W36's award step is ceremony: the order would
  land at the same city anyway, and only cancellation and the ETA
  handed to the passenger are worth building.

  No per-city state is kept, deliberately. A line is emitted every turn
  production stands on a hull, so an ORDER is the first line of a
  consecutive run for one (city, type) and the run length is how long
  the order stood -- which offline reconstruction gets for free and
  which cannot go stale across a save/load the way a stored order turn
  would.

  `best` is scored against the same type the ordering city chose rather
  than each candidate's own best ferry role: the question is who should
  have built THIS hull, and a candidate that can only build a lesser
  hull is not the same offer.
**************************************************************************/
static void deep_order_census(struct ai_type *ait, struct player *pplayer)
{
  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  city_list_iterate(pplayer->cities, pcity) {
    if (pcity->production.kind == VUT_UTYPE
        && dai_is_ferry_type(pcity->production.value.utype, ait)) {
      deep_hull_order_log(ait, pplayer, pcity,
                          pcity->production.value.utype, "prod");
    }
  } city_list_iterate_end;
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
  One line per WAITING PASSENGER per turn: W36 step 0's other two
  questions, which NEXT-SESSION section 1d has now asked three times.

  The wait -- turns from "first wanted a boat" to enrolled as a lift's
  core -- is the honest price of the fleet's remaining idleness, and
  the reason hitching cannot be priced without it: `deep_lift_enroll()`
  decides on FEASIBILITY ("can I reach the rendezvous") because the
  COMPARISON ("what would I get by waiting for my own boat") has no
  number in it. Reconstructed offline as first-seen here against the
  turn this unit id appears as `psngr` on a `deep-lift-op ... form`
  line; units that never appear are the censored tail and must be
  reported as such rather than dropped.

  Claimed passengers are logged too, and that is not noise: the line
  the wait ENDS on is the one that says it ended, and a claim that
  dissolves resumes the same wait.

  `boats` sizes the demand that finds no vehicle -- the `lift-noboat`
  population -- and `wcity`/`pcity` size the demand that finds no
  PRODUCER, which is the failure W36's award step would have to handle
  and a different thing entirely: a commission with no candidate city
  cannot fall through to a next-best one.
**************************************************************************/
static void deep_wait_census(struct ai_type *ait, struct player *pplayer)
{
  const struct civ_map *nmap = &(wld.map);
  struct unit_type *boattype;
  int pcities = 0;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  /* Producers that exist ANYWHERE for this player: a city that can
   * build a hull today into water that reaches another continent. The
   * gate's own geometry, asked of the builder rather than the bid. */
  boattype = best_role_unit_for_player(pplayer, L_FERRYBOAT);
  if (boattype != NULL) {
    city_list_iterate(pplayer->cities, pcity) {
      if (can_city_build_unit_now(nmap, pcity, boattype, RPT_CERTAIN)
          && deep_city_water_bridges(city_tile(pcity))) {
        pcities++;
      }
    } city_list_iterate_end;
  }

  unit_list_iterate(pplayer->units, punit) {
    Continent_id seen[8];
    int nseen = 0, boats = 0, nearest = -1, wcities = 0;

    if (!deep_lift_wants_boat(ait, punit)
        || tile_continent(unit_tile(punit))
           == tile_continent(punit->goto_tile)) {
      continue;
    }

    unit_list_iterate(pplayer->units, pferry) {
      int d;

      if (!dai_is_ferry(pferry, ait)
          || get_transporter_occupancy(pferry)
             >= get_transporter_capacity(pferry)
          || !could_unit_load(punit, pferry)) {
        continue;
      }
      boats++;
      d = real_map_distance(unit_tile(punit), unit_tile(pferry));
      if (nearest < 0 || d < nearest) {
        nearest = d;
      }
    } unit_list_iterate_end;

    /* Producers on the passenger's OWN water: the candidate set an
     * award would draw from if the commission were raised where the
     * passenger stands. Zero for an inland passenger, which is itself
     * a finding -- the water that matters is the one it will embark
     * from, not the one it is beside today. */
    adjc_iterate(nmap, unit_tile(punit), atile) {
      Continent_id ac = tile_continent(atile);
      int si;

      if (ac >= 0) {
        continue;
      }
      for (si = 0; si < nseen; si++) {
        if (seen[si] == ac) {
          break;
        }
      }
      if (si < nseen || nseen >= (int) ARRAY_SIZE(seen)) {
        continue;
      }
      seen[nseen++] = ac;
    } adjc_iterate_end;

    if (boattype != NULL && nseen > 0) {
      city_list_iterate(pplayer->cities, pcity) {
        bool shares = FALSE;

        if (!can_city_build_unit_now(nmap, pcity, boattype, RPT_CERTAIN)) {
          continue;
        }
        adjc_iterate(nmap, city_tile(pcity), atile) {
          int si;

          for (si = 0; si < nseen; si++) {
            if (seen[si] == tile_continent(atile)) {
              shares = TRUE;
              break;
            }
          }
          if (shares) {
            break;
          }
        } adjc_iterate_end;
        if (shares) {
          wcities++;
        }
      } city_list_iterate_end;
    }

    log_base(deep_metric_log_level(),
             "deep-wait turn=%d player=\"%s\" unit=%d type=\"%s\" born=%d"
             " at=(%d,%d) goto=(%d,%d) claimed=%d boats=%d near=%d"
             " wcity=%d pcity=%d",
             game.info.turn, player_name(pplayer), punit->id,
             unit_rule_name(punit), punit->birth_turn,
             TILE_XY(unit_tile(punit)), TILE_XY(punit->goto_tile),
             deep_op_claiming(pplayer, punit->id), boats, nearest,
             wcities, pcities);
  } unit_list_iterate_end;
}

/**********************************************************************//**
  Refresh this player's live wait spells: who is waiting for a
  crossing, and since when.

  The wait census beside this writes the same population to the log and
  a spell is reconstructed offline; this keeps it in memory so a
  DECISION can read it. Observation only -- nothing here changes a
  want, a task or a unit -- and unconditional on purpose: the estimate
  must be readable in the off arm too, or the arms differ by more than
  the knob they are meant to isolate.

  A unit is waiting when deep_lift_wants_boat() holds, its goal is on
  another landmass, and no operation has claimed it. The claim is what
  ENDS a spell, matching the offline definition (first-seen against
  enrolment as a lift's core); a claim that dissolves starts the unit's
  next spell at the turn it dissolved, which understates that unit's
  total wait and never overstates it.
**************************************************************************/
void deep_wait_spells_update(struct ai_type *ait, struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  int live = 0, longest = 0, i;

  unit_list_iterate(pplayer->units, punit) {
    int freeslot = -1;
    bool tracked = FALSE;

    if (!deep_lift_wants_boat(ait, punit)
        || tile_continent(unit_tile(punit))
           == tile_continent(punit->goto_tile)
        || deep_op_claiming(pplayer, punit->id) >= 0) {
      continue;
    }
    for (i = 0; i < DEEP_WAIT_SPELLS; i++) {
      if (dp->wait_spells[i].unit_id == punit->id) {
        dp->wait_spells[i].seen = game.info.turn;
        tracked = TRUE;
        break;
      }
      if (dp->wait_spells[i].unit_id == 0 && freeslot < 0) {
        freeslot = i;
      }
    }
    if (!tracked && freeslot >= 0) {
      dp->wait_spells[freeslot].unit_id = punit->id;
      dp->wait_spells[freeslot].since = game.info.turn;
      dp->wait_spells[freeslot].seen = game.info.turn;
    }
  } unit_list_iterate_end;

  /* Retire every spell not seen this turn: picked up, re-aimed, or
   * dead. Unit ids are not reused within a game, so a stale slot can
   * never be mistaken for a fresh waiter. */
  for (i = 0; i < DEEP_WAIT_SPELLS; i++) {
    if (dp->wait_spells[i].unit_id == 0) {
      continue;
    }
    if (dp->wait_spells[i].seen < game.info.turn) {
      dp->wait_spells[i].unit_id = 0;
      continue;
    }
    live++;
    longest = MAX(longest, game.info.turn - dp->wait_spells[i].since);
  }

  if (live > 0) {
    log_base(deep_metric_log_level(),
             "deep-waitspell turn=%d player=\"%s\" live=%d longest=%d",
             game.info.turn, player_name(pplayer), live, longest);
  }
}

static int deep_wait_cmp(const void *a, const void *b)
{
  return *(const int *) a - *(const int *) b;
}

/**********************************************************************//**
  The wait, in turns, a unit joining the queue on this water should
  expect.

  Two questions, in this order.

  1. Is a hull standing free FOR ME? Idle hulls on this water beyond
     the units already waiting on it -- the same demand/supply pair the
     colony want is damped by (deep_idle_ferries_on,
     deep_ferry_demand_on), asked of the newcomer rather than of the
     shipyard. Strictly greater, because the newcomer joins the back of
     that queue: one spare hull with one waiter ahead is not a hull for
     me. If there is one, the wait is 0 and the winner's own
     ferry_wait amortization already prices the fetch.

  2. Otherwise, how long is this water making people wait? The TAIL of
     the live spells it holds, at lift_wait_tail_pct. Deliberately the
     tail and not the mean: a fifth of spells hold three fifths of all
     waiting (findings/commission-sizing.md), so the mean describes
     nobody -- most crossings are quick and the ones that are not are
     the ones that cost a colony. The sample is right-censored and
     length-biased by construction (a spell that is still running is
     over-represented in a snapshot of running spells), and that bias
     points the same way as the statistic being estimated.

  Zero when the water holds no spell at all: no queue has been
  observed, so there is no wait to claim. That is not "no wait ever" --
  it is the honest answer of an estimator with no sample, and it is why
  a fresh empire's first crossing is priced by travel alone.
**************************************************************************/
int deep_wait_expected(struct ai_type *ait, struct player *pplayer,
                       Continent_id ocean)
{
  const struct civ_map *nmap = &(wld.map);
  struct deep_plr *dp = deep_plr_data(pplayer);
  int elapsed[DEEP_WAIT_SPELLS];
  int n = 0, i, idx, pct;

  if (ocean >= 0) {
    return 0;                   /* Not a body of water at all. */
  }
  if (deep_idle_ferries_on(ait, pplayer, ocean)
      > deep_ferry_demand_on(ait, pplayer, ocean)) {
    return 0;                   /* A hull is standing free for me. */
  }

  for (i = 0; i < DEEP_WAIT_SPELLS; i++) {
    struct unit *punit;
    bool touches = FALSE;

    if (dp->wait_spells[i].unit_id == 0) {
      continue;
    }
    punit = game_unit_by_number(dp->wait_spells[i].unit_id);
    if (punit == NULL || unit_owner(punit) != pplayer) {
      continue;
    }
    adjc_iterate(nmap, unit_tile(punit), atile) {
      if (tile_continent(atile) == ocean) {
        touches = TRUE;
        break;
      }
    } adjc_iterate_end;
    if (!touches) {
      continue;                 /* Waiting on somebody else's water. */
    }
    elapsed[n++] = MAX(0, game.info.turn - dp->wait_spells[i].since);
  }
  if (n == 0) {
    return 0;
  }

  qsort(elapsed, n, sizeof(elapsed[0]), deep_wait_cmp);
  pct = deep_tuning_get()->lift_wait_tail_pct;
  pct = CLIP(0, pct, 100);
  idx = (n * pct) / 100;
  if (idx >= n) {
    idx = n - 1;
  }

  return elapsed[idx];
}

/**********************************************************************//**
  Add every distinct body of water this tile touches to `seen`.
**************************************************************************/
static void deep_gap_add_waters(const struct tile *ptile, Continent_id *seen,
                                int *nseen, int max)
{
  adjc_iterate(&(wld.map), ptile, atile) {
    Continent_id oc = tile_continent(atile);
    int si;

    if (oc >= 0) {
      continue;                 /* Land. */
    }
    for (si = 0; si < *nseen; si++) {
      if (seen[si] == oc) {
        break;
      }
    }
    if (si < *nseen || *nseen >= max) {
      continue;
    }
    seen[(*nseen)++] = oc;
  } adjc_iterate_end;
}

/**********************************************************************//**
  The DEMAND-SUPPLY GAP on each water this player touches, one line per
  water per turn.

  This is the instrument the commission mechanism answers to, and it
  lands first on purpose (the project's own rule after two guessed
  expansion fixes were reverted and the measured one won its cell).
  The circle it is pointed at is measured on both sides already:

  * EXPANSION slice 1 -- from t20 the land search returns no legal site
    at all on a settled venue, so every founder want deep still forms
    is an overseas want and the lift is the only expansion channel;
  * W36 -- 57.4% of lift wait spells never enroll, and the fifth of
    spells that run >= 5 turns hold 61.4% of all the waiting;
  * W36-DIM -- 86% of the wait discount's firings land on `unit=0`,
    the BUILD-SIDE founder want. The discount damps the very want whose
    fulfilment would commission the hull that ends the wait.

  So demand is damped by the queue, and the queue exists because demand
  never commissions supply. The gap this line reports -- founders
  waiting against berths idle plus berths building, on ONE water -- is
  the quantity a shipyard could act on, and nothing has ever looked at
  it. `wait` is deep_wait_expected() for the same water, i.e. what a
  founder joining that queue should expect to pay in turns.

  Observation only, and computed only when the metric log is on: two
  unit-list walks and a city-list walk per water is a real cost and
  buys nothing in a scoring run.
**************************************************************************/
static void deep_lift_gap_census(struct ai_type *ait, struct player *pplayer)
{
  struct deep_plr *dp = deep_plr_data(pplayer);
  Continent_id seen[16];
  int nseen = 0, i;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }

  /* The waters that can carry a founder off this continent: the ones
   * our cities could launch a hull into, and the ones our waiting
   * founders are actually standing beside. Both halves are needed --
   * a queue on a water we hold no port on is exactly the case where
   * the commission cannot fire, and it should be visible rather than
   * absent. */
  city_list_iterate(pplayer->cities, pcity) {
    deep_gap_add_waters(city_tile(pcity), seen, &nseen,
                        (int) ARRAY_SIZE(seen));
  } city_list_iterate_end;
  unit_list_iterate(pplayer->units, punit) {
    if (deep_lift_wants_boat(ait, punit)
        && tile_continent(unit_tile(punit))
           != tile_continent(punit->goto_tile)) {
      deep_gap_add_waters(unit_tile(punit), seen, &nseen,
                          (int) ARRAY_SIZE(seen));
    }
  } unit_list_iterate_end;

  for (i = 0; i < nseen; i++) {
    Continent_id oc = seen[i];
    int building = 0;
    int founders = deep_founder_demand_on(ait, pplayer, oc);
    int berths = deep_lift_berths_on(ait, pplayer, oc, &building);
    int waiting = deep_ferry_demand_on(ait, pplayer, oc);
    int idle = deep_idle_ferries_on(ait, pplayer, oc);

    if (founders <= 0 && berths <= 0 && building <= 0) {
      continue;                 /* Nothing happening on this water. */
    }
    log_base(deep_metric_log_level(),
             "deep-liftgap turn=%d player=\"%s\" ocean=%d bridge=%d"
             " founders=%d waiting=%d idle=%d berths=%d building=%d"
             " gap=%d wait=%d colony=%d",
             game.info.turn, player_name(pplayer), oc,
             deep_water_bridges(oc) ? 1 : 0, founders, waiting, idle,
             berths, building, founders - berths - building,
             deep_wait_expected(ait, pplayer, oc),
             (dp->colony_turn >= game.info.turn - 1 ? dp->colony_want : 0));
  }
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
  The meeting point that MINIMISES THE LATER ARRIVAL: the tile where
  max(boat turns, passenger turns) is smallest, which is as close to
  equal-turns-from-both as the map allows (human, 2026-08-03).

  This replaces "a tile the boat can float on at or beside the
  passenger", and that choice is why making the rendezvous two-sided
  bought nothing: the meeting point was AT THE PASSENGER'S FEET, so
  the boat travelled the whole gap and the passenger had nowhere to
  walk. Both ends drove to a point only one of them had to reach.

  Turns come from real pathfinding on both sides, not from
  real_map_distance(). Straight line is what made formation accept 31
  musters against a boat that could never arrive, and it would be
  wrong in both directions here -- ocean paths bend around headlands
  and land paths follow roads.

  The boat's map is the caller's, built once per formation. Returns
  the tile, with *pturns set to the later of the two arrivals, or NULL
  if no tile is reachable by both.
**************************************************************************/
static struct tile *deep_lift_best_meeting(const struct civ_map *nmap,
                                           struct pf_map *boatmap,
                                           struct unit *ferry,
                                           struct unit *psngr,
                                           int radius, int *pturns)
{
  struct pf_parameter parameter;
  struct pf_map *walkmap;
  struct tile *best = NULL;
  int bestcost = -1, bestsum = -1;

  pft_fill_unit_parameter(&parameter, nmap, psngr);
  parameter.omniscience = !has_handicap(unit_owner(psngr), H_MAP);
  walkmap = pf_map_new(&parameter);

  square_iterate(nmap, unit_tile(psngr), radius, cand) {
    struct pf_position bpos, wpos;
    struct tile *shore;
    int cost, sum;

    if (!can_unit_exist_at_tile(nmap, ferry, cand)
        || !pf_map_position(boatmap, cand, &bpos)) {
      continue;
    }
    shore = deep_lift_shore_tile(nmap, psngr, cand);
    if (shore == NULL || !pf_map_position(walkmap, shore, &wpos)) {
      continue;
    }
    /* The boat cannot sail before the later of the two gets there, so
     * the max is the cost to minimise; the sum breaks ties toward the
     * meeting that wastes fewer unit-turns overall. */
    cost = MAX(bpos.turn, wpos.turn);
    sum = bpos.turn + wpos.turn;
    if (best == NULL || cost < bestcost
        || (cost == bestcost && sum < bestsum)) {
      best = cand;
      bestcost = cost;
      bestsum = sum;
    }
  } square_iterate_end;

  pf_map_destroy(walkmap);
  if (best != NULL && pturns != NULL) {
    *pturns = bestcost;
  }

  return best;
}

/**********************************************************************//**
  Can this unit stand on the shore beside the rendezvous within the
  turns the muster has left? Distance over its own move rate, which is
  optimistic (it ignores terrain and roads) and deliberately so: the
  cost of being wrong is that a hitcher fails to arrive, and a hitcher
  that fails to arrive is DROPPED rather than waited for.
**************************************************************************/
static bool deep_lift_can_make_meeting(const struct civ_map *nmap,
                                       struct unit *punit,
                                       const struct tile *meet,
                                       int turns_left)
{
  struct tile *shore = deep_lift_shore_tile(nmap, punit, meet);
  int rate, d;

  if (shore == NULL) {
    return FALSE;
  }
  /* Walking distance, so it has to BE walkable: can_unit_exist_at_tile()
   * inside deep_lift_shore_tile() says the unit could stand there, not
   * that it could get there. Without this a unit on another landmass
   * entirely reads as three tiles away across open ocean, enrolls, and
   * then cannot move -- the same class of error as measuring a
   * rendezvous by straight-line distance. */
  if (tile_continent(shore) != tile_continent(unit_tile(punit))) {
    return FALSE;
  }
  d = real_map_distance(unit_tile(punit), shore);
  rate = MAX(1, unit_move_rate(punit) / SINGLE_MOVE);

  return d <= turns_left * rate;
}

/**********************************************************************//**
  Enroll extra passengers into a lift that is still gathering.

  This is hitch-a-ride, and it is only worth building BECAUSE the
  rendezvous is two-sided and named. The original design had the boat
  detour to collect a second passenger, gated on the detour costing
  less than one turn of boat waiting; sized against that gate it was
  worth 0.4 unit-turns per game on lategame-80 and was refused
  (findings/settler-missions.md section 10). With a FIXED meeting tile
  the boat detours by exactly zero -- the joiner walks to a tile the
  hull is already sailing to -- so the rung that binds is no longer
  "is the boat near the passenger" but "is the passenger bound for the
  same continent", which measured 2.9 per game there and 122 per game
  on the island cell.

  Two contracts, both deliberate:

  1. A HITCHER NEVER DELAYS THE LIFT. Only members[0] is required to
     sail; anyone else who has not boarded when the core passenger has
     is released, not waited for. That is why the ETA test above may
     be optimistic without costing anything.
  2. A HITCHER IS ALREADY WAITING. Candidates come from
     deep_lift_wants_boat(), so holding one costs no activity it would
     otherwise have had -- it was standing still waiting for a boat
     that was not coming.

  The manifest is capped by lift_manifest_max AND by the hull, which
  is not the same bound: civ2civ3's Trireme carries 2 where its
  Transport carries 8. Concentration is a real cost -- one sunk hull
  now loses the whole manifest rather than one unit -- which is why
  the cap is a tunable rather than "fill the boat".
**************************************************************************/
static int deep_lift_enroll(struct ai_type *ait, struct player *pplayer,
                            struct deep_operation *op, struct unit *ferry,
                            const struct tile *meet, int turns_left)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct tile *beach = index_to_tile(nmap, op->beachhead);
  int room, joined = 0;

  if (beach == NULL || meet == NULL) {
    return 0;
  }
  room = MIN(tune->lift_manifest_max, get_transporter_capacity(ferry))
         - op->n_members;
  room = MIN(room, DEEP_OP_MEMBERS - op->n_members);
  if (room <= 0) {
    return 0;
  }

  unit_list_iterate(pplayer->units, cand) {
    if (room <= 0) {
      break;
    }
    if (!deep_lift_wants_boat(ait, cand)
        || deep_op_claiming(pplayer, cand->id) >= 0
        || !could_unit_load(cand, ferry)) {
      continue;
    }
    /* Same crossing, not merely another crossing: the passage it
     * wants must be the passage this hull is already running. */
    if (tile_continent(unit_tile(cand)) == tile_continent(cand->goto_tile)
        || tile_continent(cand->goto_tile) != tile_continent(beach)) {
      continue;
    }
    if (!deep_lift_can_make_meeting(nmap, cand, meet, turns_left)) {
      continue;
    }
    op->members[op->n_members++] = cand->id;
    room--;
    joined++;
    /* own=(x,y) is where this joiner actually wants to go, and
     * ownd is how far that is from the beachhead the op will release
     * it at -- the core passenger's target, not its own. A hitcher
     * landed far from its site pays that distance on foot, and
     * nothing priced it. This is the number that decides whether the
     * boat should make ONE stop or several in priority order. */
    log_base(deep_metric_log_level(),
             "deep-lift-op turn=%d player=\"%s\" hitch boat=%d unit=%d"
             " manifest=%d meet=(%d,%d) left=%d own=(%d,%d) ownd=%d",
             game.info.turn, player_name(pplayer), op->ferry, cand->id,
             op->n_members, TILE_XY(meet), turns_left,
             TILE_XY(cand->goto_tile),
             real_map_distance(cand->goto_tile, beach));
  } unit_list_iterate_end;

  return joined;
}

/**********************************************************************//**
  Should the boat hold the quay another turn for a straggler?

  The blanket "a hitcher never delays the lift" rule is too blunt in
  one direction: waiting two turns to double the manifest is obviously
  worth it on a fifteen-turn crossing and obviously not on a two-turn
  hop. So the patience is priced against the trip it is buying --
  lift_hitch_wait_ratio of the estimated crossing, and nothing when
  the ratio is 0.

  Two hard bounds keep the patience from eating the mission it is
  meant to improve:

  - It never spends the last turn of the gather window. Waiting into a
    gather-timeout would dissolve a lift whose core passenger is
    ABOARD and ready, trading a delivered colony for a hitcher.
  - It only waits for a straggler that can actually make it in the
    time being budgeted, using the same walk estimate enrollment used.

  Returns TRUE if some enrolled member is worth holding for.
**************************************************************************/
static bool deep_lift_worth_waiting(struct deep_operation *op,
                                    struct unit *ferry,
                                    const struct tile *meet,
                                    const struct tile *beach,
                                    int turns_left)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  int budget, boat_rate, eta_cross, i;

  if (tune->lift_hitch_wait_ratio <= 0.0 || meet == NULL || beach == NULL) {
    return FALSE;
  }
  boat_rate = MAX(1, unit_move_rate(ferry) / SINGLE_MOVE);
  eta_cross = (real_map_distance(unit_tile(ferry), beach) + boat_rate - 1)
              / boat_rate;
  /* Round UP to a whole turn: patience is spent in turns, and
   * truncation quietly turned "wait a quarter of a four-turn
   * crossing" into "never wait" for every short trip -- exactly the
   * cheap waits most likely to pay, since a straggler one turn out is
   * the commonest kind. Willing at all means willing for one turn. */
  budget = MAX(MAX(1, tune->lift_hitch_wait_min),
               (int) (tune->lift_hitch_wait_ratio * eta_cross));
  /* Never spend the window's last turn: sail beats time out. */
  budget = MIN(budget, turns_left - 1);
  if (budget <= 0) {
    return FALSE;
  }

  for (i = 1; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (m == NULL || unit_transport_get(m) == ferry) {
      continue;
    }
    if (deep_lift_can_make_meeting(nmap, m, meet, budget)) {
      return TRUE;
    }
  }

  return FALSE;
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
  int best_d = -1, meet_turns = 0;

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
  /* Where to meet: the tile whose LATER arrival is soonest, found by
   * real pathfinding from both ends. The boat's map is built once
   * here and reused for every candidate, and it is also the honest
   * test of the thing straight-line distance kept getting wrong --
   * whether this hull can reach that water at all. */
  {
    struct pf_parameter parameter;
    struct pf_map *boatmap;
    struct tile *land;

    pft_fill_unit_parameter(&parameter, nmap, best_boat);
    parameter.omniscience = !has_handicap(pplayer, H_MAP);
    boatmap = pf_map_new(&parameter);
    meet = deep_lift_best_meeting(nmap, boatmap, best_boat, best_psngr,
                                  tune->lift_form_radius, &meet_turns);
    /* The BEACHHEAD needs the same honesty as the rendezvous. Asking
     * only whether a floatable tile exists beside the target says
     * nothing about whether this hull can sail to it -- different
     * ocean body, or a sea reached only through a strait the boat
     * cannot enter -- and a lift formed on that answer sails nowhere.
     *
     * Measured: 21% of traced crossings (59 of 275) never reduced
     * their distance to the beachhead AT ALL, one holding at 12 tiles
     * for 22 turns until the cross budget expired. That is the whole
     * stalled population, and it is the third instance tonight of
     * "can it exist there" being used where "can it get there" was
     * meant. */
    land = deep_lift_meeting_tile(nmap, best_boat,
                                  best_psngr->goto_tile);
    if (land != NULL) {
      struct pf_position pos;

      if (!pf_map_position(boatmap, land, &pos)) {
        land = NULL;            /* Exists, unreachable: refuse. */
      }
    }
    pf_map_destroy(boatmap);
    if (land == NULL) {
      return FALSE;
    }
  }
  if (meet == NULL) {
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
           " psngr=%d gap=%d beach=(%d,%d) meet=(%d,%d) eta=%d",
           game.info.turn, player_name(pplayer), oi, op->ferry,
           op->members[0], best_d, TILE_XY(best_psngr->goto_tile),
           TILE_XY(meet), meet_turns);

  return TRUE;
}

/**********************************************************************//**
  The manifest member whose own target lies nearest the hull, and (out)
  how far that is. "Where does this boat go next" now has ONE
  implementation, shared by the stop server below and by the
  mid-crossing re-aim, which have to agree or a re-aimed operation
  would be re-aimed again at the first stop.
**************************************************************************/
static struct unit *deep_lift_nearest_target(struct deep_operation *op,
                                             const struct unit *ferry,
                                             int *gap)
{
  struct unit *next = NULL;
  int bestd = -1, k;

  for (k = 0; k < op->n_members; k++) {
    struct unit *m = game_unit_by_number(op->members[k]);
    int d;

    if (m == NULL || m->goto_tile == NULL) {
      continue;
    }
    d = real_map_distance(unit_tile(ferry), m->goto_tile);
    if (next == NULL || d < bestd) {
      next = m;
      bestd = d;
    }
  }
  if (gap != NULL) {
    *gap = bestd;
  }

  return next;
}

/**********************************************************************//**
  Serve the stop the boat is standing at: release every member whose
  own target is within lift_drop_radius of it, and if anyone is still
  aboard for somewhere else, re-aim the op at the nearest of THEIR
  targets.

  Returns TRUE if the operation continues to another stop, FALSE if
  this stop was the last and the caller should dissolve it as arrived.

  Called from BOTH arrival paths. It was originally inline in the
  slow one, and the same-turn arrival added later returned before
  reaching it -- so with same-turn arrivals now the common case, the
  routing never ran at all: 16 two-passenger manifests over 20 games
  produced zero next stops.
**************************************************************************/
static bool deep_lift_serve_stop(struct player *pplayer,
                                 struct deep_operation *op, int oi,
                                 struct unit *ferry, struct tile *beach)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct unit *next = NULL;
  int kept = 0, bestd = -1, k, was = op->n_members;

  for (k = 0; k < op->n_members; k++) {
    struct unit *m = game_unit_by_number(op->members[k]);

    if (m == NULL) {
      continue;
    }
    /* Served here means "can walk off onto its own target": adjacency
     * is the game mechanic, not a tunable. Anyone further wants
     * another stop, and lift_drop_radius decides whether the boat is
     * willing to make it. */
    if (m->goto_tile == NULL
        || real_map_distance(m->goto_tile, beach) <= 1) {
      continue;                 /* Served by this stop: let it go. */
    }
    op->members[kept++] = op->members[k];
  }
  if (kept <= 0 || kept >= was) {
    return FALSE;
  }
  op->n_members = kept;
  next = deep_lift_nearest_target(op, ferry, &bestd);
  /* lift_drop_radius is the DETOUR the boat will make for a further
   * stop, in tiles -- "two boat moves" at the human's sizing
   * (2026-08-04). Beyond it the remaining passengers are released to
   * walk, because a hull is not worth tying up indefinitely for one
   * settler however cheap hull-turns are. */
  if (next == NULL
      || bestd > tune->lift_drop_radius
      || deep_lift_meeting_tile(nmap, ferry, next->goto_tile) == NULL) {
    return FALSE;
  }
  op->beachhead = tile_index(next->goto_tile);
  op->phase_turn = game.info.turn;      /* fresh cross budget */
  log_base(deep_metric_log_level(),
           "deep-lift-op turn=%d player=\"%s\" op=%d nextstop"
           " boat=%d manifest=%d beach=(%d,%d) gap=%d",
           game.info.turn, player_name(pplayer), oi, op->ferry,
           op->n_members, TILE_XY(next->goto_tile), bestd);

  return TRUE;
}

/**********************************************************************//**
  Re-validate the manifest's missions mid-crossing.

  The beachhead is fixed when the lift FORMS (deep_op_form_lift) and
  re-aimed only when a stop is served, while the passenger's own
  validity check -- `city_can_be_built_here()` in
  deep_auto_settler_run() -- sits BELOW that function's op-hold early
  return, so a held passenger never reaches it. Between formation and
  landfall a rival can found on the target, our own second settler can
  found within citymindist of it, or the borders can move: the lift
  then completed the crossing anyway and put a settler on a tile it
  cannot build on, and the metric recorded a `lift-arrived` (D-E). The
  frequency was unmeasured because nothing logged it; `deep-lift-invalid`
  is that number.

  Same predicate as the settler path, deliberately -- a second opinion
  about foundability is a second bug waiting. Three outcomes: a member
  whose target died loses its mission and its berth on the manifest (it
  rides on as freight and re-searches from wherever it steps ashore);
  if the DEAD target was the beachhead, the op re-aims at the nearest
  surviving member's target through the same choice the stop server
  makes; and with nobody left to deliver, the operation aborts rather
  than sailing on to nothing.

  Returns FALSE if the operation was dissolved (caller must return).
**************************************************************************/
static bool deep_lift_mission_dead(struct ai_type *ait, struct unit *m)
{
  return m != NULL
         && m->goto_tile != NULL
         && def_ai_unit_data(m, ait)->task == AIUNIT_BUILD_CITY
         && !city_can_be_built_here(&(wld.map), m->goto_tile, m, FALSE);
}

static void deep_lift_invalid_log(struct player *pplayer,
                                  const struct deep_operation *op, int oi,
                                  const struct unit *m, struct unit *ferry,
                                  struct tile *beach, const char *phase)
{
  log_base(deep_metric_log_level(),
           "deep-lift-invalid turn=%d player=\"%s\" op=%d unit=%d"
           " target=(%d,%d) beach=(%d,%d) wasbeach=%d boatd=%d"
           " turns=%d phase=%s",
           game.info.turn, player_name(pplayer), oi, m->id,
           TILE_XY(m->goto_tile), TILE_XY(beach),
           same_pos(m->goto_tile, beach) ? 1 : 0,
           real_map_distance(unit_tile(ferry), beach),
           game.info.turn - op->formed_turn, phase);
}

/**********************************************************************//**
  The same question during the MUSTER, where nothing has ever asked it.

  deep_lift_revalidate() below runs on every MARCH turn, and D-E's
  numbers (3-7 aborted crossings a game) are all crossings. But
  deep_op_holds() holds a lift's members through MUSTER as firmly as
  through MARCH, and the gather window is lift_gather_turns long, so a
  passenger standing at the quay with a dead target is in exactly the
  position D-E described -- held above the settler path's own
  foundability check -- for as many turns as the rendezvous takes,
  and it then SAILS on a mission that was already dead before the
  hull left.

  Observation only: this counts what the muster is holding, and the
  release is a separate decision behind its own knob. `phase` is on
  both lines so the two populations can be read apart.
**************************************************************************/
static int deep_lift_muster_invalid(struct ai_type *ait,
                                    struct player *pplayer,
                                    struct deep_operation *op, int oi,
                                    struct unit *ferry, struct tile *beach)
{
  int i, dead = 0;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return 0;
  }
  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (deep_lift_mission_dead(ait, m)) {
      deep_lift_invalid_log(pplayer, op, oi, m, ferry, beach, "muster");
      dead++;
    }
  }

  return dead;
}

static bool deep_lift_revalidate(struct ai_type *ait, struct player *pplayer,
                                 struct deep_operation *op, int oi,
                                 struct unit *ferry, struct tile *beach,
                                 const char *phase)
{
  const struct civ_map *nmap = &(wld.map);
  bool beach_dead = FALSE;
  int i, kept = 0, dropped = 0;

  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (deep_lift_mission_dead(ait, m)) {
      deep_lift_invalid_log(pplayer, op, oi, m, ferry, beach, phase);
      if (same_pos(m->goto_tile, beach)) {
        beach_dead = TRUE;
      }
      dai_unit_new_task(ait, m, AIUNIT_NONE, NULL);
      dropped++;
      continue;
    }
    op->members[kept++] = op->members[i];
  }

  if (dropped == 0) {
    return TRUE;                /* The common case: nothing moved. */
  }
  op->n_members = kept;
  if (kept <= 0) {
    deep_op_dissolve(op, oi, "lift-invalid", pplayer);
    return FALSE;
  }
  if (beach_dead) {
    int gap = -1;
    struct unit *next = deep_lift_nearest_target(op, ferry, &gap);

    if (next == NULL
        || deep_lift_meeting_tile(nmap, ferry, next->goto_tile) == NULL) {
      /* Nothing left worth sailing to, or nowhere the hull can put it
       * ashore: abort rather than deliver onto a dead tile. */
      deep_op_dissolve(op, oi, "lift-invalid", pplayer);
      return FALSE;
    }
    op->beachhead = tile_index(next->goto_tile);
    op->phase_turn = game.info.turn;    /* Fresh cross budget. */
    log_base(deep_metric_log_level(),
             "deep-lift-op turn=%d player=\"%s\" op=%d reaim boat=%d"
             " manifest=%d beach=(%d,%d) gap=%d",
             game.info.turn, player_name(pplayer), oi, op->ferry,
             op->n_members, TILE_XY(next->goto_tile), gap);
  }

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

  /* Take the passengers off the server-side agent for the crossing.
   *
   * deep_op_holds() sets unit_ai.done, which gates DEEP's manager and
   * nothing else -- the auto-worker agent runs from the server every
   * turn regardless, and dai_manage_settler() stamps SSA_AUTOWORKER on
   * every settler it has ever touched (daiunit.c:2726). So the op
   * owned its manifest as far as the AI was concerned while something
   * else walked it off the deck mid-ocean.
   *
   * Measured on isolated-150: passengers that stray leave a median of
   * 4 turns after formation with the boat still 11-45 tiles from the
   * beachhead (12 of 14 cases), against delivered ones that leave at
   * distance <=2 after a median 11 turns -- and ALL of them carry
   * SSA_AUTOWORKER. Two clean populations, one cause.
   *
   * Not restored explicitly on release: dai_manage_settler() re-stamps
   * the agent the next time it manages the unit, which is exactly the
   * behaviour that should resume once the lift is done with it. */
  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (m != NULL && m->ssa_controller != SSA_NONE) {
      (void) unit_server_side_agent_set(pplayer, m, SSA_NONE);
    }
  }
  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (m != NULL && unit_transport_get(m) == ferry) {
      aboard++;
    }
  }

  if (op->state == DEEP_OP_MUSTER) {
    /* What is the quay holding? The hold that keeps a marching
     * passenger from re-checking its own mission (D-E) is on during
     * the gather too, and for lift_gather_turns of it.
     *
     * With lift_muster_release the gather answers the same way the
     * crossing does -- drop the dead mission, re-aim if it was the
     * beachhead, abort if nothing is left -- which is strictly the
     * cheaper end to do it at: before the hull has sailed, a released
     * founder is still standing on its own continent with its own
     * legs. At 0 the pass only counts, which is what shipped first. */
    if (deep_tuning_get()->lift_muster_release > 0) {
      if (!deep_lift_revalidate(ait, pplayer, op, oi, ferry, beach,
                                "muster")) {
        return;                 /* Aborted: nothing left to deliver. */
      }
      beach = index_to_tile(nmap, op->beachhead);
      if (beach == NULL) {
        deep_op_dissolve(op, oi, "lift-nobeach", pplayer);
        return;
      }
    } else {
      (void) deep_lift_muster_invalid(ait, pplayer, op, oi, ferry, beach);
    }

    /* The core passenger is members[0] -- the one the lift was formed
     * for. Once it is aboard the lift CAN sail; whether it should is
     * deep_lift_worth_waiting(), which holds the quay for a straggler
     * only when the crossing it is buying is long enough to pay for
     * the delay. A straggler not worth holding for is released rather
     * than waited for. (Validation compacts members[], so if the
     * original core dies a hitcher becomes the core and the lift is
     * still worth sailing.) */
    struct unit *core = (op->n_members > 0
                         ? game_unit_by_number(op->members[0]) : NULL);
    bool core_aboard = (core != NULL && unit_transport_get(core) == ferry);
    struct tile *rv = index_to_tile(nmap, op->rendezvous);
    int left = tune->lift_gather_turns - (game.info.turn - op->phase_turn);

    if (core_aboard
        && deep_lift_worth_waiting(op, ferry, rv, beach, left)) {
      log_base(deep_metric_log_level(),
               "deep-lift-op turn=%d player=\"%s\" op=%d hitch-wait"
               " boat=%d manifest=%d aboard=%d left=%d",
               game.info.turn, player_name(pplayer), oi, op->ferry,
               op->n_members, aboard, left);
      core_aboard = FALSE;      /* Hold the quay; drive the walkers. */
    }

    if (core_aboard) {
      int kept = 0;

      for (i = 0; i < op->n_members; i++) {
        struct unit *m = game_unit_by_number(op->members[i]);

        if (m != NULL && unit_transport_get(m) == ferry) {
          op->members[kept++] = op->members[i];
        } else if (m != NULL) {
          log_base(deep_metric_log_level(),
                   "deep-lift-op turn=%d player=\"%s\" op=%d hitch-drop"
                   " boat=%d unit=%d",
                   game.info.turn, player_name(pplayer), oi, op->ferry,
                   op->members[i]);
        }
      }
      op->n_members = kept;
      aboard = kept;

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
       * passenger where it stood: ~570 held-idle passenger-turns per
       * 20 games, 57% on lifts that never sailed, and EVERY
       * gather-timeout burning the whole window (mean 9.0 against a
       * cap of 8) -- the boat was not arriving late, it was not
       * arriving at all.
       *
       * MEASURED AFTERWARDS: making it two-sided did not shorten the
       * muster (2.22 -> 2.26 turns) and did not cut idle time. The
       * original claim here -- that a walking settler saves the boat
       * half the trip -- is NOT supported. Successful musters were
       * already near the floor the boat's own travel sets, and the
       * failures are unreachable rather than slow. The deep-lift-walk
       * line below exists because "the walk fires and does not help"
       * and "the walk never fires" were indistinguishable in the
       * aggregates that produced that conclusion. */
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
      /* Fill the spare capacity while the hull is on its way. Done
       * before the boat moves, so a joiner enrolled this turn starts
       * walking this turn. */
      (void) deep_lift_enroll(ait, pplayer, op, ferry, meet,
                              tune->lift_gather_turns
                              - (game.info.turn - op->phase_turn));
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
        } else {
          /* The other half of the meeting: walk to the shore beside
           * the rendezvous. Toward the fixed TILE, never toward the
           * boat -- a passenger homing on a hull that is itself
           * homing on the passenger orbits it instead of meeting it.
           *
           * Logged whether it walks or not, and why not: a member with
           * no moves, no shore, or already standing on it are three
           * different reasons the rendezvous stays one-sided, and the
           * aggregate cannot tell them apart. */
          struct tile *shore = deep_lift_shore_tile(nmap, m, meet);
          struct tile *was = unit_tile(m);
          int d0 = real_map_distance(was, meet);
          int mleft = m->moves_left;
          const char *why = "walk";

          if (shore == NULL) {
            why = "noshore";
          } else if (same_pos(was, shore)) {
            why = "arrived";
          } else if (mleft <= 0) {
            why = "nomoves";
          } else {
            (void) dai_unit_goto(ait, m, shore);
            m = game_unit_by_number(op->members[i]);
            /* Arriving with moves to spare means boarding NOW, not
             * next turn (human, 2026-08-03). The boarding tests above
             * ran against where this unit stood at the start of the
             * turn, so without this a passenger that walks onto the
             * shore beside its own waiting hull stands there a whole
             * turn looking at it -- and musters only last about two,
             * so that is a large share of the entire rendezvous. */
            if (m != NULL && unit_transport_get(m) != ferry
                && m->moves_left > 0) {
              if (same_pos(unit_tile(m), unit_tile(ferry))) {
                if (can_unit_load(m, ferry)) {
                  unit_transport_load_send(m, ferry);
                  why = "walk-board";
                }
              } else if (is_tiles_adjacent(unit_tile(m),
                                           unit_tile(ferry))) {
                (void) dai_unit_goto(ait, m, unit_tile(ferry));
                m = game_unit_by_number(op->members[i]);
                why = "walk-board";
              }
            }
          }
          log_base(deep_metric_log_level(),
                   "deep-lift-walk turn=%d player=\"%s\" op=%d unit=%d"
                   " why=%s d0=%d d1=%d moved=%d mleft=%d gap=%d",
                   game.info.turn, player_name(pplayer), oi,
                   op->members[i], why, d0,
                   m != NULL ? real_map_distance(unit_tile(m), meet) : -1,
                   (m != NULL && !same_pos(unit_tile(m), was)) ? 1 : 0,
                   mleft,
                   m != NULL ? real_map_distance(unit_tile(m),
                                                 unit_tile(ferry)) : -1);
        }
      }

      /* Sail THIS turn if the boarding above completed the manifest
       * and the hull still has moves (human, 2026-08-03). The sail
       * test at the top of this function ran before anyone had moved,
       * so without this the boat loads its passenger and then sits at
       * the quay for the rest of the turn, waiting for a state change
       * that only happens next turn. The MARCH block below is a
       * separate `if` in the same call, so flipping the state here is
       * all it takes for the crossing to start immediately. */
      core = (op->n_members > 0
              ? game_unit_by_number(op->members[0]) : NULL);
      if (core != NULL && unit_transport_get(core) == ferry
          && ferry->moves_left > 0
          && !deep_lift_worth_waiting(op, ferry, meet, beach, left)) {
        int kept = 0;

        for (i = 0; i < op->n_members; i++) {
          struct unit *m = game_unit_by_number(op->members[i]);

          if (m != NULL && unit_transport_get(m) == ferry) {
            op->members[kept++] = op->members[i];
          }
        }
        op->n_members = kept;
        aboard = kept;
        op->state = DEEP_OP_MARCH;
        op->phase_turn = game.info.turn;
        op->march_turn = game.info.turn;
        log_base(deep_metric_log_level(),
                 "deep-lift-op turn=%d player=\"%s\" op=%d sail boat=%d"
                 " manifest=%d beach=(%d,%d) sameturn=1",
                 game.info.turn, player_name(pplayer), oi, op->ferry,
                 aboard, TILE_XY(beach));
      }
    }
  }

  if (op->state == DEEP_OP_MARCH) {
    struct tile *land = deep_lift_meeting_tile(nmap, ferry, beach);

    /* TRACE, every MARCH turn, per member: which hull it is on (0 =
     * none, and a DIFFERENT id means it changed boats), where it is,
     * where the hull is, and what the hull has left to move with.
     *
     * A trace rather than another correlation. The auto-worker
     * hypothesis died because it was inferred from ONE snapshot taken
     * at detach time, where SSA_AUTOWORKER was 100% of strays and also
     * 100% of deliveries -- a constant read as a cause. The turn a
     * passenger detaches, and what the hull was doing on exactly that
     * turn, is the thing no snapshot can show. */
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);
      struct unit *tr = (m != NULL ? unit_transport_get(m) : NULL);

      log_base(deep_metric_log_level(),
               "deep-lift-trace turn=%d player=\"%s\" op=%d unit=%d"
               " on=%d boat=%d at=(%d,%d) boatat=(%d,%d) bmoves=%d"
               " bhp=%d beachd=%d alive=%d",
               game.info.turn, player_name(pplayer), oi, op->members[i],
               tr != NULL ? tr->id : 0, op->ferry,
               /* NOT `cond ? TILE_XY(t) : -1`: TILE_XY expands to two
                * comma-separated expressions, so the ternary would
                * swallow the pair with the comma operator and log the
                * y twice. Same shape as the offboard line above. */
               m != NULL ? index_to_map_pos_x(tile_index(unit_tile(m))) : -1,
               m != NULL ? index_to_map_pos_y(tile_index(unit_tile(m))) : -1,
               TILE_XY(unit_tile(ferry)), ferry->moves_left, ferry->hp,
               m != NULL ? real_map_distance(unit_tile(m), beach) : -1,
               m != NULL ? 1 : 0);
    }

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
                 " dist=%d trans=%d goal=%d turns=%d ssa=%d boatd=%d",
                 game.info.turn, player_name(pplayer), oi, op->ferry,
                 op->members[i], m != NULL ? 1 : 0,
                 mt != NULL ? index_to_map_pos_x(tile_index(mt)) : -1,
                 mt != NULL ? index_to_map_pos_y(tile_index(mt)) : -1,
                 TILE_XY(beach),
                 mt != NULL ? real_map_distance(mt, beach) : -1,
                 (m != NULL && unit_transported(m)) ? 1 : 0,
                 (m != NULL && m->goto_tile != NULL
                  && same_pos(m->goto_tile, beach)) ? 1 : 0,
                 game.info.turn - op->formed_turn,
                 /* Who was actually driving this unit. deep_op_holds()
                  * sets unit_ai.done, which gates DEEP's manager only:
                  * a server-side agent keeps acting on the unit every
                  * turn regardless, and dai_manage_settler() stamps
                  * SSA_AUTOWORKER on every settler it ever touches
                  * (daiunit.c:2726). If the strays carry an agent, the
                  * lift is being overruled rather than mis-aimed. */
                 m != NULL ? (int) m->ssa_controller : -1,
                 /* And where the hull was: a passenger that left 20
                  * tiles out while the boat was still at sea did not
                  * land badly, it was taken off mid-voyage. */
                 real_map_distance(unit_tile(ferry), beach));
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

    /* Is anyone still going anywhere? Asked EVERY march turn, because
     * the world moves while the manifest is at sea and the hold keeps
     * the passenger's own check from ever running (D-E). */
    {
      int was_beach = op->beachhead;

      if (!deep_lift_revalidate(ait, pplayer, op, oi, ferry, beach,
                                "march")) {
        return;                 /* Aborted: nothing left to deliver. */
      }
      if (op->beachhead != was_beach) {
        /* Re-aimed. Re-read the target and the landing tile the rest
         * of this block works from -- and only here, because
         * deep_lift_meeting_tile() builds a pf map. */
        beach = index_to_tile(nmap, op->beachhead);
        if (beach == NULL) {
          deep_op_dissolve(op, oi, "lift-nobeach", pplayer);
          return;
        }
        land = deep_lift_meeting_tile(nmap, ferry, beach);
      }
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
      /* Beside the target: serve this stop, and sail on if anyone
       * aboard is bound elsewhere (deep_lift_serve_stop). */
      if (deep_lift_serve_stop(pplayer, op, oi, ferry, beach)) {
        return;
      }
      /* Nobody left aboard for elsewhere: hand the manifest back. A
       * settler aboard a boat adjacent to its own goto_tile is exactly
       * the case the stock settler path disembarks correctly. */
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
      ferry = game_unit_by_number(op->ferry);
      /* Re-check arrival in the SAME turn, the mirror of sailing on
       * the turn the manifest boards (human, 2026-08-03). The test
       * above ran before the hull moved, so a boat that reached its
       * beachhead this turn used to hold its passengers aboard until
       * the next one -- a wasted turn at the far end to match the one
       * that used to be wasted at the near end. Releasing here also
       * frees the hull with whatever movement it has left, so it can
       * turn around for the next passenger instead of idling at the
       * beach: the fleet is already 3x oversupplied, and throughput
       * per hull is the thing worth buying. */
      if (ferry != NULL
          && real_map_distance(unit_tile(ferry), beach) <= 1) {
        log_base(deep_metric_log_level(),
                 "deep-lift-op turn=%d player=\"%s\" op=%d arrive boat=%d"
                 " manifest=%d at=(%d,%d) beach=(%d,%d) turns=%d"
                 " sameturn=1",
                 game.info.turn, player_name(pplayer), oi, op->ferry,
                 aboard, TILE_XY(unit_tile(ferry)), TILE_XY(beach),
                 game.info.turn - op->formed_turn);
        if (deep_lift_serve_stop(pplayer, op, oi, ferry, beach)) {
          return;
        }
        deep_op_dissolve(op, oi, "lift-arrived", pplayer);
        return;
      }
    }
  }
}

/**********************************************************************//**
  The stock manage loop with one change: this phase's holds survive.
  dai_manage_units() clears every unit's done flag before managing,
  which silently erased everything the operation layer had marked --
  the muster "hold" never held, and STRIKE members were re-tasked by
  find_something_to_kill the same turn the op fought the battle (the
  stall tripwire caught a strike member walking AWAY from the walls
  under stock orders). Same airlift, same defender assignment, same
  per-unit management; the only difference is that units held this
  phase stay done.

  The reset itself has to stay -- the flag is stale from last turn --
  so "preserve if set" would be wrong. What survives is a hold taken
  in THIS phase, whether the operation layer took it or one of the
  four first-activities passes did.
**************************************************************************/
void deep_manage_units(struct ai_type *ait, struct player *pplayer)
{
  TIMING_LOG(AIT_AIRLIFT, TIMER_START);
  dai_airlift(ait, pplayer);
  TIMING_LOG(AIT_AIRLIFT, TIMER_STOP);

  deep_lift_census(ait, pplayer);
  deep_hull_census(ait, pplayer);
  deep_order_census(ait, pplayer);
  /* After deep_run_operations() has already formed and driven every
   * lift it could this turn, which is the vantage the question needs:
   * who is STILL waiting once the lift has done its best, and was
   * there a crossing they could have joined? */
  deep_hitch_census(ait, pplayer);
  /* The live spells first, then the census that reports the same
   * population: the settler pass runs after this one in the same turn
   * (auto_workers_player follows first_activities), so a site search
   * reads a queue refreshed this turn rather than last turn's. */
  deep_wait_spells_update(ait, pplayer);
  deep_wait_census(ait, pplayer);
  /* And the same queue seen from the shipyard's side: how much of it
   * is settlement, and what supply stands against it, per water. */
  deep_lift_gap_census(ait, pplayer);

  unit_list_iterate(pplayer->units, punit) {
    struct unit_ai *unit_data = def_ai_unit_data(punit, ait);

    unit_data->done = deep_op_holds(pplayer, punit->id)
                      || deep_phase_held(pplayer, punit->id);
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
bool deep_op_targeted(const struct player *pplayer, int city_id)
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
  /* The partner discount's input, gathered once per call rather than
   * once per candidate city: how many live routes we hold with each
   * player. Skipped entirely at the compiled default. */
  int routes[MAX_NUM_PLAYER_SLOTS];
  bool spare = deep_tuning_get()->trade_partner_target_discount > 0.0;

  if (spare) {
    spare = deep_routes_with_players(pplayer, routes,
                                     ARRAY_SIZE(routes)) > 0;
  }

  players_iterate_alive(aplayer) {
    bool partner;

    if (!pplayers_at_war(pplayer, aplayer)
        || (!is_barbarian(aplayer)
            && player_diplstate_get(pplayer, aplayer)->type
               == DS_NO_CONTACT)) {
      continue;
    }
    partner = spare && player_index(aplayer) >= 0
      && player_index(aplayer) < MAX_NUM_PLAYER_SLOTS
      && routes[player_index(aplayer)] > 0;
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
      /* The runaway-leader bias (PEACE lever 3): the first consumer of
       * a signal the server has recomputed every turn since 2003 and
       * nobody has ever read. A player who is alone at the top on
       * techs or on production is worth taking a city from more than
       * the same city held by anybody else. 0.0 = off, and the
       * multiply is skipped rather than done, so the off arm is the
       * previous ordering byte-identically. */
      if (deep_tuning_get()->leader_check_gain > 0.0
          && deep_player_is_leader(ait, pplayer, aplayer)) {
        score *= 1.0 + deep_tuning_get()->leader_check_gain;
        deep_plr_data(pplayer)->leader_bias_applied++;
      }
      /* PILLAR-LITE (TRADE design call 1). Route value enters deep's
       * OWN military choice and nothing else: among targets we could
       * take, one belonging to a live trade partner is discounted, so
       * an equal stranger is burned first. War and peace stay stock's
       * until W34-B -- this cannot start or stop a war, it can only
       * re-order the targets of one already running. 0.0 is the pure
       * opportunist posture and is the compiled default, and the
       * multiply is skipped rather than done so the off arm keeps the
       * previous ordering byte-identically. */
      if (partner) {
        score *= 1.0 - deep_tuning_get()->trade_partner_target_discount;
        deep_plr_data(pplayer)->trade_partner_spared++;
      }
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
  /* Same contract for the muster fields (ERA 3b): a slot reused by an
   * ordinary wave, a raid or a lift must not inherit the previous
   * occupant's muster flag or its stall clock. */
  op->muster = 0;
  op->muster_worth = 0;
  op->muster_best = 0;
  op->muster_grew = 0;
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
  Does this target THREATEN us, in the sense gates 2 and 5 exempt
  from the stance -- the raised formation bar, and the scaled
  offensive build want?

  Two classes, both read from data already at hand and neither needing
  a constant:

  - A barbarian holding. Barbarians make no contact, keep no border
    and negotiate nothing: their cities on the map ARE the evidence,
    the same reading deep_city_survival_core() already applies to
    their units.
  - A city on a landmass where we hold cities. This is deep's own
    REACHABILITY law, the one deep_garrison_menace() uses to weight a
    neighbour ("an army that shares a continent with any of our cities
    counts in full"), applied to the owner of a city we are already at
    war with. A hostile neighbour who can march is not an ambition,
    it is a threat, and the stance must not talk an economy-weighted
    player out of answering it -- neither out of committing the
    blades it has, nor out of buying the ones it needs.

  This is gate 1's survival-floor precedent pointed outward: the floor
  is not "keep attacking", it is "keep the war you are already in
  answerable".
**************************************************************************/
bool deep_op_target_threatens(const struct player *pplayer,
                              const struct city *target)
{
  Continent_id tc = tile_continent(city_tile(target));

  if (is_barbarian(city_owner(target))) {
    return TRUE;
  }
  city_list_iterate(pplayer->cities, pcity) {
    if (tile_continent(city_tile(pcity)) == tc) {
      return TRUE;
    }
  } city_list_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  THE OFFENSE-CONSUMES-THE-WAVE INSTRUMENT (NETTHREAT item 1, evidence
  before mechanism).

  Item 1 wants the home survival wave discounted by E[enemy losses to
  our committed offense]: an enemy unit our assault is about to engage
  is not also besieging us. Before any such discount can be honest,
  two quantities have to be measured on the venue rather than assumed,
  and this pair of accessors is what makes them readable from the
  wave census:

  - WHICH enemy cities our live operations point at, so the wave
    assembly can count how many of its members are standing in one.
    `committed_only` splits MARCH/STRIKE (a wave that is moving) from
    MUSTER (a wave that may never move) -- the distinction the
    launch-rate guard is about.
  - the OBSERVED launch rate, ops_launched / ops_formed, the same
    ratio attacker_hold_damper already prices. CONSEQ N3's Iceland
    built 57 Marines against 6 launches; a discount weighted by
    intents rather than by this number licenses disarmament by
    wishful thinking.

  Lifts are excluded: a lift carries settlers, engages nobody, and
  consumes no enemy unit.
**************************************************************************/
int deep_op_live_targets(const struct player *pplayer, int *out, int max,
                         bool committed_only)
{
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int oi, n = 0;

  for (oi = 0; oi < DEEP_OPS_MAX && n < max; oi++) {
    const struct deep_operation *op = &dp->ops[oi];

    if (op->state == DEEP_OP_NONE
        || op->kind == DEEP_OPK_LIFT
        || op->target_city <= 0) {
      continue;
    }
    if (committed_only
        && op->state != DEEP_OP_MARCH && op->state != DEEP_OP_STRIKE) {
      continue;
    }
    out[n++] = op->target_city;
  }

  return n;
}

/**********************************************************************//**
  Operations that reached MARCH over operations formed, 0 when nothing
  has formed yet. The launch-rate guard's own number.
**************************************************************************/
double deep_op_launch_rate(const struct player *pplayer)
{
  const struct deep_plr *dp = deep_plr_data(pplayer);

  if (dp->ops_formed <= 0) {
    return 0.0;
  }

  return (double) dp->ops_launched / dp->ops_formed;
}

/**********************************************************************//**
  The odds an assault on this target must clear, gate 2 included
  (W34 A.2): op_take_target scaled by what this player's stance
  charges for an assault, capped at 1.0 because it is a probability,
  and never scaled at all for a target that threatens us.

  Extracted so FORMATION and LAUNCH ask the same question -- a wave
  that formed at this bar and is now released by a head count is
  released against a garrison nobody re-priced. `count` is FALSE at
  the launch seat so the stance census keeps counting decisions, not
  re-reads of one decision.
**************************************************************************/
static double deep_op_assault_bar(struct player *pplayer,
                                  struct city *target, bool count)
{
  double bar = deep_tuning_get()->op_take_target;
  double omult = deep_stance_op_mult(pplayer);

  if (omult != 1.0 && !deep_op_target_threatens(pplayer, target)) {
    bar = MIN(1.0, bar * omult);
    if (count) {
      deep_plr_data(pplayer)->stance_op_gated++;
    }
  }

  return bar;
}

/**********************************************************************//**
  Observation only (ERA): what the formation seat SAW, whichever way
  it went.

  Today a target that is scored and refused leaves no trace at all --
  only the 83 formations and 72 odds-aborts of
  findings/era-tech50-forensics-2026-08-11.md 1.F are visible in six
  games. That makes the report's highest-value unknown unanswerable:
  "no target was ever near the bar" and "targets sat at 0.60 against a
  0.65 bar" produce identical logs, and they call for opposite fixes
  (a force-composition change versus a threshold re-derivation).

  `odds` is the best take-probability any prefix of the pool reached,
  taken from the scoring loop's own return values -- the model is
  never run a second time. `bar` is the stance-scaled bar the loop
  actually compared against; `take_target` is the unscaled knob beside
  it, so a refusal caused by the stance gate is distinguishable from
  one caused by the tuning.

  The defence census is a pure read of the target tile and is gated on
  the metric log level, the same way deep-anchor's counterfactual pick
  is: under FREECIV_DEEP_QUIET it does not run.

  MUSTER STATE (ERA 3b) rides on this counter rather than on a parallel
  one, because every question about a muster is a question about the
  same decision this line already reports: what force stood there, what
  odds it reached, and what the seat did about it. `op` is NULL where
  there is no slot to read (a refusal), and then the muster fields are
  zeros and `verdict` is one of the two the counter has always carried.

  The population changed and the field says so: a reader who wants the
  pre-3b population -- one line per formation attempt -- filters
  `verdict` to formed|refused, and the per-turn muster lines all carry
  muster=1. mform/mlaunch/mabandon are the player's running totals, so
  the launch-rate reality check is readable off any late line without
  re-aggregating the stream.
**************************************************************************/
static void deep_opodds_log(struct player *pplayer, int oi,
                            struct city *target, const char *verdict,
                            double odds, double bar, int required,
                            int pool, int drafted, int eta, bool overseas,
                            const struct deep_operation *op,
                            const char *why)
{
  const struct deep_plr *dp = deep_plr_data(pplayer);
  int def_n = 0, def_best = 0;

  if (!log_do_output_for_level(deep_metric_log_level())) {
    return;
  }
  unit_list_iterate(city_tile(target)->units, punit) {
    int rating;

    if (unit_owner(punit) != city_owner(target)
        || unit_has_type_flag(punit, UTYF_CIVILIAN)) {
      continue;
    }
    def_n++;
    rating = adv_unit_def_rating_basic(punit);
    if (rating > def_best) {
      def_best = rating;
    }
  } unit_list_iterate_end;

  log_base(deep_metric_log_level(),
           "deep-opodds turn=%d player=\"%s\" op=%d target=%d"
           " verdict=\"%s\" odds=%.3f bar=%.3f take_target=%.2f"
           " required=%d pool=%d drafted=%d eta=%d overseas=%d"
           " def_n=%d def_best=%d muster=%d members=%d best=%d stall=%d"
           " worth=%d why=%s mform=%d mlaunch=%d mabandon=%d",
           game.info.turn, player_name(pplayer), oi, target->id,
           verdict, odds, bar, deep_tuning_get()->op_take_target,
           required, pool, drafted, eta, overseas ? 1 : 0,
           def_n, def_best,
           op != NULL ? op->muster : 0,
           op != NULL ? op->n_members : 0,
           op != NULL ? op->muster_best : 0,
           op != NULL && op->muster
             ? game.info.turn - op->muster_grew : 0,
           op != NULL ? op->muster_worth : 0,
           why != NULL ? why : "none",
           dp->musters_formed, dp->musters_launched,
           dp->musters_abandoned);
}

/**********************************************************************//**
  ERA 3b's WORTH gate: is this target worth STARTING a muster for?

  The muster's whole claim is that a SET of blades is worth having
  where every increment is worth nothing, so the question it must ask
  at formation is the SET question -- not "can this pool take the
  city" (the odds test already said no, and saying it louder is 3a's
  mistake) but "is the take worth having at all".

  That question already has an answer in this file, at the launch
  seat: worth x P(take) minus what the assault is expected to cost,
  converted at SHIELD_WEIGHTING through attack_loss_gain (human call
  2026-08-06). This asks it with the only odds a muster ever launches
  at -- the bar itself, since clearing the bar IS the launch condition
  -- and the loss the force standing here today would take. Nothing
  new is invented: eff worth, the bar, attack_loss_gain and
  SHIELD_WEIGHTING are all already in the caller.

  Deliberately OPTIMISTIC about the loss, and named as such: a muster
  that grows to eight blades will lose more shields than today's two
  would. A gate whose job is to refuse worthless targets may be
  generous about the cost of good ones, and the launch seat re-asks
  the same question at full strength before anything marches.

  With op_net_value off there is no such arithmetic to borrow and the
  gate is the bare "worth anything at all" it degenerates to.
**************************************************************************/
static bool deep_muster_worth(struct ai_type *ait, struct player *pplayer,
                              struct city *target, struct unit *const *pool,
                              int n, int eta, double bar, double *worth_out)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double eff = deep_conquest_worth(ait, pplayer, target);
  double loss = 0.0, surv = 0.0;

  *worth_out = eff;
  if (eff <= 0.0) {
    return FALSE;
  }
  if (!tune->op_net_value || n < 1) {
    /* Nothing to price a loss with -- the collapse re-ask runs with an
     * empty staging tile whenever the whole muster is still walking in
     * -- so the gate is the bare worth test. Formation refuses an
     * empty pool on its own account, one branch up. */
    return TRUE;
  }
  (void) deep_take_odds_cost(&(wld.map), target, pool, n, eta,
                             &loss, &surv);

  return bar * eff - tune->attack_loss_gain * loss * SHIELD_WEIGHTING > 0.0;
}

/**********************************************************************//**
  Try to form a new operation in slot `oi`: is there a met, unclaimed
  target our free attackers could actually take? required = the
  smallest strongest-first prefix of the unclaimed pool that clears
  the formation bar. Returns TRUE if the slot went live.
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
  /* ERA 3b: this slot is entering MUSTER with the odds refused. */
  bool muster = FALSE;
  int muster_worth = 0;
  /* ERA 3a: the garrison draft's release test. 0 = the absolute
   * survival_target floor this function shipped with. */
  bool marginal_draft = tune->op_draft_marginal > 0;
  /* ERA 3b: the draft also releases blades that cost their city
   * NOTHING, whichever release test is in force -- the muster's feed,
   * since a blade it does not have to pay for is worth having in a
   * set that is not fighting yet. */
  bool muster_feed = tune->op_muster > 0;
  double bar;
  /* The best take-probability any prefix of the pool reached. Kept
   * from the scoring loop's own returns -- never a second call. */
  double best_odds = 0.0;

  /* The anchor everything below is measured from: targets are scored
   * worth/(1 + distance) from here, the members' continent is this
   * city's, and "overseas" means not this city's landmass.
   *
   * It used to be the FIRST city in pplayer->cities -- an arbitrary
   * city, in list order, which for most players is wherever the
   * capital happened to be founded and after a civil war is whatever
   * survived (military audit C5). Every operation this empire ever
   * formed was biased toward that one city's neighbourhood.
   *
   * The empire's centroid instead, taken as the medoid: the own city
   * with the smallest total distance to the others. A medoid needs no
   * arithmetic on wrapped coordinates, is always a real city on real
   * ground (so the pool and the staging search still mean something),
   * and on a one-city empire it is that city, which is the old
   * behaviour exactly. */
  {
    int best = FC_INFINITY;

    city_list_iterate(pplayer->cities, pcity) {
      int total = 0;

      city_list_iterate(pplayer->cities, other) {
        total += real_map_distance(city_tile(pcity), city_tile(other));
      } city_list_iterate_end;
      if (total < best) {
        best = total;
        home_city = pcity;
      }
    } city_list_iterate_end;
  }
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

    if (target != NULL
        && log_do_output_for_level(deep_metric_log_level())) {
      /* The anchor's own lens: what the list head would have picked
       * from the same menu. Pure read, metric-log gated, and the only
       * way to count target choices this change actually moved --
       * the two arms' games diverge, so a between-arm comparison of
       * chosen targets answers a different question. */
      struct city *head = NULL, *alt;

      city_list_iterate(pplayer->cities, pcity) {
        head = pcity;
        break;
      } city_list_iterate_end;
      alt = head != NULL
            ? deep_op_pick_target(ait, pplayer, city_tile(head)) : NULL;
      log_base(deep_metric_log_level(),
               "deep-anchor turn=%d player=\"%s\" op=%d medoid=%d"
               " head=%d target=%d alt=%d changed=%d",
               game.info.turn, player_name(pplayer), oi, home_city->id,
               head != NULL ? head->id : 0, target->id,
               alt != NULL ? alt->id : 0, alt != target ? 1 : 0);
    }
  }
  if (target == NULL) {
    return FALSE;
  }

  overseas = tile_continent(city_tile(target))
             != tile_continent(city_tile(home_city));

  /* Gate 2 (W34 A.2): what this player's stance charges for an
   * assault. The bar is the odds the smallest force must clear before
   * blades are committed; scaling it by (econ/mil)^stance_op_gain
   * means an economy-weighted player forms only where the take is
   * near-certain -- the OPPORTUNISTIC half of "low mil forms assaults
   * only against opportunistic or threatening targets" -- while a
   * force-weighted one forms freely. Exactly 1.0 at identity axes.
   *
   * Capped at 1.0 because it is a probability: however far the corner
   * or the gain is pushed, a certain take still forms. That cap is
   * the gate's floor, and the threatening exemption is the other one:
   * neither may be scaled away. */
  bar = deep_op_assault_bar(pplayer, target, TRUE);

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
    double odds = deep_take_odds_at_arrival(nmap, target, pool, i, eta);

    if (odds > best_odds) {
      best_odds = odds;
    }
    if (odds >= bar) {
      required = i;
      break;
    }
  }
  if (required == 0
      && (marginal_draft || tune->survival_target > 0.0)) {
    /* Garrison draft (T160 forensics): when the free pool cannot
     * clear the bar, borrow from the walls -- one sparable blade per
     * city, weakest defender first, and only where the city still
     * clears survival_target without it. The same leave-home-briefly
     * honesty gate the sorties use, pointed at reconquest: a nation
     * whose whole army is DEFEND_HOME infantry can still claim the
     * empty city two tiles away. Drafted blades are ordinary members
     * -- claimed, marched, re-gated, banked on dissolve.
     *
     * THE MARGINAL RELEASE (ERA 3a, op_draft_marginal = 1). The
     * absolute floor above was measured over 11 232 formation
     * attempts and it is what starves the pool: the draft fires on
     * 19.4% of attempts and adds 0.28 units, because only 39.9% of
     * survival reads clear survival_target at all, while 44% of
     * attempts reach the odds model with NOTHING in the pool
     * (findings/era-probe-evidence-2026-08-12.md section 1). A floor
     * asks "is this city safe enough to lend", which refuses hardest
     * exactly where the blade is worth least -- in a city already so
     * pressed that one more defender does not save it.
     *
     * The comparison replaces the floor with the trade the decision
     * actually is: the survival the blade COSTS its own city against
     * the take-probability it BUYS the operation. Both are
     * probabilities already on hand -- survival with the blade minus
     * survival without it, whole-pool odds with the blade appended
     * minus whole-pool odds without -- so nothing is invented and no
     * scale sits between them. A city that holds fine without the
     * blade pays almost nothing and lends; a city the blade genuinely
     * saves prices its own survival above any odds an assault can
     * offer and keeps it. `held` is carried across the loop, so each
     * blade is priced against the pool the previous ones already
     * built. */
    double held = (marginal_draft && n > 0)
                  ? deep_take_odds_at_arrival(nmap, target, pool, n, eta)
                  : 0.0;

    city_list_iterate(pplayer->cities, pcity) {
      struct unit *weak = NULL;
      struct deep_cover_move cover;
      bool covered;

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
      if (weak == NULL) {
        continue;
      }
      /* THE COST SIDE, priced after rearrangement (NETTHREAT item 4).
       * Both release tests below ask what the lending city loses, and
       * both asked it as if the rest of the lattice were nailed down.
       * With cover_removal on, the question is the post-rearrangement
       * one: what remains once the cheapest one-hop neighbour has
       * backfilled, charged the lender's own loss as well.
       *
       * Priced, NOT ordered -- unlike the sortie, this formation can
       * still refuse (`required` may never clear the bar), and
       * ordering a march for a hole that never opens is exactly the
       * trickle the muster discipline exists to prevent. The blades
       * that do get drafted march as claimed members and the
       * counter-claim pass sees whatever hole is really left. */
      covered = deep_cover_removal_cost(ait, nmap, pcity, weak, &cover);
      if (marginal_draft) {
        double cost
          = covered
            ? cover.cost
            : deep_city_stack_survival(nmap, pcity, tune->survival_horizon,
                                       NULL)
              - deep_city_stack_survival_without(nmap, pcity,
                                                 tune->survival_horizon,
                                                 weak, NULL);
        double with;

        pool[n] = weak;
        with = deep_take_odds_at_arrival(nmap, target, pool, n + 1, eta);
        /* THE FREE BLADE (ERA 3b). The marginal comparison still
         * cannot bootstrap: in the flat region `with - held` is
         * exactly 0 for every blade in the set, so a positive cost
         * always wins and nothing is ever released. A muster does not
         * need the blade to buy odds, because it is not fighting this
         * turn -- it needs the blade to be FREE. Cost of exactly zero
         * is that, with no epsilon to invent: the city's survival is
         * the same number with the blade and without it. */
        if (with - held > cost || (muster_feed && cost <= 0.0)) {
          n++;
          drafted++;
          held = with;
        }
      } else {
        double surv_after
          = covered
            ? cover.surv_after
            : deep_city_stack_survival_without(nmap, pcity,
                                               tune->survival_horizon,
                                               weak, NULL);

        if (surv_after >= tune->survival_target) {
          pool[n++] = weak;
          drafted++;
        } else if (muster_feed) {
          /* The free blade again, under the absolute floor -- and this
           * is the population the probe named: a city the floor
           * refuses BECAUSE it is already below the bar, whose blade
           * changes its survival by nothing at all. The floor reads
           * that as "too dangerous to lend" when what it means is
           * "this blade is not what saves this city". */
          double cost
            = covered
              ? cover.cost
              : deep_city_stack_survival(nmap, pcity,
                                         tune->survival_horizon, NULL)
                - surv_after;

          if (cost <= 0.0) {
            pool[n++] = weak;
            drafted++;
          }
        }
      }
    } city_list_iterate_end;
    if (drafted > 0) {
      for (i = 1; i <= n; i++) {
        double odds = deep_take_odds_at_arrival(nmap, target, pool, i, eta);

        if (odds > best_odds) {
          best_odds = odds;
        }
        if (odds >= bar) {
          required = i;
          break;
        }
      }
    }
  }
  if (required < 1) {
    /* THE MUSTER (ERA 3b). No prefix of the pool clears the bar --
     * and on this venue that is the answer 98.2% of the time, with
     * 84.5% of the refusals returning odds of EXACTLY zero. The odds
     * model is not wrong to say it: at Gunpowder a city held by one
     * defender needs 8-15 attackers to be even 36% likely to clear
     * the bar, and deep brings a mean of 2.69.
     *
     * So the slot does not dissolve. If the target clears the worth
     * gate it enters MUSTER anyway, gathers across turns, and lets
     * the odds test be the LAUNCH gate it already is -- the set is
     * priced when it is complete instead of one worthless increment
     * at a time.
     *
     * Two things it will not do. It will not muster on an empty pool:
     * a slot with no members is dissolved by the validation pass on
     * the next turn regardless, so an empty muster is a log line
     * pretending to be a plan. And it will not muster without
     * op_launch_odds, because the launch gate would then be a head
     * count against a `required` no prefix ever justified -- which is
     * precisely the trickle the muster discipline exists to
     * prevent. */
    double eff = 0.0;

    if (tune->op_muster <= 0 || tune->op_launch_odds <= 0 || n < 1
        || !deep_muster_worth(ait, pplayer, target, pool, n, eta, bar,
                              &eff)) {
      deep_opodds_log(pplayer, oi, target, "refused", best_odds, bar,
                      0, n, drafted, eta, overseas, NULL, NULL);
      return FALSE;
    }
    muster = TRUE;
    muster_worth = (int) eff;
    dp->musters_formed++;
  }
  /* required == 1 forms too (T160 forensics): "one unit needs no
   * operation" assumed stock would claim singleton targets, but
   * stock garrisons sit at DEFEND_HOME and never walk into even an
   * EMPTY city -- 47 of the 52 barbarian-held cities at T160 had no
   * garrison and no suitor. The op machinery gives the singleton
   * exactly what it needs: a claimed unit, a march, a re-gate. */

  op->state = DEEP_OP_MUSTER;
  op->kind = DEEP_OPK_ASSAULT;
  if (!muster) {
    /* A muster is NOT counted as a formed operation until it launches.
     * ops_formed/ops_launched drive the never-launches damper (CONSEQ
     * item 5), whose quantity is "operations formed that never reached
     * MARCH" -- and a muster sitting unlaunched while it grows is the
     * mechanism working, not the pathology the damper was built to
     * notice. Feeding it here would starve the attacker pump exactly
     * where the muster needs it most. The muster's own three counters
     * are the ones that make a unit sink visible. */
    dp->ops_formed++;
  }
  op->target_city = target->id;
  op->staging_city = staging->id;
  /* A muster has no quorum: no prefix of the pool ever cleared the
   * bar, so any head count would be a number nothing justified. */
  op->required = muster ? 0 : required;
  op->formed_turn = game.info.turn;
  op->muster = muster ? 1 : 0;
  op->muster_worth = muster_worth;
  /* Enroll the required force plus a small margin; the rest of the
   * pool stays free as the designated reserve (E7) -- the second
   * wave if the first falls short, ordinary units otherwise. A
   * muster takes the whole pool instead: accumulating the set IS the
   * plan, and a reserve held back from a force that cannot yet fight
   * is a reserve for nothing. */
  op->n_members = muster
    ? MIN(n, DEEP_OP_MEMBERS)
    : MIN(MIN(n, required + (required + 1) / 2), DEEP_OP_MEMBERS);
  for (i = 0; i < op->n_members; i++) {
    op->members[i] = pool[i]->id;
  }
  op->muster_best = op->n_members;
  op->muster_grew = game.info.turn;
  log_base(deep_metric_log_level(),
           "deep-op turn=%d player=\"%s\" op=%d %s target=%d staging=%d"
           " required=%d pool=%d drafted=%d wave2=%d",
           game.info.turn, player_name(pplayer), oi,
           muster ? "muster" : "form", op->target_city,
           op->staging_city, op->required, op->n_members, drafted,
           dp->last_strike_city == target->id ? 1 : 0);
  deep_opodds_log(pplayer, oi, target, muster ? "muster" : "formed",
                  best_odds, bar, required, n, drafted, eta, overseas,
                  op, NULL);

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
    /* A retarget clears the muster flag (ERA 3b): the alternative had
     * to clear the FULL formation bar at the force's own eta to be
     * picked at all, so what comes out of here is an ordinary wave
     * with a real quorum, whatever went in. That is also the moment
     * the operation becomes a formed one for the damper's census --
     * a muster is counted when it stops being a muster, never
     * before, so ops_launched can never outrun ops_formed. */
    if (op->muster) {
      deep_plr_data(pplayer)->ops_formed++;
    }
    op->muster = 0;
    op->muster_worth = 0;
    op->muster_best = 0;
    op->muster_grew = game.info.turn;
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
  /* Both sides of this comparison carry their own stakes multiplier
   * (human call 2026-08-06). Keeping our city is worth what we keep
   * PLUS what we deny the taker, exactly as taking theirs is worth
   * what we gain plus what we deny -- so the defence-side read gets
   * defence_worth_mult and the op's expected take gets
   * conquest_worth_mult, once each.
   *
   * That is what makes the RATIO the tilt and 1:1 a no-op: at equal
   * multipliers this comparison is algebraically identical to the
   * pre-rider one, however large both are. Multiplying only one side
   * would have made "symmetric stakes" mean "double the defence",
   * which is a tilt wearing a symmetry's clothes. */
  double dmult = tune->defence_worth_mult;
  double raw_own = def_ai_city_data(pcity, ait)->worth;
  double city_risk = raw_own * dmult * (1.0 - p_survival);
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
    if (city_risk <= deep_conquest_worth(ait, pplayer, target) * odds) {
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
               " city=%d target=%d odds=%.2f worth=%.1f dworth=%.1f"
               " risk=%.1f",
               game.info.turn, player_name(pplayer), oi,
               unit_rule_name(blade), pcity->id, op->target_city, odds,
               raw_own, raw_own * dmult, city_risk);
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
    /* Removed from the members below, so the op no longer holds it:
     * without a phase hold of its own the manage loop clears this
     * fresh DEFEND_HOME within the same turn and dai_set_defenders()
     * re-decides who garrisons the city we just took. */
    deep_hold_unit(ait, gar, "garrison");
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
  This unit's win chance if it struck FRESH -- the counterfactual the
  tired-attack decision is about. Same trick, and the same honesty
  note, as deep_fresh_attack_power() in deepcombat.c: moves_left is
  restored before anything else can observe it.
**************************************************************************/
static double deep_fresh_win_chance(const struct civ_map *nmap,
                                    struct unit *att,
                                    const struct unit *def)
{
  int save = att->moves_left;
  double p;

  if (save < SINGLE_MOVE) {
    att->moves_left = SINGLE_MOVE;
  }
  p = unit_win_chance(nmap, att, def, NULL);
  att->moves_left = save;

  return p;
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

    /* Tired-attack discipline, at the seat where a unit is actually
     * tired (D1 hand-off). The original pass ran in first_activities,
     * where update_unit_activities() has just restored every unit's
     * movement, so its moves_left < SINGLE_MOVE test could never be
     * true -- zero firings in 206 result directories, and no
     * done-flag fix could have revived it. A unit becomes tired by
     * MARCHING, and the op's own battle is the moment the decision
     * exists: strike now at moves_left/SINGLE_MOVE of full power
     * (combat.c), or stand and strike fresh next turn.
     *
     * Held only when resting actually changes the answer: the tired
     * odds are below the floor AND the fresh odds are not. A blade
     * that is hopeless either way strikes -- a wounded straggler does
     * not earn a night's rest -- and one that already clears the
     * floor never waits. */
    if (game.info.tired_attack
        && tune->tired_attack_floor > 0.0
        && att->moves_left > 0
        && att->moves_left < SINGLE_MOVE
        && best < tune->tired_attack_floor) {
      struct unit *pdef = get_defender(nmap, att, ctile, NULL);
      double fresh = pdef != NULL
                     ? deep_fresh_win_chance(nmap, att, pdef) : 1.0;

      if (fresh >= tune->tired_attack_floor) {
        log_base(deep_metric_log_level(),
                 "deep-tired turn=%d player=\"%s\" seat=strike"
                 " unit=\"%s\" moves=%d p=%.3f fresh=%.3f target=%d",
                 game.info.turn, player_name(pplayer),
                 unit_rule_name(att), att->moves_left, best, fresh,
                 op->target_city);
        deep_hold_unit(ait, att, "tired");
        continue;
      }
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
  ERA 3b: grow a muster. Every free attacker standing on the staging
  city's continent that no live operation has claimed joins it, up to
  the storage bound.

  This is the memoryless half of 3a's falsification answered. Formation
  ran 0.88 times per player-turn, each pass asking "can I win NOW",
  each pass leaving nothing behind -- so the pool a target faced was
  whatever happened to be idle that instant, mean 2.69 against a
  requirement of 8-15. A muster keeps what it gathered and adds to it,
  which is the only way a force that big is ever standing in one place
  on this venue.

  The selector is deep_op_collect_pool() unchanged, so a muster claims
  exactly what a formation would have claimed and nothing more:
  military, attack-rated, not bodyguarding, NOT AIUNIT_DEFEND_HOME, and
  not already claimed -- members of this op included, since a claim is
  a claim. Garrisons are never conscripted here; the only path from the
  walls into a muster is the draft's own release test.

  Returns how many joined.
**************************************************************************/
static int deep_muster_enroll(struct ai_type *ait, struct player *pplayer,
                              struct deep_operation *op,
                              struct city *staging)
{
  struct unit *pool[DEEP_OP_MEMBERS];
  int n, i, joined = 0;

  if (op->n_members >= DEEP_OP_MEMBERS) {
    return 0;
  }
  n = deep_op_collect_pool(ait, pplayer,
                           tile_continent(city_tile(staging)),
                           pool, DEEP_OP_MEMBERS);
  for (i = 0; i < n && op->n_members < DEEP_OP_MEMBERS; i++) {
    op->members[op->n_members++] = pool[i]->id;
    joined++;
  }

  return joined;
}

/**********************************************************************//**
  ERA 3b's three abandon triggers, in the brief's own order of kinds.
  Returns the dissolve reason, or NULL to keep mustering.

  A muster with no end is a unit sink, and the sink is the mechanism's
  own worst case: blades that would have garrisoned or trickled into
  stock's attacks instead stand in a yard forever. So the muster is
  told, in three ways, when to stop.

  1. STALL -- op_muster_stall turns without a new high-water member
     count. Free to evaluate, and the only trigger that fires on a
     muster nothing is wrong with except that it is not working.

  2. WORTH -- the formation gate, re-asked. The target may have been
     taken by somebody else, walled, or grown a garrison that turns
     the take into a loss; the same arithmetic that admitted the
     muster is what evicts it.

  3. HOME -- the defence demand at the muster point spiking. Priced by
     deep_cover_removal_cost(), whose header has named this consumer
     since NETTHREAT: what the staging city keeps once the wave's best
     defender leaves and the cheapest one-hop backfill arrives. Below
     survival_target and the force is not free to march, so it is not
     a wave -- it is a garrison holding a piece of paper.

     ONE BLADE, not the whole wave: a proxy for a departure that has
     not been decided and may never be, at the cost of one pricing
     call per muster per turn. It understates the hole the launch
     would open, which errs toward keeping the muster.

  cover_removal moves trigger 3's cost side exactly as it moves the
  garrison draft's; that overlap is a factorial fact, not an accident,
  and the A/B design has to name which arms carry it.
**************************************************************************/
static const char *deep_muster_abandon(struct ai_type *ait,
                                       struct player *pplayer,
                                       const struct deep_operation *op,
                                       struct city *target,
                                       struct city *staging,
                                       struct unit *const *here, int present)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct deep_cover_move cover;
  struct unit *blade = NULL;
  double worth = 0.0, surv_after;
  int i;

  if (game.info.turn - op->muster_grew >= tune->op_muster_stall) {
    return "muster-stall";
  }
  if (!deep_muster_worth(ait, pplayer, target, here, present,
                         deep_op_eta(pplayer, op, target),
                         deep_op_assault_bar(pplayer, target, FALSE),
                         &worth)) {
    return "muster-worth";
  }
  if (!tune->op_muster_home || tune->survival_target <= 0.0 || present < 1) {
    return NULL;
  }
  for (i = 0; i < present; i++) {
    if (blade == NULL
        || adv_unit_def_rating_basic(here[i])
           > adv_unit_def_rating_basic(blade)) {
      blade = here[i];
    }
  }
  surv_after
    = deep_cover_removal_cost(ait, nmap, staging, blade, &cover)
      ? cover.surv_after
      : deep_city_stack_survival_without(nmap, staging,
                                         tune->survival_horizon, blade,
                                         NULL);
  if (surv_after < tune->survival_target) {
    return "muster-home";
  }

  return NULL;
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
    struct unit *here[DEEP_OP_MEMBERS];
    double odds_now = 0.0, surv_now = 0.0, loss_now = 0.0;
    const char *abandon = NULL;
    int joined = 0;
    bool go;

    if (op->muster) {
      joined = deep_muster_enroll(ait, pplayer, op, staging);
      /* The stall clock measures GROWTH, and growth means a new
       * high-water mark. A muster that loses two blades to the
       * counter-claim and enrolls two more has not grown -- it has
       * held station, and holding station is exactly the state the
       * clock exists to end. */
      if (op->n_members > op->muster_best) {
        op->muster_best = op->n_members;
        op->muster_grew = game.info.turn;
      }
      if (joined > 0) {
        log_base(deep_metric_log_level(),
                 "deep-op turn=%d player=\"%s\" op=%d grow target=%d"
                 " joined=%d members=%d best=%d",
                 game.info.turn, player_name(pplayer), oi, op->target_city,
                 joined, op->n_members, op->muster_best);
      }
    }

    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);

      if (m != NULL && same_pos(unit_tile(m), city_tile(staging))) {
        here[present++] = m;
      }
    }

    /* The launch bar in the currency the formation bar was set in.
     *
     * A head count is the linear-sum error the substrate exists to
     * replace: `required` was computed against the garrison of the
     * turn the operation FORMED, and by the time the bodies are
     * counted the target has had op_muster_wait turns to build. Ask
     * the force that is actually standing here what it can take --
     * and, now that the walk reports what the assault costs, whether
     * enough of it would still be standing afterwards to hold what it
     * takes. A wave that wins the city and dies doing it hands the
     * city back next turn.
     *
     * The wait cap keeps its half-quorum head count: that path is
     * the deliberate "go with what came" and it is a patience rule,
     * not an odds one. */
    if (tune->op_launch_odds && present > 0) {
      int eta_here = 0;

      for (i = 0; i < present; i++) {
        int rate = MAX(unit_type_get(here[i])->move_rate, 1);
        int ti = (real_map_distance(unit_tile(here[i]),
                                    city_tile(target)) * SINGLE_MOVE
                  + rate - 1) / rate;

        eta_here = MAX(eta_here, ti);
      }
      odds_now = deep_take_odds_cost(nmap, target, here, present,
                                     eta_here, &loss_now, &surv_now);
      go = odds_now >= deep_op_assault_bar(pplayer, target, FALSE)
           && surv_now >= tune->op_hold_survivors;

      /* And then the question the BAR cannot ask: is the whole fight
       * worth having (human call 2026-08-06).
       *
       * The build want prices a MARGINAL blade and therefore charges a
       * marginal loss, clamped so it can never become a subsidy. Here
       * the decision is not marginal at all -- launch this wave at
       * this city, or do not -- so the honest form is the TOTAL one:
       * what the take is worth times the chance of getting it, less
       * what the whole assault is expected to cost. Not launching is
       * the zero, so odds_now IS the probability difference.
       *
       * `worth x odds` is the operations layer's existing currency for
       * a take (deep_op_defend_bid weighs a threatened city against
       * exactly that product), and the losses convert at
       * SHIELD_WEIGHTING through attack_loss_gain -- the same rate and
       * the same referee the build side uses, so the two seats cannot
       * disagree about what a shield is worth.
       *
       * A bar-passing wave refused here is the interesting event and
       * it is the one that gets logged: this counter is by
       * construction the launches the odds bar alone WOULD have
       * taken. */
      if (go && tune->op_net_value) {
        /* Same conquest worth as the build side, applied once here:
         * taking the city gains us its value and denies it to the
         * owner (human call 2026-08-06), plus the culture stock that
         * changes hands with it (BALANCE). */
        double raw_worth = def_ai_city_data(target, ait)->worth;
        double eff_worth = deep_conquest_worth(ait, pplayer, target);
        double value = odds_now * eff_worth;
        double cost = tune->attack_loss_gain * loss_now * SHIELD_WEIGHTING;

        if (value - cost <= 0.0) {
          log_base(deep_metric_log_level(),
                   "deep-netval turn=%d player=\"%s\" op=%d refuse"
                   " target=%d force=%d odds=%.3f worth=%.1f"
                   " eworth=%.1f loss=%.2f value=%.1f cost=%.1f",
                   game.info.turn, player_name(pplayer), oi,
                   op->target_city, present, odds_now, raw_worth,
                   eff_worth, loss_now, value, cost);
          go = FALSE;
        }
      }
    } else {
      /* ERA 3b: a muster has no quorum to count against, and
       * `required` 0 would read as "everybody is here" on an empty
       * staging tile. Forming one already requires op_launch_odds, so
       * this branch is unreachable for a muster; the test says so
       * rather than relying on it. */
      go = !op->muster && present >= op->required;
    }

    /* The three abandon triggers, asked only of a muster that is not
     * launching this turn: a wave about to march is not a unit sink,
     * whatever its stall clock says. */
    if (op->muster && !go) {
      abandon = deep_muster_abandon(ait, pplayer, op, target, staging,
                                    here, present);
    }

    if (go
        /* The half-quorum patience release is for a wave that formed
         * against a priced quorum. A muster never formed against one
         * (ERA 3b): its release is the odds test, and its patience
         * rule is the stall clock, not this one. */
        || (!op->muster
            && game.info.turn - op->formed_turn >= tune->op_muster_wait
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
      if (op->muster) {
        /* The muster became an operation at the instant it launched
         * (ERA 3b): counted into BOTH censuses here, never into
         * ops_formed while it was still gathering, so the damper's
         * "formed but never marched" quantity keeps its meaning and
         * ops_launched cannot outrun ops_formed. */
        deep_plr_data(pplayer)->ops_formed++;
        dp->musters_launched++;
        deep_opodds_log(pplayer, oi, target, "muster-launch", odds_now,
                        deep_op_assault_bar(pplayer, target, FALSE),
                        op->required, present, 0,
                        deep_op_eta(pplayer, op, target),
                        deep_op_crossing(op, target), op, NULL);
        op->muster = 0;
      }
      deep_plr_data(pplayer)->ops_launched++;
      deep_brief_allies(pplayer, target, TRUE, op->march_eta);
      log_base(deep_metric_log_level(),
               "deep-op turn=%d player=\"%s\" op=%d march target=%d"
               " present=%d required=%d waited=%d eta=%d odds=%.3f"
               " surv=%.2f bar=%d",
               game.info.turn, player_name(pplayer), oi, op->target_city,
               present, op->required, game.info.turn - op->formed_turn,
               op->march_eta, odds_now, surv_now,
               tune->op_launch_odds ? 1 : 0);
      /* Fall through: the first march turn re-gates immediately. */
    } else if (abandon != NULL) {
      /* Dissolve rather than retarget: a retarget is a fresh
       * commitment at the FULL bar, which a muster by definition
       * cannot clear, and laundering an abandoned muster into another
       * one is how a unit sink hides from its own counters. Members
       * are banked, as on every deliberate abort. */
      dp->musters_abandoned++;
      deep_opodds_log(pplayer, oi, target, "muster-abandon", odds_now,
                      deep_op_assault_bar(pplayer, target, FALSE),
                      op->required, present, 0,
                      deep_op_eta(pplayer, op, target),
                      deep_op_crossing(op, target), op, abandon);
      deep_op_dissolve(op, oi, abandon, pplayer);
      return;
    } else if (!op->muster
               && game.info.turn - op->formed_turn >= tune->op_muster_wait) {
      /* Wait cap without even half a quorum: before banking, see if
       * the force that DID gather can take something else from
       * where it stands. (A muster is not on this clock -- it answers
       * to the stall clock and the two other abandon triggers.) */
      if (!deep_op_retarget(ait, pplayer, op, oi)) {
        deep_op_dissolve(op, oi, "abort-wait", pplayer);
      }
      return;
    } else {
      if (op->muster) {
        /* One line per muster per turn, carrying the odds the force
         * standing here actually reaches. This is the pre-registered
         * falsification's own instrument: 3a asked whether a bigger
         * pool moves the odds histogram out of the "exactly 0" bin,
         * and only a per-turn read of a GROWING pool against the same
         * target can answer it. */
        deep_opodds_log(pplayer, oi, target, "muster-hold", odds_now,
                        deep_op_assault_bar(pplayer, target, FALSE),
                        op->required, present, joined,
                        deep_op_eta(pplayer, op, target),
                        deep_op_crossing(op, target), op, NULL);
      }
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

  deep_conquest_culture_census(pplayer);
}
