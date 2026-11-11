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

/* The headless game fixture; see fixture.h. Only public server and
 * common entry points are used, in the order srv_init() and
 * srv_prepare() call them. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <signal.h>
#include <stdlib.h>
#include <time.h>

#include <criterion/criterion.h>

/* utility */
#include "log.h"
#include "rand.h"
#include "registry.h"
#include "support.h"

/* common */
#include "citizens.h"
#include "city.h"
#include "game.h"
#include "government.h"
#include "idex.h"
#include "improvement.h"
#include "map.h"
#include "movement.h"
#include "nation.h"
#include "player.h"
#include "research.h"
#include "tech.h"
#include "terrain.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"
#include "vision.h"

/* server */
#include "aiiface.h"
#include "citytools.h"
#include "cityturn.h"
#include "console.h"
#include "diplhand.h"
#include "maphand.h"
#include "plrhand.h"
#include "sernet.h"
#include "settings.h"
#include "srv_main.h"
#include "stdinhand.h"
#include "unittools.h"

/* server/advisors */
#include "advdata.h"
#include "advruleset.h"
#include "infracache.h"

/* server/ruleset */
#include "ruleload.h"

/* server/generator */
#include "mapgen_utils.h"

/* ai */
#include "aitraits.h"
#include "difficulty.h"

#include "fixture.h"

/* The cities and units the helpers made, freed by fx_fini() before the
 * game is, because the server's own teardown expects real ones. A unit
 * may die during the test, so its id is kept to tell whether the
 * pointer is still live. */
#define FX_MAX_OBJECTS 256
static struct city *fx_cities[FX_MAX_OBJECTS];
static int fx_ncities;
static struct unit *fx_units[FX_MAX_OBJECTS];
static int fx_unit_ids[FX_MAX_OBJECTS];
static int fx_nunits;
static double fx_last_setup;
static bool fx_in_phase;

/**********************************************************************//**
  Seconds on the monotonic clock.
**************************************************************************/
static double fx_now(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return ts.tv_sec + ts.tv_nsec / 1e9;
}

/**********************************************************************//**
  The land terrain the map is filled with: Grassland when the ruleset
  has it, else the first land terrain.
**************************************************************************/
static struct terrain *fx_fill_terrain(void)
{
  struct terrain *grass = terrain_by_rule_name("Grassland");

  if (grass != nullptr) {
    return grass;
  }
  terrain_type_iterate(pterrain) {
    if (terrain_type_terrain_class(pterrain) == TC_LAND) {
      return pterrain;
    }
  } terrain_type_iterate_end;

