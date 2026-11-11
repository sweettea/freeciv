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

/* The city governor's solver, common/aicore/cm.c, through its public
 * entry points. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* utility */
#include "mem.h"

/* common */
#include "city.h"
#include "game.h"
#include "map.h"
#include "specialist.h"

/* common/aicore */
#include "cm.h"

#include "fixture.h"

TestSuite(cm, .init = fx_init_classic, .fini = fx_fini);

/**********************************************************************//**
  Fill the map with `terrain`.
**************************************************************************/
static void fill_map(const char *terrain)
{
  int x, y;

  for (x = 0; x < FX_MAP_XSIZE; x++) {
    for (y = 0; y < FX_MAP_YSIZE; y++) {
      fx_set_terrain(x, y, terrain);
    }
  }
}

/**********************************************************************//**
  A city of `size` on (x, y), all its citizens default specialists,
  refreshed.
**************************************************************************/
static struct city *sized_city(struct player *owner, int x, int y,
                               int size)
{
  struct city *pcity = fx_city(owner, x, y, "Test");

  fx_see_all(owner);

  city_size_set(pcity, size);
  pcity->specialists[DEFAULT_SPECIALIST] = size;
  city_refresh_from_main_map(&(wld.map), pcity, nullptr);

  return pcity;
}

/**********************************************************************//**
  Turns to fill `pcity`'s granary at `surplus` food a turn, or -1 if it
  never fills.
**************************************************************************/
static int turns_to_grow(const struct city *pcity, int surplus)
{
  int needed = city_granary_size(city_size_get(pcity)) - pcity->food_stock;

  if (surplus <= 0) {
    return -1;
  }
  return (needed + surplus - 1) / surplus;
}

/**********************************************************************//**
  max_growth asks for the fastest growth the city can reach. A size 4
  city in forest with four grassland tiles can grow; with shields worth
  ten times food the governor must still pick the arrangement that
  grows in the fewest turns, not a starving one.
**************************************************************************/
Test(cm, max_growth_grows_fastest)
{
  struct player *owner;
  struct city *pcity;
  struct cm_parameter param;
  struct cm_result *result;
  int best_food, best_turns, turns;

  fill_map("Forest");
  fx_set_terrain(5, 5, "Grassland");
  fx_set_terrain(4, 5, "Grassland");
  fx_set_terrain(6, 5, "Grassland");
  fx_set_terrain(5, 4, "Grassland");
  fx_set_terrain(5, 6, "Grassland");
  owner = fx_player();
  pcity = sized_city(owner, 5, 5, 4);
  result = cm_result_new(pcity);

  /* The most food the city can make. */
  cm_init_parameter(&param);
  param.factor[O_FOOD] = 1;
  param.factor[O_SHIELD] = 0;
  param.factor[O_TRADE] = 0;
  param.factor[O_GOLD] = 0;
  param.factor[O_LUXURY] = 0;
  param.factor[O_SCIENCE] = 0;
  param.happy_factor = 0;
  cm_query_result(pcity, &param, result, FALSE);
  cr_assert(result->found_a_valid, "food %d shield %d trade %d gold %d"
            " lux %d sci %d disorder %d", result->surplus[O_FOOD],
            result->surplus[O_SHIELD], result->surplus[O_TRADE],
            result->surplus[O_GOLD], result->surplus[O_LUXURY],
            result->surplus[O_SCIENCE], result->disorder);
  best_food = result->surplus[O_FOOD];
  best_turns = turns_to_grow(pcity, best_food);
  cr_assert_gt(best_turns, 0, "best food surplus %d", best_food);

  cm_init_parameter(&param);
  param.factor[O_FOOD] = 1;
  param.factor[O_SHIELD] = 10;
  param.minimal_surplus[O_FOOD] = 1;
  param.max_growth = TRUE;
  cm_query_result(pcity, &param, result, FALSE);
  cr_assert(result->found_a_valid);
  turns = turns_to_grow(pcity, result->surplus[O_FOOD]);
  cr_expect_geq(result->surplus[O_FOOD], 1, "max_growth food surplus %d",
                result->surplus[O_FOOD]);
  cr_expect_eq(turns, best_turns, "max_growth grows in %d turns (food %d),"
               " the city can in %d (food %d)", turns,
               result->surplus[O_FOOD], best_turns, best_food);

  cm_result_destroy(result);
}

/* A brute-force solver for checking cm_query_result(): every way to
 * split a city's citizens between its workable tiles and the
 * specialists it can use, each evaluated by refreshing the city as the
 * solver does. Tiles of one terrain are interchangeable on the fixture
 * map, so an arrangement is a count per terrain and per specialist. */

#define ORACLE_MAX_KINDS 16

struct oracle_kind {
  bool is_specialist;
  Specialist_type_id spec;
  int ncindex;                  /* number of tiles of this kind */
  int cindex[MAX_CITY_TILES];   /* their city map indices */
};

struct oracle_outcome {
  int surplus[O_LAST];
  bool disorder, happy;
};

