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

/* deep's copy of assess_danger() (ai/default/daimilitary.c:709) and
 * assess_danger_unit() (daimilitary.c:536). Copied rather than called
 * (rule 3, last resort) because the per-city pf_reverse_map stock
 * builds is to be replaced by a per-sweep table of hostile-unit
 * positions, a data-structure change ai/default may not carry.
 * Everything else -- the city loop, its side effects and their order --
 * must stay exactly stock's: deep's decisions read these fields. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <stdlib.h>
#include <string.h>

/* utility */
#include "log.h"

/* common */
#include "actions.h"
#include "combat.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "unitlist.h"

/* common/aicore */
#include "path_finding.h"
#include "pf_tools.h"

/* server */
#include "srv_log.h"
#include "srv_main.h"

/* server/advisors */
#include "advdata.h"
#include "advgoto.h"
#include "advtools.h"
#include "infracache.h"

/* ai */
#include "handicaps.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daimilitary.h"
#include "daiplayer.h"
#include "daitech.h"
#include "daitools.h"

#include "deepdanger.h"

/* Where deep_assess_danger_unit() reads a hostile unit's position from:
 * a row of the per-sweep table when one is valid for this (city,
 * hostile player), stock's per-city pf_reverse_map otherwise. With
 * FREECIV_DEEP_DANGER_VERIFY set, both are kept and every table answer
 * is checked against the map (a test aid; outputs still come from the
 * table). */
struct deep_danger_src {
  const struct deep_danger_table *tab;
  const struct deep_danger_row *row;
  int slot;
  int idx;
  struct pf_reverse_map *map;
  const struct city *pcity;
};

static unsigned int deep_assess_danger_tab(struct ai_type *ait,
                                           const struct civ_map *nmap,
                                           struct city *pcity,
                                           player_unit_list_getter ul_cb,
                                           const struct deep_danger_table *tab);

static int deep_danger_verify_checks = 0;
static int deep_danger_verify_mismatches = 0;
/* (city, hostile player) pairs a sweep's table served, and those it
 * had to hand back to a per-city map; reported in verify mode. */
static int deep_danger_rows_used = 0;
static int deep_danger_rows_fallback = 0;

/**********************************************************************//**
  Is the table cross-check on?
**************************************************************************/
static bool deep_danger_verify(void)
{
  static int verify = -1;

  if (verify < 0) {
    const char *env = getenv("FREECIV_DEEP_DANGER_VERIFY");

    verify = (env != NULL && env[0] != '\0' && env[0] != '0') ? 1 : 0;
  }

  return verify > 0;
}

/**********************************************************************//**
  Log and reset the cross-check counters after a sweep.
**************************************************************************/
static void deep_danger_verify_report(const struct player *pplayer)
{
  if (!deep_danger_verify()) {
    return;
  }
  log_normal("deep-danger-verify turn=%d defender=\"%s\" checks=%d"
             " mismatches=%d rows=%d fallback=%d", game.info.turn,
             player_name(pplayer), deep_danger_verify_checks,
             deep_danger_verify_mismatches, deep_danger_rows_used,
             deep_danger_rows_fallback);
  deep_danger_verify_checks = 0;
  deep_danger_verify_mismatches = 0;
  deep_danger_rows_used = 0;
  deep_danger_rows_fallback = 0;
}

/**********************************************************************//**
  pf_reverse_map_unit_position(), from the table when the source has a
  row. Only pos->turn is filled on the table path; it is the only field
  deep_assess_danger_unit() reads.
**************************************************************************/
static bool deep_danger_position(struct deep_danger_src *src,
                                 const struct unit *punit, bool ferry,
                                 struct pf_position *pos)
{
  bool found;

  if (src->row == NULL) {
    return pf_reverse_map_unit_position(src->map, punit, pos);
  }

  found = deep_danger_row_turn(src->tab, src->row, src->idx, ferry,
                               src->slot, &pos->turn);
  if (src->map != NULL) {
    struct pf_position spos;
    bool sfound = pf_reverse_map_unit_position(src->map, punit, &spos);

    deep_danger_verify_checks++;
    if (sfound != found || (found && spos.turn != pos->turn)) {
      deep_danger_verify_mismatches++;
      log_error("deep-danger-verify mismatch turn=%d defender=\"%s\""
                " city=%d unit=%d type=\"%s\" ferry=%d table=%d/%d"
                " stock=%d/%d", game.info.turn,
                player_name(city_owner(src->pcity)), src->pcity->id,
                punit->id, utype_rule_name(unit_type_get(punit)),
                ferry ? 1 : 0, found ? 1 : 0, found ? pos->turn : -1,
                sfound ? 1 : 0, sfound ? spos.turn : -1);
    }
  }

  return found;
}

