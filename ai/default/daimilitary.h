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
#ifndef FC__DAIMILITARY_H
#define FC__DAIMILITARY_H

/* common */
#include "fc_types.h"
#include "unittype.h"

/* server/advisors */
#include "advchoice.h"

struct civ_map;

#ifdef FREECIV_WEB
#define ASSESS_DANGER_MAX_DISTANCE         40
#define AI_HANDICAP_DISTANCE_LIMIT         6  /* TODO: 20 for non-web */
#endif /* FREECIV_WEB */

/* When an enemy has this or lower number of cities left, try harder
   to finish them off. */
#define FINISH_HIM_CITY_COUNT              5

typedef struct unit_list *(player_unit_list_getter)(struct player *pplayer);

struct unit_type *dai_choose_defender_versus(struct city *pcity,
                                             struct unit *attacker);
unsigned int assess_danger(struct ai_type *ait,
                           const struct civ_map *nmap,
                           struct city *pcity,
                           player_unit_list_getter ul_cb);
struct unit_type *dai_choose_attacker(struct ai_type *ait,
                                      const struct civ_map *nmap,
                                      struct city *pcity,
                                      enum terrain_class tc,
                                      bool allow_gold_upkeep);
struct unit_type *dai_choose_bodyguard(struct ai_type *ait,
                                       const struct civ_map *nmap,
                                       struct city *pcity,
                                       enum terrain_class tc,
                                       enum unit_role_id role,
                                       bool allow_gold_upkeep);
struct adv_choice *kill_something_with(struct ai_type *ait,
                                       const struct civ_map *nmap,
                                       struct player *pplayer,
                                       struct city *pcity, struct unit *myunit,
                                       struct adv_choice *choice);
void dai_unit_consider_bodyguard(struct ai_type *ait,
                                 const struct civ_map *nmap,
                                 struct city *pcity,
                                 struct unit_type *punittype,
                                 struct adv_choice *choice);
void adjust_ai_unit_choice(struct city *pcity,
                           struct adv_choice *choice);

struct adv_choice *military_advisor_choose_build(struct ai_type *ait,
                                                 const struct civ_map *nmap,
                                                 struct player *pplayer,
                                                 struct city *pcity,
                                                 player_unit_list_getter ul_cb);
void dai_assess_danger_player(struct ai_type *ait,
                              const struct civ_map *nmap, struct player *pplayer);
int assess_defense_quadratic(struct ai_type *ait, struct city *pcity);
int assess_defense_igwall(struct ai_type *ait, struct city *pcity);
bool action_may_happen_unit_on_city(const action_id wanted_action,
                                    const struct unit *actor,
                                    const struct city *pcity,
                                    int turns);
void dai_reevaluate_building(struct city *pcity, adv_want *value,
                             unsigned int urgency, unsigned int danger,
                             int defense);
int assess_defense_unit(struct ai_type *ait, struct city *pcity,
                        struct unit *punit, bool igwall);
int assess_defense(struct ai_type *ait, struct city *pcity);
bool dai_process_defender_want(struct ai_type *ait, const struct civ_map *nmap,
                               struct player *pplayer,
                               struct city *pcity, unsigned int danger,
                               struct adv_choice *choice, adv_want extra_want);

#endif /* FC__DAIMILITARY_H */
