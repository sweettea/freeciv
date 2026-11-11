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

/* Tests of server/advisors/autoworkers.c, static functions included.
 * See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* common */
#include "extras.h"
#include "road.h"
#include "workertask.h"

/* server */
#include "aiiface.h"

/* server/advisors */
#include "autoworkers.c"
#include "infracache.h"

/* ai/default */
#include "daisettler.h"

#include "fixture.h"

TestSuite(autoworkers, .init = fx_init_classic, .fini = fx_fini);

/* Offsets of the tiles adv_workers_road_bonus() looks at, by its own
 * numbering. */
static const int rb_dx[12] = {-1,  0,  1, -1, 1, -1, 0, 1,  0, -2, 2, 0};
static const int rb_dy[12] = {-1, -1, -1,  0, 0,  1, 1, 1, -2,  0, 0, 2};

/* The road bonus at (10, 10) with Road on the tiles `idx` of that
 * numbering, each turned `quarter` quarter turns clockwise. */
static adv_want rotated_road_bonus(const int *idx, int n, int quarter)
{
  struct extra_type *road = extra_type_by_rule_name("Road");
  adv_want bonus;
  int i;

  cr_assert_not_null(road);
  for (i = 0; i < n; i++) {
    int dx = rb_dx[idx[i]], dy = rb_dy[idx[i]], q;

    for (q = 0; q < quarter; q++) {
      int t = dx;

      dx = -dy;
      dy = t;
    }
    tile_add_extra(fx_tile(10 + dx, 10 + dy), road);
  }
  bonus = adv_workers_road_bonus(&(wld.map), fx_tile(10, 10),
                                 extra_road_get(road));
  whole_map_iterate(&(wld.map), ptile) {
    tile_remove_extra(ptile, road);
  } whole_map_iterate_end;

  return bonus;
}

/* The bonus on a square map does not depend on which way the
 * neighbourhood is turned: for every set of Road tiles in it, the same
 * set turned by one, two and three quarters gets the same bonus. */
Test(autoworkers, road_bonus_is_rotation_invariant)
{
  int set, bad = 0;

  for (set = 0; set < (1 << 12); set++) {
    int idx[12], n = 0, i, q;
    adv_want b0;

    for (i = 0; i < 12; i++) {
      if (set & (1 << i)) {
        idx[n++] = i;
      }
    }
    b0 = rotated_road_bonus(idx, n, 0);
    for (q = 1; q < 4; q++) {
      adv_want b = rotated_road_bonus(idx, n, q);

      /* Whole numbers, so they compare exactly. */
      if ((int) b != (int) b0) {
        if (bad++ == 0) {
          cr_log_error("roads on 0x%03x turned %d: bonus " ADV_WANT_PRINTF
                       ", unturned " ADV_WANT_PRINTF, set, q, b, b0);
        }
      }
    }
  }
  cr_expect_eq(bad, 0, "%d of %d turned sets differ", bad, 3 << 12);
}

/* road_move_bonus() for Road on (10, 10), which has terrain
 * `terrain`, with Road on its NW neighbour only (a connectivity bonus
 * of 1). */
static adv_want road_move_bonus_on(const char *terrain)
{
  struct extra_type *road = extra_type_by_rule_name("Road");

  cr_assert_not_null(road);
  fx_set_terrain(10, 10, terrain);
  tile_add_extra(fx_tile(9, 9), road);
  cr_assert_eq((int) adv_workers_road_bonus(&(wld.map), fx_tile(10, 10),
                                            extra_road_get(road)), 1);

  return road_move_bonus(&(wld.map), fx_tile(10, 10), road);
}

/* Classic Road (1/3 move) saves 2/3 of a move on Grassland and 5/3 on
 * Hills; the bonus counts the saving in thirds, plus one. */
Test(autoworkers, road_move_bonus_classic)
{
  cr_expect_eq((int) road_move_bonus_on("Grassland"), 3);
  cr_expect_eq((int) road_move_bonus_on("Hills"), 6);
}

/* A worker request for a Road on (x, y) at `pcity`, as a client sends
 * one. */
static void request_road(struct city *pcity, int x, int y)
{
  struct worker_task *ptask = fc_malloc(sizeof(*ptask));

  worker_task_init(ptask);
  ptask->ptile = fx_tile(x, y);
  ptask->act = ACTIVITY_GEN_ROAD;
  ptask->tgt = extra_type_by_rule_name("Road");
  ptask->want = 100;
  cr_assert_not_null(ptask->tgt);
  worker_task_list_append(pcity->task_reqs, ptask);
}

/* An AI worker that takes one of two requests of a city leaves the
 * other in place. */
Test(autoworkers, ai_worker_takes_one_request)
{
  struct ai_type *ait = ai_type_by_name(default_ai_type_name());
  struct player *pplayer = fx_ai_player(nullptr);
  struct city *pcity = fx_city(pplayer, 5, 5, "Home");
  struct unit *punit = fx_unit(pplayer, "Workers", 6, 5, 0);
  struct workermap *state;

  request_road(pcity, 6, 5);
  request_road(pcity, 4, 5);
  fx_show_all(pplayer);
  fx_phase_begin();

  state = fc_calloc(MAP_INDEX_SIZE, sizeof(*state));
  whole_map_iterate(&(wld.map), ptile) {
    state[tile_index(ptile)].enroute = -1;
    state[tile_index(ptile)].eta = FC_INFINITY;
  } whole_map_iterate_end;
  citymap_turn_init(pplayer);
  initialize_infrastructure_cache(pplayer);

  dai_auto_settler_run(ait, &(wld.map), pplayer, punit, state);

  cr_expect_eq(punit->activity, ACTIVITY_GEN_ROAD, "activity %s",
               unit_activity_name(punit->activity));
  cr_assert_eq(worker_task_list_size(pcity->task_reqs), 1,
               "%d requests left", worker_task_list_size(pcity->task_reqs));
  cr_expect(same_pos(worker_task_list_get(pcity->task_reqs, 0)->ptile,
                     fx_tile(4, 5)));
  FC_FREE(state);
}
