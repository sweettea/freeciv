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

/* L0 unit test for the tuning loader's precision contract.
 *
 * THE GUARD: a tuning file whose [tuning] section has zero keys must
 * leave the active tuning byte-identical to no tuning file at all.
 * It did not, until session H1: secfile_lookup_float_default() takes
 * its default as a `float`, so every double tunable the file did not
 * mention was quantised on load (0.9 -> 0.8999999761581421) and any
 * tuning file was a second, uncontrolled treatment. That cost real
 * measurements -- W34 E34.3 FINDING 2 -- so it gets a cheap check
 * that fails loudly rather than a comment. */

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ai/deep */
#include "deeptuning.h"

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
  Write body to a fresh temporary file and point $FREECIV_DEEP_TUNING at
  it. The caller owns the returned path and must unlink() it.
**************************************************************************/
static char *tuning_file(const char *body)
{
  static char path[512];
  const char *dir = getenv("TMPDIR");
  int fd;
  FILE *fp;

  snprintf(path, sizeof(path), "%s/deeptuning_test_XXXXXX",
           (dir != NULL && dir[0] != '\0') ? dir : "/tmp");
  fd = mkstemp(path);
  if (fd < 0) {
    fprintf(stderr, "FAIL: cannot create temporary file \"%s\"\n", path);
    exit(EXIT_FAILURE);
  }
  fp = fdopen(fd, "w");
  fputs(body, fp);
  fclose(fp);
  setenv("FREECIV_DEEP_TUNING", path, 1);

  return path;
}

/**********************************************************************//**
  Reload the tuning and take a snapshot of it.
**************************************************************************/
static struct deep_tuning snapshot(void)
{
  deep_tuning_init();

  return *deep_tuning_get();
}

/**********************************************************************//**
  The loader's precision contract, three cases.
**************************************************************************/
int main(void)
{
  struct deep_tuning none, empty, onekey;
  char *path;

  /* 1. No tuning file: the compiled defaults, whatever they are. */
  unsetenv("FREECIV_DEEP_TUNING");
  none = snapshot();

  /* The check below is only meaningful if some default is NOT exactly
   * representable as a float -- otherwise quantisation would be
   * invisible and the guard vacuous. */
  CHECK((double) (float) none.site_travel_discount
        != none.site_travel_discount,
        "site_travel_discount %.17g survives a float round trip, so this"
        " test can no longer see quantisation; pick another witness",
        none.site_travel_discount);

  /* 2. A file with a [tuning] section and no keys: byte-identical. */
  path = tuning_file("; the identity tuning file, as in"
                     " benchmark/tuning/identity.ini\n[tuning]\n");
  empty = snapshot();
  unlink(path);
  CHECK(memcmp(&none, &empty, sizeof(none)) == 0,
        "a tuning file with zero keys changed the active tuning;"
        " site_travel_discount %.17g -> %.17g",
        none.site_travel_discount, empty.site_travel_discount);

  /* 3. A file with one key: that key moves, and nothing else does.
   *    Keys the file DOES set go through the float lookup exactly as
   *    they always have, so archived tuning files keep their meaning. */
  path = tuning_file("[tuning]\nsite_travel_discount = 0.5\n");
  onekey = snapshot();
  unlink(path);
  CHECK(onekey.site_travel_discount == 0.5,
        "an explicit key did not load: site_travel_discount %.17g",
        onekey.site_travel_discount);
  onekey.site_travel_discount = none.site_travel_discount;
  CHECK(memcmp(&none, &onekey, sizeof(none)) == 0,
        "a one-key tuning file moved something besides its own key");

  unsetenv("FREECIV_DEEP_TUNING");

  if (failures > 0) {
    fprintf(stderr, "%d failure(s)\n", failures);

    return EXIT_FAILURE;
  }
  printf("deeptuning: OK\n");

  return EXIT_SUCCESS;
}
