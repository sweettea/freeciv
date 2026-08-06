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

#include <stdlib.h>

/* common */
#include "city.h"
#include "combat.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "tile.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* server/advisors */
#include "advgoto.h"

#include "deepdefence.h"
#include "deepodds.h"
#include "deeptuning.h"

#include "deepcombat.h"

/* Computational bounds, not tuning: the strongest units past either
 * cap carry essentially all of the probability mass. */
#define DEEP_WAVE_MAX 32
#define DEEP_STACK_MAX 16

struct deep_combatant {
  struct unit *punit;
  int rating;
};

/**********************************************************************//**
  Strongest first; unit id breaks ties so runs are deterministic.
**************************************************************************/
static int deep_combatant_cmp(const void *a_, const void *b_)
{
  const struct deep_combatant *a = a_, *b = b_;

  if (a->rating != b->rating) {
    return b->rating - a->rating;
  }
  return a->punit->id - b->punit->id;
}

/**********************************************************************//**
  Keep the strongest `cap` combatants seen so far.
**************************************************************************/
static void deep_combatant_add(struct deep_combatant *arr, int *n, int cap,
                               struct unit *punit, int rating)
{
  int i, weakest = 0;

  if (*n < cap) {
    arr[*n].punit = punit;
    arr[*n].rating = rating;
    (*n)++;
    return;
  }
  for (i = 1; i < cap; i++) {
    if (arr[i].rating < arr[weakest].rating) {
      weakest = i;
    }
  }
  if (rating > arr[weakest].rating) {
    arr[weakest].punit = punit;
    arr[weakest].rating = rating;
  }
}

/**********************************************************************//**
  The attacker's effective strength against this defender if it
  arrived FRESH. Enemy units are assessed at our phase start, when
  most have spent their moves; pricing them tired would let
  game.info.tired_attack scale a real threat toward zero. moves_left
  is restored before anything else can observe it.
**************************************************************************/
static int deep_fresh_attack_power(struct unit *att, const struct unit *def)
{
  int save = att->moves_left;
  int power;

  if (save < SINGLE_MOVE) {
    att->moves_left = SINGLE_MOVE;
  }
  power = get_total_attack_power(att, def, NULL);
  att->moves_left = save;

  return power;
}

/**********************************************************************//**
  The city's military garrison as a sorted (best first) defender list.
  `except` (may be NULL) is left out -- the sortie passes price a
  city's night without one of its blades this way.
**************************************************************************/
static int deep_collect_defenders(struct city *pcity,
                                  struct deep_combatant *def,
                                  const struct unit *except)
{
  struct player *pplayer = city_owner(pcity);
  int n_def = 0;

  unit_list_iterate(city_tile(pcity)->units, punit) {
    if (punit != except
        && unit_owner(punit) == pplayer
        && !unit_has_type_flag(punit, UTYF_CIVILIAN)
        && punit->hp > 0) {
      deep_combatant_add(def, &n_def, DEEP_STACK_MAX, punit,
                         adv_unit_def_rating_basic(punit));
    }
  } unit_list_iterate_end;
  qsort(def, n_def, sizeof(def[0]), deep_combatant_cmp);

  return n_def;
}