/**********************************************************************//**
  How dangerous and far a unit is for a city?
**************************************************************************/
static unsigned int deep_assess_danger_unit(const struct civ_map *nmap,
                                            const struct city *pcity,
                                            struct deep_danger_src *src,
                                            const struct unit *punit,
                                            int *move_time)
{
  struct pf_position pos;
  const struct unit_type *punittype = unit_type_get(punit);
  const struct tile *ptile = city_tile(pcity);
  const struct player *uowner = unit_owner(punit);
  const struct unit *ferry;
  unsigned int danger;
  int amod = -99, dmod;
  bool attack_danger = FALSE;

  *move_time = PF_IMPOSSIBLE_MC;

  if ((utype_can_do_action_result(punittype, ACTRES_PARADROP)
       || utype_can_do_action_result(punittype, ACTRES_PARADROP_CONQUER))
      && 0 < punittype->paratroopers_range) {
    *move_time = (real_map_distance(ptile, unit_tile(punit))
                  / punittype->paratroopers_range);
  }

  if (deep_danger_position(src, punit, FALSE, &pos)
      && (PF_IMPOSSIBLE_MC == *move_time
          || *move_time > pos.turn)) {
    *move_time = pos.turn;
  }

  if (unit_transported(punit)
      && (ferry = unit_transport_get(punit))
      && deep_danger_position(src, ferry, TRUE, &pos)) {
    if ((PF_IMPOSSIBLE_MC == *move_time
         || *move_time > pos.turn)) {
      *move_time = pos.turn;
      if (!can_attack_from_non_native(punittype)) {
        (*move_time)++;
      }
    }
  }

  if (PF_IMPOSSIBLE_MC == *move_time) {
    return 0;
  }
  if (!is_native_tile(punittype, ptile)
      && !can_attack_non_native(punittype)) {
    return 0;
  }
  if (!is_native_near_tile(nmap, unit_class_get(punit), ptile)) {
    return 0;
  }

  /* Find the worst attack action to expect */
  action_by_result_iterate(paction, ACTRES_ATTACK) {
    /* Is it possible that punit will do action id to the city? */
    /* FIXME: some unit parameters (notably, veterancy) may meddle in */

    if (action_may_happen_unit_on_city(action_id(paction), punit, pcity, *move_time)) {
      int b;

      attack_danger = TRUE;

      b = get_unittype_bonus(uowner, ptile, punittype, paction, EFT_ATTACK_BONUS);
      if (b > amod) {
        amod = b;
      }
    }
  } action_by_result_iterate_end;

  /* FIXME: it's a dummy support for anti-bombard defense just to do something against
   * approaching bombarders. Some better logic is needed, see OSDN#41778 */
  if (!attack_danger) {
    action_by_result_iterate(paction, ACTRES_BOMBARD) {
      /* FIXME: some unit parameters (notably, veterancy) may meddle in */

      if (action_may_happen_unit_on_city(action_id(paction), punit, pcity,
                                         *move_time)) {
        int b;

        attack_danger = TRUE;

        b = get_unittype_bonus(uowner, ptile, punittype, paction, EFT_ATTACK_BONUS);
        if (b > amod) {
          amod = b;
        }
      }
    } action_by_result_iterate_end;
    /* Here something should be done cuz the modifier affects
     * more than one unit but not their full hp, but is not done yet...*/
  }
  if (!attack_danger) {
    /* If the unit is dangerous, it's not about its combat strength */
    return 0;
  }

  danger = adv_unit_att_rating(punit);
  dmod = 100 + get_unittype_bonus(city_owner(pcity), ptile,
                                  punittype, NULL, EFT_DEFEND_BONUS);
  return danger * (amod + 100) / MAX(dmod, 1);
}

/**********************************************************************//**
  Call assess_danger() for all cities owned by pplayer.

  This is necessary to initialize some ai data before some ai calculations.
**************************************************************************/
void deep_assess_danger_player(struct ai_type *ait,
                               const struct civ_map *nmap,
                               struct player *pplayer)
{
  /* Do nothing if game is not running */
  if (S_S_RUNNING == server_state()) {
    struct deep_danger_table *tab;

    TIMING_LOG(AIT_DANGER, TIMER_START);
    tab = deep_danger_table_new(ait, nmap, pplayer, NULL);
    TIMING_LOG(AIT_DANGER, TIMER_STOP);
    city_list_iterate(pplayer->cities, pcity) {
      (void) deep_assess_danger_tab(ait, nmap, pcity, NULL, tab);
    } city_list_iterate_end;
    deep_danger_table_destroy(tab);
    deep_danger_verify_report(pplayer);
  }
}

