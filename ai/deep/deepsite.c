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
#include "effects.h"
#include "game.h"
#include "government.h"
#include "improvement.h"
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
#include "deepops.h"
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

/* Candidate sites whose rival-pressure penalty counted a city one of
 * our own live operations is committed against, and the want that part
 * of the penalty took off them. Instrumentation for
 * expansion-logistics section 3.4; nothing reads these but the log. */
static int deep_site_contest_ours = 0;
static double deep_site_contest_want = 0.0;

/* What an existing city of ours will man, and when: its catchment
 * tiles ranked best-first, with the turn each one gets a citizen (0 if
 * it has one already, -1 if the city never reaches it). Cached for one
 * turn per city, because site evaluation asks about the same
 * neighbours once per candidate tile per settler. */
struct deep_claim {
  int ntiles;
  int *index;                   /* tile index, best first */
  int *arrive;                  /* turn that tile gets a citizen */
};

static void deep_claim_destroy(struct deep_claim *pclaim)
{
  free(pclaim->index);
  free(pclaim->arrive);
  free(pclaim);
}

#define SPECHASH_TAG deep_claim
#define SPECHASH_INT_KEY_TYPE
#define SPECHASH_IDATA_TYPE struct deep_claim *
#define SPECHASH_IDATA_FREE deep_claim_destroy
#include "spechash.h"

static struct deep_claim_hash *deep_claims = NULL;
static int deep_claims_turn = -1;

/* Site evaluation (W02, W02b). Stock cityresult_fill() values a site as
 * two tiles (city centre + single best other) plus everything else
 * divided by GROWTH_POTENTIAL_DEEMPHASIS, and doubles any tile with
 * food >= 2 -- a grassland-seeking missile that builds out instead of
 * up. Deep values the whole catchment as a stream: it simulates the
 * city's growth, so each tile enters on the turn a citizen arrives to
 * work it, and sums the tiles discounted by amortize() for that wait.
 * Centre + stream, in the currency W03 already uses for buildings.
 *
 * The predecessor of the stream was a probability-weighted AVERAGE of
 * the catchment, which could express tile quality but structurally
 * could not express tile quantity -- see deep_catchment_stream(). */

