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
#include "mem.h"

/* common */
#include "city.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "tile.h"
#include "unit.h"
#include "unittype.h"

/* common/aicore */
#include "citymap.h"
#include "path_finding.h"
#include "pf_tools.h"

/* server */
#include "citytools.h"
#include "maphand.h"
#include "srv_log.h"

/* server/advisors */
#include "advdata.h"
#include "advgoto.h"
#include "advtools.h"

/* ai */
#include "handicaps.h"

/* ai/default */
#include "daicity.h"
#include "daidata.h"
#include "daiplayer.h"
#include "daisettler.h"

#include "deepsettler.h"
#include "deeptuning.h"

#include "deepdefence.h"
#include "deepsite.h"

/* Local instantiation of the tile-data-cache hash accessors; the
 * hash objects themselves are created and destroyed by daisettler.c
 * (cityresult_new/cityresult_destroy), which binds the destructor. */
static void deep_tile_data_cache_destroy(struct tile_data_cache *ptdc)
{
  free(ptdc);
}

#define SPECHASH_TAG tile_data_cache
#define SPECHASH_INT_KEY_TYPE
#define SPECHASH_IDATA_TYPE struct tile_data_cache *
#define SPECHASH_IDATA_FREE deep_tile_data_cache_destroy
#include "spechash.h"

/* Site evaluation (W02). Stock cityresult_fill() values a site as two
 * tiles (city centre + single best other) plus everything else divided
 * by GROWTH_POTENTIAL_DEEMPHASIS, and doubles any tile with food >= 2
 * -- a grassland-seeking missile that builds out instead of up. Deep
 * scores the whole catchment, weighted by the probability the city
 * ever works each tile (1.0 inside the food-supportable target size,
 * a tuned tail weight beyond it), and expresses the total on the same
 * two-tile scale as stock (centre + probability-weighted average
 * tile), so RESULT_IS_ENOUGH, the amortize discount and the founder
 * want scale stay calibrated -- the founder-scale conversion itself
 * stays gated on W03 per the W01 conversion-gating map. */

/**********************************************************************//**
  Local isthmus test: among the candidate's eight neighbours, land
  tiles form two or more separate arcs (>=4 land/sea transitions
  around the ring) with the land share narrow -- the tile is a gate.
  A city in a gate closes a continent (human design 2026-07-30).
**************************************************************************/
static bool deep_site_is_choke(const struct civ_map *nmap,
                               struct tile *ptile)
{
  static const enum direction8 ring[8] = {
    DIR8_NORTH, DIR8_NORTHEAST, DIR8_EAST, DIR8_SOUTHEAST,
    DIR8_SOUTH, DIR8_SOUTHWEST, DIR8_WEST, DIR8_NORTHWEST
  };
  bool land[8];
  int i, nland = 0, transitions = 0;

  for (i = 0; i < 8; i++) {
    struct tile *t = mapstep(nmap, ptile, ring[i]);

    land[i] = (t != NULL && !is_ocean_tile(t));
    if (land[i]) {
      nland++;
    }
  }
  for (i = 0; i < 8; i++) {
    if (land[i] != land[(i + 1) % 8]) {
      transitions++;
    }
  }
  return nland >= 2 && nland <= 5 && transitions >= 4;
}

/**********************************************************************//**
  Probability-weighted average of the workable catchment (excluding
  the city centre), where the target size -- how many tiles the site
  can ever man -- comes from cumulative food against the ruleset's
  per-citizen food cost. Arrays are gathered by the fill loop; sorts
  in place. Returns 0 when nothing is workable.
**************************************************************************/
static adv_want deep_catchment_average(int *sums, int *foods,
                                       int *potentials, bool *overlapped,
                                       int n, int center_food)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int i, j, target, cumfood;
  double wsum = 0, vsum = 0;

  if (n == 0) {
    return 0;
  }

  /* Sort by sum, descending (insertion sort; n <= catchment size). */
  for (i = 1; i < n; i++) {
    int sv = sums[i], fv = foods[i], pv = potentials[i];
    bool ov = overlapped[i];

    for (j = i; j > 0 && sums[j - 1] < sv; j--) {
      sums[j] = sums[j - 1];
      foods[j] = foods[j - 1];
      potentials[j] = potentials[j - 1];
      overlapped[j] = overlapped[j - 1];
    }
    sums[j] = sv;
    foods[j] = fv;
    potentials[j] = pv;
    overlapped[j] = ov;
  }

  /* Largest size the site's own food can sustain: citizens eat
   * game.info.food_cost each and work the best tiles first. */
  cumfood = center_food;
  target = 0;
  for (i = 0; i < n; i++) {
    cumfood += foods[i];
    if (cumfood >= game.info.food_cost * (i + 1)) {
      target = i + 1;
    }
  }

  for (i = 0; i < n; i++) {
    /* Tiles a neighbouring own city can work are almost never ours
     * to man, whatever their rank (proximity rule). */
    double w = overlapped[i] ? tune->site_overlap_weight
               : (i < target) ? 1.0 : tune->site_tail_weight;

    wsum += w;
    /* A tile is worth what it yields now plus a discounted share of
     * what one standard improvement would add (W02 potential term). */
    vsum += w * (sums[i]
                 + tune->site_potential_discount * potentials[i]);
  }

  return vsum / wsum;
}

