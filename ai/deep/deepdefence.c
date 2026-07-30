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

#include <math.h>

/* utility */
#include "log.h"
#include "rand.h"

/* common */
#include "city.h"
#include "effects.h"
#include "game.h"
#include "improvement.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "specialist.h"
#include "traits.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* ai */
#include "aitraits.h"
#include "difficulty.h"
#include "handicaps.h"

/* server */
#include "cityhand.h"
#include "citytools.h"
#include "cityturn.h"
#include "sernet.h"
#include "srv_log.h"
#include "unithand.h"

/* server/advisors */
#include "advbuilding.h"
#include "advchoice.h"
#include "advdata.h"
#include "advgoto.h"
#include "infracache.h"

/* ai/default */
#include "daiair.h"
#include "daicity.h"
#include "daidata.h"
#include "daidiplomacy.h"
#include "daidiplomat.h"
#include "daidomestic.h"
#include "daiguard.h"
#include "daieffects.h"
#include "daihand.h"
#include "daihunter.h"
#include "daimilitary.h"
#include "daiparadrop.h"
#include "daiplayer.h"
#include "daisettler.h"
#include "daitech.h"
#include "daitools.h"
#include "daiunit.h"

#include "deepai.h"
#include "deepexplore.h"
#include "deephorizon.h"
#include "deepsite.h"
#include "deepcm.h"
#include "deeptuning.h"
#include "deepwant.h"

#include "deepdefence.h"

#define LOGLEVEL_BUILD LOG_DEBUG
#define LOG_BUY LOG_DEBUG

/* Defence bookkeeping (W11). Stock cities only count defenders that are
 * physically on the city tile (assess_defense_backend), so a city whose
 * help is two turns out keeps requisitioning at emergency priority; and
 * simultaneous city emergencies are resolved in city list order with no
 * arbitration. The deep turn drivers below are derived from
 * dai_do_first_activities() / dai_do_last_activities() /
 * dai_manage_cities() so those two behaviours can change; everything
 * else is the stock call sequence, reused. */

/**********************************************************************//**
  Log one bookkeeping line per city: the raw assessed danger, the
  on-tile defence, and what the city is building
  (benchmark/defence.py). Runs before any deep adjustment so before and
  after arms measure need on the same scale.
**************************************************************************/
static double deep_garrison_menace(struct player *pplayer);

static void deep_log_defence(struct ai_type *ait, struct player *pplayer)
{
  double menace = deep_garrison_menace(pplayer);

  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    int ndef = 0;

    unit_list_iterate(city_tile(pcity)->units, punit) {
      if (unit_owner(punit) == pplayer
          && !unit_has_type_flag(punit, UTYF_CIVILIAN)) {
        ndef++;
      }
    } unit_list_iterate_end;

    log_base(deep_metric_log_level(),
             "deep-def turn=%d player=\"%s\" city=%d danger=%u"
             " urgency=%u defense=%d ndef=%d prod=\"%s\""
             " shield=%d food=%d menace=%.2f",
             game.info.turn, player_name(pplayer), pcity->id,
             city_data->danger, city_data->urgency,
             assess_defense(ait, pcity), ndef,
             universal_rule_name(&pcity->production),
             pcity->surplus[O_SHIELD], pcity->surplus[O_FOOD],
             menace);
  } city_list_iterate_end;
}

/**********************************************************************//**
  Credit defenders that are on their way home against their target
  city's freshly assessed danger, so the city stops requisitioning
  replacements for help that is already coming. This is the missing
  mirror of the invasion bookkeeping the AI keeps about enemy cities
  (reinforcements_cost_and_value() filling ai_invasion).

  A unit counts if its task is AIUNIT_DEFEND_HOME and its goto points
  at an own city it has not reached yet; its contribution is its
  defence power in that city discounted by expected arrival time and
  the chance of surviving the trip.
**************************************************************************/
static unsigned int deep_incoming_defence_credit(struct ai_type *ait,
                                                 struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  unsigned int credit = 0;

  unit_list_iterate(pplayer->units, punit) {
    struct unit_ai *unit_data = def_ai_unit_data(punit, ait);
    int eta, power;

    if (unit_data->task != AIUNIT_DEFEND_HOME
        || punit->goto_tile == NULL
        || tile_city(punit->goto_tile) != pcity
        || same_pos(punit->goto_tile, unit_tile(punit))) {
      /* Units already in place are counted by assess_defense(). */
      continue;
    }

    power = assess_defense_unit(ait, pcity, punit, FALSE);
    if (power <= 0) {
      continue;
    }
    eta = real_map_distance(unit_tile(punit), punit->goto_tile)
          * SINGLE_MOVE / MAX(unit_type_get(punit)->move_rate, 1);
    credit += power
              * pow(tune->incoming_defence_survival, eta)
              / (1.0 + tune->incoming_defence_eta_weight * eta);
  } unit_list_iterate_end;

  return credit;
}

/**********************************************************************//**
  Danger a coastal city faces from an invasion the stock assessment
  cannot see: at-war units EMBARKED ON BOATS that could reach the
  city's neighbourhood within about one transport move. assess_danger()
  weighs threats by land movement, so a loaded trireme one strait away
  contributes nothing -- which left deep cities empty in front of boat
  invasions (garden2/isolated mixed games: 2.3x stock's city losses).

  Only cargo counts (a warship shells, it does not take cities), and
  only when it is close enough to land soon: summing every hostile
  unit overseas saturated the danger field and pinned whole
  civilizations at maximum military production (garden2 retest
  2026-07-28: city losses inverted but score 0.861 -> 0.717). Fear
  decays with distance inside the window; both knobs are tuning
  placeholders.
**************************************************************************/
static unsigned int deep_overseas_danger(const struct civ_map *nmap,
                                         struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  struct tile *ptile = city_tile(pcity);
  Continent_id home = tile_continent(ptile);
  double danger = 0.0;

  if (tune->overseas_fear <= 0.0
      || !is_terrain_class_near_tile(nmap, ptile, TC_OCEAN)) {
    return 0;
  }

  players_iterate_alive(aplayer) {
    if (!pplayers_at_war(pplayer, aplayer)) {
      continue;
    }
    unit_list_iterate(aplayer->units, aunit) {
      const struct unit *ferry = unit_transport_get(aunit);
      int rating, dist, reach;

      if (ferry == NULL
          || tile_continent(unit_tile(aunit)) == home) {
        /* Not aboard a boat, or already ashore here: assess_danger()
         * covers those. */
        continue;
      }
      rating = adv_unit_att_rating(aunit);
      if (rating <= 0) {
        continue;
      }
      reach = unit_type_get(ferry)->move_rate / SINGLE_MOVE + 1;
      dist = real_map_distance(unit_tile(aunit), ptile);
      if (dist > reach) {
        continue;
      }
      danger += rating * tune->overseas_fear
                / (1.0 + tune->overseas_decay * dist);
    } unit_list_iterate_end;
  } players_iterate_alive_end;

  return (unsigned int) danger;
}

/**********************************************************************//**
  Discount on deep's injected fear terms when the marginal defender
  itself costs upkeep the ruleset charges in gold or food. civ2civ3
  does both (and grants only 1-3 upkeep-free units per city), so every
  requisitioned garrison is a recurring treasury and larder bill;
  classic charges shields and the discount tends to 1. Uses the
  cheapest buildable defending type's government-adjusted upkeep
  (utype_upkeep_cost), so a Democracy fears the doubled bill it
  actually pays.
**************************************************************************/
static double deep_defender_upkeep_discount(const struct civ_map *nmap,
                                            struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);
  int cheapest = -1;

  if (tune->upkeep_unit_weight <= 0.0) {
    return 1.0;
  }

  unit_type_iterate(ut) {
    int cost;

    if (ut->defense_strength <= 0
        || !can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)) {
      continue;
    }
    cost = utype_upkeep_cost(ut, NULL, pplayer, O_GOLD)
           + utype_upkeep_cost(ut, NULL, pplayer, O_FOOD);
    if (cheapest < 0 || cost < cheapest) {
      cheapest = cost;
    }
  } unit_type_iterate_end;

  if (cheapest <= 0) {
    return 1.0;
  }
  return 1.0 / (1.0 + tune->upkeep_unit_weight * cheapest);
}

