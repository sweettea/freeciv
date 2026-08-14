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

#include <string.h>

/* utility */
#include "support.h"

/* common */
#include "ai.h"
#include "player.h"

/* ai/baseline/default (vendored; all globals renamed to bl_* by the
 * force-included baseline_syms.h) */
#include "daicity.h"
#include "daiplayer.h"
#include "daiunit.h"

/* ai/baseline/classic (vendored) */
#include "classicai.h"

#include "baselineai.h"

static struct ai_type *baseline_self = NULL;

/**********************************************************************//**
  Save baseline AI player relation data under the "baseline" savefile
  prefix. The vendored classic wrappers hardcode the "ai" prefix, which
  the live classic module already writes; sharing it corrupts the save
  with duplicate keys.
**************************************************************************/
static void bai_player_save_relations(struct player *pplayer,
                                      struct player *other,
                                      struct section_file *file,
                                      int plrno)
{
  dai_player_save_relations(baseline_self, "baseline", pplayer, other,
                            file, plrno);
}

/**********************************************************************//**
  Load baseline AI player relation data from the "baseline" prefix.
**************************************************************************/
static void bai_player_load_relations(struct player *pplayer,
                                      struct player *other,
                                      const struct section_file *file,
                                      int plrno)
{
  dai_player_load_relations(baseline_self, "baseline", pplayer, other,
                            file, plrno);
}

/**********************************************************************//**
  Save baseline AI city data under the "baseline" prefix.
**************************************************************************/
static void bai_city_save(struct section_file *file,
                          const struct city *pcity, const char *citystr)
{
  dai_city_save(baseline_self, "baseline", file, pcity, citystr);
}

/**********************************************************************//**
  Load baseline AI city data from the "baseline" prefix.
**************************************************************************/
static void bai_city_load(const struct section_file *file,
                          struct city *pcity, const char *citystr)
{
  dai_city_load(baseline_self, "baseline", file, pcity, citystr);
}

/**********************************************************************//**
  Save baseline AI unit data under the "baseline_" key prefix (the
  vendored classic wrapper uses the empty prefix, which the live classic
  module already writes).
**************************************************************************/
static void bai_unit_save(struct section_file *file,
                          const struct unit *punit, const char *unitstr)
{
  dai_unit_save(baseline_self, "baseline_", file, punit, unitstr);
}

/**********************************************************************//**
  Load baseline AI unit data from the "baseline_" key prefix.
**************************************************************************/
static void bai_unit_load(const struct section_file *file,
                          struct unit *punit, const char *unitstr)
{
  dai_unit_load(baseline_self, "baseline_", file, punit, unitstr);
}

/**********************************************************************//**
  Set up the vendored copy of the stock AI under the name "baseline".
**************************************************************************/
bool fc_ai_baseline_setup(struct ai_type *ai)
{
  if (!fc_ai_classic_setup(ai)) {
    return FALSE;
  }

  strncpy(ai->name, "baseline", sizeof(ai->name));

  baseline_self = ai;
  ai->funcs.player_save_relations = bai_player_save_relations;
  ai->funcs.player_load_relations = bai_player_load_relations;
  ai->funcs.city_save = bai_city_save;
  ai->funcs.city_load = bai_city_load;
  ai->funcs.unit_save = bai_unit_save;
  ai->funcs.unit_load = bai_unit_load;

  return TRUE;
}