/**********************************************************************//**
  Fill cityresult with info about the city spot; derived from stock
  cityresult_fill(). Differences: no food >= 2 doubling, and the total
  is centre + probability-weighted catchment average instead of centre
  + best other, with the remaining/GROWTH_POTENTIAL_DEEMPHASIS term
  retired (the weighting subsumes it).
**************************************************************************/
static struct cityresult *deep_cityresult_fill(struct ai_type *ait,
                                               struct player *pplayer,
                                               struct tile *center)
{
  struct city *pcity = tile_city(center);
  struct government *curr_govt = government_of_player(pplayer);
  struct player *saved_owner = NULL;
  struct tile *saved_claimer = NULL;
  bool virtual_city = FALSE;
  bool handicap = has_handicap(pplayer, H_MAP);
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  struct cityresult *result;
  const struct civ_map *nmap = &(wld.map);

  fc_assert_ret_val(adv != NULL, NULL);
  fc_assert_ret_val(center != NULL, NULL);

  pplayer->government = adv->goal.govt.gov;

  /* Create a city result and set default values. */
  result = cityresult_new(center);

  if (pcity == NULL) {
    pcity = create_city_virtual(pplayer, result->tile, "Virtuaville");
    saved_owner = tile_owner(result->tile);
    saved_claimer = tile_claimer(result->tile);
    tile_set_owner(result->tile, pplayer, result->tile); /* Temporarily */
    city_choose_build_default(nmap, pcity);
    virtual_city = TRUE;
  }

  result->city_radius_sq = city_map_radius_sq_get(pcity);