/**********************************************************************//**
  P(the garrison outlives the stream): fill the per-pair parameters
  and run the chained-duel model. Shared by both reads -- the city's
  survival against an enemy wave, and (as 1 minus) our assault's
  chance against an enemy garrison.
**************************************************************************/
static double deep_garrison_vs_stream(const struct civ_map *nmap,
                                      struct deep_combatant *att, int n_att,
                                      struct deep_combatant *def, int n_def,
                                      double *att_death)
{
  int att_hp[DEEP_WAVE_MAX], def_hp[DEEP_STACK_MAX];
  struct deep_duel pair[DEEP_WAVE_MAX * DEEP_STACK_MAX];
  int i, j;

  if (att_death != NULL) {
    for (j = 0; j < n_att; j++) {
      att_death[j] = 0.0;
    }
  }
  if (n_att == 0) {
    return 1.0;
  }
  if (n_def == 0) {
    return 0.0;
  }

  for (j = 0; j < n_att; j++) {
    att_hp[j] = att[j].punit->hp;
  }
  for (i = 0; i < n_def; i++) {
    def_hp[i] = def[i].punit->hp;
  }
  for (j = 0; j < n_att; j++) {
    for (i = 0; i < n_def; i++) {
      struct deep_duel *d = &pair[j * n_def + i];

      d->att_str = deep_fresh_attack_power(att[j].punit, def[i].punit);
      d->def_str = get_total_defense_power(att[j].punit, def[i].punit);
      get_modified_firepower(nmap, att[j].punit, def[i].punit,
                             &d->att_fp, &d->def_fp);
    }
  }

  return deep_stack_survival_losses(n_att, att_hp, n_def, def_hp, pair,
                                    att_death);
}

/**********************************************************************//**
  Probability that this city's garrison survives the wave of at-war
  units that could plausibly reach it within `horizon` turns, resolved
  as sequential duels to the death (deepodds.c). Strongest attackers
  strike first; the garrison presents its best defender until it
  falls.

  Real units only -- no unit is imagined here, and the anticipatory
  fear terms keep covering what this pass will not price. But NOT
  observed: the wave is assembled straight from aplayer->units with no
  visibility test of any kind, so it sees fogged and never-scouted
  stacks alike (RC6). The gates that do exist are at-war, contact-made
  (barbarians excepted, they never make contact), continent or coastal
  reachability, and arrival inside `horizon`. Making this honest is
  W04's job; until then every survival probability in this module is
  computed on ground truth.

  wave_out (optional) reports how many attackers qualified; with an
  empty wave the answer is 1.0 and with an empty garrison against any
  wave it is 0.0.
**************************************************************************/
double deep_city_stack_survival(const struct civ_map *nmap,
                                struct city *pcity, int horizon,
                                int *wave_out)
{
  return deep_city_stack_survival_without(nmap, pcity, horizon, NULL,
                                          wave_out);
}

/* One remembered verdict per city per phase. */
struct deep_survival_memo {
  int city_id;
  int horizon;
  double surv;
  int wave;
};

static struct deep_survival_memo *deep_survival_cache = NULL;
static int deep_survival_n = 0;
static int deep_survival_cap = 0;

/**********************************************************************//**
  Forget every remembered verdict. Called at the start of each player
  phase, which is what makes "once per city per turn" true and what
  keeps a verdict from a previous game in the same server process from
  ever being read.
**************************************************************************/
void deep_survival_cache_clear(void)
{
  deep_survival_n = 0;
}

/**********************************************************************//**
  The city's survival verdict for THIS phase -- computed on the first
  question and remembered for the rest of them.

  "Will this city hold" is asked by the citizen arrangement at the top
  of the turn (deepcm.c, threshold cm_war_survival) and by the growth
  ladder at the bottom (deephorizon.c, threshold survival_target), and
  the two used to run the model independently against a map that had
  moved in between. Two thresholds are a defensible disagreement about
  what the answer is worth -- being wrong about a citizen costs food,
  being wrong about a garrison costs the city -- but two computations
  are a coin flip: the same city could be arranged for a siege and
  credited for growth in the same turn, which is a variance source in
  every ECON tuning cell and not a decision anyone made.

  Same call, same probability, thresholds intact and separate. The
  price is that the ladder reads a verdict taken before this turn's
  movement; the phase is the unit of coherence, deliberately.
**************************************************************************/
double deep_city_survival_verdict(const struct civ_map *nmap,
                                  struct city *pcity, int horizon,
                                  int *wave_out)
{
  int i;
  struct deep_survival_memo *e;

  for (i = 0; i < deep_survival_n; i++) {
    e = &deep_survival_cache[i];
    if (e->city_id == pcity->id && e->horizon == horizon) {
      if (wave_out != NULL) {
        *wave_out = e->wave;
      }
      return e->surv;
    }
  }

  if (deep_survival_n >= deep_survival_cap) {
    deep_survival_cap = MAX(64, deep_survival_cap * 2);
    deep_survival_cache = fc_realloc(deep_survival_cache,
                                     deep_survival_cap
                                     * sizeof(*deep_survival_cache));
  }
  e = &deep_survival_cache[deep_survival_n++];
  e->city_id = pcity->id;
  e->horizon = horizon;
  e->surv = deep_city_stack_survival(nmap, pcity, horizon, &(e->wave));
  if (wave_out != NULL) {
    *wave_out = e->wave;
  }

  return e->surv;
}