struct oracle {
  struct city *pcity;
  int nkinds;
  struct oracle_kind kinds[ORACLE_MAX_KINDS];
  int count[ORACLE_MAX_KINDS];
  bool *workers_map;
  int noutcomes, maxoutcomes;
  struct oracle_outcome *outcomes;
};

/**********************************************************************//**
  Sort the city's workable tiles and usable specialists into kinds.
**************************************************************************/
static void oracle_kinds(struct oracle *o)
{
  struct city *pcity = o->pcity;
  int radius_sq = city_map_radius_sq_get(pcity);
  const struct civ_map *nmap = &(wld.map);
  struct terrain *kind_terrain[ORACLE_MAX_KINDS];
  int i;

  o->nkinds = 0;
  city_map_iterate(radius_sq, cindex, x, y) {
    struct tile *ptile = city_map_to_tile(nmap, city_tile(pcity),
                                          radius_sq, x, y);

    if (ptile == nullptr || is_free_worked_index(cindex)
        || !city_can_work_tile(pcity, ptile)) {
      continue;
    }
    for (i = 0; i < o->nkinds; i++) {
      if (kind_terrain[i] == tile_terrain(ptile)) {
        break;
      }
    }
    if (i == o->nkinds) {
      cr_assert_lt(o->nkinds, ORACLE_MAX_KINDS);
      kind_terrain[i] = tile_terrain(ptile);
      o->kinds[i].is_specialist = FALSE;
      o->kinds[i].ncindex = 0;
      o->nkinds++;
    }
    o->kinds[i].cindex[o->kinds[i].ncindex++] = cindex;
  } city_map_iterate_end;

  normal_specialist_type_iterate(sp) {
    if (city_can_use_specialist(pcity, sp)) {
      cr_assert_lt(o->nkinds, ORACLE_MAX_KINDS);
      o->kinds[o->nkinds].is_specialist = TRUE;
      o->kinds[o->nkinds].spec = sp;
      o->kinds[o->nkinds].ncindex = city_size_get(pcity);
      o->nkinds++;
    }
  } normal_specialist_type_iterate_end;
}

/**********************************************************************//**
  Refresh the city with the arrangement in o->count and record it.
**************************************************************************/
static void oracle_record(struct oracle *o)
{
  struct city *pcity = o->pcity;
  int radius_sq = city_map_radius_sq_get(pcity);
  struct oracle_outcome *out;
  int i, j;

  city_map_iterate(radius_sq, cindex, x, y) {
    o->workers_map[cindex] = is_free_worked_index(cindex);
  } city_map_iterate_end;
  memset(pcity->specialists, 0,
         sizeof(pcity->specialists[0]) * normal_specialist_count());
  for (i = 0; i < o->nkinds; i++) {
    if (o->kinds[i].is_specialist) {
      pcity->specialists[o->kinds[i].spec] = o->count[i];
    } else {
      for (j = 0; j < o->count[i]; j++) {
        o->workers_map[o->kinds[i].cindex[j]] = TRUE;
      }
    }
  }
  city_refresh_from_main_map(&(wld.map), pcity, o->workers_map);

  if (o->noutcomes == o->maxoutcomes) {
    o->maxoutcomes = MAX(64, 2 * o->maxoutcomes);
    o->outcomes = fc_realloc(o->outcomes,
                             o->maxoutcomes * sizeof(*o->outcomes));
  }
  out = &o->outcomes[o->noutcomes++];
  output_type_iterate(stat) {
    out->surplus[stat] = pcity->surplus[stat];
  } output_type_iterate_end;
  out->disorder = city_unhappy(pcity);
  out->happy = city_happy(pcity);
}

/**********************************************************************//**
  Place `left` citizens on kinds k and up, every way.
**************************************************************************/
static void oracle_enumerate(struct oracle *o, int k, int left)
{
  int n;

  if (k == o->nkinds) {
    if (left == 0) {
      oracle_record(o);
    }
    return;
  }
  for (n = 0; n <= MIN(left, o->kinds[k].ncindex); n++) {
    o->count[k] = n;
    oracle_enumerate(o, k + 1, left - n);
  }
  o->count[k] = 0;
}

/**********************************************************************//**
  Evaluate every arrangement of `pcity`, then put the city back.
**************************************************************************/
static void oracle_run(struct oracle *o, struct city *pcity)
{
  citizens saved[SP_MAX];

  memset(o, 0, sizeof(*o));
  o->pcity = pcity;
  o->workers_map = fc_calloc(city_map_tiles_from_city(pcity),
                             sizeof(*o->workers_map));
  memcpy(saved, pcity->specialists, sizeof(saved));
  oracle_kinds(o);
  oracle_enumerate(o, 0, city_size_get(pcity));
  memcpy(pcity->specialists, saved, sizeof(saved));
  city_refresh_from_main_map(&(wld.map), pcity, nullptr);
}

static void oracle_free(struct oracle *o)
{
  free(o->workers_map);
  free(o->outcomes);
}

