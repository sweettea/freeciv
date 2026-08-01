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

      if (!map_is_known(city_tile(acity), pplayer)
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
  not already spoken for by `except_op`.
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
        || ud->charge > 0) {
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
static void deep_op_dissolve(struct deep_operation *op, const char *why,
                             struct player *pplayer)
{
  log_base(deep_metric_log_level(),
           "deep-op turn=%d player=\"%s\" %s target=%d members=%d",
           game.info.turn, player_name(pplayer), why, op->target_city,
           op->n_members);
  op->state = DEEP_OP_NONE;
  op->n_members = 0;
}

/**********************************************************************//**
  The per-turn operation driver (combat E6, smallest slice): one
  operation per player. Form when enough free attackers EXIST to
  clear op_take_target against the best met target; muster them at
  the staging city nearest the target; when the quorum stands there
  (or op_muster_wait runs out with at least half), launch everyone
  at the target through the stock attack path -- they leave together,
  so they arrive as a wave instead of stock's one-at-a-time trickle.
  Launching dissolves the operation: fire and forget, second waves
  are E7's business.
**************************************************************************/
void deep_run_operations(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);
  struct deep_operation *op = &deep_plr_data(pplayer)->op;
  struct city *target, *staging;
  struct unit *pool[DEEP_OP_MEMBERS];
  int n, i, present;

  if (tune->op_take_target <= 0.0) {
    return;
  }

  /* Validate a live operation: prune dead members, drop the op if
   * the world moved on. */
  if (op->state == DEEP_OP_MUSTER) {
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
    if (target == NULL || staging == NULL
        || city_owner(staging) != pplayer
        || !pplayers_at_war(pplayer, city_owner(target))
        || op->n_members == 0) {
      deep_op_dissolve(op, "abort", pplayer);
    }
  }

  if (op->state == DEEP_OP_NONE) {
    /* Form: is there a met target our free attackers could actually
     * take? required = the smallest strongest-first prefix of the
     * pool that clears op_take_target. */
    struct city *home_city = NULL;
    int required = 0;

    city_list_iterate(pplayer->cities, pcity) {
      home_city = pcity;
      break;
    } city_list_iterate_end;
    if (home_city == NULL) {
      return;
    }
    target = deep_op_pick_target(ait, pplayer, city_tile(home_city));
    if (target == NULL) {
      return;
    }
    {
      bool overseas
        = tile_continent(city_tile(target))
          != tile_continent(city_tile(home_city));
      int eta;

      /* Staging: nearest own city on the target's continent for a
       * land assault; for an amphibious one, the nearest own COASTAL
       * city on the members' continent -- the convoy gates (naval
       * slice 3) own the crossing from there. */
      staging = NULL;
      {
        int best = FC_INFINITY;

        city_list_iterate(pplayer->cities, pcity) {
          int d = real_map_distance(city_tile(pcity),
                                    city_tile(target));

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
        return;
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
      if (required <= 1) {
        /* Nothing to coordinate: no force clears the bar, or one
         * unit does and needs no operation for it. */
        return;
      }
    }
    op->state = DEEP_OP_MUSTER;
    op->target_city = target->id;
    op->staging_city = staging->id;
    op->required = required;
    op->formed_turn = game.info.turn;
    op->n_members = MIN(n, DEEP_OP_MEMBERS);
    for (i = 0; i < op->n_members; i++) {
      op->members[i] = pool[i]->id;
    }
    log_base(deep_metric_log_level(),
             "deep-op turn=%d player=\"%s\" form target=%d staging=%d"
             " required=%d pool=%d",
             game.info.turn, player_name(pplayer), op->target_city,
             op->staging_city, op->required, op->n_members);
  }

  if (op->state != DEEP_OP_MUSTER) {
    return;
  }

  /* Muster: walk members in, hold arrivals. */
  target = game_city_by_number(op->target_city);
  staging = game_city_by_number(op->staging_city);
  present = 0;
  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);

    if (m != NULL && same_pos(unit_tile(m), city_tile(staging))) {
      present++;
    }
  }

  if (present >= op->required
      || (game.info.turn - op->formed_turn >= tune->op_muster_wait
          && present >= (op->required + 1) / 2)) {
    /* Launch: everyone at the target through the stock attack path.
     * They leave together; that was the whole point. */
    log_base(deep_metric_log_level(),
             "deep-op turn=%d player=\"%s\" strike target=%d present=%d"
             " required=%d waited=%d",
             game.info.turn, player_name(pplayer), op->target_city,
             present, op->required, game.info.turn - op->formed_turn);
    for (i = 0; i < op->n_members; i++) {
      struct unit *m = game_unit_by_number(op->members[i]);

      if (m == NULL) {
        continue;
      }
      dai_unit_new_task(ait, m, AIUNIT_ATTACK, city_tile(target));
      (void) dai_gothere(ait, pplayer, m, city_tile(target));
    }
    deep_op_dissolve(op, "launched", pplayer);
    return;
  }

  if (game.info.turn - op->formed_turn >= tune->op_muster_wait) {
    /* Wait cap without even half a quorum: bank the survivors. */
    deep_op_dissolve(op, "abort-wait", pplayer);
    return;
  }

  for (i = 0; i < op->n_members; i++) {
    struct unit *m = game_unit_by_number(op->members[i]);
    struct unit_ai *ud;

    if (m == NULL) {
      continue;
    }
    ud = def_ai_unit_data(m, ait);
    if (same_pos(unit_tile(m), city_tile(staging))) {
      ud->done = TRUE;          /* Hold at the muster point. */
    } else if (m->moves_left > 0) {
      if (dai_unit_goto(ait, m, city_tile(staging))) {
        ud->done = TRUE;
      }
    }
  }
}