/**********************************************************************//**
  Local isthmus test: among the candidate's eight neighbours, land
  tiles form two or more separate arcs (>=4 land/sea transitions
  around the ring) with the land share narrow -- the tile is a gate.
  A city in a gate closes a continent (human design 2026-07-30).
**************************************************************************/
bool deep_site_is_choke(const struct civ_map *nmap,
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
  Can the ruleset's size gate be lifted at this site by a building the
  city would be allowed to build later (the aqueduct family)? A gate
  that CAN be lifted is not a property of the site -- W03's capacity
  credit exists precisely to make sure it gets lifted, and every
  ruleset we run puts a lifter one early tech away -- while a gate with
  no lifter in the ruleset or the tech tree is final and really does
  bound how many tiles the site will ever man.

  Evaluated lazily, only if the simulation actually reaches the gate,
  because scanning the improvement list is not cheap and sites stall on
  food long before size 8 in both rulesets we measure.
**************************************************************************/
static bool deep_size_gate_is_liftable(struct city *pcity)
{
  improvement_iterate(pimprove) {
    if ((building_has_effect(pimprove, EFT_SIZE_ADJ)
         || building_has_effect(pimprove, EFT_SIZE_UNLIMIT))
        && can_city_build_improvement_later(pcity, pimprove)) {
      return TRUE;
    }
  } improvement_iterate_end;

  return FALSE;
}

/**********************************************************************//**
  Turn on which each ranked catchment tile starts being worked, filled
  into arrive[]: 0 for the tiles the citizens present at the start take
  at once, the simulated growth turn for the rest, and -1 for a tile
  the city never mans. Used both for a candidate site (founding size,
  empty granary) and for an existing city of ours (current size and
  food stock), which is what makes the two comparable in
  deep_neighbour_claim_turn().

  The city works its `size` best tiles, eats game.info.food_cost per
  citizen, banks the surplus and grows when the bank reaches
  city_granary_size() -- the server's own foodbox, so a ruleset that
  scales the box with size (both of ours do) is answered rather than
  assumed. Growth stops for good when the marginal tile no longer feeds
  the citizen working it, which is the site property that a cumulative
  food total could not express: it depends on the ORDER of the tiles,
  not their sum.

  This replaces a target size computed as "the largest k whose first k
  tiles cumulatively out-feed k citizens". That cutoff ignored the
  foodbox entirely (so a site reached its target at the same k whether
  growth took 20 turns or 200), ignored the ruleset size gate, and had
  no notion of WHEN a tile starts earning -- which is the quantity the
  stream that follows is built on.

  Turns are not bounded by a horizon constant: amortize() is a discount
  rate, so a tile arriving on turn 300 prices itself out (0.9583^300 is
  3e-6) without anyone choosing a deadline. Deliberate -- our cells end
  at 80/120/200 turns and the AI must not be told that.
**************************************************************************/
static void deep_growth_arrivals(struct city *pcity, const int *foods,
                                 int n, int center_food, int size0,
                                 int stock0, int *arrive)
{
  int gate = (get_city_bonus(pcity, EFT_SIZE_UNLIMIT) > 0
              ? FC_INFINITY : get_city_bonus(pcity, EFT_SIZE_ADJ));
  bool gate_final = FALSE, gate_checked = FALSE;
  int size, turn = 0, stock = stock0, i;

  for (i = 0; i < n; i++) {
    arrive[i] = -1;
  }
  /* Citizens the city is founded with work its best tiles from turn 0. */
  for (i = 0; i < size0 && i < n; i++) {
    arrive[i] = 0;
  }

  for (size = MAX(size0, 0); size < n; size++) {
    int surplus = center_food, need;

    for (i = 0; i < size; i++) {
      surplus += foods[i];
    }
    surplus -= game.info.food_cost * size;
    if (surplus <= 0) {
      return;
    }
    if (size + 1 > gate) {
      if (!gate_checked) {
        gate_final = !deep_size_gate_is_liftable(pcity);
        gate_checked = TRUE;
      }
      if (gate_final) {
        return;
      }
    }
    need = city_granary_size(size) - stock;
    turn += MAX(0, (need + surplus - 1) / surplus);
    stock = 0;
    arrive[size] = turn;
  }
}

/**********************************************************************//**
  Build the schedule of tiles an existing city of ours will man: its
  catchment ranked by the same weighted output the site evaluator uses,
  with the arrival turn of each tile from the same growth simulation,
  seeded from the city's real size and food stock.
**************************************************************************/
static struct deep_claim *deep_claim_build(struct city *acity)
{
  const struct civ_map *nmap = &(wld.map);
  struct adv_data *adv = adv_data_get(city_owner(acity), NULL);
  int radius_sq = city_map_radius_sq_get(acity);
  int maxtiles = MAX(city_map_tiles(radius_sq), 1);
  struct deep_claim *pclaim = fc_calloc(1, sizeof(*pclaim));
  int *foods = fc_malloc(sizeof(*foods) * maxtiles);
  int *sums = fc_malloc(sizeof(*sums) * maxtiles);
  int center_food = city_tile_output(acity, city_tile(acity), FALSE, O_FOOD);
  int n = 0, j;

  pclaim->index = fc_malloc(sizeof(*pclaim->index) * maxtiles);
  pclaim->arrive = fc_malloc(sizeof(*pclaim->arrive) * maxtiles);

  city_tile_iterate(nmap, radius_sq, city_tile(acity), ptile) {
    struct city *worker = tile_worked(ptile);
    int sum, food;

    if (ptile == city_tile(acity)
        || !city_can_work_tile(acity, ptile)
        || (worker != NULL && worker != acity)) {
      /* Not this city's to work -- and a tile another city already
       * works is nobody's counterfactual, it is simply gone. */
      continue;
    }
    food = city_tile_output(acity, ptile, FALSE, O_FOOD);
    sum = food * adv->food_priority
          + city_tile_output(acity, ptile, FALSE, O_TRADE)
            * adv->science_priority
          + city_tile_output(acity, ptile, FALSE, O_SHIELD)
            * adv->shield_priority;

    /* Insert sorted, best first -- the same ranking, in the same
     * currency, that the candidate site ranks its own catchment by. */
    for (j = n; j > 0 && sums[j - 1] < sum; j--) {
      sums[j] = sums[j - 1];
      foods[j] = foods[j - 1];
      pclaim->index[j] = pclaim->index[j - 1];
    }
    sums[j] = sum;
    foods[j] = food;
    pclaim->index[j] = tile_index(ptile);
    n++;
  } city_tile_iterate_end;

  pclaim->ntiles = n;
  deep_growth_arrivals(acity, foods, n, center_food,
                       city_size_get(acity), acity->food_stock,
                       pclaim->arrive);
  free(foods);
  free(sums);

  return pclaim;
}

/**********************************************************************//**
  The cached claim schedule of one of our cities, built on first ask
  each turn.

  A cache, not a purity violation we are happy about (W26): site
  evaluation asks about the same handful of neighbours once per
  candidate tile per settler, so rebuilding per question would put the
  neighbour's whole catchment inside the site loop. Dropped whenever
  the turn number changes and at game teardown.
**************************************************************************/
static const struct deep_claim *deep_city_claim(struct city *acity)
{
  struct deep_claim *pclaim;

  if (deep_claims == NULL) {
    deep_claims = deep_claim_hash_new();
  }
  if (deep_claims_turn != game.info.turn) {
    deep_claim_hash_clear(deep_claims);
    deep_claims_turn = game.info.turn;
  }
  if (deep_claim_hash_lookup(deep_claims, acity->id, &pclaim)) {
    return pclaim;
  }
  pclaim = deep_claim_build(acity);
  deep_claim_hash_insert(deep_claims, acity->id, pclaim);

  return pclaim;
}

/**********************************************************************//**
  Earliest turn on which a city we already own puts a citizen on this
  tile, or -1 if none of them ever does.

  This is the counterfactual the old overlap WEIGHT was standing in for.
  A boolean "some city of ours could reach this tile" is true of nearly
  every tile on a developed map, says nothing about whether that city
  has a citizen to spare for it, and -- with an early break -- recorded
  a tile contested by three of our cities exactly like one shared with
  one. What matters is whether the neighbour gets there FIRST.
**************************************************************************/
static int deep_neighbour_claim_turn(struct player *pplayer,
                                     struct tile *ptile)
{
  int best = -1;

  city_list_iterate(pplayer->cities, acity) {
    int turn;

    if (sq_map_distance(city_tile(acity), ptile)
        > city_map_radius_sq_get(acity)) {
      continue;
    }
    {
      const struct deep_claim *pclaim = deep_city_claim(acity);
      int i;

      turn = -1;
      for (i = 0; i < pclaim->ntiles; i++) {
        if (pclaim->index[i] == tile_index(ptile)) {
          turn = pclaim->arrive[i];
          break;
        }
      }
    }
    if (turn >= 0 && (best < 0 || turn < best)) {
      best = turn;
    }
  } city_list_iterate_end;

  return best;
}

/**********************************************************************//**
  Drop the per-turn claim cache. Called at game teardown.
**************************************************************************/
void deep_site_free(void)
{
  if (deep_claims != NULL) {
    deep_claim_hash_destroy(deep_claims);
    deep_claims = NULL;
  }
  deep_claims_turn = -1;
}

/**********************************************************************//**
  Value of the workable catchment (excluding the city centre) as a
  stream of discounted citizen-turns: every tile earns its yield from
  the turn the citizen who works it arrives, and is discounted by
  amortize() for the wait. Arrays are gathered by the fill loop; sorts
  in place. Returns 0 when nothing is workable.

  This REPLACES a weighted average, and the difference is the whole
  point of W02b. `vsum / wsum` down-weighted an overlapped or
  out-of-reach tile in the numerator AND the denominator, so as its
  weight fell it did not count against the site, it DROPPED OUT of the
  calculation: a site with 20 free tiles and one with 2 free tiles of
  equal quality scored identically. Measured on largepox-200, a 25-fold
  sweep of site_overlap_weight moved tiles per city by 0.2 -- spacing
  was not tunable, and no weight added to an average could make it so,
  because an average is a statement about tile QUALITY and carries no
  information about tile QUANTITY at all.

  A sum does. Twenty reachable tiles are worth more than two, by
  exactly the discounted yield of the eighteen, and a tile a
  neighbouring city takes is worth zero rather than invisible.

  The currency is the one W03 already uses for buildings:
  deep_capacity_credit() is amortize(per_citizen * steps * weight, ttg),
  and a city is the same object -- a stream of citizens arriving over
  time, each earning tile output. That puts founder_want and building
  want in one currency for the first time.

  Scale note: for the founding citizens arrive[] is 0, so their tiles
  enter undiscounted and the stream is >= stock's centre + best-other
  anchor by construction, plus the discounted tail. The first W02
  landing REPLACED that anchor with an average, deflated every founder
  want and cost 12% of score; a sum cannot repeat that mistake in that
  direction, but it does move the founder_want scale upward, which the
  known-open build-arbitration question (europe-T80) is sensitive to.
**************************************************************************/
static adv_want deep_catchment_stream(struct city *pcity, int *sums,
                                      int *foods, int *potentials,
                                      int *claimed, int n,
                                      int center_food, int size0)
{
  const struct deep_tuning *tune = deep_tuning_get();
  int i, j, kept = 0;
  double total = 0;
  /* When -- and whether -- a citizen ever reaches each ranked tile. */
  int arrive[MAX(n, 1)];

  if (n == 0) {
    return 0;
  }

  /* Sort by sum, descending (insertion sort; n <= catchment size). */
  for (i = 1; i < n; i++) {
    int sv = sums[i], fv = foods[i], pv = potentials[i], cv = claimed[i];

    for (j = i; j > 0 && sums[j - 1] < sv; j--) {
      sums[j] = sums[j - 1];
      foods[j] = foods[j - 1];
      potentials[j] = potentials[j - 1];
      claimed[j] = claimed[j - 1];
    }
    sums[j] = sv;
    foods[j] = fv;
    potentials[j] = pv;
    claimed[j] = cv;
  }

  /* Which tiles a neighbour of ours takes first is decided against the
   * schedule this site would have if it got EVERYTHING, then the site
   * is re-simulated on what is left. The optimistic first pass is the
   * conservative choice: it hands the site its earliest possible
   * arrival turns, so a tile is only conceded when the neighbour beats
   * even that. */
  deep_growth_arrivals(pcity, foods, n, center_food, size0, 0, arrive);

  for (i = 0; i < n; i++) {
    if (claimed[i] >= 0 && (arrive[i] < 0 || claimed[i] <= arrive[i])) {
      /* A city we already own puts a citizen here first, so this city
       * never works the tile: it leaves the stream entirely rather
       * than entering at a weight. Two cities cannot each earn it. */
      continue;
    }
    sums[kept] = sums[i];
    foods[kept] = foods[i];
    potentials[kept] = potentials[i];
    kept++;
  }
  if (kept == 0) {
    return 0;
  }
  deep_growth_arrivals(pcity, foods, kept, center_food, size0, 0, arrive);

  for (i = 0; i < kept; i++) {
    if (arrive[i] < 0) {
      /* No citizen ever works this tile, so it earns nothing. K -- how
       * many tiles a site is worth -- is not a parameter here; it is
       * where the simulation stops. */
      continue;
    }
    /* A tile is worth what it yields now plus a discounted share of
     * what one standard improvement would add (W02 potential term),
     * from the turn its citizen arrives. */
    total += amortize(sums[i]
                      + tune->site_potential_discount * potentials[i],
                      arrive[i]);
  }

  return total;
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
                                               struct tile *center,
                                               int found_size)
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
  int claimed[maxtiles];
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
        /* Stock writes `ptdc->sum *= PERFECTION / 2;` here. PERFECTION
         * is 3 and this is integer division, so the expression is
         * `sum *= 1` -- a NO-OP that has never done anything. Removed
         * rather than preserved: keeping a dead multiply as a tunable
         * would add a knob that provably cannot matter. (Stock also
         * doubles any tile with food >= 2 here; the probability
         * weighting replaces that blunt growth emphasis.) The same
         * dead expression is at daisettler.c:276 and is recorded in
         * findings/upstream-bugs.md; it is not fixed there. */

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

      sums[ncatch] = ptdc->sum;
      foods[ncatch] = ptdc->food;
      /* Ruleset improvement increments, in the same weighted-output
       * currency as the sums (a tile realistically gets one of the
       * two improvements). */
      /* NB this is `(x * 3) / 2`, i.e. x1.5 -- NOT the same as the
       * `PERFECTION / 2` above, which is x1. The same expression means
       * two different things depending on where the parentheses fall.
       * The placeholder preserves the 1.5. */
      potentials[ncatch] = MAX(pot_irr, pot_min)
                           * deep_tuning_get()->site_potential_weight;
      if (pressured) {
        potentials[ncatch] *= deep_tuning_get()->site_pressure_factor;
      }
      claimed[ncatch] = deep_neighbour_claim_turn(pplayer, ptile);
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

  /* The whole catchment speaks, as the discounted citizen-turns it
   * will earn. remaining stays 0 -- GROWTH_POTENTIAL_DEEMPHASIS is
   * retired. */
  {
    adv_want stream
      = deep_catchment_stream(pcity, sums, foods, potentials, claimed,
                              ncatch, result->city_center.tdc->food,
                              virtual_city ? found_size
                                           : MAX(1, city_size_get(pcity)));

    /* Explicit specials term: resources already speak through their
     * yields, but a catchment holding several is worth more than the
     * sum -- one set of buildings serves them all (W02). */
    wspecials = nspecials * deep_tuning_get()->site_special_bonus
                + MAX(0, nspecials - 1)
                  * deep_tuning_get()->site_special_cluster
                + nclaim * deep_tuning_get()->site_claim_bonus;

    if (deep_tuning_get()->site_rival_pressure > 0.0) {
      /* Contested-continent penalty. Rival proximity is otherwise
       * only ever a BONUS here: nclaim * site_claim_bonus rewards
       * founding into a rival's catchment on the theory that it
       * denies them the space. Nothing priced the other side -- a
       * city planted against a border fights for its tiles from the
       * day it is founded, invites the war it cannot yet win, and is
       * the first one lost when that war arrives.
       *
       * Counted over the site's OWN landmass only, because a rival
       * across water is a different problem (that is the boat shadow
       * and overseas terms in deepdefence.c), and weighted 1/(1+d) so
       * it is genuinely "near someone else" rather than a flat tax on
       * every shared continent -- the Europe map is one connected
       * landmass from Morocco to Siberia, so an unweighted continent
       * test would penalise every site on the board equally and
       * discriminate nothing (findings/europe-T80.md §2). */
      Continent_id here = tile_continent(result->tile);
      double contest = 0.0;
      /* ... of which, ours-to-be: the part of the penalty owed to
       * cities one of our OWN live operations is committed against
       * (expansion-logistics section 3.4). Observation only. */
      double contest_ours = 0.0;
      bool ask_ours = log_do_output_for_level(deep_metric_log_level());

      players_iterate_alive(aplayer) {
        if (aplayer == pplayer || pplayers_allied(pplayer, aplayer)) {
          continue;
        }
        city_list_iterate(aplayer->cities, acity) {
          if (tile_continent(city_tile(acity)) != here) {
            continue;
          }
          /* Bounded to the SAME neighbourhood the `pressured` test
           * above already treats as contested. Without a bound this
           * sums every non-allied city on the landmass, which on a map
           * like Europe -- one continent from Morocco to Siberia,
           * ~200 cities -- is a large and nearly IDENTICAL offset for
           * every candidate site. That does not rank sites, it just
           * pushes them all below zero, and a site scoring <= 0 is
           * discarded outright: Europe T122 showed Deep_Carthago with
           * founder_want = 0 in all 18 cities, i.e. expansion switched
           * off entirely rather than merely discouraged. */
          if (sq_map_distance(city_tile(acity), result->tile)
              > 4 * result->city_radius_sq) {
            continue;
          }
          {
            double term = 1.0 / (1.0 + real_map_distance(city_tile(acity),
                                                         result->tile));

            contest += term;
            if (ask_ours && deep_op_targeted(pplayer, acity->id)) {
              contest_ours += term;
            }
          }
        } city_list_iterate_end;
      } players_iterate_alive_end;

      /* Deep prices down exactly the ground it is about to conquer,
       * and re-prices it up only after the capture -- by which time
       * the settlers have gone elsewhere. Nothing has ever measured
       * how much of the penalty that is, so: how many candidate sites
       * carry it, and how much want it costs them at today's weights.
       * INSTRUMENT ONLY -- the penalty itself is unchanged, and a
       * correction waits on this number. */
      if (contest_ours > 0.0) {
        deep_site_contest_ours++;
        deep_site_contest_want
          += contest_ours * deep_tuning_get()->site_rival_pressure
             * MAX(0.0, 1.0 - deep_tuning_get()->site_rival_crowding
                              * deep_land_pressure());
      }

      /* Being near a rival is a choice on a roomy map and unavoidable
       * on a crowded one. Where the whole board shares a landmass the
       * penalty applies to every candidate alike, so it stops ranking
       * sites and only decides whether to expand at all -- the same
       * failure the neighbourhood bound above contains, arriving by
       * density instead of by distance. Forgive it as the map fills;
       * site_rival_crowding 0 keeps the penalty flat. */
      wspecials -= contest * deep_tuning_get()->site_rival_pressure
                   * MAX(0.0, 1.0 - deep_tuning_get()->site_rival_crowding
                                    * deep_land_pressure());
    }
    if (deep_tuning_get()->site_choke_bonus > 0.0
        && deep_site_is_choke(&(wld.map), result->tile)) {
      /* A gate city closes a continent; worth most when crowded. */
      wspecials += deep_tuning_get()->site_choke_bonus
                   * deep_map_crowding();
    }

    /* The stream SUBSUMES stock's best-other anchor rather than
     * sitting beside it: the best catchment tile is the stream's first
     * term, worked from turn zero by a founding citizen, so adding
     * best_other.tdc->sum here as well would pay for that tile twice.
     * The city centre is not in the stream (no citizen works it) and
     * is added as its own undiscounted term, which is what
     * amortize(centre, 0) means. */
    stream *= deep_tuning_get()->site_stream_weight;

    if (virtual_city) {
      result->total = result->city_center.tdc->sum + stream + wspecials;
    } else if (result->best_other.tdc != NULL) {
      result->total = stream + wspecials;
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

/* Per-search census of the naval premium: how many candidates were
 * paid it, how many were refused it because their water closes on its
 * own continent, and how much score the refusals withheld. Reset and
 * reported once per settler search (deep_settler_map_iterate), because
 * one line per CANDIDATE would be tens of thousands per game. */
static int deep_site_naval_paid = 0;
static int deep_site_naval_refused = 0;
static double deep_site_naval_refused_want = 0.0;

/* The same per-search census for the expected-wait discount: overseas
 * candidates actually discounted, and the turns of wait charged. */
static int deep_site_wait_priced = 0;
static int deep_site_wait_turns = 0;

/* THE REFUSAL LADDER (era-probe-evidence 2.  "Deep stops founding
 * because deep_contemplate_new_city() stops wanting cities, not because
 * the gate zeroes a want that exists. That decision is not
 * instrumented.")
 *
 * Every candidate tile the pf map hands the search leaves by exactly
 * one of these doors, so the buckets partition `seen` and the sum is
 * checkable on the line. What the ladder separates is the three
 * hypotheses nobody could tell apart from outside: the map is FULL (the
 * foundability gates eat everything), the map is DANGEROUS (adv_danger
 * does), or the sites are simply WORTH LESS THAN A SETTLER (they score
 * and then go negative against cost and travel). Only the third is a
 * valuation defect, and only the third is fixable by pricing. */
static int deep_site_lad_seen = 0;    /* tiles the pf map offered */
static int deep_site_lad_off = 0;     /* land bridge / peaceful border */
static int deep_site_lad_nofound = 0; /* city_can_be_built_here, unknown */
static int deep_site_lad_mindist = 0; /* citymindist reservation scan */
static int deep_site_lad_danger = 0;  /* adv_danger_at */
static int deep_site_lad_taken = 0;   /* reserved / own city / size limit */
static int deep_site_lad_starve = 0;  /* fill NULL, food/shield starvation */
static int deep_site_lad_scored = 0;  /* produced a cityresult */
static int deep_site_lad_neg = 0;     /* scored, then <= 0 after costs */
static double deep_site_lad_best_raw = 0.0;  /* best cr->total seen */
static double deep_site_lad_best_net = 0.0;  /* best cr->result seen */

/* Per-search memo for deep_wait_expected(), which walks the unit list
 * twice per body of water. Demand and supply cannot move DURING a
 * search, so one answer per water per search is exact, and without it
 * the estimate would be recomputed for every candidate the pf map
 * visits -- tens of thousands of list walks per settler. Reset at the
 * top of deep_settler_map_iterate(). */
#define DEEP_WAIT_MEMO 16
static Continent_id deep_wait_memo_oc[DEEP_WAIT_MEMO];
static int deep_wait_memo_val[DEEP_WAIT_MEMO];
static int deep_wait_memo_n = 0;

/**********************************************************************//**
  deep_wait_expected() for one water, memoised for this search.
**************************************************************************/
static int deep_site_water_wait(struct ai_type *ait, struct player *pplayer,
                                Continent_id ocean)
{
  int i;

  for (i = 0; i < deep_wait_memo_n; i++) {
    if (deep_wait_memo_oc[i] == ocean) {
      return deep_wait_memo_val[i];
    }
  }

  {
    int wait = deep_wait_expected(ait, pplayer, ocean);

    if (deep_wait_memo_n < DEEP_WAIT_MEMO) {
      deep_wait_memo_oc[deep_wait_memo_n] = ocean;
      deep_wait_memo_val[deep_wait_memo_n] = wait;
      deep_wait_memo_n++;
    }

    return wait;
  }
}

/**********************************************************************//**
  The wait a settler bound for THIS beachhead should expect, in turns:
  the shortest queue among the waters that touch it.

  The shortest and not the longest, because a hull arriving from any of
  them lands the passenger: the crossing joins whichever queue is
  moving, so charging the worst adjacent water would price a wait the
  settler would not sit through. Conservative on purpose -- this
  discount is new and its job is to RANK delivery-blind candidates, not
  to talk the AI out of colonising.

  Zero when nothing touches water, which for a candidate the ferry's
  own overlap map reached cannot happen: an overlap map enters a land
  tile only from a tile the hull can float on.
**************************************************************************/
static int deep_site_beach_wait(struct ai_type *ait, struct player *pplayer,
                                const struct tile *ptile)
{
  int best = -1;

  adjc_iterate(&(wld.map), ptile, atile) {
    Continent_id oc = tile_continent(atile);
    int wait;

    if (oc >= 0) {
      continue;                 /* Land. */
    }
    wait = deep_site_water_wait(ait, pplayer, oc);
    if (best < 0 || wait < best) {
      best = wait;
    }
  } adjc_iterate_end;

  return MAX(best, 0);
}

/**********************************************************************//**
  The composite defence factor of a candidate site, on stock's scale:
  10 is flat open ground, +1 per 10% of terrain defence, with the free
  extras a new city is given (river, and whatever the ruleset grants)
  compounded on top. Shape and inputs are stock's
  result_defense_bonus(); only what is DONE with the number differs,
  which is why the composite is computed the same way here.
**************************************************************************/
static int deep_site_defence_factor(struct player *pplayer,
                                    const struct cityresult *result)
{
  int defence = 10 + tile_terrain(result->tile)->defense_bonus / 10;
  int extra_bonus = 0;
  struct tile *vtile = tile_virtual_new(result->tile);
  struct city *vcity = create_city_virtual(pplayer, vtile, "");

  tile_set_worked(vtile, vcity);        /* Link tile_city(vtile). */
  upgrade_city_extras(vcity, NULL);     /* Give the city its free extras. */
  extra_type_iterate(pextra) {
    if (tile_has_extra(vtile, pextra)) {
      extra_bonus += pextra->defense_bonus;
    }
  } extra_type_iterate_end;
  tile_virtual_destroy(vtile);          /* Destroys vcity with it. */

  defence += (defence * extra_bonus) / 100;

  return defence;
}

/**********************************************************************//**
  What a candidate site's defensibility is worth -- valued UPWARDS.

  Stock's result_defense_bonus() is upstream bug #1
  (findings/upstream-bugs.md): it returns
  `100/(total+1) * (100/defence * DEFENSE_EMPHASIS)`, so the defence
  factor sits in a DENOMINATOR and better ground scores less. At site
  total 500 it pays grassland 39.9 and mountains 20.0, and its own
  docstring says the opposite of what it computes ("a % of total
  results equal to a given % of the defense bonus %"). 00-context
  section 5 used it as the example of a bug ai/deep "simply doesn't
  have"; deep called it at this file's site scorer until this session
  (findings/mechanism-map.md D-D).

  This is the docstring's formula, taken literally: the premium is
  site_defence_weight percent of the site's own value, scaled by the
  terrain's defence percentage. Flat ground is the zero point -- it is
  what "no defence bonus" means in every ruleset -- so a site earns a
  premium for the ground it holds and never a penalty. Rule 2's
  doctrine: the correct behaviour lands here, ai/default keeps the bug
  and it is offered upstream separately.

  The weight defaults to stock's own DEFENSE_EMPHASIS (20), which is
  the one magnitude in this area anybody has ever run a game with, and
  is registered as a PLACEHOLDER for the tuning pile rather than
  presented as measured. Setting it to 0 removes the term entirely and
  is this change's off switch.
**************************************************************************/
static adv_want deep_site_defence_bonus(struct player *pplayer,
                                        const struct cityresult *result,
                                        int *factor)
{
  int defence;

  if (deep_tuning_get()->site_defence_weight <= 0.0) {
    /* Off (the shipped default -- see the knob's comment). Skipped
     * before the virtual city, because the factor costs a
     * create_city_virtual + upgrade_city_extras per CANDIDATE and
     * nothing downstream would read it. No RNG is consumed in that
     * path, so this is a pure cost saving. */
    if (factor != NULL) {
      *factor = 0;
    }
    return 0;
  }

  defence = deep_site_defence_factor(pplayer, result);

  if (factor != NULL) {
    *factor = defence;
  }

  /* (defence - 10) / 10 is exactly the ruleset's own defence
   * percentage as a fraction: 0.0 on flat ground, 1.0 for +100%. */
  return result->total * (deep_tuning_get()->site_defence_weight / 100.0)
         * ((defence - 10) / 10.0);
}

/**********************************************************************//**
  What coastal access is worth at a candidate site: site_naval_weight
  percent of the site's own value, replacing stock's compile-time
  NAVAL_EMPHASIS (naval_bonus(), daisettler.c).

  Same shape, same adjacency test, same float arithmetic -- at the
  default 20.0 this is `(total * 20) / 100` operation for operation,
  which is what keeps the knob's introduction inert. What it adds is
  that the number can now be MOVED: a maritime premium of a fifth of a
  site's whole value is one of the largest single terms in site
  scoring and has never been measured, in stock or here.
**************************************************************************/
static adv_want deep_site_naval_bonus(const struct cityresult *result)
{
  if (!is_terrain_class_near_tile(&(wld.map), result->tile, TC_OCEAN)) {
    return 0;
  }

  return (result->total * (adv_want) deep_tuning_get()->site_naval_weight)
         / 100.0f;
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
    deep_site_lad_nofound++;
    return NULL;
  }

  /* Check if another settler has taken a spot within mindist */
  square_iterate(&(wld.map), ptile, game.info.citymindist - 1, tile1) {
    if (citymap_is_reserved(tile1)) {
      deep_site_lad_mindist++;
      return NULL;
    }
  } square_iterate_end;

  if (adv_danger_at(punit, ptile)) {
    deep_site_lad_danger++;
    return NULL;
  }

  if (pcity && (city_size_get(pcity) + unit_pop_value(punit)
                > game.info.add_to_size_limit)) {
    /* Can't exceed population limit. */
    deep_site_lad_taken++;
    return NULL;
  }

  if (!pcity && citymap_is_reserved(ptile)) {
    deep_site_lad_taken++;
    return NULL; /* Reserved, go away */
  }

  /* If (x, y) is an existing city, consider immigration */
  if (pcity && city_owner(pcity) == pplayer) {
    deep_site_lad_taken++;
    return NULL;
  }

  /* The size the city is founded at is the settler's, not a constant:
   * a ruleset can hand out founders that plant a size-2 or size-3 city
   * (and civ2civ3 charges pop_cost 2 for exactly that reason), which
   * changes how many tiles are worked from turn one. */
  cr = deep_cityresult_fill(ait, pplayer, ptile,
                            MAX(1, unit_type_get(punit)->city_size));
  /* Burn CPU, burn! */
  if (!cr) {
    /* Failed to find a good spot */
    deep_site_lad_starve++;
    return NULL;
  }

  /*** Alright: Now consider building a new city ***/

  if (food_starvation(cr) || shield_starvation(cr)) {
    deep_site_lad_starve++;
    cityresult_destroy(cr);
    return NULL;
  }

  {
    int factor = 0;
    adv_want defence = deep_site_defence_bonus(pplayer, cr, &factor);

    /* One line per CANDIDATE is far too much for a benchmark log, so
     * it is written at LOG_VERBOSE: invisible at the server's default
     * verbosity (where every deep-* metric line lives) and captured by
     * a probe run with -d 3 on a corpus position. Stock's inverted
     * term is reported beside the deep one -- it is extern, and having
     * both on one line makes the ranking flip checkable on a real
     * position instead of by arithmetic. The whole block, including
     * that call, is skipped when the level is off. */
    if (log_do_output_for_level(LOG_VERBOSE)) {
      log_verbose("deep-sitedef turn=%d player=\"%s\" at=(%d,%d)"
                  " terrain=\"%s\" factor=%d total=%.2f deep=%.2f"
                  " stock=%.2f",
                  game.info.turn, player_name(pplayer), TILE_XY(cr->tile),
                  terrain_rule_name(tile_terrain(cr->tile)), factor,
                  (double) cr->total, (double) defence,
                  (double) result_defense_bonus(pplayer, cr));
    }
    cr->total += defence;
  }

  /* The maritime premium, but only for water that goes somewhere.
   *
   * naval_bonus() pays NAVAL_EMPHASIS percent of the whole site value
   * for adjacency to ANY ocean-class tile -- a lake, or a sea that
   * closes on this same continent, is indistinguishable there from the
   * world ocean. The build side already refuses to lay a keel at such
   * a city (boat_water_gate, deepdefence.c), so the site is bid up for
   * an option the empire discovers is unavailable only at the
   * shipyard. This is the site-side mirror of that gate, using the
   * same free geometry (get_lake_surrounder, via
   * deep_city_water_bridges); the gate's own sizing found closed-water
   * hulls were 6.9% of hulls owning 11.2% of idle time.
   *
   * site_naval_gate = 0 restores stock's blind premium and is the
   * geometry A/B switch; site_naval_weight is its MAGNITUDE, which
   * stock hard-codes as NAVAL_EMPHASIS and nothing has ever measured. */
  {
    adv_want naval = deep_site_naval_bonus(cr);

    if (naval > 0) {
      if (deep_tuning_get()->site_naval_gate > 0
          && !deep_city_water_bridges(cr->tile)) {
        deep_site_naval_refused++;
        deep_site_naval_refused_want += (double) naval;
        naval = 0;
      } else {
        deep_site_naval_paid++;
      }
    }
    cr->total += naval;
  }

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

  deep_site_naval_paid = 0;
  deep_site_naval_refused = 0;
  deep_site_naval_refused_want = 0.0;
  deep_site_wait_priced = 0;
  deep_site_wait_turns = 0;
  deep_site_contest_ours = 0;
  deep_site_contest_want = 0.0;
  deep_wait_memo_n = 0;
  deep_site_lad_seen = 0;
  deep_site_lad_off = 0;
  deep_site_lad_nofound = 0;
  deep_site_lad_mindist = 0;
  deep_site_lad_danger = 0;
  deep_site_lad_taken = 0;
  deep_site_lad_starve = 0;
  deep_site_lad_scored = 0;
  deep_site_lad_neg = 0;
  deep_site_lad_best_raw = 0.0;
  deep_site_lad_best_net = 0.0;

  pfm = pf_map_new(parameter);
  pf_map_move_costs_iterate(pfm, ptile, move_cost, FALSE) {
    int turns;

    deep_site_lad_seen++;
    if (boat_cost == 0 && pclass->adv.sea_move == MOVE_NONE
        && tile_continent(ptile) != curcont) {
      /* We have an accidental land bridge. Ignore it. */
      deep_site_lad_off++;
      continue;
    }
    if (BORDERS_DISABLED != game.info.borders) {
      struct player *powner = tile_owner(ptile);

      if (NULL != powner
          && powner != pplayer
          && pplayers_in_peace(powner, pplayer)) {
        /* Land theft does not make for good neighbors. */
        deep_site_lad_off++;
        continue;
      }
    }

    /* Calculate worth */
    cr = deep_city_desirability(ait, pplayer, punit, ptile);

    /* Check if actually found something */
    if (cr == NULL) {
      continue;
    }
    deep_site_lad_scored++;
    if ((double) cr->total > deep_site_lad_best_raw) {
      deep_site_lad_best_raw = (double) cr->total;
    }

    /* This algorithm punishes long treks. HOW HARD it punishes them
     * should depend on how hard good sites are to find: on a packed
     * map the frontier is the only land left and the walk is the price
     * of expanding at all, while on an empty one there is always
     * something closer. That is exactly what stock's PERFECTION
     * constant is documented to mean -- "Decrease for maps where good
     * city placements are hard to find" -- but it is a compile-time 3
     * that cannot vary, and it is the same symbol stock also uses as a
     * search-effort bound and (through integer PERFECTION/2) as a
     * yield multiplier.
     *
     * With MORT=24 the retained value is (23/24)^(d*turns), so at the
     * stock d=3 a site 10 turns out keeps 28% of its worth and one 20
     * turns out keeps 8%. Europe T87 measured what that costs:
     * Deep_Carthago held 17 cities and zero settlers with room for ~18
     * more starting 5 tiles away (findings/europe-T80.md).
     *
     * `turns` is real terrain-aware travel time (pathfinder move cost
     * over the unit's move rate), not tile distance, so the multiplier
     * is not standing in for a distance/time confusion. Placeholders
     * 3.0 / 0.0 reproduce PERFECTION exactly. */
    turns = move_cost / parameter->move_rate;
    {
      /* Clamped at zero on purpose. amortize() is benefit * q^delay
       * with q < 1, so a NEGATIVE delay returns MORE than the benefit
       * -- a crowding response strong enough to overshoot would stop
       * discounting distance and start rewarding it, valuing a site
       * more the further away it is. Zero means "distance is free",
       * which is the intended limit. */
      double relief = 1.0 - deep_tuning_get()->site_travel_crowding
                            * deep_map_crowding();
      double d = deep_tuning_get()->site_travel_discount * MAX(relief, 0.0);

      cr->result = amortize(cr->total, (int) (d * turns));
    }

    /* The DELIVERY delay, for an overseas candidate (boat_cost > 0 is
     * the overseas leg; the on-continent search passes 0).
     *
     * Until now the overseas search ranked every candidate with no
     * wait term at all and the CHAMPION was amortized once, afterwards,
     * by the turns to the nearest available boat
     * (deepsettler.c ferry_wait). Two candidates on two different seas
     * -- one with a hull idling on it, one whose queue has been holding
     * settlers for twenty turns -- therefore ranked identically, and
     * the probe path that publishes colony_want priced no wait at all
     * (expansion-logistics section 3.1). This is that missing term, and
     * because it lives inside the candidate loop the probe gets it too:
     * both go through this function.
     *
     * Same currency and same discounter as the travel term above: turns
     * into amortize(). A wait is a delay like a walk is a delay --
     * that is the point of pricing it here rather than as a separate
     * penalty in some other unit.
     *
     * The ferry_wait amortization on the winner stays. It measures a
     * different thing (the fetch: how far the nearest free hull is from
     * THIS settler) and applies where a free hull exists, which is
     * exactly where deep_wait_expected() returns 0. */
    if (boat_cost > 0 && deep_tuning_get()->site_wait_discount > 0.0) {
      int wait = deep_site_beach_wait(ait, pplayer, ptile);

      if (wait > 0) {
        cr->result
          = amortize(cr->result,
                     (int) (deep_tuning_get()->site_wait_discount * wait));
        deep_site_wait_priced++;
        deep_site_wait_turns += wait;
      }
    }

    /* Reduce want by settler cost; also penalise for using a boat.
     * Both weights were stock's implicit 1 and 1/3; they subtract
     * SHIELDS from a weighted tile-yield sum, which is dimensionally
     * awkward enough to deserve the tuner rather than a literal. */
    cr->result -= cost * deep_tuning_get()->site_settler_cost_weight
                  + boat_cost * deep_tuning_get()->site_boat_cost_weight;

    /* The ladder's last rung, and the only one that is a VALUATION
     * verdict rather than a legality one: the site scored, and then the
     * settler it would cost -- plus the walk, plus the queue -- ate the
     * whole score. A search that ends here has found land and priced it
     * below the price of going there. */
    if ((double) cr->result > deep_site_lad_best_net) {
      deep_site_lad_best_net = (double) cr->result;
    }
    if (cr->result <= 0) {
      deep_site_lad_neg++;
    }

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

    /* Can we terminate early?
     *
     * The SAME threshold the caller uses to decide whether an overseas
     * search happens at all (deepsettler.c:711) -- one quantity, "a
     * site good enough to stop looking", read from two places. It was
     * tunable there and a compile-time RESULT_IS_ENOUGH here, so a
     * tuner that raised the knob made the phase-2 gate more demanding
     * while this loop kept stopping at 250: the search abandoned
     * candidates the caller had just been told were not good enough.
     * Default 250.0 == RESULT_IS_ENOUGH, so this is identical until
     * something moves the knob. */
    if (best && best->result > deep_tuning_get()->site_enough_result
        && turns > parameter->move_rate /* sic */
        && best_turn < turns) {
      break;
    }
  } pf_map_move_costs_iterate_end;

  /* The refusal ladder, one line per SEARCH. `seen` is every tile the
   * pf map offered and the buckets partition it, so
   * off+nofound+mindist+danger+taken+starve+scored == seen is an
   * arithmetic self-check on every line. `best_raw` is the best site
   * value the map contains and `best_net` the best after travel, wait
   * and settler cost -- the pair that says whether a search came home
   * empty because there was nothing there or because what was there
   * did not survive its own price. */
  if (deep_site_lad_seen > 0) {
    log_base(deep_metric_log_level(),
             "deep-siteladder turn=%d player=\"%s\" unit=%d boat=%d"
             " seen=%d off=%d nofound=%d mindist=%d danger=%d taken=%d"
             " starve=%d scored=%d neg=%d best_raw=%.1f best_net=%.1f"
             " found=%d",
             game.info.turn, player_name(pplayer), punit->id, boat_cost,
             deep_site_lad_seen, deep_site_lad_off, deep_site_lad_nofound,
             deep_site_lad_mindist, deep_site_lad_danger,
             deep_site_lad_taken, deep_site_lad_starve,
             deep_site_lad_scored, deep_site_lad_neg,
             deep_site_lad_best_raw, deep_site_lad_best_net,
             best != NULL ? 1 : 0);
  }

  /* One census line per SEARCH -- a few hundred a game, against the
   * tens of thousands a per-candidate line would be. `refused` is the
   * counter that proves the gate executes, and `withheld` is the score
   * the closed-water premium no longer buys. */
  if (deep_site_naval_paid > 0 || deep_site_naval_refused > 0) {
    log_base(deep_metric_log_level(),
             "deep-sitenaval turn=%d player=\"%s\" unit=%d paid=%d"
             " refused=%d withheld=%.1f best=(%d,%d)",
             game.info.turn, player_name(pplayer), punit->id,
             deep_site_naval_paid, deep_site_naval_refused,
             deep_site_naval_refused_want,
             best != NULL ? index_to_map_pos_x(tile_index(best->tile)) : -1,
             best != NULL ? index_to_map_pos_y(tile_index(best->tile)) : -1);
  }

  /* The expected-wait census, on the same one-line-per-SEARCH contract.
   * `priced` is the counter that proves the discount executed at all --
   * a term that cannot be shown firing is not evidence -- and `turns`
   * over `priced` is the average wait it charged. */
  if (deep_site_wait_priced > 0) {
    log_base(deep_metric_log_level(),
             "deep-sitewait turn=%d player=\"%s\" unit=%d priced=%d"
             " turns=%d best=(%d,%d)",
             game.info.turn, player_name(pplayer), punit->id,
             deep_site_wait_priced, deep_site_wait_turns,
             best != NULL ? index_to_map_pos_x(tile_index(best->tile)) : -1,
             best != NULL ? index_to_map_pos_y(tile_index(best->tile)) : -1);
  }

  /* Section 3.4's number: candidates taxed for a city we are already
   * coming to take, and what that clause of the tax cost them. One
   * line per search, like the other two censuses. */
  if (deep_site_contest_ours > 0) {
    log_base(deep_metric_log_level(),
             "deep-sitetarget turn=%d player=\"%s\" unit=%d sites=%d"
             " want=%.1f best=(%d,%d)",
             game.info.turn, player_name(pplayer), punit->id,
             deep_site_contest_ours, deep_site_contest_want,
             best != NULL ? index_to_map_pos_x(tile_index(best->tile)) : -1,
             best != NULL ? index_to_map_pos_y(tile_index(best->tile)) : -1);
  }

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
