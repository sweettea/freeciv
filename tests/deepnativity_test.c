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

/* L0 unit test for deep's nativity / move-type classification
 * (deepnativity.c), owed since NAVSEE flagged MOVETYPE shipping
 * without one.
 *
 * WHAT IT PINS. The helpers classify a unit class from the ruleset's
 * own terrain nativity -- which TERRAIN CLASSES it is native_to --
 * and from NOTHING else. In particular they never read
 * unit_class::move_type: that field is derived in exactly one place,
 * the CLIENT's packhand.c, so on the server it keeps its
 * zero-initialised value (UMT_LAND, SPECENUM_VALUE0) for every class
 * in every ruleset, and any server-side `== UMT_SEA` comparison is
 * dead code (findings/venues-2026-08-12.md section 0). The
 * UMT_SEA-boundary checks below assert BOTH halves: the field really
 * is UMT_LAND for a floating class, and the classification is
 * unaffected when the field is deliberately set to a wrong value --
 * so a future "fix" that starts trusting move_type fails here loudly
 * instead of silently reclassifying the fleet.
 *
 * deep_utype_is_airborne() is deliberately NOT covered: it reads the
 * action-enabler subsystem, which has no L0-sized fixture. Its
 * nativity half (walks && swims) is pinned via the BOTH class. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

/* common */
#include "game.h"
#include "map.h"
#include "terrain.h"
#include "unittype.h"

/* ai/deep */
#include "deepnativity.h"

/* Three terrains: an ocean, a land, and a lake -- the lake is a
 * SECOND terrain of class TC_OCEAN, because "it floats" must read the
 * terrain's class, not one blessed terrain (the Trireme class is
 * native_to lake AND ocean in both shipped rulesets). */
#define TERRAIN_OCEAN 0
#define TERRAIN_LAND 1
#define TERRAIN_LAKE 2

/* Four classes, one unit type each: the three nativity shapes a
 * ruleset can spell, plus a lake-only floater. */
#define UCLASS_LAND 0    /* native to land only            -> is_land */
#define UCLASS_SEA 1     /* native to ocean + lake         -> is_sea  */
#define UCLASS_BOTH 2    /* native to both: flight         -> neither */
#define UCLASS_LAKE 3    /* native to the lake only        -> is_sea  */

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
  A ruleset of three terrains and four classes, which is all the
  helpers read: tclass and native_to on the terrain side, the class
  index on the unit side. No map, no players, no actions.
**************************************************************************/
static void setup_ruleset(void)
{
  struct terrain *pterrain;
  int i;

  terrains_init();
  game.control.terrain_count = 3;

  pterrain = terrain_by_number(TERRAIN_OCEAN);
  pterrain->tclass = TC_OCEAN;
  BV_CLR_ALL(pterrain->native_to);
  BV_SET(pterrain->native_to, UCLASS_SEA);
  BV_SET(pterrain->native_to, UCLASS_BOTH);

  pterrain = terrain_by_number(TERRAIN_LAND);
  pterrain->tclass = TC_LAND;
  BV_CLR_ALL(pterrain->native_to);
  BV_SET(pterrain->native_to, UCLASS_LAND);
  BV_SET(pterrain->native_to, UCLASS_BOTH);

  pterrain = terrain_by_number(TERRAIN_LAKE);
  pterrain->tclass = TC_OCEAN;
  BV_CLR_ALL(pterrain->native_to);
  BV_SET(pterrain->native_to, UCLASS_SEA);
  BV_SET(pterrain->native_to, UCLASS_LAKE);

  unit_classes_init();
  game.control.num_unit_classes = 4;

  unit_types_init();
  game.control.num_unit_types = 4;
  for (i = 0; i < 4; i++) {
    utype_by_number(i)->uclass = uclass_by_number(i);
  }
}