  return nullptr;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_init(const char *ruleset)
{
  double start = fx_now();
  struct terrain *fill;

  /* The ruleset and nation files are read from the source tree unless
   * the environment already points somewhere. */
  setenv("FREECIV_DATA_PATH", FX_DATA_DIR, 0);

  fx_ncities = 0;
  fx_nunits = 0;
  fx_in_phase = FALSE;

  srv_init();
  /* A fixed seed, so a test that draws random numbers (directions,
   * traits under traitdistribution EVEN) draws the same ones each run. */
  fc_srand(1);
  init_connections();
  /* A failed fc_assert() raises SIGABRT, so it fails the test instead
   * of only being logged. */
  con_log_init(nullptr, LOG_ERROR, SIGABRT);
  settings_init(FALSE);
  stdinhand_init();
  server_game_init(FALSE);

  sz_strlcpy(game.server.rulesetdir, ruleset);
  cr_assert(load_rulesets(nullptr, nullptr, FALSE, nullptr,
                          FALSE, FALSE, FALSE),
            "ruleset \"%s\" did not load from %s", ruleset,
            getenv("FREECIV_DATA_PATH"));

  /* The ruleset is not activated, since activation also tops the
   * players up to 'aifill' and tests make their own. These are the
   * caches activation would fill. */
  techs_precalc_data();
  improvement_feature_cache_init();
  unit_class_iterate(pclass) {
    set_unit_class_caches(pclass);
  } unit_class_iterate_end;
  unit_type_iterate(ptype) {
    ptype->unknown_move_cost = utype_unknown_move_cost(ptype);
    set_unit_type_caches(ptype);
  } unit_type_iterate_end;
  city_production_caravan_shields_init();
  adv_units_ruleset_init();
  CALL_FUNC_EACH_AI(units_ruleset_init);

  /* A flat, non-wrapping square map: native and map coordinates are the
   * same, so (x, y) in a test means what it reads as. */
  wld.map.topology_id = 0;
  wld.map.wrap_id = 0;
  wld.map.xsize = FX_MAP_XSIZE;
  wld.map.ysize = FX_MAP_YSIZE;
  map_init_topology(&wld.map);
  main_map_allocate();
  game_map_init();

  fill = fx_fill_terrain();
  cr_assert_not_null(fill, "ruleset \"%s\" has no land terrain", ruleset);
  whole_map_iterate(&(wld.map), ptile) {
    tile_set_terrain(ptile, fill);
  } whole_map_iterate_end;
  assign_continent_numbers();

  fx_last_setup = fx_now() - start;
}

/**********************************************************************//**
  Suite init for the classic ruleset.
**************************************************************************/
void fx_init_classic(void)
{
  fx_init("classic");
}

/**********************************************************************//**
  Suite init for the civ2civ3 ruleset.
**************************************************************************/
void fx_init_civ2civ3(void)
{
  fx_init("civ2civ3");
}

/**********************************************************************//**
  Suite init for the sandbox ruleset.
**************************************************************************/
void fx_init_sandbox(void)
{
  fx_init("sandbox");
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_fini(void)
{
  int i;

  if (fx_in_phase) {
    players_iterate(pplayer) {
      CALL_PLR_AI_FUNC(phase_finished, pplayer, pplayer);
      adv_data_phase_done(pplayer);
    } players_iterate_end;
    fx_in_phase = FALSE;
  }

  for (i = 0; i < fx_nunits; i++) {
    struct unit *punit = fx_units[i];

    if (game_unit_by_number(fx_unit_ids[i]) != punit) {
      /* Died in the test; the server freed it. */
      continue;
    }
    if (punit->server.vision != nullptr) {
      vision_clear_sight(punit->server.vision);
      vision_free(punit->server.vision);
      punit->server.vision = nullptr;
    }
    unit_list_remove(unit_tile(punit)->units, punit);
    unit_list_remove(unit_owner(punit)->units, punit);
    idex_unregister_unit(&wld, punit);
    unit_virtual_destroy(punit);
  }
  fx_nunits = 0;

  for (i = 0; i < fx_ncities; i++) {
    struct city *pcity = fx_cities[i];

    tile_set_worked(city_tile(pcity), nullptr);
    adv_city_free(pcity);
    city_list_remove(city_owner(pcity)->cities, pcity);
    idex_unregister_city(&wld, pcity);
    destroy_city_virtual(pcity);
  }
  fx_ncities = 0;

  server_game_free();
  stdinhand_free();
  settings_free();
  con_log_close();
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
double fx_setup_seconds(void)
{
  return fx_last_setup;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
struct terrain *fx_terrain(const char *rule_name)
{
  struct terrain *pterrain = terrain_by_rule_name(rule_name);

  cr_assert_not_null(pterrain, "no terrain \"%s\" in %s", rule_name,
                     game.server.rulesetdir);

  return pterrain;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
const struct unit_type *fx_utype(const char *rule_name)
{
  const struct unit_type *putype = unit_type_by_rule_name(rule_name);

  cr_assert_not_null(putype, "no unit type \"%s\" in %s", rule_name,
                     game.server.rulesetdir);

  return putype;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
struct tile *fx_tile(int x, int y)
{
  struct tile *ptile = native_pos_to_tile(&(wld.map), x, y);

  cr_assert_not_null(ptile, "(%d, %d) is off the %dx%d map", x, y,
                     FX_MAP_XSIZE, FX_MAP_YSIZE);

  return ptile;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_set_terrain(int x, int y, const char *rule_name)
{
  tile_set_terrain(fx_tile(x, y), fx_terrain(rule_name));
  assign_continent_numbers();
}

/**********************************************************************//**
  A new player of AI type `ai_tname`, as fx_player() describes.
**************************************************************************/
static struct player *fx_new_player(const char *ai_tname)
{
  struct player *pplayer = server_create_player(-1, ai_tname, nullptr,
                                                FALSE);
  struct nation_type *pnation;

  cr_assert_not_null(pplayer, "could not create a %s player", ai_tname);
  server_player_init(pplayer, TRUE, TRUE);

  pnation = pick_a_nation(nullptr, TRUE, FALSE, NOT_A_BARBARIAN);
  cr_assert_not_null(pnation, "no free playable nation");
  player_set_nation(pplayer, pnation);
  ai_traits_init(pplayer);
  pplayer->is_alive = TRUE;

  return pplayer;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
struct player *fx_player(void)
{
  return fx_new_player(default_ai_type_name());
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
struct player *fx_ai_player(const char *ai_name)
{
  struct player *pplayer;

  if (ai_name == nullptr) {
    ai_name = default_ai_type_name();
  }
  cr_assert_not_null(ai_type_by_name(ai_name),
                     "the %s AI is not compiled in", ai_name);
  pplayer = fx_new_player(ai_name);

  /* As aifill() makes an AI player. */
  pplayer->ai_common.skill_level = game.info.skill_level;
  set_as_ai(pplayer);
  set_ai_level_directer(pplayer, game.info.skill_level);
  CALL_PLR_AI_FUNC(gained_control, pplayer, pplayer);

  return pplayer;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_phase_begin(void)
{
  /* As begin_phase() does first, for all players in one phase and in
   * player order. */
  players_iterate(pplayer) {
    adv_data_phase_init(pplayer, TRUE);
    CALL_PLR_AI_FUNC(phase_begin, pplayer, pplayer, TRUE);
  } players_iterate_end;
  fx_in_phase = TRUE;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_diplstate(struct player *a, struct player *b,
                  enum diplstate_type type)
{
  struct player_diplstate *ab = player_diplstate_get(a, b);
  struct player_diplstate *ba = player_diplstate_get(b, a);

  set_diplstate_type(ab, ba, type);
  ab->turns_left = ba->turns_left = 16;
  ab->first_contact_turn = ba->first_contact_turn = game.info.turn;
  ab->contact_turns_left = ba->contact_turns_left
    = game.server.contactturns;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_war(struct player *a, struct player *b)
{
  fx_diplstate(a, b, DS_WAR);
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_tech(struct player *pplayer, const char *rule_name)
{
  struct advance *padvance = advance_by_rule_name(rule_name);
  struct research *presearch = research_get(pplayer);

  cr_assert_not_null(padvance, "no tech \"%s\" in %s", rule_name,
                     game.server.rulesetdir);
  /* As a savegame load sets a known tech. */
  research_invention_set(presearch, advance_number(padvance), TECH_KNOWN);
  research_update(presearch);
  city_refresh_for_player(pplayer);
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_government(struct player *pplayer, const char *rule_name)
{
  struct government *gov = government_by_rule_name(rule_name);

  cr_assert_not_null(gov, "no government \"%s\" in %s", rule_name,
                     game.server.rulesetdir);
  government_change(pplayer, gov, FALSE);
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_show_all(struct player *pplayer)
{
  map_show_all(pplayer);
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
struct city *fx_city(struct player *owner, int x, int y, const char *name)
{
  struct tile *ptile = fx_tile(x, y);
  struct city *pcity;

  cr_assert(fx_ncities < FX_MAX_OBJECTS, "fixture city table is full");
  pcity = create_city_virtual(owner, ptile, name);
  pcity->id = identity_number();
  idex_register_city(&wld, pcity);
  city_list_prepend(owner->cities, pcity);
  tile_set_owner(ptile, owner, ptile);
  tile_set_worked(ptile, pcity);
  citizens_init(pcity);
  if (game.info.citizen_nationality) {
    citizens_nation_set(pcity, owner->slot, city_size_get(pcity));
  }
  adv_city_alloc(pcity);
  city_refresh_from_main_map(&(wld.map), pcity, nullptr);
  fx_cities[fx_ncities++] = pcity;

  return pcity;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
struct unit *fx_unit(struct player *owner, const char *utype, int x, int y,
                     int veteran)
{
  struct tile *ptile = fx_tile(x, y);
  struct unit *punit;

  cr_assert(fx_nunits < FX_MAX_OBJECTS, "fixture unit table is full");
  punit = unit_virtual_create(owner, nullptr, fx_utype(utype), veteran);
  punit->id = identity_number();
  idex_register_unit(&wld, punit);
  unit_tile_set(punit, ptile);
  unit_list_prepend(ptile->units, punit);
  unit_list_prepend(owner->units, punit);
  fx_unit_ids[fx_nunits] = punit->id;
  fx_units[fx_nunits++] = punit;

  return punit;
}

/**********************************************************************//**
  See fixture.h.
**************************************************************************/
void fx_unit_vision(struct unit *punit)
{
  cr_assert_null(punit->server.vision, "unit %d already has vision",
                 punit->id);
  punit->server.vision = vision_new(unit_owner(punit), unit_tile(punit));
  unit_refresh_vision(punit);
}