/**********************************************************************//**
  The solver's score of an outcome under `param`, as compute_fitness()
  in cm.c defines it.
**************************************************************************/
static int fitness(const int surplus[], bool disorder, bool happy,
                   const struct cm_parameter *param, bool *sufficient)
{
  int weighted = 0;

  *sufficient = TRUE;
  output_type_iterate(stat) {
    weighted += surplus[stat] * param->factor[stat];
    if (surplus[stat] < param->minimal_surplus[stat]) {
      *sufficient = FALSE;
    }
  } output_type_iterate_end;
  if (happy) {
    weighted += param->happy_factor;
  } else if (param->require_happy) {
    *sufficient = FALSE;
  }
  if (disorder && !param->allow_disorder) {
    *sufficient = FALSE;
  }

  return weighted;
}

/**********************************************************************//**
  The oracle's best sufficient score under `param`; FALSE if no
  arrangement is sufficient.
**************************************************************************/
static bool oracle_best(const struct oracle *o,
                        const struct cm_parameter *param, int *best)
{
  bool found = FALSE;
  int i;

  for (i = 0; i < o->noutcomes; i++) {
    const struct oracle_outcome *out = &o->outcomes[i];
    bool sufficient;
    int f = fitness(out->surplus, out->disorder, out->happy, param,
                    &sufficient);

    if (sufficient && (!found || f > *best)) {
      *best = f;
      found = TRUE;
    }
  }

  return found;
}

/**********************************************************************//**
  cm_query_result() finds the best sufficient arrangement whenever one
  exists. A city of size 4 to 7 on mixed land and ocean is checked
  against the brute-force oracle under a sweep of tax rates and of
  floors of 0 to 12 on gold, luxury and science, with and without
  require_happy.
**************************************************************************/
Test(cm, matches_brute_force)
{
  static const char *stripe[] = { "Grassland", "Ocean", "Hills", "Forest",
                                  "Plains", "Ocean" };
  static const int rates[][3] = { /* tax, lux, sci */
    { 0, 0, 100 }, { 60, 0, 40 }, { 40, 0, 60 }, { 30, 30, 40 },
    { 40, 20, 40 }, { 0, 60, 40 }, { 60, 40, 0 }, { 20, 60, 20 },
  };
  static const Output_type_id floored[] = { O_GOLD, O_LUXURY, O_SCIENCE };
  struct player *owner;
  struct city *pcity;
  struct cm_result *result;
  struct oracle o;
  int x, y, size, r, s, m, h, checked = 0, feasible = 0, mismatches = 0;

  for (x = 0; x < FX_MAP_XSIZE; x++) {
    for (y = 0; y < FX_MAP_YSIZE; y++) {
      fx_set_terrain(x, y, stripe[(x + 2 * y) % ARRAY_SIZE(stripe)]);
    }
  }
  fx_set_terrain(8, 8, "Grassland");
  owner = fx_player();
  pcity = fx_city(owner, 8, 8, "Test");
  fx_see_all(owner);
  result = cm_result_new(pcity);

  for (size = 4; size <= 7; size++) {
    city_size_set(pcity, size);
    memset(pcity->specialists, 0, sizeof(pcity->specialists));
    pcity->specialists[DEFAULT_SPECIALIST] = size;
    for (r = 0; r < ARRAY_SIZE(rates); r++) {
      owner->economic.tax = rates[r][0];
      owner->economic.luxury = rates[r][1];
      owner->economic.science = rates[r][2];
      city_refresh_from_main_map(&(wld.map), pcity, nullptr);
      oracle_run(&o, pcity);

      for (s = 0; s < ARRAY_SIZE(floored); s++) {
        for (m = 0; m <= 12; m++) {
          for (h = 0; h < 2; h++) {
            struct cm_parameter param;
            bool exists, sufficient;
            int best = 0, got;

            cm_init_parameter(&param);
            param.minimal_surplus[floored[s]] = m;
            param.require_happy = h;
            exists = oracle_best(&o, &param, &best);
            feasible += exists;
            cm_query_result(pcity, &param, result, FALSE);
            got = fitness(result->surplus, result->disorder, result->happy,
                          &param, &sufficient);
            checked++;
            if (result->aborted || exists != result->found_a_valid
                || (exists && got != best)) {
              if (mismatches < 10) {
                cr_log_error("size %d tax/lux/sci %d/%d/%d %s >= %d%s:"
                             " oracle %s %d, cm %s %d%s", size,
                             rates[r][0], rates[r][1], rates[r][2],
                             get_output_identifier(floored[s]), m,
                             h ? " happy" : "",
                             exists ? "valid" : "none", best,
                             result->found_a_valid ? "valid" : "none",
                             got, result->aborted ? " (aborted)" : "");
              }
              mismatches++;
            }
          }
        }
      }
      oracle_free(&o);
    }
  }

  cm_result_destroy(result);
  /* The sweep reaches both sides of every floor. */
  cr_expect_gt(feasible, 0);
  cr_expect_lt(feasible, checked);
  cr_expect_eq(mismatches, 0, "%d of %d queries (%d feasible) differ from"
               " brute force", mismatches, checked, feasible);
}