/**********************************************************************//**
  Standing fear a coastal city carries while any at-war player OWNS
  transport-capable ships (the "boat shadow"): a sudden trireme gives
  the embarked-cargo term only ~one turn of warning, so exposed
  coasts need a modest garrison and shields BEFORE the boats load --
  and via the CM's danger gate this also tempers grow-hard exactly
  where the garden2b pile regression showed grow-mad shieldless
  coasts dying (ablation 2026-07-30). Zero before any enemy can
  float: fourth application of the no-fear-before-the-threat-class
  law.
**************************************************************************/
static unsigned int deep_boat_shadow_fear(const struct civ_map *nmap,
                                          struct city *pcity)
{
  const struct deep_tuning *tune = deep_tuning_get();
  struct player *pplayer = city_owner(pcity);

  if (tune->boat_shadow_fear <= 0
      || !is_terrain_class_near_tile(nmap, city_tile(pcity), TC_OCEAN)) {
    return 0;
  }

  players_iterate_alive(aplayer) {
    if (aplayer == pplayer || !pplayers_at_war(pplayer, aplayer)) {
      continue;
    }
    unit_list_iterate(aplayer->units, aunit) {
      if (unit_type_get(aunit)->transport_capacity > 0
          && utype_move_type(unit_type_get(aunit)) == UMT_SEA) {
        return tune->boat_shadow_fear;
      }
    } unit_list_iterate_end;
  } players_iterate_alive_end;

  return 0;
}

/**********************************************************************//**
  Flat fear of barbarian uprisings, active from the first turn the
  server can actually spawn them -- the same gate barbarian.c uses --
  and zero before it, so pre-onset production is not taxed. The
  garrison floor insures otherwise-empty cities; this term keeps
  established cities from standing down the moment assessed danger
  returns to zero inside the barbarian era.
**************************************************************************/
static unsigned int deep_barbarian_fear(void)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (BARBS_DISABLED == game.server.barbarianrate
      || game.info.turn < game.server.onsetbarbarian) {
    return 0;
  }
  return tune->barbarian_fear;
}

/**********************************************************************//**
  How militarised the players we have actually met are, relative to
  us, from PUBLIC information only: the units score that the
  historian's periodic "mightiest civilizations" report broadcasts to
  everyone (server/report.c, score.units) -- the travellers' report
  that says keep the gates manned. Ratio of their summed units score
  to ours, through the tunable slope, capped at 1.0. Unmet players
  contribute nothing, so a pre-contact empire keeps no insurance
  garrison at all, and the signal rises BEFORE any war does.

  Each neighbour is weighted by REACHABILITY (the third instance of
  the "no fear before the threat class can exist" law, after the
  barbarian onset gate and the embarked-only overseas term): an army
  that shares a continent with any of our cities counts in full; an
  army wholly overseas counts at menace_overseas_weight, because it
  must first build and sail boats. Both island-native tunes (H, H2)
  independently disabled the ungated floor -- on island maps it was
  pure tax (STATUS 2026-07-30).

  Deliberately count-based and coarse: a chronic background signal,
  not an acute one. Barbarian risk is deliberately not in here;
  deep_barbarian_fear() owns it.
**************************************************************************/
static double deep_garrison_menace(struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  double theirs = 0.0;
  Continent_id ours[64];
  int n_ours = 0;

  if (tune->garrison_menace_slope <= 0.0) {
    return 1.0;   /* Disabled: unconditional floor. */
  }

  city_list_iterate(pplayer->cities, pcity) {
    Continent_id c = tile_continent(city_tile(pcity));
    int i;

    for (i = 0; i < n_ours; i++) {
      if (ours[i] == c) {
        break;
      }
    }
    if (i == n_ours && n_ours < 64) {
      ours[n_ours++] = c;
    }
  } city_list_iterate_end;

  players_iterate_alive(aplayer) {
    bool reachable = FALSE;

    if (aplayer == pplayer
        || pplayers_allied(pplayer, aplayer)
        || player_diplstate_get(pplayer, aplayer)->type
           == DS_NO_CONTACT) {
      continue;
    }
    city_list_iterate(aplayer->cities, acity) {
      Continent_id c = tile_continent(city_tile(acity));

      for (int i = 0; i < n_ours; i++) {
        if (ours[i] == c) {
          reachable = TRUE;
          break;
        }
      }
      if (reachable) {
        break;
      }
    } city_list_iterate_end;
    theirs += aplayer->score.units
              * (reachable ? 1.0 : tune->menace_overseas_weight);
  } players_iterate_alive_end;

  if (theirs <= 0.0) {
    return 0.0;
  }
  return MIN(1.0, tune->garrison_menace_slope * theirs
                  / MAX(pplayer->score.units, 1));
}

/**********************************************************************//**
  Keep a token garrison even where assessed danger is zero. Stock
  cities do this by habit and it silently insures them against every
  threat the assessment cannot see (landings, barbarian spawns); deep
  trusted the assessment literally, left new cities at ndef=0 building
  Settlers, and lost them. The floor makes the insurance explicit and
  tunable instead of removing it, and it is scaled by how militarised
  the neighbours we know about actually are -- zero before contact,
  when the world cannot reach us and every garrison displaces a
  Settler at compounding time.
**************************************************************************/
static void deep_apply_garrison_floor(struct city *pcity,
                                      struct ai_city *city_data,
                                      double upkeep_discount,
                                      double menace)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (city_data->danger == 0
      && unit_list_size(city_tile(pcity)->units) < tune->garrison_floor) {
    city_data->danger
      = (unsigned int) (tune->garrison_floor_danger * upkeep_discount
                        * menace);
  }
}

/**********************************************************************//**
  Apply the overseas fear, the en-route credit and the garrison floor
  to every city's freshly assessed danger.
**************************************************************************/
static void deep_credit_incoming_defence(struct ai_type *ait,
                                         struct player *pplayer)
{
  double menace = deep_garrison_menace(pplayer);

  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    double updisc = deep_defender_upkeep_discount(&(wld.map), pcity);
    unsigned int credit;

    city_data->danger += (unsigned int)
      (deep_overseas_danger(&(wld.map), pcity) * updisc);
    city_data->danger += (unsigned int)
      (deep_boat_shadow_fear(&(wld.map), pcity) * updisc);
    city_data->danger += (unsigned int) (deep_barbarian_fear() * updisc);
    credit = deep_incoming_defence_credit(ait, pcity);
    city_data->danger -= MIN(city_data->danger, credit);
    deep_apply_garrison_floor(pcity, city_data, updisc, menace);
  } city_list_iterate_end;
}

