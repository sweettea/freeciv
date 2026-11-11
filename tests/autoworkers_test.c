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

/* server/advisors */
#include "autoworkers.c"

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
