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
#ifndef FC__DEEPTRADE_H
#define FC__DEEPTRADE_H

struct adv_choice;
struct ai_type;
struct city;
struct player;

void deep_choose_trade_targets(struct ai_type *ait, struct player *pplayer);

int deep_routes_with_players(const struct player *pplayer, int *routes,
                             int size);
/* What those routes are worth per turn, for one partner: the trade a
 * war with them would cancel. */
int deep_route_value_with(const struct player *pplayer,
                          const struct player *aplayer);
void deep_trade_route_love(struct ai_type *ait, struct player *pplayer);

void deep_trade_merit_apply(struct ai_type *ait, struct player *pplayer,
                            struct city *pcity, struct adv_choice *choice);

void deep_wonder_merit_apply(struct ai_type *ait, struct player *pplayer,
                             struct city *pcity, struct adv_choice *choice);

#endif /* FC__DEEPTRADE_H */