/**********************************************************************//**
  Create cached information about danger, urgency and grave danger to our
  cities.

  Danger is a weight on how much power enemy units nearby have, which is
  compared to our defense.

  Urgency is the number of hostile units that can attack us in three turns.

  Grave danger is number of units that can attack us next turn.

  FIXME: We do not consider a paratrooper's mr_req and mr_sub
  fields. Not a big deal, though.

  FIXME: Due to the nature of assess_distance, a city will only be
  afraid of a boat laden with enemies if it stands on the coast (i.e.
  is directly reachable by this boat).
**************************************************************************/
static unsigned int deep_assess_danger_tab(struct ai_type *ait,
                                           const struct civ_map *nmap,
                                           struct city *pcity,
                                           player_unit_list_getter ul_cb,
                                           const struct deep_danger_table *tab)
{
  struct player *pplayer = city_owner(pcity);
  struct tile *ptile = city_tile(pcity);
  struct ai_city *city_data = def_ai_city_data(pcity, ait);
  /* How much such danger there is that building would help against. */
  unsigned int danger_reduced[B_LAST];
  int i;
  int defender;
  unsigned int urgency = 0;
  int defense;
  int total_danger = 0;

  /* TODO: Presumably most, or even all, of these arrays
   *       could be of size game.control.num_unit_types instead
   *       of full U_LAST */
  int defense_bonuses_pct[U_LAST];
  bool defender_type_handled[U_LAST];
  int best_non_scramble[U_LAST];
  bool sth_does_not_scramble = FALSE;
  int city_def_against[U_LAST];
  int assess_turns;
  bool omnimap;
  int slot;

  TIMING_LOG(AIT_DANGER, TIMER_START);

  /* Initialize data. */
  memset(&danger_reduced, 0, sizeof(danger_reduced));
  if (has_handicap(pplayer, H_DANGER)) {
    /* Always thinks that city is in grave danger */
    city_data->grave_danger = 1;
  } else {
    city_data->grave_danger = 0;
  }
  city_data->diplomat_threat = FALSE;
  city_data->has_diplomat = FALSE;

  unit_type_iterate(utype) {
    int idx = utype_index(utype);

    defense_bonuses_pct[idx] = 0;
    defender_type_handled[idx] = FALSE;
    best_non_scramble[idx] = -1;
    /* FIXME: cache it somewhere? */
    city_def_against[idx]
      = 100 + get_unittype_bonus(pplayer, ptile, utype, NULL,
                                 EFT_DEFEND_BONUS);
    city_def_against[idx] = MAX(city_def_against[idx], 1);
  } unit_type_iterate_end;

  /* What flag-specific bonuses do our units have. */
  /* We value them less than general defense increment */
  unit_list_iterate(ptile->units, punit) {
    const struct unit_type *def = unit_type_get(punit);

    if (unit_has_type_flag(punit, UTYF_DIPLOMAT)) {
      city_data->has_diplomat = TRUE;
    }
    if (!defender_type_handled[utype_index(def)]) {
      /* This is first defender of this type. Check defender type
       * specific bonuses. */

      /* Skip defenders that have no bonuses at all. Acceptable
       * side-effect is that we can't consider negative bonuses at
       * all ("No bonuses" should be better than "negative bonus") */
      if (def->cache.max_defense_mp_bonus_pct > 0) {
        unit_type_iterate(utype) {
          int idx = utype_index(utype);
          int coeff = def->cache.scramble_coeff[idx];
          int bonus;

          /* FIXME: consider EFT_FORTIFY_DEFENSE_BONUS */
          if (coeff) {
            bonus = coeff / city_def_against[idx] - 100;
          } else {
            bonus = def->cache.defense_mp_bonuses_pct[idx];
          }

          if (bonus > defense_bonuses_pct[idx]) {
            if (!coeff) {
              best_non_scramble[idx] = bonus;
            }
            defense_bonuses_pct[idx] = bonus;
          } else if (!coeff) {
            best_non_scramble[idx] = MAX(best_non_scramble[idx], bonus);
          }
        } unit_type_iterate_end;
      } else {
        /* Just remember that such a unit exists and hope its bonuses are just 0 */
        sth_does_not_scramble = TRUE;
      }

      defender_type_handled[utype_index(def)] = TRUE;
    }
  } unit_list_iterate_end;
  if (sth_does_not_scramble || unit_list_size(ptile->units) <= 0) {
    /* Scrambling units tend to be expensive. If we have a barenaked city, we'll
     * need at least a cheap unit. If we have only scramblers,
     * maybe it's OK. */
    unit_type_iterate(scrambler_type) {
      Unit_type_id id = utype_index(scrambler_type);

      best_non_scramble[id] = MAX(best_non_scramble[id], 0);
    } unit_type_iterate_end;
  }

  if (player_is_cpuhog(pplayer)) {
    assess_turns = 6;
  } else {
    assess_turns = has_handicap(pplayer, H_ASSESS_DANGER_LIMITED) ? 2 : 3;
  }

  omnimap = !has_handicap(pplayer, H_MAP);

  slot = (tab != NULL ? deep_danger_table_slot(tab, pcity) : -1);

  /* Check. */
  players_iterate(aplayer) {
    struct deep_danger_src src;
    struct unit_list *units;

    if (!adv_is_player_dangerous(pplayer, aplayer)) {
      continue;
    }
    /* Note that we still consider the units of players we are not (yet)
     * at war with. */

    if (ul_cb != NULL) {
      units = ul_cb(aplayer);
    } else {
      units = aplayer->units;
    }

    /* Stock builds a pf_reverse_map here; deep reads the sweep's table
     * when it still describes this city and these units exactly. */
    src.tab = tab;
    src.slot = slot;
    src.idx = -1;
    src.pcity = pcity;
    src.row = (slot >= 0
               ? deep_danger_table_row(ait, tab, pplayer, aplayer, units)
               : NULL);
    if (tab != NULL && deep_danger_verify()) {
      if (src.row != NULL) {
        deep_danger_rows_used++;
      } else {
        deep_danger_rows_fallback++;
      }
    }
    src.map = (src.row == NULL || deep_danger_verify()
               ? pf_reverse_map_new_for_city(nmap, pcity, aplayer,
                                             assess_turns, omnimap)
               : NULL);
    unit_list_iterate(units, punit) {
      int move_time;
      unsigned int vulnerability;
      int defbonus_pct;
      const struct unit_type *utype = unit_type_get(punit);
      struct unit_type_ai *utai = utype_ai_data(utype, ait);

#ifdef FREECIV_WEB
      /* freeciv-web ignores danger that is far in distance,
       * no matter how quickly it would reach us; even if
       * that's *immediately* over a road type that allows
       * unlimited movement. */
      int unit_distance = real_map_distance(ptile, unit_tile(punit));

      if (unit_distance > ASSESS_DANGER_MAX_DISTANCE
          || (has_handicap(pplayer, H_ASSESS_DANGER_LIMITED)
              && unit_distance > AI_HANDICAP_DISTANCE_LIMIT)) {
        /* Too far away. */
        continue;
      }
#endif /* FREECIV_WEB */

      if (!utai->carries_occupiers
          && !utype_acts_hostile(utype)) {
        /* Harmless unit. */
        continue;
      }
      src.idx++;

      /* Defender unspecific vulnerability and potential move time */
      vulnerability = deep_assess_danger_unit(nmap, pcity, &src,
                                              punit, &move_time);

      if (PF_IMPOSSIBLE_MC == move_time) {
        continue;
      }

      if (unit_can_take_over(punit) || utai->carries_occupiers) {
        /* Even if there is no attack strength,
         * we need ANY protector for the city */
        vulnerability = MAX(vulnerability, 1);
        if (3 >= move_time) {
          urgency++;
          if (1 >= move_time) {
            city_data->grave_danger++;
          }
        }
      }

      defbonus_pct = defense_bonuses_pct[utype_index(utype)];
      if (defbonus_pct > 100) {
        defbonus_pct = (defbonus_pct + 100) / 2;
      }
      /* Reduce vulnerability for specific bonuses we do have */
      vulnerability = vulnerability * 100 / (defbonus_pct + 100);
      /* Pass the order for a new defender type against it
       * to the scientific advisor, urgency considered */
      (void) dai_wants_defender_against(ait, nmap, pplayer, pcity, utype,
                                        vulnerability / MAX(move_time, 1));

      if (utype_acts_hostile(unit_type_get(punit)) && 2 >= move_time) {
        city_data->diplomat_threat = TRUE;
      }

      vulnerability *= vulnerability; /* positive feedback */
      if (1 < move_time) {
        vulnerability /= move_time;
      }

      if (unit_can_do_action(punit, ACTION_NUKE)
          || unit_can_do_action(punit, ACTION_NUKE_CITY)
          || unit_can_do_action(punit, ACTION_NUKE_UNITS)) {
        defender = dai_find_source_building(pcity, EFT_NUKE_PROOF, utype);
        if (defender != B_LAST) {
          danger_reduced[defender] += vulnerability / MAX(move_time, 1);
        }
      } else if (best_non_scramble[utype_index(utype)] >= 0) {
        /* To consider building a defensive building,
         * build first a defender that gets any profit of it */
        defender = dai_find_source_building(pcity, EFT_DEFEND_BONUS, utype);
        if (defender != B_LAST) {
          int danred = vulnerability / MAX(move_time, 1);
          /* Maybe we can build an improvement
           * that gets normal defender over scrambling one?
           * Effectively a sqrt-scale because we don't test how high
           * the effect is. */
          if (best_non_scramble[utype_index(utype)]
              < defense_bonuses_pct[utype_index(utype)]) {
            danred = danred * (100 + best_non_scramble[utype_index(utype)])
                     / (100 + defense_bonuses_pct[utype_index(utype)]);
          }
          danger_reduced[defender] += danred;
        }
      }

      total_danger += vulnerability;
    } unit_list_iterate_end;

    if (src.map != NULL) {
      pf_reverse_map_destroy(src.map);
    }

  } players_iterate_end;

  if (total_danger) {
    /* If any hostile player has any dangerous unit that can in any time
     * reach the city, we consider building walls here, if none yet.
     * FIXME: this value assumes that walls give x3 defense. */
    city_data->wallvalue = 90;
  } else {
    /* No danger.
     * This is half of the wallvalue of what danger 1 would produce. */
    city_data->wallvalue = 5;
  }

  if (0 < city_data->grave_danger) {
    /* really, REALLY urgent to defend */
    urgency += 10 * city_data->grave_danger;
  }

  /* HACK: This needs changing if multiple improvements provide
   * this effect. */
  /* FIXME: Accept only buildings helping unit classes we actually use.
   *        Now we consider any land mover helper suitable. */
  /* Sum of squared defense ratings */
  defense = assess_defense_igwall(ait, pcity);

  for (i = 0; i < B_LAST; i++) {
    if (0 < danger_reduced[i]) {
      dai_reevaluate_building(pcity, &pcity->server.adv->building_want[i],
                              urgency, danger_reduced[i], defense);
    }
  }

  if (has_handicap(pplayer, H_DANGER) && 0 == total_danger) {
    /* Has to have some danger
     * Otherwise grave_danger will be ignored. */
    city_data->danger = 1;
  } else {
    city_data->danger = total_danger;
  }
  city_data->urgency = urgency;

  TIMING_LOG(AIT_DANGER, TIMER_STOP);

  return urgency;
}