  {
  int maxtiles = city_map_tiles(result->city_radius_sq);
  int sums[maxtiles];
  int foods[maxtiles];
  int potentials[maxtiles];
  bool overlapped[maxtiles];
  int nspecials = 0;
  int nclaim = 0;
  bool pressured = FALSE;
  double wspecials = 0;
  int ncatch = 0;

  /* Contest context: a non-allied city close enough that this site
   * fights for space with it (W07-lite). Under that pressure the
   * long-term terms are compressed -- potential pays out on a shorter
   * timescale when the neighbourhood is contested. */
  players_iterate_alive(aplayer) {
    if (aplayer == pplayer || pplayers_allied(pplayer, aplayer)) {
      continue;
    }
    city_list_iterate(aplayer->cities, acity) {
      if (sq_map_distance(city_tile(acity), result->tile)
          <= 4 * result->city_radius_sq) {
        pressured = TRUE;
        break;
      }
    } city_list_iterate_end;
    if (pressured) {
      break;
    }
  } players_iterate_alive_end;

  city_tile_iterate_index(nmap, result->city_radius_sq, result->tile, ptile,
                          cindex) {
    int tindex = tile_index(ptile);
    int reserved = citymap_read(ptile);
    bool city_center = (result->tile == ptile);
    struct tile_data_cache *ptdc;

    if (reserved < 0
        || (handicap && !map_is_known(ptile, pplayer))
        || tile_worked(ptile) != NULL) {
      /* Tile is reserved or we can't see it */
      ptdc = tile_data_cache_new();
      ptdc->shield = 0;
      ptdc->trade = 0;
      ptdc->food = 0;
      ptdc->sum = -1;
      ptdc->reserved = reserved;
    } else {
      const struct tile_data_cache *ptdc_hit
        = tdc_plr_get(ait, pplayer, tindex);

      if (!ptdc_hit || city_center) {
        /* We cannot read city center from cache */
        ptdc = tile_data_cache_new();

        ptdc->food = city_tile_output(pcity, ptile, FALSE, O_FOOD);
        ptdc->shield = city_tile_output(pcity, ptile, FALSE, O_SHIELD);
        ptdc->trade = city_tile_output(pcity, ptile, FALSE, O_TRADE);
        ptdc->sum = ptdc->food * adv->food_priority
                    + ptdc->trade * adv->science_priority
                    + ptdc->shield * adv->shield_priority;
        /* Balance perfection. Stock also doubles any tile with
         * food >= 2 here; the probability weighting replaces that
         * blunt growth emphasis. */
        ptdc->sum *= PERFECTION / 2;

        if (!city_center && virtual_city) {
          tdc_plr_set(ait, pplayer, tindex, tile_data_cache_copy(ptdc));
        }
      } else {
        ptdc = tile_data_cache_copy(ptdc_hit);
      }
    }

    ptdc->reserved = reserved;

    /* Avoid crowdedness, except for city center. */
    if (ptdc->sum > 0) {
      ptdc->sum -= MIN(reserved * GROWTH_PRIORITY, ptdc->sum - 1);
    }

    /* Track city centre and the best other tile: the best-other slot
     * keeps its stock meaning (reservation target, defense/corruption
     * anchor) even though the total no longer hangs on it alone. */
    if (city_center) {
      result->city_center.tdc = ptdc;
    } else if (!result->best_other.tdc
               || ptdc->sum > result->best_other.tdc->sum) {
      result->best_other.tdc = ptdc;
      result->best_other.tile = ptile;
      result->best_other.cindex = cindex;
    }

    if (result->city_center.tdc != ptdc && ptdc->sum > 0) {
      const struct terrain *pterrain = tile_terrain(ptile);
      int pot_irr = pterrain->irrigation_food_incr * adv->food_priority;
      int pot_min = pterrain->mining_shield_incr * adv->shield_priority;

      bool near_own = FALSE;

      city_list_iterate(pplayer->cities, acity) {
        if (sq_map_distance(city_tile(acity), ptile)
            <= city_map_radius_sq_get(acity)) {
          near_own = TRUE;
          break;
        }
      } city_list_iterate_end;

      sums[ncatch] = ptdc->sum;
      foods[ncatch] = ptdc->food;
      /* Ruleset improvement increments, in the same weighted-output
       * currency as the sums (a tile realistically gets one of the
       * two improvements). */
      potentials[ncatch] = MAX(pot_irr, pot_min) * PERFECTION / 2;
      if (pressured) {
        potentials[ncatch] *= deep_tuning_get()->site_pressure_factor;
      }
      overlapped[ncatch] = near_own;
      ncatch++;

      if (tile_resource(ptile) != NULL) {
        nspecials++;
      }
      if (tile_owner(ptile) != NULL && tile_owner(ptile) != pplayer
          && !pplayers_allied(tile_owner(ptile), pplayer)) {
        /* Founding here pulls this tile inside our border: space
         * stolen from (or denied to) a non-allied neighbour. */
        nclaim++;
      }
    }

    tile_data_cache_hash_replace(result->tdc_hash, cindex, ptdc);
  } city_tile_iterate_index_end;

  /* We need a city center. */
  if (result->city_center.tdc == NULL) {
    fc_assert(result->city_center.tdc != NULL);

    return NULL;
  }

  /* The whole catchment speaks, weighted by the chance it is ever
   * worked; expressed on stock's two-tile scale. remaining stays 0 --
   * GROWTH_POTENTIAL_DEEMPHASIS is retired. */
  {
    adv_want avg
      = deep_catchment_average(sums, foods, potentials, overlapped,
                               ncatch, result->city_center.tdc->food);
    adv_want depth;

    /* Explicit specials term: resources already speak through their
     * yields, but a catchment holding several is worth more than the
     * sum -- one set of buildings serves them all (W02). */
    wspecials = nspecials * deep_tuning_get()->site_special_bonus
                + MAX(0, nspecials - 1)
                  * deep_tuning_get()->site_special_cluster
                + nclaim * deep_tuning_get()->site_claim_bonus;
    if (deep_tuning_get()->site_choke_bonus > 0.0
        && deep_site_is_choke(&(wld.map), result->tile)) {
      /* A gate city closes a continent; worth most when crowded. */
      wspecials += deep_tuning_get()->site_choke_bonus
                   * deep_map_crowding();
    }

    /* Catchment depth: the probability-weighted average tile value,
     * as an ADDITIVE term replacing stock's remaining/8 -- the first
     * W02 landing replaced the best-other anchor with this average
     * and deflated every founder want (score 0.879, t=-4.75); the
     * stock two-tile anchor stays untouched and the depth term adds
     * catchment discrimination on top. */
    depth = avg * deep_tuning_get()->site_depth_weight;

    if (virtual_city) {
      result->total = result->city_center.tdc->sum
                      + (result->best_other.tdc != NULL
                         ? result->best_other.tdc->sum : 0)
                      + depth + wspecials;
    } else if (result->best_other.tdc != NULL) {
      result->total = result->best_other.tdc->sum + depth + wspecials;
    } else {
      result->total = 0;
      pplayer->government = curr_govt;
      if (virtual_city) {
        destroy_city_virtual(pcity);
        tile_set_owner(result->tile, saved_owner, saved_claimer);
      }
      return result;
    }
  }

  if (result->best_other.tdc == NULL) {
    /* No workable tile besides the centre at all. */
    result->total = 0;
    pplayer->government = curr_govt;
    if (virtual_city) {
      destroy_city_virtual(pcity);
      tile_set_owner(result->tile, saved_owner, saved_claimer);
    }
    return result;
  }

  }