/**********************************************************************//**
  As deep_city_stack_survival(), with one garrison unit left out of
  the stack -- what the night costs if that blade sorties or is lent.
**************************************************************************/
static double deep_city_survival_core(const struct civ_map *nmap,
                                      struct city *pcity, int horizon,
                                      const struct unit *except,
                                      struct unit *const *extra, int n_extra,
                                      int *wave_out)
{
  struct deep_combatant att[DEEP_WAVE_MAX], def[DEEP_STACK_MAX];
  int n_att = 0, n_def;
  struct player *pplayer = city_owner(pcity);
  struct tile *ctile = city_tile(pcity);
  Continent_id home = tile_continent(ctile);
  bool coastal = is_terrain_class_near_tile(nmap, ctile, TC_OCEAN);
  int n_air = 0;

  if (wave_out != NULL) {
    *wave_out = 0;
  }

  players_iterate_alive(aplayer) {
    /* The initial diplstate between unmet players is war; fearing
     * their starting units pre-contact is insurance bought before
     * evidence (the ramp law). Barbarians never make contact -- their
     * units on the map ARE the evidence. */
    if (!pplayers_at_war(pplayer, aplayer)
        || (!is_barbarian(aplayer)
            && player_diplstate_get(pplayer, aplayer)->type
               == DS_NO_CONTACT)) {
      continue;
    }
    unit_list_iterate(aplayer->units, aunit) {
      const struct unit_type *at = unit_type_get(aunit);
      int rating = adv_unit_att_rating(aunit);
      int rate, turns;
      bool flies_in;

      if (rating <= 0 || aunit->hp <= 0) {
        continue;
      }
      /* Reachable: shares the continent, is already at sea aboard a
       * transport, is a warship and the city has a shore -- or it
       * flies, and we are inside its reach.
       *
       * The air term is combat E2 (military audit G5). Every enemy
       * unit that neither swam nor rode used to be dropped for being
       * on another landmass, which silently means the survival model
       * cannot see a paratrooper or a helicopter at all: the two
       * capabilities in the shipped rulesets whose whole point is
       * that no approach is visible. Reach is the drop range or the
       * move rate, from the same class test and the same sizing the
       * garrison floor's airborne class uses -- so the floor that
       * insures against air and the wave that would price it agree by
       * construction. */
      flies_in = tile_continent(unit_tile(aunit)) != home
                      && !unit_transported(aunit)
                      && deep_utype_is_airborne(at)
                      && real_map_distance(unit_tile(aunit), ctile)
                         <= deep_utype_air_reach(at);

      if (tile_continent(unit_tile(aunit)) != home
          && !unit_transported(aunit)
          && !(utype_move_type(at) == UMT_SEA && coastal)
          && !flies_in) {
        continue;
      }
      if (flies_in) {
        n_air++;
      }
      rate = MAX(at->move_rate, 1);
      turns = (real_map_distance(unit_tile(aunit), ctile) * SINGLE_MOVE
               + rate - 1) / rate;
      if (turns > horizon) {
        continue;
      }
      deep_combatant_add(att, &n_att, DEEP_WAVE_MAX, aunit, rating);
    } unit_list_iterate_end;
  } players_iterate_alive_end;

  if (n_att == 0) {
    return 1.0;
  }
  if (wave_out != NULL) {
    *wave_out = n_att;
  }
  if (n_air > 0) {
    /* Its own counter, logged only when the term has a population:
     * zero lines in a cell is the proof that the cell never reached
     * flight, which is what both benchmark cells did before this. */
    log_base(deep_metric_log_level(),
             "deep-air turn=%d player=\"%s\" city=%d wave=%d air=%d"
             " horizon=%d",
             game.info.turn, player_name(pplayer), pcity->id, n_att,
             n_air, horizon);
  }

  n_def = deep_collect_defenders(pcity, def, except);
  {
    int e;

    for (e = 0; e < n_extra; e++) {
      deep_combatant_add(def, &n_def, DEEP_STACK_MAX, extra[e],
                         adv_unit_def_rating_basic(extra[e]));
    }
    if (n_extra > 0) {
      qsort(def, n_def, sizeof(def[0]), deep_combatant_cmp);
    }
  }
  qsort(att, n_att, sizeof(att[0]), deep_combatant_cmp);

  return deep_garrison_vs_stream(nmap, att, n_att, def, n_def, NULL);
}