/* An open per-city sweep (deep_danger_sweep_begin()): the defender,
 * and its table once the first city has asked for it. */
static struct {
  bool open;
  struct player *defender;
  bool built;
  struct deep_danger_table *tab;
} deep_danger_sweep;

/**********************************************************************//**
  Open a sweep in which deep_assess_danger() will be called for some of
  pplayer's cities with no unit list getter, nothing that pathfinding
  sees changing in between. The table is built lazily by the first
  such call, so a sweep that assesses no city costs nothing. Every row
  is still re-validated per city (deep_danger_table_row()).
**************************************************************************/
void deep_danger_sweep_begin(struct player *pplayer)
{
  fc_assert(!deep_danger_sweep.open);
  deep_danger_sweep.open = TRUE;
  deep_danger_sweep.defender = pplayer;
  deep_danger_sweep.built = FALSE;
  deep_danger_sweep.tab = NULL;
}

/**********************************************************************//**
  Close the sweep opened by deep_danger_sweep_begin().
**************************************************************************/
void deep_danger_sweep_end(void)
{
  if (deep_danger_sweep.built) {
    deep_danger_verify_report(deep_danger_sweep.defender);
  }
  deep_danger_table_destroy(deep_danger_sweep.tab);
  deep_danger_sweep.open = FALSE;
  deep_danger_sweep.defender = NULL;
  deep_danger_sweep.built = FALSE;
  deep_danger_sweep.tab = NULL;
}