/**********************************************************************//**
  Melt obsolete surplus garrison (rotation move 3, design in
  findings/civ2civ3-adjustments.md). In a city with zero post-
  adjustment danger and no urgency, holding more own military units
  than the garrison floor, disband the WEAKEST defender -- but only
  when it costs ruleset upkeep and a defender at least
  rotation_power_ratio times stronger is buildable here, so quiet
  turns cannot melt a modern garrison (the obsolescence gate is the
  hysteresis). One action per city per turn; the traditional disband
  recovers shields into production where the ruleset allows (civ2civ3
  does). A Phalanx stack paying gold+food into the Musketeer era was
  the motivating waste.
**************************************************************************/
static void deep_melt_surplus_defenders(struct ai_type *ait,
                                        struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();
  const struct civ_map *nmap = &(wld.map);

  if (tune->rotation_power_ratio <= 0.0) {
    return;   /* Disabled. */
  }

  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    struct unit *worst = NULL;
    const struct unit_type *wt;
    int worst_def = 0, count = 0, best_buildable = 0;

    if (city_data->urgency > 0 || city_data->danger > 0) {
      continue;
    }
    unit_list_iterate(city_tile(pcity)->units, punit) {
      const struct unit_type *ut = unit_type_get(punit);

      if (unit_owner(punit) != pplayer
          || unit_has_type_flag(punit, UTYF_CIVILIAN)
          || get_transporter_occupancy(punit) > 0
          || ut->defense_strength <= 0
          || ut->attack_strength > ut->defense_strength) {
        /* Attack-heavy units are sortie assets, not garrison fodder:
         * with killstack a garrisoned Cannon erases the besieging
         * stack (human point 2026-07-30). Never melt them here. */
        continue;
      }
      count++;
      if (worst == NULL || ut->defense_strength < worst_def) {
        worst = punit;
        worst_def = ut->defense_strength;
      }
    } unit_list_iterate_end;

    if (worst == NULL || count <= tune->garrison_floor) {
      continue;
    }
    wt = unit_type_get(worst);
    if (utype_upkeep_cost(wt, NULL, pplayer, O_GOLD)
        + utype_upkeep_cost(wt, NULL, pplayer, O_FOOD)
        + utype_upkeep_cost(wt, NULL, pplayer, O_SHIELD) <= 0) {
      continue;   /* Free to keep; melting gains nothing. */
    }
    unit_type_iterate(ut) {
      if (ut->defense_strength > best_buildable
          && can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)) {
        best_buildable = ut->defense_strength;
      }
    } unit_type_iterate_end;
    {
      /* Recovered shields that would FINISH the current build make a
       * melt more attractive: relax the obsolescence bar (human
       * refinement 2026-07-30). Recover approximated at half the
       * unit's shield cost, the common ruleset rate. */
      double need = tune->rotation_power_ratio;

      if (pcity->shield_stock + unit_build_shield_cost_base(worst) / 2
          >= city_production_build_shield_cost(pcity)) {
        need *= tune->rotation_finish_relax;
      }
      if (best_buildable < worst_def * need) {
        continue;
      }
    }
    {
      /* Upgrade before disbanding: if the treasury can afford to
       * modernise this unit outright (same reserve-plus-shortfall
       * guard as the spend gate), that beats destroying it. */
      const struct unit_type *up = can_upgrade_unittype(pplayer, wt);

      if (up != NULL) {
        int cost = unit_upgrade_price(pplayer, wt, up);
        int reserve = dai_gold_reserve(pplayer);
        int income = player_get_expected_income(pplayer);
        bool upgraded = FALSE;

        if (income < 0) {
          reserve -= income;
        }
        if (pplayer->economic.gold - cost > reserve) {
          action_id upgrade_actions[MAX_NUM_ACTIONS];
          int i = 0;

          action_array_add_all_by_result(upgrade_actions, &i,
                                         ACTRES_UPGRADE_UNIT);
          action_array_end(upgrade_actions, i);
          action_array_iterate(upgrade_actions, act_id) {
            if (action_ever_possible(act_id)) {
              log_base(deep_metric_log_level(),
                       "deep-rotate turn=%d player=\"%s\" city=%d"
                       " upgrade=\"%s\" to=\"%s\" cost=%d",
                       game.info.turn, player_name(pplayer),
                       pcity->id, unit_rule_name(worst),
                       utype_rule_name(up), cost);
              unit_do_action(pplayer, worst->id, pcity->id, 0, "",
                             act_id);
              upgraded = TRUE;
              break;
            }
          } action_array_iterate_end;
        }
        if (upgraded) {
          continue;   /* Modernised in place; nothing to melt. */
        }
      }
    }
    log_base(deep_metric_log_level(),
             "deep-rotate turn=%d player=\"%s\" city=%d disband=\"%s\""
             " def=%d best=%d count=%d",
             game.info.turn, player_name(pplayer), pcity->id,
             unit_rule_name(worst), worst_def, best_buildable, count);
    unit_do_disband_trad(pplayer, worst, ACT_REQ_PLAYER);
  } city_list_iterate_end;
}

/**********************************************************************//**
  Convoy discipline (naval design in findings/civ2civ3-adjustments.md;
  slices 1+2 shipped together -- a full transport without escort is a
  fatter target than the dribble it replaces).

  HOLD: a sea transport sitting in one of our cities with cargo
  aboard but fewer than convoy_min_load units holds position -- but
  only while at least one own unit within convoy_wait_radius is
  assigned to board it (unit_ai->ferryboat), so the gate cannot
  deadlock: the moment nobody is coming, it sails with what it has.

  ESCORT: any cargo-carrying transport without a bodyguard recruits
  an idle own military sea unit from its tile via the stock bodyguard
  framework, which then moves the pair together.
**************************************************************************/
static void deep_manage_convoys(struct ai_type *ait,
                                struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (tune->convoy_min_load <= 1) {
    return;   /* Disabled: stock single-passenger ferrying. */
  }

  unit_list_iterate(pplayer->units, boat) {
    const struct unit_type *bt = unit_type_get(boat);
    struct city *pcity;
    int cargo, want;

    if (bt->transport_capacity <= 0
        || utype_move_type(bt) != UMT_SEA) {
      continue;
    }
    pcity = tile_city(unit_tile(boat));
    if (pcity == NULL || city_owner(pcity) != pplayer) {
      continue;
    }
    cargo = get_transporter_occupancy(boat);
    if (cargo <= 0) {
      continue;
    }

    if (aiguard_guard_of(ait, boat) == NULL) {
      unit_list_iterate(unit_tile(boat)->units, w) {
        const struct unit_type *wt = unit_type_get(w);
        struct unit_ai *wd = def_ai_unit_data(w, ait);

        if (unit_owner(w) == pplayer && w != boat
            && utype_move_type(wt) == UMT_SEA
            && wt->transport_capacity == 0
            && wt->attack_strength > 0
            && wd->task == AIUNIT_NONE && wd->charge <= 0) {
          aiguard_assign_guard_unit(ait, boat, w);
          log_base(deep_metric_log_level(),
                   "deep-convoy turn=%d player=\"%s\" escort=\"%s\""
                   " boat=%d cargo=%d",
                   game.info.turn, player_name(pplayer),
                   unit_rule_name(w), boat->id, cargo);
          break;
        }
      } unit_list_iterate_end;
    }

    want = MIN(get_transporter_capacity(boat), tune->convoy_min_load);
    if (cargo >= want) {
      continue;
    }
    {
      bool incoming = FALSE;

      unit_list_iterate(pplayer->units, p) {
        if (def_ai_unit_data(p, ait)->ferryboat == boat->id
            && !same_pos(unit_tile(p), unit_tile(boat))
            && real_map_distance(unit_tile(p), unit_tile(boat))
               <= tune->convoy_wait_radius) {
          incoming = TRUE;
          break;
        }
      } unit_list_iterate_end;
      if (incoming) {
        def_ai_unit_data(boat, ait)->done = TRUE;
        log_base(deep_metric_log_level(),
                 "deep-convoy turn=%d player=\"%s\" hold boat=%d"
                 " cargo=%d want=%d",
                 game.info.turn, player_name(pplayer), boat->id,
                 cargo, want);
      }
    }
  } unit_list_iterate_end;
}