/**********************************************************************//**
  The garrison's survival without one of its own blades (the sortie
  and lending gates: what does this city's night look like if this
  unit leaves?).
**************************************************************************/
double deep_city_stack_survival_without(const struct civ_map *nmap,
                                        struct city *pcity, int horizon,
                                        const struct unit *except,
                                        int *wave_out)
{
  return deep_city_survival_core(nmap, pcity, horizon, except, NULL, 0,
                                 wave_out);
}

/**********************************************************************//**
  The garrison's survival WITH reinforcements that are not standing
  here yet: what this city's night looks like if these blades arrive
  in time. The defence counter-claim sizes a relief force this way --
  send enough or send none, the same massing discipline the operations
  layer imposes on attacks.
**************************************************************************/
double deep_city_stack_survival_with(const struct civ_map *nmap,
                                     struct city *pcity, int horizon,
                                     struct unit *const *extra, int n_extra,
                                     int *wave_out)
{
  return deep_city_survival_core(nmap, pcity, horizon, NULL, extra, n_extra,
                                 wave_out);
}

/**********************************************************************//**
  The other direction of the same arithmetic: the chance that OUR
  ordered stream of attackers takes acity's garrison -- one minus the
  garrison's survival. The stream is priced as arriving together and
  fresh; making that simultaneity true is the operations layer's job,
  so build-side consumers should treat this as the ceiling the
  assembled force could reach.
**************************************************************************/
double deep_take_odds(const struct civ_map *nmap, struct city *acity,
                      struct unit *const *stream, int n_stream)
{
  return deep_take_odds_at_arrival(nmap, acity, stream, n_stream, 0);
}

/**********************************************************************//**
  Take odds against the garrison AS IT WILL BE when we arrive: travel
  time is the defender's budget (naval design, human refinement
  2026-07-30 -- a 12-turn sail must clear a much higher bar than a
  2-turn hop, which is exactly why long-range desultory attacks fail
  while looking winnable on paper). eta turns of sailing add
  eta x invasion_reinforce_rate virtual copies of the best defender
  the target could build, placed in the city so walls and terrain
  count. eta 0 is the plain current-garrison read.
**************************************************************************/
double deep_take_odds_at_arrival(const struct civ_map *nmap,
                                 struct city *acity,
                                 struct unit *const *stream, int n_stream,
                                 int eta)
{
  return deep_take_odds_cost(nmap, acity, stream, n_stream, eta, NULL,
                             NULL);
}

