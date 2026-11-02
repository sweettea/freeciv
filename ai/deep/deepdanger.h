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
#ifndef FC__DEEPDANGER_H
#define FC__DEEPDANGER_H

/* ai/default */
#include "daimilitary.h"

struct ai_type;
struct civ_map;
struct city;
struct player;

unsigned int deep_assess_danger(struct ai_type *ait,
                                const struct civ_map *nmap,
                                struct city *pcity,
                                player_unit_list_getter ul_cb);
void deep_assess_danger_player(struct ai_type *ait,
                               const struct civ_map *nmap,
                               struct player *pplayer);

/* Per-sweep table of hostile-unit positions relative to every city of
 * one defender: what deep_assess_danger() reads instead of building a
 * pf_reverse_map per (city, hostile player). See deepdanger.c. */
struct deep_danger_table;
struct deep_danger_row;
struct unit_list;

struct deep_danger_table *deep_danger_table_new(struct ai_type *ait,
                                                const struct civ_map *nmap,
                                                struct player *pplayer,
                                                player_unit_list_getter ul_cb);
void deep_danger_table_destroy(struct deep_danger_table *tab);
int deep_danger_table_slot(const struct deep_danger_table *tab,
                           const struct city *pcity);
const struct deep_danger_row *
deep_danger_table_row(struct ai_type *ait,
                      const struct deep_danger_table *tab,
                      const struct player *pplayer,
                      const struct player *aplayer,
                      struct unit_list *units);
bool deep_danger_row_turn(const struct deep_danger_table *tab,
                          const struct deep_danger_row *row,
                          int idx, bool ferry, int slot, int *turn);

#endif /* FC__DEEPDANGER_H */