/**********************************************************************//**
  Interrupt working civilians in danger and run them to the nearest
  own city (human request 2026-07-30: a mid-transform Engineer keeps
  digging as the Knights ride up -- stock auto-workers fear PATHS but
  never abandon an activity). A civilian outside a city with an
  at-war military unit within worker_flee_radius drops its activity,
  legs it to the closest own city on its continent, and is marked
  done for the turn so nothing re-tasks it into the field.
**************************************************************************/
static void deep_flee_workers(struct ai_type *ait, struct player *pplayer)
{
  const struct deep_tuning *tune = deep_tuning_get();

  if (tune->worker_flee_radius <= 0) {
    return;   /* Disabled. */
  }

  unit_list_iterate_safe(pplayer->units, w) {
    bool danger = FALSE;
    struct city *refuge = NULL;
    int best = FC_INFINITY;

    if (!unit_has_type_flag(w, UTYF_CIVILIAN)
        || unit_transported(w)
        || tile_city(unit_tile(w)) != NULL) {
      continue;
    }
    {
      /* A bodyguard standing with us is the answer to danger, not a
       * reason to abandon work: guarded civilians hold their ground
       * (the flee would waste the escort the border-guard system
       * just delivered). */
      struct unit *g = aiguard_guard_of(ait, w);

      if (g != NULL
          && real_map_distance(unit_tile(g), unit_tile(w)) <= 1) {
        continue;
      }
    }
    players_iterate_alive(aplayer) {
      if (!pplayers_at_war(pplayer, aplayer)) {
        continue;
      }
      unit_list_iterate(aplayer->units, e) {
        if (adv_unit_att_rating(e) > 0
            && real_map_distance(unit_tile(e), unit_tile(w))
               <= tune->worker_flee_radius) {
          danger = TRUE;
          break;
        }
      } unit_list_iterate_end;
      if (danger) {
        break;
      }
    } players_iterate_alive_end;
    if (!danger) {
      /* No immediate threat: but a civilian WORKING an exposed tile
       * (unowned ground, or an at-war unit within the wider guard
       * radius) requests a bodyguard through the stock framework, so
       * escorts arrive BEFORE the flee radius is breached (human
       * request 2026-07-30: prioritise protection at the border). */
      if (tune->worker_guard_radius > 0
          && !aiguard_has_guard(ait, w)
          && w->activity != ACTIVITY_IDLE) {
        bool exposed = tile_owner(unit_tile(w)) != pplayer;

        if (!exposed) {
          players_iterate_alive(aplayer) {
            if (!pplayers_at_war(pplayer, aplayer)) {
              continue;
            }
            unit_list_iterate(aplayer->units, e) {
              if (adv_unit_att_rating(e) > 0
                  && real_map_distance(unit_tile(e), unit_tile(w))
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
        if (exposed) {
          aiguard_request_guard(ait, w);
          log_base(deep_metric_log_level(),
                   "deep-guard turn=%d player=\"%s\" unit=\"%s\"",
                   game.info.turn, player_name(pplayer),
                   unit_rule_name(w));
        }
      }
      continue;
    }
    city_list_iterate(pplayer->cities, pcity) {
      int d = real_map_distance(city_tile(pcity), unit_tile(w));

      if (d < best
          && tile_continent(city_tile(pcity))
             == tile_continent(unit_tile(w))) {
        best = d;
        refuge = pcity;
      }
    } city_list_iterate_end;
    if (refuge == NULL) {
      continue;
    }
    if (w->goto_tile == city_tile(refuge)) {
      /* Already running there: keep running, no re-path spam. */
      def_ai_unit_data(w, ait)->done = TRUE;
      continue;
    }
    log_base(deep_metric_log_level(),
             "deep-flee turn=%d player=\"%s\" unit=\"%s\" to=%d dist=%d",
             game.info.turn, player_name(pplayer), unit_rule_name(w),
             refuge->id, best);
    unit_activity_handling(w, ACTIVITY_IDLE, ACTION_NONE);
    if (dai_unit_goto(ait, w, city_tile(refuge))) {
      def_ai_unit_data(w, ait)->done = TRUE;
    }
  } unit_list_iterate_safe_end;
}

/**********************************************************************//**
  Activities before unit movement; derived from
  dai_do_first_activities() with defence bookkeeping added between the
  danger assessment and the unit management that consumes it.
**************************************************************************/
void deep_do_first_activities(struct ai_type *ait, struct player *pplayer)
{
  TIMING_LOG(AIT_ALL, TIMER_START);
  dai_assess_danger_player(ait, &(wld.map), pplayer);

  deep_log_defence(ait, pplayer);
  deep_credit_incoming_defence(ait, pplayer);
  deep_melt_surplus_defenders(ait, pplayer);
  deep_manage_convoys(ait, pplayer);
  deep_flee_workers(ait, pplayer);
  deep_set_city_manager_params(ait, pplayer);

  TIMING_LOG(AIT_UNITS, TIMER_START);
  dai_manage_units(ait, pplayer);
  TIMING_LOG(AIT_UNITS, TIMER_STOP);
  /* STOP.  Everything else is at end of turn. */

  TIMING_LOG(AIT_ALL, TIMER_STOP);

  flush_packets(); /* AIs can be such spammers... */
}

/**********************************************************************//**
  Military build choice for one city. Derived from
  military_advisor_choose_build() in daimilitary.c (every helper is
  called, not copied); the difference is that the en-route defender
  credit is applied between the danger assessment and the decisions
  that consume it -- the stock function re-runs assess_danger()
  internally, which is why adjusting danger at phase start cannot
  affect production.
**************************************************************************/
static struct adv_choice *deep_military_choose_build(struct ai_type *ait,
                                                     const struct civ_map *nmap,
                                                     struct player *pplayer,
                                                     struct city *pcity,
                                                     player_unit_list_getter ul_cb)
{
  struct adv_data *ai = adv_data_get(pplayer, NULL);
  struct unit_type *punittype;
  unsigned int our_def, urgency;
  struct tile *ptile = pcity->tile;
  struct unit *virtualunit;
  struct ai_city *city_data = def_ai_city_data(pcity, ait);
  adv_want martial_value = 0;
  bool martial_need = FALSE;
  struct adv_choice *choice = adv_new_choice();
  bool allow_gold_upkeep;

  urgency = assess_danger(ait, nmap, pcity, ul_cb);
  /* Changing to quadratic to stop AI from building piles
   * of small units -- Syela */
  /* It has to be AFTER assess_danger() thanks to wallvalue. */
  our_def = assess_defense_quadratic(ait, pcity);

  {
    /* Fear invasions the assessment cannot see, then count the help
     * that is already on its way, so the city does not requisition
     * replacements for it (W11), and never fall below the token
     * garrison. All injected fear is discounted by what the ruleset
     * charges to keep the marginal defender. */
    double updisc = deep_defender_upkeep_discount(nmap, pcity);
    unsigned int credit;

    city_data->danger += (unsigned int)
      (deep_overseas_danger(nmap, pcity) * updisc);
    city_data->danger += (unsigned int)
      (deep_boat_shadow_fear(nmap, pcity) * updisc);
    city_data->danger += (unsigned int) (deep_barbarian_fear() * updisc);
    credit = deep_incoming_defence_credit(ait, pcity);
    city_data->danger -= MIN(city_data->danger, credit);
    deep_apply_garrison_floor(pcity, city_data, updisc,
                              deep_garrison_menace(pplayer));

    if (city_data->danger == 0 && deep_tuning_get()->rotation_want > 0
        && deep_tuning_get()->rotation_power_ratio > 0.0) {
      /* Rotation move 2 (replace-then-disband): if the BEST defender
       * present is obsolete by the rotation ratio, inject a small
       * want so the city queues a modern one; the melt (move 3)
       * removes the old unit only once the new one raises the count
       * above the floor -- new arrives before old leaves, zero
       * garrison gap. */
      int best_present = 0, best_buildable = 0;

      unit_list_iterate(ptile->units, punit) {
        const struct unit_type *put = unit_type_get(punit);

        if (unit_owner(punit) == pplayer
            && !unit_has_type_flag(punit, UTYF_CIVILIAN)
            && put->defense_strength > best_present) {
          best_present = put->defense_strength;
        }
      } unit_list_iterate_end;
      unit_type_iterate(ut) {
        if (ut->defense_strength > best_buildable
            && can_city_build_unit_now(nmap, pcity, ut, RPT_CERTAIN)) {
          best_buildable = ut->defense_strength;
        }
      } unit_type_iterate_end;
      if (best_present > 0
          && best_buildable
             >= best_present * deep_tuning_get()->rotation_power_ratio) {
        city_data->danger = (unsigned int)
          (deep_tuning_get()->rotation_want * updisc);
      }
    }
  }

  {
    /* Defensive diplomat (W01): stock writes a self-described
     * 'totally arbitrary' 16000 into the choice, outranking even
     * military emergencies by magnitude. Evaluate it on a fresh
     * choice and merge it back as an emergency-tier want with a
     * calibrated value instead. */
    struct adv_choice *dip = adv_new_choice();

    dai_choose_diplomat_defensive(ait, pplayer, pcity, dip, our_def);
    if (dip->want > 0) {
      deep_want_apply(dip, (struct deep_want) {
        .tier = DWT_EMERGENCY,
        .value = deep_tuning_get()->diplomat_defence_value });
      if (deep_want_cmp(deep_want_from_choice(dip),
                        deep_want_from_choice(choice)) > 0) {
        adv_choice_copy(choice, dip);
      }
    }
    adv_free_choice(dip);
  }

  if (pcity->feel[CITIZEN_UNHAPPY][FEELING_NATIONALITY]
      + pcity->feel[CITIZEN_ANGRY][FEELING_NATIONALITY] > 0) {
    martial_need = TRUE;
  }

  if (!martial_need) {
    normal_specialist_type_iterate(sp) {
      if (pcity->specialists[sp] > 0
          && get_specialist_output(pcity, sp, O_LUXURY) > 0) {
        martial_need = TRUE;
        break;
      }
    } normal_specialist_type_iterate_end;
  }

  if (martial_need
      && unit_list_size(pcity->tile->units)
         < get_city_bonus(pcity, EFT_MARTIAL_LAW_MAX)) {
    martial_value
      = dai_content_effect_value(pplayer, pcity,
                                 get_city_bonus(pcity,
                                                EFT_MARTIAL_LAW_BY_UNIT),
                                 1, FEELING_FINAL);
  }

  /* Otherwise no need to defend yet */
  if (city_data->danger != 0 || martial_value > 0) {
    struct impr_type *pimprove;
    int num_defenders = unit_list_size(ptile->units);
    int wall_id, danger;
    bool build_walls = TRUE;
    bool def_unit_selected = FALSE;
    int qdanger = city_data->danger * city_data->danger;

    if (qdanger <= 0) {
      /* We only need these defenders because of Martial Law value */
      danger = 0;
      build_walls = FALSE; /* Walls don't provide Martial Law */
    } else {
      /* First determine the danger. It is measured in percents of our
       * defensive strength, capped at 200 + urgency */
      if (qdanger >= our_def) {
        if (urgency == 0) {
          /* Don't waste money */
          danger = 100;
        } else if (our_def == 0) {
          danger = 200 + urgency;
        } else {
          danger = MIN(200, 100 * qdanger / our_def) + urgency;
        }
      } else {
        danger = 100 * qdanger / our_def;
      }

      if (pcity->surplus[O_SHIELD] <= 0 && our_def != 0) {
        /* Won't be able to support anything */
        danger = 0;
      }
    }

    CITY_LOG(LOG_DEBUG, pcity, "d_m_c_b urgency=%d danger=%d num_def=%d "
             "our_def=%d", urgency, danger, num_defenders, our_def);

    if (our_def == 0 && danger > 0) {
      /* Build defensive unit first! Walls will help nothing if there's
       * nobody behind them. */
      if (dai_process_defender_want(ait, nmap, pplayer, pcity, danger,
                                    choice, martial_value)) {
        deep_want_apply(choice, (struct deep_want) {
          .tier = DWT_EMERGENCY, .value = danger });
        adv_choice_set_use(choice, "first defender");
        build_walls = FALSE;
        def_unit_selected = TRUE;

        CITY_LOG(LOG_DEBUG, pcity,
                 "d_m_c_b wants first defender with " ADV_WANT_PRINTF,
                 choice->want);
      }
    }
    if (build_walls) {
      /* We will build walls if we can and want and (have "enough"
       * defenders or can just buy the walls straight away) */

      /* HACK: This needs changing if multiple improvements provide
       * this effect. */
      wall_id = dai_find_source_building(pcity, EFT_DEFEND_BONUS, NULL);
      pimprove = improvement_by_number(wall_id);

      if (wall_id != B_LAST
          && pcity->server.adv->building_want[wall_id] != 0 && our_def != 0
          && can_city_build_improvement_now(pcity, pimprove, RPT_CERTAIN)
          && (danger < 101 || num_defenders > 1
              || (city_data->grave_danger == 0
                  && pplayer->economic.gold
                     > impr_buy_gold_cost(pcity, pimprove,
                                          pcity->shield_stock)))
          && ai_fuzzy(pplayer, TRUE)) {
        if (pcity->server.adv->building_want[wall_id] > 0) {
          /* NB: great wall is under domestic */
          choice->value.building = pimprove;
          /* building_want is hacked by assess_danger() */
          choice->want = pcity->server.adv->building_want[wall_id];
          choice->want = choice->want
            * (0.5 + (ai_trait_get_value(TRAIT_BUILDER, pplayer)
                      / TRAIT_DEFAULT_VALUE / 2));
          if (urgency == 0
              && deep_want_from_choice(choice).tier == DWT_EMERGENCY) {
            /* No urgency: cap the wall want just below the tier
             * boundary (stock capped at exactly the boundary value;
             * the from/apply round trip preserves that number). */
            deep_want_apply(choice, (struct deep_want) {
              .tier = DWT_EMERGENCY, .value = 0 });
          }
          choice->type = CT_BUILDING;
          adv_choice_set_use(choice, "defense building");
          CITY_LOG(LOG_DEBUG, pcity,
                   "d_m_c_b wants defense building with " ADV_WANT_PRINTF,
                   choice->want);
        } else {
          build_walls = FALSE;
        }
      } else {
        build_walls = FALSE;
      }
    }

    /* If our choice isn't defender unit already, consider one */
    if (!def_unit_selected) {
      if ((danger > 0 && num_defenders <= urgency) || martial_value > 0) {
        struct adv_choice uchoice;

        adv_init_choice(&uchoice);

        /* Consider building defensive units */
        if (dai_process_defender_want(ait, nmap, pplayer, pcity, danger,
                                      &uchoice, martial_value)) {
          /* Potential defender found */
          if (urgency == 0
              && uchoice.value.utype->defense_strength == 1) {
            /* FIXME: check other reqs (unit class?) */
            if (get_city_bonus(pcity, EFT_HP_REGEN) > 0) {
              /* Unlikely */
              uchoice.want = MIN(49, danger);
            } else {
              uchoice.want = MIN(25, danger);
            }
          } else {
            uchoice.want = danger;
          }

          uchoice.want += martial_value;
          if (danger > 0) {
            adv_choice_set_use(&uchoice, "defender");
          } else {
            adv_choice_set_use(&uchoice, "police");
          }

          if (!build_walls || uchoice.want > choice->want) {
            adv_choice_copy(choice, &uchoice);
          }
          adv_deinit_choice(&uchoice);

          CITY_LOG(LOG_DEBUG, pcity,
                   "d_m_c_b wants %s with desire " ADV_WANT_PRINTF,
                   utype_rule_name(choice->value.utype),
                   choice->want);
        } else {
          CITY_LOG(LOG_DEBUG, pcity, "d_m_c_b cannot select defender");
        }
      } else {
        CITY_LOG(LOG_DEBUG, pcity, "d_m_c_b does not want defenders");
      }
    }
  } /* Ok, don't need to defend */

  if (pcity->surplus[O_SHIELD] <= 0
      || pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL]
         > pcity->feel[CITIZEN_UNHAPPY][FEELING_EFFECT]
      || pcity->id == ai->wonder_city) {
    /* Things we consider below are not life-saving so we don't want to
     * build them if our populace doesn't feel like it */
    return choice;
  }

  if (pplayer->economic.tax <= 50
      || city_total_unit_gold_upkeep(pcity) <= 0) {
    /* Always allow one unit with real gold upkeep (after
     * EFT_UNIT_UPKEEP_FREE_PER_CITY). Allow more if economics is so
     * strong that we have not increased taxes. */
    allow_gold_upkeep = TRUE;
  } else {
    allow_gold_upkeep = FALSE;
  }

  /* Consider making a land bodyguard */
  punittype = dai_choose_bodyguard(ait, nmap, pcity, TC_LAND,
                                   L_DEFEND_GOOD, allow_gold_upkeep);
  if (punittype) {
    dai_unit_consider_bodyguard(ait, nmap, pcity, punittype, choice);
  }

  /* If we are in severe danger, don't consider attackers. This is
     probably too general. In many cases we will want to buy attackers
     to counterattack. -- Per */
  {
    /* Severe danger: an emergency-tier want that stays emergency even
     * after discounting the martial-law share (stock compared
     * want - martial >= tier base + 0.1). */
    struct deep_want w = deep_want_from_choice(choice);

    if (w.tier == DWT_EMERGENCY && w.value - martial_value >= 0.1
        && city_data->grave_danger > 0) {
    CITY_LOG(LOGLEVEL_BUILD, pcity,
             "severe danger (want " ADV_WANT_PRINTF "), force defender",
             choice->want);
      return choice;
    }
  }

  /* Consider making an offensive diplomat */
  dai_choose_diplomat_offensive(ait, nmap, pplayer, pcity, choice);

  /* Consider making a sea bodyguard */
  punittype = dai_choose_bodyguard(ait, nmap, pcity, TC_OCEAN,
                                   L_DEFEND_GOOD, allow_gold_upkeep);
  if (punittype) {
    dai_unit_consider_bodyguard(ait, nmap, pcity, punittype, choice);
  }

  /* Consider making an airplane */
  (void) dai_choose_attacker_air(ait, nmap, pplayer, pcity, choice,
                                 allow_gold_upkeep);

  /* Consider making a paratrooper */
  dai_choose_paratrooper(ait, nmap, pplayer, pcity, choice,
                         allow_gold_upkeep);

  /* Check if we want a sailing attacker. Have to put sailing first
     before we mung the seamap */
  punittype = dai_choose_attacker(ait, nmap, pcity, TC_OCEAN,
                                  allow_gold_upkeep);
  if (punittype) {
    virtualunit = unit_virtual_create(
      pplayer, pcity, punittype,
      city_production_unit_veteran_level(pcity, punittype));
    choice = kill_something_with(ait, nmap, pplayer, pcity, virtualunit,
                                 choice);
    unit_virtual_destroy(virtualunit);
  }

  /* Consider a land attacker or a ferried land attacker
   * (in which case, we might want a ferry before an attacker)
   */
  punittype = dai_choose_attacker(ait, nmap, pcity, TC_LAND,
                                  allow_gold_upkeep);
  if (punittype) {
    virtualunit = unit_virtual_create(pplayer, pcity, punittype, 1);
    choice = kill_something_with(ait, nmap, pplayer, pcity, virtualunit,
                                 choice);
    unit_virtual_destroy(virtualunit);
  }

  /* Consider a hunter */
  dai_hunter_choice(ait, pplayer, pcity, choice, allow_gold_upkeep);

  /* Consider veteran level enhancing buildings before non-urgent units */
  adjust_ai_unit_choice(pcity, choice);

  if (choice->want <= 0) {
    CITY_LOG(LOGLEVEL_BUILD, pcity, "deep military advisor has no advice");
  } else {
    CITY_LOG(LOGLEVEL_BUILD, pcity,
             "deep military advisor choice: %s (want " ADV_WANT_PRINTF ")",
             adv_choice_rule_name(choice),
             choice->want);
  }

  return choice;
}

struct deep_emergency {
  struct city *pcity;
  unsigned int grave_danger;
  unsigned int urgency;
  int capital;
  adv_want worth;
  int id;
};

/**********************************************************************//**
  Severity order for simultaneous city emergencies: military gravity
  first, then how close the danger is, then strategic role, then plain
  city value; city id keeps the sort deterministic. Lexicographic on
  purpose -- no scale-mixing weights to invent.
**************************************************************************/
static int deep_emergency_cmp(const void *a_, const void *b_)
{
  const struct deep_emergency *a = a_, *b = b_;

  if (a->grave_danger != b->grave_danger) {
    return a->grave_danger > b->grave_danger ? -1 : 1;
  }
  if (a->urgency != b->urgency) {
    return a->urgency > b->urgency ? -1 : 1;
  }
  if (a->capital != b->capital) {
    return b->capital - a->capital;
  }
  if (a->worth != b->worth) {
    return a->worth > b->worth ? -1 : 1;
  }
  return a->id - b->id;
}

/**********************************************************************//**
  Buy and upgrade stuff. Derived from dai_spend_gold(); the difference
  is that the disband-explorers-at-home hack is gated on exploration
  not being wanted (W06) -- stock scraps every explorer that returns
  home, which in hut games destroys the units the new explorer want
  just paid for.
**************************************************************************/
static void deep_spend_gold(struct ai_type *ait, struct player *pplayer,
                            adv_want explore_want)
{
  struct adv_choice bestchoice;
  int cached_limit = dai_gold_reserve(pplayer);
  int expenses;
  bool war_footing = dai_on_war_footing(ait, pplayer);

  if (player_get_expected_income(pplayer) < 0) {
    /* Rotation move 1 (treasury-gated spending): running at a loss,
     * raise the gold floor by the shortfall so upgrades and buys
     * cannot push the projected treasury negative -- otherwise the
     * treasury guard just sells a building to pay for the shine. */
    cached_limit -= player_get_expected_income(pplayer);
  }

  if (explore_want <= 0) {
    /* Disband explorers that are at home but don't serve a purpose. */
    city_list_iterate(pplayer->cities, pcity) {
      struct tile *ptile = pcity->tile;

      unit_list_iterate_safe(ptile->units, punit) {
        if (unit_has_type_role(punit, L_EXPLORER)
            && pcity->id == punit->homecity
            && def_ai_city_data(pcity, ait)->urgency == 0) {
          CITY_LOG(LOG_BUY, pcity, "disbanding %s to increase production",
                   unit_rule_name(punit));
          unit_do_disband_trad(pplayer, punit, ACT_REQ_PLAYER);
        }
      } unit_list_iterate_safe_end;
    } city_list_iterate_end;
  }

  dai_calc_data(pplayer, NULL, &expenses, NULL);

  do {
    bool expensive; /* Don't buy when it costs x2 unless we must */
    int buycost;
    int limit = cached_limit; /* cached_limit is our gold reserve */
    struct city *pcity = NULL;
    struct ai_city *city_data;

    /* Find highest wanted item on the buy list */
    adv_init_choice(&bestchoice);
    city_list_iterate(pplayer->cities, acity) {
      struct ai_city *acity_data = def_ai_city_data(acity, ait);

      if (acity_data->choice.want
          > bestchoice.want && ai_fuzzy(pplayer, TRUE)) {
        adv_choice_copy(&bestchoice, &(acity_data->choice));
        pcity = acity;
      }
    } city_list_iterate_end;

    /* We found nothing, so we're done */
    if (pcity == NULL) {
      break;
    }

    city_data = def_ai_city_data(pcity, ait);

    /* Not dealing with this city a second time */
    city_data->choice.want = -1;

    ADV_CHOICE_ASSERT(bestchoice);

    /* Try upgrade units at danger location (high want is usually danger) */
    if (city_data->urgency > 1) {
      if (bestchoice.type == CT_BUILDING
          && is_wonder(bestchoice.value.building)) {
        CITY_LOG(LOG_BUY, pcity, "Wonder being built in dangerous position!");
      } else {
        /* If we have urgent want, spend more */
        int upgrade_limit = limit;

        if (city_data->urgency > 1) {
          upgrade_limit = expenses;
        }
        /* Upgrade only military units now */
        dai_upgrade_units(pcity, upgrade_limit, TRUE);
      }
    }

    if (pcity->anarchy != 0 && bestchoice.type != CT_BUILDING) {
      continue; /* Nothing we can do */
    }

    /* Cost to complete production */
    buycost = city_production_buy_gold_cost(pcity);

    if (buycost <= 0) {
      continue; /* Already completed */
    }

    if (is_unit_choice_type(bestchoice.type)
        && utype_is_cityfounder(bestchoice.value.utype)) {
      if (get_city_bonus(pcity, EFT_SHRINK_FOOD) <= 0
          && bestchoice.value.utype->pop_cost > 0
          && city_size_get(pcity) <= bestchoice.value.utype->pop_cost) {
        /* Don't buy settlers in cities that cannot afford
         * the population cost. */
        continue;
      } else if (city_list_size(pplayer->cities) > 6) {
        /* Don't waste precious money buying settlers late game
         * since this raises taxes, and we want science. */
        continue;
      } else if (war_footing) {
        continue;
      }
    } else {
      /* We are not a settler. Therefore we increase the cash need we
       * balance our buy desire with to keep cash at hand for
       * emergencies and for upgrades */
      limit *= 2;
    }

    /* It costs x2 to buy something with no shields contributed */
    expensive = (pcity->shield_stock == 0)
                || (pplayer->economic.gold - buycost < limit);

    if (bestchoice.type == CT_ATTACKER
        && buycost
           > utype_build_shield_cost(pcity, NULL,
                                     bestchoice.value.utype) * 2
        && !war_footing) {
      /* Too expensive for an offensive unit */
      continue;
    }

    if (pplayer->economic.gold - expenses >= buycost
        && (!expensive
            || (city_data->grave_danger != 0
                && assess_defense(ait, pcity) == 0)
            || (bestchoice.want > 200 && city_data->urgency > 1))) {
      /* Buy stuff */
      CITY_LOG(LOG_BUY, pcity,
               "Crash buy of %s for %d (want " ADV_WANT_PRINTF ")",
               adv_choice_rule_name(&bestchoice),
               buycost,
               bestchoice.want);
      really_handle_city_buy(pplayer, pcity);
    } else if (city_data->grave_danger != 0
               && bestchoice.type == CT_DEFENDER
               && assess_defense(ait, pcity) == 0) {
      /* We have no gold but MUST have a defender */
      CITY_LOG(LOG_BUY, pcity, "must have %s but can't afford it (%d < %d)!",
               adv_choice_rule_name(&bestchoice),
               pplayer->economic.gold,
               buycost);
      try_to_sell_stuff(pplayer, pcity);
      if (pplayer->economic.gold - expenses >= buycost) {
        CITY_LOG(LOG_BUY, pcity, "now we can afford it (sold something)");
        really_handle_city_buy(pplayer, pcity);
      }
      increase_maxbuycost(pplayer, buycost);
    }
  } while (TRUE);

  if (!war_footing) {
    /* Civilian upgrades now */
    city_list_iterate(pplayer->cities, pcity) {
      dai_upgrade_units(pcity, cached_limit, FALSE);
    } city_list_iterate_end;
  }

  log_base(LOG_BUY, "%s wants to keep %d in reserve (tax factor %d)",
           player_name(pplayer), cached_limit,
           pplayer->ai_common.maxbuycost);
}

/**********************************************************************//**
  Final build-choice selection for one city. Derived from
  dai_city_choose_build(); the differences (W01) are that the
  military-emergency gate is a want TIER check instead of a magnitude
  comparison, the military-vs-domestic merge goes through the deep
  want conversion, and the final decision is logged as a deep-want
  line for conversion calibration.
**************************************************************************/
static void deep_city_choose_build(struct ai_type *ait,
                                   struct player *pplayer,
                                   struct city *pcity)
{
  struct adv_choice *newchoice;
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  struct ai_city *city_data = def_ai_city_data(pcity, ait);
  const struct civ_map *nmap = &(wld.map);

  if (has_handicap(pplayer, H_AWAY)
      && !city_built_last_turn(pcity)
      && city_data->urgency == 0) {
    /* Don't change existing productions unless we have to. */
    return;
  }

  if (is_barbarian(pplayer)) {
    dai_barbarian_choose_build(pplayer, pcity, &(city_data->choice));
  } else {
    if ((deep_want_from_choice(&city_data->choice).tier != DWT_EMERGENCY
         || city_data->urgency == 0)
        && !(dai_on_war_footing(ait, pplayer) && city_data->choice.want > 0
             && pcity->id != adv->wonder_city)) {
      newchoice = domestic_advisor_choose_build(ait, pplayer, pcity);
      if (deep_want_cmp(deep_want_from_choice(newchoice),
                        deep_want_from_choice(&city_data->choice)) > 0) {
        adv_choice_copy(&(city_data->choice), newchoice);
      }
      adv_free_choice(newchoice);
    }
  }

  /* Fallbacks */
  if (city_data->choice.want == 0) {
    /* Fallbacks do happen with techlevel 0, which is now default. -- Per */
    CITY_LOG(LOG_DEBUG, pcity, "Falling back - didn't want to build"
             " soldiers, workers, caravans, settlers, or buildings!");
    city_data->choice.want = 1;
    if (best_role_unit(pcity, action_id_get_role(ACTION_TRADE_ROUTE))) {
      city_data->choice.value.utype
        = best_role_unit(pcity, action_id_get_role(ACTION_TRADE_ROUTE));
      city_data->choice.type = CT_CIVILIAN;
      adv_choice_set_use(&(city_data->choice), "fallback trade route");
    } else {
      unsigned int our_def = assess_defense_quadratic(ait, pcity);

      if (our_def == 0
          && dai_process_defender_want(ait, nmap, pplayer, pcity, 1,
                                       &(city_data->choice), 0)) {
        adv_choice_set_use(&(city_data->choice), "fallback defender");
        CITY_LOG(LOG_DEBUG, pcity, "Building fallback defender");
      } else if (best_role_unit(pcity, UTYF_WORKERS)) {
        city_data->choice.value.utype
          = dai_role_utype_for_terrain_class(pcity, UTYF_WORKERS, TC_LAND);
        city_data->choice.type = CT_CIVILIAN;
        adv_choice_set_use(&(city_data->choice), "fallback worker");
      } else {
        CITY_LOG(LOG_ERROR, pcity, "Cannot even build a fallback "
                 "(caravan/coinage/settlers). Fix the ruleset!");
        city_data->choice.want = 0;
      }
    }
  }

  if (city_data->choice.want != 0) {
    struct universal build_new;

    ADV_CHOICE_ASSERT(city_data->choice);

    deep_want_log(pcity, &city_data->choice);

    switch (city_data->choice.type) {
    case CT_CIVILIAN:
    case CT_ATTACKER:
    case CT_DEFENDER:
      build_new.kind = VUT_UTYPE;
      build_new.value.utype = city_data->choice.value.utype;
      break;
    case CT_BUILDING:
      build_new.kind = VUT_IMPROVEMENT;
      build_new.value.building = city_data->choice.value.building;
      break;
    case CT_NONE:
      build_new.kind = VUT_NONE;
      break;
    case CT_LAST:
      build_new.kind = universals_n_invalid();
      break;
    };

    change_build_target(pplayer, pcity, &build_new,
                        E_CITY_PRODUCTION_CHANGED);
  }
}

/**********************************************************************//**
  City management. Derived from dai_manage_cities(); the difference is
  that city emergencies are collected first, ranked by severity, and
  resolved in that order, instead of in city list order where the first
  city in the list strips worked tiles and gold from more important
  ones behind it (README.AI: 'Emergencies in two cities at once aren't
  handled properly'), and that explorer production competes for the
  build choice (W06; stock has no explorer want at all).
**************************************************************************/
static void deep_manage_cities(struct ai_type *ait, struct player *pplayer)
{
  int ecount = 0, i;
  struct deep_emergency emergencies[city_list_size(pplayer->cities) + 1];
  adv_want explore_want = deep_explore_want(pplayer);

  pplayer->ai_common.maxbuycost = 0;

  TIMING_LOG(AIT_EMERGENCY, TIMER_START);
  city_list_iterate(pplayer->cities, pcity) {
    if (CITY_EMERGENCY(pcity)
        || city_granary_size(city_size_get(pcity)) == pcity->food_stock) {
      /* Having a full granary isn't an emergency, but we want to rearrange */
      auto_arrange_workers(pcity); /* This usually helps */
    }
    if (CITY_EMERGENCY(pcity)) {
      struct ai_city *city_data = def_ai_city_data(pcity, ait);

      emergencies[ecount++] = (struct deep_emergency) {
        .pcity = pcity,
        .grave_danger = city_data->grave_danger,
        .urgency = city_data->urgency,
        .capital = is_capital(pcity) ? 1 : 0,
        .worth = city_data->worth,
        .id = pcity->id,
      };
    }
  } city_list_iterate_end;

  qsort(emergencies, ecount, sizeof(emergencies[0]), deep_emergency_cmp);

  for (i = 0; i < ecount; i++) {
    log_base(deep_metric_log_level(),
             "deep-emergency turn=%d player=\"%s\" rank=%d city=%d"
             " grave=%u urgency=%u capital=%d worth=%d",
             game.info.turn, player_name(pplayer), i,
             emergencies[i].id, emergencies[i].grave_danger,
             emergencies[i].urgency, emergencies[i].capital,
             (int) emergencies[i].worth);
    resolve_city_emergency(ait, pplayer, emergencies[i].pcity);
  }

  city_list_iterate(pplayer->cities, pcity) {
    dai_city_sell_noncritical(pcity, TRUE);
    sync_cities();
  } city_list_iterate_end;
  if (pplayer->economic.tax >= 30 /* Otherwise expect it to increase tax */
      && player_get_expected_income(pplayer) < -(pplayer->economic.gold)) {
    int count = city_list_size(pplayer->cities);
    struct city *sellers[count + 1];

    /* Randomized order */
    count = 0;
    city_list_iterate(pplayer->cities, pcity) {
      sellers[count++] = pcity;
    } city_list_iterate_end;

    for (i = 0; i < count; i++) {
      int replace = fc_rand(count);
      struct city *tmp;

      tmp = sellers[i];
      sellers[i] = sellers[replace];
      sellers[replace] = tmp;
    }

    i = 0;
    while (player_get_expected_income(pplayer) < -(pplayer->economic.gold)
           && i < count) {
      dai_city_sell_noncritical(sellers[i++], FALSE);
    }
  }
  TIMING_LOG(AIT_EMERGENCY, TIMER_STOP);

  TIMING_LOG(AIT_BUILDINGS, TIMER_START);
  building_advisor(pplayer);
  deep_adjust_building_wants(ait, pplayer);
  TIMING_LOG(AIT_BUILDINGS, TIMER_STOP);

  /* Initialize the infrastructure cache, which is used shortly. */
  initialize_infrastructure_cache(pplayer);
  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    struct adv_choice *choice;

    if (city_data->choice.want <= 0) {
      /* Note that this function mungs the seamap, but we don't care */
      TIMING_LOG(AIT_CITY_MILITARY, TIMER_START);
      choice = deep_military_choose_build(ait, &(wld.map), pplayer, pcity,
                                          NULL);
      adv_choice_copy(&(city_data->choice), choice);
      adv_free_choice(choice);
      TIMING_LOG(AIT_CITY_MILITARY, TIMER_STOP);
    }
    if (dai_on_war_footing(ait, pplayer) && city_data->choice.want > 0) {
      city_data->worker_want = 0;
      city_data->founder_want = 0;
      city_data->founder_turn = game.info.turn; /* Do not consider zero we set
                                                 * here a valid value,
                                                 * if real want is needed.
                                                 * Recalculate immediately
                                                 * in such situation. */
      continue; /* Go, soldiers! */
    }
    if (explore_want > city_data->choice.want) {
      /* Stock has no explorer production want anywhere; exploration
       * only ever happens as an AIUNIT_NONE fallback (W06). */
      struct unit_type *explorer = best_role_unit(pcity, L_EXPLORER);

      if (explorer != NULL) {
        city_data->choice.value.utype = explorer;
        city_data->choice.want = explore_want;
        city_data->choice.type = CT_CIVILIAN;
        adv_choice_set_use(&city_data->choice, "explorer");
      }
    }
    /* Will record its findings in pcity->worker_want */
    TIMING_LOG(AIT_CITY_TERRAIN, TIMER_START);
    contemplate_terrain_improvements(ait, pcity);
    TIMING_LOG(AIT_CITY_TERRAIN, TIMER_STOP);

    TIMING_LOG(AIT_CITY_SETTLERS, TIMER_START);
    if (city_data->founder_turn <= game.info.turn) {
      /* Will record its findings in pcity->founder_want */
      deep_contemplate_new_city(ait, pcity);
      /* Avoid recalculating all the time.. */
      city_data->founder_turn
        = game.info.turn + fc_rand(AI_CITY_RECALC_SPEED)
          + AI_CITY_RECALC_SPEED;
    } else if (pcity->server.debug) {
      /* Recalculate every turn */
      deep_contemplate_new_city(ait, pcity);
    }
    TIMING_LOG(AIT_CITY_SETTLERS, TIMER_STOP);
    ADV_CHOICE_ASSERT(city_data->choice);
  } city_list_iterate_end;
  /* Reset auto settler state for the next run. */
  dai_auto_settler_reset(ait, pplayer);

  city_list_iterate(pplayer->cities, pcity) {
    deep_city_choose_build(ait, pplayer, pcity);

    /* Initialize for next turn */
    def_ai_city_data(pcity, ait)->choice.want = -1;
  } city_list_iterate_end;

  deep_spend_gold(ait, pplayer, explore_want);
}

/**********************************************************************//**
  Activities after unit movement; derived from
  dai_do_last_activities() with deep's ranked emergency handling in
  place of the stock city management.
**************************************************************************/
/**********************************************************************//**
  Never let the server force-sell a building. When the projected
  treasury cannot cover the coming turn, sell the least valuable
  sellable building ourselves -- our choice of which, one per city per
  turn like the server's own limit -- and log the emergency. The
  server's bankruptcy handler picks its victim without consulting the
  AI (cityturn.c "cant_maintain"); an autosale is a strictly worse
  version of a choice we could have made a turn earlier.
**************************************************************************/
static void deep_balance_treasury(struct player *pplayer)
{
  int projected = pplayer->economic.gold
                  + player_get_expected_income(pplayer);

  while (projected < 0) {
    struct city *sell_city = NULL;
    struct impr_type *sell_impr = NULL;
    double sell_score = 0.0;

    city_list_iterate(pplayer->cities, pcity) {
      if (pcity->did_sell) {
        continue;
      }
      city_built_iterate(pcity, pimprove) {
        double score;

        if (!can_city_sell_building(pcity, pimprove)) {
          continue;
        }
        /* Shed the biggest recurring bill for the smallest loss of
         * invested shields. */
        score = (1.0 + city_improvement_upkeep(pcity, pimprove))
                / (1.0 + impr_build_shield_cost(pcity, pimprove));
        if (sell_impr == NULL || score > sell_score) {
          sell_city = pcity;
          sell_impr = pimprove;
          sell_score = score;
        }
      } city_built_iterate_end;
    } city_list_iterate_end;

    if (sell_impr == NULL) {
      log_base(deep_metric_log_level(),
               "deep-fin turn=%d player=\"%s\" gold=%d projected=%d"
               " nothing-sellable",
               game.info.turn, player_name(pplayer),
               pplayer->economic.gold, projected);
      return;
    }
    log_base(deep_metric_log_level(),
             "deep-fin turn=%d player=\"%s\" gold=%d projected=%d"
             " sell=\"%s\" city=%d",
             game.info.turn, player_name(pplayer),
             pplayer->economic.gold, projected,
             improvement_rule_name(sell_impr), sell_city->id);
    projected += impr_sell_gold(sell_impr)
                 + city_improvement_upkeep(sell_city, sell_impr);
    do_sell_building(pplayer, sell_city, sell_impr, "sold");
    sell_city->did_sell = TRUE;
  }
}

void deep_do_last_activities(struct ai_type *ait, struct player *pplayer)
{
  TIMING_LOG(AIT_ALL, TIMER_START);
  dai_clear_tech_wants(ait, pplayer);

  dai_manage_government(ait, pplayer);
  dai_adjust_policies(ait, pplayer);
  TIMING_LOG(AIT_TAXES, TIMER_START);
  dai_manage_taxes(ait, pplayer);
  TIMING_LOG(AIT_TAXES, TIMER_STOP);
  TIMING_LOG(AIT_CITIES, TIMER_START);
  deep_manage_cities(ait, pplayer);
  TIMING_LOG(AIT_CITIES, TIMER_STOP);
  /* After taxes and production are settled: if the treasury still
   * cannot cover the coming turn, choose our own sacrifice before
   * the server chooses one for us. */
  deep_balance_treasury(pplayer);
  TIMING_LOG(AIT_TECH, TIMER_START);
  dai_manage_tech(ait, pplayer);
  TIMING_LOG(AIT_TECH, TIMER_STOP);
  dai_manage_spaceship(pplayer);

  TIMING_LOG(AIT_ALL, TIMER_STOP);
}