/**********************************************************************//**
  assess_danger() for one city; reads the open sweep's table when the
  city belongs to its defender and no unit list getter is given.
**************************************************************************/
unsigned int deep_assess_danger(struct ai_type *ait,
                                const struct civ_map *nmap,
                                struct city *pcity,
                                player_unit_list_getter ul_cb)
{
  const struct deep_danger_table *tab = NULL;

  if (deep_danger_sweep.open && ul_cb == NULL
      && city_owner(pcity) == deep_danger_sweep.defender
      && S_S_RUNNING == server_state()) {
    if (!deep_danger_sweep.built) {
      TIMING_LOG(AIT_DANGER, TIMER_START);
      deep_danger_sweep.tab
        = deep_danger_table_new(ait, nmap, deep_danger_sweep.defender,
                                NULL);
      TIMING_LOG(AIT_DANGER, TIMER_STOP);
      deep_danger_sweep.built = TRUE;
    }
    tab = deep_danger_sweep.tab;
  }

  return deep_assess_danger_tab(ait, nmap, pcity, ul_cb, tab);
}

/* ===================== Per-sweep hostile position table ================

   Stock asks a fresh pf_reverse_map per (city, hostile player) where
   each hostile unit stands relative to that ONE city: cities x units
   bounded Dijkstras per sweep. The table below answers the same
   question for every city of the defender from one Dijkstra per
   distinct pathfinding key, and must give the bit-identical pos.turn.

   What the per-city map computes (path_finding.c pf_reverse_map_pos):
   a forward normal map from the unit's tile, parameter from
   pft_fill_reverse_parameter() (owner = the HOSTILE player,
   moves_left_initially = move_rate, fuel 1, no ZOC/TB/EC callbacks),
   whose get_action returns PF_ACTION_ATTACK at the target tile only,
   iterated until the target is popped or the popped cost reaches
   move_rate * (max_turns + 1).

   Here: the same parameter with get_action ATTACKing nowhere (data =
   NULL), iterated to the same bound, recording the cost d(N) of every
   popped tile that is one of the defender's city tiles or adjacent to
   one. The per-city answer for city tile A is then

   - A unknown to the hostile player (not omniscient): the per-city
     map never asks get_action there, so its graph IS this graph and
     the answer is d(A);
   - A the start tile: cost 0;
   - a non-civilian that may not invade A: unreachable (TB_IGNORE
     comes before get_action);
   - otherwise the ATTACK node is a sink entered from any expandable
     neighbour N at d(N) + MIN(a, moves_left(d(N))), with a = move_rate
     for all-MP/suicide attackers and SINGLE_MOVE otherwise, so the
     answer is the minimum of that over N.

   The minimum may use d(N) from THIS graph, where A is an ordinary
   (perhaps passable) node, although the per-city graph makes A a
   sink: if the shortest path to N runs through A, the neighbour N'
   it entered A from has the same distance in both graphs, is
   expandable, and d(N') <= d(N); the step cost is non-decreasing in
   d. So the minimum is unchanged.

   Every node other than A must have the same properties in both
   graphs. They do, except that pf_get_move_scope()'s PF_MS_CITY test
   reads the scope of whichever neighbour initialised the node first,
   which is expansion-order dependent -- and only matters when two
   city tiles are adjacent. The table refuses to build when any two
   cities on the map are adjacent, and the sweep then uses stock's
   per-city maps.

   Keys: the per-city map caches by pf_pos_hash_cmp() (start tile,
   move rate, same utype -- or same class, same significant flags and,
   when not omniscient, same unknown_move_cost), and the first unit
   queried with a key is the one whose utype that Dijkstra uses. The
   query order (each non-harmless unit, then its ferry) does not depend
   on the city, so the representative is the same for every city and
   the table reproduces it. */

#define DEEP_DANGER_UNREACHED (-1)
#define DEEP_DANGER_UNTRACKED (-2)

struct deep_danger_key {
  struct tile *start_tile;
  int move_rate;
  const struct unit_type *utype;
};

/* One stock query (a unit or its ferry), with what its pf parameter
 * was built from, so a stale table is detected rather than trusted. */
struct deep_danger_query {
  int unit_id;
  int tindex;
  int move_rate;
  const struct unit_type *utype;
  int key;
};

struct deep_danger_entry {
  struct deep_danger_query self;
  bool has_ferry;
  struct deep_danger_query ferry;
};

struct deep_danger_row {
  bool present;
  int nentries;
  struct deep_danger_entry *entries;
  int nkeys;
  /* turn[key * ncities + slot]; DEEP_DANGER_UNREACHED when stock's
   * pf_reverse_map_unit_position() would return FALSE. */
  int *turn;
};

struct deep_danger_table {
  const struct player *defender;
  int assess_turns;
  bool omnimap;
  int ncities;
  int *city_ids;
  struct tile **city_tiles;
  struct deep_danger_row rows[MAX_NUM_PLAYER_SLOTS];
};

