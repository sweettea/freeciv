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
#ifndef FC__DEEPCOMBAT_H
#define FC__DEEPCOMBAT_H

/* common */
#include "fc_types.h"

struct ai_type;
struct civ_map;

/* Both sides of the defensive ledger from one run of the chained-duel
 * model: what the wave is expected to cost US (hits, in successful
 * attacks, which is citizens under killcitizen) and what the garrison
 * is expected to cost THEM (att_loss, in shields). */
struct deep_defence_read {
  double surv;          /* P(the garrison holds). */
  int wave;             /* Attackers that qualified; 0 means no threat. */
  int n_def;            /* Defenders counted, including any `extra`. */
  double att_loss;      /* E[attacker shields destroyed]. */
  double hits;          /* E[successful enemy attacks on the stack]. */
};

void deep_city_defence_read(const struct civ_map *nmap, struct city *pcity,
                            int horizon, struct unit *const *extra,
                            int n_extra, struct deep_defence_read *out);

double deep_city_stack_survival(const struct civ_map *nmap,
                                struct city *pcity, int horizon,
                                int *wave_out);
double deep_city_survival_verdict(const struct civ_map *nmap,
                                  struct city *pcity, int horizon,
                                  int *wave_out);
void deep_survival_cache_clear(void);
double deep_city_stack_survival_without(const struct civ_map *nmap,
                                        struct city *pcity, int horizon,
                                        const struct unit *except,
                                        int *wave_out);
double deep_city_stack_survival_with(const struct civ_map *nmap,
                                     struct city *pcity, int horizon,
                                     struct unit *const *extra, int n_extra,
                                     int *wave_out);
/* Both counterfactuals at once: this blade leaves AND these arrive.
 * The core has always supported the pair; the hole-covering price
 * (NETTHREAT item 4) is the first caller that needs it. */
double deep_city_stack_survival_swap(const struct civ_map *nmap,
                                     struct city *pcity, int horizon,
                                     const struct unit *except,
                                     struct unit *const *extra, int n_extra,
                                     int *wave_out);
/* NETTHREAT item 2's cheaper probe: the garrison-pin counter. Tagging
 * happens inside the wave assembly; these three are the lifecycle the
 * rest of the module owes it. Counter-only -- nothing here is read by
 * a decision, and all of it is gated on the metric log level. */
void deep_pin_poll(struct player *pplayer);
void deep_pin_note_move(struct unit *punit);
void deep_pin_reset(void);

/* The reinforcement model behind the arrival odds: what the target
 * can actually build per turn (deep_reinforce_observed) and what the
 * model charges per turn of our travel (deep_reinforce_rate, the flat
 * rate plus invasion_reinforce_slope times the observed one). Exported
 * so the build side can log the same numbers the pricing used. */
double deep_reinforce_observed(const struct civ_map *nmap,
                               const struct city *acity);
double deep_reinforce_rate(const struct civ_map *nmap,
                           const struct city *acity);
double deep_take_odds(const struct civ_map *nmap, struct city *acity,
                      struct unit *const *stream, int n_stream);
double deep_take_odds_at_arrival(const struct civ_map *nmap,
                                 struct city *acity,
                                 struct unit *const *stream, int n_stream,
                                 int eta);
double deep_take_odds_cost(const struct civ_map *nmap, struct city *acity,
                           struct unit *const *stream, int n_stream,
                           int eta, double *loss_shields,
                           double *survivors);

#endif /* FC__DEEPCOMBAT_H */
