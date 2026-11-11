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

/* Registers the vendored copy of the stock AI as the "unfixed" AI type.
 * See ai/unfixed/README. Like the vendored sources, this file is
 * compiled with unfixed_syms.h force-included, so every dai_* and
 * fc_ai_classic_* name below refers to the unf_-prefixed copy. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "support.h"

/* common */
#include "ai.h"
#include "player.h"

/* ai/unfixed/default */
#include "daicity.h"
#include "daiplayer.h"
#include "daiunit.h"

/* ai/unfixed/classic */
#include "classicai.h"

#include "unfixedai.h"

static struct ai_type *unfixed_self = NULL;

/**********************************************************************//**
  Save player relation data under the "unfixed" savefile prefix.
**************************************************************************/
static void uai_player_save_relations(struct player *pplayer,
                                      struct player *other,
                                      struct section_file *file,
                                      int plrno)
{
  dai_player_save_relations(unfixed_self, "unfixed", pplayer, other,
                            file, plrno);
}

/**********************************************************************//**
  Load player relation data from the "unfixed" savefile prefix.
**************************************************************************/
static void uai_player_load_relations(struct player *pplayer,
                                      struct player *other,
                                      const struct section_file *file,
                                      int plrno)
{
  dai_player_load_relations(unfixed_self, "unfixed", pplayer, other,
                            file, plrno);
}

/**********************************************************************//**
  Save city data under the "unfixed" savefile prefix.
**************************************************************************/
static void uai_city_save(struct section_file *file,
                          const struct city *pcity, const char *citystr)
{
  dai_city_save(unfixed_self, "unfixed", file, pcity, citystr);
}

/**********************************************************************//**
  Load city data from the "unfixed" savefile prefix.
**************************************************************************/
static void uai_city_load(const struct section_file *file,
                          struct city *pcity, const char *citystr)
{
  dai_city_load(unfixed_self, "unfixed", file, pcity, citystr);
}

/**********************************************************************//**
  Save unit data under the "unfixed_" savefile prefix.
**************************************************************************/
static void uai_unit_save(struct section_file *file,
                          const struct unit *punit, const char *unitstr)
{
  dai_unit_save(unfixed_self, "unfixed_", file, punit, unitstr);
}

/**********************************************************************//**
  Load unit data from the "unfixed_" savefile prefix.
**************************************************************************/
static void uai_unit_load(const struct section_file *file,
                          struct unit *punit, const char *unitstr)
{
  dai_unit_load(unfixed_self, "unfixed_", file, punit, unitstr);
}

/**********************************************************************//**
  Set up the vendored stock AI under the name "unfixed".

  The vendored classic wrappers save under the same savefile prefixes
  as the live classic module ("ai" for player relations and cities, ""
  for units). Every registered module's save hook runs for every
  player, so sharing those prefixes would write duplicate keys and
  make the save unloadable. The six savegame hooks are therefore
  replaced with ones using an "unfixed" prefix; nothing else differs
  from the classic module.
**************************************************************************/
bool fc_ai_unfixed_setup(struct ai_type *ai)
{
  if (!fc_ai_classic_setup(ai)) {
    return FALSE;
  }

  sz_strlcpy(ai->name, "unfixed");

  unfixed_self = ai;
  ai->funcs.player_save_relations = uai_player_save_relations;
  ai->funcs.player_load_relations = uai_player_load_relations;
  ai->funcs.city_save = uai_city_save;
  ai->funcs.city_load = uai_city_load;
  ai->funcs.unit_save = uai_unit_save;
  ai->funcs.unit_load = uai_unit_load;

  return TRUE;
}
