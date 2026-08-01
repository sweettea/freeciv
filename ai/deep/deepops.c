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
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* server */
#include "maphand.h"
#include "srv_log.h"

/* server/advisors */
#include "advgoto.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daiplayer.h"
#include "daitools.h"
#include "daiunit.h"

#include "deepai.h"
#include "deepcombat.h"
#include "deeptuning.h"

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
    for (i = 0; i < op->n_members; i++) {
      if (op->members[i] == unit_id) {
        return oi;
      }
    }
  }

  return -1;
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
static void deep_op_dissolve(struct deep_operation *op, int oi,
                             const char *why, struct player *pplayer)
{
  log_base(deep_metric_log_level(),
           "deep-op turn=%d player=\"%s\" op=%d %s target=%d members=%d",
           game.info.turn, player_name(pplayer), oi, why, op->target_city,
           op->n_members);
  op->state = DEEP_OP_NONE;
  op->n_members = 0;
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
  int i, hunting = 0;

  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);
    struct unit_ai *ud;
    struct unit *victim;

    if (m == NULL) {
      continue;
    }
    ud = def_ai_unit_data(m, ait);
    if (ud->task == AIUNIT_ATTACK) {
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
      hunting++;
    }
  }

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
  struct city *target = game_city_by_number(op->target_city);
  int i;

  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (m == NULL) {
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
static int deep_op_eta(const struct deep_operation *op,
                       const struct city *target)
{
  int t[DEEP_OP_MEMBERS];
  int i, j, n = 0, k;

  for (i = 0; i < op->n_members; i++) {
    const struct unit *m = game_unit_by_number(op->members[i]);
    int rate, ti;

    if (m == NULL) {
      continue;
    }
    rate = MAX(unit_type_get(m)->move_rate, 1);
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
  struct tile *ctile = city_tile(target);
  struct unit *stream[DEEP_OP_MEMBERS];
  int i, ns = 0;

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
   * The op owns them; stock gets them back when it banks or wins. */
  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);
    int m_id;

    if (m == NULL || m->hp <= 0) {
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
    }
    if (m != NULL) {
      def_ai_unit_data(m, ait)->done = TRUE;
    }
  }

  if (city_owner(target) != pplayer
      && game.info.turn - op->phase_turn >= tune->op_muster_wait) {
    /* The walls held past our patience: bank the survivors. */
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
      bool overseas = tile_continent(city_tile(target))
                      != tile_continent(city_tile(staging));

      deep_op_launch(ait, pplayer, op);
      if (overseas) {
        /* Amphibious: the ferry system and the convoy gates take it
         * from here; sea phases are a later slice. The E7 clock has
         * to start now -- the op no longer watches the crossing. */
        log_base(deep_metric_log_level(),
                 "deep-op turn=%d player=\"%s\" op=%d strike target=%d"
                 " present=%d required=%d waited=%d",
                 game.info.turn, player_name(pplayer), oi,
                 op->target_city, present, op->required,
                 game.info.turn - op->formed_turn);
        dp->last_strike_city = op->target_city;
        dp->last_strike_turn = game.info.turn;
        deep_op_dissolve(op, oi, "launched", pplayer);
        return;
      }
      op->state = DEEP_OP_MARCH;
      op->march_turn = game.info.turn;
      op->march_eta = MAX(deep_op_eta(op, target), 1);
      log_base(deep_metric_log_level(),
               "deep-op turn=%d player=\"%s\" op=%d march target=%d"
               " present=%d required=%d waited=%d eta=%d",
               game.info.turn, player_name(pplayer), oi, op->target_city,
               present, op->required, game.info.turn - op->formed_turn,
               op->march_eta);
      /* Fall through: the first march turn re-gates immediately. */
    } else if (game.info.turn - op->formed_turn >= tune->op_muster_wait) {
      /* Wait cap without even half a quorum: bank the survivors. */
      deep_op_dissolve(op, oi, "abort-wait", pplayer);
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
    eta = deep_op_eta(op, target);

    if (eta <= 1) {
      /* Contact: the wave stands at the walls. The op takes its
       * members back from the stock attack path and fights the
       * battle itself; the E7 second-wave window opens now. */
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
         * the eta never closes. Same patience knob as the muster. */
        deep_op_dissolve(op, oi, "abort-march", pplayer);
      }
      return;
    }
  }

  if (op->state == DEEP_OP_STRIKE) {
    deep_op_drive_strike(ait, pplayer, op, oi);
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
    if (target == NULL
        || !pplayers_at_war(pplayer, city_owner(target))
        || op->n_members == 0
        /* A muster needs its staging city; a marching wave has
         * already left it behind. */
        || (op->state == DEEP_OP_MUSTER
            && (staging == NULL || city_owner(staging) != pplayer))) {
      deep_op_dissolve(op, oi, "abort", pplayer);
    }
  }

  /* Form at most one assault and one raid per turn; assault first,
   * so the army's claim on fresh units outranks the hunt's. Fresh
   * raiders merge into a standing raid op rather than burning
   * another slot -- one hunt container per player. */
  {
    bool tried_assault = FALSE, tried_raid = FALSE;

    for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
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
    for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
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

  for (oi = 0; oi < DEEP_OPS_MAX; oi++) {
    if (dp->ops[oi].state == DEEP_OP_NONE) {
      continue;
    }
    if (dp->ops[oi].kind == DEEP_OPK_RAID) {
      deep_op_drive_raid(ait, pplayer, &dp->ops[oi], oi);
    } else {
      deep_op_drive(ait, pplayer, &dp->ops[oi], oi);
    }
  }
}