  if (virtual_city) {
    /* Corruption and waste of a size one city deducted, as stock. */
    int shield = result->city_center.tdc->shield
                 + result->best_other.tdc->shield;
    result->waste = adv->shield_priority
                    * city_waste(pcity, O_SHIELD, shield, NULL);

    if (game.info.fulltradesize == 1) {
      int trade = result->city_center.tdc->trade
                  + result->best_other.tdc->trade;
      result->corruption = adv->science_priority
                           * city_waste(pcity, O_TRADE, trade, NULL);
    } else {
      result->corruption = 0;
    }
  } else {
    city_size_add(pcity, 1);
    result->corruption = adv->science_priority
      * (city_waste(pcity, O_TRADE, result->best_other.tdc->trade, NULL)
         - pcity->waste[O_TRADE]);
    result->waste = adv->shield_priority
      * (city_waste(pcity, O_SHIELD, result->best_other.tdc->shield, NULL)
         - pcity->waste[O_SHIELD]);
    city_size_add(pcity, -1);
  }
  result->total -= result->corruption;
  result->total -= result->waste;
  result->total = MAX(0, result->total);

  pplayer->government = curr_govt;
  if (virtual_city) {
    destroy_city_virtual(pcity);
    tile_set_owner(result->tile, saved_owner, saved_claimer);
  }

  fc_assert_ret_val(result->city_center.tdc->sum >= 0, NULL);

  return result;
}

/**********************************************************************//**
  Desirability of founding at ptile; derived from stock
  city_desirability(), calling the deep evaluator.
**************************************************************************/
static struct cityresult *deep_city_desirability(struct ai_type *ait,
                                                 struct player *pplayer,
                                                 struct unit *punit,
                                                 struct tile *ptile)
{
  struct city *pcity = tile_city(ptile);
  struct cityresult *cr = NULL;
  const struct civ_map *nmap = &(wld.map);

  fc_assert_ret_val(punit, NULL);
  fc_assert_ret_val(pplayer, NULL);

  if (!city_can_be_built_here(nmap, ptile, punit, FALSE)
      || (has_handicap(pplayer, H_MAP)
          && !map_is_known(ptile, pplayer))) {
    return NULL;
  }

  /* Check if another settler has taken a spot within mindist */
  square_iterate(&(wld.map), ptile, game.info.citymindist - 1, tile1) {
    if (citymap_is_reserved(tile1)) {
      return NULL;
    }
  } square_iterate_end;

  if (adv_danger_at(punit, ptile)) {
    return NULL;
  }

  if (pcity && (city_size_get(pcity) + unit_pop_value(punit)
                > game.info.add_to_size_limit)) {
    /* Can't exceed population limit. */
    return NULL;
  }

  if (!pcity && citymap_is_reserved(ptile)) {
    return NULL; /* Reserved, go away */
  }

  /* If (x, y) is an existing city, consider immigration */
  if (pcity && city_owner(pcity) == pplayer) {
    return NULL;
  }

  cr = deep_cityresult_fill(ait, pplayer, ptile); /* Burn CPU, burn! */
  if (!cr) {
    /* Failed to find a good spot */
    return NULL;
  }

  /*** Alright: Now consider building a new city ***/

  if (food_starvation(cr) || shield_starvation(cr)) {
    cityresult_destroy(cr);
    return NULL;
  }

  cr->total += result_defense_bonus(pplayer, cr);
  cr->total += naval_bonus(cr);

  /* Add remaining points, which is our potential (always 0 here --
   * the catchment weighting already spoke). */
  cr->total += cr->remaining;

  fc_assert_ret_val(cr->total >= 0, NULL); /* Does not free cr! */

  return cr;
}

