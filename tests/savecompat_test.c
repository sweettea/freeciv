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

/* Tests of the savegame upgrade steps in server/savegame/savecompat.c,
 * read back with the loader's static functions in savegame3.c.
 *
 * Both files are compiled into this test, so their static functions
 * can be called by name. See tests/README.md. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <criterion/criterion.h>

/* utility */
#include "registry.h"

/* common */
#include "vision.h"

/* server/savegame */
#include "savecompat.c"
#include "savegame3.c"

#include "fixture.h"

TestSuite(savecompat, .init = fx_init_classic, .fini = fx_fini);

/* A 3.2 (format 60) savegame section for one player with one real
 * city, c0, and two cities it remembers seeing, dc0 and dc1. 3.2 saved
 * an original owner for real cities but not for remembered ones. */
static struct section_file *save_032(void)
{
  struct section_file *file = secfile_new(FALSE);
  const char *actions[] = { "Found City" };
  int i;

  secfile_insert_int(file, 60, "savefile.version");
  secfile_insert_int(file, 1, "savefile.action_size");
  secfile_insert_str_vec(file, actions, 1, "savefile.action_vector");

  secfile_insert_int(file, 1, "player0.ncities");
  secfile_insert_int(file, 0, "player0.c0.original");

  secfile_insert_int(file, 2, "player0.dc_total");
  for (i = 0; i < 2; i++) {
    secfile_insert_int(file, 3 + i * 4, "player0.dc%d.x", i);
    secfile_insert_int(file, 3, "player0.dc%d.y", i);
    secfile_insert_int(file, 0, "player0.dc%d.owner", i);
    secfile_insert_int(file, 101 + i, "player0.dc%d.id", i);
    secfile_insert_int(file, 3, "player0.dc%d.size", i);
    secfile_insert_str(file, "", "player0.dc%d.improvements", i);
  }

  return file;
}

/* compat_load_030300() gives every remembered city an original owner
 * under the key sg_load_player_vision_city() reads it from, so the
 * upgraded 3.2 save loads each of them. */
Test(savecompat, upgrade_030300_keeps_remembered_cities)
{
  struct player *plr = fx_player();
  struct loaddata loading;
  int i;

  memset(&loading, 0, sizeof(loading));
  loading.file = save_032();
  loading.version = 60;
  sg_success = TRUE;

  compat_load_030300(&loading, SAVEGAME_3);
  cr_assert(sg_success);

  for (i = 0; i < 2; i++) {
    char citystr[32];
    struct vision_site *pdcity = vision_site_new(0, nullptr, nullptr);
    int original;

    fc_snprintf(citystr, sizeof(citystr), "player0.dc%d", i);
    cr_expect(secfile_lookup_int(loading.file, &original,
                                 "%s.original", citystr),
              "%s.original is missing after the upgrade", citystr);
    cr_expect(sg_load_player_vision_city(&loading, plr, pdcity, citystr),
              "%s does not load after the upgrade", citystr);
    vision_site_destroy(pdcity);
  }

  /* The real city's original owner is untouched. */
  cr_expect_eq(secfile_lookup_int_default(loading.file, -2,
                                          "player0.c0.original"), 0);

  free(loading.action.order);
  secfile_destroy(loading.file);
}
