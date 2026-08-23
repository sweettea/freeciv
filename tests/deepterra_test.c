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

/* L0 unit test for the terrain-change graph walk (deepterra.c).
 *
 * WHAT IT PINS, and why a test rather than a game. The mechanism W29
 * exists for -- "the terrain two steps along is what makes the first
 * step worth taking" -- is only ever exercised in a game when a city
 * happens to want the far terrain, on a venue that has the near one,
 * with a worker free. None of that is needed to check that the search
 * finds the far terrain and prices the path, and a game cannot check
 * it at all: the log line says a chain was priced, not that the price
 * was right.
 *
 * The fixture is civ2civ3's own reachable component with its own times
 * (Grassland/Plains/Forest, plant 15/10, cultivate 10) plus two
 * terrains that exist to catch specific ways a search goes wrong: a
 * dead end and a two-route node where the expensive route is tried
 * first. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

/* common */
#include "game.h"
#include "terrain.h"

/* ai/deep */
#include "deepterra.h"

/* civ2civ3's triangle, then the two probes. */
#define TERRAIN_GRASSLAND 0
#define TERRAIN_PLAINS 1
#define TERRAIN_FOREST 2
#define TERRAIN_JUNGLE 3    /* cultivates to a dead end (Swamp) */
#define TERRAIN_SWAMP 4     /* no cultivate result, no plant result */
#define TERRAIN_MARSH 5     /* two routes to Forest, the direct one dear */
#define TERRAIN_COUNT 6

static int failures = 0;

#define CHECK(cond, ...)                                                \
  do {                                                                  \
    if (!(cond)) {                                                      \
      failures++;                                                       \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);              \
      fprintf(stderr, __VA_ARGS__);                                     \
      fprintf(stderr, "\n");                                            \
    }                                                                   \
  } while (0)

/**********************************************************************//**
  Set one terrain's two outgoing edges. A NULL result is spelled T_NONE,
  which is what a ruleset's "no" becomes.
**************************************************************************/
static void set_edges(int idx, int cultivate_to, int cultivate_time,
                      int plant_to, int plant_time)
{
  struct terrain *pterrain = terrain_by_number(idx);

  pterrain->cultivate_result =
    (cultivate_to < 0 ? T_NONE : terrain_by_number(cultivate_to));
  pterrain->cultivate_time = cultivate_time;
  pterrain->plant_result =
    (plant_to < 0 ? T_NONE : terrain_by_number(plant_to));
  pterrain->plant_time = plant_time;
  /* Transform edges exist in the fixture and must never be followed. */
  pterrain->transform_result = terrain_by_number(TERRAIN_SWAMP);
  pterrain->transform_time = 1;
}

/**********************************************************************//**
  The cumulative cost the walk found for 'idx', or -1 if it was not
  reached.
**************************************************************************/
static int cost_of(const struct deep_terra_step *out, int n, int idx)
{
  int i;

  for (i = 0; i < n; i++) {
    if (out[i].pterrain == terrain_by_number(idx)) {
      return out[i].turns;
    }
  }

  return -1;
}

/**********************************************************************//**
  Terrains and edges. No map, no city, no player: the walk reads
  terrain structures and nothing else.
**************************************************************************/
static void setup_ruleset(void)
{
  terrains_init();
  game.control.terrain_count = TERRAIN_COUNT;

  /* civ2civ3: Grassland plants to Forest (15), Forest cultivates to
   * Plains (10) and plants to Grassland (15), Plains plants to Forest
   * (10). Nothing cultivates Grassland or Plains. */
  set_edges(TERRAIN_GRASSLAND, -1, 0, TERRAIN_FOREST, 15);
  set_edges(TERRAIN_PLAINS, -1, 0, TERRAIN_FOREST, 10);
  set_edges(TERRAIN_FOREST, TERRAIN_PLAINS, 10, TERRAIN_GRASSLAND, 15);
  /* Jungle cultivates to Swamp, which goes nowhere at all. */
  set_edges(TERRAIN_JUNGLE, TERRAIN_SWAMP, 10, -1, 0);
  set_edges(TERRAIN_SWAMP, -1, 0, -1, 0);
  /* Marsh reaches Forest directly for 40, or via Plains for 5 + 10. */
  set_edges(TERRAIN_MARSH, TERRAIN_FOREST, 40, TERRAIN_PLAINS, 5);
}