/**********************************************************************//**
  pf_pos_hash_cmp() (path_finding.c), on the fields the reverse map
  varies.
**************************************************************************/
static bool deep_danger_key_eq(const struct deep_danger_key *a,
                               const struct deep_danger_key *b,
                               bool omniscient)
{
  static const enum unit_type_flag_id flags[] = {
    UTYF_IGTER, UTYF_CIVILIAN, UTYF_COAST_STRICT
  };
  size_t i;

  if (a->start_tile != b->start_tile || a->move_rate != b->move_rate) {
    return FALSE;
  }
  if (a->utype == b->utype) {
    return TRUE;
  }
  if (utype_class(a->utype) != utype_class(b->utype)) {
    return FALSE;
  }
  if (!omniscient
      && a->utype->unknown_move_cost != b->utype->unknown_move_cost) {
    return FALSE;
  }
  for (i = 0; i < ARRAY_SIZE(flags); i++) {
    if (utype_has_flag(a->utype, flags[i])
        != utype_has_flag(b->utype, flags[i])) {
      return FALSE;
    }
  }

  return TRUE;
}

/**********************************************************************//**
  Fill a query from a unit, as pf_reverse_map_unit_pos() fills its
  parameter.
**************************************************************************/
static void deep_danger_query_fill(struct deep_danger_query *q,
                                   const struct unit *punit)
{
  q->unit_id = punit->id;
  q->tindex = tile_index(unit_tile(punit));
  q->move_rate = unit_move_rate(punit);
  q->utype = unit_type_get(punit);
  q->key = -1;
}

/**********************************************************************//**
  Does the query still describe punit?
**************************************************************************/
static bool deep_danger_query_matches(const struct deep_danger_query *q,
                                      const struct unit *punit)
{
  return (q->unit_id == punit->id
          && q->tindex == tile_index(unit_tile(punit))
          && q->move_rate == unit_move_rate(punit)
          && q->utype == unit_type_get(punit));
}

/**********************************************************************//**
  Find or append the key for a query; keys are numbered in first-query
  order, which is stock's representative order.
**************************************************************************/
static int deep_danger_key_of(struct deep_danger_key **keys, int *nkeys,
                              int *cap, const struct deep_danger_query *q,
                              bool omniscient)
{
  struct deep_danger_key k = {
    .start_tile = index_to_tile(&(wld.map), q->tindex),
    .move_rate = q->move_rate,
    .utype = q->utype
  };
  int i;

  for (i = 0; i < *nkeys; i++) {
    if (deep_danger_key_eq(&(*keys)[i], &k, omniscient)) {
      return i;
    }
  }
  if (*nkeys == *cap) {
    *cap = MAX(16, *cap * 2);
    *keys = fc_realloc(*keys, *cap * sizeof(**keys));
  }
  (*keys)[*nkeys] = k;

  return (*nkeys)++;
}

/**********************************************************************//**
  Is node N, popped at cost d, one the per-city map would expand from?
  (pf_normal_node_init() + the head of pf_normal_map_iterate(); no
  get_TB/get_zoc callbacks, get_action NONE away from the target.)
  N is never a city tile here (no two cities adjacent), so the scope
  does not depend on the neighbour that initialised it.
**************************************************************************/
static bool deep_danger_expands(const struct pf_parameter *param,
                                struct tile *ptile, int d)
{
  enum pf_move_scope scope;
  bool can_disembark = FALSE;

  if (param->move_rate <= 0 && d >= 0) {
    return FALSE;
  }
  if (!param->omniscience
      && TILE_UNKNOWN == tile_get_known(ptile, param->owner)) {
    return TRUE; /* PF_MS_NATIVE, TB_NORMAL */
  }
  scope = param->get_move_scope(ptile, &can_disembark, PF_MS_NONE, param);
  if (PF_MS_NONE == scope) {
    return FALSE;
  }
  if (PF_MS_TRANSPORT == scope && !can_disembark) {
    return FALSE; /* TB_DONT_LEAVE */
  }

  return TRUE;
}

/**********************************************************************//**
  pf_turns() for the reverse parameter (fuel 1).
**************************************************************************/
static int deep_danger_turns(const struct pf_parameter *param, int cost)
{
  if (cost <= 0) {
    return 0;
  } else if (param->move_rate <= 0) {
    return FC_INFINITY;
  } else {
    return cost / param->move_rate;
  }
}