int main(void)
{
  const struct unit_type *land_ut, *sea_ut, *both_ut, *lake_ut;

  setup_ruleset();
  land_ut = utype_by_number(UCLASS_LAND);
  sea_ut = utype_by_number(UCLASS_SEA);
  both_ut = utype_by_number(UCLASS_BOTH);
  lake_ut = utype_by_number(UCLASS_LAKE);

  /* The class-level primitives: does it walk, does it float. */
  CHECK(deep_uclass_walks(uclass_by_number(UCLASS_LAND))
        && !deep_uclass_swims(uclass_by_number(UCLASS_LAND)),
        "a land-only class must walk and not swim");
  CHECK(deep_uclass_swims(uclass_by_number(UCLASS_SEA))
        && !deep_uclass_walks(uclass_by_number(UCLASS_SEA)),
        "an ocean-native class must swim and not walk");
  CHECK(deep_uclass_walks(uclass_by_number(UCLASS_BOTH))
        && deep_uclass_swims(uclass_by_number(UCLASS_BOTH)),
        "a both-native (flying) class must walk and swim");

  /* The type-level classification: UMT_LAND / UMT_SEA / UMT_BOTH
   * reproduced from nativity (deepnativity.c's mapping comment). */
  CHECK(deep_utype_is_land(land_ut) && !deep_utype_is_sea(land_ut),
        "land-only type misclassified");
  CHECK(deep_utype_is_sea(sea_ut) && !deep_utype_is_land(sea_ut),
        "ocean-native type misclassified");
  CHECK(!deep_utype_is_sea(both_ut) && !deep_utype_is_land(both_ut),
        "a flying (both-native) type is neither pure land nor pure"
        " sea, exactly as the old UMT tests read it");

  /* A class native only to a LAKE still floats: is_ocean() is the
   * terrain's class, not one particular ocean terrain. */
  CHECK(deep_utype_is_sea(lake_ut) && !deep_utype_is_land(lake_ut),
        "a lake-only class floats; terrain CLASS decides, not the"
        " specific terrain");

  /* THE UMT_SEA BOUNDARY, half one: on the server the derived field
   * really is the zero-initialised UMT_LAND for a floating class --
   * the fact that made every server-side `== UMT_SEA` test dead code.
   * The helper must classify the class as sea anyway. */
  CHECK(uclass_by_number(UCLASS_SEA)->move_type == UMT_LAND,
        "premise: the server-side move_type field must still be its"
        " zero-initialised UMT_LAND -- if something now sets it, the"
        " dead-code finding and this test both need re-reading");
  CHECK(deep_utype_is_sea(sea_ut),
        "the floating class must be sea WHILE its move_type field"
        " reads UMT_LAND");

  /* Half two: poison the field in both directions and re-ask. A
   * future 'fix' that starts trusting move_type flips one of these. */
  uclass_by_number(UCLASS_LAND)->move_type = UMT_SEA;
  uclass_by_number(UCLASS_SEA)->move_type = UMT_BOTH;
  CHECK(deep_utype_is_land(land_ut) && !deep_utype_is_sea(land_ut),
        "classification must ignore move_type: land class still land"
        " with the field forced to UMT_SEA");
  CHECK(deep_utype_is_sea(sea_ut) && !deep_utype_is_land(sea_ut),
        "classification must ignore move_type: sea class still sea"
        " with the field forced to UMT_BOTH");

  /* deep_utype_air_reach: drop range or full-move tiles, whichever is
   * longer. SINGLE_MOVE is terrain_control.move_fragments. */
  terrain_control.move_fragments = 3;
  {
    struct unit_type *ut = utype_by_number(UCLASS_BOTH);

    ut->move_rate = 9;
    ut->paratroopers_range = 2;
    CHECK(deep_utype_air_reach(ut) == 3,
          "reach must be the move rate in tiles (9/3) when it beats"
          " the drop range");
    ut->paratroopers_range = 5;
    CHECK(deep_utype_air_reach(ut) == 5,
          "reach must be the drop range when it beats the move rate");
  }

  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return EXIT_FAILURE;
  }

  printf("deepnativity: all checks passed\n");

  return EXIT_SUCCESS;
}