/**********************************************************************//**
  Find nearest and best city placement in a PF iteration; derived from
  stock settler_map_iterate(), calling the deep evaluator.
**************************************************************************/
struct cityresult *deep_settler_map_iterate(struct ai_type *ait,
                                            struct pf_parameter *parameter,
                                            struct unit *punit,
                                            int boat_cost)
{
  struct cityresult *cr = NULL, *best = NULL;
  int best_turn = 0; /* Which turn we found the best fit */
  struct player *pplayer = unit_owner(punit);
  struct pf_map *pfm;
  struct unit_class *pclass = unit_class_get(punit);
  Continent_id curcont = tile_continent(unit_tile(punit));
  int cost = unit_build_shield_cost_base(punit);

  pfm = pf_map_new(parameter);
  pf_map_move_costs_iterate(pfm, ptile, move_cost, FALSE) {
    int turns;

    if (boat_cost == 0 && pclass->adv.sea_move == MOVE_NONE
        && tile_continent(ptile) != curcont) {
      /* We have an accidental land bridge. Ignore it. */
      continue;
    }
    if (BORDERS_DISABLED != game.info.borders) {
      struct player *powner = tile_owner(ptile);

      if (NULL != powner
          && powner != pplayer
          && pplayers_in_peace(powner, pplayer)) {
        /* Land theft does not make for good neighbors. */
        continue;
      }
    }

    /* Calculate worth */
    cr = deep_city_desirability(ait, pplayer, punit, ptile);

    /* Check if actually found something */
    if (cr == NULL) {
      continue;
    }

    /* This algorithm punishes long treks */
    turns = move_cost / parameter->move_rate;
    cr->result = amortize(cr->total, PERFECTION * turns);

    /* Reduce want by settler cost; also penalise for using a boat. */
    cr->result -= cost + boat_cost / 3;

    /* Find best spot */
    if ((!best && cr->result > 0)
        || (best && cr->result > best->result)) {
      cityresult_destroy(best);
      best = cr;
      cr = NULL;
      best_turn = turns;
    } else {
      cityresult_destroy(cr);
      cr = NULL;
    }

    /* Can we terminate early? */
    if (best && best->result > RESULT_IS_ENOUGH
        && turns > parameter->move_rate /* sic */
        && best_turn < turns) {
      break;
    }
  } pf_map_move_costs_iterate_end;

  pf_map_destroy(pfm);

  return best;
}

/**********************************************************************//**
  Estimate the founder want of a new city built by pcity; derived from
  stock contemplate_new_city(), going through the deep site evaluator
  so production decisions and settler routing finally price sites the
  same way (gap noted since W05).
**************************************************************************/
void deep_contemplate_new_city(struct ai_type *ait, struct city *pcity)
{
  struct unit *virtualunit;
  struct tile *pcenter = city_tile(pcity);
  struct player *pplayer = city_owner(pcity);
  struct unit_type *unit_type;

  if (game.scenario.prevent_new_cities) {
    return;
  }

  unit_type = best_role_unit(pcity, action_id_get_role(ACTION_FOUND_CITY));

  if (unit_type == NULL) {
    log_debug("No ACTION_FOUND_CITY role unit available");
    return;
  }

  /* Create a localized "virtual" unit to do operations with. */
  virtualunit = unit_virtual_create(pplayer, pcity, unit_type, 0);
  unit_tile_set(virtualunit, pcenter);

  if (is_ai(pplayer)) {
    struct cityresult *result;
    bool is_coastal = is_terrain_class_near_tile(&(wld.map), pcenter,
                                                 TC_OCEAN);
    struct ai_city *city_data = def_ai_city_data(pcity, ait);

    result = deep_find_best_city_placement(ait, virtualunit, is_coastal,
                                           is_coastal);

    if (result) {
      fc_assert_ret(0 <= result->result); /* 'result' is not freed! */

      CITY_LOG(LOG_DEBUG, pcity, "want(" ADV_WANT_PRINTF ") to establish"
               " city at (%d, %d) and will %s to get there", result->result,
               TILE_XY(result->tile),
               (result->virt_boat ? "build a boat" :
                (result->overseas ? "use a boat" : "walk")));

      city_data->founder_want = (result->virt_boat ?
                                 -result->result : result->result);
      city_data->founder_boat = result->overseas;

      cityresult_destroy(result);
    } else {
      CITY_LOG(LOG_DEBUG, pcity, "want no city");
      city_data->founder_want = 0;
    }
  } else {
    fc_assert_ret(is_ai(pplayer));
  }

  unit_virtual_destroy(virtualunit);
}
