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
#ifndef FC__TESTS_FIXTURE_H
#define FC__TESTS_FIXTURE_H

/* The headless game fixture for the Criterion suite.
 *
 * fx_init() brings the server up the way srv_prepare() does, minus the
 * network: server state, settings, a named ruleset loaded from the
 * source tree's data/, and a FX_MAP_XSIZE x FX_MAP_YSIZE flat
 * non-wrapping map filled with one land terrain. The helpers below then
 * place players, cities and units on chosen tiles; fx_fini() frees what
 * they made and the game with it.
 *
 * Use it as a suite's init/fini:
 *
 *   TestSuite(mysuite, .init = fx_init_civ2civ3, .fini = fx_fini);
 *
 * Criterion runs every test in its own process, so each test starts
 * from a fresh fixture and nothing a test changes reaches another.
 *
 * A failed fc_assert() raises SIGABRT, which Criterion reports as a
 * crash. A test that documents such a failure says so with
 * `.signal = SIGABRT`. */

#include <stdbool.h>

/* common */
#include "player.h"

struct city;
struct player;
struct terrain;
struct tile;
struct unit;
struct unit_type;

#define FX_MAP_XSIZE 20
#define FX_MAP_YSIZE 20

/* Load `ruleset` (a directory name under data/, e.g. "civ2civ3") and
 * activate it, then build the map. Fails the running test if the
 * ruleset does not load. */
void fx_init(const char *ruleset);
void fx_init_classic(void);
void fx_init_civ2civ3(void);
void fx_init_sandbox(void);
void fx_fini(void);

/* Wall-clock seconds the last fx_init() took. */
double fx_setup_seconds(void);

/* Lookups by rule name; each fails the running test if absent. */
struct terrain *fx_terrain(const char *rule_name);
const struct unit_type *fx_utype(const char *rule_name);

/* The tile at native (x, y), and a way to change its terrain. */
struct tile *fx_tile(int x, int y);
void fx_set_terrain(int x, int y, const char *rule_name);

/* A new human-controlled player with the next free playable nation,
 * its traits drawn, alive, and in no contact with anybody. Its AI type
 * is the server's default one, as for any player. */
struct player *fx_player(void);

/* A new AI-controlled player of AI type `ai_name` (nullptr for the
 * server's default, "classic" in a stock build), set up as aifill()
 * sets one up: skill level from the game, AI control gained. The AI
 * type's per-player, per-city and per-unit data are allocated as the
 * player, its cities and its units are made. */
struct player *fx_ai_player(const char *ai_name);

/* Start a phase for every player as begin_phase() does: the advisor
 * data and each AI's phase data (diplomacy intel, danger, the
 * per-phase caches). Call it after placing players, cities and units
 * and before calling AI code that reads them; fx_fini() ends the
 * phase. */
void fx_phase_begin(void);

/* Put two players in diplomatic state `type` with each other, in
 * contact, with the state's 16-turn countdown (armistice, ceasefire)
 * full. */
void fx_diplstate(struct player *a, struct player *b,
                  enum diplstate_type type);

/* fx_diplstate(a, b, DS_WAR). */
void fx_war(struct player *a, struct player *b);

/* Make the tech `rule_name` known to `pplayer`'s research, as a
 * savegame load does, and refresh its cities. */
void fx_tech(struct player *pplayer, const char *rule_name);

/* Change `pplayer`'s government to `rule_name` at once, without a
 * revolution, through government_change(): rates are capped and its
 * cities refreshed. */
void fx_government(struct player *pplayer, const char *rule_name);

/* Show `pplayer` the whole map as it stands now: terrain, and the
 * cities on it as sites it knows. Actions against a city need the
 * actor's player to know the city, so call this after placing them. */
void fx_show_all(struct player *pplayer);

/* A virtual city of size 1 owned by `owner` on (x, y): registered in
 * the index and the owner's city list, its centre tile worked and
 * owned, its citizens of the owner's nationality, given its advisor
 * data, and refreshed. */
struct city *fx_city(struct player *owner, int x, int y, const char *name);

/* A virtual unit of type `utype` (rule name) at (x, y) with full hp
 * and the given veteran level, registered in the index, the tile's
 * unit list and the owner's unit list. Homeless. */
struct unit *fx_unit(struct player *owner, const char *utype, int x, int y,
                     int veteran);

#endif /* FC__TESTS_FIXTURE_H */