/**********************************************************************//**
  Run one key's Dijkstra and fill its turn[] row for every city slot.
  dist[] is map-sized: DEEP_DANGER_UNTRACKED everywhere except the
  tracked tiles (city tiles and their neighbours), which this resets.
**************************************************************************/
static void deep_danger_run_key(const struct civ_map *nmap,
                                const struct deep_danger_table *tab,
                                const struct player *aplayer,
                                const struct deep_danger_key *k,
                                int *dist, const int *tracked, int ntracked,
                                int *turn_out)
{
  struct pf_parameter param;
  struct pf_map *pfm;
  int max_cost, attack_cost, slot, i;

  for (i = 0; i < ntracked; i++) {
    dist[tracked[i]] = DEEP_DANGER_UNREACHED;
  }

  pft_fill_reverse_parameter(nmap, &param, NULL);
  param.owner = aplayer;
  param.omniscience = tab->omnimap;
  param.map = nmap;
  param.start_tile = k->start_tile;
  param.move_rate = k->move_rate;
  param.moves_left_initially = param.move_rate;
  param.utype = k->utype;

  max_cost = param.move_rate * (tab->assess_turns + 1);
  pfm = pf_map_new(&param);
  do {
    struct tile *ptile = pf_map_iter(pfm);
    int cost = pf_map_iter_move_cost(pfm);
    int tindex = tile_index(ptile);

    if (cost >= max_cost) {
      break;
    }
    if (DEEP_DANGER_UNTRACKED != dist[tindex]) {
      dist[tindex] = cost;
    }
  } while (pf_map_iterate(pfm));
  pf_map_destroy(pfm);

  if (utype_action_takes_all_mp(param.utype,
                                action_by_number(ACTION_ATTACK))
      || utype_can_do_action(param.utype, ACTION_SUICIDE_ATTACK)) {
    attack_cost = param.move_rate;
  } else {
    attack_cost = SINGLE_MOVE;
  }

  for (slot = 0; slot < tab->ncities; slot++) {
    struct tile *ctile = tab->city_tiles[slot];
    int best = -1;

    if (ctile == param.start_tile) {
      best = 0;
    } else if (!param.omniscience
               && TILE_UNKNOWN == tile_get_known(ctile, aplayer)) {
      best = dist[tile_index(ctile)];
    } else if (!utype_has_flag(param.utype, UTYF_CIVILIAN)
               && !player_can_invade_tile(aplayer, ctile)) {
      best = -1;
    } else {
      adjc_iterate(nmap, ctile, ntile) {
        int d = dist[tile_index(ntile)];
        int ml, c;

        if (DEEP_DANGER_UNREACHED == d) {
          continue;
        }
        if (!deep_danger_expands(&param, ntile, d)) {
          continue;
        }
        /* pf_moves_left(), fuel 1 */
        if (d <= 0) {
          ml = param.move_rate - d;
        } else if (param.move_rate <= 0) {
          ml = 0;
        } else {
          ml = param.move_rate - (d % param.move_rate);
        }
        c = d + MIN(attack_cost, ml);
        if (best < 0 || c < best) {
          best = c;
        }
      } adjc_iterate_end;
    }

    turn_out[slot] = (best >= 0 && best < max_cost
                      ? deep_danger_turns(&param, best)
                      : DEEP_DANGER_UNREACHED);
  }
}

