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

/**********************************************************************//**
  How dangerous and far a unit is for a city?
**************************************************************************/
static unsigned int deep_assess_danger_unit(const struct civ_map *nmap,
                                            const struct city *pcity,
                                            struct pf_reverse_map *pcity_map,
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

  if (pf_reverse_map_unit_position(pcity_map, punit, &pos)
      && (PF_IMPOSSIBLE_MC == *move_time
          || *move_time > pos.turn)) {
    *move_time = pos.turn;
  }

  if (unit_transported(punit)
      && (ferry = unit_transport_get(punit))
      && pf_reverse_map_unit_position(pcity_map, ferry, &pos)) {
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
    city_list_iterate(pplayer->cities, pcity) {
      (void) deep_assess_danger(ait, nmap, pcity, NULL);
    } city_list_iterate_end;
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
unsigned int deep_assess_danger(struct ai_type *ait,
                                const struct civ_map *nmap,
                                struct city *pcity,
                                player_unit_list_getter ul_cb)
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

  /* Check. */
  players_iterate(aplayer) {
    struct pf_reverse_map *pcity_map;
    struct unit_list *units;

    if (!adv_is_player_dangerous(pplayer, aplayer)) {
      continue;
    }
    /* Note that we still consider the units of players we are not (yet)
     * at war with. */

    pcity_map = pf_reverse_map_new_for_city(nmap, pcity, aplayer, assess_turns,
                                            omnimap);

    if (ul_cb != NULL) {
      units = ul_cb(aplayer);
    } else {
      units = aplayer->units;
    }
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

      /* Defender unspecific vulnerability and potential move time */
      vulnerability = deep_assess_danger_unit(nmap, pcity, pcity_map,
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

    pf_reverse_map_destroy(pcity_map);

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