int main(void)
{
  struct deep_terra_step out[DEEP_TERRA_REACH_MAX];
  int n;

  setup_ruleset();

  /* THE MECHANISM. From Grassland, planting reaches Forest in 15 and
   * then Plains in 25 -- and Plains is not a legal one-step result of
   * anything Grassland can do, so this is exactly the terrain the
   * one-step evaluator cannot see at any price. */
  n = deep_terra_reach(terrain_by_number(TERRAIN_GRASSLAND),
                       ACTIVITY_PLANT, out);
  CHECK(n == 2, "Grassland+plant reaches Forest and Plains, got %d", n);
  CHECK(out[0].pterrain == terrain_by_number(TERRAIN_FOREST)
        && out[0].turns == 15,
        "out[0] must be the immediate result at its own time");
  CHECK(cost_of(out, n, TERRAIN_PLAINS) == 25,
        "Grassland -plant 15-> Forest -cultivate 10-> Plains costs 25,"
        " got %d", cost_of(out, n, TERRAIN_PLAINS));
  CHECK(cost_of(out, n, TERRAIN_GRASSLAND) == -1,
        "the start terrain must never be an entry: Forest plants back"
        " to Grassland, and that path spends 30 turns to arrive where"
        " it began");

  /* The mirror: Plains to Grassland, also two steps, also invisible
   * one step at a time. The times differ, so this is not the same
   * assertion twice. */
  n = deep_terra_reach(terrain_by_number(TERRAIN_PLAINS),
                       ACTIVITY_PLANT, out);
  CHECK(n == 2 && out[0].turns == 10
        && cost_of(out, n, TERRAIN_GRASSLAND) == 25,
        "Plains -plant 10-> Forest -plant 15-> Grassland costs 25");

  /* Forest's own two activities, each forced as the first step: the
   * far side of each is the start terrain, so each stops at one. */
  n = deep_terra_reach(terrain_by_number(TERRAIN_FOREST),
                       ACTIVITY_CULTIVATE, out);
  CHECK(n == 1 && out[0].pterrain == terrain_by_number(TERRAIN_PLAINS)
        && out[0].turns == 10,
        "Forest+cultivate reaches Plains alone (Plains plants back to"
        " Forest, the start)");
  n = deep_terra_reach(terrain_by_number(TERRAIN_FOREST),
                       ACTIVITY_PLANT, out);
  CHECK(n == 1 && out[0].pterrain == terrain_by_number(TERRAIN_GRASSLAND),
        "Forest+plant reaches Grassland alone");

  /* An activity the terrain cannot do is no chain, not an empty one. */
  n = deep_terra_reach(terrain_by_number(TERRAIN_GRASSLAND),
                       ACTIVITY_CULTIVATE, out);
  CHECK(n == 0, "Grassland has no cultivate result, got %d entries", n);

  /* A dead end is reported as itself and nothing follows it. */
  n = deep_terra_reach(terrain_by_number(TERRAIN_JUNGLE),
                       ACTIVITY_CULTIVATE, out);
  CHECK(n == 1 && out[0].pterrain == terrain_by_number(TERRAIN_SWAMP),
        "Jungle+cultivate reaches Swamp and stops");

  /* Two routes to one node: the search must report the cheap one even
   * though the dear one is discovered first (cultivate is edge 0). */
  n = deep_terra_reach(terrain_by_number(TERRAIN_MARSH),
                       ACTIVITY_PLANT, out);
  CHECK(cost_of(out, n, TERRAIN_FOREST) == 15,
        "Marsh -plant 5-> Plains -plant 10-> Forest costs 15, not the"
        " 40 of the direct cultivate edge; got %d",
        cost_of(out, n, TERRAIN_FOREST));
  CHECK(cost_of(out, n, TERRAIN_GRASSLAND) == 30,
        "and Grassland beyond it costs 15 + 15 = 30, which is the"
        " relaxation actually propagating; got %d",
        cost_of(out, n, TERRAIN_GRASSLAND));

  /* Transform edges are not followed: every terrain in the fixture
   * transforms to Swamp in 1 turn, so a walk that followed them would
   * find Swamp everywhere for almost nothing. */
  n = deep_terra_reach(terrain_by_number(TERRAIN_GRASSLAND),
                       ACTIVITY_PLANT, out);
  CHECK(cost_of(out, n, TERRAIN_SWAMP) == -1,
        "transform edges must not be walked -- Fusion Power gates them"
        " in civ2civ3, and an unpriced promise is not a chain");

  /* And transform is not a legal first step here either. */
  n = deep_terra_reach(terrain_by_number(TERRAIN_GRASSLAND),
                       ACTIVITY_TRANSFORM, out);
  CHECK(n == 0, "ACTIVITY_TRANSFORM is not this graph's edge type");

  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return EXIT_FAILURE;
  }

  printf("deepterra: all checks passed\n");

  return EXIT_SUCCESS;
}