/**********************************************************************//**
  The take odds AND what taking it is expected to cost: the same walk,
  reporting expected attacker losses in shields and the expected size
  of the force still standing afterwards.

  loss_shields is the sum over the stream of P(this unit dies) times
  its build cost -- the currency the build auction and the granary are
  already priced in, so "is it worth what it costs" is finally a
  subtraction rather than a comparison of scales (military audit G1).
  survivors is the same walk's other half: what would garrison the
  city we just took (the hold-the-city test).

  Both inherit every assumption the substrate ships with, stated in
  findings "Combat workstream epic 1": one wave, no healing, no
  promotion, no reinforcement between duels, military garrison only,
  observed units only, and the 32/16 computational caps -- a stream
  longer than 32 is priced on its 32 strongest, and the rest are
  charged nothing.
**************************************************************************/
double deep_take_odds_cost(const struct civ_map *nmap, struct city *acity,
                           struct unit *const *stream, int n_stream,
                           int eta, double *loss_shields,
                           double *survivors)
{
  struct deep_combatant att[DEEP_WAVE_MAX], def[DEEP_STACK_MAX];
  int n_att = 0, n_def, j, reinforce = 0;
  struct unit *virt[DEEP_STACK_MAX];
  int n_virt = 0;
  double survival;
  double death[DEEP_WAVE_MAX];
  bool want_cost = (loss_shields != NULL || survivors != NULL);

  if (loss_shields != NULL) {
    *loss_shields = 0.0;
  }
  if (survivors != NULL) {
    *survivors = 0.0;
  }

  for (j = 0; j < n_stream; j++) {
    deep_combatant_add(att, &n_att, DEEP_WAVE_MAX, stream[j],
                       adv_unit_att_rating(stream[j]));
  }
  if (n_att == 0) {
    return 0.0;
  }
  qsort(att, n_att, sizeof(att[0]), deep_combatant_cmp);

  n_def = deep_collect_defenders(acity, def, NULL);

  if (eta > 0) {
    const struct deep_tuning *tune = deep_tuning_get();
    /* Barbarians lean against defending their takes (human doctrine
     * + T160 forensics: 47 of 52 barb-held cities garrisonless for
     * ~40 turns while an 87-unit horde raged in the field). Pricing
     * them with the civilised reactive-build model walls off the
     * whole reconquest pool behind phantom Musketeers. */
    double rrate = is_barbarian(city_owner(acity))
                   ? tune->barb_reinforce_rate
                   : tune->invasion_reinforce_rate;

    reinforce = (int) (eta * rrate + 0.5);
  }
  if (reinforce > 0) {
    struct player *owner = city_owner(acity);
    const struct unit_type *best = NULL;

    unit_type_iterate(ut) {
      if (ut->defense_strength > 0
          && can_city_build_unit_now(nmap, acity, ut, RPT_CERTAIN)
          && (best == NULL
              || ut->defense_strength > best->defense_strength)) {
        best = ut;
      }
    } unit_type_iterate_end;
    while (best != NULL && n_virt < reinforce
           && n_def + n_virt < DEEP_STACK_MAX
           && n_virt < DEEP_STACK_MAX) {
      struct unit *v = unit_virtual_create(owner, acity,
                                           (struct unit_type *) best, 0);

      unit_tile_set(v, city_tile(acity));
      virt[n_virt++] = v;
    }
    for (j = 0; j < n_virt; j++) {
      deep_combatant_add(def, &n_def, DEEP_STACK_MAX, virt[j],
                         adv_unit_def_rating_basic(virt[j]));
    }
    qsort(def, n_def, sizeof(def[0]), deep_combatant_cmp);
  }

  survival = deep_garrison_vs_stream(nmap, att, n_att, def, n_def,
                                     want_cost ? death : NULL);

  if (want_cost) {
    double dead = 0.0, shields = 0.0;

    for (j = 0; j < n_att; j++) {
      dead += death[j];
      shields += death[j] * unit_build_shield_cost_base(att[j].punit);
    }
    if (loss_shields != NULL) {
      *loss_shields = shields;
    }
    if (survivors != NULL) {
      *survivors = n_att - dead;
    }
  }

  for (j = 0; j < n_virt; j++) {
    unit_virtual_destroy(virt[j]);
  }

  return 1.0 - survival;
}