/**********************************************************************//**
  Are any two cities on the map adjacent? Then pf node scopes depend on
  expansion order and the table is not provably exact.
**************************************************************************/
static bool deep_danger_cities_adjacent(const struct civ_map *nmap)
{
  players_iterate(pplayer) {
    city_list_iterate(pplayer->cities, pcity) {
      adjc_iterate(nmap, city_tile(pcity), ntile) {
        if (tile_city(ntile) != NULL) {
          return TRUE;
        }
      } adjc_iterate_end;
    } city_list_iterate_end;
  } players_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  The defending player's assess_turns, as deep_assess_danger() computes
  it.
**************************************************************************/
static int deep_danger_assess_turns(const struct player *pplayer)
{
  if (player_is_cpuhog(pplayer)) {
    return 6;
  }
  return has_handicap(pplayer, H_ASSESS_DANGER_LIMITED) ? 2 : 3;
}

/**********************************************************************//**
  Build the table for a sweep over pplayer's cities with the given unit
  list getter. Returns NULL when the table cannot be exact (adjacent
  cities) or there is nothing to sweep; callers then use stock's maps.
**************************************************************************/
struct deep_danger_table *deep_danger_table_new(struct ai_type *ait,
                                                const struct civ_map *nmap,
                                                struct player *pplayer,
                                                player_unit_list_getter ul_cb)
{
  struct deep_danger_table *tab;
  int *dist, *tracked;
  int ntracked = 0, slot;

  if (city_list_size(pplayer->cities) <= 0
      || deep_danger_cities_adjacent(nmap)) {
    return NULL;
  }

  tab = fc_calloc(1, sizeof(*tab));
  tab->defender = pplayer;
  tab->assess_turns = deep_danger_assess_turns(pplayer);
  tab->omnimap = !has_handicap(pplayer, H_MAP);
  tab->ncities = city_list_size(pplayer->cities);
  tab->city_ids = fc_malloc(tab->ncities * sizeof(*tab->city_ids));
  tab->city_tiles = fc_malloc(tab->ncities * sizeof(*tab->city_tiles));

  dist = fc_malloc(MAP_INDEX_SIZE * sizeof(*dist));
  for (slot = 0; slot < MAP_INDEX_SIZE; slot++) {
    dist[slot] = DEEP_DANGER_UNTRACKED;
  }
  tracked = fc_malloc(tab->ncities * 9 * sizeof(*tracked));

  slot = 0;
  city_list_iterate(pplayer->cities, pcity) {
    struct tile *ctile = city_tile(pcity);

    tab->city_ids[slot] = pcity->id;
    tab->city_tiles[slot] = ctile;
    slot++;
    if (DEEP_DANGER_UNTRACKED == dist[tile_index(ctile)]) {
      dist[tile_index(ctile)] = DEEP_DANGER_UNREACHED;
      tracked[ntracked++] = tile_index(ctile);
    }
    adjc_iterate(nmap, ctile, ntile) {
      if (DEEP_DANGER_UNTRACKED == dist[tile_index(ntile)]) {
        dist[tile_index(ntile)] = DEEP_DANGER_UNREACHED;
        tracked[ntracked++] = tile_index(ntile);
      }
    } adjc_iterate_end;
  } city_list_iterate_end;

  players_iterate(aplayer) {
    struct deep_danger_row *row = &tab->rows[player_index(aplayer)];
    struct unit_list *units;
    struct deep_danger_key *keys = NULL;
    int nkeys = 0, cap = 0, k;

    if (!adv_is_player_dangerous(pplayer, aplayer)) {
      continue;
    }
    units = (ul_cb != NULL ? ul_cb(aplayer) : aplayer->units);

    row->present = TRUE;
    row->entries = fc_malloc(MAX(1, unit_list_size(units))
                             * sizeof(*row->entries));
    unit_list_iterate(units, punit) {
      const struct unit_type *utype = unit_type_get(punit);
      struct unit_type_ai *utai = utype_ai_data(utype, ait);
      struct deep_danger_entry *e;
      const struct unit *ferry;

      if (!utai->carries_occupiers
          && !utype_acts_hostile(utype)) {
        continue; /* Harmless: stock never queries its map for it. */
      }
      e = &row->entries[row->nentries++];
      deep_danger_query_fill(&e->self, punit);
      e->self.key = deep_danger_key_of(&keys, &nkeys, &cap, &e->self,
                                       tab->omnimap);
      e->has_ferry = (unit_transported(punit)
                      && (ferry = unit_transport_get(punit)) != NULL);
      if (e->has_ferry) {
        deep_danger_query_fill(&e->ferry, ferry);
        e->ferry.key = deep_danger_key_of(&keys, &nkeys, &cap, &e->ferry,
                                          tab->omnimap);
      }
    } unit_list_iterate_end;

    row->nkeys = nkeys;
    row->turn = fc_malloc(MAX(1, nkeys * tab->ncities)
                          * sizeof(*row->turn));
    for (k = 0; k < nkeys; k++) {
      deep_danger_run_key(nmap, tab, aplayer, &keys[k], dist,
                          tracked, ntracked,
                          row->turn + k * tab->ncities);
    }
    free(keys);
  } players_iterate_end;

  free(dist);
  free(tracked);

  return tab;
}

/**********************************************************************//**
  Free a table.
**************************************************************************/
void deep_danger_table_destroy(struct deep_danger_table *tab)
{
  int i;

  if (tab == NULL) {
    return;
  }
  for (i = 0; i < MAX_NUM_PLAYER_SLOTS; i++) {
    free(tab->rows[i].entries);
    free(tab->rows[i].turn);
  }
  free(tab->city_ids);
  free(tab->city_tiles);
  free(tab);
}

/**********************************************************************//**
  The table's slot for pcity, or -1 when the city is not in it (or has
  moved, which cannot happen, but is checked rather than assumed).
**************************************************************************/
int deep_danger_table_slot(const struct deep_danger_table *tab,
                           const struct city *pcity)
{
  int slot;

  for (slot = 0; slot < tab->ncities; slot++) {
    if (tab->city_ids[slot] == pcity->id) {
      return (tab->city_tiles[slot] == city_tile(pcity) ? slot : -1);
    }
  }

  return -1;
}

/**********************************************************************//**
  The row for aplayer when it still describes `units` exactly (same
  non-harmless units in the same order, same tiles, move rates, types
  and ferries) and the sweep parameters still hold; NULL otherwise,
  and the caller falls back to stock's per-city map for that player.
**************************************************************************/
const struct deep_danger_row *
deep_danger_table_row(struct ai_type *ait,
                      const struct deep_danger_table *tab,
                      const struct player *pplayer,
                      const struct player *aplayer,
                      struct unit_list *units)
{
  const struct deep_danger_row *row = &tab->rows[player_index(aplayer)];
  int i = 0;

  if (tab->defender != pplayer
      || tab->assess_turns != deep_danger_assess_turns(pplayer)
      || tab->omnimap != !has_handicap(pplayer, H_MAP)
      || !row->present) {
    return NULL;
  }
  unit_list_iterate(units, punit) {
    const struct unit_type *utype = unit_type_get(punit);
    struct unit_type_ai *utai = utype_ai_data(utype, ait);
    const struct deep_danger_entry *e;
    const struct unit *ferry;
    bool has_ferry;

    if (!utai->carries_occupiers
        && !utype_acts_hostile(utype)) {
      continue;
    }
    if (i >= row->nentries) {
      return NULL;
    }
    e = &row->entries[i++];
    if (!deep_danger_query_matches(&e->self, punit)) {
      return NULL;
    }
    has_ferry = (unit_transported(punit)
                 && (ferry = unit_transport_get(punit)) != NULL);
    if (has_ferry != e->has_ferry
        || (has_ferry && !deep_danger_query_matches(&e->ferry, ferry))) {
      return NULL;
    }
  } unit_list_iterate_end;

  return (i == row->nentries ? row : NULL);
}

/**********************************************************************//**
  Stock's pf_reverse_map_unit_position(...).turn for the idx-th
  non-harmless unit of a row (or its ferry) at a city slot; FALSE when
  stock would find no position.
**************************************************************************/
bool deep_danger_row_turn(const struct deep_danger_table *tab,
                          const struct deep_danger_row *row,
                          int idx, bool ferry, int slot, int *turn)
{
  const struct deep_danger_entry *e = &row->entries[idx];
  int key = (ferry ? e->ferry.key : e->self.key);
  int t = row->turn[key * tab->ncities + slot];

  if (DEEP_DANGER_UNREACHED == t) {
    return FALSE;
  }
  *turn = t;

  return TRUE;
}
